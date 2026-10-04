#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <functional>
#include <memory>

class QFile;

namespace rocksdb {
class DB;
class ColumnFamilyHandle;
struct Options;
}

/**
 * @class RocksDBBackend
 * @brief RocksDBの操作とQMLへの公開を担うバックエンド
 *
 * RocksDBの型はPIMPLパターンでヘッダから隠蔽し、QMLにはシングルトンBackendとして公開する
 * DBの操作結果は戻り値 (boolまたはQJsonObject) とシグナルで呼び出し側へ伝える
 *
 * d : rocksdb::DBやカラムファミリハンドルなど、RocksDBの内部状態を所有する
 */
class RocksDBBackend : public QObject
{
    Q_OBJECT

    /** @brief QMLへ接続状態を公開するプロパティ */
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    /** @brief QMLへ開いているデータベースのパスを公開するプロパティ */
    Q_PROPERTY(QString databasePath READ databasePath NOTIFY databasePathChanged)
    /** @brief QMLへ利用可能なカラムファミリー一覧を公開するプロパティ */
    Q_PROPERTY(QStringList columnFamilies READ columnFamilies NOTIFY columnFamiliesChanged)
    /** @brief QMLへ現在選択中のカラムファミリーを公開するプロパティ */
    Q_PROPERTY(QString currentColumnFamily READ currentColumnFamily WRITE setCurrentColumnFamily NOTIFY currentColumnFamilyChanged)
    /** @brief QMLへ最近開いたデータベース一覧を公開するプロパティ */
    Q_PROPERTY(QStringList recentDatabases READ recentDatabases NOTIFY recentDatabasesChanged)
    /** @brief QMLへOPTIONSファイルなしで開いたフォールバック状態を公開するプロパティ */
    Q_PROPERTY(bool openedWithDefaults READ openedWithDefaults NOTIFY openedWithDefaultsChanged)
    /** @brief QMLへ読み取り専用モードかどうかを公開するプロパティ */
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY readOnlyChanged)
    /** @brief QMLへTTLデータベース検出状態を公開するプロパティ */
    Q_PROPERTY(bool ttlDetected READ ttlDetected NOTIFY ttlDetectedChanged)
    /** @brief QMLへ編集が禁止される状態を公開するプロパティ */
    Q_PROPERTY(bool editLocked READ editLocked NOTIFY editLockedChanged)
    /** @brief QMLへ編集禁止の理由を公開するプロパティ */
    Q_PROPERTY(QString editLockReason READ editLockReason NOTIFY editLockedChanged)

