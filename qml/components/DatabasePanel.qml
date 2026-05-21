import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

GroupBox {
    property var theme

    signal openDatabaseRequested()
    signal recentDatabaseSelected(string path)

    padding: 12
    topPadding: 36
    label: Label {
        x: parent.padding
        y: 8
        text: qsTr("Database Path")
        color: theme.foreground
        font.bold: true
    }
    background: Rectangle {
        color: "transparent"
        border.color: theme.divider
        radius: 2
    }

    ColumnLayout {
        width: parent.width
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: Backend.databasePath || qsTr("No database selected")
                elide: Text.ElideMiddle
                font.family: "monospace"
                color: theme.foreground
            }

            Button {
                text: qsTr("Open Database")
                Material.theme: Material.Light
                Material.foreground: theme.primaryText
                onClicked: openDatabaseRequested()
                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 40
                    color: parent.pressed ? Qt.darker(theme.primary, 1.2) :
                           parent.hovered ? Qt.lighter(theme.primary, 1.1) : theme.primary
                    radius: 2
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: Backend.recentDatabases.length > 0

            Label {
                text: qsTr("Recent:")
                color: theme.secondaryText
                font.pixelSize: 12
            }

            Repeater {
                model: Backend.recentDatabases

                Button {
                    text: modelData
                    flat: true
                    font.pixelSize: 12
                    Material.foreground: theme.accent
                    onClicked: recentDatabaseSelected(modelData)
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Clear")
                flat: true
                font.pixelSize: 12
                Material.foreground: theme.secondaryText
                onClicked: Backend.clearRecent()
            }
        }
    }
}
