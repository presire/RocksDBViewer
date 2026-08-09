import QtQuick

QtObject {
    property bool isDark: false

    readonly property color background: isDark ? "#303030" : "#fafafa"
    readonly property color surface: isDark ? "#424242" : "#ffffff"
    readonly property color foreground: isDark ? "#ffffff" : "#212121"
    readonly property color primary: "#1976d2"
    readonly property color primaryText: "#ffffff"
    readonly property color accent: isDark ? "#448aff" : "#1976d2"
    readonly property color divider: isDark ? "#616161" : "#e0e0e0"
    readonly property color secondaryText: isDark ? "#bdbdbd" : "#757575"
    readonly property color disabled: isDark ? "#9e9e9e" : "#bdbdbd"
    readonly property color error: "#f44336"
    readonly property color success: "#4caf50"
    readonly property color warning: "#ff9800"
    // Issue #2 fallback banner colors (light/dark aware)
    readonly property color warningBackground: isDark ? "#3E2723" : "#FFF8E1"
    readonly property color warningBorder: isDark ? "#FF6F00" : "#FF8F00"
    readonly property color warningText: isDark ? "#FFD54F" : "#E65100"
}
