// Compact outlined M3 text field (radius 12), commits on Enter / focus out.
import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: tf
    signal committed(string text)
    property string suffix: ""

    implicitHeight: 40
    leftPadding: 14
    rightPadding: suffix.length ? 34 : 12
    color: Theme.onSurface
    selectionColor: Theme.primaryContainer
    selectedTextColor: Theme.onPrimaryContainer
    placeholderTextColor: Theme.onSurfaceVariant
    font.family: Theme.fontFamily
    font.pixelSize: 14
    selectByMouse: true
    verticalAlignment: TextInput.AlignVCenter

    onAccepted: { committed(text); focus = false }
    onActiveFocusChanged: if (!activeFocus) committed(text)

    background: Rectangle {
        radius: tf.activeFocus ? 8 : 12
        color: tf.activeFocus ? Theme.surfaceContainerHighest : Theme.surfaceContainerHigh
        border.width: tf.activeFocus ? 2 : 1
        border.color: tf.activeFocus ? Theme.primary : (tf.hovered ? Theme.outline : Theme.outlineVariant)
        Behavior on radius { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.05 } }
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
    }
}
