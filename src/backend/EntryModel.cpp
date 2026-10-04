#include <QJsonDocument>
#include "EntryModel.h"

/**
 * @brief EntryModelを構築する
 * @param parent 親QObject
 */
EntryModel::EntryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

/**
 * @brief モデルが保持する行数を返す
 * @param parent 親インデックス
 * @return エントリ数
 * @note 親インデックスは階層化していないため使用しない
 */
int EntryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_entries.size();
}

/**
 * @brief 指定インデックスとロールのデータを返す
 * @param index モデルインデックス
 * @param role Qtロール
 * @return ロールに対応するデータ
 * @note DisplayValueRoleではJSONを整形し、200文字を超える値を省略する
 */
QVariant EntryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return QVariant();

    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case KeyRole:
        return entry.key;
    case ValueRole:
        return entry.value;
    case DisplayValueRole: {
        QString display = entry.value;
        // JSONを整形する
        QJsonDocument doc = QJsonDocument::fromJson(display.toUtf8());
        if (!doc.isNull()) {
            display = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        }
        if (display.length() > 200) {
            display = display.left(200) + "...";
        }
        return display;
    }
    default:
        return QVariant();
    }
}

/**
 * @brief QMLで使用するロール名を返す
 * @return ロール番号とロール名の対応表
 */
QHash<int, QByteArray> EntryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[KeyRole] = "key";
    roles[ValueRole] = "value";
    roles[DisplayValueRole] = "displayValue";
    return roles;
}

/**
 * @brief エントリ一覧をJSONオブジェクトから一括設定する
 * @param entries キーと値のペアを含むJSONオブジェクト
 */
void EntryModel::setEntries(const QJsonObject &entries)
{
    beginResetModel();
    m_entries.clear();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        m_entries.append({it.key(), it.value().toString()});
    }
    endResetModel();
}

/**
 * @brief モデルの全エントリを削除する
 */
void EntryModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

/**
 * @brief 指定行のキーを返す
 * @param index 行番号
 * @return キー文字列
 * @note 範囲外の行番号では空文字列を返す
 */
QString EntryModel::keyAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return QString();
    return m_entries.at(index).key;
}

/**
 * @brief 指定行の値を返す
 * @param index 行番号
 * @return 値文字列
 * @note 範囲外の行番号では空文字列を返す
 */
QString EntryModel::valueAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return QString();
    return m_entries.at(index).value;
}
