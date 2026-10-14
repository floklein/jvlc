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
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

// The Recent section: the media played most recently, newest first, grouped
// by Today, Yesterday, This week and Earlier. Removing media from Recent or
// clearing it never changes their watched state or progress.
Widgets.PageExt {
    id: root

    property var pagePrefix: [] // behave like a Page

    title: qsTr("Recent")

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Recent view")
    AccessibleCompat.id: "recentDisplay"

    headerActions: Component {
        Widgets.IconToolButton {
            font.pixelSize: VLCStyle.icon_banner
            width: VLCStyle.bannerButton_width
            height: VLCStyle.bannerButton_height

            text: VLCIcons.more
            description: qsTr("Recent menu")

            checked: pageMenu.visible

            onClicked: pageMenu.popup([], mapToGlobal(0, height))
        }
    }

    MediaView {
        focus: true

        anchors.fill: parent

        model: recentModel

        // a list grouped by day, newest first
        grid: null
        currentComponent: list
        sortModel: []
        listSectionProperty: "recent_group"

        emptyLabel: Component {
            Widgets.EmptyLabel {
                focus: true

                cover: VLCStyle.noArtAlbumCover

                text: root.search.pattern === "" ? qsTr("Nothing played recently")
                                                 : qsTr("No media found")
            }
        }

        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd

        enableBeginningFade: root.enableBeginningFade
        enableEndFade: root.enableEndFade
    }

    MLRecentMediaModel {
        id: recentModel

        ml: MediaLib

        searchPattern: root.search.pattern
    }

    // the page "…" menu
    NativeMenu {
        id: pageMenu

        actions: [{
            "text": qsTr("Clear Recent"),
            "action": () => recentModel.clearRecent(),
            "visible": () => recentModel.count > 0
        }]

        onRequestData: (requestID) => setData(requestID, [])
    }
}
