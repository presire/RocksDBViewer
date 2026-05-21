import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: root
    width: 320
    height: childrenRect.height

    function show(message, type) {
        toastModel.append({message: message, type: type});
    }

    ListModel {
        id: toastModel
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.right: parent.right
        spacing: 8

        Repeater {
            model: toastModel

            delegate: Rectangle {
                Layout.alignment: Qt.AlignRight
                width: toastContent.width + 24
                height: toastContent.height + 16
                radius: 8
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
                        color: "white"
                        wrapMode: Text.Wrap
                        Layout.maximumWidth: 260
                    }
                }

                // Entry animation
                PropertyAnimation {
                    target: parent
                    property: "opacity"
                    to: 1
                    duration: 200
                    running: true
                }

                // Exit animation after delay
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
