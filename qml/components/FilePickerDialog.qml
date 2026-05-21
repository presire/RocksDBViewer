import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

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
    Material.theme: Material.Light

    property var theme
    property int materialTheme
    property bool folderMode: false
    property bool saveMode: false
    property var nameFilters: ["*.*"]
    property string titleText: qsTr("Select")
    property string selectedPath: ""
    property string pendingSavePath: ""
    property string searchBuffer: ""

    signal pathSelected(string path)

    function openDialog() {
        selectedPath = "";
        searchBuffer = "";
        if (saveMode) {
            fileNameField.text = "";
        }
        open();
    }

    function performSearch() {
        var idx = fsModel.findIndexByPrefix(searchBuffer);
        if (idx >= 0) {
            fileList.currentIndex = idx;
            fileList.positionViewAtIndex(idx, ListView.Center);
        }
    }

    function syncSelectionToCurrentIndex() {
        var item = fsModel.get(fileList.currentIndex);
        if (item && item.filePath && (!dialogRoot.folderMode || item.isDirectory)) {
            dialogRoot.selectedPath = item.filePath;
        }
    }

    function activateCurrentItem() {
        var item = fsModel.get(fileList.currentIndex);
        if (!item) return;
        if (item.isDirectory) {
            fsModel.setPath(item.filePath);
            dialogRoot.selectedPath = "";
            dialogRoot.searchBuffer = "";
        } else {
            if (dialogRoot.folderMode) return;
            dialogRoot.selectedPath = item.filePath;
            if (dialogRoot.saveMode) {
                fileNameField.text = item.fileName;
            }
            dialogRoot.pathSelected(item.filePath);
            dialogRoot.close();
        }
    }

    function handleEscape() {
        if (dialogRoot.searchBuffer.length > 0) {
            dialogRoot.searchBuffer = "";
        } else {
            dialogRoot.close();
        }
    }

    Shortcut {
        sequence: "Return"
        enabled: fileList.activeFocus
        onActivated: activateCurrentItem()
    }

    Shortcut {
        sequence: "Esc"
        enabled: fileList.activeFocus
        onActivated: handleEscape()
    }

    Shortcut {
        sequence: "Esc"
        enabled: !fileList.activeFocus
        onActivated: dialogRoot.close()
    }

    onOpened: {
        fileList.forceActiveFocus();
        searchBuffer = "";
        if (fileList.count > 0) {
            fileList.currentIndex = 0;
            syncSelectionToCurrentIndex();
        }
    }

    onSearchBufferChanged: {
        searchPopup.visible = searchBuffer.length > 0;
    }

    Shortcut {
        sequence: "Alt+Left"
        onActivated: {
            fsModel.navigateUp();
            dialogRoot.selectedPath = "";
            dialogRoot.searchBuffer = "";
        }
    }

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
            color: theme.primaryText
        }
    }

    function extractFilterPattern(filterString) {
        var match = filterString.match(/\(([^)]+)\)/);
        return match ? match[1] : "*.*";
    }

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

            TextField {
                id: pathField
                Layout.fillWidth: true
                text: fsModel.currentPath
                color: theme.foreground
                placeholderTextColor: theme.secondaryText
                Material.theme: dialogRoot.materialTheme
                font.family: "monospace"
                font.pixelSize: 12
                onAccepted: {
                    fsModel.setPath(text);
                    dialogRoot.searchBuffer = "";
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: !dialogRoot.folderMode

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

            TextField {
                id: fileNameField
                visible: dialogRoot.saveMode
                Layout.fillWidth: true
                placeholderText: qsTr("File name")
                color: theme.foreground
                placeholderTextColor: theme.secondaryText
                Material.theme: dialogRoot.materialTheme
            }
        }

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

            ListView {
                id: fileList
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                cacheBuffer: 200
                model: fsModel
                focus: true
                ScrollBar.vertical: ScrollBar {}

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
                            dialogRoot.selectedPath = item.filePath;
                            if (dialogRoot.saveMode) {
                                fileNameField.text = item.fileName;
                            }
                            dialogRoot.pathSelected(item.filePath);
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
                        if (dialogRoot.searchBuffer.length > 0) {
                            dialogRoot.searchBuffer = "";
                        } else {
                            dialogRoot.close();
                        }
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
                        if (event.text.length > 0 && event.modifiers === Qt.NoModifier) {
                            dialogRoot.searchBuffer += event.text;
                            performSearch();
                            syncSelectionToCurrentIndex();
                            event.accepted = true;
                        }
                        break;
                    }
                }

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

            Button {
                text: qsTr("OK")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                highlighted: true
                onClicked: {
                    if (dialogRoot.folderMode) {
                        var path = dialogRoot.selectedPath.length > 0 ? dialogRoot.selectedPath : fsModel.currentPath;
                        dialogRoot.pathSelected(path);
                        dialogRoot.close();
                    } else if (dialogRoot.saveMode) {
                        if (fileNameField.text.length === 0)
                            return;
                        var fullPath = fsModel.currentPath + "/" + fileNameField.text;
                        if (fsModel.fileExists(fullPath)) {
                            dialogRoot.pendingSavePath = fullPath;
                            overwriteDialog.open();
                        } else {
                            dialogRoot.pathSelected(fullPath);
                            dialogRoot.close();
                        }
                    } else {
                        if (dialogRoot.selectedPath.length === 0)
                            return;
                        dialogRoot.pathSelected(dialogRoot.selectedPath);
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

    Connections {
        target: fsModel
        function onCurrentPathChanged() {
            pathField.text = fsModel.currentPath;
            fileList.currentIndex = 0;
            syncSelectionToCurrentIndex();
        }
    }

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

    Dialog {
        id: overwriteDialog
        title: qsTr("Confirm Overwrite")
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            text: qsTr("The file already exists. Overwrite?")
            wrapMode: Text.Wrap
            color: theme.foreground
        }

        onAccepted: {
            dialogRoot.pathSelected(dialogRoot.pendingSavePath);
            dialogRoot.close();
        }
    }
}
