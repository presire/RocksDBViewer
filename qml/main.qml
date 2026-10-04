import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

/**
 * @brief アプリケーションのメインウィンドウ
 *
 * ヘッダー・統計・データベースパネル・コントロールバー・データテーブルを縦に並べ、Backendとの橋渡しを行う
 * データベース未接続の場合はWelcomePage、接続中の場合はControlBarとDataTableを表示する
 * ページング・ダイアログ・トースト・ショートカットなど、画面全体にまたがる状態と処理をここで管理する
 * 配色はThemeのインスタンスで保持し、各コンポーネントへthemeプロパティとして渡す
 */
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

    /** 現在表示しているページ番号(0始まり) */
    property int currentPage: 0

    /** 1ページあたりの表示件数 */
    property int pageSize: 1000

    /** 検索条件に一致する総エントリ数(totalCountCappedがtrueの場合は上限で打ち切られた値) */
    property int totalEntries: 0

    /** 総エントリ数の集計が上限で打ち切られたかどうか */
    property bool totalCountCapped: false

    /** UTF-8として表示できないため、一覧から除外したエントリ数 */
    property int hiddenNonUtf8Count: 0

    /** Backend.getDatabaseStats()で取得した統計情報(total_count・column_family_count・disk_usage_bytes) */
    property var databaseStats: ({})

    /** OPTIONSファイル無しで開いた場合の警告バナーを閉じたかどうか(データベースが変わるとリセットされる) */
    property bool formatWarningDismissed: false

    /** 全削除の確認ダイアログに表示する、削除対象の総エントリ数 */
    property int clearConfirmEntryCount: 0

    /** 全削除の確認ダイアログに表示する、削除対象のうち非UTF-8のエントリ数 */
    property int clearConfirmNonUtf8: 0

    // 起動時にダークモードの保存値を反映する(initialDarkModeはmain.cppのコンテキストプロパティ)
    Component.onCompleted: {
        if (initialDarkMode) {
            Material.theme = Material.Dark;
            theme.isDark = true;
            root.color = theme.background;
        }
    }

    // 配色定数(ライト・ダークの切り替えはisDarkで行う)
    Theme {
        id: theme
    }

    // Backendから取得した全エントリを保持するモデル
    EntryModel {
        id: entryModel
    }

    // 検索・ソートを適用して、テーブルへ渡すプロキシモデル
    FilterProxyModel {
        id: proxyModel
        sourceModel: entryModel
    }

    /**
     * @brief 現在のページ分のエントリをBackendから取得し、ページング状態を更新する
     *
     * 総件数の集計上限は、10000件と「(現在のページ+2)ページ分」のうち大きい方とする
     * 取得結果に応じて、totalEntries・totalCountCapped・hiddenNonUtf8Countを更新する
     * @param searchText 検索文字列(空文字の場合は全件が対象)
     * @return Backend.getPage()の戻り値(entries・total・capped・hiddenNonUtf8を持つオブジェクト)
     */
    function fetchPage(searchText) {
        // 総件数の集計上限(次ページの有無を判定できるよう、現在ページより先も数える)
        var cap = Math.max(10000, (currentPage + 2) * pageSize);
        var res = Backend.getPage(searchText, currentPage * pageSize, pageSize, cap);
        totalEntries = res.total;
        totalCountCapped = res.capped;
        hiddenNonUtf8Count = res.hiddenNonUtf8;
        return res;
    }

    /**
     * @brief 現在のページのデータを再取得し、モデルへ反映する
     *
     * 未接続の場合は何もしない
     * 削除などで総ページ数が減り、現在のページが範囲外になった場合は、最終ページへ補正して再取得する
     */
    function reloadData() {
        if (!Backend.connected) return;
        var searchText = controlBar.searchText || "";
        var res = fetchPage(searchText);
        var maxPage = Math.max(0, Math.ceil(totalEntries / pageSize) - 1);
        // 現在のページが範囲外になった場合は、最終ページへ補正して再取得する
        if (currentPage > maxPage) {
            currentPage = maxPage;
            res = fetchPage(searchText);
        }
        entryModel.setEntries(res.entries);
    }

    /**
     * @brief データベースの統計情報を再取得する
     *
     * 未接続の場合は、空のオブジェクトで初期化する
     */
    function reloadStats() {
        databaseStats = Backend.connected ? Backend.getDatabaseStats() : ({});
    }

    /**
     * @brief 指定したページへ移動し、データを再取得する
     * @param page 移動先のページ番号(0始まり)
     * @note 範囲外の値は、先頭ページまたは最終ページへ丸められる
     */
    function goToPage(page) {
        if (page < 0) page = 0;
        var maxPage = Math.max(0, Math.ceil(totalEntries / pageSize) - 1);
        if (page > maxPage) page = maxPage;
        currentPage = page;
        reloadData();
    }

    // Backendの状態変化・通知を受け取り、画面へ反映する
    Connections {
        target: Backend
        function onDataChanged() {
            reloadData();
            reloadStats();
        }
        function onCurrentColumnFamilyChanged() {
            root.currentPage = 0;
            reloadData();
            reloadStats();
        }
        function onConnectedChanged() {
            if (Backend.connected) {
                reloadData();
                reloadStats();
            } else {
                entryModel.clear();
                totalEntries = 0;
                totalCountCapped = false;
                hiddenNonUtf8Count = 0;
                databaseStats = ({});
            }
        }
        function onToastRequested(message, type) {
            toastManager.show(message, type);
        }
        function onErrorOccurred(message) {
            toastManager.show(message, "error");
        }
        // 開いているデータベースが変わった場合は、警告バナーの非表示状態を戻す
        function onDatabasePathChanged() {
            root.formatWarningDismissed = false;
        }
        function onOpenedWithDefaultsChanged() {
            // 警告状態が切り替わるたびに、非表示状態を戻す
            root.formatWarningDismissed = false;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // タイトル・言語切り替え・テーマ切り替え・接続状態を表示するヘッダー
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

        // データベースの統計情報(接続中のみ表示)
        StatsPanel {
            Layout.fillWidth: true
            theme: theme
            stats: root.databaseStats
            visible: Backend.connected
            onRefreshRequested: {
                reloadStats();
                toastManager.show(qsTr("Stats refreshed"), "info");
            }
        }

        // データベースのパス表示、フォルダ選択、最近開いたデータベースの一覧
        DatabasePanel {
            Layout.fillWidth: true
            theme: theme
            onOpenDatabaseRequested: folderDialog.openDialog()
            onRecentDatabaseSelected: function(path) { Backend.openDatabase(path); }
        }

        // 接続中のみ表示するメイン領域(コントロールバーとデータテーブル)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Backend.connected
            spacing: 12

            // OPTIONSファイル無しで開いた場合の警告バナー
            // この状態で編集すると、古いRocksDBツールで読めないSSTファイルが作られる可能性がある
            Rectangle {
                id: formatWarningBanner
                Layout.fillWidth: true
                visible: Backend.openedWithDefaults && !root.formatWarningDismissed
                color: theme.warningBackground
                border.color: theme.warningBorder
                border.width: 1
                radius: 4
                implicitHeight: warningRow.implicitHeight + 16

                RowLayout {
                    id: warningRow
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text {
                        text: "\u26A0 " + qsTr("Database opened with default options. Edits may not be readable by older RocksDB tools.")
                        color: theme.warningText
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }

                    Button {
                        text: qsTr("Dismiss")
                        flat: true
                        onClicked: root.formatWarningDismissed = true
                    }
                }
            }

            // 検索・ソート・ページング・データ操作ボタン
            ControlBar {
                id: controlBar
                Layout.fillWidth: true
                theme: theme
                materialTheme: root.Material.theme
                proxyModel: proxyModel
                autoRefreshRunning: autoRefreshTimer.running
                currentPage: root.currentPage
                totalPages: Math.max(1, Math.ceil(root.totalEntries / root.pageSize))
                pageSizeValue: root.pageSize
                totalCountCapped: root.totalCountCapped
                hiddenNonUtf8Count: root.hiddenNonUtf8Count
                onAddRequested: editDialog.openDialog()
                onRefreshRequested: {
                    reloadData();
                    toastManager.show(qsTr("Data refreshed"), "success");
                }
                onAutoRefreshToggled: {
                    autoRefreshTimer.running = !autoRefreshTimer.running;
                    toastManager.show(autoRefreshTimer.running ? qsTr("Auto-refresh started") : qsTr("Auto-refresh stopped"), "info");
                }
                // ソート順を、元の順序・昇順・降順の3段階で順に切り替える
                onSortRequested: {
                    proxyModel.sortOrderState = (proxyModel.sortOrderState + 1) % 3;
                }
                onExportRequested: exportDialog.openDialog()
                onImportRequested: importDialog.openDialog()
                // 確認ダイアログに表示する件数を取得してから、ダイアログを開く
                onClearAllRequested: {
                    var counts = Backend.getEntryCounts();
                    root.clearConfirmEntryCount = counts.total;
                    root.clearConfirmNonUtf8 = counts.nonUtf8;
                    clearConfirmDialog.open();
                }
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

            // エントリ一覧(編集・削除は確認ダイアログを経由して実行する)
            DataTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                theme: theme
                materialTheme: root.Material.theme
                proxyModel: proxyModel
                onEditRequested: function(key, value) { editDialog.openDialog(key, value) }
                onDeleteRequested: function(key) {
                    deleteConfirmDialog.pendingKey = key;
                    deleteConfirmDialog.open();
                }
            }
        }

        // 未接続時の案内表示
        WelcomePage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !Backend.connected
            theme: theme
        }
    }

    // データベースのフォルダ選択ダイアログ
    FilePickerDialog {
        id: folderDialog
        folderMode: true
        titleText: qsTr("Select RocksDB Directory")
        theme: theme
        materialTheme: root.Material.theme
        onPathSelected: function(path) { Backend.openDatabase(path); }
    }

    // インポートするJSONファイルの選択ダイアログ
    FilePickerDialog {
        id: importDialog
        saveMode: false
        titleText: qsTr("Import JSON")
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        theme: theme
        materialTheme: root.Material.theme
        onPathSelected: function(path) { Backend.importFromFile(path); }
    }

    // エクスポート先のJSONファイルの選択ダイアログ(既存ファイルの上書き確認を含む)
    FilePickerDialog {
        id: exportDialog
        saveMode: true
        titleText: qsTr("Export JSON")
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        defaultSuffix: "json"
        theme: theme
        materialTheme: root.Material.theme
        onPathSelected: function(path, overwriteApproved) { Backend.exportToFile(path, overwriteApproved); }
    }

    // 全削除の確認ダイアログ(非UTF-8のエントリが含まれる場合は、その件数も表示する)
    Dialog {
        id: clearConfirmDialog
        title: qsTr("Confirm Clear All")
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            text: root.clearConfirmNonUtf8 > 0
                  ? qsTr("Are you sure you want to delete all %1 entries in column family '%3'? %2 of them are non-UTF-8 entries that are not shown in the table.").arg(root.clearConfirmEntryCount).arg(root.clearConfirmNonUtf8).arg(Backend.currentColumnFamily)
                  : qsTr("Are you sure you want to delete all %1 entries in column family '%2'?").arg(root.clearConfirmEntryCount).arg(Backend.currentColumnFamily)
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }

        onAccepted: Backend.clearData()
    }

    // 1件削除の確認ダイアログ
    Dialog {
        id: deleteConfirmDialog

        /** 削除確認中のエントリのキー */
        property string pendingKey: ""
        title: qsTr("Confirm Delete")
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            text: qsTr("Are you sure you want to delete key '%1'?").arg(deleteConfirmDialog.pendingKey)
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }

        onAccepted: Backend.deleteData(pendingKey)
    }

    // エントリの追加・編集・閲覧ダイアログ
    EditModal {
        id: editDialog
        theme: theme
        materialTheme: root.Material.theme
        onToastRequested: function(message, type) { toastManager.show(message, type) }
    }

    // 画面右上に表示するトースト通知
    ToastManager {
        id: toastManager
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
    }

    // 自動更新タイマー(10秒間隔で現在のページを再取得する)
    Timer {
        id: autoRefreshTimer
        interval: 10000
        repeat: true
        onTriggered: reloadData()
    }

    // キーボードショートカット
    // [Ctrl+O] データベースを開く
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: folderDialog.openDialog()
    }
    // [Ctrl+R] データを再読み込みする
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: {
            reloadData();
            toastManager.show(qsTr("Data refreshed"), "success");
        }
    }
    // [Ctrl+N] 新規エントリを追加する(接続中かつ編集モードの場合のみ有効)
    Shortcut {
        sequence: "Ctrl+N"
        enabled: Backend.connected && !Backend.readOnly
        onActivated: editDialog.openDialog()
    }
    // [Ctrl+F] 検索欄へフォーカスする
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: controlBar.focusSearchField()
    }
    // [Ctrl+Q] アプリケーションを終了する
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
    // [Ctrl+T] ライト・ダークのテーマを切り替え、設定を保存する
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
