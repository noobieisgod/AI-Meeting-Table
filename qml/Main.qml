pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import "TranscriptScroll.js" as TranscriptScroll

ApplicationWindow {
    id: root
    required property var appController

    width: 420
    height: 860
    visible: true
    title: "AI Meeting Table"

    Material.theme: Material.Light
    Material.accent: "#2563eb"

    property var tableRows: []
    property var currentTable: ({})
    property var seatRows: []
    property var transcriptRows: []
    property var attachmentRows: []
    property var artifactRows: []
    property var logRows: []
    property var modelRefreshRows: []
    property int selectedTab: 0
    property int editingSeatIndex: -1
    property var editingSeat: ({})
    property string transcriptViewTableId: ""
    property var transcriptScrollStates: ({})
    property var pendingTranscriptRestore: null
    property int transcriptRefreshGeneration: 0
    readonly property real transcriptFollowThreshold: 64

    function refreshTables() {
        root.tableRows = root.appController.tables();
    }

    function refreshCurrentTable() {
        root.currentTable = root.appController.currentTable();
    }

    function refreshSeats() {
        root.seatRows = root.appController.seats();
    }

    function refreshTranscript() {
        var nextTableId = root.appController.currentTableId || "";
        var previousTableId = root.transcriptViewTableId;
        var previousState = null;
        if (previousTableId.length > 0) {
            if (root.pendingTranscriptRestore && root.pendingTranscriptRestore.tableId === previousTableId) {
                previousState = root.pendingTranscriptRestore.state;
            } else {
                previousState = TranscriptScroll.capture(transcriptList, root.transcriptFollowThreshold);
            }
            root.transcriptScrollStates[previousTableId] = previousState;
        }

        var restoreState = null;
        if (nextTableId === previousTableId && previousState) {
            restoreState = previousState;
        } else if (root.transcriptScrollStates[nextTableId]) {
            restoreState = root.transcriptScrollStates[nextTableId];
        } else {
            restoreState = {
                follow: true,
                contentY: 0
            };
        }

        var rows = root.appController.transcript();
        root.transcriptRefreshGeneration += 1;
        root.transcriptRows = rows;
        root.transcriptViewTableId = nextTableId;
        root.pendingTranscriptRestore = {
            generation: root.transcriptRefreshGeneration,
            tableId: nextTableId,
            expectedCount: rows.length,
            state: restoreState,
            lastContentHeight: -1,
            stablePasses: 0,
            attempts: 0
        };
        transcriptRestoreTimer.interval = 16;
        transcriptRestoreTimer.restart();
    }

    function refreshArtifacts() {
        root.artifactRows = root.appController.artifacts();
    }

    function refreshAttachments() {
        root.attachmentRows = root.appController.attachments();
    }

    function refreshLogs() {
        root.logRows = root.appController.logs();
    }

    function refreshSettings() {
        root.modelRefreshRows = root.appController.modelRefreshStatuses();
    }

    function refreshAll() {
        root.refreshTables();
        root.refreshCurrentTable();
        root.refreshSeats();
        root.refreshTranscript();
        root.refreshAttachments();
        root.refreshArtifacts();
        root.refreshLogs();
        root.refreshSettings();
    }

    function openSeatEditor(index) {
        root.editingSeatIndex = index;
        root.editingSeat = root.seatRows[index] || ({});
        seatNameField.text = root.editingSeat.displayName || "";
        occupiedSwitch.checked = Boolean(root.editingSeat.occupied);
        providerCombo.currentIndex = root.editingSeat.providerIndex || 0;
        root.refreshModelCombo(root.editingSeat.modelId || "");
        effortCombo.currentIndex = root.editingSeat.effortIndex || 0;
        roleCombo.currentIndex = root.editingSeat.roleIndex || 0;
        seatSheet.open();
    }

    function refreshModelCombo(selectedModelId) {
        var models = root.appController.modelsForProvider(providerCombo.currentIndex);
        modelCombo.model = models;
        var found = 0;
        for (var i = 0; i < models.length; i++) {
            if (models[i].id === selectedModelId) {
                found = i;
                break;
            }
        }
        modelCombo.currentIndex = found;
    }

    function showErrorIfNeeded() {
        var message = root.appController.lastError();
        if (message && message.length > 0) {
            errorDialog.text = message;
            errorDialog.open();
        }
    }

    function saveProviderKeysAndRefresh() {
        var ok = root.appController.saveApiKey(0, openAiKeyField.text);
        ok = root.appController.saveApiKey(1, googleKeyField.text) && ok;
        ok = root.appController.saveApiKey(2, anthropicKeyField.text) && ok;
        if (!ok) {
            root.showErrorIfNeeded();
            return;
        }
        root.appController.refreshModels();
    }

    function hideKeyboardAfterSend() {
        composer.focus = false;
        // qmllint disable missing-property
        Qt.inputMethod.hide();
        // qmllint enable missing-property
        Qt.callLater(function () {
            root.contentItem.forceActiveFocus();
        });
    }

    Component.onCompleted: {
        root.appController.startupInitialRefreshStarted();
        root.refreshAll();
        root.appController.startupInitialRefreshCompleted();
        root.appController.startupPrimaryControlsReady();
    }

    Connections {
        target: root.appController
        function onStateChanged() {
            root.refreshCurrentTable();
        }
        function onTablesChanged() {
            root.refreshTables();
        }
        function onSeatsChanged() {
            root.refreshSeats();
        }
        function onTranscriptChanged() {
            root.refreshTranscript();
        }
        function onAttachmentsChanged() {
            root.refreshAttachments();
        }
        function onArtifactsChanged() {
            root.refreshArtifacts();
        }
        function onLogsChanged() {
            root.refreshLogs();
        }
        function onSettingsChanged() {
            root.refreshSettings();
        }
        function onAttachmentImportFailed() {
            root.showErrorIfNeeded();
        }
        function onContinuationRequested(reason) {
            continuationDialog.text = reason;
            continuationDialog.open();
        }
    }

    Timer {
        id: transcriptRestoreTimer
        interval: 16
        repeat: false
        onTriggered: {
            var pending = root.pendingTranscriptRestore;
            if (!pending || pending.generation !== root.transcriptRefreshGeneration) {
                return;
            }

            transcriptList.forceLayout();
            pending.attempts += 1;
            var currentHeight = transcriptList.contentHeight;
            var countReady = transcriptList.count === pending.expectedCount;
            var heightStable = countReady && Math.abs(currentHeight - pending.lastContentHeight) < 0.5;
            pending.stablePasses = heightStable ? pending.stablePasses + 1 : 0;
            pending.lastContentHeight = currentHeight;

            if (pending.stablePasses >= 2) {
                TranscriptScroll.restore(transcriptList, pending.state);
                root.transcriptScrollStates[pending.tableId] = TranscriptScroll.capture(transcriptList, root.transcriptFollowThreshold);
                root.pendingTranscriptRestore = null;
                root.appController.startupTranscriptVisualStable();
            } else {
                transcriptRestoreTimer.interval = pending.attempts < 12 ? 16 : 100;
                transcriptRestoreTimer.restart();
            }
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4

            ToolButton {
                implicitWidth: 48
                implicitHeight: 48
                Accessible.name: "Meeting tables"
                onClicked: tableDrawer.open()
                contentItem: Item {
                    implicitWidth: 24
                    implicitHeight: 24
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Repeater {
                            model: 3
                            Rectangle {
                                width: 22
                                height: 2
                                radius: 1
                                color: "#111827"
                            }
                        }
                    }
                }
            }

            Label {
                text: root.currentTable.title || "AI Meeting Table"
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            ToolButton {
                text: "\u2699"
                Accessible.name: "Settings"
                onClicked: settingsSheet.open()
            }
        }
    }

    Drawer {
        id: tableDrawer
        width: Math.min(root.width * 0.86, 420)
        height: root.height

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Label {
                text: "Meeting Tables"
                font.pixelSize: 22
                font.bold: true
            }

            TextField {
                id: tableSearch
                placeholderText: "Search"
                Layout.fillWidth: true
            }

            ListView {
                id: tableList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.tableRows.filter(function (row) {
                    return !tableSearch.text || row.title.toLowerCase().indexOf(tableSearch.text.toLowerCase()) >= 0;
                })
                delegate: ItemDelegate {
                    id: tableDelegate
                    required property var modelData

                    width: tableList.width
                    highlighted: tableDelegate.modelData.selected
                    onClicked: {
                        root.appController.selectTable(tableDelegate.modelData.tableId);
                        tableDrawer.close();
                    }
                    contentItem: Column {
                        spacing: 2
                        Label {
                            text: (tableDelegate.modelData.pinned ? "\u2605 " : "") + tableDelegate.modelData.title
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            text: tableDelegate.modelData.phase + "  Round " + tableDelegate.modelData.round + "  " + tableDelegate.modelData.updatedAt
                            font.pixelSize: 12
                            opacity: 0.72
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "New"
                    Layout.fillWidth: true
                    onClicked: createTableDialog.open()
                }
                Button {
                    text: "Duplicate"
                    Layout.fillWidth: true
                    onClicked: {
                        if (!root.appController.duplicateCurrentTable())
                            root.showErrorIfNeeded();
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: root.currentTable.pinned ? "Unpin" : "Pin"
                    Layout.fillWidth: true
                    onClicked: root.appController.togglePinCurrentTable()
                }
                Button {
                    text: "Rename"
                    Layout.fillWidth: true
                    onClicked: {
                        renameField.text = root.currentTable.title || "";
                        renameDialog.open();
                    }
                }
                Button {
                    text: "Delete"
                    Layout.fillWidth: true
                    Material.foreground: "#b91c1c"
                    onClicked: deleteDialog.open()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StackLayout {
            id: mainPages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.selectedTab

            ScrollView {
                id: tablePage
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: Math.max(0, tablePage.availableWidth - 32)
                    x: 16
                    y: 16
                    spacing: 12

                    Pane {
                        Layout.fillWidth: true
                        Material.elevation: 1
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: root.currentTable.phase || "Idle"
                                    font.pixelSize: 22
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: root.currentTable.elapsed || "00:00"
                                    opacity: 0.72
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: root.width >= 520 ? 4 : 2
                                rowSpacing: 8
                                columnSpacing: 8
                                Repeater {
                                    model: ["Round " + (root.currentTable.round || 0), "Tokens " + (root.currentTable.usedTokens || 0) + "/" + (root.currentTable.maxTokens || 0), "Cost $" + Number(root.currentTable.usedCost || 0).toFixed(2), (root.currentTable.artifactCount || 0) + " artifacts"]
                                    Label {
                                        id: statisticLabel
                                        required property string modelData

                                        text: statisticLabel.modelData
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignHCenter
                                        padding: 8
                                        background: Rectangle {
                                            color: "#eef2ff"
                                            radius: 8
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Pane {
                        Layout.fillWidth: true
                        implicitHeight: Math.max(360, root.height * 0.48)
                        Material.elevation: 1

                        Item {
                            anchors.fill: parent
                            Rectangle {
                                id: tableOval
                                width: Math.min(parent.width * 0.34, 170)
                                height: Math.min(parent.height * 0.52, 220)
                                radius: width / 2
                                color: "#d7c3a4"
                                border.color: "#7a5b35"
                                anchors.centerIn: parent
                            }

                            Repeater {
                                model: root.seatRows
                                delegate: Button {
                                    id: seatButton
                                    required property int index
                                    required property var modelData

                                    width: Math.min(parent.width * 0.34, 150)
                                    height: 72
                                    text: seatButton.modelData.displayName + "\n" + (seatButton.modelData.occupied ? seatButton.modelData.model : "Empty")
                                    font.pixelSize: 12
                                    Accessible.name: seatButton.text
                                    onClicked: root.openSeatEditor(seatButton.index)
                                    Material.background: seatButton.modelData.active ? "#60a5fa" : seatButton.modelData.decisionMaker ? "#fde68a" : seatButton.modelData.occupied ? "#dbeafe" : "#f3f4f6"
                                    Material.foreground: "#111827"

                                    contentItem: Item {
                                        Column {
                                            width: parent.width
                                            anchors.centerIn: parent
                                            spacing: 2

                                            Label {
                                                width: parent.width
                                                text: seatButton.modelData.displayName
                                                font.pixelSize: 12
                                                horizontalAlignment: Text.AlignHCenter
                                                elide: Text.ElideRight
                                                maximumLineCount: 1
                                            }
                                            Label {
                                                width: parent.width
                                                text: seatButton.modelData.occupied ? seatButton.modelData.model : "Empty"
                                                font.pixelSize: 11
                                                horizontalAlignment: Text.AlignHCenter
                                                wrapMode: Text.WrapAnywhere
                                                elide: Text.ElideRight
                                                maximumLineCount: 2
                                            }
                                        }
                                    }

                                    property real angle: ((seatButton.index / Math.max(1, root.seatRows.length)) * Math.PI * 2) - Math.PI / 2
                                    property real seatRadiusBoost: (seatButton.index === 0 || seatButton.index === 4) ? 34 : 0
                                    x: parent.width / 2 + Math.cos(angle) * Math.min(parent.width * 0.34, 230) - width / 2
                                    y: parent.height / 2 + Math.sin(angle) * (Math.min(parent.height * 0.32, 180) + seatRadiusBoost) - height / 2
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: root.appController.running ? "Pause" : (root.currentTable.phase === "Paused" || root.currentTable.phase === "Needs continuation" ? "Continue" : "Run")
                            icon.name: root.appController.running ? "media-playback-pause" : "media-playback-start"
                            Layout.fillWidth: true
                            onClicked: {
                                if (root.appController.running) {
                                    if (!root.appController.pauseSession())
                                        root.showErrorIfNeeded();
                                } else {
                                    if (!root.appController.runOrResume())
                                        root.showErrorIfNeeded();
                                }
                            }
                        }
                        Button {
                            text: "Stop"
                            Layout.fillWidth: true
                            onClicked: root.appController.stopSession()
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 16
                    }
                }
            }

            Page {
                id: transcriptPage

                Pane {
                    id: transcriptActions
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    implicitHeight: 52
                    padding: 8

                    RowLayout {
                        anchors.fill: parent
                        Label {
                            text: "Transcript"
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Button {
                            text: "Copy Full Transcript"
                            enabled: root.transcriptRows.length > 0
                            onClicked: {
                                if (!root.appController.copyFullTranscript()) {
                                    root.showErrorIfNeeded();
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: transcriptList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: transcriptActions.bottom
                    anchors.bottom: composerPane.top
                    anchors.margins: 12
                    anchors.bottomMargin: 8
                    clip: true
                    model: root.transcriptRows
                    spacing: 8
                    delegate: Pane {
                        id: transcriptDelegate
                        required property var modelData

                        width: transcriptList.width
                        Material.elevation: 0
                        background: Rectangle {
                            color: transcriptDelegate.modelData.isUser ? "#eff6ff" : (transcriptDelegate.modelData.isDecision ? "#fef3c7" : "#ffffff")
                            border.color: "#e5e7eb"
                            radius: 8
                        }
                        ColumnLayout {
                            width: parent.width
                            Label {
                                text: "[" + transcriptDelegate.modelData.timestamp + "] " + transcriptDelegate.modelData.speaker + "  " + transcriptDelegate.modelData.phase
                                font.bold: true
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            TextEdit {
                                id: transcriptBody
                                text: transcriptDelegate.modelData.content
                                textFormat: TextEdit.MarkdownText
                                wrapMode: TextEdit.Wrap
                                color: "#111827"
                                readOnly: true
                                selectByMouse: true
                                persistentSelection: true
                                activeFocusOnPress: true
                                Layout.fillWidth: true
                                Layout.preferredHeight: contentHeight
                                Accessible.name: "Transcript message from " + transcriptDelegate.modelData.speaker
                                onLinkActivated: function (link) {}
                            }
                        }
                    }
                    Column {
                        anchors.centerIn: parent
                        spacing: 8
                        visible: transcriptList.count === 0
                        width: parent.width * 0.86
                        Label {
                            text: "Nothing Here Yet"
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: 22
                            font.bold: true
                            color: "#6b7280"
                        }
                        Label {
                            text: "Run the table to generate content."
                            anchors.horizontalCenter: parent.horizontalCenter
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            width: parent.width
                            color: "#9ca3af"
                        }
                    }
                }

                Pane {
                    id: composerPane
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.bottomMargin: 12
                    implicitHeight: composerLayout.implicitHeight + topPadding + bottomPadding
                    ColumnLayout {
                        id: composerLayout
                        anchors.fill: parent
                        spacing: 6

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: visible ? childrenRect.height : 0
                            visible: root.attachmentRows.length > 0
                            spacing: 6

                            Repeater {
                                model: root.attachmentRows
                                delegate: Rectangle {
                                    id: attachmentChip
                                    required property var modelData

                                    width: Math.min(composerPane.width - 16, chipRow.implicitWidth + 16)
                                    height: 32
                                    radius: 8
                                    color: "#eef2ff"
                                    border.color: "#c7d2fe"

                                    RowLayout {
                                        id: chipRow
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        spacing: 4
                                        Label {
                                            text: attachmentChip.modelData.displayName
                                            elide: Text.ElideMiddle
                                            Layout.maximumWidth: 220
                                            Accessible.name: "Attachment " + attachmentChip.modelData.displayName
                                        }
                                        ToolButton {
                                            text: "×"
                                            Accessible.name: "Remove attachment " + attachmentChip.modelData.displayName
                                            onClicked: {
                                                if (!root.appController.removeAttachment(attachmentChip.modelData.attachmentId))
                                                    root.showErrorIfNeeded();
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            ToolButton {
                                text: root.appController.attachmentImportInProgress ? "Cancel" : "+"
                                enabled: root.appController.attachmentImportStatus !== "Cancelling attachment import..."
                                Accessible.name: root.appController.attachmentImportInProgress
                                    ? "Cancel attachment import"
                                    : "Add attachment"
                                onClicked: {
                                    if (root.appController.attachmentImportInProgress)
                                        root.appController.cancelAttachmentImport();
                                    else
                                        attachmentDialog.open();
                                }
                            }
                            BusyIndicator {
                                running: root.appController.attachmentImportInProgress
                                visible: running
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                            }
                            Label {
                                visible: root.appController.attachmentImportInProgress
                                text: root.appController.attachmentImportStatus
                                elide: Text.ElideRight
                                Layout.maximumWidth: 180
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 82
                                radius: 8
                                color: "#ffffff"
                                border.color: "#9ca3af"
                                border.width: 1
                                clip: true
                                ScrollView {
                                    id: composerScroll
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    clip: true
                                    TextArea {
                                        id: composer
                                        width: composerScroll.availableWidth
                                        placeholderText: "Message"
                                        placeholderTextColor: "#6b7280"
                                        color: "#111827"
                                        wrapMode: Text.Wrap
                                        background: null
                                    }
                                }
                            }
                            Button {
                                text: "Send"
                                onClicked: {
                                    if (root.appController.sendMessage(composer.text)) {
                                        composer.clear();
                                        root.hideKeyboardAfterSend();
                                    } else {
                                        root.showErrorIfNeeded();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Page {
                id: artifactsPage

                ListView {
                    id: artifactList
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    model: root.artifactRows
                    spacing: 8
                    delegate: Pane {
                        id: artifactDelegate
                        required property var modelData

                        width: ListView.view.width
                        Material.elevation: 0
                        background: Rectangle {
                            color: "#ffffff"
                            border.color: "#e5e7eb"
                            radius: 8
                        }
                        ColumnLayout {
                            width: parent.width
                            spacing: 6
                            Label {
                                text: artifactDelegate.modelData.summary
                                font.bold: true
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: artifactDelegate.modelData.phase + " Round " + artifactDelegate.modelData.round + "  " + artifactDelegate.modelData.createdAt
                                color: "#4b5563"
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Text {
                                text: {
                                    var content = root.appController.artifactContent(artifactDelegate.modelData.versionId);
                                    return content && content.length > 0 ? content : "_No artifact content available._";
                                }
                                textFormat: Text.MarkdownText
                                wrapMode: Text.Wrap
                                color: "#111827"
                                Layout.fillWidth: true
                                onLinkActivated: function (link) {}
                            }
                        }
                    }
                    Column {
                        anchors.centerIn: parent
                        spacing: 8
                        visible: artifactList.count === 0
                        width: parent.width * 0.86
                        Label {
                            text: "Nothing Here Yet"
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: 22
                            font.bold: true
                            color: "#6b7280"
                        }
                        Label {
                            text: "Run the table to generate content."
                            anchors.horizontalCenter: parent.horizontalCenter
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            width: parent.width
                            color: "#9ca3af"
                        }
                    }
                }
            }

            Page {
                id: logPage

                ListView {
                    id: logList
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    model: root.logRows
                    spacing: 4
                    delegate: Rectangle {
                        id: logDelegate
                        required property var modelData

                        width: ListView.view.width
                        implicitHeight: logCardContent.implicitHeight + 20
                        radius: 8
                        color: "#ffffff"
                        border.color: "#e5e7eb"
                        border.width: 1

                        Column {
                            id: logCardContent
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            Label {
                                width: parent.width
                                text: "[" + logDelegate.modelData.timestamp + "] " + logDelegate.modelData.type + (logDelegate.modelData.actorName ? " | " + logDelegate.modelData.actorName : "") + " | " + logDelegate.modelData.phase + " R" + logDelegate.modelData.round
                                color: "#6b7280"
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }

                            Label {
                                width: parent.width
                                text: logDelegate.modelData.summary
                                color: "#111827"
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }
                        }
                    }
                    Column {
                        anchors.centerIn: parent
                        spacing: 8
                        visible: logList.count === 0
                        width: parent.width * 0.86
                        Label {
                            text: "Nothing Here Yet"
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: 22
                            font.bold: true
                            color: "#6b7280"
                        }
                        Label {
                            text: "Run the table to generate content."
                            anchors.horizontalCenter: parent.horizontalCenter
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            width: parent.width
                            color: "#9ca3af"
                        }
                    }
                }
            }
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            currentIndex: root.selectedTab
            onCurrentIndexChanged: root.selectedTab = currentIndex
            TabButton {
                text: "Table"
            }
            TabButton {
                text: "Transcript"
            }
            TabButton {
                text: "Artifacts"
            }
            TabButton {
                text: "Log"
            }
        }
    }

    Dialog {
        id: seatSheet
        title: root.editingSeatIndex >= 0 ? "Seat " + (root.editingSeatIndex + 1) : "Seat"
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        width: Math.min(root.width - 32, 520)
        x: (root.width - width) / 2
        y: Math.max(24, (root.height - height) / 2)
        onAccepted: {
            var selectedModel = modelCombo.model && modelCombo.model.length > modelCombo.currentIndex ? modelCombo.model[modelCombo.currentIndex].id : "";
            if (!root.appController.saveSeat(root.editingSeatIndex, occupiedSwitch.checked, seatNameField.text, providerCombo.currentIndex, selectedModel, effortCombo.currentIndex, roleCombo.currentIndex)) {
                root.showErrorIfNeeded();
            }
        }

        ColumnLayout {
            width: parent.width
            Switch {
                id: occupiedSwitch
                text: "Seat is occupied"
            }
            TextField {
                id: seatNameField
                placeholderText: "Display name"
                Layout.fillWidth: true
                enabled: occupiedSwitch.checked
            }
            ComboBox {
                id: providerCombo
                model: ["ChatGPT", "Gemini", "Claude"]
                Layout.fillWidth: true
                enabled: occupiedSwitch.checked
                onCurrentIndexChanged: root.refreshModelCombo("")
            }
            ComboBox {
                id: modelCombo
                textRole: "displayName"
                valueRole: "id"
                Layout.fillWidth: true
                enabled: occupiedSwitch.checked
            }
            ComboBox {
                id: effortCombo
                model: ["Auto", "Light", "Balanced", "Deep"]
                Layout.fillWidth: true
                enabled: occupiedSwitch.checked
            }
            ComboBox {
                id: roleCombo
                model: ["Participant", "Final Decision Maker", "Lead Planner", "Lead Executioner", "Lead Quality Control"]
                Layout.fillWidth: true
                enabled: occupiedSwitch.checked
            }
        }
    }

    Dialog {
        id: settingsSheet
        title: "Settings"
        modal: true
        standardButtons: Dialog.Close
        parent: Overlay.overlay
        property real overlayWidth: parent && parent.width > 0 ? parent.width : root.width
        property real overlayHeight: parent && parent.height > 0 ? parent.height : root.height
        width: Math.min(overlayWidth - 32, 620)
        x: Math.round((overlayWidth - width) / 2)
        y: Math.round((overlayHeight - height) / 2)

        contentItem: Flickable {
            implicitWidth: Math.min(root.width - 96, 540)
            implicitHeight: Math.max(320, Math.min(settingsSheet.overlayHeight - 360, 560))
            contentWidth: width
            contentHeight: settingsColumn.implicitHeight + 16
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: settingsColumn
                width: parent.width
                spacing: 12

                Label {
                    text: "Provider Credentials"
                    font.pixelSize: 18
                    font.bold: true
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        text: "OpenAI"
                        font.bold: true
                    }
                    TextField {
                        id: openAiKeyField
                        placeholderText: "OpenAI API key"
                        echoMode: TextInput.PasswordEchoOnEdit
                        text: root.appController.apiKey(0)
                        Layout.fillWidth: true
                        onEditingFinished: {
                            if (!root.appController.saveApiKey(0, text))
                                root.showErrorIfNeeded();
                        }
                    }
                    Label {
                        text: root.appController.apiKeyStatus(0)
                        color: "#4b5563"
                        font.pixelSize: 12
                    }

                    Label {
                        text: "Google"
                        font.bold: true
                    }
                    TextField {
                        id: googleKeyField
                        placeholderText: "Google API key"
                        echoMode: TextInput.PasswordEchoOnEdit
                        text: root.appController.apiKey(1)
                        Layout.fillWidth: true
                        onEditingFinished: {
                            if (!root.appController.saveApiKey(1, text))
                                root.showErrorIfNeeded();
                        }
                    }
                    Label {
                        text: root.appController.apiKeyStatus(1)
                        color: "#4b5563"
                        font.pixelSize: 12
                    }

                    Label {
                        text: "Anthropic"
                        font.bold: true
                    }
                    TextField {
                        id: anthropicKeyField
                        placeholderText: "Anthropic API key"
                        echoMode: TextInput.PasswordEchoOnEdit
                        text: root.appController.apiKey(2)
                        Layout.fillWidth: true
                        onEditingFinished: {
                            if (!root.appController.saveApiKey(2, text))
                                root.showErrorIfNeeded();
                        }
                    }
                    Label {
                        text: root.appController.apiKeyStatus(2)
                        color: "#4b5563"
                        font.pixelSize: 12
                    }
                }
                Button {
                    text: "Refresh Models"
                    Layout.fillWidth: true
                    onClicked: root.saveProviderKeysAndRefresh()
                }
                Repeater {
                    model: root.modelRefreshRows
                    delegate: Label {
                        id: modelRefreshLabel
                        required property var modelData

                        text: modelRefreshLabel.modelData.message
                        color: modelRefreshLabel.modelData.success ? (modelRefreshLabel.modelData.fallback ? "#92400e" : "#166534") : "#991b1b"
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }

                Label {
                    text: "Global Hard Stops"
                    font.pixelSize: 18
                    font.bold: true
                }
                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    TextField {
                        id: maxPhaseTokens
                        placeholderText: "Max tokens per phase"
                        text: "12000"
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: maxTotalTokens
                        placeholderText: "Max total tokens"
                        text: "48000"
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: maxCost
                        placeholderText: "Max cost"
                        text: "10.00"
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: maxRounds
                        placeholderText: "Max rounds"
                        text: "6"
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: maxLoops
                        placeholderText: "Max Exec/QC loops"
                        text: "4"
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: maxPhaseSeconds
                        placeholderText: "Phase seconds"
                        text: "120"
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                    TextField {
                        id: maxSessionSeconds
                        placeholderText: "Session seconds"
                        text: "900"
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                }
                Button {
                    text: "Save Hard Stops"
                    Layout.fillWidth: true
                    onClicked: {
                        if (!root.appController.saveGlobalBudget(Number(maxPhaseTokens.text), Number(maxTotalTokens.text), Number(maxCost.text), Number(maxRounds.text), Number(maxLoops.text), Number(maxPhaseSeconds.text), Number(maxSessionSeconds.text))) {
                            root.showErrorIfNeeded();
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: attachmentDialog
        title: "Choose Attachment"
        onAccepted: {
            if (!root.appController.addAttachment(attachmentDialog.selectedFile))
                root.showErrorIfNeeded();
        }
    }

    Dialog {
        id: createTableDialog
        title: "Create Meeting Table"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Math.min(root.width - 32, 420)
        x: (root.width - width) / 2
        y: Math.max(24, (root.height - height) / 2)
        onAccepted: root.appController.createTable(createTitle.text, Number(createSeats.currentText))
        ColumnLayout {
            width: parent.width
            TextField {
                id: createTitle
                text: "New Meeting Table"
                Layout.fillWidth: true
            }
            ComboBox {
                id: createSeats
                model: ["1", "2", "3", "4", "5", "6", "7", "8"]
                currentIndex: 3
                Layout.fillWidth: true
            }
        }
    }

    Dialog {
        id: renameDialog
        title: "Rename Table"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: Math.min(root.width - 32, 420)
        x: (root.width - width) / 2
        y: Math.max(24, (root.height - height) / 2)
        onAccepted: {
            if (!root.appController.renameCurrentTable(renameField.text))
                root.showErrorIfNeeded();
        }
        TextField {
            id: renameField
            width: parent.width
        }
    }

    MessageDialog {
        id: deleteDialog
        title: "Delete Meeting Table"
        text: "Delete this table and its transcript, log, and artifact history?"
        buttons: MessageDialog.Yes | MessageDialog.No
        onButtonClicked: function (button, role) {
            if (button === MessageDialog.Yes)
                root.appController.deleteCurrentTable();
        }
    }

    MessageDialog {
        id: errorDialog
        title: "AI Meeting Table"
        buttons: MessageDialog.Ok
    }

    MessageDialog {
        id: continuationDialog
        title: "Continuation Required"
        buttons: MessageDialog.Ok
    }
}
