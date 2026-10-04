#include "JsonUtils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include <QRegularExpression>

/**
 * @brief JsonUtilsを構築する
 *
 * @param parent 親QObject
 */
JsonUtils::JsonUtils(QObject *parent)
    : QObject(parent)
{
}

/**
 * @brief JSON文字列をインデント付きで整形して返す
 *
 * 無効なJSONの場合は入力をそのまま返す
 *
 * @param json JSON文字列
 * @return 整形後のJSON文字列
 */
QString JsonUtils::formatJson(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) return json;
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

/**
 * @brief JSON文字列を1行へ最小化して返す
 *
 * 無効なJSONの場合は入力をそのまま返す
 *
 * @param json JSON文字列
 * @return 最小化後のJSON文字列
 */
QString JsonUtils::minifyJson(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) return json;
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

/**
 * @brief 文字列が有効なJSONか判定する
 *
 * @param json 判定対象文字列
 * @return 有効なJSONの場合はtrue
 */
bool JsonUtils::isValidJson(const QString &json)
{
    QJsonParseError err;
    QJsonDocument::fromJson(json.toUtf8(), &err);
    return err.error == QJsonParseError::NoError;
}

/**
 * @brief JSON文字列の検証結果メッセージを返す
 *
 * 有効なJSONの場合は[Valid JSON format]を返す
 * 空文字列の場合は空文字列を返す
 * それ以外の場合は[Not in JSON format (will be saved as string)]を返す
 *
 * @param json JSON文字列
 * @return 検証結果メッセージ
 */
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

/**
 * @brief 文字列がJSONらしい形式か判定する
 *
 * 先頭と末尾が[{]と[}]または[[]と[]]で囲まれているかを確認する
 *
 * @param value 判定対象文字列
 * @return JSONらしい形式の場合はtrue
 */
bool JsonUtils::isJsonLike(const QString &value)
{
    QString trimmed = value.trimmed();
    return (trimmed.startsWith('{') && trimmed.endsWith('}')) ||
           (trimmed.startsWith('[') && trimmed.endsWith(']'));
}

/**
 * @brief JSONスケルトンテンプレートの静的データを返す
 *
 * 初回呼び出し時にテンプレート識別子、表示名、内容、キー一致パターンを生成する
 * @return テンプレート情報を保持する静的なQVariantList
 */
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

/**
 * @brief JSONスケルトンテンプレート一覧を返す
 *
 * @return テンプレート情報のQVariantList
 */
QVariantList JsonUtils::skeletonTemplates()
{
    return skeletonData();
}

/**
 * @brief 指定IDのスケルトンテンプレート内容を返す
 *
 * @param id テンプレートID
 * @return テンプレート内容文字列
 * @note 該当がない場合は空文字列を返す
 */
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

/**
 * @brief キーに最も適合するスケルトンテンプレートIDを提案する
 *
 * @param key キー文字列
 * @return 提案するテンプレートID
 * @note 該当がない場合は空文字列を返す
 */
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
