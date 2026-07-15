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
    minimumWidth: 320
    minimumHeight: 540
    visible: true
    title: "AI Meeting Table"
    color: backgroundColor
    font.family: uiFont
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    property var tableRows: []
    property var currentTable: ({})
    property var seatRows: []
    property var transcriptRows: []
    property var attachmentRows: []
    property var artifactRows: []
    property var logRows: []
    property var modelRefreshRows: []
    property var appearanceSettings: ({})
    property var safeguards: ({})
    property int selectedPage: 1
    property int selectedSettingsPage: 0
    property int editingSeatIndex: -1
    property var editingSeat: ({})
    property bool addingSeat: false
    readonly property var seatColorPresets: [
        { name: "Blue", value: "#4f86c6" },
        { name: "Cyan", value: "#2f9eaa" },
        { name: "Green", value: "#3f956f" },
        { name: "Amber", value: "#b98220" },
        { name: "Orange", value: "#c66a2b" },
        { name: "Red", value: "#bd5454" },
        { name: "Purple", value: "#8169b3" },
        { name: "Pink", value: "#b45582" }
    ]
    property int settingsGeneration: 0
    property string artifactPreviewTitle: ""
    property string artifactPreviewBody: ""
    property string toastMessage: ""
    property string transcriptViewTableId: ""
    property var transcriptScrollStates: ({})
    property var pendingTranscriptRestore: null
    property int transcriptRefreshGeneration: 0

    readonly property bool desktopLayout: width >= 760
    readonly property bool calmTheme: appearanceSettings.colorTheme === "Calm Workspace"
    readonly property bool systemDark: Application.styleHints.colorScheme === Qt.Dark
    readonly property bool darkMode: appearanceSettings.appearance === "Dark"
                                     || (appearanceSettings.appearance === "System" && systemDark)
    readonly property string uiFont: appearanceSettings.fontStyle === "Console"
                                     ? "Cascadia Mono"
                                     : appearanceSettings.fontStyle === "Workspace"
                                       ? "Segoe UI"
                                       : Application.font.family
    readonly property color backgroundColor: darkMode
                                                ? (calmTheme ? "#1c1a17" : "#10191e")
                                                : (calmTheme ? "#f8f5ef" : "#f2f7f8")
    readonly property color surfaceColor: darkMode
                                             ? (calmTheme ? "#29251f" : "#18262e")
                                             : (calmTheme ? "#fffdf9" : "#ffffff")
    readonly property color raisedColor: darkMode
                                            ? (calmTheme ? "#231f1b" : "#142129")
                                            : (calmTheme ? "#f1e9dd" : "#e8f0f2")
    readonly property color lineColor: darkMode
                                          ? (calmTheme ? "#5b5146" : "#35505b")
                                          : (calmTheme ? "#d8cdbc" : "#b8cdd3")
    readonly property color textColor: darkMode
                                          ? (calmTheme ? "#f1e9df" : "#dbe8ee")
                                          : (calmTheme ? "#342a1d" : "#17313a")
    readonly property color mutedColor: darkMode
                                           ? (calmTheme ? "#b9aa98" : "#90a6ae")
                                           : (calmTheme ? "#766652" : "#526d76")
    readonly property color accentColor: calmTheme ? "#a06036" : "#16866c"
    readonly property color accentInkColor: "#ffffff"
    readonly property color dangerColor: darkMode ? "#ff8a83" : "#a92f2a"
    readonly property real transcriptFollowThreshold: 64

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: accentColor
    Material.primary: accentColor
    Material.background: backgroundColor
    Material.foreground: textColor

    function refreshTables() { tableRows = appController.tables(); }
    function refreshCurrentTable() { currentTable = appController.currentTable(); }
    function refreshSeats() { seatRows = appController.seats(); }
    function refreshAttachments() { attachmentRows = appController.attachments(); }
    function refreshArtifacts() { artifactRows = appController.artifacts(); }
    function refreshLogs() { logRows = appController.logs(); }

    function refreshTranscript() {
        var nextTableId = appController.currentTableId || "";
        var previousTableId = transcriptViewTableId;
        var previousState = null;
        if (previousTableId.length > 0) {
            previousState = pendingTranscriptRestore && pendingTranscriptRestore.tableId === previousTableId
                ? pendingTranscriptRestore.state
                : TranscriptScroll.capture(transcriptList, transcriptFollowThreshold);
            transcriptScrollStates[previousTableId] = previousState;
        }
        var restoreState = nextTableId === previousTableId && previousState
            ? previousState
            : transcriptScrollStates[nextTableId] || { follow: true, contentY: 0 };
        var rows = appController.transcript();
        transcriptRefreshGeneration += 1;
        transcriptRows = rows;
        transcriptViewTableId = nextTableId;
        pendingTranscriptRestore = {
            generation: transcriptRefreshGeneration,
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

    function refreshSettings() {
        appearanceSettings = appController.settings();
        safeguards = appController.attachmentSafeguards();
        modelRefreshRows = appController.modelRefreshStatuses();
        settingsGeneration += 1;
        Qt.callLater(loadSettingsFields);
    }

    function refreshAll() {
        refreshTables();
        refreshCurrentTable();
        refreshSeats();
        refreshTranscript();
        refreshAttachments();
        refreshArtifacts();
        refreshLogs();
        refreshSettings();
    }

    function showErrorIfNeeded() {
        var message = appController.lastError();
        if (message && message.length > 0) {
            errorDialog.text = message;
            errorDialog.open();
        }
    }

    function showToast(message) {
        toastMessage = message;
        toastTimer.restart();
    }

    function sessionButtonLabel() {
        if (appController.running) return "Pause";
        if (currentTable.phase === "Paused" || currentTable.phase === "Needs continuation") return "Continue";
        return "Start";
    }

    function activateSession() {
        var ok = appController.running ? appController.pauseSession() : appController.runOrResume();
        if (!ok) showErrorIfNeeded();
    }

    function artifactType(phase) {
        if (phase === "Planning") return "Plan";
        if (phase === "Execution") return "Working artifact";
        if (phase === "Quality Control") return "Quality review";
        if (phase === "Present" || phase === "Completed") return "Final decision";
        return "Session artifact";
    }

    function openSeatEditor(index, adding) {
        editingSeatIndex = index;
        editingSeat = seatRows[index] || ({});
        addingSeat = adding;
        occupiedSwitch.checked = adding ? true : Boolean(editingSeat.occupied);
        seatNameField.text = adding ? "" : editingSeat.displayName || "";
        providerCombo.currentIndex = editingSeat.providerIndex || 0;
        effortCombo.currentIndex = editingSeat.effortIndex || 0;
        roleCombo.currentIndex = editingSeat.roleIndex || 0;
        var selectedColor = adding ? seatColorPresets[index % seatColorPresets.length].value : editingSeat.color || seatColorPresets[0].value;
        colorCombo.model = seatColorsFor(selectedColor);
        colorCombo.currentIndex = seatColorIndex(colorCombo.model, selectedColor);
        refreshModelCombo(editingSeat.modelId || "");
        seatDialog.open();
    }

    function seatColorsFor(selectedColor) {
        var options = [];
        var found = false;
        for (var i = 0; i < seatColorPresets.length; i++) {
            options.push(seatColorPresets[i]);
            found = found || seatColorPresets[i].value.toLowerCase() === selectedColor.toLowerCase();
        }
        if (!found && /^#[0-9a-fA-F]{6}$/.test(selectedColor)) {
            options.push({ name: "Custom", value: selectedColor.toLowerCase() });
        }
        return options;
    }

    function seatColorIndex(options, selectedColor) {
        for (var i = 0; i < options.length; i++) {
            if (options[i].value.toLowerCase() === selectedColor.toLowerCase()) return i;
        }
        return 0;
    }

    function openAddSeat() {
        for (var i = 0; i < seatRows.length; i++) {
            if (!seatRows[i].occupied) {
                openSeatEditor(i, true);
                return;
            }
        }
        errorDialog.text = "This table already uses all eight seats.";
        errorDialog.open();
    }

    function refreshModelCombo(selectedModelId) {
        var models = appController.modelsForProvider(providerCombo.currentIndex);
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

    function loadSettingsFields() {
        if (!appearanceSettings || appearanceSettings.appearance === undefined) return;
        appearanceCombo.currentIndex = Math.max(0, appearanceCombo.model.indexOf(appearanceSettings.appearance));
        themeCombo.currentIndex = Math.max(0, themeCombo.model.indexOf(appearanceSettings.colorTheme));
        fontCombo.currentIndex = Math.max(0, fontCombo.model.indexOf(appearanceSettings.fontStyle));
        maxPhaseTokens.text = String(appearanceSettings.maxTokensPerPhase);
        maxTotalTokens.text = String(appearanceSettings.maxTotalTokens);
        maxCost.text = String(appearanceSettings.maxTotalCost);
        maxRounds.text = String(appearanceSettings.maxRounds);
        maxLoops.text = String(appearanceSettings.maxExecQcLoops);
        maxPhaseSeconds.text = String(appearanceSettings.maxPhaseSeconds);
        maxSessionSeconds.text = String(appearanceSettings.maxSessionSeconds);
    }

    function saveAppearanceFromControls() {
        if (!appController.saveAppearance(appearanceCombo.currentText, themeCombo.currentText, fontCombo.currentText)) {
            showErrorIfNeeded();
        } else {
            refreshSettings();
        }
    }

    function saveLimits() {
        limitError.text = "";
        var phaseTokens = Number(maxPhaseTokens.text);
        var totalTokens = Number(maxTotalTokens.text);
        var cost = Number(maxCost.text);
        var rounds = Number(maxRounds.text);
        var loops = Number(maxLoops.text);
        var phaseSeconds = Number(maxPhaseSeconds.text);
        var sessionSeconds = Number(maxSessionSeconds.text);
        if (![phaseTokens, totalTokens, cost, rounds, loops, phaseSeconds, sessionSeconds].every(function (value) { return value > 0; })) {
            limitError.text = "Every hard stop must be a positive value.";
            return;
        }
        if (totalTokens < phaseTokens) {
            limitError.text = "Total tokens must be at least the per phase limit.";
            return;
        }
        if (sessionSeconds < phaseSeconds) {
            limitError.text = "Session seconds must be at least the phase limit.";
            return;
        }
        if (!appController.saveGlobalBudget(phaseTokens, totalTokens, cost, rounds, loops, phaseSeconds, sessionSeconds)) {
            limitError.text = appController.lastError();
            return;
        }
        showToast("Hard stops saved");
        refreshSettings();
    }

    function hideKeyboardAfterSend() {
        composer.focus = false;
        // qmllint disable missing-property
        Qt.inputMethod.hide();
        // qmllint enable missing-property
        Qt.callLater(function () { root.contentItem.forceActiveFocus(); });
    }

    Component.onCompleted: {
        appController.startupInitialRefreshStarted();
        refreshAll();
        appController.startupInitialRefreshCompleted();
        appController.startupPrimaryControlsReady();
    }

    Connections {
        target: root.appController
        function onStateChanged() { root.refreshCurrentTable(); }
        function onTablesChanged() { root.refreshTables(); }
        function onSeatsChanged() { root.refreshSeats(); }
        function onTranscriptChanged() { root.refreshTranscript(); }
        function onAttachmentsChanged() { root.refreshAttachments(); }
        function onArtifactsChanged() { root.refreshArtifacts(); }
        function onLogsChanged() { root.refreshLogs(); }
        function onSettingsChanged() { root.refreshSettings(); }
        function onAttachmentImportFailed() { root.showErrorIfNeeded(); }
        function onContinuationRequested(reason) {
            continuationDialog.text = reason;
            continuationDialog.open();
        }
    }

    Timer {
        id: transcriptRestoreTimer
        interval: 16
        onTriggered: {
            var pending = root.pendingTranscriptRestore;
            if (!pending || pending.generation !== root.transcriptRefreshGeneration) return;
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

    Timer { id: toastTimer; interval: 2600; onTriggered: root.toastMessage = "" }

    Shortcut {
        enabled: Qt.platform.os === "android" && (root.selectedPage === 3 || root.selectedSettingsPage !== 0)
        sequence: StandardKey.Cancel
        onActivated: {
            if (root.selectedSettingsPage !== 0) root.selectedSettingsPage = 0;
            else root.selectedPage = 1;
        }
    }

    component NavButton: Button {
        id: navButton
        required property int destination
        flat: true
        checkable: true
        checked: root.selectedPage === destination
        Layout.fillWidth: true
        implicitHeight: 48
        font.bold: checked
        onClicked: root.selectedPage = destination
        background: Rectangle {
            color: navButton.checked ? root.surfaceColor : "transparent"
            border.color: navButton.checked ? root.lineColor : "transparent"
            border.width: 1
            radius: 6
            Rectangle {
                visible: navButton.checked
                width: root.desktopLayout ? 4 : parent.width
                height: root.desktopLayout ? parent.height : 3
                anchors.left: parent.left
                anchors.top: parent.top
                color: root.accentColor
                radius: 2
            }
        }
        contentItem: Label {
            text: navButton.text
            color: navButton.checked ? root.textColor : root.mutedColor
            font: navButton.font
            horizontalAlignment: root.desktopLayout ? Text.AlignLeft : Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            leftPadding: root.desktopLayout ? 10 : 0
        }
    }

    component PanelSurface: Rectangle {
        color: root.surfaceColor
        border.color: root.lineColor
        border.width: 1
        radius: 7
    }

    header: Rectangle {
        implicitHeight: 62 + root.SafeArea.margins.top
        color: root.raisedColor
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: root.lineColor }
        RowLayout {
            anchors.fill: parent
            anchors.topMargin: root.SafeArea.margins.top
            anchors.leftMargin: 16 + root.SafeArea.margins.left
            anchors.rightMargin: 16 + root.SafeArea.margins.right
            spacing: 12
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Label {
                    text: "AI MEETING TABLE"
                    color: root.accentColor
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1.2
                }
                Label {
                    text: root.currentTable.title || "Meeting workspace"
                    color: root.textColor
                    font.pixelSize: 17
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            Label {
                text: root.currentTable.phase || "Idle"
                color: root.mutedColor
                font.pixelSize: 12
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root.SafeArea.margins.left
            Layout.rightMargin: root.SafeArea.margins.right
            spacing: 0

            Rectangle {
                visible: root.desktopLayout
                Layout.preferredWidth: 168
                Layout.fillHeight: true
                color: root.raisedColor
                border.color: root.lineColor
                border.width: 1
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5
                    NavButton { text: "Tables"; destination: 0 }
                    NavButton { text: "Session"; destination: 1 }
                    NavButton { text: "Event Log"; destination: 2 }
                    Item { Layout.fillHeight: true }
                    NavButton { text: "Settings"; destination: 3 }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.selectedPage

                ScrollView {
                    id: tablesPage
                    clip: true
                    ColumnLayout {
                        width: Math.max(0, tablesPage.availableWidth - 32)
                        x: 16
                        y: 18
                        spacing: 14
                        RowLayout {
                            Layout.fillWidth: true
                            ColumnLayout {
                                spacing: 3
                                Layout.fillWidth: true
                                Label { text: "Tables"; color: root.textColor; font.pixelSize: 30; font.bold: true }
                                Label { text: "Open an existing meeting or create a new table."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            }
                            Button { text: "New table"; onClicked: createTableDialog.open() }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: root.tableRows
                                delegate: Button {
                                    id: tableRow
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 78
                                    Accessible.name: "Open table " + modelData.title
                                    onClicked: {
                                        root.appController.selectTable(modelData.tableId);
                                        root.selectedPage = 1;
                                    }
                                    background: Rectangle {
                                        color: tableRow.modelData.selected ? root.raisedColor : root.surfaceColor
                                        border.color: tableRow.modelData.selected ? root.accentColor : root.lineColor
                                        border.width: tableRow.modelData.selected ? 2 : 1
                                        radius: 7
                                    }
                                    contentItem: RowLayout {
                                        spacing: 12
                                        ColumnLayout {
                                            spacing: 4
                                            Layout.fillWidth: true
                                            Label {
                                                text: (tableRow.modelData.pinned ? "Pinned | " : "") + tableRow.modelData.title
                                                color: root.textColor
                                                font.bold: true
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: tableRow.modelData.phase + " | Round " + tableRow.modelData.round + " | " + tableRow.modelData.updatedAt
                                                color: root.mutedColor
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                        Label { text: tableRow.modelData.transcriptCount + " messages"; color: root.mutedColor; visible: root.width >= 520 }
                                    }
                                }
                            }
                            Label {
                                visible: root.tableRows.length === 0
                                text: "No meeting tables yet. Create one to configure seats and begin a session."
                                color: root.mutedColor
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                                padding: 24
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Button { text: "Duplicate"; enabled: Boolean(root.currentTable.tableId); onClicked: { if (!root.appController.duplicateCurrentTable()) root.showErrorIfNeeded(); } }
                            Button { text: root.currentTable.pinned ? "Unpin" : "Pin"; enabled: Boolean(root.currentTable.tableId); onClicked: root.appController.togglePinCurrentTable() }
                            Button { text: "Rename"; enabled: Boolean(root.currentTable.tableId); onClicked: { renameField.text = root.currentTable.title || ""; renameDialog.open(); } }
                            Button { text: "Delete"; enabled: Boolean(root.currentTable.tableId); Material.foreground: root.dangerColor; onClicked: deleteDialog.open() }
                        }
                        Item { Layout.preferredHeight: 20 }
                    }
                }

                ScrollView {
                    id: sessionPage
                    clip: true
                    ColumnLayout {
                        id: sessionContent
                        width: Math.max(0, sessionPage.availableWidth - 28)
                        x: 14
                        y: 16
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { text: "LIVE SESSION"; color: root.accentColor; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.1 }
                                Label { text: root.currentTable.title || "No table selected"; color: root.textColor; font.pixelSize: 27; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                            Flow {
                                spacing: 7
                                Button {
                                    text: root.sessionButtonLabel()
                                    Accessible.name: text + " session"
                                    font.bold: true
                                    Material.background: root.accentColor
                                    Material.foreground: root.accentInkColor
                                    onClicked: root.activateSession()
                                }
                                Button {
                                    text: "Stop"
                                    Accessible.name: "Stop session"
                                    enabled: root.appController.running || root.currentTable.phase === "Paused" || root.currentTable.phase === "Needs continuation"
                                    onClicked: { if (!root.appController.stopSession()) root.showErrorIfNeeded(); }
                                }
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: root.width >= 900 ? 5 : 2
                            rowSpacing: 1
                            columnSpacing: 1
                            Repeater {
                                model: [
                                    { label: "Status", value: root.currentTable.phase || "Idle" },
                                    { label: "Elapsed", value: root.currentTable.elapsed || "00:00" },
                                    { label: "Round", value: String(root.currentTable.round || 0) },
                                    { label: "Tokens", value: (root.currentTable.usageEstimated ? "Approx. " : "") + (root.currentTable.usedTokens || 0) + " / " + (root.currentTable.maxTokens || 0) },
                                    { label: root.currentTable.costEstimateComplete === false ? "Cost estimate" : "Cost", value: root.currentTable.costEstimateComplete === false ? "Unavailable / $" + Number(root.currentTable.maxCost || 0).toFixed(2) : "$" + Number(root.currentTable.usedCost || 0).toFixed(2) + " / $" + Number(root.currentTable.maxCost || 0).toFixed(2) }
                                ]
                                delegate: Rectangle {
                                    id: metric
                                    required property int index
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.columnSpan: metric.index === 4 && root.width < 900 ? 2 : 1
                                    Layout.minimumWidth: 110
                                    Layout.preferredHeight: 62
                                    color: root.raisedColor
                                    border.color: root.lineColor
                                    border.width: 1
                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 9
                                        spacing: 3
                                        Label { text: metric.modelData.label; color: root.mutedColor; font.pixelSize: 11 }
                                        Label { text: metric.modelData.value; color: root.textColor; font.bold: true; elide: Text.ElideRight; width: parent.width }
                                    }
                                }
                            }
                        }

                        GridLayout {
                            id: sessionGrid
                            Layout.fillWidth: true
                            columns: sessionContent.width >= 980 ? 2 : 1
                            columnSpacing: 12
                            rowSpacing: 12

                            PanelSurface {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 700
                                Layout.preferredHeight: Math.max(560, Math.min(690, root.height - 180))
                                Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 0
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 52
                                        Layout.leftMargin: 14
                                        Layout.rightMargin: 8
                                        Label { text: "Transcript"; color: root.textColor; font.pixelSize: 17; font.bold: true; Layout.fillWidth: true }
                                        Button {
                                            text: "Copy full transcript"
                                            flat: true
                                            implicitHeight: 40
                                            enabled: root.transcriptRows.length > 0
                                            Accessible.name: "Copy full transcript"
                                            onClicked: {
                                                if (root.appController.copyFullTranscript()) root.showToast("Transcript copied");
                                                else root.showErrorIfNeeded();
                                            }
                                        }
                                    }
                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.lineColor }
                                    ListView {
                                        id: transcriptList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.leftMargin: 10
                                        Layout.rightMargin: 10
                                        clip: true
                                        spacing: 0
                                        model: root.transcriptRows
                                        delegate: Rectangle {
                                            id: messageRow
                                            required property var modelData
                                            width: transcriptList.width
                                            implicitHeight: messageLayout.implicitHeight + 26
                                            color: messageRow.modelData.isDecision ? root.raisedColor : "transparent"
                                            border.color: root.lineColor
                                            border.width: 0
                                            Rectangle { width: 4; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; color: messageRow.modelData.color; radius: 2 }
                                            ColumnLayout {
                                                id: messageLayout
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.margins: 12
                                                anchors.leftMargin: 16
                                                spacing: 9
                                                GridLayout {
                                                    Layout.fillWidth: true
                                                    columns: width >= 360 ? 2 : 1
                                                    columnSpacing: 12
                                                    rowSpacing: 3
                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 2
                                                        Label { text: messageRow.modelData.speaker; color: root.textColor; font.pixelSize: 14; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                                        Label { text: messageRow.modelData.role; color: root.mutedColor; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                                    }
                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 2
                                                        Label { text: messageRow.modelData.timestamp; color: root.textColor; font.pixelSize: 14; font.bold: true; horizontalAlignment: messageLayout.width >= 360 ? Text.AlignRight : Text.AlignLeft; Layout.fillWidth: true }
                                                        Label { text: messageRow.modelData.model; color: root.mutedColor; font.pixelSize: 12; horizontalAlignment: messageLayout.width >= 360 ? Text.AlignRight : Text.AlignLeft; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                                                    }
                                                }
                                                TextEdit {
                                                    text: messageRow.modelData.content
                                                    textFormat: TextEdit.MarkdownText
                                                    wrapMode: TextEdit.Wrap
                                                    color: root.textColor
                                                    readOnly: true
                                                    selectByMouse: true
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: contentHeight
                                                    Accessible.name: "Transcript message from " + messageRow.modelData.speaker
                                                    onLinkActivated: function (link) {}
                                                }
                                            }
                                            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: root.lineColor }
                                        }
                                        Label {
                                            anchors.centerIn: parent
                                            visible: transcriptList.count === 0
                                            width: Math.min(parent.width - 32, 420)
                                            text: "No transcript yet\n\nAdd a message, configure the seats, then start the table."
                                            color: root.mutedColor
                                            horizontalAlignment: Text.AlignHCenter
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.lineColor }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 12
                                        Layout.rightMargin: 12
                                        Layout.topMargin: 10
                                        Layout.bottomMargin: 12
                                        spacing: 7
                                        Label { text: "Message to the table"; color: root.textColor; font.bold: true }
                                        ScrollView {
                                            id: composerScroll
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: Math.min(176, Math.max(104, composer.contentHeight + 24))
                                            clip: true
                                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                            ScrollBar.vertical.policy: composer.contentHeight + 24 > composerScroll.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                                            TextArea {
                                                id: composer
                                                width: composerScroll.availableWidth
                                                implicitHeight: Math.max(composerScroll.availableHeight, contentHeight + topPadding + bottomPadding)
                                                placeholderText: "Add instructions, context, or a follow up question"
                                                wrapMode: TextEdit.WrapAnywhere
                                                color: root.textColor
                                                selectByMouse: true
                                                Accessible.name: "Message to the table"
                                                background: Rectangle { color: root.backgroundColor; border.color: composer.activeFocus ? root.accentColor : root.lineColor; border.width: composer.activeFocus ? 2 : 1; radius: 6 }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Button {
                                                text: root.appController.attachmentImportInProgress ? "Cancel import" : "Add attachment"
                                                implicitHeight: 40
                                                onClicked: root.appController.attachmentImportInProgress ? root.appController.cancelAttachmentImport() : attachmentDialog.open()
                                            }
                                            Label { visible: root.appController.attachmentImportInProgress; text: root.appController.attachmentImportStatus; color: root.mutedColor; elide: Text.ElideRight; Layout.fillWidth: true }
                                            Item { visible: !root.appController.attachmentImportInProgress; Layout.fillWidth: true }
                                            Button {
                                                text: "Send"
                                                implicitHeight: 40
                                                font.bold: true
                                                onClicked: {
                                                    if (root.appController.sendMessage(composer.text)) {
                                                        composer.clear();
                                                        root.hideKeyboardAfterSend();
                                                    } else root.showErrorIfNeeded();
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            GridLayout {
                                id: sidePanels
                                Layout.fillWidth: true
                                Layout.preferredWidth: 340
                                Layout.minimumWidth: sessionGrid.columns === 2 ? 300 : 0
                                Layout.alignment: Qt.AlignTop
                                columns: sessionGrid.columns === 1 && sessionContent.width >= 650 ? 2 : 1
                                columnSpacing: 12
                                rowSpacing: 12

                                PanelSurface {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    implicitHeight: seatsPanelContent.implicitHeight + 28
                                    ColumnLayout {
                                        id: seatsPanelContent
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 10
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label { text: "Seats"; color: root.textColor; font.bold: true; font.pixelSize: 16; Layout.fillWidth: true }
                                            Label { text: root.seatRows.filter(function (seat) { return seat.occupied; }).length + " occupied"; color: root.mutedColor; font.pixelSize: 11 }
                                        }
                                        Flow {
                                            id: seatFlow
                                            Layout.fillWidth: true
                                            spacing: 8
                                            Repeater {
                                                model: root.seatRows
                                                delegate: Button {
                                                    id: seatCard
                                                    required property int index
                                                    required property var modelData
                                                    visible: modelData.occupied
                                                    width: visible ? (seatFlow.width >= 280 ? (seatFlow.width - 8) / 2 : seatFlow.width) : 0
                                                    height: visible ? 102 : 0
                                                    Accessible.name: "Configure " + modelData.displayName
                                                    onClicked: root.openSeatEditor(index, false)
                                                    background: Rectangle {
                                                        color: root.raisedColor
                                                        border.color: seatCard.modelData.active ? seatCard.modelData.color : root.lineColor
                                                        border.width: seatCard.modelData.active ? 2 : 1
                                                        radius: 6
                                                        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 5; color: seatCard.modelData.color; radius: 2 }
                                                    }
                                                    contentItem: Column {
                                                        leftPadding: 7
                                                        spacing: 4
                                                        Label { width: parent.width - 7; text: seatCard.modelData.displayName; color: root.textColor; font.bold: true; elide: Text.ElideRight }
                                                        Label { width: parent.width - 7; text: seatCard.modelData.role; color: root.mutedColor; font.pixelSize: 11; elide: Text.ElideRight }
                                                        Label { width: parent.width - 7; text: seatCard.modelData.provider + " | " + seatCard.modelData.model; color: root.mutedColor; font.pixelSize: 11; wrapMode: Text.WrapAnywhere; maximumLineCount: 2 }
                                                    }
                                                }
                                            }
                                            Button {
                                                id: addSeatTile
                                                width: seatFlow.width >= 280 ? (seatFlow.width - 8) / 2 : seatFlow.width
                                                height: 102
                                                text: "Add Seat"
                                                Accessible.name: "Add Seat"
                                                font.bold: true
                                                onClicked: root.openAddSeat()
                                                background: Rectangle { color: "transparent"; border.color: root.accentColor; border.width: 1; radius: 6 }
                                                contentItem: Label { text: addSeatTile.text; color: root.accentColor; font: addSeatTile.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            }
                                        }
                                    }
                                }

                                PanelSurface {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    implicitHeight: attachmentPanelContent.implicitHeight + 28
                                    ColumnLayout {
                                        id: attachmentPanelContent
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 8
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label { text: "Attachments"; color: root.textColor; font.bold: true; font.pixelSize: 16; Layout.fillWidth: true }
                                        }
                                        Label { visible: root.attachmentRows.length === 0 && !root.appController.attachmentImportInProgress; text: "Use the attachment button beside the message composer."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                        Repeater {
                                            model: root.attachmentRows
                                            delegate: RowLayout {
                                                id: attachmentRow
                                                required property var modelData
                                                Layout.fillWidth: true
                                                ColumnLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 2
                                                    Label { text: attachmentRow.modelData.displayName; color: root.textColor; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                                    Label { text: attachmentRow.modelData.size + " | " + attachmentRow.modelData.mimeType; color: root.mutedColor; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                                                }
                                                Button { text: "Open"; flat: true; enabled: attachmentRow.modelData.available; Accessible.name: "Open attachment " + attachmentRow.modelData.displayName; onClicked: { if (!root.appController.openAttachment(attachmentRow.modelData.attachmentId)) root.showErrorIfNeeded(); } }
                                                Button { text: "Remove"; flat: true; Accessible.name: "Remove attachment " + attachmentRow.modelData.displayName; onClicked: { if (!root.appController.removeAttachment(attachmentRow.modelData.attachmentId)) root.showErrorIfNeeded(); } }
                                            }
                                        }
                                        ProgressBar { visible: root.appController.attachmentImportInProgress; indeterminate: true; Layout.fillWidth: true }
                                        Label { visible: root.appController.attachmentImportInProgress; text: root.appController.attachmentImportStatus; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    }
                                }

                                PanelSurface {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    implicitHeight: generationPanelContent.implicitHeight + 28
                                    ColumnLayout {
                                        id: generationPanelContent
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 9
                                        Label { text: "Generation"; color: root.textColor; font.bold: true; font.pixelSize: 16 }
                                        Label { text: root.appController.running ? "Session is running" : root.currentTable.phase || "Idle"; color: root.textColor; font.bold: true }
                                        Label { text: root.appController.running ? "Provider calls follow the configured seat order and hard stops." : "Generation begins when the table starts."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                        ProgressBar {
                                            Layout.fillWidth: true
                                            from: 0
                                            to: Math.max(1, Number(root.currentTable.maxTokens || 1))
                                            value: Number(root.currentTable.usedTokens || 0)
                                            indeterminate: root.appController.running && Number(root.currentTable.maxTokens || 0) === 0
                                        }
                                        Label { text: (root.currentTable.usageEstimated ? "Approximately " : "") + (root.currentTable.usedTokens || 0) + " tokens used"; color: root.mutedColor; font.pixelSize: 11 }
                                    }
                                }

                                PanelSurface {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop
                                    implicitHeight: artifactsPanelContent.implicitHeight + 28
                                    ColumnLayout {
                                        id: artifactsPanelContent
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 8
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label { text: "Artifacts"; color: root.textColor; font.bold: true; font.pixelSize: 16; Layout.fillWidth: true }
                                            Label { text: String(root.artifactRows.length); color: root.mutedColor }
                                        }
                                        Label { visible: root.artifactRows.length === 0; text: "No generated artifacts yet."; color: root.mutedColor }
                                        Repeater {
                                            model: root.artifactRows
                                            delegate: Button {
                                                id: artifactRow
                                                required property var modelData
                                                Layout.fillWidth: true
                                                implicitHeight: 62
                                                Accessible.name: "Open " + root.artifactType(modelData.phase) + " artifact"
                                                onClicked: {
                                                    root.artifactPreviewTitle = root.artifactType(modelData.phase) + " | " + modelData.createdAt;
                                                    root.artifactPreviewBody = root.appController.artifactContent(modelData.versionId) || "Artifact content is unavailable.";
                                                    artifactDialog.open();
                                                }
                                                background: Rectangle { color: root.raisedColor; border.color: root.lineColor; border.width: 1; radius: 5 }
                                                contentItem: Column {
                                                    spacing: 3
                                                    Label { width: parent.width; text: root.artifactType(artifactRow.modelData.phase); color: root.textColor; font.bold: true; elide: Text.ElideRight }
                                                    Label { width: parent.width; text: artifactRow.modelData.summary + " | " + artifactRow.modelData.createdAt; color: root.mutedColor; font.pixelSize: 11; elide: Text.ElideRight }
                                                }
                                            }
                                        }
                                        Button {
                                            text: root.artifactRows.length === 0 ? "Start session to generate" : "Continue session"
                                            flat: true
                                            Layout.fillWidth: true
                                            onClicked: root.activateSession()
                                        }
                                    }
                                }
                            }
                        }
                        Item { Layout.preferredHeight: 18 }
                    }
                }

                ScrollView {
                    id: eventLogPage
                    clip: true
                    ColumnLayout {
                        width: Math.max(0, eventLogPage.availableWidth - 32)
                        x: 16
                        y: 18
                        spacing: 12
                        Label { text: "Event Log"; color: root.textColor; font.pixelSize: 30; font.bold: true }
                        Label { text: "Session events, provider failures, retries, limits, and decisions."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: root.logRows
                                delegate: PanelSurface {
                                    id: logRow
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: logRowContent.implicitHeight + 22
                                    ColumnLayout {
                                        id: logRowContent
                                        anchors.fill: parent
                                        anchors.margins: 11
                                        spacing: 5
                                        Label { text: logRow.modelData.timestamp + " | " + logRow.modelData.type + " | " + logRow.modelData.phase + " R" + logRow.modelData.round; color: root.mutedColor; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                        Label { text: logRow.modelData.summary; color: root.textColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    }
                                }
                            }
                            Label { visible: root.logRows.length === 0; text: "No events recorded yet."; color: root.mutedColor; padding: 24 }
                        }
                        Item { Layout.preferredHeight: 18 }
                    }
                }

                ScrollView {
                    id: settingsPage
                    clip: true
                    ColumnLayout {
                        width: Math.max(0, settingsPage.availableWidth - 32)
                        x: 16
                        y: 18
                        spacing: 14
                        Label { text: "Settings"; color: root.textColor; font.pixelSize: 30; font.bold: true }
                        Label { text: "Provider credentials, model catalogs, hard stops, attachment safeguards, and appearance."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        ComboBox {
                            visible: !root.desktopLayout
                            Layout.fillWidth: true
                            model: ["Provider credentials", "Models", "Hard stops", "Attachments", "Appearance"]
                            currentIndex: root.selectedSettingsPage
                            onActivated: root.selectedSettingsPage = currentIndex
                            Accessible.name: "Settings category"
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: 14
                            ColumnLayout {
                                visible: root.desktopLayout
                                Layout.preferredWidth: 190
                                Layout.alignment: Qt.AlignTop
                                spacing: 4
                                Repeater {
                                    model: ["Provider credentials", "Models", "Hard stops", "Attachments", "Appearance"]
                                    delegate: Button {
                                        required property int index
                                        required property string modelData
                                        text: modelData
                                        flat: true
                                        checkable: true
                                        checked: root.selectedSettingsPage === index
                                        Layout.fillWidth: true
                                        onClicked: root.selectedSettingsPage = index
                                    }
                                }
                            }
                            StackLayout {
                                id: settingsContent
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignTop
                                currentIndex: root.selectedSettingsPage

                                ColumnLayout {
                                    spacing: 12
                                    Label { text: "Provider credentials"; color: root.textColor; font.pixelSize: 20; font.bold: true }
                                    Label { text: "Keys are stored by the device credential store. They are only sent to the selected provider when a session uses that provider."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    Flow {
                                        id: providerFlow
                                        Layout.fillWidth: true
                                        spacing: 10
                                        Repeater {
                                            model: ["OpenAI", "Gemini", "Anthropic"]
                                            delegate: PanelSurface {
                                                id: providerCard
                                                required property int index
                                                required property string modelData
                                                property bool reveal: false
                                                width: providerFlow.width >= 780 ? (providerFlow.width - 20) / 3 : providerFlow.width
                                                height: 230
                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 12
                                                    spacing: 8
                                                    Label { text: providerCard.modelData; color: root.textColor; font.bold: true; font.pixelSize: 16 }
                                                    Label { text: "API key"; color: root.textColor }
                                                    TextField {
                                                        id: keyField
                                                        Layout.fillWidth: true
                                                        echoMode: providerCard.reveal ? TextInput.Normal : TextInput.Password
                                                        placeholderText: "Enter a new API key"
                                                        Accessible.name: providerCard.modelData + " API key"
                                                    }
                                                    Label {
                                                        text: {
                                                            root.settingsGeneration;
                                                            return root.appController.apiKeyStatus(providerCard.index);
                                                        }
                                                        color: root.mutedColor
                                                        font.pixelSize: 11
                                                    }
                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        Button { text: providerCard.reveal ? "Hide" : "Show"; onClicked: providerCard.reveal = !providerCard.reveal }
                                                        Item { Layout.fillWidth: true }
                                                        Button {
                                                            text: "Clear"
                                                            onClicked: {
                                                                if (root.appController.saveApiKey(providerCard.index, "")) {
                                                                    keyField.clear();
                                                                    root.refreshSettings();
                                                                    root.showToast(providerCard.modelData + " key cleared");
                                                                } else root.showErrorIfNeeded();
                                                            }
                                                        }
                                                        Button {
                                                            text: "Save"
                                                            font.bold: true
                                                            enabled: keyField.text.length > 0
                                                            onClicked: {
                                                                if (root.appController.saveApiKey(providerCard.index, keyField.text)) {
                                                                    root.refreshSettings();
                                                                    root.showToast(providerCard.modelData + " key saved");
                                                                } else root.showErrorIfNeeded();
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    spacing: 12
                                    Label { text: "Models"; color: root.textColor; font.pixelSize: 20; font.bold: true }
                                    Label { text: "Provider, model, effort, and role are configured per seat. Refresh the provider catalogs after updating credentials."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    Button { text: "Refresh model catalogs"; onClicked: root.appController.refreshModels() }
                                    Repeater {
                                        model: root.modelRefreshRows
                                        delegate: PanelSurface {
                                            id: modelStatusRow
                                            required property var modelData
                                            Layout.fillWidth: true
                                            implicitHeight: 58
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                Label { text: modelStatusRow.modelData.provider || "Provider"; color: root.textColor; font.bold: true; Layout.fillWidth: true }
                                                Label { text: modelStatusRow.modelData.message || "Not refreshed"; color: root.mutedColor; wrapMode: Text.Wrap }
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    spacing: 10
                                    Label { text: "Global hard stops"; color: root.textColor; font.pixelSize: 20; font.bold: true }
                                    Label { text: "These mandatory limits stop provider work. Total tokens cannot be lower than per phase tokens, and session seconds cannot be lower than phase seconds."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: settingsContent.width >= 620 ? 2 : 1
                                        columnSpacing: 10
                                        rowSpacing: 8
                                        Label { text: "Max tokens per phase"; color: root.textColor }
                                        TextField { id: maxPhaseTokens; Layout.fillWidth: true; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1 } }
                                        Label { text: "Max total tokens"; color: root.textColor }
                                        TextField { id: maxTotalTokens; Layout.fillWidth: true; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1 } }
                                        Label { text: "Max total cost (USD)"; color: root.textColor }
                                        TextField { id: maxCost; Layout.fillWidth: true; inputMethodHints: Qt.ImhFormattedNumbersOnly; validator: DoubleValidator { bottom: 0.01; decimals: 2 } }
                                        Label { text: "Max rounds"; color: root.textColor }
                                        TextField { id: maxRounds; Layout.fillWidth: true; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1 } }
                                        Label { text: "Max Execution / QC loops"; color: root.textColor }
                                        TextField { id: maxLoops; Layout.fillWidth: true; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1 } }
                                        Label { text: "Max phase seconds"; color: root.textColor }
                                        TextField { id: maxPhaseSeconds; Layout.fillWidth: true; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1 } }
                                        Label { text: "Max session seconds"; color: root.textColor }
                                        TextField { id: maxSessionSeconds; Layout.fillWidth: true; inputMethodHints: Qt.ImhDigitsOnly; validator: IntValidator { bottom: 1 } }
                                    }
                                    Label { id: limitError; color: root.dangerColor; wrapMode: Text.Wrap; Layout.fillWidth: true; Accessible.role: Accessible.AlertMessage }
                                    Button { text: "Save hard stops"; font.bold: true; onClicked: root.saveLimits() }
                                }

                                ColumnLayout {
                                    spacing: 12
                                    Label { text: "Attachment safeguards"; color: root.textColor; font.pixelSize: 20; font.bold: true }
                                    Label { text: "These fixed production safeguards are enforced during import and cannot be weakened here."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    Repeater {
                                        model: [
                                            { label: "Maximum attachment size", value: (root.safeguards.maximumAttachmentMiB || 0) + " MiB hard stop" },
                                            { label: "Free space reserve", value: (root.safeguards.freeSpaceReserveMiB || 0) + " MiB required" },
                                            { label: "No progress timeout", value: (root.safeguards.noProgressTimeoutSeconds || 0) + " seconds" }
                                        ]
                                        delegate: PanelSurface {
                                            id: safeguardRow
                                            required property var modelData
                                            Layout.fillWidth: true
                                            implicitHeight: 62
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 11
                                                Label { text: safeguardRow.modelData.label; color: root.textColor; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
                                                Label { text: safeguardRow.modelData.value; color: root.mutedColor; horizontalAlignment: Text.AlignRight; wrapMode: Text.Wrap }
                                            }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    spacing: 10
                                    Label { text: "Appearance"; color: root.textColor; font.pixelSize: 20; font.bold: true }
                                    Label { text: "Appearance, color theme, and font style are independent and persist across launches."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    Label { text: "Appearance"; color: root.textColor }
                                    ComboBox { id: appearanceCombo; Layout.fillWidth: true; model: ["Light", "Dark", "System"]; onActivated: root.saveAppearanceFromControls() }
                                    Label { text: "Color theme"; color: root.textColor }
                                    ComboBox { id: themeCombo; Layout.fillWidth: true; model: ["Signal Session", "Calm Workspace"]; onActivated: root.saveAppearanceFromControls() }
                                    Label { text: "Font style"; color: root.textColor }
                                    ComboBox { id: fontCombo; Layout.fillWidth: true; model: ["System", "Workspace", "Console"]; onActivated: root.saveAppearanceFromControls() }
                                    PanelSurface {
                                        Layout.fillWidth: true
                                        implicitHeight: 92
                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 14
                                            spacing: 6
                                            Label { text: "Live preview"; color: root.textColor; font.bold: true }
                                            Label { width: parent.width; text: (root.appearanceSettings.appearance || "System") + " | " + (root.appearanceSettings.colorTheme || "Signal Session") + " | " + (root.appearanceSettings.fontStyle || "System"); color: root.mutedColor; wrapMode: Text.Wrap }
                                        }
                                    }
                                }
                            }
                        }
                        Item { Layout.preferredHeight: 18 }
                    }
                }
            }
        }

        Rectangle {
            visible: !root.desktopLayout
            Layout.fillWidth: true
            Layout.preferredHeight: 64 + root.SafeArea.margins.bottom
            color: root.raisedColor
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: root.lineColor }
            RowLayout {
                anchors.fill: parent
                anchors.topMargin: 5
                anchors.bottomMargin: 5 + root.SafeArea.margins.bottom
                anchors.leftMargin: 5 + root.SafeArea.margins.left
                anchors.rightMargin: 5 + root.SafeArea.margins.right
                spacing: 4
                NavButton { text: "Tables"; destination: 0 }
                NavButton { text: "Session"; destination: 1 }
                NavButton { text: "Event Log"; destination: 2 }
                NavButton { text: "Settings"; destination: 3 }
            }
        }
    }

    Rectangle {
        visible: root.toastMessage.length > 0
        z: 20
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.desktopLayout ? 18 + root.SafeArea.margins.bottom : 78 + root.SafeArea.margins.bottom
        width: Math.min(parent.width - 24, toastLabel.implicitWidth + 32)
        height: 48
        radius: 6
        color: root.raisedColor
        border.color: root.accentColor
        border.width: 2
        Label { id: toastLabel; anchors.centerIn: parent; text: root.toastMessage; color: root.textColor; Accessible.role: Accessible.AlertMessage }
    }

    Dialog {
        id: seatDialog
        title: root.addingSeat ? "Add Seat" : "Configure seat"
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        width: Math.min(root.width - root.SafeArea.margins.left - root.SafeArea.margins.right - 24, 560)
        height: Math.min(root.height - root.SafeArea.margins.top - root.SafeArea.margins.bottom - 36, 720)
        x: root.SafeArea.margins.left + (root.width - root.SafeArea.margins.left - root.SafeArea.margins.right - width) / 2
        y: root.SafeArea.margins.top + Math.max(18, (root.height - root.SafeArea.margins.top - root.SafeArea.margins.bottom - height) / 2)
        onAccepted: {
            var selectedModel = modelCombo.model && modelCombo.model.length > modelCombo.currentIndex ? modelCombo.model[modelCombo.currentIndex].id : "";
            if (!root.appController.saveSeat(root.editingSeatIndex, root.addingSeat ? true : occupiedSwitch.checked, seatNameField.text, providerCombo.currentIndex, selectedModel, effortCombo.currentIndex, roleCombo.currentIndex, colorCombo.currentValue)) root.showErrorIfNeeded();
        }
        contentItem: ScrollView {
            id: seatScroll
            clip: true
            ColumnLayout {
                width: seatScroll.availableWidth
                spacing: 9
                Switch { id: occupiedSwitch; visible: !root.addingSeat; text: "Seat is occupied" }
                Label { text: "Seat name"; color: root.textColor }
                TextField { id: seatNameField; Layout.fillWidth: true; enabled: root.addingSeat || occupiedSwitch.checked; placeholderText: "Display name"; Accessible.name: "Seat name" }
                Label { text: "Role"; color: root.textColor }
                ComboBox { id: roleCombo; Layout.fillWidth: true; enabled: root.addingSeat || occupiedSwitch.checked; model: ["Participant", "Final Decision Maker", "Lead Planner", "Lead Executioner", "Lead Quality Control"] }
                Label { text: "Provider"; color: root.textColor }
                ComboBox { id: providerCombo; Layout.fillWidth: true; enabled: root.addingSeat || occupiedSwitch.checked; model: ["OpenAI", "Gemini", "Anthropic"]; onActivated: root.refreshModelCombo("") }
                Label { text: "Model"; color: root.textColor }
                ComboBox { id: modelCombo; Layout.fillWidth: true; enabled: root.addingSeat || occupiedSwitch.checked; textRole: "displayName"; valueRole: "id" }
                Label { text: "Effort"; color: root.textColor }
                ComboBox { id: effortCombo; Layout.fillWidth: true; enabled: root.addingSeat || occupiedSwitch.checked; model: ["Auto", "Light", "Balanced", "Deep"] }
                Label { text: "Seat color"; color: root.textColor }
                RowLayout {
                    Layout.fillWidth: true
                    Rectangle { Layout.preferredWidth: 42; Layout.preferredHeight: 42; radius: 6; color: colorCombo.currentValue || root.seatColorPresets[0].value; border.color: root.lineColor; border.width: 1 }
                    ComboBox {
                        id: colorCombo
                        Layout.fillWidth: true
                        textRole: "name"
                        valueRole: "value"
                        Accessible.name: "Seat color"
                        delegate: ItemDelegate {
                            id: colorChoice
                            required property int index
                            required property var modelData
                            width: colorCombo.width
                            highlighted: colorCombo.highlightedIndex === index
                            contentItem: RowLayout {
                                spacing: 10
                                Rectangle { Layout.preferredWidth: 24; Layout.preferredHeight: 24; radius: 4; color: colorChoice.modelData.value; border.color: root.lineColor; border.width: 1 }
                                Label { text: colorChoice.modelData.name; color: root.textColor; Layout.fillWidth: true }
                            }
                        }
                    }
                }
                Label { text: "Seat color appears as an accent and does not replace the speaker name or role."; color: root.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
        }
    }

    Dialog {
        id: artifactDialog
        title: root.artifactPreviewTitle
        modal: true
        standardButtons: Dialog.Close
        width: Math.min(root.width - root.SafeArea.margins.left - root.SafeArea.margins.right - 24, 760)
        height: Math.min(root.height - root.SafeArea.margins.top - root.SafeArea.margins.bottom - 36, 720)
        x: root.SafeArea.margins.left + (root.width - root.SafeArea.margins.left - root.SafeArea.margins.right - width) / 2
        y: root.SafeArea.margins.top + Math.max(18, (root.height - root.SafeArea.margins.top - root.SafeArea.margins.bottom - height) / 2)
        contentItem: ScrollView {
            clip: true
            TextArea {
                text: root.artifactPreviewBody
                textFormat: TextEdit.MarkdownText
                readOnly: true
                wrapMode: TextArea.Wrap
                color: root.textColor
                Accessible.name: "Artifact preview"
            }
        }
    }

    Dialog {
        id: createTableDialog
        title: "Create meeting table"
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        width: Math.min(root.width - 24, 460)
        x: (root.width - width) / 2
        onAccepted: { if (!root.appController.createTable(createTitle.text, Number(createSeats.currentText))) root.showErrorIfNeeded(); else root.selectedPage = 1; }
        ColumnLayout {
            width: parent.width
            Label { text: "Table title"; color: root.textColor }
            TextField { id: createTitle; Layout.fillWidth: true; text: "New Meeting Table"; selectByMouse: true }
            Label { text: "Initial seats"; color: root.textColor }
            ComboBox { id: createSeats; Layout.fillWidth: true; model: ["1", "2", "3", "4", "5", "6", "7", "8"]; currentIndex: 3 }
        }
    }

    Dialog {
        id: renameDialog
        title: "Rename table"
        modal: true
        standardButtons: Dialog.Save | Dialog.Cancel
        width: Math.min(root.width - 24, 460)
        x: (root.width - width) / 2
        onAccepted: { if (!root.appController.renameCurrentTable(renameField.text)) root.showErrorIfNeeded(); }
        TextField { id: renameField; width: parent.width; Accessible.name: "Table title" }
    }

    MessageDialog {
        id: deleteDialog
        title: "Delete table"
        text: "Delete this meeting table and its stored session data?"
        buttons: MessageDialog.Yes | MessageDialog.No
        onButtonClicked: function (button) { if (button === MessageDialog.Yes && !root.appController.deleteCurrentTable()) root.showErrorIfNeeded(); }
    }

    MessageDialog { id: errorDialog; title: "Action could not be completed"; buttons: MessageDialog.Ok }
    MessageDialog { id: continuationDialog; title: "Continuation required"; buttons: MessageDialog.Ok }

    FileDialog {
        id: attachmentDialog
        title: "Select attachment"
        fileMode: FileDialog.OpenFile
        onAccepted: { if (!root.appController.addAttachment(selectedFile)) root.showErrorIfNeeded(); }
    }
}
