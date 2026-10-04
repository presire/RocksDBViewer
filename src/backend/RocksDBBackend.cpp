#include "RocksDBBackend.h"
#include "utils/SettingsMigration.h"
#include "utils/SettingsKeys.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDebug>
#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>
#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/convenience.h>
#include <rocksdb/cache.h>
#include <rocksdb/env.h>
#include <rocksdb/utilities/options_util.h>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <system_error>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#if defined(Q_OS_LINUX)
#include <fcntl.h>
#include <sys/syscall.h>
#endif
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
#include <filesystem>

namespace {
// 256MiB: インポート・読み込みファイルサイズの上限(S4)
constexpr qint64 kMaxImportFileSize = 256LL * 1024LL * 1024LL;
// 4MiB: TTL検出(B10)と事前パース無害化(S1)のためのOPTIONS読み込み上限
// これを超えるファイルは読み込めないものとして扱う
constexpr qint64 kMaxOptionsProbeSize = 4LL * 1024LL * 1024LL;
// 64MiB: 共有ブロックキャッシュサイズとwrite_buffer_sizeの上限値(S1)
constexpr size_t kBoundedCacheSize = 64 << 20;
// num_levelsがこの値を超えると拒否する
// RefreshDerivedOptionsがOpen時にこの値でベクタを確保するため
// (1000000000レベル≒8GiB)、実DBは一桁程度(既定7)である
constexpr qint64 kMaxOptionsLevels = 1000;
// num_shard_bitsがこの値を超えると上限化する
// キャッシュは2^num_shard_bits個のシャードに分割されるため
// (19bit≒50万ミューテックス)、8bit(256シャード)で十分である
// 既定のテーブルキャッシュは6を使用する
constexpr qint64 kMaxCacheShardBits = 8;
// bucket_count(ハッシュmemtableファクトリ)がこの値を超えると上限化する
// 既定は1000000(skiplist)と50000(linklist)で、
// それ超はOpen時にmemtable毎に大きな配列を確保する
constexpr qint64 kMaxBucketCount = 1048576;
// table_cache_numshardbitsがこの値を超えると上限化する(既定6)
constexpr qint64 kMaxTableCacheShardBits = 10;
// inplace_update_num_locksがこの値を超えると上限化する(既定10000)
constexpr qint64 kMaxMemtableLocks = 1048576;
// huge_page_tlb_sizeがこの値を超えると上限化する(既定0=無効)
constexpr qint64 kMaxHugePageTlbSize = 2097152;

/**
 * @brief rocksdb::SliceをUTF-8文字列に変換する
 *
 * StatelessかつConvertInitialBomフラグ付きのデコーダを使い、
 * 先頭BOMの保持と末尾の不完全マルチバイト検出を行ったうえで、
 * ラウンドトリップのバイト一致まで検証する
 * いずれかに失敗した場合はstd::nulloptを返す
 *
 * @param slice 変換対象のSlice
 * @return 変換後のQString、または無効な場合は std::nullopt
 */
std::optional<QString> utf8StringFromSlice(const rocksdb::Slice &slice)
{
    const QByteArray bytes(slice.data(), static_cast<qsizetype>(slice.size()));
    QStringDecoder decoder(QStringDecoder::Utf8,
                           QStringConverter::Flag::Stateless
                               | QStringConverter::Flag::ConvertInitialBom);
    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        return std::nullopt;
    }
    if (text.toUtf8() != bytes) {
        return std::nullopt;
    }
    return text;
}

/**
 * @brief QStringをUTF-8バイト列のstd::stringに変換する
 *
 * @param text 変換対象文字列
 * @return UTF-8エンコードされたstd::string
 */
std::string utf8BytesFromString(const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

/**
 * @brief rocksdb::StatusをQStringに変換する
 *
 * @param status RocksDBステータス
 * @return ステータスメッセージ文字列
 */
QString statusToQString(const rocksdb::Status &status)
{
    return QString::fromStdString(status.ToString());
}

/**
 * @brief 個別書き込み用のWriteOptionsを生成する
 *
 * sync=true、disableWAL=falseで個別エントリの書き込みを永続化する
 *
 * @return 書き込みオプション
 */
rocksdb::WriteOptions makeDurableWriteOptions()
{
    rocksdb::WriteOptions options;
    options.sync = true;
    options.disableWAL = false;
    return options;
}

/**
 * @brief バッチ書き込み用のWriteOptionsを生成する
 *
 * sync=false、disableWAL=falseで大量書き込みを効率的に行う
 *
 * @return 書き込みオプション
 */
rocksdb::WriteOptions makeBatchWriteOptions()
{
    rocksdb::WriteOptions options;
    options.sync = false;
    options.disableWAL = false;
    return options;
}

/**
 * @brief データベースOpen用のDBOptionsを生成する
 *
 * 信頼できないOPTIONSファイル由来の値は使わず、安全な既定値から構築する (S1)
 *
 * @return デフォルトのOpenオプション
 */
rocksdb::DBOptions makeOpenDbOptions()
{
    rocksdb::DBOptions options;
    options.create_if_missing = false;
    options.create_missing_column_families = false;
    options.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;
    options.paranoid_checks = true;
    options.best_efforts_recovery = false;
    return options;
}

/**
 * @brief 指定カラムファミリーのデータを永続化する
 *
 * SyncWALがサポートされていない環境では、Flushを使用して代替する
 *
 * @param db RocksDBインスタンス
 * @param handle カラムファミリーハンドル
 * @return 永続化結果のステータス
 */
rocksdb::Status persistColumnFamily(rocksdb::DB *db, rocksdb::ColumnFamilyHandle *handle)
{
    const rocksdb::Status syncStatus = db->SyncWAL();
    if (!syncStatus.ok() && syncStatus.IsNotSupported()) {
        rocksdb::FlushOptions flushOptions;
        flushOptions.wait = true;
        return db->Flush(flushOptions, handle);
    }
    return syncStatus;
}

/**
 * @brief 文字列をJSON文字列リテラルに変換する
 *
 * QtのJSONシリアライザで ["..."] を生成し、両端の角括弧を除去することで、エスケープ処理をQtに委譲する (S4)
 *
 * @param text 変換対象文字列
 * @return ダブルクォート付きのJSON文字列リテラル
 */
QString jsonStringLiteral(const QString &text)
{
    const QByteArray wrapped =
        QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(wrapped.mid(1, wrapped.size() - 2));
}

} // namespace

/**
 * @brief 対象ファイルをアトミックに書き換える(静的メンバ)
 *
 * 対象と同じディレクトリにQTemporaryFile(O_EXCL, mode 0600)を作成し、書き込み結果を毎回検査、flushとfsyncのうえcloseする
 *
 * 公開手順はallowOverwriteで切り替える:
 * - true: 従来どおり置換(rename)、末端シンボリックリンクは追従せず、リンク自体を置換する
 * - false: 原子的no-replace公開、Linuxではrenameat2(RENAME_NOREPLACE)、それ以外はlink()+unlink()、
 *          WindowsではREPLACE指定なしのMoveFileExWを用い、宛先が同時出現した場合も黙って置換せず失敗する
 *   末端シンボリックリンクは存在扱いで失敗し、どちらの手順でも
 *   末端リンクを追従することはない
 *
 * setAutoRemove(false)は公開成功後にのみ呼び出す
 */
bool RocksDBBackend::writeFileAtomically(const QString &targetPath,
                                         const std::function<bool(QFile &)> &writer,
                                         QString *error, bool allowOverwrite)
{
    const QFileInfo targetInfo(targetPath);
    const QDir dir = targetInfo.dir();
    if (!dir.exists()) {
        if (error) {
            *error = tr("Directory does not exist");
        }
        return false;
    }
    // 高速な存在検査であり、公開自体が原子的no-replaceのため、検査と公開の競合 (TOCTOU) は公開側で閉じる
    if (!allowOverwrite
        && (targetInfo.exists() || targetInfo.isSymLink())) {
        if (error) {
            *error = tr("File already exists: %1").arg(targetPath);
        }
        return false;
    }

    QTemporaryFile tmp(dir.filePath(QStringLiteral(".rocksdbviewer-tmp-XXXXXX")));
    tmp.setAutoRemove(true);
    if (!tmp.open()) {
        if (error) {
            *error = tmp.errorString();
        }
        return false;
    }
    if (targetInfo.exists()) {
        QFile::setPermissions(tmp.fileName(), QFile::permissions(targetPath));
    }
    if (!writer(tmp)) {
        if (error && error->isEmpty()) {
            *error = tmp.errorString().isEmpty() ? tr("Write failed")
                                                  : tmp.errorString();
        }
        return false;
    }
    tmp.flush();

#ifdef Q_OS_UNIX
    if (::fsync(tmp.handle()) != 0) {
        if (error) {
            *error = QString::fromLocal8Bit(strerror(errno));
        }
        return false;
    }
#elif defined(Q_OS_WIN)
    // QFileはCRTのfdを公開しないためfsync相当(_commit)を呼べない
    // flush+同一ボリュームへの原子的移動で足りると判断し省略する
#endif

    tmp.close();
    if (tmp.error() != QFile::NoError) {
        if (error) {
            *error = tmp.errorString();
        }
        return false;
    }

    if (allowOverwrite) {
        std::error_code ec;
        // UTF-16経由でpathを構築し、Windowsのコードページ依存の変換を避ける
        std::filesystem::rename(
            std::filesystem::path(tmp.fileName().toStdU16String()),
            std::filesystem::path(targetPath.toStdU16String()), ec);
        if (ec) {
            if (error) {
                *error = QString::fromStdString(ec.message());
            }
            return false;
        }
        tmp.setAutoRemove(false);
        return true;
    }

#ifdef Q_OS_WIN
    const std::wstring fromW = tmp.fileName().toStdWString();
    const std::wstring toW = targetPath.toStdWString();
    // MOVEFILE_REPLACE_EXISTINGを付けないため宛先存在時は失敗する
    // 同一ディレクトリ内 (同一ボリューム) の移動は原子的である
    if (!MoveFileExW(fromW.c_str(), toW.c_str(), MOVEFILE_WRITE_THROUGH)) {
        const DWORD code = GetLastError();
        if (error) {
            if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) {
                *error = tr("File already exists: %1").arg(targetPath);
            } else {
                *error = tr("Failed to publish file: %1 (code %2)")
                             .arg(targetPath)
                             .arg(code);
            }
        }
        return false;
    }
    tmp.setAutoRemove(false);
    return true;
