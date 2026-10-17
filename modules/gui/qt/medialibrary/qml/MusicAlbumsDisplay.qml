/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

Widgets.PageExt {
    id: root

    title: qsTr("Albums")

    itemCount: albumsView.model.count

    // albums default to Title A-Z, the first entry of their sort menu; with no
    // criteria, the page's descending order listed them Z-A
    Component.onCompleted: {
        root.sort.setDefault("title", Qt.AscendingOrder)
    }

    MusicAlbums {
        id: albumsView

        anchors.fill: parent

        focus: true

        searchPattern: root.search.pattern
        sortCriteria: root.sort.criteria
        sortOrder: root.sort.order

        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd

        enableBeginningFade: root.enableBeginningFade
        enableEndFade: root.enableEndFade

        onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
    }

}
