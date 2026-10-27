// Top app bar (Tinkercad layout): menu, app + project name, theme toggle,
// file actions, Components | Code connected group, Start Simulation FAB.
import QtQuick
import QtQuick.Layouts
import "controls"

Rectangle {
    id: bar
    color: Theme.surfaceContainer
    // Narrow windows: file actions move into the menu, labels shorten
    readonly property bool compact: width < 960
    readonly property bool tiny: width < 700
    Behavior on color { ColorAnimation { duration: 200 } }

    Rectangle { // bottom divider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.outlineVariant
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 16
        spacing: 8

        IconButton {
            id: menuButton
            objectName: "menuButton"
            iconName: "menu"
            size: 48
            tip: qsTr("Menu")
            onClicked: Shell.popupMenu(menuButton)
        }
        Image {
            source: "qrc:/simulide.png"
            sourceSize: Qt.size(56, 56)
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            smooth: true
        }
        Txt {
            objectName: "appTitle"
            text: "SimulIDE"
            font.pixelSize: 22
            font.weight: Font.Bold
            font.letterSpacing: -0.4
            color: Theme.onSurface
        }
        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 28
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            color: Theme.outlineVariant
        }
        Rectangle { // project name chip
            objectName: "projectName"
            Layout.preferredHeight: 40
            Layout.preferredWidth: Math.min(projectText.implicitWidth + 32 + (Files.modified ? 16 : 0), 360)
            Layout.minimumWidth: 60
            Layout.fillWidth: false
            visible: !bar.tiny
            radius: 20
            color: projectHover.hovered ? Theme.surfaceContainerHighest : "transparent"
            Behavior on color { ColorAnimation { duration: 150 } }
            HoverHandler { id: projectHover }
            Txt {
                id: projectText
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: dot.visible ? dot.left : parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: Files.title.length ? Files.title : qsTr("New Circuit")
                style: "titleM"
                color: Theme.onSurfaceVariant
            }
            Rectangle {
                id: dot
                visible: Files.modified
                width: 8; height: 8; radius: 4
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.tertiary
            }
            Tip { anchor: parent; hovered: projectHover.hovered; text: Files.modified ? qsTr("Unsaved changes") : "" }
        }

        Item { Layout.fillWidth: true }

        IconButton {
            id: themeButton
            objectName: "themeToggleButton"
            iconName: Theme.dark ? "light_mode" : "dark_mode"
            tip: qsTr("Switch light / dark theme")
            onClicked: { spin.restart(); Shell.toggleTheme() }
            RotationAnimation on rotation { id: spin; running: false; from: -90; to: 0; duration: 450; easing.type: Easing.OutBack }
        }
        IconButton {
            objectName: "newCircuitButton"
            visible: !bar.compact
            iconName: "note_add"
            tip: qsTr("New Circuit (Ctrl+N)")
            onClicked: Files.newCircuit()
        }
        IconButton {
            objectName: "openCircuitButton"
            visible: !bar.compact
            iconName: "folder_open"
            tip: qsTr("Open Circuit (Ctrl+O)")
            onClicked: Files.open()
        }
        IconButton {
            objectName: "saveCircuitButton"
            visible: !bar.compact
            iconName: "save"
            tip: qsTr("Save Circuit (Ctrl+S)")
            onClicked: Files.save()
        }

        Row { // Connected button group
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            spacing: 2
            ConnectedButton {
                objectName: "componentsButton"
                position: "first"
                iconName: "widgets"
                text: bar.tiny ? "" : qsTr("Components")
                tip: qsTr("Show / hide the component panel")
                checked: Shell.componentsVisible
                onClicked: Shell.componentsVisible = !Shell.componentsVisible
            }
            ConnectedButton {
                objectName: "codeButton"
                position: "last"
                iconName: "code"
                text: bar.tiny ? "" : qsTr("Code")
                tip: qsTr("Show / hide the code editor")
                checked: Shell.codeVisible
                onClicked: Shell.codeVisible = !Shell.codeVisible
            }
        }
        SimFab {
            objectName: "simButton"
            shortLabel: bar.compact
        }
    }
}
