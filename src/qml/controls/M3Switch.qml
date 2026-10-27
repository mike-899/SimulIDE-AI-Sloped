// M3 switch: 52x32 track; the handle grows 16 -> 24 when on and 28 while
// pressed (spring), with a check icon when selected.
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: sw
    checkable: true
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    implicitWidth: 52
    implicitHeight: 32

    background: Rectangle {
        radius: height / 2
        color: sw.checked ? Theme.primary : Theme.surfaceContainerHighest
        border.width: sw.checked ? 0 : 2
        border.color: Theme.outline
        Behavior on color { ColorAnimation { duration: 200 } }
    }
    contentItem: Item {
        Rectangle {
            id: handle
            property real d: sw.pressed ? 28 : sw.checked ? 24 : 16
            width: d
            height: d
            radius: d / 2
            anchors.verticalCenter: parent.verticalCenter
            x: sw.checked ? sw.width - 4 - d : (sw.pressed ? 2 : 8)
            color: sw.checked ? Theme.onPrimary : Theme.outline
            Behavior on d { SpringAnimation { spring: 5; damping: 0.3; epsilon: 0.05 } }
            Behavior on x { SpringAnimation { spring: 4; damping: 0.32; epsilon: 0.05 } }
            Behavior on color { ColorAnimation { duration: 200 } }
            Icon {
                anchors.centerIn: parent
                visible: sw.checked && handle.d > 20
                name: "check"
                size: 16
                color: Theme.onPrimaryContainer
            }
            Rectangle { // state layer halo
                anchors.centerIn: parent
                width: 40; height: 40; radius: 20
                color: sw.checked ? Theme.primary : Theme.onSurface
                opacity: sw.pressed ? 0.12 : sw.hovered ? 0.08 : 0
                Behavior on opacity { NumberAnimation { duration: 150 } }
            }
        }
    }
}
