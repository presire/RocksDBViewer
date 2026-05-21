#pragma once

#include <QObject>
#include <QTranslator>
#include <QQmlEngine>

class I18nManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setCurrentLanguage NOTIFY currentLanguageChanged)

public:
    explicit I18nManager(QQmlEngine *engine, QObject *parent = nullptr);

    QString currentLanguage() const;
    Q_INVOKABLE void setCurrentLanguage(const QString &lang);
    Q_INVOKABLE QString tr(const QString &text) const;

signals:
    void currentLanguageChanged();

private:
    QQmlEngine *m_engine;
    QTranslator m_translator;
    QString m_currentLanguage = "";
};
