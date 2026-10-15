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

import VLC.MainInterface

// The library folders whose media were played the most, most played first
VideoAll {
    id: root

    property alias searchPattern: foldersModel.searchPattern
    property alias limit: foldersModel.limit

    signal folderRequested(var model, int reason)

    model: MLVideoFoldersModel {
        id: foldersModel

        ml: MediaLib

        // the folders of songs too
        anyMedia: true

        sortCriteria: "playcount"
        sortOrder: Qt.DescendingOrder
    }

    // ranked by play count, the view offers no other sort
    sortModel: []

    sectionProperty: ""

    showGroupCountColumn: true

    contextMenu: MLContextMenu { model: foldersModel }

    gridLabels: function (model) {
        if (!model)
            return ""
        const count = (n) => n < 100 ? n : "99+"
        const labels = []
        if (model.nb_videos > 0)
            labels.push(qsTr("%1 Videos").arg(count(model.nb_videos)))
        if (model.nb_audio > 0)
            labels.push(qsTr("%1 Songs").arg(count(model.nb_audio)))
        return labels
    }

    listLabels: getLabel

    function onAction(indexes) {
        root.folderRequested(foldersModel.getDataAt(indexes[0]), Qt.TabFocusReason)
    }

    function onDoubleClick(object) {
        root.folderRequested(object, Qt.MouseFocusReason)
    }
}
