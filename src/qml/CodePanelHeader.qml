// Code panel header: title, target MCU selector, close button.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "controls"

Rectangle {
    color: Theme.surfaceContainerLow
    Behavior on color { ColorAnimation { duration: 200 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 8
        spacing: 8

        Icon { name: "code"; color: Theme.primary }
        Txt {
            objectName: "codePanelTitle"
            text: qsTr("Code")
            style: "titleM"
            emphasized: true
        }
        Item { Layout.fillWidth: true }
        Txt {
            text: qsTr("Target")
            style: "labelM"
            color: Theme.onSurfaceVariant
        }
        // Target MCU: upload/debug, RAM table and Serial Monitor follow it
        AbstractButton {
            id: mcu
            objectName: "mcuSelector"
            enabled: !Mcus.empty
            hoverEnabled: true
            implicitHeight: 40
            implicitWidth: Math.min(mcuRow.implicitWidth + 32, 280)
            onClicked: {
                var labels = Mcus.mcus.map(function (m) { return m.label })
                var i = Shell.popupChoice(mcu, labels, Mcus.currentIndex)
                if (i >= 0) Mcus.select(i)
            }
            background: Rectangle {
                id: mbg
                radius: mcu.pressed ? 12 : height / 2
                color: Mcus.empty ? Theme.alpha(Theme.onSurface, 0.08) : Theme.secondaryContainer
                Behavior on radius { SpringAnimation { spring: 4; damping: 0.28; epsilon: 0.05 } }
                Behavior on color { ColorAnimation { duration: 200 } }
                StateLayer {
                    radius: mbg.radius
                    color: Theme.onSecondaryContainer
                    hovered: mcu.hovered
                    pressed: mcu.pressed
                }
            }
            contentItem: Item {
                Row {
                    id: mcuRow
                    anchors.centerIn: parent
                    spacing: 8
                    Icon {
                        name: "memory"
                        size: 18
                        color: Mcus.empty ? Theme.alpha(Theme.onSurface, 0.38) : Theme.onSecondaryContainer
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, 200)
                        text: Mcus.empty ? qsTr("No MCU")
                              : (Mcus.currentIndex >= 0 ? Mcus.mcus[Mcus.currentIndex].label : qsTr("Select MCU"))
                        style: "labelL"
                        color: Mcus.empty ? Theme.alpha(Theme.onSurface, 0.38) : Theme.onSecondaryContainer
                    }
                    Icon {
                        visible: !Mcus.empty
                        name: "arrow_drop_down"
                        size: 18
                        color: Theme.onSecondaryContainer
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
            Tip {
                anchor: mcu
                hovered: mcu.hovered
                text: qsTr("MCU targeted by upload, debugger, RAM table and Serial Monitor")
            }
        }
        IconButton {
            objectName: "codePanelClose"
            iconName: "close"
            tip: qsTr("Close code panel")
            onClicked: Shell.codeVisible = false
        }
    }
}