#else
    const QByteArray fromN = QFile::encodeName(tmp.fileName());
    const QByteArray toN = QFile::encodeName(targetPath);
    bool published = false;
#if defined(Q_OS_LINUX) && defined(SYS_renameat2)
    // RENAME_NOREPLACE: 宛先が存在すればEEXISTで失敗する原子的リネーム
    // 古いカーネル (ENOSYS/EINVAL) のみlinkフォールバックに進む
    const long renameat2Rc =
        ::syscall(SYS_renameat2, AT_FDCWD, fromN.constData(), AT_FDCWD,
                  toN.constData(), RENAME_NOREPLACE);
    if (renameat2Rc == 0) {
        published = true;
    } else if (errno == EEXIST) {
        if (error) {
            *error = tr("File already exists: %1").arg(targetPath);
        }
        return false;
    } else if (errno != ENOSYS && errno != EINVAL) {
        if (error) {
            *error = QString::fromLocal8Bit(strerror(errno));
        }
        return false;
    }
#endif
    if (!published) {
        // linkは宛先名が存在すればEEXISTで失敗する原子的操作である
        // ハードリンク未対応FSでは失敗するが、黙って置換はしない
        if (::link(fromN.constData(), toN.constData()) != 0) {
            if (error) {
                if (errno == EEXIST) {
                    *error = tr("File already exists: %1").arg(targetPath);
                } else {
                    *error = QString::fromLocal8Bit(strerror(errno));
                }
            }
            return false;
        }
        ::unlink(fromN.constData());
    }
    tmp.setAutoRemove(false);
    return true;
#endif
}

namespace {

/**
 * @brief DBディレクトリ内の最新のOPTIONSファイル名を返す
 *
 * RocksDBのGetLatestOptionsFileNameと同じ選択規則(番号最大)を用いる
 * 存在しない場合は、空文字列を返す
 */
QString latestOptionsFileName(const QString &canonicalPath)
{
    const QDir dir(canonicalPath);
    const QStringList files = dir.entryList({QStringLiteral("OPTIONS-*")}, QDir::Files);
    QString best;
    quint64 bestNum = 0;
    bool found = false;
    for (const QString &name : files) {
        if (!name.startsWith(QStringLiteral("OPTIONS-"))) {
            continue;
        }
        bool numOk = false;
        const quint64 n = name.mid(8).toULongLong(&numOk);
        if (!numOk) {
            continue;
        }
        if (!found || n > bestNum) {
            found = true;
            bestNum = n;
            best = name;
        }
    }
    return found ? dir.filePath(best) : QString();
}

/**
 * @brief OPTIONSファイル読み込み結果を表す列挙型
 */
enum class OptionsReadStatus {
    Ok,          // 正常に読み込んだ
    Missing,     // ファイルが見つからない
    TooLarge,    // 安全な読み込み上限を超えた
    Unreadable,  // ファイルを読み込めない
};

/**
 * @brief 最新のOPTIONSファイルを通読する
 *
 * 上限(kMaxOptionsProbeSize)を超えるファイルは読み込まずTooLargeを返す
 * チャンク読みで上限+1バイトまでしか保持しないため、巨大ファイルでもメモリを使い果たさない
 */
OptionsReadStatus readLatestOptionsContent(const QString &canonicalPath,
                                           QByteArray *content)
{
    const QString optionsPath = latestOptionsFileName(canonicalPath);
    if (optionsPath.isEmpty()) {
        return OptionsReadStatus::Missing;
    }
    if (QFileInfo(optionsPath).size() > kMaxOptionsProbeSize) {
        return OptionsReadStatus::TooLarge;
    }
    QFile file(optionsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return OptionsReadStatus::Unreadable;
    }
    QByteArray data;
    while (!file.atEnd()) {
        data += file.read(1 << 20);
        if (data.size() > kMaxOptionsProbeSize) {
            return OptionsReadStatus::TooLarge;
        }
        if (file.error() != QFile::NoError) {
            return OptionsReadStatus::Unreadable;
        }
    }
    if (content) {
        *content = data;
    }
    return OptionsReadStatus::Ok;
}

/**
 * @brief key = number形式の値を範囲内に収める
 *
 * 行ベースではなく全文走査のため、テーブルファクトリ値に埋め込まれた
 * {...;num_shard_bits=19;...}のような構造内指定にも効く
 * minValueにLLONG_MINを渡すと下限を検査しない
 */
QByteArray clampNumericOption(const QByteArray &content, const char *key,
                              qint64 minValue, qint64 maxValue)
{
    const QRegularExpression re(QStringLiteral("\\b%1\\s*=\\s*(-?\\d+)")
                                    .arg(QString::fromLatin1(key)));
    QByteArray result;
    result.reserve(content.size());
    qsizetype pos = 0;
    auto it = re.globalMatch(QString::fromLatin1(content));
    while (it.hasNext()) {
        const auto match = it.next();
        bool numOk = false;
        const qint64 value = match.captured(1).toLongLong(&numOk);
        qint64 clamped = value;
        if (numOk) {
            if (value > maxValue) {
                clamped = maxValue;
            } else if (minValue != LLONG_MIN && value < minValue) {
                clamped = minValue;
            }
        }
        const qsizetype start = match.capturedStart(1);
        const qsizetype end = match.capturedEnd(1);
        result += QByteArray::fromRawData(
            content.constData() + pos, start - pos);
        if (numOk && clamped != value) {
            result += QByteArray::number(clamped);
        } else {
            result += QByteArray::fromRawData(
                content.constData() + start, end - start);
        }
        pos = end;
    }
    result += QByteArray::fromRawData(content.constData() + pos,
                                      content.size() - pos);
    return result;
}

/**
 * @brief OPTIONS内容の無害化結果を保持する構造体
 */
struct SanitizeResult {
    bool ok = false;           // 無害化に成功したか
    QByteArray content;        // 無害化後のOPTIONS内容
    qint64 badNumLevels = 0;   // 拒否理由となったnum_levels値
};

/**
 * @brief OPTIONS内容をRocksDBのパース前に無害化する(S1)
 *
 * - キャッシュ系カスタマイザブル行 (block_cache等) を削除する
 *   共有キャッシュは常に自前の64MiB品に置換されるため、削除してもディスク上の解釈に影響しない
 * - 割り当て量に比例する数値 (num_shard_bits/bucket_count等) を上限化する
 * - num_levelsが[1,1000]の範囲外なら拒否する (拒否理由を持ち帰る)
 *   num_levelsは将来のcompaction配置に影響するため黙って書き換えない
 */
SanitizeResult sanitizeOptionsContent(const QByteArray &raw)
{
    static const char *kDroppedCacheKeys[] = {
        "block_cache",
        "block_cache_compressed",
        "persistent_cache",
        "row_cache",
        "blob_cache",
        "secondary_cache",
    };
    const QList<QByteArray> lines = raw.split('\n');
    QByteArray filtered;
    filtered.reserve(raw.size());
    bool first = true;
    for (const QByteArray &line : lines) {
        QByteArray key;
        const QString trimmed =
            QString::fromLatin1(line).trimmed();
        const bool isKv = !trimmed.startsWith(u'#')
            && !trimmed.startsWith(u'[') && trimmed.contains(u'=');
        if (isKv) {
            key = trimmed.section(u'=', 0, 0).trimmed().toLatin1();
        }
        bool drop = false;
        if (!key.isEmpty()) {
            for (const char *dropped : kDroppedCacheKeys) {
                if (key == dropped) {
                    drop = true;
                    break;
                }
            }
        }
        if (drop) {
            continue;
        }
        if (!first) {
            filtered += '\n';
        }
        first = false;
        filtered += line;
    }

    // 構造内に埋め込まれた指定にも効くよう全文走査で上限化する
    QByteArray clamped = clampNumericOption(filtered, "num_shard_bits",
                                            LLONG_MIN, kMaxCacheShardBits);
    clamped = clampNumericOption(clamped, "bucket_count", 1, kMaxBucketCount);
    clamped = clampNumericOption(clamped, "table_cache_numshardbits", 0,
                                 kMaxTableCacheShardBits);
    clamped = clampNumericOption(clamped, "inplace_update_num_locks",
                                 LLONG_MIN, kMaxMemtableLocks);
    clamped = clampNumericOption(clamped, "huge_page_tlb_size", LLONG_MIN,
                                 kMaxHugePageTlbSize);

    const QRegularExpression levelsRe(
        QStringLiteral("\\bnum_levels\\s*=\\s*(-?\\d+)"));
    auto levelIt = levelsRe.globalMatch(QString::fromLatin1(clamped));
    while (levelIt.hasNext()) {
        bool numOk = false;
        const qint64 levels = levelIt.next().captured(1).toLongLong(&numOk);
        if (numOk && (levels < 1 || levels > kMaxOptionsLevels)) {
            SanitizeResult result;
            result.ok = false;
            result.badNumLevels = levels;
            return result;
        }
    }

    SanitizeResult result;
    result.ok = true;
    result.content = clamped;
    return result;
}

/**
 * @brief CFの生名から表示名を生成する
 *
 * 有効なUTF-8ならデコード名、そうでなければ0x<hex>(non-UTF-8)形式にし、重複時は連番サフィックスで一意化する (B14)
 *
 * @param rawNames RocksDB由来のCF生名リスト
 * @return 表示名リスト (生名と同順・同要素数)
 */
QStringList displayNamesForColumnFamilies(const std::vector<std::string> &rawNames)
{
    QStringList display;
    display.reserve(static_cast<int>(rawNames.size()));
    for (const auto &raw : rawNames) {
        const rocksdb::Slice slice(raw);
        QString name;
        const auto decoded = utf8StringFromSlice(slice);
        if (decoded) {
            name = *decoded;
        } else {
            const QByteArray bytes(raw.data(), static_cast<qsizetype>(raw.size()));
            name = QStringLiteral("0x%1 (non-UTF-8)").arg(
                QString::fromLatin1(bytes.toHex()));
        }
        QString unique = name;
        int suffix = 2;
        while (display.contains(unique)) {
            unique = QStringLiteral("%1 #%2").arg(name).arg(suffix++);
        }
        display.append(unique);
    }
    return display;
}

} // namespace

