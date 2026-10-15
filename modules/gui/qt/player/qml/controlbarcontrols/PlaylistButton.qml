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


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Style
import VLC.Util


// The Queue button of the Control bar
Widgets.IconToolButton {
    id: playlistBtn

    text: VLCIcons.queue
    onClicked: {
        MainCtx.playqueuePanel.visible = !MainCtx.playqueuePanel.visible
        if (MainCtx.playqueuePanel.visible && MainCtx.playqueuePanel.docked) {
            playlistWidget.gainFocus(playlistBtn)
        }
    }
    checked: MainCtx.playqueuePanel.visible

    description: qsTr("Queue")
    shortcut: "Ctrl+L" // GlobalShortcuts.qml

    // Figma "Icon Wrapper": 24 px icon, 4 px padding, 30 px wide
    leftPadding: VLCStyle.dp(3, VLCStyle.scale)
    rightPadding: VLCStyle.dp(3, VLCStyle.scale)
    topPadding: VLCStyle.dp(4, VLCStyle.scale)
    bottomPadding: VLCStyle.dp(4, VLCStyle.scale)

    toggle: true

    AccessibleCompat.id: "playqueueButton"

    // drop files from the library or the file manager to add them to the
    // Queue, even when the panel is closed
    DropArea {
        anchors.fill: parent

        onEntered: (drag) => {
            if (Helpers.isValidInstanceOf(drag.source, Widgets.DragItem))
                // not the Queue's own rows
                drag.accepted = !(drag.source.view?.model instanceof PlaylistListModel)
            else
                drag.accepted = drag.hasUrls
        }

        onDropped: (drop) => {
            if (Helpers.isValidInstanceOf(drop.source, Widgets.DragItem)) {
                drop.source.getSelectedInputItem().then((inputItems) => {
                    if (Helpers.isArray(inputItems) && inputItems.length > 0)
                        MainPlaylistController.append(inputItems, false)
                })
            } else if (drop.hasUrls) {
                const urlList = []
                for (let url in drop.urls)
                    urlList.push(drop.urls[url])
                MainPlaylistController.append(urlList, false)
            }

            // keep the window from playing the drop as well
            drop.accept(Qt.IgnoreAction)
        }
    }
}
