#pragma once

#include <QAbstractListModel>
#include <QJsonObject>

class EntryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        KeyRole = Qt::UserRole + 1,
        ValueRole,
        DisplayValueRole
    };
    Q_ENUM(Roles)

    explicit EntryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setEntries(const QJsonObject &entries);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString keyAt(int index) const;
    Q_INVOKABLE QString valueAt(int index) const;

private:
    struct Entry {
        QString key;
        QString value;
    };
    QList<Entry> m_entries;
};