/**
 * @struct RocksDBBackend::Impl
 * @brief RocksDBBackendのRocksDB関連状態を保持するPIMPL
 *
 * db : RocksDBデータベース本体を所有する
 * handles : dbより先に破棄する必要があるカラムファミリーハンドルを保持する
 * cfNames : QMLへ公開するカラムファミリーの表示名を保持する
 */
struct RocksDBBackend::Impl {
    // RocksDBリソース
    std::unique_ptr<rocksdb::DB> db;                         // RocksDBデータベース本体
    std::vector<rocksdb::ColumnFamilyHandle*> handles;       // dbより先に破棄するカラムファミリーハンドル

    // カラムファミリー状態
    QStringList cfNames;                                     // QMLへ公開するカラムファミリー表示名
    std::vector<std::string> cfRawNames;                     // RocksDBから取得したカラムファミリー生名
    QString currentCf;                                       // 現在選択中のカラムファミリー表示名

    // 接続状態
    QString dbPath;                                          // 開いているデータベースの正規化済みパス
    bool openedWithDefaults = false;                         // OPTIONS読み込み失敗時のフォールバック状態
    bool readOnly = false;                                   // 読み取り専用モード
    bool ttlDetected = false;                                // TTLデータベース検出状態
    bool editLocked = false;                                 // TTLまたはTTL状態未判定による編集禁止状態
    QString editLockReason;                                  // 編集禁止の理由

    // 永続設定
    QStringList recentDatabases;                             // 未使用の最近開いたデータベース一覧キャッシュ
};

/**
 * @brief RocksDBBackendを構築する
 *
 * @param parent 親QObject
 */
