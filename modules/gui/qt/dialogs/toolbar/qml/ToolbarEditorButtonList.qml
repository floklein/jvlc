/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick
import QtQuick.Templates as T
import QtQml.Models
import QtQuick.Layouts


import VLC.Player
import VLC.PlayerControls
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

// The palette of Customize Player (Figma 2107:5604): the items that are not
// on the bars yet. Spacer and Divider always stay. Dropping a bar item here
// removes it from the player.
GridView {
    id: root

    clip: true

    T.ScrollBar.vertical: Widgets.ScrollBarExt { }

    // the Layout of the edited player
    property PlayerControlbarModel bars: null

    // bumped when the bars change, to filter the palette again
    property int _revision: 0

    model: {
        void _revision
        const list = PlayerControlbarControls.controlList
        if (!bars)
            return list
        return list.filter((entry) => bars.canAdd(entry.id))
    }

    currentIndex: -1
    highlightFollowsCurrentItem: false

    activeFocusOnTab: true
    keyNavigationEnabled: true

    cellWidth: VLCStyle.dp(108, VLCStyle.scale)
    cellHeight: VLCStyle.dp(84, VLCStyle.scale)

    boundsBehavior: Flickable.StopAtBounds

    property alias removeInfoRectVisible: removeInfoRect.visible

    signal controlDragStarted(int id)
    signal controlDragStopped(int id)

    // Enter on an item: add it to the bars
    signal addRequested(int id)

    Accessible.role: Accessible.List
    Accessible.name: qsTr("Items")

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Connections {
        target: root.bars

        function onControlListChanged() {
            root._revision++
        }
    }

    onActiveFocusChanged: {
        if (activeFocus && currentIndex < 0 && count > 0)
            currentIndex = 0
    }

    onCountChanged: {
        if (currentIndex >= count)
            currentIndex = count - 1
    }

    Keys.onPressed: (event) => {
        // Enter or Space adds the current item
        if (KeyHelper.matchOk(event) && currentIndex >= 0 && currentIndex < count) {
            root.addRequested(model[currentIndex].id)
            event.accepted = true
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1

        preventStealing: true
    }

    property Component implicitFlickableScrollHandler: DefaultFlickableScrollHandler { }

    // NOTE: This property can be set to null to prevent using a scroll handler:
    property FlickableScrollHandler scrollHandler: {
        if (interactive) {
            // JS ownership:
            return implicitFlickableScrollHandler.createObject(null, { target: root })
        } else {
            return null
        }
    }

    DropArea {
        id: dropArea
        anchors.fill: parent

        z: 3

        function isFromList() {
            if (drag.source.objectName === "buttonsList")
                return true
            else
                return false
        }

        onEntered: (drag) => {
            // the VLC menu can't be removed
            if (!isFromList() && root.bars && !root.bars.isRemovable(drag.source.controlId))
                drag.accepted = false
        }

        onDropped: (drop) => {
            if (isFromList())
                return

            if (root.bars.removeControl(drop.source.dndView.model, drop.source.DelegateModel.itemsIndex))
                drop.accept(Qt.MoveAction)
        }
    }

    Rectangle {
        id: removeInfoRect
        anchors.fill: parent
        z: 2

        visible: false

        opacity: 0.8
        color: theme.bg.primary

        border.color: theme.border
        border.width: VLCStyle.dp(2, VLCStyle.scale)
        border.pixelAligned: (radius < Number.EPSILON)

        Text {
            anchors.centerIn: parent

            text: VLCIcons.del
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter

            font.pointSize: VLCStyle.fontHeight_xxlarge

            font.family: VLCIcons.fontFamily
            color: theme.fg.secondary

            // an icon glyph, not text to read
            Accessible.ignored: true
        }

        MouseArea {
            anchors.fill: parent

            cursorShape: visible ? Qt.DragMoveCursor : Qt.ArrowCursor
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1

        visible: buttonDragItem.Drag.active

        cursorShape: visible ? Qt.DragMoveCursor : Qt.ArrowCursor
    }

    delegate: MouseArea {
        id: cell

        required property var modelData
        required property int index

        width: cellWidth
        height: cellHeight

        hoverEnabled: true
        cursorShape: Qt.OpenHandCursor

        objectName: "buttonsList"

        drag.target: buttonDragItem

        drag.smoothed: false

        readonly property int mIndex: modelData.id

        readonly property bool selected: GridView.isCurrentItem && root.activeFocus

        Accessible.role: Accessible.ListItem
        Accessible.name: modelData.text
        Accessible.selected: selected

        readonly property ColorContext colorContext: ColorContext {
            colorSet: ColorContext.Item

            focused: cell.selected
            hovered: cell.containsMouse
        }

        drag.onActiveChanged: {
            if (drag.active) {
                root.controlDragStarted(mIndex)

                buttonDragItem.text = modelData.label
                buttonDragItem.Drag.source = this
                buttonDragItem.Drag.start()

                GridView.delayRemove = true
            } else {
                buttonDragItem.Drag.drop()

                root.controlDragStopped(mIndex)

                GridView.delayRemove = false
            }
        }

        onPressed: (mouse) => {
            root.currentIndex = index
            const pos = mapToItem(buttonDragItem.parent, mouseX, mouseY)
            buttonDragItem.y = pos.y + VLCStyle.dragDelta
            buttonDragItem.x = pos.x + VLCStyle.dragDelta
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_xxxsmall

            color: cell.containsMouse && !buttonDragItem.Drag.active ? colorContext.bg.highlight
                                                                     : "transparent"
            radius: VLCStyle.radius_normal

            border.width: cell.selected ? VLCStyle.focus_border : 0
            border.color: colorContext.visualFocus

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: VLCStyle.margin_xsmall

                spacing: VLCStyle.margin_xxsmall

                // Figma: the icon in a dashed square
                Item {
                    Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)
                    Layout.alignment: Qt.AlignHCenter

                    EditorDashedBorder {
                        anchors.fill: parent
                        color: colorContext.border
                    }

                    EditorDummyButton {
                        anchors.fill: parent

                        visible: text !== ""
                        font.pixelSize: VLCStyle.icon_normal
                        color: colorContext.fg.primary
                        text: modelData.label
                    }

                    // the Divider is a line
                    Rectangle {
                        anchors.centerIn: parent

                        visible: modelData.id === ControlListModel.DIVIDER
                        width: VLCStyle.border
                        height: VLCStyle.dp(16, VLCStyle.scale)

                        color: colorContext.fg.primary
                    }
                }

                Widgets.ListSubtitleLabel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Figma 2107:5604: regular labels, quieter than the icons
                    color: colorContext.fg.secondary
                    elide: Text.ElideNone
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.weight: Font.Normal
                    maximumLineCount: 2
                    text: modelData.text
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignTop
                }
            }
        }
    }
}
