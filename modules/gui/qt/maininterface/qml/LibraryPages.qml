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
pragma Singleton

import QtQml

import VLC.MainInterface

// The sections of the library, shared by the full-window library and the
// Library pane, and the state of the Library pane
QtObject {
    // the Library pane is open beside the video (session only): the library
    // toggle and the folder button of the Top bar open it, ↗ and Esc close it
    property bool paneOpen: false

    readonly property var pageModel: [
        {
            name: "home",
            url: MainCtx.mediaLibraryAvailable ?
                 "qrc:///qt/qml/VLC/MediaLibrary/HomeDisplay.qml" :
                 "qrc:///qt/qml/VLC/MainInterface/NoMedialibHome.qml"
        }, {
            name: "video",
            url: "qrc:///qt/qml/VLC/MediaLibrary/VideoDisplay.qml"
        }, {
            name: "music",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MusicDisplay.qml"
        }, {
            name: "network",
            url: "qrc:///qt/qml/VLC/Network/NetworkDisplay.qml"
        }, {
            name: "playlists",
            url: "qrc:///qt/qml/VLC/MediaLibrary/PlaylistsDisplay.qml"
        }, {
            name: "recent",
            url: "qrc:///qt/qml/VLC/MediaLibrary/RecentDisplay.qml"
        }
    ]

    // Opens the library at the folder of the playing file (its MLItemId, from
    // MLMediaFolder): in the Videos folder tree for a video, at its top when
    // the folder is not in the library; in Music for anything else.
    function showFolder(folderId, isVideo : bool, focusReason : int) {
        // Back leads Home from there
        if (History.viewPath.length === 0)
            History.update(["home"])

        if (!isVideo)
            History.push(["music"], {}, focusReason)
        else if (folderId && folderId.isValid())
            History.push(["video", "folders"], { parentId: folderId }, focusReason)
        else
            History.push(["video", "folders"], {}, focusReason)
    }
}
