// Rectangle with an independent (animatable) radius per corner, used by
// connected button groups where inner and outer corners differ.
import QtQuick
import QtQuick.Shapes

Shape {
    id: shape
    property color color: "transparent"
    property real tl: 0
    property real tr: 0
    property real br: 0
    property real bl: 0

    readonly property real maxR: Math.min(width, height) / 2
    readonly property real rtl: Math.max(0, Math.min(tl, maxR))
    readonly property real rtr: Math.max(0, Math.min(tr, maxR))
    readonly property real rbr: Math.max(0, Math.min(br, maxR))
    readonly property real rbl: Math.max(0, Math.min(bl, maxR))

    antialiasing: true
    layer.enabled: true // MSAA for the geometry renderer
    layer.samples: 4

    ShapePath {
        strokeWidth: -1
        fillColor: shape.color
        startX: shape.rtl; startY: 0
        PathLine { x: shape.width - shape.rtr; y: 0 }
        PathArc { x: shape.width; y: shape.rtr; radiusX: shape.rtr; radiusY: shape.rtr }
        PathLine { x: shape.width; y: shape.height - shape.rbr }
        PathArc { x: shape.width - shape.rbr; y: shape.height; radiusX: shape.rbr; radiusY: shape.rbr }
        PathLine { x: shape.rbl; y: shape.height }
        PathArc { x: 0; y: shape.height - shape.rbl; radiusX: shape.rbl; radiusY: shape.rbl }
        PathLine { x: 0; y: shape.rtl }
        PathArc { x: shape.rtl; y: 0; radiusX: shape.rtl; radiusY: shape.rtl }
    }
}
