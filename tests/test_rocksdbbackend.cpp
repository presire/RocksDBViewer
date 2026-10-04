#include <QtTest/QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMap>
#include <QRegularExpression>
#include <memory>
#include <rocksdb/db.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/utilities/db_ttl.h>
#include <rocksdb/utilities/options_util.h>
#include "backend/RocksDBBackend.h"

/**
 * @brief RocksDBBackendの内部テスト補助関数を配置する無名名前空間
 */
namespace {
/**
 * @brief 指定パスに特定のformat_versionでサンプルDBを作成する
 *
 * Issue#2の回帰テスト用に、古いformat_version (例:2) でDBを作成する
 * 編集後にformat_versionが昇格されていないことを検証する
 *
 * @param path DBパス
 * @param formatVersion BlockBasedTableOptions::format_versionの値
 * @return 作成に成功した場合はtrue
 */
bool createSampleDbWithFormatVersion(const QString &path, uint32_t formatVersion)
{
    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::BlockBasedTableOptions bbto;
    bbto.format_version = formatVersion;
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(bbto));

    std::unique_ptr<rocksdb::DB> db;
    rocksdb::Status s = rocksdb::DB::Open(options, path.toStdString(), &db);
    if (!s.ok() || !db) {
        return false;
    }

    db->Put(rocksdb::WriteOptions(), "sample:k1", "v1");
    db->Put(rocksdb::WriteOptions(), "sample:k2", "v2");

    // 指定したformat_versionを持つSSTファイルを確実に生成する
    rocksdb::FlushOptions fo;
    fo.wait = true;
    s = db->Flush(fo);
    s = db->Close();
    return s.ok();
}

/**
 * @brief 最新のOPTIONS-*ファイルの内容を返す
 *
 * @param dbPath DBディレクトリ
 * @return 最新のOPTIONS-*ファイルの内容
 *         存在しない場合は空文字列
 */
QString readLatestOptionsFile(const QString &dbPath)
{
    QDir dir(dbPath);
    const QStringList files = dir.entryList({"OPTIONS-*"}, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        return {};
    }
    QFile f(dir.filePath(files.last()));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QTextStream(&f).readAll();
}

/**
 * @brief いずれかのOPTIONS-*ファイルが指定部分文字列を含むかを返す
 *
 * @param dbPath DBディレクトリ
 * @param needle 探す文字列
 * @return いずれかのOPTIONS-*ファイルがneedleを含めばtrue
 */
bool anyOptionsFileContains(const QString &dbPath, const QString &needle)
{
    QDir dir(dbPath);
    const QStringList files = dir.entryList({"OPTIONS-*"}, QDir::Files, QDir::Name);
    for (const auto &name : files) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString content = QTextStream(&f).readAll();
        if (content.contains(needle)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief DBディレクトリからOPTIONS-*ファイルを全削除する
 *
 * LoadLatestOptionsのフォールバック分岐を強制的に起こすために使用
 *
 * @param dbPath DBディレクトリ
 * @return 削除したファイル数
 */
int removeAllOptionsFiles(const QString &dbPath)
{
    QDir dir(dbPath);
    int removed = 0;
    for (const auto &name : dir.entryList({"OPTIONS-*"}, QDir::Files)) {
        if (dir.remove(name)) {
            ++removed;
        }
    }
    return removed;
}

/**
 * @brief 素朴な単一CFのDBを作成する
 *
 * @param path DBパス
 * @return 作成に成功した場合はtrue
 */
bool createSimpleDb(const QString &path)
{
    rocksdb::Options options;
    options.create_if_missing = true;
    std::unique_ptr<rocksdb::DB> db;
    rocksdb::Status s = rocksdb::DB::Open(options, path.toStdString(), &db);
    if (!s.ok() || !db) {
        return false;
    }
    s = db->Put(rocksdb::WriteOptions(), "hello", "world");
    if (!s.ok()) {
        return false;
    }
    rocksdb::FlushOptions fo;
    fo.wait = true;
    s = db->Flush(fo);
    if (!s.ok()) {
        return false;
    }
    return db->Close().ok();
}

/**
 * @brief ディレクトリ配下の全ファイルの相対パス -> 更新時刻マップを返す
 *
 * 読取専用接続前後でのファイル不変検証に使う
 *
 * @param path ディレクトリパス
 * @return ファイル状態マップ
 */
QMap<QString, QDateTime> snapshotDirState(const QString &path)
{
    QMap<QString, QDateTime> state;
    QDir dir(path);
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        state.insert(dir.relativeFilePath(it.filePath()),
                     it.fileInfo().lastModified());
    }
    return state;
}
} // 無名名前空間

/**
 * @class TestRocksDBBackend
 * @brief RocksDBBackendの実DB単体テストを提供する
 *
 * DBの接続、編集、読取専用、OPTIONSファイルの安全性、入出力を検証する
 * 一時ディレクトリに実DBを作成するため、ROCKSDB_ROOT由来のrocksdbライブラリが必要
 */
