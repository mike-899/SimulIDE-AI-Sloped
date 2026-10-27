// M3 icon button with expressive shape morphing: round at rest, squarer
// while pressed; a selected toggle morphs round -> square (radius 12).
// variant: "standard" | "tonal" | "filled" | "vibrant" (on primaryContainer)
import QtQuick
import QtQuick.Controls.Basic

AbstractButton {
    id: btn
    property string iconName: ""
    property string variant: "standard"
    property string tip: ""
    property int size: 40
    property int iconSize: 24

    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    readonly property color containerColor: {
        if (!enabled) return variant === "standard" || variant === "vibrant"
                             ? "transparent" : Theme.alpha(Theme.onSurface, 0.12)
        if (variant === "filled") return checkable && !checked ? Theme.surfaceContainerHighest : Theme.primary
        if (variant === "tonal") return checkable && !checked ? Theme.surfaceContainerHighest : Theme.secondaryContainer
        if (checked) return variant === "vibrant" ? Theme.onPrimaryContainer : Theme.secondaryContainer
        return "transparent"
    }
    readonly property color contentColor: {
        if (!enabled) return Theme.alpha(Theme.onSurface, 0.38)
        if (variant === "filled") return checkable && !checked ? Theme.primary : Theme.onPrimary
        if (variant === "tonal") return checkable && !checked ? Theme.onSurfaceVariant : Theme.onSecondaryContainer
        if (variant === "vibrant") return checked ? Theme.primaryContainer : Theme.onPrimaryContainer
        return checked ? Theme.onSecondaryContainer : Theme.onSurfaceVariant
    }
    readonly property real restRadius: checked ? 12 : height / 2

    background: Rectangle {
        id: bg
        radius: btn.pressed ? 8 : btn.restRadius
        color: btn.containerColor
        Behavior on radius { SpringAnimation { spring: 4.5; damping: 0.28; epsilon: 0.05 } }
        Behavior on color { ColorAnimation { duration: 200 } }
        StateLayer {
            radius: bg.radius
            color: btn.contentColor
            hovered: btn.hovered && btn.enabled
            pressed: btn.pressed
        }
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: btn.iconName
            size: btn.iconSize
            color: btn.contentColor
            scale: btn.pressed ? 0.88 : 1
            Behavior on scale { SpringAnimation { spring: 5; damping: 0.3; epsilon: 0.005 } }
        }
    }
    Tip { anchor: btn; text: btn.tip; hovered: btn.hovered }
}
