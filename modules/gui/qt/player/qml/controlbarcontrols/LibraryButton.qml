/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
import VLC.Style
import VLC.Playlist
import VLC.Util

// The library toggle
Widgets.IconToolButton {
    id: root

    signal requestLockUnlockAutoHide(bool lock)

    // Figma: 28 px square
    implicitWidth: VLCStyle.dp(28, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

    text: VLCIcons.sidebar_left
    description: qsTr("Library")

    AccessibleCompat.id: "topBarLibraryButton"

    checked: LibraryPages.paneOpen

    onHoveredChanged: root.requestLockUnlockAutoHide(hovered)

    // opens and closes the Library pane beside the video
    onClicked: LibraryPages.paneOpen = !LibraryPages.paneOpen
}