class TestRocksDBBackend : public QObject
{
    Q_OBJECT
private slots:
    /**
     * @brief DBの接続と切断後の接続状態を検証する
     * @note 前提: 一時ディレクトリに[format_version]=2のサンプルDBを作成する
     *       操作: DBを開いて接続状態とDBパスを確認し、切断する
     *       期待: 接続中はconnectedがtrue、切断後はfalseになりDBパスも空になる
     */
    void testOpenAndClose();
    /**
     * @brief 編集した値が再接続後も保持されることを検証する
     * @note 前提: 書込み可能でサンプルDBを開く
     *       操作: 値を保存して切断し、再接続後にgetDataByKeyで取得する
     *       期待: 再接続後も保存した値を取得できる
     */
    void testSetDataPersistsAcrossReopen();
    /**
     * @brief Issue#2で修正した[format_version]保持を検証する
     * @note 前提: [format_version]=2のサンプルDBを作成する
     *       操作: 書込み可能で編集してから切断する
     *       期待: 最新のOPTIONS-*ファイルが[format_version]=2を維持する
     */
    void testFormatVersionPreservedIssue2Regression();
    /**
     * @brief 通常のOPTIONS読み込み時は既定値フラグが無効であることを検証する
     * @note 前提: 正常なサンプルDBを作成する
     *       操作: DBを開いてopenedWithDefaultsを確認し、切断後も再確認する
     *       期待: LoadLatestOptions成功時はopenedWithDefaultsがfalseを返す
     */
    void testOpenedWithDefaultsFalseOnNormalOpen();
    /**
     * @brief OPTIONS未検出時の既定値フラグと警告を検証する
     * @note 前提: 全OPTIONS-*ファイルを削除してLoadLatestOptionsを失敗させる
     *       操作: DBを開き、openedWithDefaultsChangedとtoastRequestedを監視する
     *       期待: フラグがtrueになり[warning]トーストが通知され、切断時にfalseへ戻る
     */
    void testOpenedWithDefaultsTrueOnMissingOptions();
    /**
     * @brief 既定の接続が読取専用でファイルを変更しないことを検証する
     * @note 前提: 接続前のファイル状態を記録する
     *       操作: 既定設定で開き、各書込み操作の拒否と読取専用の再オープンを確認する
     *       期待: 書込み操作が全て拒否され、LOCKを保持せずファイル状態も不変である
     */
    void testDefaultOpenIsReadOnly();
    /**
     * @brief 外部wal_dirを指定したDBの接続拒否を検証する
     * @note 前提: OPTIONSの[wal_dir]が外部ディレクトリを指すDBとダミーログファイルを作成する
     *       操作: 書込み可能で接続を試みる
     *       期待: 接続が拒否され、エラーが通知され、外部ログファイルが保護される
     */
    void testExternalWalDirRefused();
    /**
     * @brief BOM付きキーと不完全UTF-8キーの扱いを検証する
     * @note 前提: BOM付きキーと通常キーを保存し、直接RocksDB APIで不完全なUTF-8キーを追加する
     *       操作: DBを開いて一覧取得、削除、ページ取得を行う
     *       期待: BOM付きキーは通常キーと区別され、不完全なUTF-8キーは非表示として計上される
     */
    void testBomAndTruncatedKeys();
    /**
     * @brief エクスポートとインポートの値保持を検証する
     * @note 前提: 非正規JSON、プレーン文字列、制御文字含む値など多様な文字列を保存する
     *       操作: JSONへエクスポート後に全削除し、インポートで復元する
     *       期待: 全ての値がバイト単位で元の値と一致する
     */
    void testExportImportRoundTrip();
    /**
     * @brief シンボリックリンク先へのエクスポート安全性を検証する
     * @note 前提: 通常ファイルと、それを指すシンボリックリンクを作成する
     *       操作: 上書き有効でリンクパスへエクスポートする
     *       期待: リンク本体だけが通常ファイルへ置換され、リンク先の内容は変更されない
     */
    void testExportToSymlinkTarget();
    /**
     * @brief 10000件を超えるページ取得を検証する
     * @note 前提: 10050件のキーを持つDBを作成する
     *       操作: 上限10000件の設定で先頭ページと上限後のページを取得する
     *       期待: 先頭ページはcappedがtrue、残り50件のページは上限外の全件を返す
     */
    void testGetPageBeyond10000();
    /**
     * @brief 接続失敗後も現在のDBを維持することを検証する
     * @note 前提: 正常なDBを接続済みにする
     *       操作: 存在しないパスでの接続を試みる
     *       期待: 接続失敗後も前のDBが接続済みで読取り可能なまま維持される
     */
    void testFailedOpenKeepsPreviousDb();
    /**
     * @brief 読取専用モード切替による書込み制御を検証する
     * @note 前提: 書込み可能でDBを開く
     *       操作: 読取専用へ切替えて保存を試み、書込み可能へ復帰して再度保存する
     *       期待: 読取専用中は保存が拒否され、復帰後は保存に成功する
     */
    void testSetReadOnlyMode();
    /**
     * @brief TTL DBを強制読取専用にすることを検証する
     * @note 前提: DBWithTTLで作成したTTL DBを用意する
     *       操作: 書込み可能指定で接続し、読取専用解除を試みる
     *       期待: TTL DBは読取専用になり、ttlDetectedがtrueで編集モードへ戻せない
     */
    void testTtlDbForcedReadOnly();
    /**
     * @brief 改ざんOPTIONSファイルの拒否または無害化を検証する
     * @note 前提: 正常なOPTIONS-*ファイルを改ざんして異常な[num_levels]や[block_cache]を設定する
     *       操作: それぞれの改ざん状態で接続を試み、所要時間を計測する
     *       期待: クラッシュや長時間の停止を起こさず、拒否または無害化される
     */
    void testCraftedOptionsRefusedOrSanitized();
    /**
     * @brief TTL判定不能なOPTIONSファイルで編集をロックすることを検証する
     * @note 前提: TTLマーカーが走査上限の外へ追いやられたOPTIONS-*と、OPTIONS-*が欠落したDBを用意する
     *       操作: それぞれのDBを接続し、編集ロック状態と解除を確認する
     *       期待: TTL判定不能なDBは編集がロックされ、通常のDBはロックされない
     */
    void testTtlLockFailClosed();
    /**
     * @brief モード切替後も選択したカラムファミリーを維持することを検証する
     * @note 前提: 非既定のカラムファミリー[second]を持つDBを作成して選択する
     *       操作: 読取専用と書込み可能を交互に切替える
     *       期待: どちらの方向の切替後も選択したカラムファミリーが維持される
     */
    void testModeSwitchPreservesColumnFamily();
    /**
     * @brief 既存出力ファイルに対する上書き可否を検証する
     * @note 前提: センチネル内容の既存ファイルを用意する
     *       操作: 上書き無効と有効のそれぞれでエクスポートする
     *       期待: 無効時は失敗して内容を維持し、有効時はJSONへ置換される
     */
    void testExportNoReplaceSemantics();
    /**
     * @brief PlainTableカラムファミリーの全削除後の書込み可能性を検証する
     * @note 前提: PlainTableテーブルのカラムファミリー[plain]へ10件のキーを書き込む
     *       操作: 全キーを削除し、件数と新規保存を確認する
     *       期待: 全削除後に件数がゼロになり、カラムファミリーは書込み可能なまま維持される
     */
    void testClearDataPlainTable();
    /**
     * @brief 別の無効パスへの接続失敗後も現在のDBを維持することを検証する
     * @note 前提: 正常なDBと、接続に失敗する既存の空ディレクトリを用意する
     *       操作: 無効ディレクトリへの接続を試みた後、前のDBを編集する
     *       期待: 接続失敗後も前のDBが接続済みで編集可能なまま維持される
     */
    void testFailedOpenDifferentPathKeepsDb();
};