RocksDBBackend::RocksDBBackend(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
{
}

/**
 * @brief RocksDBBackendを破棄する
 *
 * デストラクタでデータベース接続を閉じる
 */
RocksDBBackend::~RocksDBBackend()
{
    closeDatabase();
}

/**
 * @brief データベースに接続しているかどうかを返す
 *
 * @return 接続中の場合はtrue
 */
bool RocksDBBackend::connected() const
{
    return d->db != nullptr;
}

/**
 * @brief 現在開いているデータベースのパスを返す
 *
 * @return データベースパス
 */
QString RocksDBBackend::databasePath() const
{
    return d->dbPath;
}

/**
 * @brief データベースに含まれるカラムファミリー名のリストを返す
 *
 * 非UTF-8名は、"0x<hex>(non-UTF-8)"形式の表示名になる
 *
 * @return カラムファミリー名のリスト
 */
QStringList RocksDBBackend::columnFamilies() const
{
    return d->cfNames;
}

/**
 * @brief 現在選択中のカラムファミリー名を返す
 *
 * @return カラムファミリー名
 */
QString RocksDBBackend::currentColumnFamily() const
{
    return d->currentCf;
}

/**
 * @brief 現在選択中のカラムファミリーを設定する
 *
 * 一覧に存在しない名は無視し、エラーを通知する
 *
 * @param cf カラムファミリー名
 */
void RocksDBBackend::setCurrentColumnFamily(const QString &cf)
{
    if (d->currentCf == cf) {
        return;
    }
    if (!d->cfNames.contains(cf)) {
        emit errorOccurred(tr("Column family not found: %1").arg(cf));
        return;
    }
    d->currentCf = cf;
    emit currentColumnFamilyChanged();
    emit dataChanged();
}

/**
 * @brief 最近開いたデータベースのパスリストを返す
 *
 * @return 最近のデータベースパスリスト（最大10件）
 */
QStringList RocksDBBackend::recentDatabases() const
{
    auto settings = SettingsMigration::newSettings();
    return settings.value(SettingsKeys::recentDatabases).toStringList();
}

/**
 * @brief 現在開いているDBがデフォルトオプションで開かれたかどうかを返す
 *
 * LoadLatestOptionsが失敗したフォールバック状態かどうか
 * この状態では編集結果が古いRocksDBツールで読めないSSTファイルになる可能性がある
 *
 * @return デフォルトオプションで開かれた場合はtrue
 */
bool RocksDBBackend::openedWithDefaults() const
{
    return d->openedWithDefaults;
}

/**
 * @brief データベースが読み取り専用モードで開かれているかを返す
 *
 * @return 読み取り専用で接続中の場合は true
 */
bool RocksDBBackend::readOnly() const
{
    return d->readOnly;
}

/**
 * @brief 開いているDBがTTLデータベースとして検出されたかを返す
 *
 * @return TTLデータベースの場合は true
 */
bool RocksDBBackend::ttlDetected() const
{
    return d->ttlDetected;
}

/**
 * @brief 編集がロックされているかどうかを返す
 *
 * TTL検出時またはTTL状態が判定不能な場合 (OPTIONS欠落・読み取り不可・巨大・パース失敗) にtrueになる
 */
bool RocksDBBackend::editLocked() const
{
    return d->editLocked;
}

/**
 * @brief 編集ロックの理由を返す
 *
 * ロックされていない場合は空文字列を返す
 */
QString RocksDBBackend::editLockReason() const
{
    return d->editLockReason;
}

/**
 * @brief 指定の正規化済みパスでDBを開く内部処理
 *
 * 状態シグナル(connectedChanged等)を一切出さず、dの内容を置き換える
 * 失敗時はdを空のままにし、*errorに理由を格納する
 * TTLデータベースが検出された場合は編集要求でも読み取り専用に強制し、*ttlForcedにtrueを格納する (B10)
 *
 * @param canonicalPath 正規化済みDBディレクトリパス
 * @param readOnly 読み取り専用で開く場合は true
 * @param error 失敗理由の格納先
 * @param ttlForced TTL強制読み取り専用の有無の格納先
 * @param usedDefaults LoadLatestOptions失敗フォールバックの有無の格納先
 * @return 開くことに成功した場合は true
 */
bool RocksDBBackend::openInternal(const QString &canonicalPath, bool readOnly,
                                 QString *error, bool *ttlForced,
                                 bool *usedDefaults)
{
    const std::string dbPath = utf8BytesFromString(canonicalPath);

    std::vector<std::string> rawCfs;
    const rocksdb::Status listStatus = rocksdb::DB::ListColumnFamilies(rocksdb::Options(), dbPath, &rawCfs);
    if (!listStatus.ok()) {
        rawCfs = {"default"};
    }

    // 信頼できないOPTIONSファイルの無害化 (S1):
    // RocksDBのパース前に自前で最新OPTIONSを通読・無害化する
    // キャッシュ系の行は削除し、割り当て量に比例する数値を上限化した複製を専用の一時ファイルに書き、それをLoadOptionsFromFileに渡す
    // これにより、パース時 (例:num_shard_bits=19) やOpen時 (例:num_levels=10^9) の巨大割り当てを未然に防ぐ
    std::shared_ptr<rocksdb::Cache> boundedCache = rocksdb::NewLRUCache(kBoundedCacheSize);

    rocksdb::ConfigOptions configOptions;
    configOptions.ignore_unknown_options = true;

    bool isTtl = false;
    bool ttlUnknown = false;
    QString lockReason;
    rocksdb::DBOptions loadedDbOptions = makeOpenDbOptions();
    std::vector<rocksdb::ColumnFamilyDescriptor> loadedDescs;
    bool useLoaded = false;

    QByteArray optionsContent;
    const OptionsReadStatus optionsRead = readLatestOptionsContent(canonicalPath, &optionsContent);
    if (optionsRead == OptionsReadStatus::Ok) {
        // 全文走査でTTL印を探す (B10)、上限以内の通読が前提のため、部分走査にはならない
        isTtl = optionsContent.contains("TtlCompactionFilterFactory")
            || optionsContent.contains("TtlMergeOperator");
        const SanitizeResult sanitized = sanitizeOptionsContent(optionsContent);
        if (!sanitized.ok) {
            if (error) {
                *error = tr("Refusing to open: the OPTIONS file specifies "
                            "unsupported num_levels (%1)")
                             .arg(sanitized.badNumLevels);
            }
            return false;
        }
        QTemporaryFile sanitizedFile;
        sanitizedFile.setAutoRemove(true);
        bool tempOk = sanitizedFile.open();
        if (tempOk) {
            qint64 written = 0;
            while (written < sanitized.content.size()) {
                const qint64 n = sanitizedFile.write(
                    sanitized.content.constData() + written,
                    sanitized.content.size() - written);
                if (n < 0) {
                    break;
                }
                written += n;
            }
            tempOk = written == sanitized.content.size();
        }
        if (tempOk) {
            sanitizedFile.close();
            tempOk = sanitizedFile.error() == QFile::NoError;
        }

        if (!tempOk) {
            ttlUnknown = true;
            lockReason = tr("Editing is locked: the OPTIONS file could not "
                            "be processed, so TTL status cannot be determined");
        }
        else {
            rocksdb::Status loadStatus;
            try {
                loadStatus = rocksdb::LoadOptionsFromFile(
                    configOptions,
                    utf8BytesFromString(
                        QFileInfo(sanitizedFile.fileName()).absoluteFilePath()),
                    &loadedDbOptions, &loadedDescs, &boundedCache);
            }
            catch (const std::exception &e) {
                // craftedな値 (Cache::CreateFromStringのstoull等) が例外でパースを中断させる場合の背止め
                loadStatus = rocksdb::Status::InvalidArgument(e.what());
            }
            catch (...) {
                loadStatus =
                    rocksdb::Status::InvalidArgument("options parsing failed");
            }
            useLoaded = loadStatus.ok();
            if (!useLoaded) {
                ttlUnknown = true;
                lockReason = tr(
                    "Editing is locked: the OPTIONS file could not be loaded "
                    "(%1), so TTL status cannot be determined")
                                 .arg(statusToQString(loadStatus));
            }
        }
    }
    else {
        ttlUnknown = true;
        if (optionsRead == OptionsReadStatus::Missing) {
            lockReason = tr("Editing is locked: no OPTIONS file was found, "
                            "so TTL status cannot be determined");
        }
        else if (optionsRead == OptionsReadStatus::TooLarge) {
            lockReason = tr("Editing is locked: the OPTIONS file exceeds "
                            "the 4 MiB safety limit, so TTL status cannot "
                            "be determined");
        }
        else {
            lockReason = tr("Editing is locked: the OPTIONS file could not "
                            "be read, so TTL status cannot be determined");
        }
    }

    if (isTtl) {
        lockReason = tr("TTL database detected: editing is locked because "
                        "plain writes would corrupt TTL timestamps");
    }

    if (useLoaded) {
        // 外部WALディレクトリの拒否(S1a): 黙ってクリアすると
        // 正規の外部WAL運用DBが未フラッシュデータを失うため、開くこと自体を拒否する
        const QString loadedWalDir =
            QString::fromStdString(loadedDbOptions.wal_dir);
        if (!loadedWalDir.isEmpty()) {
            const QString canonicalWalDir =
                QFileInfo(loadedWalDir).canonicalFilePath();
            if (canonicalWalDir.isEmpty() || canonicalWalDir != canonicalPath) {
                if (error) {
                    *error = tr("Refusing to open: the OPTIONS file specifies "
                                "an external WAL directory (%1)")
                                 .arg(loadedWalDir);
                }
                return false;
            }
        }
    }

    // 最終的なDBOptionsは安全な既定値から構築する(S1b)
    // DBOptionsはディスク上のフォーマットに影響しないため、永続化ファイル由来の値はホワイトリスト分だけ引き継ぎ、
    // db_log_dir・wal_filter・スレッド/キャッシュ/シャード数なそれ以外はすべて破棄する
    rocksdb::DBOptions dbOptions = makeOpenDbOptions();
    if (useLoaded) {
        dbOptions.allow_2pc = loadedDbOptions.allow_2pc;
        dbOptions.atomic_flush = loadedDbOptions.atomic_flush;
        dbOptions.allow_mmap_reads = loadedDbOptions.allow_mmap_reads;
    }

    std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
    descriptors.reserve(rawCfs.size());
    for (const auto &name : rawCfs) {
        rocksdb::ColumnFamilyOptions cfOptions;
        if (useLoaded) {
            const auto it = std::find_if(
                loadedDescs.begin(), loadedDescs.end(),
                [&name](const rocksdb::ColumnFamilyDescriptor &desc) {
                    return desc.name == name;
                });
            if (it != loadedDescs.end()) {
                cfOptions = it->options;
            }
        }
        // 永続化CFオプションの上限化(S1c): comparator・table_factory・merge_operator・compression・prefix_extractorなど
        // データ読み取りに必要な設定は保持する
        if (cfOptions.write_buffer_size > kBoundedCacheSize) {
            cfOptions.write_buffer_size = kBoundedCacheSize;
        }
        cfOptions.arena_block_size = 0;
        cfOptions.memtable_huge_page_size = 0;
        if (cfOptions.max_write_buffer_number > 4) {
            cfOptions.max_write_buffer_number = 4;
        }
        descriptors.emplace_back(name, cfOptions);
    }

    // TTL検出またはTTL判定不能の場合は編集要求でも読み取り専用に強制し、編集ロックとして記録する (B10,C)
    const bool editLocked = isTtl || ttlUnknown;
    const bool effectiveReadOnly = readOnly || editLocked;
    if (ttlForced) {
        *ttlForced = editLocked;
    }

    if (usedDefaults) {
        *usedDefaults = !useLoaded;
    }

    if (effectiveReadOnly) {
        // 読み取り専用ではDBディレクトリにLOGファイルを生成・ローテートしないよう、info_logを/dev/null行きのロガーに向ける (S2)
        // librocksdbにLoggerのRTTIが無いため自作サブクラスはリンク不可であり、Env標準のファイルロガーを使う
        std::shared_ptr<rocksdb::Logger> nullLog;
#ifdef Q_OS_WIN
        const char *nullDevice = "NUL";
#else
        const char *nullDevice = "/dev/null";
#endif
        if (rocksdb::Env::Default()->NewLogger(nullDevice, &nullLog).ok() && nullLog) {
            dbOptions.info_log = nullLog;
        }
    }

    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    std::unique_ptr<rocksdb::DB> db;
#if ROCKSDB_MAJOR >= 10
    // RocksDB 10+ exposes only the std::unique_ptr<DB>* overload.
    rocksdb::Status s;
    if (effectiveReadOnly) {
        s = rocksdb::DB::OpenForReadOnly(dbOptions, dbPath, descriptors,
                                         &handles, &db, false);
    } else {
        s = rocksdb::DB::Open(dbOptions, dbPath, descriptors, &handles, &db);
    }
#else
    rocksdb::DB* rawDb = nullptr;
    rocksdb::Status s;
    if (effectiveReadOnly) {
        // RocksDB9系以前の生ポインタ版OpenForReadOnly引き数に合わせる
        s = rocksdb::DB::OpenForReadOnly(dbOptions, dbPath, descriptors, &handles, &rawDb, false);
        db.reset(rawDb);
    }
    else {
        s = rocksdb::DB::Open(dbOptions, dbPath, descriptors, &handles, &rawDb);
        db.reset(rawDb);
    }
#endif
    if (!s.ok()) {
        if (db) {
            for (auto* h : handles) {
                db->DestroyColumnFamilyHandle(h);
            }
        }
        else {
            for (auto* h : handles) {
                delete h;
            }
        }

        if (error) {
            *error = statusToQString(s);
        }
        return false;
    }

    d->db = std::move(db);
    d->handles = handles;
    d->dbPath = canonicalPath;
    d->cfRawNames = rawCfs;
    d->cfNames = displayNamesForColumnFamilies(rawCfs);

    if (!d->cfNames.isEmpty()) {
        d->currentCf = d->cfNames.first();
    }
    else {
        d->currentCf.clear();
    }

    d->readOnly = effectiveReadOnly;
    d->ttlDetected = isTtl;
    d->editLocked = editLocked;
    d->editLockReason = editLocked ? lockReason : QString();
    d->openedWithDefaults = !useLoaded;

    return true;
}

/**
 * @brief 状態シグナルを出さずに開いているDBを閉じる
 *
 * errorOccurredは出し、openDatabase/setReadOnlyMode内部の閉鎖・再オープン処理で使用する
 */
void RocksDBBackend::closeInternal()
{
    if (!d->db) {
        return;
    }
    // 読み取り専用モードではSyncWAL/Flushを省略する (S2)
    // 読み取り専用DBに対するこれらはNotSupportedエラーを返すだけのため
    if (!d->readOnly) {
        const rocksdb::Status syncStatus = d->db->SyncWAL();
        if (!syncStatus.ok() && syncStatus.IsNotSupported()) {
            rocksdb::FlushOptions flushOptions;
            flushOptions.wait = true;
            for (auto* h : d->handles) {
                const rocksdb::Status flushStatus = d->db->Flush(flushOptions, h);
                if (!flushStatus.ok()) {
                    emit errorOccurred(statusToQString(flushStatus));
                    qWarning() << "RocksDB Flush before close failed:"
                               << statusToQString(flushStatus);
                }
            }
        }
        else if (!syncStatus.ok()) {
            emit errorOccurred(statusToQString(syncStatus));
            qWarning() << "RocksDB SyncWAL before close failed:"
                       << statusToQString(syncStatus);
        }
    }

    for (auto* h : d->handles) {
        const rocksdb::Status s = d->db->DestroyColumnFamilyHandle(h);
        if (!s.ok()) {
            emit errorOccurred(statusToQString(s));
        }
    }
    d->handles.clear();
    const rocksdb::Status closeStatus = d->db->Close();

    if (!closeStatus.ok()) {
        emit errorOccurred(statusToQString(closeStatus));
        qWarning() << "RocksDB close failed:" << statusToQString(closeStatus);
    }

    d->db.reset();
    d->dbPath.clear();
    d->cfNames.clear();
    d->cfRawNames.clear();
    d->currentCf.clear();
}

/**
 * @brief DB状態に関する全シグナルを出す
 */
void RocksDBBackend::emitAllStateSignals()
{
    emit connectedChanged();
    emit databasePathChanged();
    emit columnFamiliesChanged();
    emit currentColumnFamilyChanged();
    emit readOnlyChanged();
    emit ttlDetectedChanged();
    emit editLockedChanged();
    emit dataChanged();
}

/**
 * @brief 指定パスのRocksDBデータベースを開く
 *
 * 既定では読み取り専用で開き、パスは一度だけ正規化し、以降のRocksDB呼び出し・保存にはすべて正規化パスを使う (T2)
 * 異なるパスのオープンに失敗しても以前のDBには触れない
 * 同一パスの開き直しに失敗した場合は以前の状態への復元を試み、復元自体の失敗も報告する (G)
 * 編集ロック中のDBに対する編集要求は読み取り専用に強制し、理由を通知する (B10,C)
 */
bool RocksDBBackend::openDatabase(const QString &path, bool readOnly)
{
    const QString canonicalPath = QFileInfo(path).canonicalFilePath();
    if (canonicalPath.isEmpty() || !QDir(canonicalPath).exists()) {
        emit errorOccurred(tr("Database directory does not exist: %1").arg(path));
        return false;
    }

    // 別パスの場合は候補を先に開き、成功時のみ交換する (G)
    // 旧DBを先に閉じないため、失敗しても旧DBは無傷で使える
    if (d->db && canonicalPath != d->dbPath) {
        RocksDBBackend staging;
        staging.blockSignals(true);
        QString error;
        bool ttlForced = false;
        bool usedDefaults = false;
        if (!staging.openInternal(canonicalPath, readOnly, &error, &ttlForced, &usedDefaults)) {
            emit errorOccurred(error);
            return false;
        }
        const bool prevDefaults = d->openedWithDefaults;
        closeInternal();
        d->db = std::move(staging.d->db);
        d->handles = std::move(staging.d->handles);
        d->dbPath = staging.d->dbPath;
        d->cfRawNames = staging.d->cfRawNames;
        d->cfNames = staging.d->cfNames;
        d->currentCf = staging.d->currentCf;
        d->readOnly = staging.d->readOnly;
        d->ttlDetected = staging.d->ttlDetected;
        d->editLocked = staging.d->editLocked;
        d->editLockReason = staging.d->editLockReason;
        d->openedWithDefaults = staging.d->openedWithDefaults;

        if (d->openedWithDefaults != prevDefaults) {
            emit openedWithDefaultsChanged();
        }

        if (d->openedWithDefaults) {
            qWarning() << "Database opened with default options (LoadLatestOptions failed)."
                       << "Edits may produce SST files unreadable by older RocksDB tooling.";
            emit toastRequested(
                tr("Database opened with default options. "
                   "Edits may not be readable by older RocksDB tools."),
                "warning");
        }

        // 編集要求が読み取り専用に強制された場合のみ理由を通知する (C)
        // 明示的な読み取り専用オープンでは何も言わない
        if (!readOnly && ttlForced) {
            emit errorOccurred(d->editLockReason);
            emit toastRequested(d->editLockReason, "warning");
        }

        addToRecent(canonicalPath);
        emitAllStateSignals();
        return true;
    }

    const QString prevPath = d->dbPath;
    const bool prevReadOnly = d->readOnly;
    const QString prevCf = d->currentCf;
    const bool hadPrev = (d->db != nullptr);
    const bool prevDefaults = d->openedWithDefaults;

    closeInternal();
    d->openedWithDefaults = false;
    d->readOnly = false;
    d->ttlDetected = false;
    d->editLocked = false;
    d->editLockReason.clear();

    QString error;
    bool ttlForced = false;
    bool usedDefaults = false;
    if (!openInternal(canonicalPath, readOnly, &error, &ttlForced, &usedDefaults)) {
        // 新しいDBのオープンに失敗したら以前のDBを復元する (B7)
        QString restoreError;
        bool restoreTtl = false;
        bool restoreDefaults = false;
        if (hadPrev) {
            if (openInternal(prevPath, prevReadOnly, &restoreError, &restoreTtl,
                             &restoreDefaults)) {
                if (!d->cfNames.contains(prevCf)) {
                    d->currentCf = d->cfNames.isEmpty() ? QString()
                                                        : d->cfNames.first();
                }
                else {
                    d->currentCf = prevCf;
                }
            }
            else {
                closeInternal();
                d->openedWithDefaults = false;
                d->readOnly = false;
                d->ttlDetected = false;
                d->editLocked = false;
                d->editLockReason.clear();
                emit errorOccurred(tr("Failed to restore the previous database: %1").arg(restoreError));
            }
        }

        emit errorOccurred(error);
        emitAllStateSignals();

        if (prevDefaults != d->openedWithDefaults) {
            emit openedWithDefaultsChanged();
        }

        return false;
    }

    if (d->openedWithDefaults != prevDefaults) {
        emit openedWithDefaultsChanged();
    }

    if (d->openedWithDefaults) {
        qWarning() << "Database opened with default options (LoadLatestOptions failed)."
                   << "Edits may produce SST files unreadable by older RocksDB tooling.";
        emit toastRequested(
            tr("Database opened with default options. "
               "Edits may not be readable by older RocksDB tools."),
            "warning");
    }

    // 編集要求が読み取り専用に強制された場合のみ理由を通知する (C)
    // 明示的な読み取り専用オープンでは何も言わない
    if (!readOnly && ttlForced) {
        emit errorOccurred(d->editLockReason);
        emit toastRequested(d->editLockReason, "warning");
    }

    addToRecent(canonicalPath);
    emitAllStateSignals();

    return true;
}

/**
 * @brief 現在のDBを指定モードで開き直す
 *
 * モードが変わらない場合は何もせずtrueを返す
 * 未接続時はfalseを返し、編集ロック中の編集要求は拒否する
 * 成功時は以前に選択されていたカラムファミリーを (残っていれば) 復元する (D)
 * 再オープン失敗時は以前のモードに復元し、復元自体の失敗も報告する (G)
 * 編集ロック中の編集要求は拒否する
 */
bool RocksDBBackend::setReadOnlyMode(bool readOnly)
{
    if (!d->db) {
        return false;
    }
    if (d->readOnly == readOnly) {
        return true;
    }
    if (!readOnly && d->editLocked) {
        emit errorOccurred(d->editLockReason.isEmpty()
                               ? tr("Editing is locked for this database")
                               : d->editLockReason);
        return false;
    }
    const QString currentPath = d->dbPath;
    const QString currentCf = d->currentCf;
    const bool prevDefaults = d->openedWithDefaults;

    closeInternal();
    d->openedWithDefaults = false;
    d->readOnly = false;
    d->ttlDetected = false;
    d->editLocked = false;
    d->editLockReason.clear();

    QString error;
    bool ttlForced = false;
    bool usedDefaults = false;
    if (!openInternal(currentPath, readOnly, &error, &ttlForced, &usedDefaults)) {
        QString restoreError;
        bool restoreTtl = false;
        bool restoreDefaults = false;
        if (openInternal(currentPath, !readOnly, &restoreError, &restoreTtl,
                         &restoreDefaults)) {
            if (d->cfNames.contains(currentCf)) {
                d->currentCf = currentCf;
            }
        } else {
            closeInternal();
            d->openedWithDefaults = false;
            d->readOnly = false;
            d->ttlDetected = false;
            d->editLocked = false;
            d->editLockReason.clear();
            emit errorOccurred(
                tr("Failed to restore the previous database: %1")
                    .arg(restoreError));
        }
        emit errorOccurred(error);
        emitAllStateSignals();
        if (prevDefaults != d->openedWithDefaults) {
            emit openedWithDefaultsChanged();
        }
        return false;
    }
    // 成功時は選択されていたCFを復元する (D)
    if (!currentCf.isEmpty() && d->cfNames.contains(currentCf)) {
        d->currentCf = currentCf;
    }
    if (d->openedWithDefaults != prevDefaults) {
        emit openedWithDefaultsChanged();
    }
    if (ttlForced) {
        emit toastRequested(d->editLockReason, "warning");
    }
    emitAllStateSignals();
    return true;
}

/**
 * @brief 開いているデータベースを閉じる
 *
 * 読み取り専用モードではSyncWAL/Flushを省略する
 */
void RocksDBBackend::closeDatabase()
{
    if (!d->db) {
        return;
    }

    // 後続のオープンをクリーンな状態から始められるよう、Issue #2のフォールバックフラグをリセットする
    if (d->openedWithDefaults) {
        d->openedWithDefaults = false;
        emit openedWithDefaultsChanged();
    }

    closeInternal();
    const bool wasReadOnly = d->readOnly;
    const bool hadTtl = d->ttlDetected;
    const bool wasLocked = d->editLocked;
    d->openedWithDefaults = false;
    d->readOnly = false;
    d->ttlDetected = false;
    d->editLocked = false;
    d->editLockReason.clear();

    emit connectedChanged();
    emit databasePathChanged();
    emit columnFamiliesChanged();
    emit currentColumnFamilyChanged();
    if (wasReadOnly) {
        emit readOnlyChanged();
    }
    if (hadTtl) {
        emit ttlDetectedChanged();
    }
    if (wasLocked) {
        emit editLockedChanged();
    }
}

/**
 * @brief 指定名のカラムファミリーハンドルを返す
 *
 * d->cfNames上の表示名の添字で引き、handlesと平行に対応付ける
 * 非UTF-8名を再エンコードして照合することはしない (B14)
 *
 * @param name カラムファミリー名 (表示名)
 * @return ハンドルポインタ、見つからない場合はnullptr
 */
rocksdb::ColumnFamilyHandle* RocksDBBackend::getColumnFamilyHandle(const QString &name)
{
    if (!d->db) {
        return nullptr;
    }
    const int index = d->cfNames.indexOf(name);
    if (index < 0 || index >= static_cast<int>(d->handles.size())) {
        return nullptr;
    }
    return d->handles[static_cast<size_t>(index)];
}

/**
 * @brief 現在のカラムファミリーから検索・ページングしてデータを取得する
 *
 * キーと値の両方を大文字小文字を無視して検索する
 * 非UTF-8のエントリはスキップする
 * ページが上限で埋まっただけでは警告トーストを出さず、ページ外にさらに一致エントリがある場合のみ警告する (B4)
 *
 * @param search 検索文字列 (空の場合は全件)
 * @param offset オフセット
 * @param limit 取得上限 ([1,10000]に丸める)
 * @return キーと値のペアを含むQJsonObject
 */
QJsonObject RocksDBBackend::getData(const QString &search, int offset, int limit)
{
    QJsonObject result;
    if (!d->db) {
        return result;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return result;
    }

    const int safeOffset = std::max(0, offset);
    const int effectiveLimit = std::clamp(limit <= 0 ? 1000 : limit, 1, 10000);

    rocksdb::ReadOptions ro;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    const QString term = search.toLower();
    int count = 0;
    int matched = 0;
    bool skippedBinary = false;
    bool hasMore = false;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        const auto key = utf8StringFromSlice(it->key());
        const auto value = utf8StringFromSlice(it->value());
        if (!key || !value) {
            skippedBinary = true;
            continue;
        }
        if (!term.isEmpty()
            && !key->toLower().contains(term)
            && !value->toLower().contains(term)) {
            continue;
        }
        if (matched >= safeOffset && count < effectiveLimit) {
            result[*key] = *value;
            count++;
        }
        matched++;
        if (count >= effectiveLimit && matched > safeOffset + effectiveLimit) {
            hasMore = true;
            break;
        }
    }
    if (!it->status().ok()) {
        emit errorOccurred(QString::fromStdString(it->status().ToString()));
        return result;
    }
    if (skippedBinary) {
        emit toastRequested(tr("Skipped entries containing non-UTF-8 keys or values"), "warning");
    }
    if (hasMore) {
        emit toastRequested(tr("Loaded %1 entries (limit reached)").arg(effectiveLimit), "warning");
    }
    return result;
}

