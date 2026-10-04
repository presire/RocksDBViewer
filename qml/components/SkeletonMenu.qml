import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import RocksDBViewerApp

/**
 * @brief JSONスケルトン(雛形)を選択するメニュー
 *
 * メニューを開く直前に、JsonUtils.skeletonTemplates()の一覧から項目を作り直す
 * キー名から推測されたスケルトン (suggestedId) の項目は、太字で表示する
 * 選択された項目の内容は、skeletonSelectedシグナルで親へ通知する
 * Materialの明暗はmaterialThemeプロパティ経由で受け取る
 */
Menu {
    id: root

    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** 子コントロールへ適用するMaterial.themeの値 (Material.LightまたはMaterial.Dark) */
    property int materialTheme

    /** キー名から推測されたスケルトンのID (該当する項目を太字にする、推測できない場合は空文字) */
    property string suggestedId: ""

    /**
     * @brief スケルトンが選択されたことを通知する
     * @param id 選択されたスケルトンのID
     * @param content 選択されたスケルトンの内容 (JSON文字列)
     */
    signal skeletonSelected(string id, string content)

    /**
     * @brief メニュー項目をスケルトンの一覧から作り直す
     *
     * 既存の項目をすべて破棄してから、JsonUtils.skeletonTemplates()の内容で再生成する
     * 項目の表示名は、翻訳キー (labelKey) をqsTr()で翻訳したものを使う
     */
    function refresh() {
        // 既存の項目をすべて破棄する
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

    // メニュー項目の雛形 (refresh()で動的に生成する)
    Component {
        id: menuItemComp
        MenuItem {
            /** 項目が表すスケルトンのID */
            property string skeletonId: ""

            /** 項目が表すスケルトンの内容 (JSON文字列) */
            property string skeletonContent: ""
            Material.theme: root.materialTheme
            onTriggered: root.skeletonSelected(skeletonId, skeletonContent)
        }
    }

    // 表示のたびに、項目を最新の状態へ作り直す
    onAboutToShow: refresh()
}
