#include "SettingsMigration.h"
#include "SettingsKeys.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

QSettings SettingsMigration::newSettings()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString settingsFile = configPath + "/Presire/RocksDBViewer.conf";
    return QSettings(settingsFile, QSettings::IniFormat);
}

bool SettingsMigration::isMigrationCompleted()
{
    QSettings settings = newSettings();
    return settings.contains(SettingsKeys::internalMigrationCompleted);
}

bool SettingsMigration::migrateIfNeeded()
{
    if (isMigrationCompleted()) {
        return true;
    }

    try {
        // Open old settings (NativeFormat - default directory)
        QSettings oldSettings(QSettings::NativeFormat, QSettings::UserScope,
                              "Presire", "RocksDBViewer");

        QSettings newSettings = SettingsMigration::newSettings();

        // Copy all known keys
        const char* keys[] = {
            SettingsKeys::language,
            SettingsKeys::windowX,
            SettingsKeys::windowY,
            SettingsKeys::windowWidth,
            SettingsKeys::windowHeight,
            SettingsKeys::recentDatabases
        };

        for (const char* key : keys) {
            if (oldSettings.contains(key)) {
                newSettings.setValue(key, oldSettings.value(key));
            }
        }

        // Validate window state
        validateWindowState(newSettings);

        // Mark migration as completed
        newSettings.setValue(SettingsKeys::internalMigrationCompleted, true);
        newSettings.sync();

        // Remove old settings (best effort)
        removeOldSettings();

        return true;
    } catch (...) {
        qWarning() << "Settings migration failed, using defaults";
        return false;
    }
}

void SettingsMigration::validateWindowState(QSettings &settings)
{
    // Validate x, y >= 0
    int x = settings.value(SettingsKeys::windowX, 0).toInt();
    int y = settings.value(SettingsKeys::windowY, 0).toInt();
    if (x < 0) settings.setValue(SettingsKeys::windowX, 0);
    if (y < 0) settings.setValue(SettingsKeys::windowY, 0);

    // Validate width, height: 100-3840
    int width = settings.value(SettingsKeys::windowWidth, 1400).toInt();
    int height = settings.value(SettingsKeys::windowHeight, 900).toInt();
    if (width < 100 || width > 3840) settings.setValue(SettingsKeys::windowWidth, 1400);
    if (height < 100 || height > 3840) settings.setValue(SettingsKeys::windowHeight, 900);
}

void SettingsMigration::removeOldSettings()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString oldDir = configPath + "/Presire/RocksDBViewer";
    QDir dir(oldDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}
