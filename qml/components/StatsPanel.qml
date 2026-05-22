import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    property var theme
    property var stats: ({})

    signal refreshRequested()

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
            text: qsTr("Entries: %1").arg(stats.total_count || 0)
            color: theme.foreground
            font.bold: true
        }

        Label {
            text: qsTr("Column Families: %1").arg(stats.column_family_count || 0)
            color: theme.foreground
        }

        Label {
            text: {
                var bytes = stats.disk_usage_bytes || 0;
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
            onClicked: refreshRequested()
        }
    }
}
