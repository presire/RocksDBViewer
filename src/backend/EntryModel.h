#pragma once

#include <QAbstractListModel>
#include <QJsonObject>

/**
 * @class EntryModel
 * @brief キーと値のエントリをQMLへ公開するリストモデル
 *
 * JSONオブジェクトから受け取ったキーと値を行として保持する
 * 各行はキー、生の値、表示用に整形した値のロールで参照できる
 *
 * m_entries : 表示するキーと値のエントリ一覧
 */
class EntryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief EntryModelが公開するロール
     */
    enum Roles {
        KeyRole = Qt::UserRole + 1,  // キー文字列
        ValueRole,                   // 値の生文字列
        DisplayValueRole             // 表示用の値 (JSONは整形し、200文字を超える場合は省略)
    };
    Q_ENUM(Roles)

    /**
     * @brief EntryModelを構築する
     * @param parent 親QObject
     */
    explicit EntryModel(QObject *parent = nullptr);

    /**
     * @brief モデルが保持する行数を返す
     * @param parent 親インデックス
     * @return エントリ数
     * @note 親インデックスは階層化していないため使用しない
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief 指定インデックスとロールのデータを返す
     * @param index モデルインデックス
     * @param role Qtロール
     * @return ロールに対応するデータ
     * @note DisplayValueRoleではJSONを整形し、200文字を超える値を省略する
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief QMLで使用するロール名を返す
     * @return ロール番号とロール名の対応表
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief エントリ一覧をJSONオブジェクトから一括設定する
     * @param entries キーと値のペアを含むJSONオブジェクト
     */
    Q_INVOKABLE void setEntries(const QJsonObject &entries);

    /**
     * @brief モデルの全エントリを削除する
     */
    Q_INVOKABLE void clear();

    /**
     * @brief 指定行のキーを返す
     * @param index 行番号
     * @return キー文字列
     * @note 範囲外の行番号では空文字列を返す
     */
    Q_INVOKABLE QString keyAt(int index) const;

    /**
     * @brief 指定行の値を返す
     * @param index 行番号
     * @return 値文字列
     * @note 範囲外の行番号では空文字列を返す
     */
    Q_INVOKABLE QString valueAt(int index) const;

private:
    /**
     * @struct Entry
     * @brief モデル内で保持するキーと値の組
     */
    struct Entry {
        QString key;    // キー文字列
        QString value;  // 値の生文字列
    };

    // エントリ
    QList<Entry> m_entries;  // 表示するキーと値のエントリ一覧
};