/**
 * @brief 単一スナップショット上で検索・ページングしてデータを取得する
 *
 * カウントとページ取得を1本のイテレータ・1つの暗黙スナップショットで行い、
 * カウントとページの競合を除去する(B4)、トーストは出さない
 *
 * @param search 検索文字列 (空の場合は全件)
 * @param offset オフセット (0以上に丸める)
 * @param limit 取得上限 ([1,10000]に丸める)
 * @param maxCount 集計上限
 * @return entries/total/capped/hiddenNonUtf8を含むQJsonObject
 */
QJsonObject RocksDBBackend::getPage(const QString &search, int offset, int limit,
                                   int maxCount)
{
    QJsonObject result;
    QJsonObject entries;
    if (!d->db) {
        result["entries"] = entries;
        result["total"] = 0;
        result["capped"] = false;
        result["hiddenNonUtf8"] = 0;
        return result;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        result["entries"] = entries;
        result["total"] = 0;
        result["capped"] = false;
        result["hiddenNonUtf8"] = 0;
        return result;
    }

    const int safeOffset = std::max(0, offset);
    const int effectiveLimit = std::clamp(limit <= 0 ? 1000 : limit, 1, 10000);
    const int safeMax = std::max(1, maxCount);
    const int effectiveMax = std::max(safeMax, safeOffset + effectiveLimit);

    rocksdb::ReadOptions ro;
    const rocksdb::Snapshot *snapshot = d->db->GetSnapshot();
    ro.snapshot = snapshot;
    const QString term = search.toLower();
    int matches = 0;
    int hiddenNonUtf8 = 0;

    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        const auto key = utf8StringFromSlice(it->key());
        const auto value = utf8StringFromSlice(it->value());
        if (!key || !value) {
            hiddenNonUtf8++;
            continue;
        }
        if (!term.isEmpty()
            && !key->toLower().contains(term)
            && !value->toLower().contains(term)) {
            continue;
        }
        if (matches >= safeOffset && matches < safeOffset + effectiveLimit) {
            entries[*key] = *value;
        }
        matches++;
        if (matches - safeOffset >= effectiveLimit && matches > effectiveMax) {
            break;
        }
    }
    const rocksdb::Status iterStatus = it->status();
    it.reset();
    d->db->ReleaseSnapshot(snapshot);
    if (!iterStatus.ok()) {
        emit errorOccurred(statusToQString(iterStatus));
        result["entries"] = QJsonObject();
        result["total"] = 0;
        result["capped"] = false;
        result["hiddenNonUtf8"] = hiddenNonUtf8;
        return result;
    }

    result["entries"] = entries;
    result["total"] = std::min(matches, effectiveMax);
    result["capped"] = matches > effectiveMax;
    result["hiddenNonUtf8"] = hiddenNonUtf8;
    return result;
}

