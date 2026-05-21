#include "EntryModel.h"
#include <QJsonDocument>

EntryModel::EntryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EntryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_entries.size();
}

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

QHash<int, QByteArray> EntryModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[KeyRole] = "key";
    roles[ValueRole] = "value";
    roles[DisplayValueRole] = "displayValue";
    return roles;
}

void EntryModel::setEntries(const QJsonObject &entries)
{
    beginResetModel();
    m_entries.clear();
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        m_entries.append({it.key(), it.value().toString()});
    }
    endResetModel();
}

void EntryModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

QString EntryModel::keyAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return QString();
    return m_entries.at(index).key;
}

QString EntryModel::valueAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return QString();
    return m_entries.at(index).value;
}
