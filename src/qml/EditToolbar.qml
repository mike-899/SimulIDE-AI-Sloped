// Floating toolbar over the canvas (M3 Expressive, vibrant variant on
// primaryContainer, full radius). Buttons drive the circuit QActions.
import QtQuick
import QtQuick.Layouts
import "controls"

Item {
    id: root
    readonly property int margin: 10
    width: bar.width + 2 * margin
    height: bar.height + 2 * margin

    readonly property var groups: [
        [ ["copy", "content_copy"], ["paste", "content_paste"], ["remove", "delete"] ],
        [ ["undo", "undo"], ["redo", "redo"] ],
        [ ["rotateCW", "rotate_right"], ["rotateCCW", "rotate_left"], ["hFlip", "flip"], ["vFlip", "flip_vertical"] ],
        [ ["zoomIn", "zoom_in"], ["zoomOut", "zoom_out"], ["zoomFit", "fit_screen"] ]
    ]

    Elevation { target: bar; level: 3 }

    Rectangle {
        id: bar
        x: root.margin
        y: root.margin
        height: 56
        width: row.implicitWidth + 16
        radius: height / 2
        color: Theme.primaryContainer
        Behavior on color { ColorAnimation { duration: 200 } }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 2

            Repeater {
                model: root.groups
                Row {
                    id: group
                    required property var modelData
                    required property int index
                    spacing: 0
                    Rectangle { // divider between groups
                        visible: group.index > 0
                        width: 1; height: 24
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.alpha(Theme.onPrimaryContainer, 0.25)
                    }
                    Item { visible: group.index > 0; width: 4; height: 1 }
                    Repeater {
                        model: group.modelData
                        IconButton {
                            required property var modelData
                            readonly property var act: Shell.actions[modelData[0]]
                            variant: "vibrant"
                            iconName: modelData[1]
                            iconSize: 22
                            enabled: act ? act.enabled : false
                            tip: act ? act.toolTip : ""
                            onClicked: act.trigger()
                        }
                    }
                    Item { width: 4; height: 1 }
                }
            }

            Rectangle {
                width: 1; height: 24
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.alpha(Theme.onPrimaryContainer, 0.25)
            }
            Item { width: 4; height: 1 }
            IconButton {
                objectName: "pauseButton"
                variant: "vibrant"
                iconName: Sim.paused ? "play_arrow" : "pause"
                checked: Sim.paused
                enabled: Sim.canPause
                tip: qsTr("Pause / Resume Simulation")
                onClicked: Sim.pause()
            }
            // Real speed chip (with an expressive loading shape while running)
            Rectangle {
                id: speed
                objectName: "simSpeedLabel"
                anchors.verticalCenter: parent.verticalCenter
                height: 36
                width: speedRow.implicitWidth + 24
                radius: 18
                color: Sim.error ? Theme.error
                     : Sim.running ? Theme.onPrimaryContainer : Theme.alpha(Theme.onPrimaryContainer, 0.10)
                Behavior on color { ColorAnimation { duration: 200 } }
                Behavior on width { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.5 } }
                Row {
                    id: speedRow
                    anchors.centerIn: parent
                    spacing: 8
                    LoadingShape {
                        visible: Sim.state === "On"
                        anchors.verticalCenter: parent.verticalCenter
                        size: 18
                        color: Theme.primaryContainer
                        running: visible
                    }
                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        style: "labelL"
                        emphasized: Sim.running
                        color: Sim.error ? Theme.onError
                             : Sim.running ? Theme.primaryContainer : Theme.onPrimaryContainer
                        text: Sim.error ? qsTr("Circuit ERROR")
                            : Sim.state === "Off" ? qsTr("Stopped")
                            : Sim.state === "Debug" ? qsTr("Debugger")
                            : qsTr("Speed %1 %").arg(Sim.speed)
                    }
                }
            }
        }
    }
}
