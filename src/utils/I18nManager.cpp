#include "I18nManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>

/**
 * @brief I18nManagerを構築する
 *
 * @param engine 再翻訳を行う対象のQQmlEngine
 * @param parent 親QObject
 */
I18nManager::I18nManager(QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

/**
 * @brief 現在設定されている言語コードを返す
 *
 * @return 言語コード (例: "ja", "en")
 */
QString I18nManager::currentLanguage() const
{
    return m_currentLanguage;
}

/**
 * @brief 現在の言語を設定する
 *
 * まず、実行ファイルと同じディレクトリのファイルシステムパスから翻訳ファイルを読み込み、失敗した場合はリソース内のパスをフォールバックとして使用する
 * 読み込みに成功した場合は翻訳をインストールし、QMLエンジンを再翻訳する
 *
 * @param lang 言語コード
 */
void I18nManager::setCurrentLanguage(const QString &lang)
{
    if (m_currentLanguage == lang) return;

    QCoreApplication::removeTranslator(&m_translator);

    // Try filesystem path first (build directory)
    QString fsPath = QDir(QCoreApplication::applicationDirPath()).filePath(
        QString("rocksdbviewer_%1.qm").arg(lang));

    bool ok = m_translator.load(fsPath);

    if (!ok) {
        // Fallback: try resource path
        QString resPath = QString(":/i18n/rocksdbviewer_%1.qm").arg(lang);
        ok = m_translator.load(resPath);
    }

    if (ok) {
        QCoreApplication::installTranslator(&m_translator);
        m_currentLanguage = lang;
        m_engine->retranslate();
        emit currentLanguageChanged();
    } else {
        qWarning() << "Failed to load translator for language:" << lang;
    }
}

/**
 * @brief 指定文字列を翻訳する
 *
 * @param text 翻訳対象文字列
 * @return 翻訳後の文字列
 */
QString I18nManager::tr(const QString &text) const
{
    return QObject::tr(text.toUtf8().constData());
}
