import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import RocksDBViewerApp

Menu {
    id: root

    property var theme
    property int materialTheme
    property string suggestedId: ""

    signal skeletonSelected(string id, string content)

    function refresh() {
        while (count > 0) {
            var oldItem = itemAt(0);
            removeItem(oldItem);
            oldItem.destroy();
        }
        var templates = JsonUtils.skeletonTemplates();
        for (var i = 0; i < templates.length; ++i) {
            var t = templates[i];
            var newItem = menuItemComp.createObject(null, {
                text: qsTr(t.labelKey),
                skeletonId: t.id,
                skeletonContent: JsonUtils.generateSkeleton(t.id)
            });
            newItem.font.bold = (t.id === suggestedId);
            addItem(newItem);
        }
    }

    Component {
        id: menuItemComp
        MenuItem {
            property string skeletonId: ""
            property string skeletonContent: ""
            Material.theme: root.materialTheme
            onTriggered: root.skeletonSelected(skeletonId, skeletonContent)
        }
    }

    onAboutToShow: refresh()
}
