// Side panel page switcher: Components | RAM | Files (connected group).
import QtQuick
import QtQuick.Layouts
import "controls"

Rectangle {
    color: Theme.surfaceContainerLow
    Behavior on color { ColorAnimation { duration: 200 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 10
        anchors.bottomMargin: 6
        spacing: 2

        Repeater {
            model: [
                { icon: "widgets", label: qsTr("Components"), tip: qsTr("Component library") },
                { icon: "memory", label: qsTr("RAM"), tip: qsTr("RamTable") },
                { icon: "folder", label: qsTr("Files"), tip: qsTr("File explorer") }
            ]
            ConnectedButton {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.preferredWidth: index === 0 ? 2 : 1
                compact: true
                position: index === 0 ? "first" : index === 2 ? "last" : "middle"
                iconName: modelData.icon
                text: modelData.label
                tip: modelData.tip
                checked: Shell.sidePage === index
                onClicked: Shell.sidePage = index
            }
        }
    }
}
