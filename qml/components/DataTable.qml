import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

/**
 * @brief エントリ (キーと値) を一覧表示するテーブル
 *
 * proxyModelを受け取ってListViewに表示し、各行のコピー・編集・削除操作を行う
 * 編集と削除はシグナルとして親へ通知し、実際の処理は親 (main.qml) が行う
 * キーのコピーは、Backend.copyToClipboard()を直接呼び出す
 * 読み取り専用モード (Backend.readOnly) の場合は、削除ボタンを無効化する
 * 配色はthemeプロパティ、Materialの明暗はmaterialThemeプロパティ経由で受け取る
 */
Rectangle {
    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** 子コントロールへ適用するMaterial.themeの値 (Material.LightまたはMaterial.Dark) */
    property int materialTheme

    /** 表示対象のモデル (FilterProxyModel) */
    property var proxyModel

    /** キー列の幅 (ヘッダーと各行で共通) */
    property int keyColumnWidth: 280

    /** 操作ボタン列の幅 (ヘッダーと各行で共通) */
    property int actionsColumnWidth: 140

    /**
     * @brief 行の編集 (読み取り専用モードの場合は閲覧)が要求されたことを通知する
     * @param key 対象エントリのキー
     * @param value 対象エントリの値 (整形前の元の値)
     */
    signal editRequested(string key, string value)

    /**
     * @brief 行の削除が要求されたことを通知する
     * @param key 対象エントリのキー
     */
    signal deleteRequested(string key)

    color: theme.surface
    radius: 4
    border.color: theme.divider
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        // テーブルヘッダー (キー・値・操作の列見出し)
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

        // テーブル本体 (行数が多くてもスクロールできるよう、ListViewで表示する)
        ListView {
            id: tableView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: proxyModel
            ScrollBar.vertical: ScrollBar {}

            // 1行分の表示 (値の長さに応じて行の高さが変わる)
            delegate: Rectangle {
                width: ListView.view.width
                height: Math.max(44, valueLabel.implicitHeight + 16)
                Material.theme: materialTheme
                // 偶数行と奇数行で背景色を変え、縞模様にする
                color: index % 2 === 0 ? theme.background : Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.05)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Label {
                        Layout.preferredWidth: keyColumnWidth
                        text: model.key
                        textFormat: Text.PlainText
                        font.family: "monospace"
                        elide: Text.ElideMiddle
                        color: theme.accent
                    }

                    Label {
                        id: valueLabel
                        Layout.fillWidth: true
                        text: model.displayValue
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        color: theme.foreground
                    }

                    RowLayout {
                        Layout.preferredWidth: actionsColumnWidth
                        spacing: 6
                        // キーをクリップボードへコピーするボタン
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
                        // 編集ダイアログを開くボタン
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
                        // 削除を要求するボタン (読み取り専用モードでは無効化し、薄く表示する)
                        Button {
                            text: "🗑"
                            flat: true
                            enabled: !Backend.readOnly
                            opacity: enabled ? 1.0 : 0.3
                            implicitWidth: 36
                            implicitHeight: 36
                            font.pixelSize: 18
                            Material.theme: materialTheme
                            Material.foreground: enabled ? theme.error : theme.disabled
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
}
