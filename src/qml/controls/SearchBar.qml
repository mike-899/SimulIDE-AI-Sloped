// M3 search bar: 56 px full-radius pill, leading search icon, trailing clear.
import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: bar
    property alias text: field.text
    property string placeholder: qsTr("Search")
    signal edited(string text)

    implicitHeight: 56
    radius: height / 2
    color: field.activeFocus ? Theme.surfaceContainerHighest : Theme.surfaceContainerHigh
    border.width: field.activeFocus ? 2 : 0
    border.color: Theme.primary
    Behavior on color { ColorAnimation { duration: 200 } }

    StateLayer {
        radius: bar.radius
        color: Theme.onSurface
        hovered: hover.hovered && !field.activeFocus
    }
    HoverHandler { id: hover; cursorShape: Qt.IBeamCursor }

    Icon {
        id: lead
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        name: "search"
        color: Theme.onSurfaceVariant
    }
    TextField {
        id: field
        anchors.left: lead.right
        anchors.leftMargin: 12
        anchors.right: clear.visible ? clear.left : parent.right
        anchors.rightMargin: clear.visible ? 0 : 16
        anchors.verticalCenter: parent.verticalCenter
        placeholderText: bar.placeholder
        placeholderTextColor: Theme.onSurfaceVariant
        color: Theme.onSurface
        selectionColor: Theme.primaryContainer
        selectedTextColor: Theme.onPrimaryContainer
        font.family: Theme.fontFamily
        font.pixelSize: 16
        background: null
        padding: 0
        onTextChanged: bar.edited(text)
        Keys.onEscapePressed: { text = ""; focus = false }
    }
    IconButton {
        id: clear
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: field.text.length > 0
        iconName: "close"
        tip: qsTr("Clear search")
        onClicked: field.text = ""
    }
}
