#include "FilterProxyModel.h"
#include "EntryModel.h"
#include <QDebug>

FilterProxyModel::FilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterRole(EntryModel::KeyRole);
    setSortRole(EntryModel::KeyRole);
    setDynamicSortFilter(true);
}

QString FilterProxyModel::filterText() const
{
    return m_filterText;
}

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

int FilterProxyModel::sortOrderState() const
{
    return m_sortOrderState;
}

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

bool FilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    QString left = sourceModel()->data(source_left, EntryModel::KeyRole).toString();
    QString right = sourceModel()->data(source_right, EntryModel::KeyRole).toString();
    return left < right;
}