/**
 * @brief DBの接続と切断後の接続状態を検証する
 */
void TestRocksDBBackend::testOpenAndClose()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    RocksDBBackend backend;
    QVERIFY(!backend.connected());
    QVERIFY(backend.openDatabase(dir.path()));
    QVERIFY(backend.connected());
    QCOMPARE(backend.databasePath(), dir.path());

    backend.closeDatabase();
    QVERIFY(!backend.connected());
    QVERIFY(backend.databasePath().isEmpty());
}

/**
 * @brief 編集した値が再接続後も保持されることを検証する
 */
void TestRocksDBBackend::testSetDataPersistsAcrossReopen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QVERIFY(backend.setData("regression:persist", "persisted-value"));
    backend.closeDatabase();

    // 再接続後も保存した値が保持されることを確認する
    QVERIFY(backend.openDatabase(dir.path(), false));
    const QJsonObject result = backend.getDataByKey("regression:persist");
    QVERIFY2(!result.isEmpty(), "Saved key missing after reopen");
    QCOMPARE(result.value("value").toString(), QStringLiteral("persisted-value"));
}

/**
 * @brief Issue#2で修正した[format_version]保持を検証する
 */
void TestRocksDBBackend::testFormatVersionPreservedIssue2Regression()
{
    // 幅広いRocksDB版で利用できるformat_version=2を明示したDBを作成する
    // RocksDBBackendで編集後も保存済みOPTIONS-*ファイルはformat_version=2を保持する必要がある
    // LoadLatestOptionsが失敗してRocksDB 11.xの既定値で開くと、新しいSSTとOPTIONS-*が既定値になりIssue#2が再発する
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    // 作成直後のOPTIONS-*ファイルがformat_version=2を記録していることを確認する
    QVERIFY2(anyOptionsFileContains(dir.path(), "format_version=2"),
             "Pre-condition: created DB should record format_version=2");

    {
        RocksDBBackend backend;
        QVERIFY(backend.openDatabase(dir.path(), false));
        QVERIFY(backend.setData("issue2:regression", "edited"));
        backend.closeDatabase();
    }

    // 最新OPTIONS-*ファイルはformat_version=2を維持する必要がある
    // 修正が後退した場合はRocksDB 11.xの既定値であるformat_version=6または7が記録される
    const QString latest = readLatestOptionsFile(dir.path());
    QVERIFY2(!latest.isEmpty(), "No OPTIONS file found after edit+close");
    QVERIFY2(latest.contains("format_version=2"),
             "Issue #2 REGRESSION: format_version was upgraded after edit");
}

/**
 * @brief 通常のOPTIONS読み込み時は既定値フラグが無効であることを検証する
 */
void TestRocksDBBackend::testOpenedWithDefaultsFalseOnNormalOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path()));
    QVERIFY2(!backend.openedWithDefaults(),
             "openedWithDefaults must be false when LoadLatestOptions succeeds");
    backend.closeDatabase();
    QVERIFY(!backend.openedWithDefaults());
}

/**
 * @brief OPTIONS未検出時の既定値フラグと警告を検証する
 */
void TestRocksDBBackend::testOpenedWithDefaultsTrueOnMissingOptions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    // 全OPTIONS-*ファイルを削除してLoadLatestOptionsを失敗させる
    QVERIFY2(removeAllOptionsFiles(dir.path()) > 0,
             "Pre-condition: expected at least one OPTIONS file to remove");

    RocksDBBackend backend;

    // 警告がUIへ通知されることを確認するためトーストシグナルを監視する
    QSignalSpy toastSpy(&backend, &RocksDBBackend::toastRequested);
    QSignalSpy flagSpy(&backend, &RocksDBBackend::openedWithDefaultsChanged);

    QVERIFY(backend.openDatabase(dir.path()));

    QVERIFY2(backend.openedWithDefaults(),
             "openedWithDefaults must be true when LoadLatestOptions fails");
    QCOMPARE(flagSpy.count(), 1);

    // フォールバックのリスクを示す[warning]トーストが少なくとも1件通知されることを確認する
    bool sawWarning = false;
    for (const auto &args : toastSpy) {
        if (args.size() >= 2 && args.at(1).toString() == "warning") {
            sawWarning = true;
            break;
        }
    }
    QVERIFY2(sawWarning,
             "Expected a 'warning' toast when opening with default options");

    backend.closeDatabase();
    QVERIFY(!backend.openedWithDefaults());
    QCOMPARE(flagSpy.count(), 2); // 切断時のtrueからfalseへの遷移
}

