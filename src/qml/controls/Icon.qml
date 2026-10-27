// Material Symbol tinted by the C++ "icon" image provider.
import QtQuick

Image {
    id: root
    property string name: ""
    property color color: Theme.onSurfaceVariant
    property int size: 24

    width: size
    height: size
    smooth: true
    mipmap: true
    fillMode: Image.PreserveAspectFit
    // 2x source keeps glyphs crisp on HiDPI and when scaled by springs
    sourceSize: Qt.size(size * 2, size * 2)
    source: name.length ? "image://icon/" + name + "/" + String(color).substring(1) : ""
}
