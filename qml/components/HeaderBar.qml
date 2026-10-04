import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

/**
 * @brief アプリケーションのタイトルと、言語・テーマ切り替えを表示するヘッダー
 *
 * 左からアイコン・タイトル・言語切り替え (JP/EN)・テーマ切り替えスイッチ・接続状態を並べる
 * 言語とテーマの切り替えはシグナルとして親へ通知し、実際の処理は親 (main.qml) が行う
 * 現在の言語はI18n.currentLanguage、接続状態はBackend.connectedを参照する
 * 配色はthemeプロパティ、Materialの明暗はmaterialThemeプロパティ経由で受け取る
 */
RowLayout {
    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** 子コントロールへ適用するMaterial.themeの値 (Material.LightまたはMaterial.Dark) */
    property int materialTheme

    /**
     * @brief 表示言語の切り替えが要求されたことを通知する
     * @param lang 切り替え先の言語コード ("ja"または"en")
     */
    signal languageChanged(string lang)

    /** ライト・ダークのテーマ切り替えが要求されたことを通知する */
    signal themeToggled()

    spacing: 16

    // アプリケーションのアイコン
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

    // タイトルと右側のコントロールの間を埋めるスペーサー
    Item { Layout.fillWidth: true }

    // 言語切り替えボタン (現在の言語のボタンを強調表示する)
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

    // テーマ切り替えスイッチ (ライト・ダークでつまみの位置・色・アイコンが切り替わる)
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

    // データベースの接続状態
    Label {
        text: Backend.connected ? qsTr("Connected") : qsTr("Not Connected")
        color: Backend.connected ? theme.success : theme.secondaryText
        font.bold: true
    }
}