/**
 * @brief 既定の接続が読取専用でファイルを変更しないことを検証する
 */
void TestRocksDBBackend::testDefaultOpenIsReadOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    // 既定の接続は読取専用で、書込みを拒否しLOCKを保持せず、接続前後でDBファイルを変更しない
    const QMap<QString, QDateTime> before = snapshotDirState(dir.path());

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path()));
    QVERIFY(backend.connected());
    QVERIFY2(backend.readOnly(), "Default open must be read-only");
    QVERIFY2(!backend.setData("a:readonly", "x"), "setData must fail read-only");
    QVERIFY2(!backend.deleteData("hello"), "deleteData must fail read-only");
    QVERIFY2(!backend.clearData(), "clearData must fail read-only");
    QVERIFY2(!backend.importData(QJsonObject{{"k", "v"}}),
             "importData must fail read-only");

    // ビューアが読取専用でDBを保持する間も、2件目の読取専用接続は成功する必要がある
    // db.hでは複数のOpenForReadOnlyを定義済みの動作としている
    // 読取専用接続中に同じDBを読書きで開く動作は未定義のため、このテストはLOCK未取得とファイル不変性だけを確認する
    {
        std::unique_ptr<rocksdb::DB> second;
        const rocksdb::Status s = rocksdb::DB::OpenForReadOnly(
            rocksdb::Options(), dir.path().toStdString(), &second);
        QVERIFY2(s.ok(), qPrintable(QString::fromStdString(s.ToString())));
        QVERIFY(second->Close().ok());
    }
    QVERIFY(backend.connected());
    backend.closeDatabase();

    QCOMPARE(snapshotDirState(dir.path()), before);
}

/**
 * @brief 外部wal_dirを指定したDBの接続拒否を検証する
 */
void TestRocksDBBackend::testExternalWalDirRefused()
{
    QTemporaryDir dbDir;
    QTemporaryDir walDir;
    QVERIFY(dbDir.isValid());
    QVERIFY(walDir.isValid());

    // OPTIONS-*のwal_dirが別ディレクトリを指すDBを作成する
    {
        rocksdb::Options options;
        options.create_if_missing = true;
        options.wal_dir = walDir.path().toStdString();
        std::unique_ptr<rocksdb::DB> db;
        QVERIFY(rocksdb::DB::Open(options, dbDir.path().toStdString(), &db).ok());
        QVERIFY(db->Put(rocksdb::WriteOptions(), "k", "v").ok());
        QVERIFY(db->Close().ok());
    }
    QVERIFY2(anyOptionsFileContains(dbDir.path(), "wal_dir="),
             "Pre-condition: OPTIONS should record wal_dir");

    // 改ざんOPTIONS-*により削除され得る外部ログファイルを作成する
    const QString dummyPath = walDir.filePath("000001.log");
    {
        QFile dummy(dummyPath);
        QVERIFY(dummy.open(QIODevice::WriteOnly | QIODevice::Text));
        dummy.write("do-not-delete");
        dummy.close();
    }

    RocksDBBackend backend;
    QSignalSpy errorSpy(&backend, &RocksDBBackend::errorOccurred);
    QVERIFY2(!backend.openDatabase(dbDir.path(), false),
             "DB with external wal_dir must be refused");
    QVERIFY(!backend.connected());
    QVERIFY(!errorSpy.isEmpty());

    QFile dummy(dummyPath);
    QVERIFY(dummy.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(dummy.readAll()), QStringLiteral("do-not-delete"));
}

/**
 * @brief BOM付きキーと不完全UTF-8キーの扱いを検証する
 */
void TestRocksDBBackend::testBomAndTruncatedKeys()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    // BOM付きキーと通常キーは区別され、不完全な末尾マルチバイトキーは非表示として計上される必要がある
    const QString bomKey = QStringLiteral("\uFEFFk");
    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QVERIFY(backend.setData(bomKey, "bom-value"));
    QVERIFY(backend.setData("k", "plain-value"));
    backend.closeDatabase();

    {
        rocksdb::Options options;
        std::unique_ptr<rocksdb::DB> direct;
        QVERIFY(rocksdb::DB::Open(options, dir.path().toStdString(), &direct).ok());
        const std::string truncated("a\xe3\x81", 3);
        QVERIFY(direct->Put(rocksdb::WriteOptions(), truncated, "raw").ok());
        QVERIFY(direct->Close().ok());
    }

    QVERIFY(backend.openDatabase(dir.path(), false));
    const QJsonObject all = backend.getData();
    QVERIFY2(all.contains(bomKey), "BOM key must be listed");
    QVERIFY2(all.contains("k"), "Plain key must be listed");
    QVERIFY2(all.value(bomKey).toString() == "bom-value", "BOM value intact");
    QVERIFY(backend.deleteData("k"));
    QVERIFY(backend.getDataByKey("k").isEmpty());
    QVERIFY2(!backend.getDataByKey(bomKey).isEmpty(),
             "Deleting 'k' must not delete the BOM key");

    const QJsonObject page = backend.getPage(QString(), 0, 1000, 10000);
    QVERIFY2(page.value("hiddenNonUtf8").toInt() >= 1,
             "Truncated key must be counted as hiddenNonUtf8");
    QVERIFY(!page.value("entries").toObject().contains("a"));
}

