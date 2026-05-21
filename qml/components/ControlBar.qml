import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

RowLayout {
    property var theme
    property int materialTheme
    property var proxyModel
    property bool autoRefreshRunning: false

    signal addRequested()
    signal refreshRequested()
    signal autoRefreshToggled()
    signal sortRequested()
    signal exportRequested()
    signal importRequested()
    signal clearAllRequested()
    signal columnFamilyChanged(string cf)
    signal filterTextChanged(string text)
    signal pageSizeChanged(int size)
    signal previousPageRequested()
    signal nextPageRequested()

    function focusSearchField() {
        searchField.forceActiveFocus();
        searchField.selectAll();
    }

    property string searchText: searchField.text

    property int currentPage: 0
    property int totalPages: 1
    property int pageSizeValue: 1000

    spacing: 12

    ComboBox {
        id: cfCombo
        Layout.preferredWidth: 220
        model: Backend.columnFamilies
        currentIndex: model.indexOf(Backend.currentColumnFamily)
        onActivated: columnFamilyChanged(currentText)
        Material.theme: materialTheme
        topPadding: 8
        bottomPadding: 8
        leftPadding: 8
        rightPadding: 8
        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            color: theme.surface
            border.color: theme.foreground
            radius: 2
        }
        contentItem: Text {
            text: cfCombo.displayText
            font: cfCombo.font
            color: theme.foreground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Canvas {
            id: canvas
            x: cfCombo.width - width - cfCombo.rightPadding
            y: cfCombo.topPadding + (cfCombo.availableHeight - height) / 2
            width: 12
            height: 8
            contextType: "2d"

            Connections {
                target: theme
                function onForegroundChanged() { canvas.requestPaint() }
            }

            onPaint: {
                context.reset();
                context.moveTo(0, 0);
                context.lineTo(width, 0);
                context.lineTo(width / 2, height);
                context.closePath();
                context.fillStyle = theme.foreground;
                context.fill();
            }
        }
        delegate: ItemDelegate {
            width: cfCombo.width
            contentItem: Text {
                text: modelData
                font: cfCombo.font
                color: highlighted ? theme.primaryText : theme.foreground
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            highlighted: cfCombo.highlightedIndex === index
            background: Rectangle {
                color: highlighted ? theme.primary : "transparent"
            }
        }
        popup: Popup {
            y: cfCombo.height - 1
            width: cfCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1
            Material.theme: materialTheme

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: cfCombo.popup.visible ? cfCombo.delegateModel : null
                currentIndex: cfCombo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                color: theme.surface
                border.color: theme.divider
                radius: 2
            }
        }
    }

    TextField {
        id: searchField
        Layout.fillWidth: true
        Layout.minimumHeight: 48
        placeholderText: qsTr("Search by key or value...")
        onTextChanged: debounceTimer.restart()
        color: theme.foreground
        placeholderTextColor: theme.secondaryText
        verticalAlignment: Text.AlignVCenter
        topPadding: 12
        bottomPadding: 12
        leftPadding: 12
        rightPadding: 12
        background: Rectangle {
            implicitWidth: 200
            implicitHeight: 48
            color: theme.surface
            border.color: searchField.activeFocus ? theme.primary : theme.foreground
            radius: 2
        }

        Timer {
            id: debounceTimer
            interval: 300
            onTriggered: function() { filterTextChanged(searchField.text) }
        }
    }

    Label {
        text: proxyModel.rowCount() + qsTr(" entries")
        font.bold: true
        color: theme.foreground
    }

    // Action buttons
    RowLayout {
        spacing: 4

        Button {
            text: qsTr("Add New")
            flat: true
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Add new entry")
            ToolTip.visible: hovered
            onClicked: addRequested()
        }

        Button {
            text: qsTr("Refresh")
            flat: true
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Refresh data")
            ToolTip.visible: hovered
            onClicked: refreshRequested()
        }

        Button {
            text: autoRefreshRunning ? qsTr("Auto-Refresh OFF") : qsTr("Auto-Refresh ON")
            flat: true
            enabled: Backend.connected
            Material.foreground: enabled ? (autoRefreshRunning ? theme.success : theme.foreground) : theme.disabled
            ToolTip.text: qsTr("Toggle auto-refresh")
            ToolTip.visible: hovered
            onClicked: autoRefreshToggled()
        }

        Button {
            text: qsTr("Sort")
            flat: true
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Toggle sort order")
            ToolTip.visible: hovered
            onClicked: sortRequested()
        }

        Button {
            text: qsTr("Export")
            flat: true
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Export data to JSON")
            ToolTip.visible: hovered
            onClicked: exportRequested()
        }

        Button {
            text: qsTr("Import")
            flat: true
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Import data from JSON")
            ToolTip.visible: hovered
            onClicked: importRequested()
        }

        Button {
            text: qsTr("Clear All")
            flat: true
            enabled: Backend.connected
            Material.foreground: enabled ? theme.error : theme.disabled
            ToolTip.text: qsTr("Clear all data")
            ToolTip.visible: hovered
            onClicked: clearAllRequested()
        }
    }

    RowLayout {
        spacing: 4
        Button {
            text: "◀"
            flat: true
            enabled: currentPage > 0
            Material.foreground: enabled ? theme.foreground : theme.disabled
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: parent.enabled ? theme.foreground : theme.disabled
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: previousPageRequested()
        }
        Label {
            text: (currentPage + 1) + " / " + totalPages
            color: theme.foreground
            font.bold: true
        }
        Button {
            text: "▶"
            flat: true
            enabled: currentPage < totalPages - 1
            Material.foreground: enabled ? theme.foreground : theme.disabled
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: parent.enabled ? theme.foreground : theme.disabled
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: nextPageRequested()
        }
    }

    ComboBox {
        id: pageSizeCombo
        Layout.preferredWidth: 100
        model: [100, 500, 1000, 5000]
        currentIndex: model.indexOf(pageSizeValue)
        onActivated: function(index) { pageSizeChanged(model[index]); }
        Material.theme: materialTheme
        topPadding: 8
        bottomPadding: 8
        leftPadding: 8
        rightPadding: 8
        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            color: theme.surface
            border.color: theme.foreground
            radius: 2
        }
        contentItem: Text {
            text: pageSizeCombo.displayText
            font: pageSizeCombo.font
            color: theme.foreground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Canvas {
            id: pageSizeCanvas
            x: pageSizeCombo.width - width - pageSizeCombo.rightPadding
            y: pageSizeCombo.topPadding + (pageSizeCombo.availableHeight - height) / 2
            width: 12
            height: 8
            contextType: "2d"

            Connections {
                target: theme
                function onForegroundChanged() { pageSizeCanvas.requestPaint() }
            }

            onPaint: {
                context.reset();
                context.moveTo(0, 0);
                context.lineTo(width, 0);
                context.lineTo(width / 2, height);
                context.closePath();
                context.fillStyle = theme.foreground;
                context.fill();
            }
        }
        delegate: ItemDelegate {
            width: pageSizeCombo.width
            contentItem: Text {
                text: modelData
                font: pageSizeCombo.font
                color: highlighted ? theme.primaryText : theme.foreground
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            highlighted: pageSizeCombo.highlightedIndex === index
            background: Rectangle {
                color: highlighted ? theme.primary : "transparent"
            }
        }
        popup: Popup {
            y: pageSizeCombo.height - 1
            width: pageSizeCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1
            Material.theme: materialTheme

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: pageSizeCombo.popup.visible ? pageSizeCombo.delegateModel : null
                currentIndex: pageSizeCombo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                color: theme.surface
                border.color: theme.divider
                radius: 2
            }
        }
    }
}
