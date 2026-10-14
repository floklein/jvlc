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

import VLC.MediaLibrary

// The Playlist dialogs, opened from anywhere through PlaylistManager:
// New Playlist and the "More…" picker of Add to Playlist
Item {
    id: root

    // the item blurred behind the dialogs
    property Item rootWindow: null

    Connections {
        target: PlaylistManager

        function onNewPlaylistRequested(items) {
            newPlaylist.openWith(items)
        }

        function onPickerRequested(items) {
            picker.openWith(items)
        }
    }

    NewPlaylistDialog {
        id: newPlaylist

        rootWindow: root.rootWindow
    }

    PlaylistPicker {
        id: picker

        rootWindow: root.rootWindow

        onNewPlaylistRequested: (items) => newPlaylist.openWith(items)
    }
}
