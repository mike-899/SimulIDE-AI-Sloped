// Material 3 type scale on Inter (docs/M3_EXPRESSIVE.md).
// style: displayS, headlineS, titleL, titleM, titleS, bodyL, bodyM, bodyS,
//        labelL, labelM, labelS. emphasized raises the weight one step.
import QtQuick

Text {
    id: t
    property string style: "bodyM"
    property bool emphasized: false

    readonly property var scale: ({
        "displayS": [36, 44, 400], "headlineS": [24, 32, 400],
        "titleL": [22, 28, 400], "titleM": [16, 24, 500], "titleS": [14, 20, 500],
        "bodyL": [16, 24, 400], "bodyM": [14, 20, 400], "bodyS": [12, 16, 400],
        "labelL": [14, 20, 500], "labelM": [12, 16, 500], "labelS": [11, 16, 500]
    })
    readonly property var spec: scale[style] || scale["bodyM"]

    color: Theme.onSurface
    font.family: Theme.fontFamily
    font.pixelSize: spec[0]
    font.weight: emphasized ? (spec[2] >= 500 ? Font.Bold : Font.Medium)
                            : (spec[2] >= 500 ? Font.Medium : Font.Normal)
    font.letterSpacing: style.indexOf("label") === 0 ? 0.1 : 0
    lineHeightMode: Text.FixedHeight
    lineHeight: spec[1]
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
    renderType: Text.NativeRendering

    Behavior on color { ColorAnimation { duration: 200 } }
}