/**
 * @brief エクスポートとインポートの値保持を検証する
 */
void TestRocksDBBackend::testExportImportRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    // exportToFileとimportFromFileは値をバイト単位で再現する必要がある
    const QString nonCanon = QStringLiteral("{\"b\": 1,  \"a\":2}");
    const QString canon = QStringLiteral("{\"a\":1}");
    const QString plain = QStringLiteral("hello world");
    const QString tricky = QString::fromUtf8("quote \" backslash \\ newline \n tab \t \x01 end");

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QVERIFY(backend.setData("v_noncanon", nonCanon));
    QVERIFY(backend.setData("v_canon", canon));
    QVERIFY(backend.setData("v_plain", plain));
    QVERIFY(backend.setData("v_tricky", tricky));

    const QString exportPath = dir.filePath("roundtrip.json");
    QVERIFY(backend.exportToFile(exportPath));

    // 出力ファイル自体が有効なJSONオブジェクトであることを確認する
    {
        QFile f(exportPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        QVERIFY(doc.isObject());
    }

    QVERIFY(backend.clearData());
    QCOMPARE(backend.getEntryCounts().value("total").toInt(), 0);

    const QString emptyExportPath = dir.filePath("empty.json");
    QVERIFY(backend.exportToFile(emptyExportPath));
    {
        QFile f(emptyExportPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        QVERIFY(doc.isObject());
        QVERIFY(doc.object().isEmpty());
    }

    QVERIFY(backend.importFromFile(exportPath));

    QCOMPARE(backend.getDataByKey("v_noncanon").value("value").toString(), nonCanon);
    QCOMPARE(backend.getDataByKey("v_canon").value("value").toString(), canon);
    QCOMPARE(backend.getDataByKey("v_plain").value("value").toString(), plain);
    QCOMPARE(backend.getDataByKey("v_tricky").value("value").toString(), tricky);
}

/**
 * @brief シンボリックリンク先へのエクスポート安全性を検証する
 */
void TestRocksDBBackend::testExportToSymlinkTarget()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    // allowOverwriteを有効にしてシンボリックリンク経由で出力すると、リンク本体だけを置換して参照先を変更しない必要がある
    const QString realPath = dir.filePath("real.json");
    {
        QFile real(realPath);
        QVERIFY(real.open(QIODevice::WriteOnly | QIODevice::Text));
        real.write("original-content");
        real.close();
    }
    const QString linkPath = dir.filePath("link.json");
    QFile::remove(linkPath);
    QVERIFY(QFile::link(realPath, linkPath));
    QVERIFY(QFileInfo(linkPath).isSymLink());

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QVERIFY(backend.exportToFile(linkPath, true));

    QVERIFY2(!QFileInfo(linkPath).isSymLink(),
             "Symlink must be replaced by a regular file");
    {
        QFile real(realPath);
        QVERIFY(real.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(real.readAll()),
                 QStringLiteral("original-content"));
    }
    {
        QFile exported(linkPath);
        QVERIFY(exported.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(exported.readAll());
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object().value("hello").toString(), QStringLiteral("world"));
    }
}

/**
 * @brief 10000件を超えるページ取得を検証する
 */
void TestRocksDBBackend::testGetPageBeyond10000()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // 10050件のDBでは10000件の上限を超えたページにも到達できる必要がある
    {
        rocksdb::Options options;
        options.create_if_missing = true;
        std::unique_ptr<rocksdb::DB> db;
        QVERIFY(rocksdb::DB::Open(options, dir.path().toStdString(), &db).ok());
        rocksdb::WriteBatch batch;
        for (int i = 0; i < 10050; ++i) {
            const QString key = QStringLiteral("k%1").arg(i, 5, 10, QChar('0'));
            const QByteArray keyBytes = key.toUtf8();
            const QByteArray valueBytes =
                QStringLiteral("v%1").arg(i).toUtf8();
            QVERIFY(batch
                        .Put(std::string(keyBytes.constData(),
                                         static_cast<size_t>(keyBytes.size())),
                             std::string(valueBytes.constData(),
                                         static_cast<size_t>(valueBytes.size())))
                        .ok());
        }
        QVERIFY(db->Write(rocksdb::WriteOptions(), &batch).ok());
        QVERIFY(db->Close().ok());
    }

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path()));
    const QJsonObject first = backend.getPage(QString(), 0, 100, 10000);
    QCOMPARE(first.value("entries").toObject().size(), 100);
    QCOMPARE(first.value("total").toInt(), 10000);
    QCOMPARE(first.value("capped").toBool(), true);

    const QJsonObject last = backend.getPage(QString(), 10000, 100, 10000);
    QCOMPARE(last.value("entries").toObject().size(), 50);
    QCOMPARE(last.value("total").toInt(), 10050);
    QCOMPARE(last.value("capped").toBool(), false);
    QVERIFY(last.value("entries").toObject().contains("k10049"));
}

/**
 * @brief 接続失敗後も現在のDBを維持することを検証する
 */
