import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

/**
 * @brief エントリの追加・編集・閲覧を行うダイアログ
 *
 * openDialog()の引数に応じて、追加・編集・閲覧のいずれかのモードで開く
 * 読み取り専用モード (Backend.readOnly) で既存エントリを開いた場合は、閲覧モードとなり編集できない
 * 値はJSON整形・圧縮・スケルトン (雛形) 生成に対応し、JSONとして不正な場合は警告メッセージを表示する
 * 新規追加時に同じキーが既に存在する場合は、上書きの確認ダイアログを表示する
 * 配色はthemeプロパティ、Materialの明暗はmaterialThemeプロパティ経由で受け取る
 */
Dialog {
    id: root
    title: qsTr("Edit Entry")
    modal: true
    anchors.centerIn: parent
    width: Math.min(700, parent.width * 0.8)
    height: Math.min(550, parent.height * 0.8)
    Material.theme: root.materialTheme

    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** 子コントロールへ適用するMaterial.themeの値 (Material.LightまたはMaterial.Dark) */
    property int materialTheme

    /** 編集中のエントリのキー (新規追加の場合は空文字) */
    property string editKey: ""

    /** 閲覧モードかどうか (読み取り専用モードで既存エントリを開いた場合にtrue) */
    property bool viewMode: false

    /**
     * @brief ユーザーへの通知が必要なことを親へ伝える
     * @param message 通知メッセージ
     * @param type 通知の種別 ("success"・"error"・"warning"・"info")
     */
    signal toastRequested(string message, string type)

    /**
     * @brief ダイアログを開く
     *
     * keyを省略した場合は新規追加モード、指定した場合は編集モードとなる
     * 編集モードでは、キーは変更できない
     * 読み取り専用モード (Backend.readOnly) で既存エントリを開いた場合は、閲覧モードとなる
     * @param key 編集対象のキー (新規追加の場合は省略)
     * @param value 編集対象の値 (新規追加の場合は省略)
     */
    function openDialog(key, value) {
        editKey = key || "";
        viewMode = key && Backend.readOnly;
        keyField.text = key || "";
        // 既存エントリのキーは変更できない
        keyField.enabled = !key;
        valueArea.text = value || "";
        valueArea.readOnly = viewMode;
        root.title = viewMode ? qsTr("View Entry") : (key ? qsTr("Edit Entry") : qsTr("Add Entry"));
        open();
    }

    /**
     * @brief 入力内容をBackendへ保存する
     *
     * 保存に成功した場合のみ、ダイアログを閉じる
     */
    function saveEntry() {
        if (Backend.setData(keyField.text, valueArea.text)) {
            root.close();
        }
    }

    /**
     * @brief 値の入力欄へJSONスケルトンを挿入する
     *
     * 値の入力欄に既に内容がある場合は、上書きの確認ダイアログを表示する
     * @param content 挿入するスケルトン (JSON文字列)
     */
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
            // キー名から推測したスケルトンを、メニューとGenerateボタンへ反映する
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
                visible: !root.viewMode
                Material.theme: materialTheme
                Material.foreground: theme.foreground
                ToolTip.text: qsTr("Generate JSON skeleton")
                ToolTip.visible: hovered
                onClicked: skeletonMenu.open()
            }
            Button {
                text: qsTr("Format")
                flat: true
                visible: !root.viewMode
                Material.theme: materialTheme
                Material.foreground: theme.foreground
                onClicked: valueArea.text = JsonUtils.formatJson(valueArea.text)
            }
            Button {
                text: qsTr("Minify")
                flat: true
                visible: !root.viewMode
                Material.theme: materialTheme
                Material.foreground: theme.foreground
                onClicked: valueArea.text = JsonUtils.minifyJson(valueArea.text)
            }
        }

        // Generateボタンの直下に表示するスケルトン選択メニュー
        SkeletonMenu {
            id: skeletonMenu
            theme: root.theme
            materialTheme: root.materialTheme
            y: generateButton.y + generateButton.height
            x: generateButton.x
            onSkeletonSelected: function(id, content) { insertSkeleton(content); }
        }

        // 値の入力欄 (横スクロールは使わず、折り返して表示する)
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

        // 値のJSON検証結果 (有効な場合は成功色、無効な場合は警告色で表示する)
        Label {
            text: JsonUtils.validationMessage(valueArea.text)
            textFormat: Text.PlainText
            color: JsonUtils.isValidJson(valueArea.text) ? theme.success : theme.warning
            visible: text.length > 0
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Button {
                text: qsTr("Save")
                visible: !root.viewMode
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                highlighted: true
                // キーの未入力チェックと、新規追加時の重複チェックを行ってから保存する
                onClicked: {
                    if (keyField.text.length === 0) {
                        toastRequested(qsTr("Please enter a key"), "error");
                        return;
                    }
                    if (root.editKey.length === 0 && Backend.keyExists(keyField.text)) {
                        keyOverwriteDialog.open();
                        return;
                    }
                    root.saveEntry();
                }
            }
            Button {
                text: root.viewMode ? qsTr("Close") : qsTr("Cancel")
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                onClicked: root.close()
            }
        }
    }

    // 入力済みの値をスケルトンで上書きしてよいかの確認ダイアログ
    Dialog {
        id: overwriteDialog

        /** 上書き確認中のスケルトン (JSON文字列) */
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

    // 新規追加時に、既存のキーを上書きしてよいかの確認ダイアログ
    Dialog {
        id: keyOverwriteDialog
        title: qsTr("Confirm Overwrite")
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            text: qsTr("Key '%1' already exists. Overwrite?").arg(keyField.text)
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: theme.foreground
        }

        onAccepted: root.saveEntry()
    }
}
