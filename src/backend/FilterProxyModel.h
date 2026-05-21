#pragma once

#include <QSortFilterProxyModel>

class FilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int sortOrderState READ sortOrderState WRITE setSortOrderState NOTIFY sortOrderStateChanged)

public:
    explicit FilterProxyModel(QObject *parent = nullptr);

    QString filterText() const;
    void setFilterText(const QString &text);

    int sortOrderState() const; // 0:none, 1:asc, 2:desc
    void setSortOrderState(int order);

signals:
    void filterTextChanged();
    void sortOrderStateChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    QString m_filterText;
    int m_sortOrderState = 0;
};
