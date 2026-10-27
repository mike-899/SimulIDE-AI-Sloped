// Component library (Tinkercad layout): category dropdown, search bar and
// a grid of cards grouped in sections. Data: "Components" (ComponentModel).
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "controls"

Rectangle {
    id: panel
    color: Theme.surfaceContainerLow
    Behavior on color { ColorAnimation { duration: 200 } }

    readonly property int gap: 8
    readonly property real gridWidth: list.width - 12 // room for the scrollbar
    readonly property int columns: Math.max(2, Math.floor((gridWidth + gap) / (96 + gap)))
    readonly property real cardWidth: Math.floor((gridWidth - (columns - 1) * gap) / columns)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 4
        anchors.topMargin: 6
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.rightMargin: 8
            spacing: 6
            PillDropdown {
                objectName: "componentCategoryBox"
                Layout.fillWidth: true
                leadingIcon: "widgets"
                model: Components.categories
                currentIndex: Components.categoryIndex
                maxMenuHeight: panel.height - 80
                onActivated: (i) => Components.category = Components.categories[i].key
            }
            IconButton {
                objectName: "manageComponentsButton"
                iconName: "tune"
                tip: qsTr("Manage Components (show/hide)")
                onClicked: Components.manage()
            }
        }

        SearchBar {
            id: search
            objectName: "componentSearch"
            Layout.fillWidth: true
            Layout.rightMargin: 8
            placeholder: qsTr("Search components")
            text: Components.search
            onEdited: (t) => debounce.restart()
            Timer {
                id: debounce
                interval: 120
                onTriggered: Components.search = search.text
            }
        }

        ListView {
            id: list
            objectName: "componentGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: Components
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: Theme.outlineVariant
                }
            }
            // Scroll back to the top when the category / search changes
            Connections {
                target: Components
                function onContentChanged() { list.positionViewAtBeginning() }
            }

            delegate: Column {
                id: section
                required property string title
                required property var cards
                width: list.width
                spacing: panel.gap
                topPadding: 4
                bottomPadding: 10

                Txt {
                    visible: section.title.length > 0
                    text: section.title
                    style: "titleS"
                    emphasized: true
                    color: Theme.primary
                    leftPadding: 4
                }
                Flow {
                    width: panel.gridWidth
                    spacing: panel.gap
                    Repeater {
                        model: section.cards
                        ComponentCard {
                            required property var modelData
                            data_: modelData
                            width: panel.cardWidth
                            height: 104
                        }
                    }
                }
            }

            Txt {
                anchors.centerIn: parent
                visible: Components.cardCount === 0
                text: qsTr("No components found")
                style: "bodyM"
                color: Theme.onSurfaceVariant
            }
        }
    }

    MouseArea { // Right click: manage components (as the Widgets panel)
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: Components.manage()
    }
}
