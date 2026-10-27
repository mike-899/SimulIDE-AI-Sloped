// Pill-shaped dropdown (M3 Expressive menu button) with an animated menu.
// model: array of { key, label }.
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: dd
    property var model: []
    property int currentIndex: 0
    property string leadingIcon: ""
    property real maxMenuHeight: 420
    signal activated(int index)

    readonly property string currentLabel: model.length > currentIndex && currentIndex >= 0
                                           ? model[currentIndex].label : ""
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    implicitHeight: 48
    text: currentLabel
    onClicked: menu.opened ? menu.close() : menu.open()

    background: Rectangle {
        id: bg
        radius: menu.visible ? 16 : (dd.pressed ? 12 : height / 2)
        color: menu.visible ? Theme.secondaryContainer : Theme.surfaceContainerHighest
        Behavior on radius { SpringAnimation { spring: 4; damping: 0.28; epsilon: 0.05 } }
        Behavior on color { ColorAnimation { duration: 200 } }
        StateLayer {
            radius: bg.radius
            color: Theme.onSurface
            hovered: dd.hovered
            pressed: dd.pressed
        }
    }
    contentItem: Item {
        Icon {
            id: lead
            visible: dd.leadingIcon.length > 0
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            name: dd.leadingIcon
            size: 20
            color: Theme.primary
        }
        Txt {
            anchors.left: lead.visible ? lead.right : parent.left
            anchors.leftMargin: lead.visible ? 10 : 20
            anchors.right: arrow.left
            anchors.verticalCenter: parent.verticalCenter
            text: dd.currentLabel
            style: "labelL"
            emphasized: true
            color: menu.visible ? Theme.onSecondaryContainer : Theme.onSurface
        }
        Icon {
            id: arrow
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            name: "expand_more"
            color: Theme.onSurfaceVariant
            rotation: menu.visible ? 180 : 0
            Behavior on rotation { SpringAnimation { spring: 4; damping: 0.3 } }
        }
    }

    Popup {
        id: menu
        y: dd.height + 6
        width: dd.width
        height: Math.min(list.contentHeight + 16, dd.maxMenuHeight)
        padding: 8
        transformOrigin: Item.Top
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
                NumberAnimation { property: "scale"; from: 0.85; to: 1; duration: 320; easing.type: Easing.OutBack; easing.overshoot: 1.4 }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 140; easing.type: Easing.InCubic }
        }
        background: Item {
            Elevation { target: mbg; level: 3 }
            Rectangle {
                id: mbg
                anchors.fill: parent
                radius: 16
                color: Theme.surfaceContainer
            }
        }
        contentItem: ListView {
            id: list
            clip: true
            model: dd.model
            currentIndex: dd.currentIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: AbstractButton {
                id: item
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 44
                hoverEnabled: true
                readonly property bool selected: index === dd.currentIndex
                onClicked: { menu.close(); dd.activated(index) } // owner binds currentIndex
                background: Rectangle {
                    radius: item.selected ? 22 : 12
                    color: item.selected ? Theme.secondaryContainer : "transparent"
                    Behavior on color { ColorAnimation { duration: 150 } }
                    StateLayer {
                        radius: parent.radius
                        color: Theme.onSurface
                        hovered: item.hovered
                        pressed: item.pressed
                    }
                }
                contentItem: Item {
                    Icon {
                        id: chk
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        name: "check"
                        size: 20
                        visible: item.selected
                        color: Theme.onSecondaryContainer
                    }
                    Txt {
                        anchors.left: parent.left
                        anchors.leftMargin: 44
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: item.modelData.label
                        style: "labelL"
                        emphasized: item.selected
                        color: item.selected ? Theme.onSecondaryContainer : Theme.onSurface
                    }
                }
            }
        }
    }
}
