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


import VLC.Style
import VLC.Util
import VLC.PlayerControls
import VLC.Widgets as Widgets

// One zone of a bar in Customize Player: its items can be dragged, and
// selected with the keyboard (Enter removes, Alt+Left/Right moves)
ListView {
    id: playerBtnDND

    spacing: VLCStyle.margin_xsmall
    orientation: Qt.Horizontal
    clip: true

    currentIndex: -1
    highlightFollowsCurrentItem: false
    boundsBehavior: Flickable.StopAtBounds

    activeFocusOnTab: true
    keyNavigationEnabled: true

    // the Layout of the edited player
    property PlayerControlbarModel bars: null

    property bool containsDrag: footerItem.dropVisible

    property alias scrollBar: scrollBar

    property bool extraWidthAvailable: true

    // asks the editor to select an item in another zone after a move
    signal selectRequested(var zone, int index)

    Accessible.role: Accessible.List

    T.ScrollBar.horizontal: Widgets.ScrollBarExt {
        id: scrollBar

        visible: playerBtnDND.contentWidth > playerBtnDND.width
    }

    onActiveFocusChanged: {
        if (activeFocus && currentIndex < 0 && count > 0)
            currentIndex = 0
    }

    remove: Transition {
        OpacityAnimator {
            from: 1.0
            to: 0.0

            duration: VLCStyle.duration_long
        }
    }

    Keys.onPressed: (event) => {
        if (currentIndex < 0 || !bars)
            return

        if ((event.modifiers & Qt.AltModifier)
                && (event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
            const at = bars.shiftControl(model, currentIndex, (event.key === Qt.Key_Left) ? -1 : 1)
            if (at.zone)
                playerBtnDND.selectRequested(at.zone, at.index)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                   || event.key === Qt.Key_Delete) {
            const index = currentIndex
            if (bars.removeControl(model, index))
                currentIndex = Math.min(index, count - 1)
            event.accepted = true
        }
    }

    // whether the dragged item may drop here
    function acceptsDrag(drag) {
        if (!bars || !drag.source)
            return false
        const id = (drag.source.objectName === "buttonsList") ? drag.source.mIndex
                                                               : drag.source.controlId
        return bars.canPlace(id, model)
    }

    function dropEvent(drag, destIndex) {
        if (drag.source.dndView === playerBtnDND) {
            // moving from same section
            bars.moveControl(model, drag.source.DelegateModel.itemsIndex,
                             model, destIndex)
            drag.source.visible = true
            drag.accept(Qt.MoveAction)
        } else if (drag.source.objectName === "buttonsList") {
            // adding from the palette
            bars.addControl(model, destIndex, drag.source.mIndex)
            drag.accept(Qt.CopyAction)
        } else {
            // moving between sections or views
            if (bars.moveControl(drag.source.dndView.model, drag.source.DelegateModel.itemsIndex,
                                 model, destIndex))
                drag.accept(Qt.MoveAction)
            else
                drag.source.visible = true
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    VLCFlickableScrollHandler {
        fallbackScroll: true
        enabled: true
    }

    MouseArea {
        anchors.fill: parent

        preventStealing: true

        z: -1

        cursorShape: root.dragActive ? Qt.DragMoveCursor : Qt.ArrowCursor
    }

    footer: Item {
        implicitHeight: playerBtnDND.height
        implicitWidth: Math.max(implicitHeight, playerBtnDND.width - x)

        property alias dropVisible: footerDropArea.containsDrag

        Rectangle {
            id: footerDropIndicator
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left

            z: 2

            implicitWidth: VLCStyle.dp(2, VLCStyle.scale)
            implicitHeight: VLCStyle.icon_medium

            visible: dropVisible
            color: theme.accent
        }

        DropArea {
            id: footerDropArea

            anchors {
                left: parent.left
                right: parent.right
                top: footerDropIndicator.top
                bottom: footerDropIndicator.bottom
            }

            onEntered: (drag) => {
                if (!playerBtnDND.acceptsDrag(drag)
                        || (drag.source.dndView === playerBtnDND &&
                            drag.source.DelegateModel.itemsIndex === playerBtnDND.count - 1)) {
                    drag.accepted = false
                }
            }

            onDropped: (drop) => {
                let destIndex = playerBtnDND.count

                if (drop.source.dndView === playerBtnDND)
                    --destIndex

                dropEvent(drop, destIndex)
            }
        }
    }

    delegate: EditorDNDDelegate {
        height: Math.min((contentItem.implicitHeight > 0) ? contentItem.implicitHeight : Number.MAX_VALUE, VLCStyle.controlLayoutHeight)

        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        dndView: playerBtnDND

        Binding {
            when: dropArea.containsDrag
            value: true

            target: playerBtnDND
            property: "containsDrag"
        }
    }
}
