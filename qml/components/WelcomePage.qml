import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    property var theme

    Column {
        anchors.centerIn: parent
        spacing: 16

        // Image {
        //     anchors.horizontalCenter: parent.horizontalCenter
        //     width: 160
        //     height: 160
        //     source: "qrc:/RocksDBViewerApp/assets/RocksDBViewer.png"
        //     fillMode: Image.PreserveAspectFit
        // }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr('Click "Open Database" to select a RocksDB directory.')
            font.pixelSize: 16
            color: theme.secondaryText
        }
    }
}
