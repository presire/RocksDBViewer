import QtQuick

/**
 * @brief アプリケーション全体の配色定数
 *
 * isDarkの値に応じて、ライト・ダークの配色を切り替える
 * main.qmlでインスタンス化し、各コンポーネントへthemeプロパティとして渡す
 * 各コンポーネントはtheme経由で参照し、色を直接記述しない
 */
QtObject {
    /** ダークテーマが有効かどうか(trueの場合はダーク配色、falseの場合はライト配色) */
    property bool isDark: false

    /** ウィンドウ全体およびテーブル偶数行の背景色(isDarkで切り替わる) */
    readonly property color background: isDark ? "#303030" : "#fafafa"

    /** カード・パネル・ダイアログなど、背景より一段手前の面の色(isDarkで切り替わる) */
    readonly property color surface: isDark ? "#424242" : "#ffffff"

    /** 本文テキストおよび枠線に使う標準の前景色(isDarkで切り替わる) */
    readonly property color foreground: isDark ? "#ffffff" : "#212121"

    /** 主要ボタン・テーブルヘッダーなどに使う基調色(isDarkに依存しない固定色) */
    readonly property color primary: "#1976d2"

    /** primary色の背景上に載せるテキストの色(isDarkに依存しない固定色) */
    readonly property color primaryText: "#ffffff"

    /** キー名・リンク的なボタン・選択行の強調に使うアクセント色(isDarkで切り替わる) */
    readonly property color accent: isDark ? "#448aff" : "#1976d2"

    /** 区切り線・枠線の色(isDarkで切り替わる) */
    readonly property color divider: isDark ? "#616161" : "#e0e0e0"

    /** 補足テキスト・プレースホルダー・未接続表示などの控えめな文字色(isDarkで切り替わる) */
    readonly property color secondaryText: isDark ? "#bdbdbd" : "#757575"

    /** 無効化されたコントロールの文字色(isDarkで切り替わる) */
    readonly property color disabled: isDark ? "#9e9e9e" : "#bdbdbd"

    /** エラー・破壊的操作(削除・全削除)を示す色(isDarkに依存しない固定色) */
    readonly property color error: "#f44336"

    /** 成功・接続中・有効状態を示す色(isDarkに依存しない固定色) */
    readonly property color success: "#4caf50"

    /** 警告・読み取り専用表示を示す色(isDarkに依存しない固定色) */
    readonly property color warning: "#ff9800"

    /** OPTIONSファイル無しで開いた場合の警告バナーの背景色(isDarkで切り替わる) */
    readonly property color warningBackground: isDark ? "#3E2723" : "#FFF8E1"

    /** OPTIONSファイル無しで開いた場合の警告バナーの枠線色(isDarkで切り替わる) */
    readonly property color warningBorder: isDark ? "#FF6F00" : "#FF8F00"

    /** OPTIONSファイル無しで開いた場合の警告バナーの文字色(isDarkで切り替わる) */
    readonly property color warningText: isDark ? "#FFD54F" : "#E65100"
}
