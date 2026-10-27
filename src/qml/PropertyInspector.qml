// Floating property inspector (radius 28) over the canvas. Enters with
// emphasized decelerate + OutBack scale; one editor per property kind.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "controls"

Item {
    id: root
    property real maxHeight: 600        // set by MainWindow::placeOverlays
    readonly property int margin: 12    // room for the shadow
    readonly property bool shown: Inspector.active

    width: 320 + 2 * margin
    height: Math.min(maxHeight, card.implicitHeight + 2 * margin)

    Elevation { target: card; level: 3 }

    Rectangle {
        id: card
        x: root.margin
        y: root.margin
        width: 320
        height: root.height - 2 * root.margin
        radius: 28
        color: Theme.surfaceContainer
        clip: true
        implicitHeight: header.height + (Inspector.collapsed ? 0 : body.implicitHeight) + 8
        Behavior on color { ColorAnimation { duration: 200 } }
        Behavior on implicitHeight {
            NumberAnimation { duration: 350; easing.type: Easing.Bezier; easing.bezierCurve: [0.05, 0.7, 0.1, 1.0, 1, 1] }
        }

        opacity: root.shown ? 1 : 0
        scale: root.shown ? 1 : 0.9
        transformOrigin: Item.TopRight
        Behavior on opacity {
            NumberAnimation {
                duration: root.shown ? 400 : 200
                easing.type: Easing.Bezier
                easing.bezierCurve: root.shown ? [0.05, 0.7, 0.1, 1.0, 1, 1] : [0.3, 0, 0.8, 0.15, 1, 1]
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: root.shown ? 380 : 200
                easing.type: root.shown ? Easing.OutBack : Easing.InCubic
                easing.overshoot: 1.6
            }
        }

        // ---- Header -----------------------------------------------------
        Item {
            id: header
            width: parent.width
            height: 72
            Rectangle { // type chip
                id: chip
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.top: parent.top
                anchors.topMargin: 14
                height: 22
                width: chipText.implicitWidth + 16
                radius: 11
                color: Theme.tertiaryContainer
                Txt {
                    id: chipText
                    anchors.centerIn: parent
                    text: Inspector.itemType
                    style: "labelS"
                    color: Theme.onTertiaryContainer
                }
            }
            Txt {
                objectName: "propertyInspectorTitle"
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.right: helpButton.left
                anchors.top: chip.bottom
                anchors.topMargin: 2
                text: Inspector.itemId
                style: "titleL"
                emphasized: true
                font.letterSpacing: -0.2
            }
            IconButton {
                id: helpButton
                anchors.right: collapse.left
                anchors.verticalCenter: parent.verticalCenter
                iconName: "help"
                checkable: true
                visible: Inspector.help.length > 0
                tip: qsTr("Help")
            }
            IconButton {
                id: collapse
                objectName: "propertyInspectorCollapse"
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                iconName: Inspector.collapsed ? "expand_more" : "expand_less"
                tip: Inspector.collapsed ? qsTr("Expand") : qsTr("Collapse")
                onClicked: Inspector.collapsed = !Inspector.collapsed
            }
        }

        // ---- Body -------------------------------------------------------
        Flickable {
            id: body
            objectName: "propertiesPanel"
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            visible: !Inspector.collapsed
            clip: true
            contentHeight: content.implicitHeight
            implicitHeight: content.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle { implicitWidth: 6; radius: 3; color: Theme.outlineVariant }
            }

            Column {
                id: content
                width: body.width
                spacing: 4
                bottomPadding: 12

                Rectangle { // help text
                    visible: helpButton.checked
                    x: 16
                    width: parent.width - 32
                    height: visible ? helpText.implicitHeight + 20 : 0
                    radius: 16
                    color: Theme.surfaceContainerHighest
                    Txt {
                        id: helpText
                        anchors.fill: parent
                        anchors.margins: 10
                        text: Inspector.help
                        style: "bodyS"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        lineHeightMode: Text.ProportionalHeight
                        lineHeight: 1.1
                        color: Theme.onSurfaceVariant
                        maximumLineCount: 14
                    }
                }

                Repeater {
                    model: Inspector.model
                    delegate: Item {
                        id: row
                        required property int index
                        required property string name
                        required property string label
                        required property var value
                        required property string display
                        required property string kind
                        required property var options
                        required property bool readOnly

                        width: content.width
                        height: 48

                        function commit(v) {
                            if (String(v) !== row.display)
                                Inspector.model.setValue(row.index, v)
                        }

                        Rectangle { // row hover layer
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            radius: 16
                            color: Theme.onSurface
                            opacity: rowHover.hovered ? 0.05 : 0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }
                        HoverHandler { id: rowHover }

                        Txt {
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            width: 112
                            text: row.label.length ? row.label.charAt(0).toUpperCase() + row.label.slice(1) : ""
                            style: "bodyM"
                            color: Theme.onSurfaceVariant
                        }
                        Item {
                            id: editorBox
                            anchors.left: parent.left
                            anchors.leftMargin: 136
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            height: 40

                            // bool
                            M3Switch {
                                visible: row.kind === "bool"
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                enabled: !row.readOnly
                                checkable: false
                                checked: row.kind === "bool" && row.value === true
                                onClicked: Inspector.model.setValue(row.index, !checked)
                            }
                            // enum
                            AbstractButton {
                                id: enumButton
                                visible: row.kind === "enum"
                                anchors.fill: parent
                                enabled: !row.readOnly
                                hoverEnabled: true
                                onClicked: {
                                    var i = Shell.popupChoice(enumButton, row.options, row.options.indexOf(row.display))
                                    if (i >= 0) row.commit(row.options[i])
                                }
                                background: Rectangle {
                                    id: ebg
                                    radius: enumButton.pressed ? 8 : 12
                                    color: Theme.secondaryContainer
                                    Behavior on radius { SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.05 } }
                                    StateLayer {
                                        radius: ebg.radius
                                        color: Theme.onSecondaryContainer
                                        hovered: enumButton.hovered
                                        pressed: enumButton.pressed
                                    }
                                }
                                contentItem: Item {
                                    Txt {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 14
                                        anchors.right: arrow.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: row.display
                                        style: "labelL"
                                        color: Theme.onSecondaryContainer
                                    }
                                    Icon {
                                        id: arrow
                                        anchors.right: parent.right
                                        anchors.rightMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        name: "arrow_drop_down"
                                        size: 20
                                        color: Theme.onSecondaryContainer
                                    }
                                }
                            }
                            // color
                            Rectangle {
                                id: swatch
                                visible: row.kind === "color"
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 32; height: 32; radius: 16
                                color: row.kind === "color" ? row.value : "transparent"
                                border.width: 1
                                border.color: Theme.outline
                            }
                            // text-like editors
                            M3TextField {
                                visible: row.kind === "int" || row.kind === "double"
                                         || row.kind === "string" || row.kind === "color"
                                         || (row.kind === "other" && !row.readOnly)
                                anchors.left: row.kind === "color" ? swatch.right : parent.left
                                anchors.leftMargin: row.kind === "color" ? 8 : 0
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                readOnly: row.readOnly
                                text: row.display
                                inputMethodHints: row.kind === "int" || row.kind === "double"
                                                  ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
                                onCommitted: (t) => row.commit(t)
                                Keys.onUpPressed: if (row.kind === "int" || row.kind === "double") row.commit(Number(row.display) + 1)
                                Keys.onDownPressed: if (row.kind === "int" || row.kind === "double") row.commit(Number(row.display) - 1)
                            }
                            Txt {
                                visible: row.kind === "other" && row.readOnly
                                anchors.fill: parent
                                text: row.display
                                style: "bodyM"
                                color: Theme.onSurface
                            }
                        }
                    }
                }
            }
        }
    }
}
