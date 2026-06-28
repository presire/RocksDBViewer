#include "RocksDBBackend.h"
#include "utils/SettingsMigration.h"
#include "utils/SettingsKeys.h"
#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/convenience.h>
#include <rocksdb/utilities/options_util.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>
#include <QDirIterator>
#include <QFileInfo>
#include <QStringDecoder>
#include <algorithm>
#include <optional>

namespace {
std::optional<QString> utf8StringFromSlice(const rocksdb::Slice &slice)
{
    QByteArray bytes(slice.data(), static_cast<qsizetype>(slice.size()));
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        return std::nullopt;
    }
    return text;
}

std::string utf8BytesFromString(const QString &text)
{
    QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString statusToQString(const rocksdb::Status &status)
{
    return QString::fromStdString(status.ToString());
}

rocksdb::WriteOptions makeDurableWriteOptions()
{
    rocksdb::WriteOptions options;
    options.sync = true;
    options.disableWAL = false;
    return options;
}

rocksdb::WriteOptions makeBatchWriteOptions()
{
    rocksdb::WriteOptions options;
    options.sync = false;
    options.disableWAL = false;
    return options;
}

rocksdb::DBOptions makeOpenDbOptions()
{
    rocksdb::DBOptions options;
    options.create_if_missing = false;
    options.create_missing_column_families = false;
    options.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;
    return options;
}

rocksdb::Status persistColumnFamily(rocksdb::DB *db, rocksdb::ColumnFamilyHandle *handle)
{
    rocksdb::Status syncStatus = db->SyncWAL();
    if (!syncStatus.ok() && syncStatus.IsNotSupported()) {
        rocksdb::FlushOptions flushOptions;
        flushOptions.wait = true;
        return db->Flush(flushOptions, handle);
    }
    return syncStatus;
}
}

struct RocksDBBackend::Impl {
    std::unique_ptr<rocksdb::DB> db;
    QString dbPath;
    QStringList cfNames;
    QString currentCf;
    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    QStringList recentDatabases;
};

