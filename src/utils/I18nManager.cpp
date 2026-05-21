#include "I18nManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>

I18nManager::I18nManager(QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

QString I18nManager::currentLanguage() const
{
    return m_currentLanguage;
}

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

QString I18nManager::tr(const QString &text) const
{
    return QObject::tr(text.toUtf8().constData());
}
