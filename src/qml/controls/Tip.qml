// Delayed native tooltip (QToolTip via Shell, so it is not clipped by the
// QQuickWidget that hosts the control). A press on the anchor dismisses it
// until the pointer leaves and comes back (so a clicked button's tip does
// not linger over what the click just revealed).
import QtQuick

Timer {
    id: tip
    property Item anchor
    property string text: ""
    property bool hovered: false
    property bool dismissed: false
    interval: 800
    running: hovered && !dismissed && text.length > 0
    onTriggered: Shell.showToolTip(anchor, text)
    onHoveredChanged: {
        if (!hovered) {
            dismissed = false
            Shell.hideToolTip()
        }
    }
    property Connections presses: Connections {
        target: tip.anchor
        ignoreUnknownSignals: true
        function onPressedChanged() {
            if (tip.anchor.pressed) {
                tip.dismissed = true
                Shell.hideToolTip()
            }
        }
    }
}
