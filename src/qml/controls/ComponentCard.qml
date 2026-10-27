// Component library card (radius 16). Hover lifts it (scale 1.03, spring)
// and rounds the corners; press squares them. Dragging starts a real
// QDrag in C++ with the mime data CircuitView::dropEvent expects.
import QtQuick

Item {
    id: card
    property var data_: ({})
    implicitWidth: 96
    implicitHeight: 100

    readonly property bool hovered: mouse.containsMouse
    readonly property bool pressed: mouse.pressed

    z: hovered ? 2 : 0
    scale: pressed ? 0.97 : hovered ? 1.03 : 1
    Behavior on scale { SpringAnimation { spring: 4.5; damping: 0.26; epsilon: 0.001 } }

    Elevation { target: bg; level: card.hovered ? 2 : 0 }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: card.pressed ? 12 : card.hovered ? 20 : 16
        color: card.hovered ? Theme.surfaceContainerHigh : Theme.surfaceContainerLow
        border.width: 1
        border.color: card.hovered ? "transparent" : Theme.outlineVariant
        Behavior on radius { SpringAnimation { spring: 4; damping: 0.28; epsilon: 0.05 } }
        Behavior on color { ColorAnimation { duration: 200 } }

        StateLayer {
            radius: bg.radius
            color: Theme.primary
            hovered: card.hovered
            pressed: card.pressed
        }
    }

    Image {
        id: art
        anchors.top: parent.top
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: 56
        height: 42
        sourceSize: Qt.size(112, 84)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        source: card.data_.icon || ""
    }
    Txt {
        anchors.top: art.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignTop
        style: "labelM"
        text: card.data_.name || ""
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        maximumLineCount: 2
        lineHeight: 15
        color: Theme.onSurface
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property point origin
        property bool dragging: false
        onPressed: (ev) => { origin = Qt.point(ev.x, ev.y); dragging = false }
        onPositionChanged: (ev) => {
            if (!pressed || dragging) return
            if (Math.abs(ev.x - origin.x) + Math.abs(ev.y - origin.y) < 8) return
            dragging = true
            Shell.hideToolTip()
            Components.startDrag(card.data_.name) // blocks until drop
            dragging = false
            // The drop consumed the release: reset the pressed state
            mouse.enabled = false
            mouse.enabled = true
        }
    }
    Tip {
        anchor: card
        hovered: card.hovered && !mouse.pressed
        text: (card.data_.name || "") + (card.data_.info ? "\n" + card.data_.info : "")
              + "\n" + qsTr("Drag into the circuit to add it")
    }
}
