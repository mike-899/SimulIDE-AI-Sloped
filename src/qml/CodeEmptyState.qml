// Shown over the code editor while no file is open.
import QtQuick
import "controls"

Rectangle {
    objectName: "codeEmptyState"
    color: Theme.surface
    Behavior on color { ColorAnimation { duration: 200 } }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 320)
        spacing: 12

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 72; height: 72; radius: 28
            color: Theme.secondaryContainer
            Icon {
                anchors.centerIn: parent
                name: "code"
                size: 36
                color: Theme.onSecondaryContainer
            }
        }
        Txt {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("No file open")
            style: "titleL"
            emphasized: true
        }
        Txt {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            text: qsTr("Open a source file (.ino, .asm, .c, .gcb) to edit, compile and upload it to the target MCU, or start a new one.")
            style: "bodyM"
            color: Theme.onSurfaceVariant
        }
        Item { width: 1; height: 4 }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            PillButton {
                objectName: "codeOpenFileButton"
                iconName: "folder_open"
                text: qsTr("Open file")
                onClicked: Files.openCode()
            }
            PillButton {
                objectName: "codeNewFileButton"
                iconName: "note_add"
                tonal: true
                text: qsTr("New file")
                onClicked: Files.newCode()
            }
        }
    }
}
