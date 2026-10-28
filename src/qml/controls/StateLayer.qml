// M3 state layer (hover 8 %, pressed 12 %) plus a press "ripple" that
// grows with the container shape, so it never leaks past rounded corners.
import QtQuick

Item {
    id: root
    property color color: Theme.onSurface
    property bool hovered: false
    property bool pressed: false
    property real radius: 0
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.color
        opacity: root.pressed ? 0.12 : root.hovered ? 0.08 : 0
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }

    Rectangle {
        id: ripple
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        radius: root.radius
        color: root.color
        opacity: 0
        scale: 0.6
    }

    ParallelAnimation {
        id: rippleAnim
        NumberAnimation { target: ripple; property: "scale"; from: 0.55; to: 1.0; duration: 380; easing.type: Easing.OutCubic }
        SequentialAnimation {
            NumberAnimation { target: ripple; property: "opacity"; from: 0; to: 0.14; duration: 90 }
            NumberAnimation { target: ripple; property: "opacity"; to: 0; duration: 420; easing.type: Easing.OutQuad }
        }
    }
    onPressedChanged: if (pressed) rippleAnim.restart()
}
