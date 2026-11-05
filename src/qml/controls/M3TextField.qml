// Compact outlined M3 text field (radius 12), commits on Enter / focus out.
// Set `error` (and `errorText`) to show the M3 error state; Esc emits
// reverted() so the owner can restore the real value.
import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: tf
    signal committed(string text)
    signal reverted()
    property string suffix: ""
    property bool error: false
    property string errorText: ""

    onErrorChanged: if (error && errorText.length) Shell.showToolTip(tf, errorText)
                    else if (!error) Shell.hideToolTip()
    Keys.onEscapePressed: { reverted(); focus = false }

    Txt { // M3 error indicator
        visible: tf.error
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: "!"
        style: "labelL"
        color: Theme.error
    }

    implicitHeight: 40
    leftPadding: 14
    rightPadding: suffix.length ? 34 : (error ? 26 : 12)
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
        border.width: tf.activeFocus || tf.error ? 2 : 1
        border.color: tf.error ? Theme.error
                    : tf.activeFocus ? Theme.primary
                    : (tf.hovered ? Theme.outline : Theme.outlineVariant)
        Behavior on radius { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.05 } }
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }
    }
}
