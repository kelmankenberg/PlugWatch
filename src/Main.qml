import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1100
    height: 760
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: qsTr("Plugwatch")
    color: "#101114"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool newOnly: false
    property int sortMode: 0
    property string selectedId: ""
    property var categoryOptions: [qsTr("All categories")].concat(backend.categories)
    property var results: []
    readonly property var selectedPlugin: {
        for (let i = 0; i < backend.plugins.length; ++i) {
            if (backend.plugins[i].id === selectedId)
                return backend.plugins[i]
        }
        return null
    }

    function updateResults() {
        const query = searchField.text.trim().toLocaleLowerCase()
        let items = backend.plugins.filter(function(plugin) {
            if (win.newOnly && backend.newIds.indexOf(plugin.id) < 0)
                return false
            if (categoryBox.currentIndex > 0
                    && plugin.category !== backend.categories[categoryBox.currentIndex - 1])
                return false
            if (!query)
                return true
            const text = [plugin.name, plugin.author, plugin.category, plugin.description,
                          (plugin.tags || []).join(" ")].join(" ").toLocaleLowerCase()
            return text.indexOf(query) >= 0
        })
        items.sort(function(a, b) {
            if (win.sortMode === 1)
                return a.category.localeCompare(b.category) || a.name.localeCompare(b.name)
            if (win.sortMode === 2)
                return a.name.localeCompare(b.name)
            const aTime = Date.parse(a.listedAt || "") || 0
            const bTime = Date.parse(b.listedAt || "") || 0
            return bTime - aTime || a.name.localeCompare(b.name)
        })
        results = items
        if (!selectedId || !items.some(function(plugin) { return plugin.id === selectedId }))
            selectedId = items.length ? items[0].id : ""
    }

    function selectRequestedPlugin() {
        if (!initialPluginId)
            return
        for (let i = 0; i < backend.plugins.length; ++i) {
            if (backend.plugins[i].id === initialPluginId) {
                selectedId = initialPluginId
                return
            }
        }
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        onActivated: win.helpVisible = !win.helpVisible
    }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: backend.refresh()
    }
    Shortcut {
        sequence: "Ctrl+R"
        context: Qt.ApplicationShortcut
        onActivated: backend.refresh()
    }
    Shortcut {
        sequence: "/"
        context: Qt.ApplicationShortcut
        onActivated: searchField.forceActiveFocus()
    }

    Connections {
        target: backend
        function onPluginsChanged() {
            win.categoryOptions = [qsTr("All categories")].concat(backend.categories)
            win.updateResults()
            win.selectRequestedPlugin()
        }
        function onCheckTimeChanged() {
            if (!checkTimeField.activeFocus)
                checkTimeField.text = backend.checkTime
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 76
            color: "#17191d"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 22
                spacing: 14
                ColumnLayout {
                    spacing: 1
                    Label {
                        text: qsTr("Plugwatch")
                        font.pixelSize: 23
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: qsTr("Omarchy plugin marketplace")
                        color: "#a6a8ad"
                        font.pixelSize: 12
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    visible: backend.newIds.length > 0
                    text: qsTr("New: %1").arg(backend.newIds.length.toLocaleString(Qt.locale().name))
                    color: theme.accent
                    font.weight: Font.DemiBold
                }
                Button {
                    text: backend.loading ? qsTr("Checking…") : qsTr("Check now")
                    enabled: !backend.loading
                    onClicked: backend.refresh()
                }
                ToolButton {
                    text: "?"
                    Accessible.name: qsTr("Show keyboard shortcuts")
                    onClicked: win.helpVisible = true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 18
            spacing: 16

            Frame {
                Layout.preferredWidth: 415
                Layout.minimumWidth: 300
                Layout.fillHeight: true
                padding: 14
                Material.roundedScale: theme.radius
                background: Rectangle {
                    color: "#17191d"
                    radius: theme.radius
                    border.color: "#2b2e34"
                }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Plugin listings")
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: qsTr("Showing %1").arg(win.results.length.toLocaleString(Qt.locale().name))
                            color: "#a6a8ad"
                            font.pixelSize: 12
                        }
                    }
                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search name, summary, author, or tags")
                        selectByMouse: true
                        onTextChanged: win.updateResults()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {
                            id: categoryBox
                            Layout.fillWidth: true
                            model: win.categoryOptions
                            currentIndex: 0
                            onActivated: win.updateResults()
                        }
                        ComboBox {
                            id: sortBox
                            Layout.preferredWidth: 145
                            model: [qsTr("Newest"), qsTr("Category"), qsTr("Name")]
                            currentIndex: win.sortMode
                            onActivated: function(index) {
                                win.sortMode = index
                                win.updateResults()
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: qsTr("New arrivals (%1)").arg(backend.newIds.length)
                            highlighted: win.newOnly
                            onClicked: {
                                win.newOnly = !win.newOnly
                                win.updateResults()
                            }
                        }
                        Button {
                            text: qsTr("All plugins")
                            highlighted: !win.newOnly
                            onClicked: {
                                win.newOnly = false
                                win.updateResults()
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: backend.newIds.length === 0
                        text: qsTr("New listings will appear here after the next successful check.")
                        color: "#a6a8ad"
                        wrapMode: Text.Wrap
                        font.pixelSize: 12
                    }
                    ListView {
                        id: pluginList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: win.results
                        spacing: 5
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            id: pluginRow
                            required property var modelData
                            width: ListView.view.width
                            highlighted: modelData.id === win.selectedId
                            onClicked: win.selectedId = modelData.id
                            contentItem: RowLayout {
                                spacing: 10
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Label {
                                        Layout.fillWidth: true
                                        text: pluginRow.modelData.name
                                        elide: Text.ElideRight
                                        font.weight: Font.DemiBold
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: pluginRow.modelData.category
                                        color: "#a6a8ad"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                                Label {
                                    visible: backend.newIds.indexOf(pluginRow.modelData.id) >= 0
                                    text: qsTr("NEW")
                                    color: theme.accent
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                }
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: pluginList.count === 0
                            text: backend.error.length > 0
                                  ? qsTr("No cached listings are available.")
                                  : qsTr("No plugins match these filters.")
                            color: "#a6a8ad"
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 24
                Material.roundedScale: theme.radius
                background: Rectangle {
                    color: "#17191d"
                    radius: theme.radius
                    border.color: "#2b2e34"
                }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 16
                    Label {
                        visible: !win.selectedPlugin
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.Wrap
                        text: backend.error.length > 0
                              ? qsTr("Could not load the marketplace catalog.\n%1").arg(backend.error)
                              : qsTr("Select a plugin to see its summary and installation details.")
                        color: "#a6a8ad"
                    }
                    ColumnLayout {
                        visible: !!win.selectedPlugin
                        Layout.fillWidth: true
                        spacing: 14
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: win.selectedPlugin ? win.selectedPlugin.name : ""
                                font.pixelSize: 27
                                font.weight: Font.DemiBold
                                wrapMode: Text.Wrap
                            }
                            Label {
                                visible: win.selectedPlugin
                                         && backend.newIds.indexOf(win.selectedPlugin.id) >= 0
                                text: qsTr("NEW")
                                color: theme.accent
                                font.weight: Font.Bold
                            }
                        }
                        Label {
                            text: win.selectedPlugin
                                  ? qsTr("%1 · by %2").arg(win.selectedPlugin.category,
                                                           win.selectedPlugin.author)
                                  : ""
                            color: "#a6a8ad"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14
                            Rectangle {
                                Layout.preferredWidth: 200
                                Layout.preferredHeight: 128
                                visible: win.selectedPlugin && win.selectedPlugin.thumbnailUrl.length > 0
                                color: "#101114"
                                radius: Math.min(4, theme.radius)
                                border.color: "#2b2e34"
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    source: win.selectedPlugin ? win.selectedPlugin.thumbnailUrl : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    cache: true
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: win.selectedPlugin ? win.selectedPlugin.description : ""
                                wrapMode: Text.Wrap
                                font.pixelSize: 15
                                lineHeight: 1.25
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: win.selectedPlugin && (win.selectedPlugin.tags || []).length > 0
                            text: win.selectedPlugin
                                  ? qsTr("Tags: %1").arg(win.selectedPlugin.tags.join(", "))
                                  : ""
                            color: "#a6a8ad"
                            wrapMode: Text.Wrap
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: "#2b2e34"
                        }
                        Label {
                            text: qsTr("Installation")
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: win.selectedPlugin && win.selectedPlugin.installCommand.length > 0
                            text: qsTr("Copy this command, then review it before running in a terminal.")
                            color: "#a6a8ad"
                            wrapMode: Text.Wrap
                        }
                        TextArea {
                            Layout.fillWidth: true
                            visible: win.selectedPlugin && win.selectedPlugin.installCommand.length > 0
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            text: win.selectedPlugin ? win.selectedPlugin.installCommand : ""
                            font.family: "monospace"
                            Material.roundedScale: theme.radius
                            background: Rectangle {
                                color: "#101114"
                                radius: theme.radius
                                border.color: "#2b2e34"
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: win.selectedPlugin && win.selectedPlugin.installCommand.length === 0
                            text: win.selectedPlugin ? win.selectedPlugin.installNote : ""
                            wrapMode: Text.Wrap
                            color: "#d4d5d8"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button {
                                visible: win.selectedPlugin
                                         && win.selectedPlugin.installCommand.length > 0
                                text: qsTr("Copy install command")
                                highlighted: true
                                onClicked: backend.copyInstallCommand(win.selectedPlugin.id)
                            }
                            Button {
                                text: win.selectedPlugin && win.selectedPlugin.installCommand.length === 0
                                      ? qsTr("Upstream installation")
                                      : qsTr("Open marketplace page")
                                onClicked: if (win.selectedPlugin)
                                               Qt.openUrlExternally(win.selectedPlugin.installCommand.length > 0
                                                   ? win.selectedPlugin.marketplaceUrl
                                                   : win.selectedPlugin.repo)
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                visible: win.selectedPlugin
                                         && backend.newIds.indexOf(win.selectedPlugin.id) >= 0
                                text: qsTr("Mark seen")
                                onClicked: backend.markPluginAsSeen(win.selectedPlugin.id)
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Morning check time")
                            color: "#a6a8ad"
                        }
                        TextField {
                            id: checkTimeField
                            Layout.preferredWidth: 92
                            text: backend.checkTime
                            placeholderText: "HH:MM"
                            inputMethodHints: Qt.ImhTime
                            selectByMouse: true
                            validator: RegularExpressionValidator {
                                regularExpression: /^(?:[01][0-9]|2[0-3]):[0-5][0-9]$/
                            }
                            onAccepted: backend.setCheckTime(text)
                        }
                        Button {
                            text: qsTr("Set")
                            enabled: checkTimeField.acceptableInput
                                     && checkTimeField.text !== backend.checkTime
                            onClicked: backend.setCheckTime(checkTimeField.text)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: backend.error.length > 0
                              ? backend.error
                              : backend.notice.length > 0
                                ? backend.notice
                                : backend.lastChecked.length > 0
                                  ? qsTr("Last checked: %1").arg(
                                        new Date(backend.lastChecked).toLocaleString(Qt.locale().name))
                                  : qsTr("The catalog is checked automatically each day at %1.")
                                        .arg(backend.checkTime)
                        color: backend.error.length > 0 ? "#ff8a80" : "#a6a8ad"
                        wrapMode: Text.Wrap
                        font.pixelSize: 12
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: backend.newIds.length > 0
                        Button {
                            text: qsTr("Mark all new as seen")
                            onClicked: backend.markNewAsSeen()
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: qsTr("Unseen: %1").arg(backend.newIds.length)
                            color: theme.accent
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.bottomMargin: 12
            text: qsTr("Community plugins are third-party code. Review the source before installing.")
            color: "#a6a8ad"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: helpDialog
        anchors.centerIn: parent
        width: Math.min(420, win.width - 40)
        modal: true
        title: qsTr("Keyboard shortcuts")
        standardButtons: Dialog.Ok
        Material.roundedScale: theme.radius
        contentItem: ColumnLayout {
            spacing: 10
            Label { text: qsTr("Space / Ctrl+R — check the marketplace") }
            Label { text: qsTr("/ — focus search") }
            Label { text: qsTr("? — show this help") }
            Label { text: qsTr("Q — quit") }
        }
        onOpened: win.helpVisible = true
        onClosed: win.helpVisible = false
    }

    onHelpVisibleChanged: {
        if (helpVisible && !helpDialog.visible)
            helpDialog.open()
        else if (!helpVisible && helpDialog.visible)
            helpDialog.close()
    }

    Component.onCompleted: {
        newOnly = initialNewOnly
        updateResults()
        selectRequestedPlugin()
    }
}
