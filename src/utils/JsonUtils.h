#pragma once

#include <QObject>
#include <QString>
#include <QJsonParseError>

class JsonUtils : public QObject
{
    Q_OBJECT
public:
    explicit JsonUtils(QObject *parent = nullptr);

    Q_INVOKABLE static QString formatJson(const QString &json);
    Q_INVOKABLE static QString minifyJson(const QString &json);
    Q_INVOKABLE static bool isValidJson(const QString &json);
    Q_INVOKABLE static QString validationMessage(const QString &json);
    Q_INVOKABLE static bool isJsonLike(const QString &value);

    // JSON skeleton generation
    Q_INVOKABLE static QVariantList skeletonTemplates();
    Q_INVOKABLE static QString generateSkeleton(const QString &id);
    Q_INVOKABLE static QString suggestSkeletonForKey(const QString &key);
};
