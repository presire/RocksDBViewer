#pragma once

#include <QObject>
#include <QSettings>

/**
 * @class SettingsMigration
 * @brief 旧NativeFormat設定をINI形式の設定へ安全に移行する
 *
 * 既知の設定キーだけを新しい設定へコピーする
 * ウィンドウ状態を安全な範囲に補正し、移行後に旧設定を可能な範囲で削除する
 */
class SettingsMigration : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief アプリケーション設定を保存するQSettingsを生成する
     * @return INI形式のQSettingsインスタンス
     */
    static QSettings newSettings();

    /**
     * @brief 必要に応じて旧設定から新設定へ移行する
     * @return 移行が成功した場合または既に完了している場合はtrue
     */
    static bool migrateIfNeeded();

    /**
     * @brief 設定の移行が完了しているか判定する
     * @return 移行済みの場合はtrue
     */
    static bool isMigrationCompleted();

private:
    /**
     * @brief ウィンドウ状態の設定値を検証して補正する
     *
     * ウィンドウサイズの上限は現在のシステムの画面解像度から決定する
     * @param settings 検証対象のQSettings
     */
    static void validateWindowState(QSettings &settings);

    /**
     * @brief 旧設定の残骸を安全に削除する
     */
    static void removeOldSettings();
};
