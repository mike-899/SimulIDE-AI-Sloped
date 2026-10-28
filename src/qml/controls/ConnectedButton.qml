// Member of an M3 Expressive connected button group: outer corners round,
// inner corners 8; the selected button morphs to a full pill.
// position: "first" | "middle" | "last" | "single"
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: btn
    property string iconName: ""
    property string position: "middle"
    property string tip: ""
    property bool compact: false

    checkable: false // checked is bound by the group owner
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    implicitHeight: compact ? 40 : 48
    implicitWidth: row.implicitWidth + (compact ? 24 : 32)

    readonly property real full: height / 2
    readonly property real inner: pressed ? 4 : (compact ? 6 : 8)
    readonly property bool roundL: checked || position === "first" || position === "single"
    readonly property bool roundR: checked || position === "last" || position === "single"

    readonly property color containerColor: checked ? Theme.primary : Theme.surfaceContainerHighest
    readonly property color contentColor: checked ? Theme.onPrimary : Theme.onSurfaceVariant

    background: Item {
        RoundRect {
            id: shape
            anchors.fill: parent
            color: btn.containerColor
            tl: btn.roundL ? btn.full : btn.inner
            bl: btn.roundL ? btn.full : btn.inner
            tr: btn.roundR ? btn.full : btn.inner
            br: btn.roundR ? btn.full : btn.inner
            Behavior on tl { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.05 } }
            Behavior on tr { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.05 } }
            Behavior on bl { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.05 } }
            Behavior on br { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.05 } }
            Behavior on color { ColorAnimation { duration: 200 } }
        }
        // State layer: a plain rounded rect inset so it stays inside the shape
        StateLayer {
            anchors.margins: 2
            radius: Math.min(shape.tl, shape.tr) - 2
            color: btn.contentColor
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
                name: btn.iconName
                visible: btn.iconName.length > 0
                size: btn.compact ? 18 : 20
                color: btn.contentColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Txt {
                text: btn.text
                visible: text.length > 0
                style: btn.compact ? "labelM" : "labelL"
                emphasized: btn.checked
                color: btn.contentColor
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
    Tip { anchor: btn; text: btn.tip; hovered: btn.hovered }
}