/**
 * @brief 指定プレフィックスで始まるキーのデータを取得する
 *
 * 生キーで前方一致を判定し、範囲外で即座に走査を打ち切る
 * prefix_extractor由来の誤った打ち切りを避けるため、prefix_same_as_startは使わずtotal_order_seekとする (B9)
 * 上限到達時の警告は、上限外にさらに一致がある場合のみ出す
 *
 * @param prefix 検索プレフィックス
 * @param limit 取得上限
 * @return キーと値のペアを含むQJsonObject
 */
QJsonObject RocksDBBackend::getDataByPrefix(const QString &prefix, int limit)
{
    QJsonObject result;
    if (!d->db || prefix.isEmpty()) {
        return result;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return result;
    }

    const int effectiveLimit = limit <= 0 ? 10000 : limit;
    const std::string prefixBytes = utf8BytesFromString(prefix);
    const rocksdb::Slice prefixSlice(prefixBytes);

    rocksdb::ReadOptions ro;
    ro.total_order_seek = true;

    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    int count = 0;
    bool skippedBinary = false;
    bool hasMore = false;
    for (it->Seek(prefixSlice); it->Valid(); it->Next()) {
        if (!it->key().starts_with(prefixSlice)) {
            break;
        }
        const auto key = utf8StringFromSlice(it->key());
        const auto value = utf8StringFromSlice(it->value());
        if (!key || !value) {
            skippedBinary = true;
            continue;
        }
        if (count >= effectiveLimit) {
            hasMore = true;
            break;
        }
        result[*key] = *value;
        count++;
    }
    if (!it->status().ok()) {
        emit errorOccurred(QString::fromStdString(it->status().ToString()));
        return result;
    }
    if (skippedBinary) {
        emit toastRequested(tr("Skipped entries containing non-UTF-8 keys or values"), "warning");
    }
    if (hasMore) {
        emit toastRequested(tr("Loaded %1 entries (limit reached)").arg(effectiveLimit), "warning");
    }
    return result;
}

/**
 * @brief 指定キーの値を取得する
 *
 * @param key キー
 * @return キーと値のペアを含むQJsonObject、見つからない場合は空
 */
QJsonObject RocksDBBackend::getDataByKey(const QString &key)
{
    QJsonObject result;
    if (!d->db) {
        return result;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return result;
    }

    std::string value;
    const rocksdb::Status s =
        d->db->Get(rocksdb::ReadOptions(), cf, utf8BytesFromString(key), &value);
    if (s.ok()) {
        const rocksdb::Slice valueSlice(value);
        const auto textValue = utf8StringFromSlice(valueSlice);
        if (!textValue) {
            emit toastRequested(tr("Skipped entry containing non-UTF-8 value"), "warning");
            return result;
        }
        result["key"] = key;
        result["value"] = *textValue;
    }
    return result;
}

/**
 * @brief 現在のカラムファミリーにキー・値を書き込む
 *
 * 読み取り専用モードでは拒否する
 *
 * @param key キー
 * @param value 値
 * @return 書き込みに成功した場合は true
 */
bool RocksDBBackend::setData(const QString &key, const QString &value)
{
    if (!d->db || key.isEmpty()) {
        return false;
    }
    if (d->readOnly) {
        emit errorOccurred(
            tr("The database is open in read-only mode. Enable edit mode to modify data."));
        return false;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return false;
    }

    const rocksdb::Status s = d->db->Put(makeDurableWriteOptions(), cf,
                                         utf8BytesFromString(key),
                                         utf8BytesFromString(value));
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    emit dataChanged();
    emit toastRequested(tr("Data saved"), "success");
    return true;
}

/**
 * @brief 現在のカラムファミリーから指定キーを削除する
 *
 * 読み取り専用モードでは拒否する
 *
 * @param key 削除対象キー
 * @return 削除に成功した場合はtrue
 */
bool RocksDBBackend::deleteData(const QString &key)
{
    if (!d->db) {
        return false;
    }
    if (d->readOnly) {
        emit errorOccurred(
            tr("The database is open in read-only mode. Enable edit mode to modify data."));
        return false;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return false;
    }

    const rocksdb::Status s = d->db->Delete(makeDurableWriteOptions(), cf,
                                            utf8BytesFromString(key));
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    emit dataChanged();
    emit toastRequested(tr("Data deleted"), "success");
    return true;
}

/**
 * @brief 現在のカラムファミリーの全データを削除する
 *
 * 書き込み前にテーブル形式のDeleteRange対応を判定する (S5)
 * PlainTable等の未対応形式では最初からキー毎バッチで削除する
 * 対応形式では先頭・末尾キーを求め、DeleteRange+Deleteの1バッチでアトミックに削除し、DeleteRange試行後のフォールバックは行わない
 * Write成功後は永続化失敗時もdataChangedを出し、エラーを通知してfalseを返す (B7)
 *
 * 読み取り専用モードでは拒否する
 *
 * @param filePath 入力ファイルパス
 * @return インポートに成功した場合はtrue
 */
