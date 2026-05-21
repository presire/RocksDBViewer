#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <memory>

namespace rocksdb {
class DB;
class ColumnFamilyHandle;
struct Options;
}

class RocksDBBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString databasePath READ databasePath NOTIFY databasePathChanged)
    Q_PROPERTY(QStringList columnFamilies READ columnFamilies NOTIFY columnFamiliesChanged)
    Q_PROPERTY(QString currentColumnFamily READ currentColumnFamily WRITE setCurrentColumnFamily NOTIFY currentColumnFamilyChanged)
    Q_PROPERTY(QStringList recentDatabases READ recentDatabases NOTIFY recentDatabasesChanged)

public:
    explicit RocksDBBackend(QObject *parent = nullptr);
    ~RocksDBBackend();

    bool connected() const;
    QString databasePath() const;
    QStringList columnFamilies() const;
    QString currentColumnFamily() const;
    void setCurrentColumnFamily(const QString &cf);
    QStringList recentDatabases() const;

    Q_INVOKABLE bool openDatabase(const QString &path);
    Q_INVOKABLE void closeDatabase();
    Q_INVOKABLE QJsonObject getData(const QString &search = QString(), int offset = 0, int limit = 1000);
    Q_INVOKABLE QJsonObject getDataByPrefix(const QString &prefix, int limit = 10000);
    Q_INVOKABLE QJsonObject getDataByKey(const QString &key);
    Q_INVOKABLE QJsonObject getDatabaseStats();
    Q_INVOKABLE int getTotalEntryCount();
    Q_INVOKABLE bool setData(const QString &key, const QString &value);
    Q_INVOKABLE bool deleteData(const QString &key);
    Q_INVOKABLE bool clearData();
    Q_INVOKABLE QJsonObject exportData();
    Q_INVOKABLE bool importData(const QJsonObject &data);

    // File I/O helpers
    Q_INVOKABLE bool writeJsonToFile(const QString &filePath, const QJsonObject &data);
    Q_INVOKABLE QJsonObject readJsonFromFile(const QString &filePath);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void addToRecent(const QString &path);
    Q_INVOKABLE void clearRecent();
    Q_INVOKABLE void removeFromRecent(const QString &path);

    // Theme persistence
    Q_INVOKABLE void saveDarkMode(bool isDark);
    Q_INVOKABLE bool loadDarkMode() const;

signals:
    void connectedChanged();
    void databasePathChanged();
    void columnFamiliesChanged();
    void currentColumnFamilyChanged();
    void errorOccurred(const QString &message);
    void dataChanged();
    void toastRequested(const QString &message, const QString &type);
    void recentDatabasesChanged();

private:
    rocksdb::ColumnFamilyHandle* getColumnFamilyHandle(const QString &name);

    struct Impl;
    std::unique_ptr<Impl> d;
};
