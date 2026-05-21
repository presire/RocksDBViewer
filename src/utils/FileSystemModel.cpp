#include "FileSystemModel.h"
#include <QDir>
#include <algorithm>
#include "SettingsMigration.h"
#include "SettingsKeys.h"

FileSystemModel::FileSystemModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_showFiles(true)
    , m_showHidden(false)
{
    auto settings = SettingsMigration::newSettings();
    m_showHidden = settings.value(SettingsKeys::showHiddenFiles, false).toBool();
    setPath(QDir::homePath());
}

int FileSystemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant FileSystemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return QVariant();

    const QFileInfo &info = m_entries.at(index.row());
    switch (role) {
    case FileNameRole:
        return info.fileName();
    case FilePathRole:
        return info.absoluteFilePath();
    case IsDirectoryRole:
        return info.isDir();
    case FileSizeRole:
        return info.size();
    case FileSizeStringRole:
        return formatFileSize(info.size());
    case ModifiedDateRole:
        return info.lastModified();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileSystemModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FileNameRole] = "fileName";
    roles[FilePathRole] = "filePath";
    roles[IsDirectoryRole] = "isDirectory";
    roles[FileSizeRole] = "fileSize";
    roles[FileSizeStringRole] = "fileSizeString";
    roles[ModifiedDateRole] = "modifiedDate";
    return roles;
}

QString FileSystemModel::currentPath() const
{
    return m_currentPath;
}

void FileSystemModel::setCurrentPath(const QString &path)
{
    if (m_currentPath != path) {
        m_currentPath = path;
        emit currentPathChanged();
        reload();
    }
}

QStringList FileSystemModel::nameFilters() const
{
    return m_nameFilters;
}

void FileSystemModel::setNameFilters(const QStringList &filters)
{
    if (m_nameFilters != filters) {
        m_nameFilters = filters;
        emit nameFiltersChanged();
        reload();
    }
}

void FileSystemModel::setPath(const QString &path)
{
    setCurrentPath(path);
}

void FileSystemModel::navigateUp()
{
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        setCurrentPath(dir.absolutePath());
    }
}

void FileSystemModel::refresh()
{
    reload();
}

bool FileSystemModel::fileExists(const QString &path) const
{
    return QFile::exists(path);
}

QVariantMap FileSystemModel::get(int index) const
{
    QVariantMap map;
    if (index < 0 || index >= m_entries.size())
        return map;
    const QFileInfo &info = m_entries.at(index);
    map["fileName"] = info.fileName();
    map["filePath"] = info.absoluteFilePath();
    map["isDirectory"] = info.isDir();
    map["fileSize"] = info.size();
    map["modifiedDate"] = info.lastModified();
    return map;
}

int FileSystemModel::findIndexByPrefix(const QString &prefix) const
{
    if (prefix.isEmpty())
        return -1;
    QString lowerPrefix = prefix.toLower();
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).fileName().toLower().startsWith(lowerPrefix)) {
            return i;
        }
    }
    return -1;
}

bool FileSystemModel::showFiles() const
{
    return m_showFiles;
}

void FileSystemModel::setShowFiles(bool show)
{
    if (m_showFiles != show) {
        m_showFiles = show;
        emit showFilesChanged();
        reload();
    }
}

bool FileSystemModel::showHidden() const
{
    return m_showHidden;
}

void FileSystemModel::setShowHidden(bool show)
{
    if (m_showHidden != show) {
        m_showHidden = show;
        auto settings = SettingsMigration::newSettings();
        settings.setValue(SettingsKeys::showHiddenFiles, m_showHidden);
        emit showHiddenChanged();
        reload();
    }
}

QString FileSystemModel::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
}

void FileSystemModel::reload()
{
    beginResetModel();
    m_entries.clear();

    QDir dir(m_currentPath);
    if (!dir.exists()) {
        endResetModel();
        return;
    }

    QDir::Filters dirFlags = QDir::Dirs | QDir::NoDotAndDotDot;
    QDir::Filters fileFlags = QDir::Files;
    if (m_showHidden) {
        dirFlags |= QDir::Hidden;
        fileFlags |= QDir::Hidden;
    }
    QFileInfoList dirs = dir.entryInfoList(dirFlags, QDir::Name);
    QFileInfoList files = dir.entryInfoList(fileFlags, QDir::Name);

    if (!m_nameFilters.isEmpty()) {
        QFileInfoList filteredFiles;
        for (const auto &fi : files) {
            for (const QString &pattern : m_nameFilters) {
                if (QDir::match(pattern, fi.fileName())) {
                    filteredFiles.append(fi);
                    break;
                }
            }
        }
        files = filteredFiles;
    }

    auto caseInsensitiveLessThan = [](const QFileInfo &a, const QFileInfo &b) {
        return a.fileName().compare(b.fileName(), Qt::CaseInsensitive) < 0;
    };
    std::sort(dirs.begin(), dirs.end(), caseInsensitiveLessThan);
    std::sort(files.begin(), files.end(), caseInsensitiveLessThan);

    m_entries.reserve(dirs.size() + (m_showFiles ? files.size() : 0));
    for (const auto &d : dirs) {
        m_entries.append(d);
    }
    if (m_showFiles) {
        for (const auto &f : files) {
            m_entries.append(f);
        }
    }

    endResetModel();
}