bool RocksDBBackend::clearData()
{
    if (!d->db) {
        return false;
    }
    if (d->readOnly) {
        emit errorOccurred(
            tr("The database is open in read-only mode. Enable edit mode to modify data."));
        return false;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return false;
    }

    // DeleteRange未対応形式 (PlainTable等) ではDeleteRangeの書き込み自体が失敗し、
    // DBのバックグラウンドエラーとして記録されて後続の書き込みも壊すため、書き込み前に判定して経路を分ける
    // BlockBasedTableFactoryはIsDeleteRangeSupported()==true、PlainTableFactoryは継承既定のfalseを返す
    bool rangeSupported = false;
    const rocksdb::Options liveOptions = d->db->GetOptions(cf);
    if (liveOptions.table_factory) {
        rangeSupported = liveOptions.table_factory->IsDeleteRangeSupported();
    }

    rocksdb::Status s;
    if (rangeSupported) {
        rocksdb::ReadOptions ro;
        std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
        it->SeekToFirst();
        if (!it->Valid()) {
            if (!it->status().ok()) {
                emit errorOccurred(statusToQString(it->status()));
                return false;
            }
            return true;
        }
        const std::string firstKey(it->key().data(), it->key().size());
        it->SeekToLast();
        if (!it->status().ok()) {
            emit errorOccurred(statusToQString(it->status()));
            return false;
        }
        const std::string lastKey(it->key().data(), it->key().size());
        it.reset();

        rocksdb::WriteBatch batch;
        s = batch.DeleteRange(cf, rocksdb::Slice(firstKey),
                              rocksdb::Slice(lastKey));
        if (s.ok()) {
            s = batch.Delete(cf, rocksdb::Slice(lastKey));
        }
        if (s.ok()) {
            s = d->db->Write(makeBatchWriteOptions(), &batch);
        }
    } else {
        rocksdb::WriteBatch batch;
        std::unique_ptr<rocksdb::Iterator> scan(
            d->db->NewIterator(rocksdb::ReadOptions(), cf));
        for (scan->SeekToFirst(); scan->Valid(); scan->Next()) {
            const rocksdb::Status putStatus = batch.Delete(cf, scan->key());
            if (!putStatus.ok()) {
                emit errorOccurred(statusToQString(putStatus));
                return false;
            }
        }
        if (!scan->status().ok()) {
            emit errorOccurred(statusToQString(scan->status()));
            return false;
        }
        s = d->db->Write(makeBatchWriteOptions(), &batch);
    }
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    const rocksdb::Status syncStatus = persistColumnFamily(d->db.get(), cf);
    emit dataChanged();
    if (!syncStatus.ok()) {
        emit errorOccurred(statusToQString(syncStatus));
        return false;
    }
    emit toastRequested(tr("All data deleted"), "success");
    return true;
}

/**
 * @brief 現在のカラムファミリーの全データをJSONとしてエクスポートする
 *
 * JSONとして解析可能な値はオブジェクト/配列に変換し、それ以外は文字列として返す
 * 反復子エラー時は部分結果ではなく空オブジェクトを返し、エラーを通知する (S4)
 *
 * @return キーと値のペアを含むQJsonObject
 */
QJsonObject RocksDBBackend::exportData()
{
    QJsonObject result;
    if (!d->db) {
        return result;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return result;
    }

    rocksdb::ReadOptions ro;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    bool skippedBinary = false;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        const auto key = utf8StringFromSlice(it->key());
        const auto raw = utf8StringFromSlice(it->value());
        if (!key || !raw) {
            skippedBinary = true;
            continue;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw->toUtf8());

        if (doc.isObject()) {
            result[*key] = doc.object();
        }
        else if (doc.isArray()) {
            result[*key] = doc.array();
        }
        else {
            result[*key] = *raw;
        }
    }

    if (!it->status().ok()) {
        emit errorOccurred(QString::fromStdString(it->status().ToString()));
        return QJsonObject();
    }

    if (skippedBinary) {
        emit toastRequested(tr("Skipped entries containing non-UTF-8 keys or values"), "warning");
    }
    return result;
}

/**
 * @brief JSONデータを現在のカラムファミリーに一括インポートする
 *
 * 文字列はそのまま、オブジェクト/配列はCompact JSONで格納する
 * その他のスカラー (数値・真偽値・null) はcompact JSONテキストとして格納する (S4)
 * Write成功後は永続化失敗時もdataChangedを出し、エラーを通知してfalseを返す (B7)
 * 読み取り専用モードでは拒否する
 *
 * @param data キーと値のペアを含むQJsonObject
 * @return インポートに成功した場合はtrue
 */
bool RocksDBBackend::importData(const QJsonObject &data)
{
    if (!d->db) {
        return false;
    }

    if (d->readOnly) {
        emit errorOccurred(
            tr("The database is open in read-only mode. Enable edit mode to modify data."));
        return false;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return false;
    }

    rocksdb::WriteBatch batch;
    int count = 0;
    for (auto it = data.begin(); it != data.end(); ++it) {
        QString valueStr;
        if (it.value().isObject()) {
            valueStr = QString::fromUtf8(
                QJsonDocument(it.value().toObject()).toJson(QJsonDocument::Compact));
        }
        else if (it.value().isArray()) {
            valueStr = QString::fromUtf8(
                QJsonDocument(it.value().toArray()).toJson(QJsonDocument::Compact));
        }
        else if (it.value().isString()) {
            valueStr = it.value().toString();
        }
        else {
            const QByteArray scalar =
                QJsonDocument(QJsonArray{it.value()}).toJson(QJsonDocument::Compact);
            valueStr = QString::fromUtf8(scalar.mid(1, scalar.size() - 2));
        }

        const rocksdb::Status s = batch.Put(cf, utf8BytesFromString(it.key()), utf8BytesFromString(valueStr));
        if (!s.ok()) {
            emit errorOccurred(statusToQString(s));
            return false;
        }
        count++;
    }

    const rocksdb::Status s = d->db->Write(makeBatchWriteOptions(), &batch);
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    const rocksdb::Status syncStatus = persistColumnFamily(d->db.get(), cf);
    emit dataChanged();
    if (!syncStatus.ok()) {
        emit errorOccurred(statusToQString(syncStatus));
        return false;
    }
    emit toastRequested(tr("%1 records imported").arg(count), "success");
    return true;
}

/**
 * @brief 現在のカラムファミリーの合計エントリ数を返す
 *
 * maxCount件まで数えて打ち切り、トーストは出さない (B4)
 *
 * @param search 検索文字列（空の場合は全件）
 * @param maxCount 集計上限
 * @return エントリ数
 */
int RocksDBBackend::getTotalEntryCount(const QString &search, int maxCount)
{
    if (!d->db) {
        return 0;
    }

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return 0;
    }

    const int safeMax = std::max(1, maxCount);
    rocksdb::ReadOptions ro;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    const QString term = search.toLower();
    int count = 0;
    for (it->SeekToFirst(); it->Valid() && count < safeMax; it->Next()) {
        const auto key = utf8StringFromSlice(it->key());
        const auto value = utf8StringFromSlice(it->value());
        if (!key || !value) {
            continue;
        }
        if (term.isEmpty() || key->toLower().contains(term)
            || value->toLower().contains(term)) {
            count++;
        }
    }
    if (!it->status().ok()) {
        emit errorOccurred(QString::fromStdString(it->status().ToString()));
        return count;
    }
    return count;
}

/**
 * @brief 指定キーが現在のカラムファミリーに存在するかを返す
 *
 * QMLの追加モード上書き確認に使う (B7)
 *
 * @param key キー
 * @return 存在する場合はtrue
 */
bool RocksDBBackend::keyExists(const QString &key)
{
    if (!d->db) {
        return false;
    }
    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return false;
    }

    std::string value;
    const rocksdb::Status s = d->db->Get(rocksdb::ReadOptions(), cf, utf8BytesFromString(key), &value);

    return s.ok();
}

/**
 * @brief 現在のカラムファミリーのエントリ数を返す
 *
 * 生走査で全件と非UTF-8件を数え、「全削除」確認で隠れた非UTF-8エントリも削除対象だと示すために使う (B7)
 *
 * @return {"total": n, "nonUtf8": m} を含むQJsonObject
 */
QJsonObject RocksDBBackend::getEntryCounts()
{
    QJsonObject result;
    result["total"] = 0;
    result["nonUtf8"] = 0;
    if (!d->db) {
        return result;
    }
    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return result;
    }
    rocksdb::ReadOptions ro;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    int total = 0;
    int nonUtf8 = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        total++;
        if (!utf8StringFromSlice(it->key()) || !utf8StringFromSlice(it->value())) {
            nonUtf8++;
        }
    }
    if (!it->status().ok()) {
        emit errorOccurred(statusToQString(it->status()));
        return result;
    }
    result["total"] = total;
    result["nonUtf8"] = nonUtf8;
    return result;
}

/**
 * @brief データベースの統計情報を取得する
 *
 * 全件走査は行わず、CF毎の推定キー数を使う (S5)
 * 既存のJSONキー (total_count等) は維持し、estimated=trueを付加する
 *
 * @return 統計情報を含むQJsonObject
 */
QJsonObject RocksDBBackend::getDatabaseStats()
{
    QJsonObject stats;
    if (!d->db) {
        return stats;
    }

    qint64 totalCount = 0;
    for (const auto &cfName : d->cfNames) {
        auto* cf = getColumnFamilyHandle(cfName);
        if (!cf) {
            continue;
        }
        uint64_t estimate = 0;
        if (d->db->GetIntProperty(cf, "rocksdb.estimate-num-keys", &estimate)) {
            stats[cfName + "_count"] = static_cast<qint64>(estimate);
            totalCount += static_cast<qint64>(estimate);
        } else {
            stats[cfName + "_count"] = 0;
        }
    }
    stats["total_count"] = totalCount;
    stats["column_family_count"] = d->cfNames.size();
    stats["estimated"] = true;

    const QFileInfo dbDir(d->dbPath);
    if (dbDir.isDir()) {
        qint64 totalSize = 0;
        QDirIterator it(d->dbPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            totalSize += it.fileInfo().size();
        }
        stats["disk_usage_bytes"] = totalSize;
    } else {
        stats["disk_usage_bytes"] = 0;
    }

    return stats;
}