void TestRocksDBBackend::testFailedOpenKeepsPreviousDb()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    // 接続失敗後も先に開いたDBの接続状態を維持する必要がある
    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path()));
    const QString prevPath = backend.databasePath();
    QVERIFY(backend.connected());

    QVERIFY2(!backend.openDatabase(dir.filePath("does-not-exist")),
             "Opening a nonexistent path must fail");
    QVERIFY2(backend.connected(), "Previous DB must stay connected");
    QCOMPARE(backend.databasePath(), prevPath);
    QVERIFY(!backend.getDataByKey("hello").isEmpty());
}

/**
 * @brief 読取専用モード切替による書込み制御を検証する
 */
void TestRocksDBBackend::testSetReadOnlyMode()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    // モード切替は書込み可否を適切に制御する必要がある
    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QVERIFY(!backend.readOnly());
    QVERIFY(backend.setData("h:rw", "1"));

    QVERIFY(backend.setReadOnlyMode(true));
    QVERIFY(backend.readOnly());
    QVERIFY(!backend.setData("h:ro", "x"));
    QVERIFY(backend.getDataByKey("h:ro").isEmpty());

    QVERIFY(backend.setReadOnlyMode(true)); // 何も変更せず読取専用を維持する
    QVERIFY(backend.readOnly());

    QVERIFY(backend.setReadOnlyMode(false));
    QVERIFY(!backend.readOnly());
    QVERIFY(backend.setData("h:rw2", "2"));
    QCOMPARE(backend.getDataByKey("h:rw2").value("value").toString(),
             QStringLiteral("2"));
}

/**
 * @brief TTL DBを強制読取専用にすることを検証する
 */
void TestRocksDBBackend::testTtlDbForcedReadOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // readOnly=falseで開いたTTL DBは最終的に読取専用になる必要がある
    const QString ttlPath = dir.filePath("ttldb");
    {
        rocksdb::Options options;
        options.create_if_missing = true;
        rocksdb::DBWithTTL *ttlDb = nullptr;
        QVERIFY2(rocksdb::DBWithTTL::Open(options, ttlPath.toStdString(),
                                         &ttlDb, 3600)
                     .ok(),
                 "DBWithTTL::Open failed");
        QVERIFY(ttlDb->Put(rocksdb::WriteOptions(), "t:k", "t:v").ok());
        QVERIFY(ttlDb->Close().ok());
        delete ttlDb;
    }
    QVERIFY2(anyOptionsFileContains(ttlPath, "TtlCompactionFilterFactory")
                 || anyOptionsFileContains(ttlPath, "TtlMergeOperator"),
             "Pre-condition: OPTIONS should record TTL markers");

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(ttlPath, false));
    QVERIFY2(backend.readOnly(), "TTL DB must be forced read-only");
    QVERIFY2(backend.ttlDetected(), "ttlDetected must be true");
    QVERIFY2(!backend.setReadOnlyMode(false),
             "setReadOnlyMode(false) must fail on a TTL DB");
    QVERIFY(backend.readOnly());
    // TTL値にはタイムスタンプ接尾辞があるためUTF-8復号で非表示になる場合がある
    // keyExistsで読取専用のままデータへ到達できることを確認する
    QVERIFY(backend.keyExists("t:k"));
}

/**
 * @brief 改ざんOPTIONSファイルの拒否または無害化を検証する
 */
void TestRocksDBBackend::testCraftedOptionsRefusedOrSanitized()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    QDir dbDir(dir.path());
    const QStringList optionsFiles =
        dbDir.entryList({"OPTIONS-*"}, QDir::Files, QDir::Name);
    QVERIFY(!optionsFiles.isEmpty());
    const QString optionsPath = dbDir.filePath(optionsFiles.last());
    QFile optionsFile(optionsPath);
    QVERIFY(optionsFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString original = QTextStream(&optionsFile).readAll();
    optionsFile.close();
    QVERIFY(original.contains("num_levels="));

    // 異常なnum_levelsは高速に接続を拒否し、クラッシュしない必要がある
    {
        QString crafted = original;
        crafted.replace(QRegularExpression("\\bnum_levels=\\d+"),
                        "num_levels=1000000000");
        QVERIFY(optionsFile.open(QIODevice::WriteOnly | QIODevice::Text
                                 | QIODevice::Truncate));
        QTextStream(&optionsFile) << crafted;
        optionsFile.close();

        RocksDBBackend backend;
        QSignalSpy errorSpy(&backend, &RocksDBBackend::errorOccurred);
        QElapsedTimer timer;
        timer.start();
        QVERIFY2(!backend.openDatabase(dir.path()),
                 "Open with num_levels=1000000000 must be refused");
        QVERIFY2(timer.elapsed() < 15000,
                 "Refusal must be fast (no huge allocation)");
        QVERIFY(!backend.connected());
        QVERIFY2(!errorSpy.isEmpty(), "Refusal must report an error");
        bool mentionsLevels = false;
        for (const auto &args : errorSpy) {
            if (args.at(0).toString().contains("num_levels")) {
                mentionsLevels = true;
                break;
            }
        }
        QVERIFY2(mentionsLevels, "Error must mention num_levels");
    }

    // num_shard_bits=19を持つ改ざんblock_cacheは、拒否または無害化のどちらでも接続時にクラッシュや停止を起こさない必要がある
    {
        QString crafted = original;
        crafted.replace("[TableOptions/BlockBasedTable \"default\"]",
                        "[TableOptions/BlockBasedTable \"default\"]\n"
                        "  block_cache={capacity=8388608; num_shard_bits=19}");
        QVERIFY(optionsFile.open(QIODevice::WriteOnly | QIODevice::Text
                                 | QIODevice::Truncate));
        QTextStream(&optionsFile) << crafted;
        optionsFile.close();

        RocksDBBackend backend;
        QElapsedTimer timer;
        timer.start();
        const bool opened = backend.openDatabase(dir.path());
        QVERIFY2(timer.elapsed() < 15000,
                 "Crafted block_cache must not stall the open");
        if (opened) {
            QVERIFY(!backend.getDataByKey("hello").isEmpty());
        } else {
            QVERIFY(!backend.connected());
        }
    }
}

