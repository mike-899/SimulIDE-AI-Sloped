// Soft shadow drawn as stacked translucent rounded rectangles (no
// graphical-effects module needed, works with the software renderer).
// Place it before the target in the same parent: Elevation { target: card }
import QtQuick

Item {
    id: root
    property Item target
    property int level: 2       // 0..5
    property real radius: target ? target.radius : 0

    x: target ? target.x : 0
    y: target ? target.y : 0
    width: target ? target.width : 0
    height: target ? target.height : 0
    scale: target ? target.scale : 1
    opacity: target ? target.opacity : 1
    visible: level > 0

    Repeater {
        model: 2 + root.level
        Rectangle {
            readonly property real spread: (index + 1) * (0.6 + root.level * 0.35)
            x: -spread
            y: -spread + root.level * 0.6 + index * 0.4
            width: root.width + 2 * spread
            height: root.height + 2 * spread
            radius: root.radius + spread
            color: "transparent"
            border.width: spread
            border.color: Theme.alpha(Theme.shadow, 0.10 / (index + 1))
        }
    }
}
