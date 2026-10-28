// Play triangle that morphs into a stop square (morph: 0 = play, 1 = stop).
import QtQuick

Canvas {
    id: c
    property real morph: 0
    property color color: "white"
    property int size: 24
    width: size
    height: size
    renderStrategy: Canvas.Immediate

    onMorphChanged: requestPaint()
    onColorChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var s = width, m = Math.max(0, Math.min(1, morph))
        // play: 4 points (last two coincide at the tip), stop: square
        var play = [[0.30, 0.18], [0.30, 0.82], [0.84, 0.50], [0.84, 0.50]]
        var stop = [[0.24, 0.24], [0.24, 0.76], [0.76, 0.76], [0.76, 0.24]]
        ctx.fillStyle = c.color
        ctx.strokeStyle = c.color
        ctx.lineJoin = "round"
        ctx.lineWidth = s * 0.10
        ctx.beginPath()
        for (var i = 0; i < 4; i++) {
            var x = (play[i][0] + (stop[i][0] - play[i][0]) * morph) * s
            var y = (play[i][1] + (stop[i][1] - play[i][1]) * morph) * s
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
        }
        ctx.closePath()
        ctx.fill()
        ctx.stroke()
    }
}
