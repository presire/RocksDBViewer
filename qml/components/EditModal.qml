import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

Dialog {
    id: root
    title: qsTr("Edit Entry")
    modal: true
    anchors.centerIn: parent
    width: Math.min(700, parent.width * 0.8)
    height: Math.min(550, parent.height * 0.8)
    Material.theme: Material.Light

    property var theme
    property int materialTheme
    property string editKey: ""

    signal toastRequested(string message, string type)

    function openDialog(key, value) {
        editKey = key || "";
        keyField.text = key || "";
        keyField.enabled = !key;
        valueArea.text = value || "";
        root.title = key ? qsTr("Edit Entry") : qsTr("Add Entry");
        open();
    }

    function insertSkeleton(content) {
        if (valueArea.text.trim().length > 0) {
            overwriteDialog.skeletonContent = content;
            overwriteDialog.open();
        } else {
            valueArea.text = content;
        }
    }

    background: Rectangle {
        color: theme.surface
        radius: 2
        border.color: theme.divider
        border.width: 1
    }

    header: Label {
        text: root.title
        font.bold: true
        font.pixelSize: 16
        color: theme.foreground
        padding: 16
        leftPadding: 24
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: qsTr("Key")
            color: theme.foreground
        }
        TextField {
            id: keyField
            Layout.fillWidth: true
            placeholderText: qsTr("e.g., user:123")
            color: theme.foreground
            placeholderTextColor: theme.secondaryText
            Material.theme: materialTheme
            onTextChanged: {
                var id = JsonUtils.suggestSkeletonForKey(text);
                skeletonMenu.suggestedId = id;
                if (id.length > 0) {
                    var templates = JsonUtils.skeletonTemplates();
                    for (var i = 0; i < templates.length; ++i) {
                        if (templates[i].id === id) {
                            generateButton.ToolTip.text = qsTr("Suggested: %1").arg(qsTr(templates[i].labelKey));
                            break;
                        }
                    }
                    generateButton.highlighted = true;
                } else {
                    generateButton.ToolTip.text = qsTr("Generate JSON skeleton");
                    generateButton.highlighted = false;
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Value")
                Layout.fillWidth: true
                color: theme.foreground
            }
            Button {
                id: generateButton
                text: qsTr("Generate")
                flat: true
                Material.theme: materialTheme
                Material.foreground: theme.foreground
                ToolTip.text: qsTr("Generate JSON skeleton")
                ToolTip.visible: hovered
                onClicked: skeletonMenu.open()
            }
            Button {
                text: qsTr("Format")
                flat: true
                Material.theme: materialTheme
                Material.foreground: theme.foreground
                onClicked: valueArea.text = JsonUtils.formatJson(valueArea.text)
            }
            Button {
                text: qsTr("Minify")
                flat: true
                Material.theme: materialTheme
                Material.foreground: theme.foreground
                onClicked: valueArea.text = JsonUtils.minifyJson(valueArea.text)
            }
        }

        SkeletonMenu {
            id: skeletonMenu
            theme: root.theme
            materialTheme: root.materialTheme
            y: generateButton.y + generateButton.height
            x: generateButton.x
            onSkeletonSelected: function(id, content) { insertSkeleton(content); }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            TextArea {
                id: valueArea
                width: parent.width
                height: parent.height
                font.family: "monospace"
                placeholderText: qsTr('e.g., {"name": "John", "age": 30}')
                wrapMode: Text.Wrap
                color: theme.foreground
                placeholderTextColor: theme.secondaryText
                Material.theme: materialTheme
            }
        }

        Label {
            text: JsonUtils.validationMessage(valueArea.text)
            color: JsonUtils.isValidJson(valueArea.text) ? theme.success : theme.warning
            visible: text.length > 0
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Button {
                text: qsTr("Save")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                highlighted: true
                onClicked: {
                    if (keyField.text.length === 0) {
                        toastRequested(qsTr("Please enter a key"), "error");
                        return;
                    }
                    if (Backend.setData(keyField.text, valueArea.text)) {
                        root.close();
                    }
                }
            }
            Button {
                text: qsTr("Cancel")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: root.close()
            }
        }
    }

    Dialog {
        id: overwriteDialog
        property string skeletonContent: ""
        title: qsTr("Overwrite existing value?")
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            text: qsTr("The value field already has content. Overwrite with skeleton?")
            wrapMode: Text.Wrap
            color: theme.foreground
        }

        onAccepted: valueArea.text = skeletonContent
    }
}
