#pragma once

/**
 * @namespace SettingsKeys
 * @brief 永続設定で使用するキー文字列を集約する
 */
namespace SettingsKeys {
    inline constexpr char language[] = "language";                                      // 言語コード
    inline constexpr char windowX[] = "window/x";                                       // ウィンドウのX座標
    inline constexpr char windowY[] = "window/y";                                       // ウィンドウのY座標
    inline constexpr char windowWidth[] = "window/width";                               // ウィンドウ幅
    inline constexpr char windowHeight[] = "window/height";                             // ウィンドウ高
    inline constexpr char recentDatabases[] = "recentDatabases";                        // 最近開いたデータベース
    inline constexpr char darkMode[] = "darkMode";                                      // ダークモード設定
    inline constexpr char internalMigrationCompleted[] = "internal/migrationCompleted"; // 内部設定移行完了フラグ
    inline constexpr char customSkeletons[] = "customSkeletons";                        // カスタムJSONスケルトン
    inline constexpr char showHiddenFiles[] = "showHiddenFiles";                        // 非表示ファイル表示設定
}
