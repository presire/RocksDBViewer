import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/**
 * @brief データベースの統計情報を表示するパネル
 *
 * 推定エントリ数・カラムファミリ数・ディスク使用量を横一列に表示する
 * ディスク使用量は、値の大きさに応じてB・KB・MBのいずれかの単位で表示する
 * 統計情報の再取得は、refreshRequestedシグナルとして親へ通知する
 * 配色は、themeプロパティ経由で受け取る
 */
Rectangle {
    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** 表示する統計情報 (total_count・column_family_count・disk_usage_bytesを持つオブジェクト) */
    property var stats: ({})

    /** 統計情報の再取得が要求されたことを通知する */
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

        // エントリ数の推定値
        Label {
            text: qsTr("Entries (est.): %1").arg(stats.total_count || 0)
            color: theme.foreground
            font.bold: true
        }

        // カラムファミリ数
        Label {
            text: qsTr("Column Families: %1").arg(stats.column_family_count || 0)
            color: theme.foreground
        }

        // ディスク使用量(1[KB]未満は[B]、1[MB]未満は[KB]、それ以上は[MB]で表示する)
        Label {
            text: {
                var bytes = stats.disk_usage_bytes || 0;
                if (bytes < 1024) return qsTr("Disk: %1 B").arg(bytes);
                if (bytes < 1024 * 1024) return qsTr("Disk: %1 KB").arg((bytes / 1024).toFixed(1));
                return qsTr("Disk: %1 MB").arg((bytes / 1024 / 1024).toFixed(1));
            }
            color: theme.foreground
        }

        // 統計情報と更新ボタンの間を埋めるスペーサー
        Item { Layout.fillWidth: true }

        // 統計情報の再取得ボタン
        Button {
            text: qsTr("Refresh Stats")
            flat: true
            font.pixelSize: 12
            Material.foreground: theme.accent
            onClicked: refreshRequested()
        }
    }
}