RocksDBBackend::RocksDBBackend(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
{
}

RocksDBBackend::~RocksDBBackend()
{
    closeDatabase();
}

bool RocksDBBackend::connected() const
{
    return d->db != nullptr;
}

QString RocksDBBackend::databasePath() const
{
    return d->dbPath;
}

QStringList RocksDBBackend::columnFamilies() const
{
    return d->cfNames;
}

QString RocksDBBackend::currentColumnFamily() const
{
    return d->currentCf;
}

void RocksDBBackend::setCurrentColumnFamily(const QString &cf)
{
    if (d->currentCf != cf) {
        d->currentCf = cf;
        emit currentColumnFamilyChanged();
        emit dataChanged();
    }
}

QStringList RocksDBBackend::recentDatabases() const
{
    auto settings = SettingsMigration::newSettings();
    return settings.value(SettingsKeys::recentDatabases).toStringList();
}

bool RocksDBBackend::openDatabase(const QString &path)
{
    closeDatabase();

    const std::string dbPath = utf8BytesFromString(path);

    std::vector<std::string> cfs;
    rocksdb::Status s = rocksdb::DB::ListColumnFamilies(rocksdb::Options(), dbPath, &cfs);
    if (!s.ok()) {
        cfs = {"default"};
    }

    // Issue #2 fix: Load persisted OPTIONS file to preserve the original
    // table_factory settings (especially BlockBasedTableOptions::format_version).
    // Opening with RocksDB 11.x defaults would write new SST files in
    // format_version 7, which is unreadable by older tooling (e.g. ldb < 10.4).
    // We also pass ignore_unknown_options=true so forward-compatibility options
    // stored by a newer RocksDB do not block opening on this build.
    rocksdb::ConfigOptions configOptions;
    configOptions.ignore_unknown_options = true;

    rocksdb::DBOptions dbOptions = makeOpenDbOptions();
    std::vector<rocksdb::ColumnFamilyDescriptor> loadedDescs;
    std::shared_ptr<rocksdb::Cache> cache;
    rocksdb::Status loadStatus = rocksdb::LoadLatestOptions(
        configOptions, dbPath, &dbOptions, &loadedDescs, &cache);
    bool useLoaded = false;
    if (loadStatus.ok()) {
        useLoaded = true;
        // Re-apply safety-critical open policies regardless of persisted state.
        dbOptions.create_if_missing = false;
        dbOptions.create_missing_column_families = false;
        dbOptions.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;
    } else {
        qWarning() << "LoadLatestOptions failed, falling back to default options:"
                   << statusToQString(loadStatus);
    }

    std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
    descriptors.reserve(cfs.size());
    for (const auto &name : cfs) {
        if (useLoaded) {
            auto it = std::find_if(
                loadedDescs.begin(), loadedDescs.end(),
                [&name](const rocksdb::ColumnFamilyDescriptor &d) { return d.name == name; });
            if (it != loadedDescs.end()) {
                descriptors.emplace_back(name, it->options);
            } else {
                descriptors.emplace_back(name, rocksdb::ColumnFamilyOptions());
            }
        } else {
            descriptors.emplace_back(name, rocksdb::ColumnFamilyOptions());
        }
    }

    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    std::unique_ptr<rocksdb::DB> db;
    s = rocksdb::DB::Open(dbOptions, dbPath, descriptors, &handles, &db);
    if (!s.ok()) {
        if (db) {
            for (auto* h : handles) {
                db->DestroyColumnFamilyHandle(h);
            }
        }
        emit errorOccurred(statusToQString(s));
        return false;
    }

    d->db = std::move(db);
    d->handles = handles;
    d->dbPath = path;
    d->cfNames.clear();
    for (const auto &name : cfs) {
        d->cfNames.append(QString::fromStdString(name));
    }
    if (!d->cfNames.isEmpty()) {
        d->currentCf = d->cfNames.first();
    }

    addToRecent(path);

    emit connectedChanged();
    emit databasePathChanged();
    emit columnFamiliesChanged();
    emit currentColumnFamilyChanged();
    emit dataChanged();
    return true;
}

void RocksDBBackend::closeDatabase()
{
    if (!d->db) return;

    rocksdb::Status syncStatus = d->db->SyncWAL();
    if (!syncStatus.ok() && syncStatus.IsNotSupported()) {
        rocksdb::FlushOptions flushOptions;
        flushOptions.wait = true;
        for (auto* h : d->handles) {
            rocksdb::Status flushStatus = d->db->Flush(flushOptions, h);
            if (!flushStatus.ok()) {
                emit errorOccurred(statusToQString(flushStatus));
                qWarning() << "RocksDB Flush before close failed:" << statusToQString(flushStatus);
            }
        }
    } else if (!syncStatus.ok()) {
        emit errorOccurred(statusToQString(syncStatus));
        qWarning() << "RocksDB SyncWAL before close failed:" << statusToQString(syncStatus);
    }

    for (auto* h : d->handles) {
        rocksdb::Status s = d->db->DestroyColumnFamilyHandle(h);
        if (!s.ok()) {
            emit errorOccurred(statusToQString(s));
        }
    }
    d->handles.clear();
    rocksdb::Status closeStatus = d->db->Close();
    if (!closeStatus.ok()) {
        emit errorOccurred(statusToQString(closeStatus));
        qWarning() << "RocksDB close failed:" << statusToQString(closeStatus);
    }
    d->db.reset();
    d->dbPath.clear();
    d->cfNames.clear();
    d->currentCf.clear();

    emit connectedChanged();
    emit databasePathChanged();
    emit columnFamiliesChanged();
    emit currentColumnFamilyChanged();
}

rocksdb::ColumnFamilyHandle* RocksDBBackend::getColumnFamilyHandle(const QString &name)
{
    if (!d->db) return nullptr;
    for (size_t i = 0; i < d->handles.size(); ++i) {
        if (d->handles[i]->GetName() == utf8BytesFromString(name)) {
            return d->handles[i];
        }
    }
    return nullptr;
}

QJsonObject RocksDBBackend::getData(const QString &search, int offset, int limit)
{
    QJsonObject result;
    if (!d->db) return result;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return result;

    rocksdb::ReadOptions ro;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    QString term = search.toLower();
    int count = 0;
    int matched = 0;
    const int maxEntries = 10000;
    int effectiveLimit = (limit > 0 && limit < maxEntries) ? limit : maxEntries;
    bool skippedBinary = false;

    for (it->SeekToFirst(); it->Valid() && count < effectiveLimit; it->Next()) {
        auto key = utf8StringFromSlice(it->key());
        auto value = utf8StringFromSlice(it->value());
        if (!key || !value) {
            skippedBinary = true;
            continue;
        }
        if (term.isEmpty() || key->toLower().contains(term) || value->toLower().contains(term)) {
            if (matched >= offset) {
                result[*key] = *value;
                count++;
            }
            matched++;
        }
    }
    if (!it->status().ok()) {
        emit errorOccurred(QString::fromStdString(it->status().ToString()));
        return result;
    }
    if (skippedBinary) {
        emit toastRequested(tr("Skipped entries containing non-UTF-8 keys or values"), "warning");
    }
    if (count >= effectiveLimit) {
        emit toastRequested(tr("Loaded %1 entries (limit reached)").arg(effectiveLimit), "warning");
    }
    return result;
}

QJsonObject RocksDBBackend::getDataByPrefix(const QString &prefix, int limit)
{
    QJsonObject result;
    if (!d->db || prefix.isEmpty()) return result;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return result;

    rocksdb::ReadOptions ro;
    ro.prefix_same_as_start = true;

    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    int count = 0;
    bool skippedBinary = false;
    for (it->Seek(utf8BytesFromString(prefix)); it->Valid() && count < limit; it->Next()) {
        auto key = utf8StringFromSlice(it->key());
        auto value = utf8StringFromSlice(it->value());
        if (!key || !value) {
            skippedBinary = true;
            continue;
        }
        if (!key->startsWith(prefix)) break;
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
    if (count >= limit) {
        emit toastRequested(tr("Loaded %1 entries (limit reached)").arg(limit), "warning");
    }
    return result;
}

QJsonObject RocksDBBackend::getDataByKey(const QString &key)
{
    QJsonObject result;
    if (!d->db) return result;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return result;

    std::string value;
    rocksdb::Status s = d->db->Get(rocksdb::ReadOptions(), cf, utf8BytesFromString(key), &value);
    if (s.ok()) {
        rocksdb::Slice valueSlice(value);
        auto textValue = utf8StringFromSlice(valueSlice);
        if (!textValue) {
            emit toastRequested(tr("Skipped entry containing non-UTF-8 value"), "warning");
            return result;
        }
        result["key"] = key;
        result["value"] = *textValue;
    }
    return result;
}

bool RocksDBBackend::setData(const QString &key, const QString &value)
{
    if (!d->db || key.isEmpty()) return false;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return false;

    rocksdb::Status s = d->db->Put(makeDurableWriteOptions(), cf, utf8BytesFromString(key), utf8BytesFromString(value));
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    emit dataChanged();
    emit toastRequested(tr("Data saved"), "success");
    return true;
}

bool RocksDBBackend::deleteData(const QString &key)
{
    if (!d->db) return false;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return false;

    rocksdb::Status s = d->db->Delete(makeDurableWriteOptions(), cf, utf8BytesFromString(key));
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    emit dataChanged();
    emit toastRequested(tr("Data deleted"), "success");
    return true;
}

bool RocksDBBackend::clearData()
{
    if (!d->db) return false;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return false;

    rocksdb::ReadOptions ro;
    rocksdb::WriteBatch batch;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        rocksdb::Status s = batch.Delete(cf, it->key());
        if (!s.ok()) {
            emit errorOccurred(statusToQString(s));
            return false;
        }
    }
    if (!it->status().ok()) {
        emit errorOccurred(statusToQString(it->status()));
        return false;
    }

    rocksdb::Status s = d->db->Write(makeBatchWriteOptions(), &batch);
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    rocksdb::Status syncStatus = persistColumnFamily(d->db.get(), cf);
    if (!syncStatus.ok()) {
        emit errorOccurred(statusToQString(syncStatus));
        return false;
    }
    emit dataChanged();
    emit toastRequested(tr("All data deleted"), "success");
    return true;
}

QJsonObject RocksDBBackend::exportData()
{
    QJsonObject result;
    if (!d->db) return result;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return result;

    rocksdb::ReadOptions ro;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    bool skippedBinary = false;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        auto key = utf8StringFromSlice(it->key());
        auto raw = utf8StringFromSlice(it->value());
        if (!key || !raw) {
            skippedBinary = true;
            continue;
        }
        QJsonDocument doc = QJsonDocument::fromJson(raw->toUtf8());
        if (doc.isObject()) {
            result[*key] = doc.object();
        } else if (doc.isArray()) {
            result[*key] = doc.array();
        } else {
            result[*key] = *raw;
        }
    }
    if (!it->status().ok()) {
        emit errorOccurred(QString::fromStdString(it->status().ToString()));
        return result;
    }
    if (skippedBinary) {
        emit toastRequested(tr("Skipped entries containing non-UTF-8 keys or values"), "warning");
    }
    return result;
}

