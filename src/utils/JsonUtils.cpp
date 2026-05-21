#include "JsonUtils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include <QRegularExpression>

JsonUtils::JsonUtils(QObject *parent)
    : QObject(parent)
{
}

QString JsonUtils::formatJson(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) return json;
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString JsonUtils::minifyJson(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) return json;
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

bool JsonUtils::isValidJson(const QString &json)
{
    QJsonParseError err;
    QJsonDocument::fromJson(json.toUtf8(), &err);
    return err.error == QJsonParseError::NoError;
}

QString JsonUtils::validationMessage(const QString &json)
{
    QJsonParseError err;
    QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError) {
        return QObject::tr("Valid JSON format");
    }
    if (json.trimmed().isEmpty()) {
        return QString();
    }
    return QObject::tr("Not in JSON format (will be saved as string)");
}

bool JsonUtils::isJsonLike(const QString &value)
{
    QString trimmed = value.trimmed();
    return (trimmed.startsWith('{') && trimmed.endsWith('}')) ||
           (trimmed.startsWith('[') && trimmed.endsWith(']'));
}

static const QVariantList &skeletonData()
{
    static const QVariantList data = []() {
        QVariantList list;
        auto add = [&list](const QString &id, const QString &labelKey,
                           const QString &content, const QString &keyPattern = QString()) {
            QVariantMap m;
            m.insert(QStringLiteral("id"), id);
            m.insert(QStringLiteral("labelKey"), labelKey);
            m.insert(QStringLiteral("content"), content);
            m.insert(QStringLiteral("keyPattern"), keyPattern);
            list.append(m);
        };
        add(QStringLiteral("empty-object"),   QStringLiteral("Empty Object"),     QStringLiteral("{}"));
        add(QStringLiteral("empty-array"),    QStringLiteral("Empty Array"),      QStringLiteral("[]"));
        add(QStringLiteral("simple-object"),  QStringLiteral("Simple Object"),    QStringLiteral("{\"id\": \"\", \"name\": \"\", \"value\": \"\"}"));
        add(QStringLiteral("nested-object"),  QStringLiteral("Nested Object"),    QStringLiteral("{\"user\": {\"id\": \"\", \"name\": \"\"}, \"meta\": {\"createdAt\": \"\"}}"));
        add(QStringLiteral("array-objects"),  QStringLiteral("Array of Objects"), QStringLiteral("[{\"id\": \"\", \"name\": \"\"}]"));
        add(QStringLiteral("user-profile"),   QStringLiteral("User Profile"),     QStringLiteral("{\"id\": \"\", \"name\": \"\", \"email\": \"\", \"createdAt\": \"\"}"), QStringLiteral("^user:"));
        add(QStringLiteral("config"),         QStringLiteral("Config"),           QStringLiteral("{\"key\": \"\", \"value\": \"\", \"enabled\": true}"), QStringLiteral("^config:"));
        return list;
    }();
    return data;
}

QVariantList JsonUtils::skeletonTemplates()
{
    return skeletonData();
}

QString JsonUtils::generateSkeleton(const QString &id)
{
    const auto &list = skeletonData();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() == id)
            return m.value(QStringLiteral("content")).toString();
    }
    return QString();
}

QString JsonUtils::suggestSkeletonForKey(const QString &key)
{
    if (key.trimmed().isEmpty())
        return QString();

    const auto &list = skeletonData();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        const QString pattern = m.value(QStringLiteral("keyPattern")).toString();
        if (pattern.isEmpty())
            continue;
        QRegularExpression re(pattern);
        if (re.match(key).hasMatch())
            return m.value(QStringLiteral("id")).toString();
    }
    return QString();
}
