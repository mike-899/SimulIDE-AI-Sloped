// M3 keyboard focus indicator: a 3 px primary ring 2 px outside the control.
// Only shown for keyboard focus (visualFocus), never after a mouse click.
import QtQuick

Rectangle {
    property Item target: parent
    property real targetRadius: height / 2
    anchors.fill: target
    anchors.margins: -5
    radius: targetRadius + 5
    color: "transparent"
    border.width: 3
    border.color: Theme.primary
    visible: target && target.visualFocus
    z: 100
}
