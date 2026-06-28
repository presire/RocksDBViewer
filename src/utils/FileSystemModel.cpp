#include "FileSystemModel.h"
#include <QDir>
#include <algorithm>
#include "SettingsMigration.h"
#include "SettingsKeys.h"

/**
 * @brief FileSystemModelを構築する
 *
 * ホームディレクトリを初期パスとし、非表示ファイルの表示設定を永続化された設定から読み込む
 *
 * @param parent 親QObject
 */
FileSystemModel::FileSystemModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_showFiles(true)
    , m_showHidden(false)
{
    auto settings = SettingsMigration::newSettings();
    m_showHidden = settings.value(SettingsKeys::showHiddenFiles, false).toBool();
    setPath(QDir::homePath());
}

/**
 * @brief モデルが保持する行数を返す
 *
 * @param parent 親インデックス (通常は使用しない)
 * @return エントリ数
 */
int FileSystemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

/**
 * @brief 指定インデックス・ロールのデータを返す
 *
 * @param index モデルインデックス
 * @param role Qtロール
 * @return ロールに対応するデータ
 */
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

/**
 * @brief QML側で使用するロール名を返す
 *
 * @return ロール番号とロール名のマッピング
 */
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

/**
 * @brief 現在のパスを返す
 *
 * @return 現在表示中のディレクトリパス
 */
QString FileSystemModel::currentPath() const
{
    return m_currentPath;
}

/**
 * @brief 現在のパスを設定する
 *
 * パスが変更された場合は currentPathChanged シグナルを発行し、エントリリストを再読み込みする
 *
 * @param path 新しいパス
 */
void FileSystemModel::setCurrentPath(const QString &path)
{
    if (m_currentPath != path) {
        m_currentPath = path;
        emit currentPathChanged();
        reload();
    }
}

/**
 * @brief ファイル名フィルタリストを返す
 *
 * @return フィルタパターンのリスト
 */
QStringList FileSystemModel::nameFilters() const
{
    return m_nameFilters;
}

/**
 * @brief ファイル名フィルタを設定する
 *
 * @param filters フィルタパターンのリスト
 */
void FileSystemModel::setNameFilters(const QStringList &filters)
{
    if (m_nameFilters != filters) {
        m_nameFilters = filters;
        emit nameFiltersChanged();
        reload();
    }
}

/**
 * @brief 表示パスを設定する
 *
 * @param path 新しいパス
 */
void FileSystemModel::setPath(const QString &path)
{
    setCurrentPath(path);
}

/**
 * @brief 親ディレクトリへ移動する
 */
void FileSystemModel::navigateUp()
{
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        setCurrentPath(dir.absolutePath());
    }
}

/**
 * @brief 現在のディレクトリ内容を再読み込みする
 */
void FileSystemModel::refresh()
{
    reload();
}

/**
 * @brief 指定パスのファイル・ディレクトリが存在するか判定する
 *
 * @param path パス
 * @return 存在する場合は true
 */
bool FileSystemModel::fileExists(const QString &path) const
{
    return QFile::exists(path);
}

/**
 * @brief 指定インデックスのファイル情報をマップ形式で返す
 *
 * @param index インデックス
 * @return ファイル情報を含む QVariantMap
 */
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

/**
 * @brief プレフィックスに一致する最初のエントリのインデックスを返す
 *
 * @param prefix 検索プレフィックス
 * @return 一致するインデックス。見つからない場合は -1
 */
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

/**
 * @brief ファイルを表示するかどうかを返す
 *
 * @return ファイルを表示する場合は true
 */
bool FileSystemModel::showFiles() const
{
    return m_showFiles;
}

/**
 * @brief ファイル表示の有無を設定する
 *
 * @param show ファイルを表示する場合は true
 */
void FileSystemModel::setShowFiles(bool show)
{
    if (m_showFiles != show) {
        m_showFiles = show;
        emit showFilesChanged();
        reload();
    }
}

/**
 * @brief 非表示ファイルを表示するかどうかを返す
 *
 * @return 非表示ファイルを表示する場合は true
 */
bool FileSystemModel::showHidden() const
{
    return m_showHidden;
}

/**
 * @brief 非表示ファイル表示の有無を設定する
 *
 * 設定は永続化される
 *
 * @param show 非表示ファイルを表示する場合は true
 */
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

/**
 * @brief バイト数を人間可読な文字列に変換する
 *
 * @param bytes バイト数
 * @return 人間可読なサイズ文字列 (例: "1.5 MB")
 */
QString FileSystemModel::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
}

/**
 * @brief ディレクトリエントリを再読み込みする
 */
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
