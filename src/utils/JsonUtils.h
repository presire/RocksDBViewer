#pragma once

#include <QObject>
#include <QString>
#include <QJsonParseError>

/**
 * @class JsonUtils
 * @brief QMLから利用するJSON文字列の整形、検証、スケルトン生成を提供する
 *
 * JSON文字列の整形と最小化を行い、入力が有効なJSONかどうかを判定する
 * 定義済みのJSONスケルトンを一覧、生成、キー名に応じた提案として公開する
 */
class JsonUtils : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief JsonUtilsを構築する
     * @param parent 親QObject
     */
    explicit JsonUtils(QObject *parent = nullptr);

    /**
     * @brief JSON文字列をインデント付きで整形して返す
     * @param json JSON文字列
     * @return 整形後のJSON文字列
     * @note 無効なJSONの場合は入力文字列をそのまま返す
     */
    Q_INVOKABLE static QString formatJson(const QString &json);

    /**
     * @brief JSON文字列を1行へ最小化して返す
     * @param json JSON文字列
     * @return 最小化後のJSON文字列
     * @note 無効なJSONの場合は入力文字列をそのまま返す
     */
    Q_INVOKABLE static QString minifyJson(const QString &json);

    /**
     * @brief 文字列が有効なJSONか判定する
     * @param json 判定対象文字列
     * @return 有効なJSONの場合はtrue
     */
    Q_INVOKABLE static bool isValidJson(const QString &json);

    /**
     * @brief JSON文字列の検証結果メッセージを返す
     * @param json 検証するJSON文字列
     * @return 検証結果メッセージ
     */
    Q_INVOKABLE static QString validationMessage(const QString &json);

    /**
     * @brief 文字列がJSONらしい形式か判定する
     * @param value 判定対象文字列
     * @return JSONらしい形式の場合はtrue
     */
    Q_INVOKABLE static bool isJsonLike(const QString &value);

    /**
     * @brief JSONスケルトンテンプレート一覧を返す
     * @return テンプレート情報のQVariantList
     */
    Q_INVOKABLE static QVariantList skeletonTemplates();

    /**
     * @brief 指定識別子のJSONスケルトンを生成する
     * @param id テンプレート識別子
     * @return テンプレート内容文字列
     * @note 該当がない場合は空文字列を返す
     */
    Q_INVOKABLE static QString generateSkeleton(const QString &id);

    /**
     * @brief キーに最も適合するJSONスケルトン識別子を提案する
     * @param key キー文字列
     * @return 提案するテンプレート識別子
     * @note 該当がない場合は空文字列を返す
     */
    Q_INVOKABLE static QString suggestSkeletonForKey(const QString &key);
};
