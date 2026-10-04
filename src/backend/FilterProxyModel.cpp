#include "FilterProxyModel.h"
#include "EntryModel.h"
#include <QDebug>

/**
 * @brief FilterProxyModelを構築する
 * @param parent 親QObject
 * @note キーロールでフィルタとソートを行うよう初期化する
 * @note 表示行数の変化時にcountChangedシグナルを発行するよう接続する
 */
FilterProxyModel::FilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterRole(EntryModel::KeyRole);
    setSortRole(EntryModel::KeyRole);
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::rowsInserted, this, &FilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &FilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &FilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &FilterProxyModel::countChanged);
}

/**
 * @brief 現在のフィルタ文字列を返す
 * @return フィルタ文字列
 */
QString FilterProxyModel::filterText() const
{
    return m_filterText;
}

/**
 * @brief フィルタ文字列を設定する
 * @param text フィルタ文字列
 * @note 空文字列ではフィルタを解除する
 * @note 大文字小文字を区別しない正規表現フィルタを設定する
 */
void FilterProxyModel::setFilterText(const QString &text)
{
    if (m_filterText != text) {
        m_filterText = text;
        // キーまたは値でフィルタする
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
 * @return ソート順状態 (0:ソートなし、1:昇順、2:降順)
 */
int FilterProxyModel::sortOrderState() const
{
    return m_sortOrderState;
}

/**
 * @brief ソート順状態を設定する
 * @param order ソート順状態 (0:ソート解除、1:昇順、2:降順)
 * @note 0を指定するとソース側の元の並び順に戻る
 * @note 状態が変化した場合のみsortOrderStateChangedシグナルを発行する
 */
void FilterProxyModel::setSortOrderState(int order)
{
    if (m_sortOrderState != order) {
        m_sortOrderState = order;
        if (order == 0) {
            // ソートを解除してソース側の並び順に戻す
            sort(-1);
        } else if (order == 1) {
            sort(0, Qt::AscendingOrder);
        } else if (order == 2) {
            sort(0, Qt::DescendingOrder);
        }
        emit sortOrderStateChanged();
    }
}

/**
 * @brief 現在の表示行数を返す
 * @return 表示中の行数
 * @note QMLではproxyModel.countとして参照できる
 */
int FilterProxyModel::count() const
{
    return rowCount();
}

/**
 * @brief 指定行がフィルタ条件に合致するか判定する
 * @param sourceRow ソースモデルの行番号
 * @param sourceParent ソースモデルの親インデックス
 * @return キーまたは値にフィルタ文字列が含まれる場合はtrue
 * @note フィルタ文字列が空の場合は常にtrueを返す
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
 * @brief 2つのソースインデックスをキーで比較する
 * @param source_left 左側のソースインデックス
 * @param source_right 右側のソースインデックス
 * @return 左側のキーが右側のキーより辞書順で前の場合はtrue
 */
bool FilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    QString left = sourceModel()->data(source_left, EntryModel::KeyRole).toString();
    QString right = sourceModel()->data(source_right, EntryModel::KeyRole).toString();
    return left < right;
}
