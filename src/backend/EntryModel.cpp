#include "EntryModel.h"
#include <QJsonDocument>

/**
 * @brief EntryModelを構築する
 *
 * @param parent 親QObject
 */
EntryModel::EntryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

/**
 * @brief モデルが保持する行数を返す
 *
 * @param parent 親インデックス (通常は使用しない)
 * @return エントリ数
 */
int EntryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_entries.size();
}

/**
 * @brief 指定インデックス・ロールのデータを返す
 *
 * DisplayValueRole の場合、JSONであれば整形し、200文字を超える場合は
 * 末尾に "..." を付けて省略表示する。
 *
 * @param index モデルインデックス
 * @param role Qtロール
 * @return ロールに対応するデータ
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
        // Try to pretty-print JSON
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
 * @brief QML側で使用するロール名を返す
 *
 * @return ロール番号とロール名のマッピング
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
 * @brief エントリリストを一括設定する
 *
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
 * @brief モデルのデータをクリアする
 */
void EntryModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

/**
 * @brief 指定インデックスのキーを返す
 *
 * @param index インデックス
 * @return キー文字列。範囲外の場合は空文字列
 */
QString EntryModel::keyAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return QString();
    return m_entries.at(index).key;
}

/**
 * @brief 指定インデックスの値を返す
 *
 * @param index インデックス
 * @return 値文字列。範囲外の場合は空文字列
 */
QString EntryModel::valueAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return QString();
    return m_entries.at(index).value;
}
