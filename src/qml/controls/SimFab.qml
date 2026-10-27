// Extended FAB "Start Simulation" (56 px, radius 16). Its container
// animates to the error colour while running and the icon morphs
// play -> stop with a spring.
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: fab
    property string state_: Sim.state
    readonly property bool on: state_ === "On" || state_ === "Paused"

    enabled: Sim.canToggle
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    implicitHeight: 56
    implicitWidth: content.implicitWidth + 40
    text: state_ === "On" ? qsTr("Stop Simulation")
        : state_ === "Paused" ? qsTr("Paused")
        : state_ === "Debug" ? qsTr("Debugging")
        : qsTr("Start Simulation")

    Behavior on implicitWidth { SpringAnimation { spring: 3.5; damping: 0.3; epsilon: 0.5 } }

    readonly property color containerColor: state_ === "Debug" ? Theme.tertiaryContainer
        : state_ === "Paused" ? Theme.tertiary
        : on ? Theme.error : Theme.primary
    readonly property color contentColor: state_ === "Debug" ? Theme.onTertiaryContainer
        : state_ === "Paused" ? Theme.onTertiary
        : on ? Theme.onError : Theme.onPrimary

    onClicked: Sim.toggle()

    Elevation { target: bg; level: fab.hovered ? 3 : 2; z: -1 }

    background: Rectangle {
        id: bg
        radius: fab.pressed ? 12 : (fab.on ? 28 : 16)
        color: fab.containerColor
        scale: fab.pressed ? 0.97 : 1
        Behavior on radius { SpringAnimation { spring: 3.5; damping: 0.26; epsilon: 0.05 } }
        Behavior on scale { SpringAnimation { spring: 5; damping: 0.3; epsilon: 0.002 } }
        Behavior on color { ColorAnimation { duration: 200 } }
        StateLayer {
            radius: bg.radius
            color: fab.contentColor
            hovered: fab.hovered
            pressed: fab.pressed
        }
    }
    contentItem: Item {
        Row {
            id: content
            anchors.centerIn: parent
            spacing: 12
            PlayStopIcon {
                visible: fab.state_ !== "Debug" && fab.state_ !== "Paused"
                anchors.verticalCenter: parent.verticalCenter
                color: fab.contentColor
                morph: fab.on ? 1 : 0
                Behavior on morph { SpringAnimation { spring: 4; damping: 0.32; epsilon: 0.002 } }
            }
            Icon {
                visible: !(fab.state_ !== "Debug" && fab.state_ !== "Paused")
                anchors.verticalCenter: parent.verticalCenter
                name: fab.state_ === "Debug" ? "bug_report" : "pause"
                color: fab.contentColor
            }
            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: fab.text
                style: "titleM"
                emphasized: true
                color: fab.contentColor
            }
        }
    }
    Tip { anchor: fab; text: qsTr("Power the circuit on / off"); hovered: fab.hovered }
}
