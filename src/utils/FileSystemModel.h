#pragma once

#include <QAbstractListModel>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>

class FileSystemModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath WRITE setCurrentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters NOTIFY nameFiltersChanged)
    Q_PROPERTY(bool showFiles READ showFiles WRITE setShowFiles NOTIFY showFilesChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)

public:
    enum Roles {
        FileNameRole = Qt::UserRole + 1,
        FilePathRole,
        IsDirectoryRole,
        FileSizeRole,
        FileSizeStringRole,
        ModifiedDateRole
    };
    Q_ENUM(Roles)

    explicit FileSystemModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentPath() const;
    void setCurrentPath(const QString &path);

    QStringList nameFilters() const;
    void setNameFilters(const QStringList &filters);

    Q_INVOKABLE void setPath(const QString &path);
    Q_INVOKABLE void navigateUp();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool fileExists(const QString &path) const;
    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE int findIndexByPrefix(const QString &prefix) const;

    bool showFiles() const;
    void setShowFiles(bool show);

    bool showHidden() const;
    void setShowHidden(bool show);

signals:
    void currentPathChanged();
    void nameFiltersChanged();
    void showFilesChanged();
    void showHiddenChanged();

private:
    void reload();
    static QString formatFileSize(qint64 bytes);

    QString m_currentPath;
    QStringList m_nameFilters;
    QVector<QFileInfo> m_entries;
    bool m_showFiles;
    bool m_showHidden;
};