bool RocksDBBackend::importData(const QJsonObject &data)
{
    if (!d->db) return false;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return false;

    rocksdb::WriteBatch batch;
    int count = 0;
    for (auto it = data.begin(); it != data.end(); ++it) {
        QString valueStr;
        if (it.value().isObject()) {
            valueStr = QString::fromUtf8(QJsonDocument(it.value().toObject()).toJson(QJsonDocument::Compact));
        } else if (it.value().isArray()) {
            valueStr = QString::fromUtf8(QJsonDocument(it.value().toArray()).toJson(QJsonDocument::Compact));
        } else {
            valueStr = it.value().toVariant().toString();
        }
        rocksdb::Status s = batch.Put(cf, utf8BytesFromString(it.key()), utf8BytesFromString(valueStr));
        if (!s.ok()) {
            emit errorOccurred(statusToQString(s));
            return false;
        }
        count++;
    }

    rocksdb::Status s = d->db->Write(makeBatchWriteOptions(), &batch);
    if (!s.ok()) {
        emit errorOccurred(statusToQString(s));
        return false;
    }
    rocksdb::Status syncStatus = persistColumnFamily(d->db.get(), cf);
    if (!syncStatus.ok()) {
        emit errorOccurred(statusToQString(syncStatus));
        return false;
    }
    emit dataChanged();
    emit toastRequested(tr("%1 records imported").arg(count), "success");
    return true;
}

