// Delayed native tooltip (QToolTip via Shell, so it is not clipped by the
// QQuickWidget that hosts the control).
import QtQuick

Timer {
    id: tip
    property Item anchor
    property string text: ""
    property bool hovered: false
    interval: 650
    running: hovered && text.length > 0
    onTriggered: Shell.showToolTip(anchor, text)
    onHoveredChanged: if (!hovered) Shell.hideToolTip()
}
