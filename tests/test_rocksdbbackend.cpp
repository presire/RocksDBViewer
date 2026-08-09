#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <memory>
#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/options_util.h>

#include "backend/RocksDBBackend.h"

// QTest silences qDebug/qWarning by default; surface them for debugging.
namespace {
/**
 * @brief 指定パスに特定のformat_versionでサンプルDBを作成する
 *
 * Issue #2 回帰テスト用: 古い format_version (例:2) を使ってDBを作成し、
 * 編集後に format_version が昇格されていないことを検証する。
 *
 * @param path DBパス
 * @param formatVersion BlockBasedTableOptions::format_version 値
 * @return 作成に成功した場合 true
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

    // Flush so an SST file with the requested format_version is materialised.
    rocksdb::FlushOptions fo;
    fo.wait = true;
    s = db->Flush(fo);
    s = db->Close();
    return s.ok();
}

/**
 * @brief 最新のOPTIONSファイルの内容を返す
 *
 * @param dbPath DBディレクトリ
 * @return 最新のOPTIONS-* ファイルの内容。存在しない場合は空文字列。
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
 * @brief いずれかのOPTIONSファイルが指定部分文字列を含むかを返す
 *
 * @param dbPath DBディレクトリ
 * @param needle 探す文字列
 * @return いずれかのOPTIONS-* ファイルが needle を含めば true
 */
bool anyOptionsFileContains(const QString &dbPath, const QString &needle)
{
    QDir dir(dbPath);
    const QStringList files = dir.entryList({"OPTIONS-*"}, QDir::Files, QDir::Name);
    for (const auto &name : files) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QString content = QTextStream(&f).readAll();
        if (content.contains(needle)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief DBディレクトリからOPTIONSファイルを全削除する
 *
 * LoadLatestOptions のフォールバック分岐を強制的に起こすために使用。
 *
 * @param dbPath DBディレクトリ
 * @return 削除したファイル数
 */
int removeAllOptionsFiles(const QString &dbPath)
{
    QDir dir(dbPath);
    int removed = 0;
    for (const auto &name : dir.entryList({"OPTIONS-*"}, QDir::Files)) {
        if (dir.remove(name)) ++removed;
    }
    return removed;
}
} // namespace

/**
 * @brief RocksDBBackend の単体テスト
 *
 * Issue #2 修正(format_version 保持)と、フォールバック警告(openedWithDefaults)
 * を検証する。実DBを使用するため ROCKER_ROOT 由来の rocksdb ライブラリが必要。
 */
class TestRocksDBBackend : public QObject
{
    Q_OBJECT
private slots:
    void testOpenAndClose();
    void testSetDataPersistsAcrossReopen();
    void testFormatVersionPreservedIssue2Regression();
    void testOpenedWithDefaultsFalseOnNormalOpen();
    void testOpenedWithDefaultsTrueOnMissingOptions();
};

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

void TestRocksDBBackend::testSetDataPersistsAcrossReopen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    RocksDBBackend backend;
    QVERIFY(backend.openDatabase(dir.path()));
    QVERIFY(backend.setData("regression:persist", "persisted-value"));
    backend.closeDatabase();

    // Reopen and confirm the value survived.
    QVERIFY(backend.openDatabase(dir.path()));
    const QJsonObject result = backend.getDataByKey("regression:persist");
    QVERIFY2(!result.isEmpty(), "Saved key missing after reopen");
    QCOMPARE(result.value("value").toString(), QStringLiteral("persisted-value"));
}

void TestRocksDBBackend::testFormatVersionPreservedIssue2Regression()
{
    // Create a DB with an explicit older format_version (2) that is
    // universally supported by ldb across major versions. After editing via
    // RocksDBBackend, the persisted OPTIONS file must still report
    // format_version=2. If LoadLatestOptions failed silently and we opened
    // with RocksDB 11.x defaults, the new SSTs/OPTIONS would carry the
    // build-default format_version instead, regressing Issue #2.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    // Sanity: original OPTIONS file records format_version=2.
    QVERIFY2(anyOptionsFileContains(dir.path(), "format_version=2"),
             "Pre-condition: created DB should record format_version=2");

    {
        RocksDBBackend backend;
        QVERIFY(backend.openDatabase(dir.path()));
        QVERIFY(backend.setData("issue2:regression", "edited"));
        backend.closeDatabase();
    }

    // The latest OPTIONS file (written or retained by RocksDB on close)
    // must still declare format_version=2. If the fix regressed, this would
    // carry format_version=6 or 7 (RocksDB 11.x build defaults).
    const QString latest = readLatestOptionsFile(dir.path());
    QVERIFY2(!latest.isEmpty(), "No OPTIONS file found after edit+close");
    QVERIFY2(latest.contains("format_version=2"),
             "Issue #2 REGRESSION: format_version was upgraded after edit");
}

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

void TestRocksDBBackend::testOpenedWithDefaultsTrueOnMissingOptions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(createSampleDbWithFormatVersion(dir.path(), 2));

    // Force LoadLatestOptions to fail by removing all OPTIONS files.
    QVERIFY2(removeAllOptionsFiles(dir.path()) > 0,
             "Pre-condition: expected at least one OPTIONS file to remove");

    RocksDBBackend backend;

    // Spy on the toast signal to verify the warning is surfaced to the UI.
    QSignalSpy toastSpy(&backend, &RocksDBBackend::toastRequested);
    QSignalSpy flagSpy(&backend, &RocksDBBackend::openedWithDefaultsChanged);

    QVERIFY(backend.openDatabase(dir.path()));

    QVERIFY2(backend.openedWithDefaults(),
             "openedWithDefaults must be true when LoadLatestOptions fails");
    QCOMPARE(flagSpy.count(), 1);

    // Expect at least one warning toast describing the fallback risk.
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
    QCOMPARE(flagSpy.count(), 2); // true->false transition on close
}

QTEST_MAIN(TestRocksDBBackend)
#include "test_rocksdbbackend.moc"
