import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import RocksDBViewerApp

/**
 * @brief 検索・ソート・ページング・データ操作を行うコントロールバー
 *
 * カラムファミリ選択、編集モード切り替え、検索欄、件数表示、操作ボタン、ページ送り、ページサイズ選択を横一列に並べる
 * 操作内容はすべてシグナルとして親へ通知し、実際の処理は親 (main.qml) が行う
 * 検索文字列の変更は、入力が止まってから300[ms]後にfilterTextChangedで通知する
 * 配色はthemeプロパティ、Materialの明暗はmaterialThemeプロパティ経由で受け取る
 */
RowLayout {
    /** 配色定数を持つTheme.qmlのインスタンス */
    property var theme

    /** 子コントロールへ適用するMaterial.themeの値 (Material.LightまたはMaterial.Dark) */
    property int materialTheme

    /** 件数表示に使うモデル(FilterProxyModel) */
    property var proxyModel

    /** 検索欄に入力されている文字列 (読み取り用) */
    property string searchText: searchField.text

    /** 自動更新が実行中かどうか (ボタンの表示文言と色に反映される) */
    property bool autoRefreshRunning: false

    /** 現在のページ番号 (0始まり) */
    property int currentPage: 0

    /** 総ページ数 */
    property int totalPages: 1

    /** 現在のページサイズ (ページサイズ選択欄の初期選択に使う) */
    property int pageSizeValue: 1000

    /** 総エントリ数の集計が上限で打ち切られたかどうか (trueの場合は総ページ数の後ろに"+"を付けて表示する) */
    property bool totalCountCapped: false

    /** UTF-8として表示できないため、一覧から除外したエントリ数 */
    property int hiddenNonUtf8Count: 0

    /** 新規エントリの追加が要求されたことを通知する */
    signal addRequested()

    /** データの再読み込みが要求されたことを通知する */
    signal refreshRequested()

    /** 自動更新の開始・停止の切り替えが要求されたことを通知する */
    signal autoRefreshToggled()

    /** ソート順の切り替えが要求されたことを通知する */
    signal sortRequested()

    /** JSONファイルへのエクスポートが要求されたことを通知する */
    signal exportRequested()

    /** JSONファイルからのインポートが要求されたことを通知する */
    signal importRequested()

    /** 全データの削除が要求されたことを通知する */
    signal clearAllRequested()

    /**
     * @brief カラムファミリが選択されたことを通知する
     * @param cf 選択されたカラムファミリ名
     */
    signal columnFamilyChanged(string cf)

    /**
     * @brief 検索文字列が確定したことを通知する
     *
     * 入力が止まってから300[ms]後に通知する
     * @param text 確定した検索文字列
     */
    signal filterTextChanged(string text)

    /**
     * @brief ページサイズが選択されたことを通知する
     * @param size 選択されたページサイズ (件数)
     */
    signal pageSizeChanged(int size)

    /** 前のページへの移動が要求されたことを通知する */
    signal previousPageRequested()

    /** 次のページへの移動が要求されたことを通知する */
    signal nextPageRequested()

    /**
     * @brief 検索欄へフォーカスを移し、入力済みの文字列を全選択する
     *
     * [Ctrl+F]のショートカットから呼び出される
     */
    function focusSearchField() {
        searchField.forceActiveFocus();
        searchField.selectAll();
    }

    spacing: 8

    // カラムファミリの選択欄 (選択中の値はBackend.currentColumnFamilyに追従する)
    ComboBox {
        id: cfCombo
        Layout.preferredWidth: 160
        model: Backend.columnFamilies
        currentIndex: model.indexOf(Backend.currentColumnFamily)
        onActivated: columnFamilyChanged(currentText)
        Material.theme: materialTheme
        topPadding: 8
        bottomPadding: 8
        leftPadding: 8
        rightPadding: 8
        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            color: theme.surface
            border.color: theme.foreground
            radius: 2
        }
        contentItem: Text {
            text: cfCombo.displayText
            textFormat: Text.PlainText
            font: cfCombo.font
            color: theme.foreground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        // ドロップダウンの三角形マークを、Canvasで描画する
        indicator: Canvas {
            id: canvas
            x: cfCombo.width - width - cfCombo.rightPadding
            y: cfCombo.topPadding + (cfCombo.availableHeight - height) / 2
            width: 12
            height: 8
            contextType: "2d"

            // テーマ切り替えで前景色が変わった場合は、三角形を再描画する
            Connections {
                target: theme
                function onForegroundChanged() { canvas.requestPaint() }
            }

            onPaint: {
                context.reset();
                context.moveTo(0, 0);
                context.lineTo(width, 0);
                context.lineTo(width / 2, height);
                context.closePath();
                context.fillStyle = theme.foreground;
                context.fill();
            }
        }
        delegate: ItemDelegate {
            width: cfCombo.width
            contentItem: Text {
                text: modelData
                textFormat: Text.PlainText
                font: cfCombo.font
                color: highlighted ? theme.primaryText : theme.foreground
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            highlighted: cfCombo.highlightedIndex === index
            background: Rectangle {
                color: highlighted ? theme.primary : "transparent"
            }
        }
        popup: Popup {
            y: cfCombo.height - 1
            width: cfCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1
            Material.theme: materialTheme

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: cfCombo.popup.visible ? cfCombo.delegateModel : null
                currentIndex: cfCombo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                color: theme.surface
                border.color: theme.divider
                radius: 2
            }
        }
    }

    // 編集モードの切り替えスイッチと、読み取り専用などの状態表示 (カラムファミリ選択欄の隣)
    ColumnLayout {
        spacing: 0
        // 無効化されたコントロールにはホバーイベントが届かないため、ツールチップはコンテナ側に持たせる
        ToolTip.text: Backend.editLocked ? Backend.editLockReason : qsTr("Edit mode")
        ToolTip.visible: editModeHover.hovered

        HoverHandler {
            id: editModeHover
        }

        Switch {
            id: editModeSwitch
            Layout.alignment: Qt.AlignHCenter
            checked: !Backend.readOnly
            enabled: Backend.connected && !Backend.editLocked
            Material.theme: materialTheme
            Material.foreground: theme.foreground
            onToggled: {
                Backend.setReadOnlyMode(!checked);
                // ユーザー操作で解除されたcheckedのバインディングを、Backendの状態へ追従するよう張り直す
                checked = Qt.binding(function() { return !Backend.readOnly; });
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            visible: Backend.connected
            text: Backend.editLocked ? qsTr("Locked") : (Backend.readOnly ? qsTr("Read-only") : qsTr("Edit mode"))
            color: Backend.readOnly ? theme.warning : theme.primary
            font.pixelSize: 11
        }
    }

    // キー・値の検索欄 (入力が止まってから通知する)
    TextField {
        id: searchField
        Layout.fillWidth: true
        Layout.minimumWidth: 130
        Layout.minimumHeight: 48
        placeholderText: qsTr("Search by key or value...")
        onTextChanged: debounceTimer.restart()
        color: theme.foreground
        placeholderTextColor: theme.secondaryText
        verticalAlignment: Text.AlignVCenter
        topPadding: 12
        bottomPadding: 12
        leftPadding: 12
        rightPadding: 12
        background: Rectangle {
            implicitWidth: 200
            implicitHeight: 48
            color: theme.surface
            border.color: searchField.activeFocus ? theme.primary : theme.foreground
            radius: 2
        }

        // 入力のたびに通知しないよう、300[ms]の間引きを行う
        Timer {
            id: debounceTimer
            interval: 300
            onTriggered: function() { filterTextChanged(searchField.text) }
        }
    }

    // 検索結果を含む、現在表示中の件数
    Label {
        text: qsTr("%1 entries").arg(proxyModel.count)
        font.bold: true
        color: theme.foreground
    }

    // データ操作ボタン群 (各ボタンは、接続状態・編集モードに応じて有効・無効が切り替わる)
    RowLayout {
        spacing: 2

        Button {
            text: qsTr("Add New")
            flat: true
            leftPadding: 6
            rightPadding: 6
            enabled: Backend.connected && !Backend.readOnly
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Add new entry")
            ToolTip.visible: hovered
            onClicked: addRequested()
        }

        Button {
            text: qsTr("Refresh")
            flat: true
            leftPadding: 6
            rightPadding: 6
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Refresh data")
            ToolTip.visible: hovered
            onClicked: refreshRequested()
        }

        Button {
            text: autoRefreshRunning ? qsTr("Auto-Refresh OFF") : qsTr("Auto-Refresh ON")
            flat: true
            leftPadding: 6
            rightPadding: 6
            enabled: Backend.connected
            Material.foreground: enabled ? (autoRefreshRunning ? theme.success : theme.foreground) : theme.disabled
            ToolTip.text: qsTr("Toggle auto-refresh")
            ToolTip.visible: hovered
            onClicked: autoRefreshToggled()
        }

        Button {
            text: qsTr("Sort")
            flat: true
            leftPadding: 6
            rightPadding: 6
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Toggle sort order")
            ToolTip.visible: hovered
            onClicked: sortRequested()
        }

        Button {
            text: qsTr("Export")
            flat: true
            leftPadding: 6
            rightPadding: 6
            enabled: Backend.connected
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Export data to JSON")
            ToolTip.visible: hovered
            onClicked: exportRequested()
        }

        Button {
            text: qsTr("Import")
            flat: true
            leftPadding: 6
            rightPadding: 6
            enabled: Backend.connected && !Backend.readOnly
            Material.foreground: enabled ? theme.foreground : theme.disabled
            ToolTip.text: qsTr("Import data from JSON")
            ToolTip.visible: hovered
            onClicked: importRequested()
        }

        Button {
            text: qsTr("Clear All")
            flat: true
            leftPadding: 6
            rightPadding: 6
            enabled: Backend.connected && !Backend.readOnly
            Material.foreground: enabled ? theme.error : theme.disabled
            ToolTip.text: qsTr("Clear all data")
            ToolTip.visible: hovered
            onClicked: clearAllRequested()
        }
    }

    // ページ送りボタンと現在のページ番号の表示
    RowLayout {
        spacing: 4
        Button {
            text: "◀"
            flat: true
            enabled: currentPage > 0
            Material.foreground: enabled ? theme.foreground : theme.disabled
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: parent.enabled ? theme.foreground : theme.disabled
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: previousPageRequested()
        }
        Label {
            text: totalCountCapped
                  ? qsTr("%1 / %2+").arg(currentPage + 1).arg(totalPages)
                  : qsTr("%1 / %2").arg(currentPage + 1).arg(totalPages)
            color: theme.foreground
            font.bold: true
        }
        Button {
            text: "▶"
            flat: true
            enabled: currentPage < totalPages - 1
            Material.foreground: enabled ? theme.foreground : theme.disabled
            contentItem: Text {
                text: parent.text
                font: parent.font
                color: parent.enabled ? theme.foreground : theme.disabled
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: nextPageRequested()
        }
    }

    // 非UTF-8のため除外したエントリがある場合のみ表示する
    Label {
        visible: hiddenNonUtf8Count > 0
        text: qsTr("%1 non-UTF-8 entries hidden").arg(hiddenNonUtf8Count)
        color: theme.warning
        font.pixelSize: 11
    }

    // 1ページあたりの表示件数の選択欄
    ComboBox {
        id: pageSizeCombo
        Layout.preferredWidth: 100
        model: [100, 500, 1000, 5000]
        currentIndex: model.indexOf(pageSizeValue)
        onActivated: function(index) { pageSizeChanged(model[index]); }
        Material.theme: materialTheme
        topPadding: 8
        bottomPadding: 8
        leftPadding: 8
        rightPadding: 8
        background: Rectangle {
            implicitWidth: 120
            implicitHeight: 40
            color: theme.surface
            border.color: theme.foreground
            radius: 2
        }
        contentItem: Text {
            text: pageSizeCombo.displayText
            font: pageSizeCombo.font
            color: theme.foreground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        // ドロップダウンの三角形マークを、Canvasで描画する
        indicator: Canvas {
            id: pageSizeCanvas
            x: pageSizeCombo.width - width - pageSizeCombo.rightPadding
            y: pageSizeCombo.topPadding + (pageSizeCombo.availableHeight - height) / 2
            width: 12
            height: 8
            contextType: "2d"

            // テーマ切り替えで前景色が変わった場合は、三角形を再描画する
            Connections {
                target: theme
                function onForegroundChanged() { pageSizeCanvas.requestPaint() }
            }

            onPaint: {
                context.reset();
                context.moveTo(0, 0);
                context.lineTo(width, 0);
                context.lineTo(width / 2, height);
                context.closePath();
                context.fillStyle = theme.foreground;
                context.fill();
            }
        }
        delegate: ItemDelegate {
            width: pageSizeCombo.width
            contentItem: Text {
                text: modelData
                font: pageSizeCombo.font
                color: highlighted ? theme.primaryText : theme.foreground
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            highlighted: pageSizeCombo.highlightedIndex === index
            background: Rectangle {
                color: highlighted ? theme.primary : "transparent"
            }
        }
        popup: Popup {
            y: pageSizeCombo.height - 1
            width: pageSizeCombo.width
            implicitHeight: contentItem.implicitHeight
            padding: 1
            Material.theme: materialTheme

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: pageSizeCombo.popup.visible ? pageSizeCombo.delegateModel : null
                currentIndex: pageSizeCombo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }

            background: Rectangle {
                color: theme.surface
                border.color: theme.divider
                radius: 2
            }
        }
    }
}
