#pragma once

#include <QAbstractListModel>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @class FileSystemModel
 * @brief カスタムファイル選択ダイアログ用のファイルシステム一覧モデル
 *
 * 指定ディレクトリの子ディレクトリとファイルを名前順で公開する
 * ファイル名フィルタ、ファイル表示、非表示ファイル表示を切り替えられる
 * 非表示ファイル表示の設定は永続化する
 *
 * m_currentPath : 現在表示中のディレクトリパス
 * m_nameFilters : 適用中のファイル名フィルタ
 * m_showFiles : ファイルを表示するかどうか
 * m_showHidden : 非表示ファイルを表示するかどうか
 * m_entries : 表示対象のファイル情報
 */
class FileSystemModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath WRITE setCurrentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters NOTIFY nameFiltersChanged)
    Q_PROPERTY(bool showFiles READ showFiles WRITE setShowFiles NOTIFY showFilesChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)

public:
    /**
     * @brief QMLへ公開するファイル情報のロール
     */
    enum Roles {
        FileNameRole = Qt::UserRole + 1,  // ファイル名
        FilePathRole,                     // 絶対ファイルパス
        IsDirectoryRole,                  // ディレクトリかどうか
        FileSizeRole,                     // ファイルサイズ
        FileSizeStringRole,               // 表示用ファイルサイズ
        ModifiedDateRole                  // 最終更新日時
    };
    Q_ENUM(Roles)

    /**
     * @brief FileSystemModelを構築する
     * @param parent 親QObject
     */
    explicit FileSystemModel(QObject *parent = nullptr);

    /**
     * @brief モデルが保持する行数を返す
     * @param parent 親インデックス
     * @return エントリ数
     * @note 通常は使用しない
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 指定インデックスとロールのデータを返す
     * @param index モデルインデックス
     * @param role Qtロール
     * @return ロールに対応するデータ
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief QML側で使用するロール名を返す
     * @return ロール番号とロール名の対応表
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 現在のディレクトリパスを返す
     * @return 現在表示中のディレクトリパス
     */
    QString currentPath() const;

    /**
     * @brief 現在のディレクトリパスを設定する
     * @param path 新しいディレクトリパス
     */
    void setCurrentPath(const QString &path);

    /**
     * @brief ファイル名フィルタリストを返す
     * @return フィルタパターンのリスト
     */
    QStringList nameFilters() const;

    /**
     * @brief ファイル名フィルタを設定する
     * @param filters フィルタパターンのリスト
     */
    void setNameFilters(const QStringList &filters);

    /**
     * @brief 表示するディレクトリパスを設定する
     * @param path 新しいディレクトリパス
     */
    Q_INVOKABLE void setPath(const QString &path);

    /**
     * @brief 親ディレクトリへ移動する
     */
    Q_INVOKABLE void navigateUp();

    /**
     * @brief 現在のディレクトリ内容を再読み込みする
     */
    Q_INVOKABLE void refresh();

    /**
     * @brief 指定パスのファイルまたはディレクトリが存在するか判定する
     * @param path 判定するパス
     * @return 存在する場合はtrue
     */
    Q_INVOKABLE bool fileExists(const QString &path) const;

    /**
     * @brief 指定インデックスのファイル情報をマップ形式で返す
     * @param index モデル内のインデックス
     * @return ファイル情報を含むQVariantMap
     */
    Q_INVOKABLE QVariantMap get(int index) const;

    /**
     * @brief プレフィックスに一致する最初のエントリのインデックスを返す
     * @param prefix 検索プレフィックス
     * @return 一致するインデックス
     * @note 見つからない場合は-1を返す
     */
    Q_INVOKABLE int findIndexByPrefix(const QString &prefix) const;

    /**
     * @brief ファイルを表示するかどうかを返す
     * @return ファイルを表示する場合はtrue
     */
    bool showFiles() const;

    /**
     * @brief ファイル表示の有無を設定する
     * @param show ファイルを表示する場合はtrue
     */
    void setShowFiles(bool show);

    /**
     * @brief 非表示ファイルを表示するかどうかを返す
     * @return 非表示ファイルを表示する場合はtrue
     */
    bool showHidden() const;

    /**
     * @brief 非表示ファイル表示の有無を設定する
     * @param show 非表示ファイルを表示する場合はtrue
     */
    void setShowHidden(bool show);

signals:
    /**
     * @brief 現在のディレクトリパスが変更されたことを通知する
     */
    void currentPathChanged();

    /**
     * @brief ファイル名フィルタが変更されたことを通知する
     */
    void nameFiltersChanged();

    /**
     * @brief ファイル表示設定が変更されたことを通知する
     */
    void showFilesChanged();

    /**
     * @brief 非表示ファイル表示設定が変更されたことを通知する
     */
    void showHiddenChanged();

private:
    /**
     * @brief ディレクトリエントリを再読み込みする
     */
    void reload();

    /**
     * @brief バイト数を人間可読なサイズ文字列へ変換する
     * @param bytes バイト数
     * @return 人間可読なサイズ文字列
     */
    static QString formatFileSize(qint64 bytes);

    // パスとフィルタ
    QString m_currentPath;          // 現在表示中のディレクトリパス
    QStringList m_nameFilters;      // 適用中のファイル名フィルタ

    // 表示設定
    bool m_showFiles;               // ファイルを表示するかどうか
    bool m_showHidden;              // 非表示ファイルを表示するかどうか

    // モデルデータ
    QVector<QFileInfo> m_entries;   // 表示対象のファイル情報
};