int RocksDBBackend::getTotalEntryCount(const QString &search)
{
    if (!d->db) return 0;

    auto* cf = getColumnFamilyHandle(d->currentCf);
    if (!cf) return 0;

    rocksdb::ReadOptions ro;
    std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
    const QString term = search.toLower();
    constexpr int maxEntries = 10000;
    int count = 0;
    for (it->SeekToFirst(); it->Valid() && count < maxEntries; it->Next()) {
        auto key = utf8StringFromSlice(it->key());
        auto value = utf8StringFromSlice(it->value());
        if (!key || !value) {
            continue;
        }
        if (term.isEmpty() || key->toLower().contains(term) || value->toLower().contains(term)) {
            count++;
        }
    }
    if (!it->status().ok()) {
        emit errorOccurred(QString::fromStdString(it->status().ToString()));
        return count;
    }
    return count;
}

QJsonObject RocksDBBackend::getDatabaseStats()
{
    QJsonObject stats;
    if (!d->db) return stats;

    int totalCount = 0;
    for (const auto &cfName : d->cfNames) {
        auto* cf = getColumnFamilyHandle(cfName);
        if (!cf) continue;
        rocksdb::ReadOptions ro;
        std::unique_ptr<rocksdb::Iterator> it(d->db->NewIterator(ro, cf));
        int cfCount = 0;
        for (it->SeekToFirst(); it->Valid(); it->Next()) cfCount++;
        if (!it->status().ok()) {
            emit errorOccurred(QString::fromStdString(it->status().ToString()));
            continue;
        }
        stats[cfName + "_count"] = cfCount;
        totalCount += cfCount;
    }
    stats["total_count"] = totalCount;
    stats["column_family_count"] = d->cfNames.size();

    QFileInfo dbDir(d->dbPath);
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

void RocksDBBackend::copyToClipboard(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
    emit toastRequested(tr("Copied to clipboard"), "success");
}

void RocksDBBackend::addToRecent(const QString &path)
{
    auto settings = SettingsMigration::newSettings();
    QStringList recent = settings.value(SettingsKeys::recentDatabases).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10) recent.removeLast();
    settings.setValue(SettingsKeys::recentDatabases, recent);
    emit recentDatabasesChanged();
}

void RocksDBBackend::clearRecent()
{
    auto settings = SettingsMigration::newSettings();
    settings.remove(SettingsKeys::recentDatabases);
    emit recentDatabasesChanged();
}

void RocksDBBackend::removeFromRecent(const QString &path)
{
    auto settings = SettingsMigration::newSettings();
    QStringList recent = settings.value(SettingsKeys::recentDatabases).toStringList();
    recent.removeAll(path);
    settings.setValue(SettingsKeys::recentDatabases, recent);
    emit recentDatabasesChanged();
}

bool RocksDBBackend::writeJsonToFile(const QString &filePath, const QJsonObject &data)
{
    QJsonDocument doc(data);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(tr("Failed to open file for writing"));
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    emit toastRequested(tr("Data exported"), "success");
    return true;
}

QJsonObject RocksDBBackend::readJsonFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(tr("Failed to open file for reading"));
        return QJsonObject();
    }
    QByteArray raw = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred(tr("Invalid JSON file"));
        return QJsonObject();
    }
    return doc.object();
}

void RocksDBBackend::saveDarkMode(bool isDark)
{
    auto settings = SettingsMigration::newSettings();
    settings.setValue(SettingsKeys::darkMode, isDark);
}

bool RocksDBBackend::loadDarkMode() const
{
    auto settings = SettingsMigration::newSettings();
    return settings.value(SettingsKeys::darkMode, false).toBool();
}
