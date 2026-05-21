import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

ApplicationWindow {
    id: root
    visible: true
    width: 1400
    minimumWidth: 1280
    height: 900
    minimumHeight: 640
    title: qsTr("RocksDB Viewer")

    Material.theme: Material.Light
    Material.accent: Material.Blue

    color: theme.background

    Theme {
        id: theme
    }

    Component.onCompleted: {
        if (initialDarkMode) {
            Material.theme = Material.Dark;
            theme.isDark = true;
            root.color = theme.background;
        }
    }

    // Models
    EntryModel {
        id: entryModel
    }

    FilterProxyModel {
        id: proxyModel
        sourceModel: entryModel
    }

    // Pagination state
    property int currentPage: 0
    property int pageSize: 1000
    property int totalEntries: 0

    // Data reload logic
    function reloadData() {
        if (!Backend.connected) return;
        totalEntries = Backend.getTotalEntryCount();
        var searchText = controlBar.searchText || "";
        var data = Backend.getData(searchText, currentPage * pageSize, pageSize);
        entryModel.setEntries(data);
    }

    function goToPage(page) {
        if (page < 0) page = 0;
        var maxPage = Math.ceil(totalEntries / pageSize) - 1;
        if (page > maxPage) page = maxPage;
        currentPage = page;
        reloadData();
    }

    Connections {
        target: Backend
        function onDataChanged() {
            reloadData();
        }
        function onCurrentColumnFamilyChanged() {
            reloadData();
        }
        function onConnectedChanged() {
            if (Backend.connected) {
                reloadData();
            } else {
                entryModel.clear();
            }
        }
        function onToastRequested(message, type) {
            toastManager.show(message, type);
        }
        function onErrorOccurred(message) {
            toastManager.show(message, "error");
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        HeaderBar {
            Layout.fillWidth: true
            theme: theme
            materialTheme: root.Material.theme
            onLanguageChanged: function(lang) { I18n.setCurrentLanguage(lang); }
            onThemeToggled: {
                Material.theme = (Material.theme === Material.Dark) ? Material.Light : Material.Dark;
                theme.isDark = Material.theme === Material.Dark;
                root.color = theme.background;
                Backend.saveDarkMode(theme.isDark);
            }
        }

        StatsPanel {
            Layout.fillWidth: true
            theme: theme
            visible: Backend.connected
        }

        DatabasePanel {
            Layout.fillWidth: true
            theme: theme
            onOpenDatabaseRequested: folderDialog.openDialog()
            onRecentDatabaseSelected: function(path) { Backend.openDatabase(path); }
        }

        // Main content (only when connected)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Backend.connected
            spacing: 12

            ControlBar {
                id: controlBar
                Layout.fillWidth: true
                theme: theme
                materialTheme: root.Material.theme
                proxyModel: proxyModel
                autoRefreshRunning: autoRefreshTimer.running
                currentPage: root.currentPage
                totalPages: Math.ceil(root.totalEntries / root.pageSize)
                pageSizeValue: root.pageSize
                onAddRequested: editDialog.openDialog()
                onRefreshRequested: {
                    reloadData();
                    toastManager.show(qsTr("Data refreshed"), "success");
                }
                onAutoRefreshToggled: {
                    autoRefreshTimer.running = !autoRefreshTimer.running;
                    toastManager.show(autoRefreshTimer.running ? qsTr("Auto-refresh started") : qsTr("Auto-refresh stopped"), "info");
                }
                onSortRequested: {
                    proxyModel.sortOrderState = (proxyModel.sortOrderState + 1) % 3;
                }
                onExportRequested: exportDialog.openDialog()
                onImportRequested: importDialog.openDialog()
                onClearAllRequested: clearConfirmDialog.open()
                onColumnFamilyChanged: function(cf) { Backend.currentColumnFamily = cf }
                onFilterTextChanged: function(text) {
                    proxyModel.filterText = text;
                    root.currentPage = 0;
                    reloadData();
                }
                onPageSizeChanged: function(size) {
                    root.pageSize = size;
                    root.currentPage = 0;
                    reloadData();
                }
                onPreviousPageRequested: goToPage(root.currentPage - 1)
                onNextPageRequested: goToPage(root.currentPage + 1)
            }

            DataTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                theme: theme
                materialTheme: root.Material.theme
                proxyModel: proxyModel
                onEditRequested: function(key, value) { editDialog.openDialog(key, value) }
                onDeleteRequested: function(key) { Backend.deleteData(key) }
            }
        }

        WelcomePage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !Backend.connected
            theme: theme
        }
    }

    FilePickerDialog {
        id: folderDialog
        folderMode: true
        titleText: qsTr("Select RocksDB Directory")
        theme: theme
        materialTheme: root.Material.theme
        onPathSelected: function(path) { Backend.openDatabase(path); }
    }

    FilePickerDialog {
        id: importDialog
        saveMode: false
        titleText: qsTr("Import JSON")
        nameFilters: ["JSON files (*.json)", "All files (*.*)"]
        theme: theme
        materialTheme: root.Material.theme
        onPathSelected: function(path) {
            var data = Backend.readJsonFromFile(path);
            if (Object.keys(data).length > 0) {
                Backend.importData(data);
            }
        }
    }

    FilePickerDialog {
        id: exportDialog
        saveMode: true
        titleText: qsTr("Export JSON")
        nameFilters: ["JSON files (*.json)", "All files (*.*)"]
        theme: theme
        materialTheme: root.Material.theme
        onPathSelected: function(path) {
            if (!path.endsWith(".json")) path += ".json";
            var data = Backend.exportData();
            Backend.writeJsonToFile(path, data);
        }
    }

    Dialog {
        id: clearConfirmDialog
        title: qsTr("Confirm Clear All")
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            text: qsTr("Are you sure you want to delete all data in column family '%1'?").arg(Backend.currentColumnFamily)
            wrapMode: Text.Wrap
        }

        onAccepted: Backend.clearData()
    }

    EditModal {
        id: editDialog
        theme: theme
        materialTheme: root.Material.theme
        onToastRequested: function(message, type) { toastManager.show(message, type) }
    }

    ToastManager {
        id: toastManager
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
    }

    Timer {
        id: autoRefreshTimer
        interval: 10000
        repeat: true
        onTriggered: reloadData()
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: folderDialog.openDialog()
    }
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: {
            reloadData();
            toastManager.show(qsTr("Data refreshed"), "success");
        }
    }
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: editDialog.openDialog()
    }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: controlBar.focusSearchField()
    }
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Ctrl+T"
        onActivated: {
            Material.theme = (Material.theme === Material.Dark) ? Material.Light : Material.Dark;
            theme.isDark = Material.theme === Material.Dark;
            root.color = theme.background;
            Backend.saveDarkMode(theme.isDark);
        }
    }
}
