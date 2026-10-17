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
import VLC.Util

// The Library pane (Figma 187:328): the library beside the video, as an icon
// rail and a list of the current page with the library header. It shows the
// library where the full-window library is (both follow History), and ↗ in
// its header opens the full-window library.
FocusScope {
    id: root

    // over the video (fullscreen and Hide UI): the Theme's Tint, over the
    // Video blur where there is one
    property bool overlaid: false

    // NOTE: At large scales or in a small window it leaves room for the video:
    //       240 dp, or half the window.
    implicitWidth: {
        // Figma 848:1996: 427 px with its icon-only sidebar
        const width = rail.width - VLCStyle.navigationRail_width + VLCStyle.libraryPane_width
        if (Window.width <= 0)
            return width
        return Math.min(width, Math.max(Window.width / 2, Window.width - VLCStyle.dp(240, VLCStyle.scale)))
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Library pane")
    AccessibleCompat.id: "libraryPane"

    // Functions

    function loadCurrentHistoryView(focusReason) {
        pageLoader.loadView(History.viewPath, History.viewProp, focusReason)
    }

    // opens a page of the rail, or scrolls back to the top of the open one
    function _openPage(uri) {
        if (pageLoader.isDefaulLoadedForPath([...uri])
            || (uri.length > 0 && History.exactMatch(History.viewPath, uri))) {
            pageLoader.positionContentAtBeginning()
            return
        }

        History.push(uri)
    }

    // Events

    Component.onCompleted: {
        if (History.viewPath.length === 0)
            History.update(["home"])
        loadCurrentHistoryView(Qt.OtherFocusReason)
    }

    Keys.onPressed: (event) => {
        if (KeyHelper.matchSearch(event)) {
            MainCtx.search.askShow()
            event.accepted = true
        }
    }

    Connections {
        target: History

        function onNavigate(focusReason) {
            root.loadCurrentHistoryView(focusReason)
        }
    }

    // Children

    ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Widgets.TintBackground {
        anchors.fill: parent

        overVideo: root.overlaid
        colorContext: theme
    }

    SideNavigationPane {
        id: rail

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        // Figma 848:1996: 50 px, the icons centered, 8 px from the top
        width: VLCStyle.navigationRail_width + VLCStyle.applicationHorizontalMargin

        delegateShowText: false

        topPadding: VLCStyle.margin_xsmall
        leftPadding: (VLCStyle.navigationRail_width - VLCStyle.navigationItem_iconWidth) / 2
        rightPadding: leftPadding

        safeAreaLeftMargin: VLCStyle.applicationHorizontalMargin

        // the Tint of the pane shows through over the video
        background.visible: !root.overlaid

        NavigationBarContextMenu {
            id: railMenu

            ctx: MainCtx

            model: rail.model

            onPageRequested: (uri) => root._openPage(uri)
        }

        onItemClicked: (modelUri, item) => {
            // the sections of an expandable item open in a menu
            if (item.expandable) {
                railMenu.popup(item.index, item.mapToGlobal(item.width, 0), !item.hovered)
                return
            }

            root._openPage(modelUri)
        }

        Navigation.parentItem: root
        Navigation.rightItem: pageLoader
    }

    Widgets.PageLoader {
        id: pageLoader

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: rail.right
        anchors.right: parent.right

        focus: true

        clip: true

        pageModel: LibraryPages.pageModel

        Navigation.parentItem: root
        Navigation.leftItem: rail
    }
}
