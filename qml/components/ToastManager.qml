import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/**
 * @brief 画面の隅に表示するトースト通知の管理コンポーネント
 *
 * show()で追加された通知を縦に並べ、フェードインしてから3秒後にフェードアウトして自動的に消す
 * 通知の種別ごとに、背景色とアイコンが切り替わる
 * 配置は、親側のanchorsで指定する (main.qmlでは画面右上)
 */
Item {
    id: root
    width: 320
    height: childrenRect.height

    /**
     * @brief トースト通知を追加する
     * @param message 通知メッセージ
     * @param type 通知の種別 ("success"・"error"・"warning"のいずれか、それ以外は"info"として扱う)
     */
    function show(message, type) {
        toastModel.append({message: message, type: type});
    }

    // 表示中の通知(message・type)を保持するモデル
    ListModel {
        id: toastModel
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.right: parent.right
        spacing: 8

        // 通知は同時に数件しか存在しないため、Repeaterで生成する
        Repeater {
            model: toastModel

            delegate: Rectangle {
                Layout.alignment: Qt.AlignRight
                width: toastContent.width + 24
                height: toastContent.height + 16
                radius: 8
                // 種別ごとの背景色
                color: {
                    switch (type) {
                    case "success": return "#4caf50";
                    case "error": return "#f44336";
                    case "warning": return "#ff9800";
                    default: return "#2196f3";
                    }
                }
                opacity: 0

                RowLayout {
                    id: toastContent
                    anchors.centerIn: parent
                    spacing: 8

                    // 種別ごとのアイコン
                    Label {
                        text: {
                            switch (type) {
                            case "success": return "✓";
                            case "error": return "✕";
                            case "warning": return "⚠";
                            default: return "ℹ";
                            }
                        }
                        color: "white"
                        font.bold: true
                    }

                    Label {
                        text: message
                        textFormat: Text.PlainText
                        color: "white"
                        wrapMode: Text.Wrap
                        Layout.maximumWidth: 260
                    }
                }

                // 表示時のフェードイン
                PropertyAnimation {
                    target: parent
                    property: "opacity"
                    to: 1
                    duration: 200
                    running: true
                }

                // 3秒表示した後にフェードアウトし、モデルから通知を削除する
                SequentialAnimation {
                    running: true
                    PauseAnimation { duration: 3000 }
                    NumberAnimation {
                        target: parent
                        property: "opacity"
                        to: 0
                        duration: 300
                    }
                    ScriptAction {
                        script: toastModel.remove(index);
                    }
                }
            }
        }
    }
}