/**
 * @brief TTL判定不能なOPTIONSファイルで編集をロックすることを検証する
 */
void TestRocksDBBackend::testTtlLockFailClosed()
{
    // 先頭コメントによりTTLマーカーが4MiBの走査上限を超えても、見落とさず編集をロックする必要がある
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ttlPath = dir.filePath("ttldb");
    {
        rocksdb::Options options;
        options.create_if_missing = true;
        rocksdb::DBWithTTL *ttlDb = nullptr;
        QVERIFY2(rocksdb::DBWithTTL::Open(options, ttlPath.toStdString(),
                                         &ttlDb, 3600)
                     .ok(),
                 "DBWithTTL::Open failed");
        QVERIFY(ttlDb->Put(rocksdb::WriteOptions(), "t:k", "t:v").ok());
        QVERIFY(ttlDb->Close().ok());
        delete ttlDb;
    }
    QDir ttlDir(ttlPath);
    const QStringList optionsFiles =
        ttlDir.entryList({"OPTIONS-*"}, QDir::Files, QDir::Name);
    QVERIFY(!optionsFiles.isEmpty());
    {
        QFile optionsFile(ttlDir.filePath(optionsFiles.last()));
        QVERIFY(optionsFile.open(QIODevice::ReadOnly));
        const QByteArray original = optionsFile.readAll();
        optionsFile.close();
        QVERIFY(optionsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        optionsFile.write("#");
        optionsFile.write(QByteArray(4 * 1024 * 1024 + 64, 'x'));
        optionsFile.write("\n");
        optionsFile.write(original);
        optionsFile.close();
    }

    {
        RocksDBBackend backend;
        QVERIFY(backend.openDatabase(ttlPath));
        QVERIFY2(backend.editLocked(),
                 "Oversized OPTIONS must lock editing (TTL undeterminable)");
        QVERIFY2(!backend.editLockReason().isEmpty(),
                 "editLockReason must explain the lock");
        QVERIFY(backend.readOnly());
        QSignalSpy errorSpy(&backend, &RocksDBBackend::errorOccurred);
        QVERIFY2(!backend.setReadOnlyMode(false),
                 "setReadOnlyMode(false) must be refused while locked");
        QVERIFY(backend.readOnly());
        QVERIFY(!errorSpy.isEmpty());
    }

    // 削除されたOPTIONS-*ファイルでも編集をロックする必要がある
    QTemporaryDir dir2;
    QVERIFY(dir2.isValid());
    QVERIFY(createSimpleDb(dir2.path()));
    QVERIFY(removeAllOptionsFiles(dir2.path()) > 0);
    {
        RocksDBBackend backend;
        QVERIFY(backend.openDatabase(dir2.path()));
        QVERIFY2(backend.editLocked(),
                 "Missing OPTIONS must lock editing (TTL undeterminable)");
        QVERIFY2(!backend.editLockReason().isEmpty(),
                 "editLockReason must explain the lock");
        QVERIFY2(!backend.setReadOnlyMode(false),
                 "setReadOnlyMode(false) must be refused while locked");
        QVERIFY(backend.readOnly());
    }

    // 通常のDBはロックされず、理由も空である必要がある
    QTemporaryDir dir3;
    QVERIFY(dir3.isValid());
    QVERIFY(createSimpleDb(dir3.path()));
    {
        RocksDBBackend backend;
        QVERIFY(backend.openDatabase(dir3.path()));
        QVERIFY(!backend.editLocked());
        QVERIFY(backend.editLockReason().isEmpty());
        QVERIFY(backend.setReadOnlyMode(false));
        QVERIFY(!backend.editLocked());
    }
}

/**
 * @brief モード切替後も選択したカラムファミリーを維持することを検証する
 */
void TestRocksDBBackend::testModeSwitchPreservesColumnFamily()
{
    // 非既定のカラムファミリーを選択し、読取専用と書込み可能を両方向へ切替えても選択を維持する必要がある
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        rocksdb::Options options;
        options.create_if_missing = true;
        options.create_missing_column_families = true;
        std::unique_ptr<rocksdb::DB> db;
        std::vector<rocksdb::ColumnFamilyDescriptor> descs;
        descs.emplace_back(rocksdb::kDefaultColumnFamilyName,
                            rocksdb::ColumnFamilyOptions());
        descs.emplace_back("second", rocksdb::ColumnFamilyOptions());
        std::vector<rocksdb::ColumnFamilyHandle *> handles;
        QVERIFY2(rocksdb::DB::Open(rocksdb::DBOptions(options),
                                   dir.path().toStdString(), descs, &handles,
                                   &db)
                     .ok(),
                 "Multi-CF setup failed");
        QVERIFY(db->Put(rocksdb::WriteOptions(), handles[1], "s:k", "s:v").ok());
        for (auto *h : handles) {
            db->DestroyColumnFamilyHandle(h);
        }
        QVERIFY(db->Close().ok());
    }

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QVERIFY(backend.columnFamilies().contains("second"));
    backend.setCurrentColumnFamily("second");
    QCOMPARE(backend.currentColumnFamily(), QStringLiteral("second"));

    QVERIFY(backend.setReadOnlyMode(true));
    QCOMPARE(backend.currentColumnFamily(), QStringLiteral("second"));

    QVERIFY(backend.setReadOnlyMode(false));
    QCOMPARE(backend.currentColumnFamily(), QStringLiteral("second"));
    QCOMPARE(backend.getDataByKey("s:k").value("value").toString(),
             QStringLiteral("s:v"));
}

