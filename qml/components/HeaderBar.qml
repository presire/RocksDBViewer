import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

RowLayout {
    property var theme
    property int materialTheme

    signal languageChanged(string lang)
    signal themeToggled()

    spacing: 16

    Image {
        source: "qrc:/RocksDBViewerApp/assets/RocksDBViewer@128.png"
        sourceSize.width: 40
        sourceSize.height: 40
    }

    Label {
        text: qsTr("RocksDB Viewer")
        font.pixelSize: 28
        font.bold: true
        color: theme.foreground
    }

    Item { Layout.fillWidth: true }

    // Language switch
    RowLayout {
        spacing: 8
        Button {
            text: "JP"
            flat: true
            highlighted: I18n.currentLanguage === "ja"
            Material.theme: materialTheme
            Material.foreground: theme.foreground
            onClicked: languageChanged("ja")
        }
        Button {
            text: "EN"
            flat: true
            highlighted: I18n.currentLanguage === "en"
            Material.theme: materialTheme
            Material.foreground: theme.foreground
            onClicked: languageChanged("en")
        }
    }

    // Theme switch
    Item {
        id: themeToggle
        implicitWidth: 76
        implicitHeight: 32

        Rectangle {
            id: track
            anchors.fill: parent
            radius: height / 2
            color: theme.isDark ? "#1e293b" : "#e2e8f0"
            border.color: theme.divider
            border.width: 1

            Behavior on color {
                ColorAnimation { duration: 200 }
            }
        }

        Rectangle {
            id: thumb
            width: 28
            height: 28
            radius: 14
            x: theme.isDark ? parent.width - width - 2 : 2
            anchors.verticalCenter: parent.verticalCenter
            color: theme.isDark ? "#60a5fa" : "#fbbf24"
            rotation: theme.isDark ? 360 : 0

            Behavior on x {
                NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
            }

            Behavior on color {
                ColorAnimation { duration: 200 }
            }

            Behavior on rotation {
                NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
            }

            Text {
                anchors.centerIn: parent
                text: theme.isDark ? "\u263E" : "\u263C"
                color: "white"
                font.pixelSize: 14
                font.bold: true
            }
        }

        MouseArea {
            id: themeToggleMouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: themeToggled()
        }

        ToolTip.text: qsTr("Toggle theme")
        ToolTip.visible: themeToggleMouseArea.containsMouse
    }

    // Status
    Label {
        text: Backend.connected ? qsTr("Connected") : qsTr("Not Connected")
        color: Backend.connected ? theme.success : theme.secondaryText
        font.bold: true
    }
}
