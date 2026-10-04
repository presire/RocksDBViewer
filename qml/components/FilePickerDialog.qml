import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

/**
 * @brief フォルダ・ファイルの選択と、保存先の指定を行うカスタムダイアログ
 *
 * folderModeがtrueの場合はフォルダ選択、falseの場合はファイル選択となる
 * saveModeがtrueの場合はファイル名の入力欄を表示し、既存ファイルの上書き前に確認ダイアログを表示する
 * 一覧はFileSystemModelで表示し、キーボード操作とマウス操作の両方に対応する
 * 一覧上で文字を入力すると、先頭一致で項目を検索する (searchBuffer)
 * 選択結果は、pathSelectedシグナルで親へ通知する
 * 配色はthemeプロパティ、Materialの明暗はmaterialThemeプロパティ経由で受け取る
 *
 * キーボード操作
 * - [Return] 選択中の項目を開く(フォルダへ移動、またはファイルを確定)
 * - [Esc] 入力欄・検索文字列を順にクリアし、何も無ければダイアログを閉じる
 * - [Alt+Left] 親フォルダへ移動する
 * - [Home][End][PageUp][PageDown] 一覧内を移動する
 */
Dialog {
    id: dialogRoot
    modal: true
    anchors.centerIn: parent
    width: Math.min(800, parent.width * 0.8)
    height: Math.min(900, parent.height * 0.9)
    implicitHeight: height
    contentHeight: height - (header ? header.implicitHeight : 0)
    padding: 0
    closePolicy: Popup.CloseOnPressOutsideParent
    Material.theme: dialogRoot.materialTheme

    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** 子コントロールへ適用するMaterial.themeの値 (Material.LightまたはMaterial.Dark) */
    property int materialTheme

    /** フォルダ選択モードかどうか (trueの場合はフォルダのみ選択でき、ファイルは表示しない) */
    property bool folderMode: false

    /** 保存モードかどうか (trueの場合はファイル名の入力欄を表示し、既存ファイルの上書きを確認する) */
    property bool saveMode: false

    /** ファイル種別の絞り込み候補 (例: "JSON files (*.json)"、括弧内のパターンを絞り込みに使う) */
    property var nameFilters: ["*"]

    /** 保存時にファイル名へ補う拡張子 (ドットを含まない、拡張子が既にある場合は補わない) */
    property string defaultSuffix: ""

    /** ダイアログのタイトルとして表示する文字列 */
    property string titleText: qsTr("Select")

    /** 一覧で選択中のパス (未選択の場合は空文字) */
    property string selectedPath: ""

    /** 上書き確認中の保存先パス */
    property string pendingSavePath: ""

    /** 一覧上で入力された、先頭一致検索の文字列 (空でない間は右上に表示する) */
    property string searchBuffer: ""

    /**
     * @brief パスが選択されたことを通知する
     * @param path 選択されたフォルダまたはファイルのパス
     * @param overwriteApproved 既存ファイルの上書きをユーザが確認済みの場合のみtrue
     */
    signal pathSelected(string path, bool overwriteApproved)

    /**
     * @brief 選択状態と検索文字列を初期化して、ダイアログを開く
     *
     * 保存モードの場合は、ファイル名の入力欄も空にする
     */
    function openDialog() {
        selectedPath = "";
        searchBuffer = "";
        if (saveMode) {
            fileNameField.text = "";
        }
        open();
    }

    /**
     * @brief searchBufferに先頭一致する項目へ、一覧の選択を移す
     *
     * 一致する項目が無い場合は、選択を変更しない
     */
    function performSearch() {
        var idx = fsModel.findIndexByPrefix(searchBuffer);
        if (idx >= 0) {
            fileList.currentIndex = idx;
            fileList.positionViewAtIndex(idx, ListView.Center);
        }
    }

    /**
     * @brief キー入力が、検索文字列として使える文字かどうかを判定する
     * @param text キー入力で得られた文字列
     * @return 空でなく、制御文字を含まない場合はtrue
     */
    function isSearchInput(text) {
        return text.length > 0 && !/[\x00-\x1F\x7F]/.test(text);
    }

    /**
     * @brief 一覧の現在位置の項目を、selectedPathへ反映する
     *
     * フォルダ選択モードの場合は、フォルダのみを反映する
     */
    function syncSelectionToCurrentIndex() {
        var item = fsModel.get(fileList.currentIndex);
        if (item && item.filePath && (!dialogRoot.folderMode || item.isDirectory)) {
            dialogRoot.selectedPath = item.filePath;
        }
    }

    /**
     * @brief 拡張子が無いファイル名へ、defaultSuffixを補う
     * @param name ファイル名
     * @return 拡張子を補ったファイル名 (補う必要が無い場合は、そのままのファイル名)
     */
    function applySuffix(name) {
        if (defaultSuffix.length > 0 && name.lastIndexOf(".") < 0) {
            return name + "." + defaultSuffix;
        }
        return name;
    }

    /**
     * @brief 現在のフォルダへ指定したファイル名で保存することを要求する
     *
     * 同名のファイルが既に存在する場合は、上書きの確認ダイアログを表示する
     * 存在しない場合は、pathSelectedを通知してダイアログを閉じる
     * @param fileName 保存するファイル名 (空文字の場合は何もしない)
     */
    function requestSave(fileName) {
        if (fileName.length === 0) {
            return;
        }
        var fullPath = fsModel.currentPath + "/" + applySuffix(fileName);
        if (fsModel.fileExists(fullPath)) {
            dialogRoot.pendingSavePath = fullPath;
            overwriteDialog.open();
        } else {
            dialogRoot.pathSelected(fullPath, false);
            dialogRoot.close();
        }
    }

    /**
     * @brief 一覧の現在位置の項目を開く、または確定する
     *
     * フォルダの場合は、そのフォルダへ移動する
     * ファイルの場合は、保存モードならファイル名を入力欄へ反映して保存を要求し、それ以外ならパスを確定する
     * フォルダ選択モードでは、ファイルは無視する
     */
    function activateCurrentItem() {
        var item = fsModel.get(fileList.currentIndex);
        if (!item) return;
        if (item.isDirectory) {
            fsModel.setPath(item.filePath);
            dialogRoot.selectedPath = "";
            dialogRoot.searchBuffer = "";
        } else {
            if (dialogRoot.folderMode) return;
            if (dialogRoot.saveMode) {
                fileNameField.text = item.fileName;
                requestSave(item.fileName);
                return;
            }
            dialogRoot.selectedPath = item.filePath;
            dialogRoot.pathSelected(item.filePath, false);
            dialogRoot.close();
        }
    }

    /**
     * @brief [Esc]キーの処理を行う
     *
     * パス入力欄、ファイル名入力欄、検索文字列の順に、内容があればクリアする
     * いずれも空の場合は、ダイアログを閉じる
     */
    function handleEscape() {
        if (pathField.activeFocus && pathField.text.length > 0) {
            pathField.text = "";
            return;
        }

        if (fileNameField.activeFocus && fileNameField.text.length > 0) {
            fileNameField.text = "";
            return;
        }

        if (dialogRoot.searchBuffer.length > 0) {
            dialogRoot.searchBuffer = "";
            return;
        }

        dialogRoot.close();
    }

    // [Return] 一覧にフォーカスがある場合に、選択中の項目を開く
    Shortcut {
        sequence: "Return"
        enabled: fileList.activeFocus
        onActivated: activateCurrentItem()
    }

    // [Esc] 入力欄にフォーカスが無く、確認ダイアログやフィルタ候補が開いていない場合に処理する
    Shortcut {
        sequence: "Esc"
        enabled: dialogRoot.visible
                 && !pathField.activeFocus
                 && !fileNameField.activeFocus
                 && !overwriteDialog.visible
                 && !filterCombo.popup.visible
        onActivated: handleEscape()
    }

    // 開いた時点で一覧へフォーカスを移し、先頭の項目を選択する
    onOpened: {
        fileList.forceActiveFocus();
        searchBuffer = "";
        selectedPath = "";
        if (fileList.count > 0) {
            fileList.currentIndex = 0;
        }
    }

    // 検索文字列がある間だけ、右上の検索表示を出す
    onSearchBufferChanged: {
        searchPopup.visible = searchBuffer.length > 0;
    }

    // [Alt+Left] 親フォルダへ移動する
    Shortcut {
        sequence: "Alt+Left"
        onActivated: {
            fsModel.navigateUp();
            dialogRoot.selectedPath = "";
            dialogRoot.searchBuffer = "";
        }
    }

    // 先頭一致検索の入力中の文字列を、右上に表示するポップアップ
    Rectangle {
        id: searchPopup
        width: 220
        height: 36
        color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.9)
        radius: 4
        visible: false
        z: 1000
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16

        Label {
            anchors.centerIn: parent
            text: qsTr("Search: ") + dialogRoot.searchBuffer
            textFormat: Text.PlainText
            color: theme.primaryText
        }
    }

    /**
     * @brief ファイル種別の絞り込み文字列から、括弧内のパターンを取り出す
     * @param filterString 絞り込み文字列 (例: "JSON files (*.json)")
     * @return 括弧内のパターン (括弧が無い場合は"*")
     */
    function extractFilterPattern(filterString) {
        var match = filterString.match(/\(([^)]+)\)/);
        return match ? match[1] : "*";
    }

    /**
     * @brief バイト数を、B・KB・MB・GBのいずれかの単位で整形する
     * @param bytes バイト数
     * @return 単位付きの文字列 (KB以上は小数第1位まで)
     * @note 現在はどこからも呼び出されていない
     */
    function formatFileSize(bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        if (bytes < 1024 * 1024 * 1024) return (bytes / 1024 / 1024).toFixed(1) + " MB";
        return (bytes / 1024 / 1024 / 1024).toFixed(1) + " GB";
    }

    background: Rectangle {
        color: theme.surface
        radius: 2
        border.color: theme.divider
        border.width: 1
    }

    header: Label {
        text: dialogRoot.titleText
        font.bold: true
        font.pixelSize: 16
        color: theme.foreground
        padding: 16
        leftPadding: 24
    }

    // フォルダの内容を一覧するモデル (フォルダ選択モードではファイルを表示せず、絞り込みも使わない)
    FileSystemModel {
        id: fsModel
        showFiles: !dialogRoot.folderMode
        nameFilters: dialogRoot.folderMode ? [] : [dialogRoot.extractFilterPattern(dialogRoot.nameFilters[filterCombo.currentIndex])]
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // 親フォルダへ移動するボタン
            Button {
                text: "▲"
                flat: true
                Material.theme: dialogRoot.materialTheme
                Material.foreground: theme.foreground
                onClicked: {
                    fsModel.navigateUp();
                    dialogRoot.selectedPath = "";
                    dialogRoot.searchBuffer = "";
                }
                ToolTip.text: qsTr("Go to parent folder")
                ToolTip.visible: hovered
            }

            // 現在のパスの表示欄 (直接入力して[Return]で移動もできる)
            TextField {
                id: pathField
                Layout.fillWidth: true
                text: fsModel.currentPath
                color: theme.foreground
                placeholderTextColor: theme.secondaryText
                Material.theme: dialogRoot.materialTheme
                font.family: "monospace"
                font.pixelSize: 14
                onAccepted: {
                    fsModel.setPath(text);
                    dialogRoot.searchBuffer = "";
                    dialogRoot.selectedPath = "";
                }
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Escape) {
                        dialogRoot.handleEscape();
                        event.accepted = true;
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: !dialogRoot.folderMode

            // ファイル種別の絞り込み欄
            ComboBox {
                id: filterCombo
                visible: !dialogRoot.folderMode
                model: dialogRoot.nameFilters
                currentIndex: 0
                Layout.preferredWidth: 200
                Material.theme: dialogRoot.materialTheme
                onActivated: {
                    fsModel.nameFilters = [dialogRoot.extractFilterPattern(dialogRoot.nameFilters[currentIndex])];
                }
            }

            // 保存するファイル名の入力欄 (保存モードのみ表示)
            TextField {
                id: fileNameField
                visible: dialogRoot.saveMode
                Layout.fillWidth: true
                placeholderText: qsTr("File name")
                color: theme.foreground
                placeholderTextColor: theme.secondaryText
                Material.theme: dialogRoot.materialTheme
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Escape) {
                        dialogRoot.handleEscape();
                        event.accepted = true;
                    }
                }
            }
        }

        // 隠しファイル・隠しフォルダを表示するかどうかの切り替え
        CheckBox {
            id: showHiddenCheck
            text: qsTr("Show hidden files and folders")
            checked: fsModel.showHidden
            focusPolicy: Qt.NoFocus
            Material.theme: dialogRoot.materialTheme
            Material.accent: theme.accent
            Material.foreground: theme.foreground
            onCheckedChanged: {
                fsModel.showHidden = checked;
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 500
            color: theme.background
            border.color: theme.divider
            border.width: 1
            radius: 2

            // フォルダ内の項目一覧 (キーボード操作とマウス操作に対応する)
            ListView {
                id: fileList
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                cacheBuffer: 200
                model: fsModel
                focus: true
                ScrollBar.vertical: ScrollBar {}

                // クリックで選択、ダブルクリックでフォルダへ移動またはファイルを確定する
                MouseArea {
                    anchors.fill: parent
                    onClicked: function(mouse) {
                        fileList.forceActiveFocus();
                        var idx = fileList.indexAt(mouse.x, mouse.y + fileList.contentY);
                        if (idx < 0) return;
                        var item = fsModel.get(idx);
                        if (!item) return;
                        if (dialogRoot.folderMode && !item.isDirectory) return;
                        fileList.currentIndex = idx;
                        dialogRoot.selectedPath = item.filePath;
                        if (dialogRoot.saveMode && !item.isDirectory) {
                            fileNameField.text = item.fileName;
                        }
                    }
                    onDoubleClicked: function(mouse) {
                        fileList.forceActiveFocus();
                        var idx = fileList.indexAt(mouse.x, mouse.y + fileList.contentY);
                        if (idx < 0) return;
                        var item = fsModel.get(idx);
                        if (!item) return;
                        if (item.isDirectory) {
                            fsModel.setPath(item.filePath);
                            dialogRoot.selectedPath = "";
                            dialogRoot.searchBuffer = "";
                        } else {
                            if (dialogRoot.folderMode) return;
                            if (dialogRoot.saveMode) {
                                fileNameField.text = item.fileName;
                                fileList.currentIndex = idx;
                                dialogRoot.selectedPath = item.filePath;
                                requestSave(item.fileName);
                                return;
                            }
                            dialogRoot.selectedPath = item.filePath;
                            dialogRoot.pathSelected(item.filePath, false);
                            dialogRoot.close();
                        }
                    }
                }

                Keys.onUpPressed: {
                    if (currentIndex > 0) currentIndex--;
                    syncSelectionToCurrentIndex();
                }
                Keys.onDownPressed: {
                    if (currentIndex < count - 1) currentIndex++;
                    syncSelectionToCurrentIndex();
                }
                Keys.onReturnPressed: {
                    if (currentItem) {
                        activateCurrentItem();
                    }
                }
                // 移動キー・[Esc]・[Backspace]の処理と、文字入力による先頭一致検索
                Keys.onPressed: function(event) {
                    switch (event.key) {
                    case Qt.Key_Home:
                        currentIndex = 0;
                        syncSelectionToCurrentIndex();
                        event.accepted = true;
                        break;
                    case Qt.Key_End:
                        currentIndex = count - 1;
                        syncSelectionToCurrentIndex();
                        event.accepted = true;
                        break;
                    case Qt.Key_PageUp:
                        currentIndex = Math.max(0, currentIndex - Math.floor(height / 36));
                        syncSelectionToCurrentIndex();
                        event.accepted = true;
                        break;
                    case Qt.Key_PageDown:
                        currentIndex = Math.min(count - 1, currentIndex + Math.floor(height / 36));
                        syncSelectionToCurrentIndex();
                        event.accepted = true;
                        break;
                    case Qt.Key_Escape:
                        dialogRoot.handleEscape();
                        event.accepted = true;
                        break;
                    case Qt.Key_Backspace:
                        if (dialogRoot.searchBuffer.length > 0) {
                            dialogRoot.searchBuffer = dialogRoot.searchBuffer.slice(0, -1);
                            performSearch();
                        }
                        event.accepted = true;
                        break;
                    case Qt.Key_Delete:
                        event.accepted = true;
                        break;
                    default:
                        if (event.modifiers === Qt.NoModifier && dialogRoot.isSearchInput(event.text)) {
                            dialogRoot.searchBuffer += event.text;
                            performSearch();
                            syncSelectionToCurrentIndex();
                            event.accepted = true;
                        }
                        break;
                    }
                }

                // 1項目分の表示 (選択中の項目は強調し、偶数行はわずかに色を変える)
                delegate: Rectangle {
                    width: ListView.view.width
                    height: 36
                    focus: true
                    Keys.forwardTo: [fileList]
                    color: {
                        if (ListView.isCurrentItem)
                            return Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.35);
                        if (index % 2 === 0)
                            return Qt.rgba(theme.foreground.r, theme.foreground.g, theme.foreground.b, 0.03);
                        return "transparent";
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Label {
                            text: model.isDirectory ? "▶" : "•"
                            color: model.isDirectory ? theme.accent : theme.secondaryText
                            font.pixelSize: 14
                        }

                        Label {
                            Layout.fillWidth: true
                            text: model.fileName
                            textFormat: Text.PlainText
                            color: theme.foreground
                            elide: Text.ElideMiddle
                        }

                        Label {
                            text: model.isDirectory ? "" : model.fileSizeString
                            color: theme.secondaryText
                            font.pixelSize: 11
                            visible: !model.isDirectory
                        }

                        Label {
                            text: model.isDirectory ? "" : Qt.formatDateTime(model.modifiedDate, "yyyy/MM/dd hh:mm")
                            color: theme.secondaryText
                            font.pixelSize: 11
                            visible: !model.isDirectory
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // 選択を確定するボタン (モードごとに確定の条件が異なる)
            Button {
                text: qsTr("OK")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                highlighted: true
                onClicked: {
                    if (dialogRoot.folderMode) {
                        var path = dialogRoot.selectedPath.length > 0 ? dialogRoot.selectedPath : fsModel.currentPath;
                        dialogRoot.pathSelected(path, false);
                        dialogRoot.close();
                    } else if (dialogRoot.saveMode) {
                        requestSave(fileNameField.text);
                    } else {
                        if (dialogRoot.selectedPath.length === 0)
                            return;
                        dialogRoot.pathSelected(dialogRoot.selectedPath, false);
                        dialogRoot.close();
                    }
                }
            }

            Button {
                text: qsTr("Cancel")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: dialogRoot.close()
            }
        }
    }

    // 表示フォルダが変わった場合は、パス表示・現在位置・選択状態を初期化する
    Connections {
        target: fsModel
        function onCurrentPathChanged() {
            pathField.text = fsModel.currentPath;
            fileList.currentIndex = 0;
            dialogRoot.selectedPath = "";
        }
    }

    // マウスの戻るボタンで、親フォルダへ移動する
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        onClicked: function(mouse) {
            if (mouse.button === Qt.BackButton) {
                fsModel.navigateUp();
                dialogRoot.selectedPath = "";
                dialogRoot.searchBuffer = "";
            }
        }
    }

    // 既存ファイルを上書きしてよいかの確認ダイアログ (承認時はoverwriteApprovedをtrueにして通知する)
    Dialog {
        id: overwriteDialog
        title: qsTr("Confirm Overwrite")
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            text: qsTr("The file '%1' already exists. Overwrite?").arg(dialogRoot.pendingSavePath)
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: theme.foreground
        }

        onAccepted: {
            dialogRoot.pathSelected(dialogRoot.pendingSavePath, true);
            dialogRoot.close();
        }
    }
}