/**
 * @brief 既存出力ファイルに対する上書き可否を検証する
 */
void TestRocksDBBackend::testExportNoReplaceSemantics()
{
    // 既存ファイルへのexportToFile(path,false)は失敗して内容をバイト単位で維持し、trueでは置換する必要がある
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSimpleDb(dir.path()));

    const QString exportPath = dir.filePath("out.json");
    {
        QFile existing(exportPath);
        QVERIFY(existing.open(QIODevice::WriteOnly | QIODevice::Text));
        existing.write("sentinel-content");
        existing.close();
    }
    QFile probe(exportPath);
    QVERIFY(probe.open(QIODevice::ReadOnly));
    const QByteArray before = probe.readAll();
    probe.close();

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QSignalSpy errorSpy(&backend, &RocksDBBackend::errorOccurred);
    QVERIFY2(!backend.exportToFile(exportPath),
             "exportToFile without overwrite must fail on existing file");
    QVERIFY(!errorSpy.isEmpty());

    QVERIFY(probe.open(QIODevice::ReadOnly));
    QCOMPARE(probe.readAll(), before);
    probe.close();

    QVERIFY(backend.exportToFile(exportPath, true));
    QVERIFY(probe.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(probe.readAll());
    probe.close();
    QVERIFY(doc.isObject());
    QCOMPARE(doc.object().value("hello").toString(), QStringLiteral("world"));

    QVERIFY(backend.exportToFile(dir.filePath("fresh.json")));
}

/**
 * @brief PlainTableカラムファミリーの全削除後の書込み可能性を検証する
 */
void TestRocksDBBackend::testClearDataPlainTable()
{
    // PlainTable CFのclearDataは全キーを削除し、その後もCFを編集可能に維持する必要がある
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        rocksdb::Options options;
        options.create_if_missing = true;
        options.create_missing_column_families = true;
        rocksdb::ColumnFamilyOptions plainCf;
        // PlainTableハッシュモードでは作成時と再接続時の両方でプレフィックス抽出器が必要になる
        // テスト用キーはすべて3バイトの[p:k]プレフィックスを共有する
        plainCf.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(3));
        plainCf.table_factory.reset(
            rocksdb::NewPlainTableFactory(rocksdb::PlainTableOptions()));
        std::unique_ptr<rocksdb::DB> db;
        std::vector<rocksdb::ColumnFamilyDescriptor> descs;
        descs.emplace_back(rocksdb::kDefaultColumnFamilyName,
                            rocksdb::ColumnFamilyOptions());
        descs.emplace_back("plain", plainCf);
        std::vector<rocksdb::ColumnFamilyHandle *> handles;
        QVERIFY2(rocksdb::DB::Open(rocksdb::DBOptions(options),
                                   dir.path().toStdString(), descs, &handles,
                                   &db)
                     .ok(),
                 "PlainTable setup failed");
        for (int i = 0; i < 10; ++i) {
            const std::string key = "p:k" + std::to_string(i);
            QVERIFY(db->Put(rocksdb::WriteOptions(), handles[1], key, "v").ok());
        }
        for (auto *h : handles) {
            db->DestroyColumnFamilyHandle(h);
        }
        QVERIFY(db->Close().ok());
    }

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path(), false));
    QVERIFY(backend.columnFamilies().contains("plain"));
    backend.setCurrentColumnFamily("plain");
    QVERIFY(backend.getTotalEntryCount() > 0);

    QVERIFY2(backend.clearData(), "clearData on PlainTable CF must succeed");
    QCOMPARE(backend.getEntryCounts().value("total").toInt(), 0);
    QVERIFY2(backend.setData("p:after", "ok"),
             "CF must stay writable after clearData");
    QCOMPARE(backend.getDataByKey("p:after").value("value").toString(),
             QStringLiteral("ok"));
}

/**
 * @brief 別の無効パスへの接続失敗後も現在のDBを維持することを検証する
 */
void TestRocksDBBackend::testFailedOpenDifferentPathKeepsDb()
{
    // 別の既存だが無効なディレクトリへの接続失敗後も、前のDBは変更せず利用可能に維持する必要がある
    QTemporaryDir dirA;
    QVERIFY(dirA.isValid());
    QVERIFY(createSimpleDb(dirA.path()));
    QTemporaryDir invalidDir;
    QVERIFY(invalidDir.isValid());

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dirA.path()));
    const QString prevPath = backend.databasePath();

    QSignalSpy errorSpy(&backend, &RocksDBBackend::errorOccurred);
    QVERIFY2(!backend.openDatabase(invalidDir.path()),
             "Opening an invalid directory must fail");
    QVERIFY2(backend.connected(), "Previous DB must stay connected");
    QCOMPARE(backend.databasePath(), prevPath);
    QVERIFY(!errorSpy.isEmpty());
    QVERIFY(!backend.getDataByKey("hello").isEmpty());
    QVERIFY(backend.setReadOnlyMode(false));
    QVERIFY(backend.setData("f:still", "writable"));
    QCOMPARE(backend.getDataByKey("f:still").value("value").toString(),
             QStringLiteral("writable"));
}

QTEST_MAIN(TestRocksDBBackend)

#include "test_rocksdbbackend.moc"
