#include "SettingsMigration.h"
#include "SettingsKeys.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

/**
 * @brief アプリケーション設定を保存するQSettingsを生成する
 *
 * INI形式の設定ファイルを Presire/RocksDBViewer.conf として、標準設定ディレクトリに作成・読み込みする
 *
 * @return 新規QSettingsインスタンス
 */
QSettings SettingsMigration::newSettings()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString settingsFile = configPath + "/Presire/RocksDBViewer.conf";
    return QSettings(settingsFile, QSettings::IniFormat);
}

/**
 * @brief 設定の移行が完了しているか判定する
 *
 * @return 移行済みの場合は true
 */
bool SettingsMigration::isMigrationCompleted()
{
    QSettings settings = newSettings();
    return settings.contains(SettingsKeys::internalMigrationCompleted);
}

/**
 * @brief 必要に応じて旧設定から新設定への移行を行う
 *
 * 旧NativeFormat設定から既知のキーをINI形式の新設定にコピーし、ウィンドウ状態を検証して移行完了フラグを立てる
 * 移行成功後は旧設定ディレクトリを削除する
 *
 * @return 移行が成功した、または既に完了している場合は true
 */
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

/**
 * @brief ウィンドウ状態の設定値を検証・補正する
 *
 * x, y が負の値の場合は 0 に、width, height が 100～3840 の範囲外の場合はデフォルト値に補正する
 *
 * @param settings 検証対象のQSettings
 */
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

/**
 * @brief 旧設定ディレクトリを削除する
 */
void SettingsMigration::removeOldSettings()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString oldDir = configPath + "/Presire/RocksDBViewer";
    QDir dir(oldDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}
