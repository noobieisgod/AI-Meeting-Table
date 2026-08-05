import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material

ScrollView {
    id: page

    required property var appRoot

    signal newTableRequested()
    signal renameRequested()
    signal deleteRequested()

    clip: true

    ColumnLayout {
        width: Math.max(0, page.availableWidth - 32)
        x: 16
        y: 18
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 3
                Layout.fillWidth: true
                Label { text: "Tables"; color: page.appRoot.textColor; font.pixelSize: 30; font.bold: true }
                Label { text: "Open an existing meeting or create a new table."; color: page.appRoot.mutedColor; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
            Button { text: "New table"; onClicked: page.newTableRequested() }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: page.appRoot.tableRows
                delegate: Button {
                    id: tableRow
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 78
                    Accessible.name: "Open table " + modelData.title
                    onClicked: {
                        page.appRoot.appController.selectTable(modelData.tableId);
                        page.appRoot.selectedPage = 1;
                    }
                    background: Rectangle {
                        color: tableRow.modelData.selected ? page.appRoot.raisedColor : page.appRoot.surfaceColor
                        border.color: tableRow.modelData.selected ? page.appRoot.accentColor : page.appRoot.lineColor
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
                                color: page.appRoot.textColor
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: tableRow.modelData.phase + " | Round " + tableRow.modelData.round + " | " + tableRow.modelData.updatedAt
                                color: page.appRoot.mutedColor
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            text: tableRow.modelData.transcriptCount + " messages"
                            color: page.appRoot.mutedColor
                            visible: page.appRoot.width >= 520
                        }
                    }
                }
            }
            Label {
                visible: page.appRoot.tableRows.length === 0
                text: "No meeting tables yet. Create one to configure seats and begin a session."
                color: page.appRoot.mutedColor
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                padding: 24
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "Duplicate"
                enabled: Boolean(page.appRoot.currentTable.tableId)
                onClicked: {
                    if (!page.appRoot.appController.duplicateCurrentTable())
                        page.appRoot.showErrorIfNeeded();
                }
            }
            Button {
                text: page.appRoot.currentTable.pinned ? "Unpin" : "Pin"
                enabled: Boolean(page.appRoot.currentTable.tableId)
                onClicked: page.appRoot.appController.togglePinCurrentTable()
            }
            Button {
                text: "Rename"
                enabled: Boolean(page.appRoot.currentTable.tableId)
                onClicked: page.renameRequested()
            }
            Button {
                text: "Delete"
                enabled: Boolean(page.appRoot.currentTable.tableId)
                Material.foreground: page.appRoot.dangerColor
                onClicked: page.deleteRequested()
            }
        }

        Item { Layout.preferredHeight: 20 }
    }
}
