#include "SettingsMigration.h"
#include "SettingsKeys.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSize>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QDebug>

/**
 * @brief アプリケーション設定を保存するQSettingsを生成する
 *
 * INI形式の設定ファイルをPresire/RocksDBViewer.confとして標準設定ディレクトリに作成または読み込む
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
 * @return 移行済みの場合はtrue
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
 * @return 移行が成功した場合または既に完了している場合はtrue
 */
bool SettingsMigration::migrateIfNeeded()
{
    if (isMigrationCompleted()) {
        return true;
    }

    try {
        // 複数のアプリケーションインスタンスによる同時移行を直列化する
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QDir().mkpath(configPath + "/Presire");
        QLockFile lock(configPath + "/Presire/RocksDBViewer.migration.lock");
        if (!lock.tryLock(5000)) {
            qWarning() << "Settings migration skipped: could not acquire lock";
            return false;
        }
        // ロックの待機中に別のインスタンスが移行を完了した可能性を確認する
        if (isMigrationCompleted()) {
            return true;
        }

        // 既定の保存先にあるNativeFormatの旧設定を開く
        QSettings oldSettings(QSettings::NativeFormat, QSettings::UserScope, "Presire", "RocksDBViewer");

        QSettings newSettings = SettingsMigration::newSettings();

        // 既知の設定キーをすべてコピーする
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

        // ウィンドウ状態を検証する
        validateWindowState(newSettings);

        // 移行完了を記録する
        newSettings.setValue(SettingsKeys::internalMigrationCompleted, true);
        newSettings.sync();

        if (newSettings.status() != QSettings::NoError) {
            qWarning() << "Settings migration failed: could not write new settings";
            return false;
        }

        // 可能な範囲で旧設定を削除する
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
 * [window/x]、[window/y]が負の値の場合は0に補正する
 * [window/width]、[window/height]が100から現在のシステムの画面解像度(全画面を合わせた仮想デスクトップのサイズ)の範囲外の場合はデフォルト値に補正する
 * 画面情報を取得できない場合(QGuiApplication未生成など)は、フォールバック上限値(3840)を使用する
 *
 * @param settings 検証対象のQSettings
 */
void SettingsMigration::validateWindowState(QSettings &settings)
{
    // 現在のシステムの画面解像度から上限値を決定する
    // 画面情報を取得できない場合はフォールバック値を使用する
    constexpr int fallbackMaxSize = 3840;
    constexpr int minSize = 100;
    constexpr int defaultWidth = 1400;
    constexpr int defaultHeight = 900;

    int maxWidth = fallbackMaxSize;
    int maxHeight = fallbackMaxSize;
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        // 複数ディスプレイ構成では全画面を合わせた仮想デスクトップのサイズを上限とする
        const QSize virtualSize = screen->virtualSize();
        if (virtualSize.width() >= minSize && virtualSize.height() >= minSize) {
            maxWidth = virtualSize.width();
            maxHeight = virtualSize.height();
        }
    }

    // xとyが0以上であることを検証する
    int x = settings.value(SettingsKeys::windowX, 0).toInt();
    int y = settings.value(SettingsKeys::windowY, 0).toInt();
    if (x < 0) settings.setValue(SettingsKeys::windowX, 0);
    if (y < 0) settings.setValue(SettingsKeys::windowY, 0);

    // widthとheightが100から画面解像度の範囲内であることを検証する
    // デフォルト値が画面より大きくならないよう、上限値で制限する
    int width = settings.value(SettingsKeys::windowWidth, defaultWidth).toInt();
    int height = settings.value(SettingsKeys::windowHeight, defaultHeight).toInt();
    if (width < minSize || width > maxWidth) {
        settings.setValue(SettingsKeys::windowWidth, qMin(defaultWidth, maxWidth));
    }
    if (height < minSize || height > maxHeight) {
        settings.setValue(SettingsKeys::windowHeight, qMin(defaultHeight, maxHeight));
    }
}

/**
 * @brief 旧設定の残骸を安全に削除する
 *
 * 再帰削除は行わない
 * Windowsのレジストリ由来でfileName()が空の場合はQSettings::clear()で削除する
 * 新設定ファイルと別パスのファイル由来設定は、その1ファイルのみ削除する
 * Linuxでは旧ファイルと新設定ファイルが同一パスのため削除しない
 * 残存ディレクトリでは既知のファイル名だけをQFile::remove()で削除する
 * シンボリックリンクはリンク自体だけを削除し、リンク先には触れない
 * 非再帰のrmdir()を試み、未知の内容は残す
 * 未知の内容が残っても移行完了フラグを維持するため、次回起動時に再試行しない
 */
void SettingsMigration::removeOldSettings()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString newSettingsFile = configPath + "/Presire/RocksDBViewer.conf";
    QString oldDir = configPath + "/Presire/RocksDBViewer";

    // パス自体がシンボリックリンクの場合は触れず、リンク先にも触れない
    if (QFileInfo(oldDir).isSymLink()) {
        qInfo() << "Old settings path is a symlink, leaving it untouched:" << oldDir;
        return;
    }

    // 旧NativeFormatストアを後始末し、再帰削除は行わない
    QSettings oldSettings(QSettings::NativeFormat, QSettings::UserScope,
                          "Presire", "RocksDBViewer");
    QString oldFileName = oldSettings.fileName();
    if (oldFileName.isEmpty()) {
#ifdef Q_OS_WIN
        oldSettings.clear();
        oldSettings.sync();
#endif
    } else if (QDir::cleanPath(oldFileName) != QDir::cleanPath(newSettingsFile)) {
        // QFile::remove()はシンボリックリンク自体を削除し、リンク先には触れない
        QFile::remove(oldFileName);
    }
    // oldFileNameとnewSettingsFileが同一のLinuxでは新設定を温存する

    // 残存ディレクトリでは既知ファイルのみ削除し、非再帰rmdirを試行する
    QDir dir(oldDir);
    if (!dir.exists()) {
        return;
    }
    // ファイルベースのQSettingsが取り得る唯一の既知ファイル名を指定する
    // 新設定ファイルと同名でもPresire/RocksDBViewer/配下の別パスのため安全に処理できる
    // それ以外の名前やサブディレクトリは未知の内容として残す
    const QStringList knownFiles = { QStringLiteral("RocksDBViewer.conf") };
    const QStringList entries = dir.entryList(
        QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &name : entries) {
        if (!knownFiles.contains(name)) {
            continue;
        }
        const QString filePath = dir.filePath(name);
        const QFileInfo entryInfo(filePath);
        if (entryInfo.isDir() && !entryInfo.isSymLink()) {
            continue;  // ディレクトリには入らない
        }
        if (QDir::cleanPath(filePath) == QDir::cleanPath(newSettingsFile)) {
            continue;  // 新設定ファイルは決して削除しない
        }
        QFile::remove(filePath);
    }
    // 非再帰削除のため空でなければ失敗するが、未知の内容を残すためそのままにする
    if (!QDir().rmdir(oldDir)) {
        qInfo() << "Legacy settings directory left untouched:" << oldDir;
    }
}
