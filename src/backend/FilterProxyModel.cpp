#include "FilterProxyModel.h"
#include "EntryModel.h"
#include <QDebug>

/**
 * @brief FilterProxyModelを構築する
 *
 * キーロールでフィルタリングし、キーロールでソートするよう初期化する。
 *
 * @param parent 親QObject
 */
FilterProxyModel::FilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterRole(EntryModel::KeyRole);
    setSortRole(EntryModel::KeyRole);
    setDynamicSortFilter(true);
}

/**
 * @brief 現在のフィルタ文字列を返す
 *
 * @return フィルタ文字列
 */
QString FilterProxyModel::filterText() const
{
    return m_filterText;
}

/**
 * @brief フィルタ文字列を設定する
 *
 * 空文字列の場合はフィルタを解除し、それ以外は大文字小文字を無視して
 * 正規表現フィルタを適用する。
 *
 * @param text フィルタ文字列
 */
void FilterProxyModel::setFilterText(const QString &text)
{
    if (m_filterText != text) {
        m_filterText = text;
        // Filter by key or value
        if (text.isEmpty()) {
            setFilterRegularExpression(QRegularExpression());
        } else {
            QRegularExpression re(text, QRegularExpression::CaseInsensitiveOption);
            setFilterRegularExpression(re);
        }
        emit filterTextChanged();
    }
}

/**
 * @brief 現在のソート順状態を返す
 *
 * @return ソート順状態 (0: ソートなし、1: 昇順、2: 降順)
 */
int FilterProxyModel::sortOrderState() const
{
    return m_sortOrderState;
}

/**
 * @brief ソート順状態を設定する
 *
 * 0 でソート解除、1 で昇順、2 で降順にソートする。
 *
 * @param order ソート順状態
 */
void FilterProxyModel::setSortOrderState(int order)
{
    if (m_sortOrderState != order) {
        m_sortOrderState = order;
        if (order == 0) {
            // No sort: invalidate to restore original order
            invalidate();
        } else if (order == 1) {
            sort(0, Qt::AscendingOrder);
        } else if (order == 2) {
            sort(0, Qt::DescendingOrder);
        }
        emit sortOrderStateChanged();
    }
}

/**
 * @brief 指定行がフィルタ条件に合致するか判定する
 *
 * フィルタ文字列が空の場合は常に true を返す。
 * キーまたは値のいずれかにフィルタ文字列が含まれる場合に true を返す。
 *
 * @param sourceRow ソースモデル行番号
 * @param sourceParent 親インデックス
 * @return 行を表示する場合は true
 */
bool FilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterText.isEmpty())
        return true;

    QModelIndex keyIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    QString key = sourceModel()->data(keyIndex, EntryModel::KeyRole).toString();
    QString value = sourceModel()->data(keyIndex, EntryModel::ValueRole).toString();

    return key.contains(m_filterText, Qt::CaseInsensitive) ||
           value.contains(m_filterText, Qt::CaseInsensitive);
}

/**
 * @brief 2つのソースインデックスを比較してソート順を決定する
 *
 * キーロールの文字列を辞書順で比較する。
 *
 * @param source_left 左辺インデックス
 * @param source_right 右辺インデックス
 * @return left < right の場合は true
 */
bool FilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    QString left = sourceModel()->data(source_left, EntryModel::KeyRole).toString();
    QString right = sourceModel()->data(source_right, EntryModel::KeyRole).toString();
    return left < right;
}