public:
    /**
     * @brief RocksDBBackendを構築する
     * @param parent 親QObject
     */
    explicit RocksDBBackend(QObject *parent = nullptr);

    /** @brief RocksDBBackendを破棄して接続を閉じる */
    ~RocksDBBackend();

    /** @brief データベースへ接続しているか返す
     * @return 接続中ならtrue
     */
    bool connected() const;

    /** @brief 開いているデータベースの正規化済みパスを返す
     * @return データベースのパス
     */
    QString databasePath() const;

    /** @brief 利用可能なカラムファミリーの表示名一覧を返す
     * @return カラムファミリーの表示名一覧
     */
    QStringList columnFamilies() const;

    /** @brief 現在選択中のカラムファミリー表示名を返す
     * @return 選択中のカラムファミリー表示名
     */
    QString currentColumnFamily() const;

    /** @brief QMLから現在のカラムファミリーを切り替える
     * @param cf 切り替えるカラムファミリーの表示名
     * @note 存在しない名前はエラーシグナルで通知して変更しない
     */
    void setCurrentColumnFamily(const QString &cf);

    /** @brief 最近開いたデータベースのパス一覧を返す
     * @return 最大10件のデータベースパス一覧
     */
    QStringList recentDatabases() const;

    /** @brief デフォルトオプションで開いたフォールバック状態か返す
     * @return OPTIONS読み込み失敗時のフォールバックならtrue
     */
    bool openedWithDefaults() const;

    /** @brief 読み取り専用モードで開かれているか返す
     * @return 読み取り専用ならtrue
     */
    bool readOnly() const;

    /** @brief TTLデータベースとして検出されたか返す
     * @return TTLデータベースならtrue
     */
    bool ttlDetected() const;

    /** @brief 編集が禁止されているか返す
     * @return 編集禁止ならtrue
     */
    bool editLocked() const;

    /** @brief 編集禁止の理由を返す
     * @return 編集可能な場合は空文字列
     */
    QString editLockReason() const;

    /** @brief QMLから指定パスのデータベースを開く
     * @param path 開くデータベースディレクトリのパス
     * @param readOnly 読み取り専用で開く場合はtrue
     * @return 開くことに成功した場合はtrue
     * @note TTL検出 / TTL状態未判定時は、編集要求を読み取り専用へ強制する
     */
    Q_INVOKABLE bool openDatabase(const QString &path, bool readOnly = true);

    /** @brief QMLから現在のデータベースを指定モードで開き直す
     * @param readOnly 読み取り専用にする場合はtrue
     * @return モード変更または変更不要の場合はtrue
     */
    Q_INVOKABLE bool setReadOnlyMode(bool readOnly);

    /** @brief QMLから開いているデータベースを閉じる */
    Q_INVOKABLE void closeDatabase();

    /** @brief QMLから現在のカラムファミリーを検索して取得する
     * @param search キーと値に適用する検索文字列
     * @param offset 取得開始位置
     * @param limit 取得件数の上限
     * @return キーと値の組を含むQJsonObject
     */
    Q_INVOKABLE QJsonObject getData(const QString &search = QString(), int offset = 0, int limit = 1000);

    /** @brief QMLから単一スナップショット上のページと件数を取得する
     * @param search キーと値に適用する検索文字列
     * @param offset 取得開始位置
     * @param limit ページ件数の上限
     * @param maxCount 集計上限
     * @return entries、total、capped、hiddenNonUtf8を含むQJsonObject
     */
    Q_INVOKABLE QJsonObject getPage(const QString &search, int offset, int limit, int maxCount);

    /** @brief QMLから指定プレフィックスで始まるデータを取得する
     * @param prefix キーに適用するプレフィックス
     * @param limit 取得件数の上限
     * @return キーと値の組を含むQJsonObject
     */
    Q_INVOKABLE QJsonObject getDataByPrefix(const QString &prefix, int limit = 10000);

    /** @brief QMLから指定キーのデータを取得する
     * @param key 取得するキー
     * @return keyとvalueを含むQJsonObject、見つからない場合は空オブジェクト
     */
    Q_INVOKABLE QJsonObject getDataByKey(const QString &key);

    /** @brief QMLからデータベース統計を取得する
     * @return カラムファミリー別推定件数とディスク使用量を含むQJsonObject
     */
    Q_INVOKABLE QJsonObject getDatabaseStats();

    /** @brief QMLから検索条件に一致するエントリ数を取得する
     * @param search キーと値に適用する検索文字列
     * @param maxCount 集計上限
     * @return 上限までの一致件数
     */
    Q_INVOKABLE int getTotalEntryCount(const QString &search = QString(), int maxCount = 10000);

    /** @brief QMLから指定キーの存在を確認する
     * @param key 確認するキー
     * @return 存在する場合はtrue
     */
    Q_INVOKABLE bool keyExists(const QString &key);

    /** @brief QMLから現在のカラムファミリーの全件数を取得する
     * @return totalとnonUtf8を含むQJsonObject
     */
    Q_INVOKABLE QJsonObject getEntryCounts();

    /** @brief QMLから現在のカラムファミリーへキーと値を書き込む
     * @param key 保存するキー
     * @param value 保存する値
     * @return 書き込みに成功した場合はtrue
     */
    Q_INVOKABLE bool setData(const QString &key, const QString &value);

    /** @brief QMLから現在のカラムファミリーのキーを削除する
     * @param key 削除するキー
     * @return 削除に成功した場合はtrue
     */
    Q_INVOKABLE bool deleteData(const QString &key);

    /** @brief QMLから現在のカラムファミリーの全データを削除する
     * @return 削除と永続化に成功した場合はtrue
     */
    Q_INVOKABLE bool clearData();

    /** @brief QMLから現在のカラムファミリーをメモリ上のJSONへ変換する
     * @return キーと値の組を含むQJsonObject
     * @note JSONオブジェクトと配列の値は再シリアライズされるためバイト完全な複製にはならない
     */
    Q_INVOKABLE QJsonObject exportData();

    /** @brief QMLからメモリ上のJSONを現在のカラムファミリーへ一括保存する
     * @param data 保存するキーと値の組
     * @return 保存と永続化に成功した場合はtrue
     * @note バイト完全な入出力にはexportToFileとimportFromFileを使用する
     */
    Q_INVOKABLE bool importData(const QJsonObject &data);

    /** @brief QMLから現在のカラムファミリーをJSONファイルへ出力する
     * @param filePath 出力ファイルのパス
     * @param allowOverwrite 既存ファイルを置換する場合はtrue
     * @return 出力に成功した場合はtrue
     */
    Q_INVOKABLE bool exportToFile(const QString &filePath,
                                  bool allowOverwrite = false);

    /** @brief QMLからJSONファイルを現在のカラムファミリーへ入力する
     * @param filePath 入力ファイルのパス
     * @return 入力と永続化に成功した場合はtrue
     */
    Q_INVOKABLE bool importFromFile(const QString &filePath);

    /** @brief QMLからQJsonObjectをJSONファイルへ書き出す
     * @param filePath 出力ファイルのパス
     * @param data 書き出すJSONオブジェクト
     * @param allowOverwrite 既存ファイルを置換する場合はtrue
     * @return 書き出しに成功した場合はtrue
     */
    Q_INVOKABLE bool writeJsonToFile(const QString &filePath,
                                     const QJsonObject &data,
                                     bool allowOverwrite = false);

    /** @brief QMLからJSONファイルをQJsonObjectとして読み込む
     * @param filePath 入力ファイルのパス
     * @return 読み込んだJSONオブジェクト、失敗時は空オブジェクト
     */
    Q_INVOKABLE QJsonObject readJsonFromFile(const QString &filePath);

    /** @brief QMLから文字列をシステムクリップボードへコピーする
     * @param text コピーする文字列
     */
    Q_INVOKABLE void copyToClipboard(const QString &text);

    /** @brief QMLから最近開いたデータベースへパスを追加する
     * @param path 追加するデータベースのパス
     */
    Q_INVOKABLE void addToRecent(const QString &path);

    /** @brief QMLから最近開いたデータベース一覧を消去する */
    Q_INVOKABLE void clearRecent();

    /** @brief QMLから最近開いたデータベース一覧のパスを削除する
     * @param path 削除するデータベースのパス
     */
    Q_INVOKABLE void removeFromRecent(const QString &path);

    /** @brief QMLからダークモード設定を保存する
     * @param isDark ダークモードを有効にする場合はtrue
     */
    Q_INVOKABLE void saveDarkMode(bool isDark);

    /** @brief QMLから保存済みダークモード設定を取得する
     * @return ダークモードが有効な場合はtrue
     */
    Q_INVOKABLE bool loadDarkMode() const;

