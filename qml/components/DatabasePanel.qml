import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

/**
 * @brief データベースのパス表示と、データベースの選択を行うパネル
 *
 * 現在開いているデータベースのパス (Backend.databasePath) と、フォルダ選択を開くボタンを表示する
 * 最近開いたデータベース (Backend.recentDatabases) がある場合は、一覧を折り返して表示する
 * 履歴の消去は、Backend.clearRecent()を直接呼び出す
 * データベースを開く操作はシグナルとして親へ通知し、実際の処理は親 (main.qml) が行う
 * 配色は、themeプロパティ経由で受け取る
 */
GroupBox {
    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** データベースを開くためのフォルダ選択が要求されたことを通知する */
    signal openDatabaseRequested()

    /**
     * @brief 最近開いたデータベースが選択されたことを通知する
     * @param path 選択されたデータベースのパス
     */
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
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
                font.family: "monospace"
                color: theme.foreground
            }

            // フォルダ選択ダイアログを開くボタン
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

        // 最近開いたデータベースの一覧 (履歴がある場合のみ表示する)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: Backend.recentDatabases.length > 0

            Label {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 5
                text: qsTr("Recent:")
                color: theme.secondaryText
                font.pixelSize: 13
            }

            // 履歴が多い場合は折り返す
            // Layout.preferredWidth: 0により、横幅のimplicitサイズが親レイアウトを押し広げるのを防ぐ
            Flow {
                id: recentFlow
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                spacing: 0

                // 履歴の件数は最大10件のため、Repeaterで生成する
                Repeater {
                    model: Backend.recentDatabases

                    Button {
                        width: Math.min(implicitWidth, recentFlow.width)
                        text: modelData
                        flat: true
                        font.pixelSize: 13
                        leftPadding: 6
                        rightPadding: 6
                        topPadding: 4
                        bottomPadding: 4
                        Material.foreground: theme.accent
                        onClicked: recentDatabaseSelected(modelData)

                        // Material既定のbackgroundは、implicitHeightが40のため、行間が広がらないよう軽量な背景に差し替える
                        background: Rectangle {
                            radius: 2
                            color: parent.hovered || parent.pressed
                                   ? Qt.alpha(theme.accent, 0.12) : "transparent"
                        }

                        contentItem: Text {
                            text: parent.text
                            textFormat: Text.PlainText
                            font: parent.font
                            color: theme.accent
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // 履歴を消去するボタン
            Button {
                Layout.alignment: Qt.AlignTop
                text: qsTr("Clear")
                flat: true
                font.pixelSize: 12
                Material.foreground: theme.secondaryText
                onClicked: Backend.clearRecent()
            }
        }
    }
}
