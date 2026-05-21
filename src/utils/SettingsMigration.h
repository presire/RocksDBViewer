#pragma once

#include <QObject>
#include <QSettings>

class SettingsMigration : public QObject
{
    Q_OBJECT
public:
    static QSettings newSettings();
    static bool migrateIfNeeded();
    static bool isMigrationCompleted();

private:
    static void validateWindowState(QSettings &settings);
    static void removeOldSettings();
};
