#pragma once

#include <QObject>
#include <QTranslator>
#include <QQmlEngine>

/**
 * @class I18nManager
 * @brief QMLアプリケーションの実行時言語切り替えを管理する
 *
 * 埋め込みリソース内の翻訳ファイルだけを読み込み、許可した言語コードだけを受け付ける
 * 翻訳の適用後にQQmlEngineを再翻訳して、qsTr()の表示を更新する
 *
 * m_engine : 再翻訳するQMLエンジン
 * m_translator : 現在適用中の翻訳器
 * m_currentLanguage : 現在の言語コード
 */
class I18nManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setCurrentLanguage NOTIFY currentLanguageChanged)

public:
    /**
     * @brief I18nManagerを構築する
     * @param engine 再翻訳を行うQQmlEngine
     * @param parent 親QObject
     */
    explicit I18nManager(QQmlEngine *engine, QObject *parent = nullptr);

    /**
     * @brief 現在設定されている言語コードを返す
     * @return 言語コード(例:[ja]、[en])
     */
    QString currentLanguage() const;

    /**
     * @brief 現在の言語を設定する
     * @param lang 言語コード
     * @note [ja]と[en]以外の言語コードは無視する
     */
    Q_INVOKABLE void setCurrentLanguage(const QString &lang);

    /**
     * @brief 指定文字列を現在の言語へ翻訳する
     * @param text 翻訳対象文字列
     * @return 翻訳後の文字列
     */
    Q_INVOKABLE QString tr(const QString &text) const;

signals:
    /**
     * @brief 現在の言語が変更されたことを通知する
     */
    void currentLanguageChanged();

private:
    // QML再翻訳
    QQmlEngine *m_engine;              // 再翻訳するQMLエンジン
    QTranslator m_translator;          // 現在適用中の翻訳器

    // 言語状態
    QString m_currentLanguage = "";    // 現在の言語コード
};
