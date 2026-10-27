// M3 text-labelled button (full radius): filled (primary) or tonal.
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: btn
    property string iconName: ""
    property bool tonal: false

    implicitHeight: 40
    implicitWidth: row.implicitWidth + 48
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    readonly property color fg: tonal ? Theme.onSecondaryContainer : Theme.onPrimary

    background: Rectangle {
        id: bg
        radius: btn.pressed ? 12 : height / 2
        color: btn.tonal ? Theme.secondaryContainer : Theme.primary
        Behavior on radius { SpringAnimation { spring: 4; damping: 0.28; epsilon: 0.05 } }
        StateLayer {
            radius: bg.radius
            color: btn.fg
            hovered: btn.hovered
            pressed: btn.pressed
        }
    }
    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Icon {
                visible: btn.iconName.length > 0
                name: btn.iconName
                size: 18
                color: btn.fg
                anchors.verticalCenter: parent.verticalCenter
            }
            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: btn.text
                style: "labelL"
                color: btn.fg
            }
        }
    }
}
