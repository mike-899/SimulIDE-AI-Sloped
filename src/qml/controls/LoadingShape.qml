// M3 Expressive loading indicator: a rotating shape that morphs between
// a soft 4-lobe and 8-lobe "cookie".
import QtQuick

Canvas {
    id: c
    property color color: Theme.primary
    property int size: 24
    property bool running: true
    property real phase: 0
    property real spin: 0
    width: size
    height: size
    renderStrategy: Canvas.Immediate

    // Advance in coarse steps from a timer instead of two 60 fps
    // NumberAnimations: every frame of this Canvas re-renders and
    // re-composites the whole Qt Quick overlay over the circuit view, which
    // cost more CPU than the simulation itself. ~12 fps is enough for a
    // status indicator.
    Timer {
        interval: 80
        repeat: true
        running: c.running && c.visible
        onTriggered: {
            c.phase = (c.phase + 0.1) % 2
            c.spin = (c.spin + 12) % 360
        }
    }
    onPhaseChanged: requestPaint()
    onColorChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var r = width / 2
        // 0..1..0 blend between 4 and 8 lobes, eased
        var t = phase < 1 ? phase : 2 - phase
        t = t * t * (3 - 2 * t)
        var amp4 = 0.16 * (1 - t), amp8 = 0.10 * t
        var rot = spin * Math.PI / 180
        ctx.fillStyle = c.color
        ctx.beginPath()
        for (var i = 0; i <= 72; i++) {
            var a = i / 72 * 2 * Math.PI
            var k = 1 - amp4 - amp8 + amp4 * Math.cos(4 * a) + amp8 * Math.cos(8 * a)
            var x = r + r * 0.92 * k * Math.cos(a + rot)
            var y = r + r * 0.92 * k * Math.sin(a + rot)
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.closePath()
        ctx.fill()
    }
}
