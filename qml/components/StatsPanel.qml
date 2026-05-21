import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

Rectangle {
    property var theme

    color: theme.surface
    radius: 4
    border.color: theme.divider
    border.width: 1
    implicitHeight: statsRow.implicitHeight + 24

    RowLayout {
        id: statsRow
        anchors.fill: parent
        anchors.margins: 12
        spacing: 24

        Label {
            text: qsTr("Entries: %1").arg(Backend.connected ? Backend.getDatabaseStats().total_count : 0)
            color: theme.foreground
            font.bold: true
        }

        Label {
            text: qsTr("Column Families: %1").arg(Backend.connected ? Backend.getDatabaseStats().column_family_count : 0)
            color: theme.foreground
        }

        Label {
            text: {
                if (!Backend.connected) return qsTr("Disk: -");
                var bytes = Backend.getDatabaseStats().disk_usage_bytes || 0;
                if (bytes < 1024) return qsTr("Disk: %1 B").arg(bytes);
                if (bytes < 1024 * 1024) return qsTr("Disk: %1 KB").arg((bytes / 1024).toFixed(1));
                return qsTr("Disk: %1 MB").arg((bytes / 1024 / 1024).toFixed(1));
            }
            color: theme.foreground
        }

        Item { Layout.fillWidth: true }

        Button {
            text: qsTr("Refresh Stats")
            flat: true
            font.pixelSize: 12
            Material.foreground: theme.accent
            visible: Backend.connected
            onClicked: {
                var stats = Backend.getDatabaseStats();
                toastManager.show(qsTr("Stats refreshed"), "info");
            }
        }
    }
}
