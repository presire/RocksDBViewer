import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/**
 * @brief データベース未接続の場合に表示する案内ページ
 *
 * データベースを開く操作を促すメッセージを中央に表示する
 * 配色は、themeプロパティ経由で受け取る
 */
Item {
    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    Column {
        anchors.centerIn: parent
        spacing: 16

        // ロゴ画像 (現在は使用していない)
        // Image {
        //     anchors.horizontalCenter: parent.horizontalCenter
        //     width: 160
        //     height: 160
        //     source: "qrc:/RocksDBViewerApp/assets/RocksDBViewer.png"
        //     fillMode: Image.PreserveAspectFit
        // }

        // 操作案内のメッセージ
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr('Click "Open Database" to select a RocksDB directory.')
            font.pixelSize: 16
            color: theme.secondaryText
        }
    }
}
