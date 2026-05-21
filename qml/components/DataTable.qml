import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

Rectangle {
    property var theme
    property int materialTheme
    property var proxyModel

    signal editRequested(string key, string value)
    signal deleteRequested(string key)

    color: theme.surface
    radius: 4
    border.color: theme.divider
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        // Table Header
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: theme.primary
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Label {
                    id: keyHeader
                    Layout.preferredWidth: keyColumnWidth
                    text: qsTr("Key")
                    font.bold: true
                    color: theme.primaryText
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Value")
                    font.bold: true
                    color: theme.primaryText
                }
                Label {
                    id: actionsHeader
                    Layout.preferredWidth: actionsColumnWidth
                    text: qsTr("Actions")
                    font.bold: true
                    color: theme.primaryText
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        // Table Body
        ListView {
            id: tableView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: proxyModel
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                width: ListView.view.width
                height: Math.max(44, valueLabel.implicitHeight + 16)
                Material.theme: materialTheme
                color: index % 2 === 0 ? theme.background : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.05)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Label {
                        Layout.preferredWidth: keyColumnWidth
                        text: model.key
                        font.family: "monospace"
                        elide: Text.ElideMiddle
                        color: theme.accent
                    }

                    Label {
                        id: valueLabel
                        Layout.fillWidth: true
                        text: model.displayValue
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        color: theme.foreground
                    }

                    RowLayout {
                        Layout.preferredWidth: actionsColumnWidth
                        spacing: 6
                        Button {
                            text: "📋"
                            flat: true
                            implicitWidth: 36
                            implicitHeight: 36
                            font.pixelSize: 18
                            Material.theme: materialTheme
                            Material.foreground: theme.foreground
                            background: Rectangle {
                                color: "transparent"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: theme.foreground
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                Backend.copyToClipboard(model.key)
                                ToolTip.show(qsTr("Copied!"), 1500)
                            }
                            ToolTip.text: qsTr("Copy key")
                            ToolTip.visible: hovered
                        }
                        Button {
                            text: "✎"
                            flat: true
                            implicitWidth: 36
                            implicitHeight: 36
                            font.pixelSize: 18
                            Material.theme: materialTheme
                            Material.foreground: theme.foreground
                            background: Rectangle {
                                color: "transparent"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: theme.foreground
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: editRequested(model.key, model.value)
                            ToolTip.text: qsTr("Edit")
                            ToolTip.visible: hovered
                        }
                        Button {
                            text: "🗑"
                            flat: true
                            implicitWidth: 36
                            implicitHeight: 36
                            font.pixelSize: 18
                            Material.theme: materialTheme
                            Material.foreground: theme.error
                            background: Rectangle {
                                color: "transparent"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: theme.error
                                renderType: Text.NativeRendering
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: deleteRequested(model.key)
                            ToolTip.text: qsTr("Delete")
                            ToolTip.visible: hovered
                        }
                    }
                }
            }
        }
    }

    property int keyColumnWidth: 280
    property int actionsColumnWidth: 140
}