signals:
    /** @brief QMLへ接続状態の変化を通知する */
    void connectedChanged();
    /** @brief QMLへデータベースパスの変化を通知する */
    void databasePathChanged();
    /** @brief QMLへカラムファミリー一覧の変化を通知する */
    void columnFamiliesChanged();
    /** @brief QMLへ現在のカラムファミリーの変化を通知する */
    void currentColumnFamilyChanged();
    /** @brief QMLへ操作エラーの内容を通知する
     * @param message エラー内容
     */
    void errorOccurred(const QString &message);
    /** @brief QMLへ表示データの再読み込みが必要なことを通知する */
    void dataChanged();
    /** @brief QMLへトースト通知の内容を通知する
     * @param message 表示するメッセージ
     * @param type 通知種別
     */
    void toastRequested(const QString &message, const QString &type);
    /** @brief QMLへ最近開いたデータベース一覧の変化を通知する */
    void recentDatabasesChanged();
    /** @brief QMLへデフォルトオプションのフォールバック状態変化を通知する */
    void openedWithDefaultsChanged();
    /** @brief QMLへ読み取り専用モードの変化を通知する */
    void readOnlyChanged();
    /** @brief QMLへTTLデータベース検出状態の変化を通知する */
    void ttlDetectedChanged();
    /** @brief QMLへ編集禁止状態または理由の変化を通知する */
    void editLockedChanged();

private:
    /** @brief 表示名に対応するカラムファミリーハンドルを返す
     * @param name カラムファミリーの表示名
     * @return 対応するハンドル、見つからない場合はnullptr
     */
    rocksdb::ColumnFamilyHandle* getColumnFamilyHandle(const QString &name);

    /** @brief 正規化済みパスで状態通知なしにデータベースを開く
     * @param canonicalPath 正規化済みデータベースディレクトリのパス
     * @param readOnly 読み取り専用で開く場合はtrue
     * @param error 失敗理由の格納先
     * @param ttlForced 編集要求を読み取り専用へ強制したかの格納先
     * @param usedDefaults OPTIONS読み込み失敗フォールバックの有無の格納先
     * @return 開くことに成功した場合はtrue
     */
    bool openInternal(const QString &canonicalPath, bool readOnly, QString *error,
                      bool *ttlForced, bool *usedDefaults);

    /** @brief 状態通知なしに開いているデータベースを閉じる */
    void closeInternal();

    /** @brief DB状態に対応する全QML通知シグナルを発行する */
    void emitAllStateSignals();

    /** @brief 一時ファイルを使って対象ファイルを原子的に書き換える
     * @param targetPath 出力先ファイルのパス
     * @param writer 一時ファイルへ内容を書き込む関数
     * @param error 失敗理由の格納先
     * @param allowOverwrite 既存ファイルを置換する場合はtrue
     * @return 公開に成功した場合はtrue
     */
    static bool writeFileAtomically(const QString &targetPath,
                                    const std::function<bool(QFile &)> &writer,
                                    QString *error, bool allowOverwrite);

    /**
     * @struct Impl
     * @brief RocksDBBackendの実装状態を保持するPIMPL
     *
     * handles : dbより先に破棄する必要があるカラムファミリーハンドルを保持する
     * db : RocksDBデータベース本体を所有する
     */
    struct Impl;

    // 実装状態
    std::unique_ptr<Impl> d;  // RocksDBの内部状態を所有するPIMPL
};