/**
 * @brief クリップボードに文字列をコピーする
 *
 * @param text コピーする文字列
 */
void RocksDBBackend::copyToClipboard(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
    emit toastRequested(tr("Copied to clipboard"), "success");
}

/**
 * @brief 最近開いたデータベースリストにパスを追加する
 *
 * 既存の重複を削除し、先頭に追加して最大10件に制限する
 *
 * @param path 追加するデータベースパス
 */
void RocksDBBackend::addToRecent(const QString &path)
{
    auto settings = SettingsMigration::newSettings();
    QStringList recent = settings.value(SettingsKeys::recentDatabases).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10) {
        recent.removeLast();
    }
    settings.setValue(SettingsKeys::recentDatabases, recent);
    emit recentDatabasesChanged();
}

/**
 * @brief 最近開いたデータベースリストをクリアする
 */
void RocksDBBackend::clearRecent()
{
    auto settings = SettingsMigration::newSettings();
    settings.remove(SettingsKeys::recentDatabases);
    emit recentDatabasesChanged();
}

/**
 * @brief 最近開いたデータベースリストから指定パスを削除する
 *
 * @param path 削除するデータベースパス
 */
void RocksDBBackend::removeFromRecent(const QString &path)
{
    auto settings = SettingsMigration::newSettings();
    QStringList recent = settings.value(SettingsKeys::recentDatabases).toStringList();
    recent.removeAll(path);
    settings.setValue(SettingsKeys::recentDatabases, recent);
    emit recentDatabasesChanged();
}

/**
 * @brief QJsonObjectを指定ファイルにJSON形式で書き出す
 *
 * 同一ディレクトリの一時ファイル経由でアトミックに置換し、全工程の成功時のみ成功トーストを出す (S4)
 *
 * @param filePath 出力ファイルパス
 * @param data 書き出すデータ
 * @return 書き出しに成功した場合はtrue
 */
bool RocksDBBackend::writeJsonToFile(const QString &filePath,
                                      const QJsonObject &data,
                                      bool allowOverwrite)
{
    const QByteArray payload = QJsonDocument(data).toJson(QJsonDocument::Indented);
    QString error;
    const bool ok = writeFileAtomically(
        filePath,
        [&payload](QFile &tmp) {
            qint64 written = 0;
            while (written < payload.size()) {
                const qint64 n = tmp.write(payload.constData() + written,
                                           payload.size() - written);
                if (n < 0) {
                    return false;
                }
                written += n;
            }
            return true;
        },
        &error, allowOverwrite);
    if (!ok) {
        emit errorOccurred(tr("Failed to write file: %1").arg(error));
        return false;
    }
    emit toastRequested(tr("Data exported"), "success");
    return true;
}

/**
 * @brief 現在のCFをJSONファイルへストリーミング出力する
 *
 * メモリ上にQJsonObjectを構築せず、反復順の字下げJSONオブジェクトを一時ファイルへ直接書き出してアトミックに置換する (S4)
 * 値はcompact往復がバイト一致する場合のみネストJSONとし(B3)、非UTF-8エントリは数えて警告トーストで報告する
 * 反復子エラー時はファイルを作らずfalseを返す
 *
 * @param filePath 出力ファイルパス
 * @return 出力に成功した場合は true
 */
bool RocksDBBackend::exportToFile(const QString &filePath, bool allowOverwrite)
{
    if (!d->db) {
        return false;
    }
    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) {
        emit errorOccurred(tr("Column family not found: %1").arg(d->currentCf));
        return false;
    }

    int exported = 0;
    int skippedNonUtf8 = 0;
    QString error;
    const bool ok = writeFileAtomically(
        filePath,
        [this, cf, &exported, &skippedNonUtf8](QFile &tmp) {
            rocksdb::ReadOptions ro;
            std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
            auto writeAll = [&tmp](const QByteArray &chunk) {
                qint64 written = 0;
                while (written < chunk.size()) {
                    const qint64 n = tmp.write(chunk.constData() + written,
                                               chunk.size() - written);
                    if (n < 0) {
                        return false;
                    }
                    written += n;
                }
                return true;
            };
            if (!writeAll("{\n")) {
                return false;
            }
            bool first = true;
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                const auto key = utf8StringFromSlice(it->key());
                const auto raw = utf8StringFromSlice(it->value());
                if (!key || !raw) {
                    skippedNonUtf8++;
                    continue;
                }
                // B3完全往復:compact再出力がバイト一致の場合のみネストJSONにする
                QString valueOut;
                const QByteArray rawBytes = raw->toUtf8();
                const QJsonDocument parsed =
                    QJsonDocument::fromJson(rawBytes);
                if (!parsed.isNull()
                    && parsed.toJson(QJsonDocument::Compact) == rawBytes
                    && (parsed.isObject() || parsed.isArray())) {
                    valueOut = *raw;
                } else {
                    valueOut = jsonStringLiteral(*raw);
                }
                const QByteArray line =
                    QStringLiteral("%1%2: %3")
                        .arg(first ? QStringLiteral("  ")
                                   : QStringLiteral(",\n  "),
                             jsonStringLiteral(*key), valueOut)
                        .toUtf8();
                if (!writeAll(line)) {
                    return false;
                }
                first = false;
                exported++;
            }
            if (!it->status().ok()) {
                return false;
            }
            return writeAll(first ? QByteArrayLiteral("}\n")
                                 : QByteArrayLiteral("\n}\n"));
        },
        &error, allowOverwrite);

    // 空DBでは"{\n}\n"(空オブジェクト)を出力し、有効なJSONであり、importFromFileで正しく読める
    if (!ok) {
        // 反復子エラーか書き込み失敗かで文面を分ける
        if (error.isEmpty()) {
            emit errorOccurred(tr("Failed to export database"));
        }
        else {
            emit errorOccurred(tr("Failed to write file: %1").arg(error));
        }
        return false;
    }

    if (skippedNonUtf8 > 0) {
        emit toastRequested(tr("%1 entries with non-UTF-8 keys or values were not exported").arg(skippedNonUtf8), "warning");
    }

    emit toastRequested(tr("%1 records exported").arg(exported), "success");
    return true;
}

/**
 * @brief JSONファイルの内容を現在のCFへインポートする
 *
 * 256MiB上限のbounded readで検査し、構文エラー文面付きで報告する
 * 最上位はオブジェクトでなければならず、実投入はimportDataに委譲する (S4)
 * 読み取り専用モードでは拒否する
 *
 * @param filePath 入力ファイルパス
 * @return インポートに成功した場合はtrue
 */
bool RocksDBBackend::importFromFile(const QString &filePath)
{
    if (!d->db) {
        return false;
    }
    if (d->readOnly) {
        emit errorOccurred(
            tr("The database is open in read-only mode. Enable edit mode to modify data."));
        return false;
    }
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        emit errorOccurred(tr("Failed to open file for reading"));
        return false;
    }
    if (info.size() > kMaxImportFileSize) {
        emit errorOccurred(tr("Import file is too large (limit 256 MiB)"));
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Failed to open file for reading"));
        return false;
    }
    QByteArray raw;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1 << 20);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            emit errorOccurred(tr("Failed to open file for reading"));
            return false;
        }
        raw += chunk;
        if (raw.size() > kMaxImportFileSize) {
            emit errorOccurred(tr("Import file is too large (limit 256 MiB)"));
            return false;
        }
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit errorOccurred(
            tr("Invalid JSON file: %1").arg(parseError.errorString()));
        return false;
    }
    if (!doc.isObject()) {
        emit errorOccurred(tr("Invalid JSON file: top level must be an object"));
        return false;
    }
    return importData(doc.object());
}

/**
 * @brief 指定ファイルからJSONオブジェクトを読み込む
 *
 * 256MiB上限のbounded readで検査する (S4)
 *
 * @param filePath 入力ファイルパス
 * @return 読み込んだQJsonObject、失敗時は空
 */
QJsonObject RocksDBBackend::readJsonFromFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        emit errorOccurred(tr("Failed to open file for reading"));
        return QJsonObject();
    }
    if (info.size() > kMaxImportFileSize) {
        emit errorOccurred(tr("Import file is too large (limit 256 MiB)"));
        return QJsonObject();
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(tr("Failed to open file for reading"));
        return QJsonObject();
    }
    QByteArray raw;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1 << 20);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            emit errorOccurred(tr("Failed to open file for reading"));
            return QJsonObject();
        }
        raw += chunk;
        if (raw.size() > kMaxImportFileSize) {
            emit errorOccurred(tr("Import file is too large (limit 256 MiB)"));
            return QJsonObject();
        }
    }
    file.close();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit errorOccurred(tr("Invalid JSON file: %1").arg(parseError.errorString()));
        return QJsonObject();
    }
    return doc.object();
}

/**
 * @brief ダークモード設定を保存する
 *
 * @param isDark ダークモードを有効にする場合はtrue
 */
void RocksDBBackend::saveDarkMode(bool isDark)
{
    auto settings = SettingsMigration::newSettings();
    settings.setValue(SettingsKeys::darkMode, isDark);
}

/**
 * @brief ダークモード設定を読み込む
 *
 * @return ダークモードが有効な場合はtrue
 */
bool RocksDBBackend::loadDarkMode() const
{
    auto settings = SettingsMigration::newSettings();
    return settings.value(SettingsKeys::darkMode, false).toBool();
}
