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
import QtQml

import VLC.MediaLibrary
import VLC.Dialogs
import VLC.Util

// The page "…" menu of Videos and Songs: Add Folder to Library…, then,
// inside a folder, Add Subtitles to This Folder… (Videos), Rescan and
// Properties. At the top of the tree, Rescan rescans the whole library.
NativeMenu {
    id: root

    // the folder shown (MLItemId), the top of the tree when unset
    property var folderId: undefined

    // FolderActions.Videos or FolderActions.Songs
    property int type: FolderActions.Videos

    actions: [{
        "text": qsTr("Add Folder to Library…"),
        "action": () => DialogsProvider.addLibraryFolderDialog()
    }, {
        "text": qsTr("Add Subtitles to This Folder…"),
        "action": (dataList) => DialogsProvider.addFolderSubtitlesDialog(dataList[0].mrl),
        "visible": (dataList) => dataList.length === 1 && (dataList[0].path ?? "") !== ""
                                 && root.type !== FolderActions.Songs
    }, {
        "text": qsTr("Rescan"),
        "action": (dataList) => FolderActions.rescan(dataList.length === 1 ? dataList[0].mrl : "")
    }, {
        "separator": true
    }, {
        "text": qsTr("Properties"),
        "action": () => FolderActions.showProperties(root.folderId, root.type),
        "visible": (dataList) => dataList.length === 1
    }]

    // the data is the description of the folder, none at the top
    onRequestData: (requestID) => {
        if (root.folderId === undefined) {
            setData(requestID, [])
            return
        }

        FolderActions.describe(root.folderId, root.type, (folder) => {
            setData(requestID, Object.keys(folder).length > 0 ? [folder] : [])
        })
    }
}
