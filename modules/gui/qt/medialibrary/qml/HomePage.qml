/******************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Author: Ash <ashutoshv191@gmail.com>
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
 ******************************************************************************/
import QtQuick
import QtQuick.Window
import QtQuick.Templates as T

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Dialogs

// Home: rows to resume what was played. Empty rows are hidden, and each row's
// chevron opens its full list.
Widgets.PageExt {
    id: root

    property var pagePrefix: [] // behave like a Page

    title: qsTr("Home")

    hasGridListMode: true

    isSearchable: true

    property real listCoverHeight: VLCStyle.listAlbumCover_height
    property real listCoverWidth: VLCStyle.listAlbumCover_width
    property real listCoverRadius: VLCStyle.listAlbumCover_radius

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    signal seeAllButtonClicked(string name, int reason)
    signal folderRequested(var model, int reason)
    signal playlistRequested(var model, int reason)
    signal artistRequested(var id, int reason)

    readonly property var _rows: [continueWatchingRow, listenAgainRow, foldersRow, playlistsRow,
                                  artistsRow, streamsRow, favoritePlaylistsRow, favoritesRow]

    // A single media is enough to tell an empty library
    readonly property MLMediaModel _libraryMedia: MLMediaModel {
        ml: MediaLib
        limit: 1
    }

    readonly property bool _libraryEmpty: !_libraryMedia.loading && _libraryMedia.maximumCount === 0

    readonly property bool _rowsLoading: _rows.some((row) => row.model.loading)

    readonly property bool _hasRows: _rows.some((row) => row.visible)

    function _rowLimit(row) {
        // FIXME: Make limit 0 load no items, instead of loading all items.
        return MainCtx.gridView ? Math.max(row.currentItem?.nbItemPerRow ?? null, 1) : 5
    }

    function positionContentAtBeginning() {
        contentYBehavior.enabled = true
        flickable.contentY = -flickable.originY
        contentYBehavior.enabled = false
    }

    function setCurrentItemFocus(reason) {
        // `focus` is checked, because that indicates the item that has the focus within the focus scope
        const row = _rows.find((row) => row.visible && row.focus) ?? _rows.find((row) => row.visible)
        if (row)
            row.setCurrentItemFocus(reason)
        else if (addFolderButton.visible)
            addFolderButton.forceActiveFocus(reason)
    }

    function _showItem(item) {
        contentYBehavior.enabled = true
        Helpers.positionFlickableToContainItem(flickable, item)
        contentYBehavior.enabled = false
    }

    function _currentRowItem(row) {
        // FIXME: `ExpandGridView` does not have `currentItem`.
        return row.currentItem?.currentItem ?? row.currentItem?._getItem(row.currentIndex)
    }

    function _onRowActiveFocusChanged(row) {
        if (row.activeFocus)
            _showItem(_currentRowItem(row) ?? row)
    }

    function _onRowCurrentIndexChanged(row) {
        if (!row.activeFocus)
            return
        const item = _currentRowItem(row)
        if (item)
            _showItem(item)
    }

    // the Library pane (Figma 1106:5633) has smaller titles: 16 px
    component RowHeader: Widgets.ViewHeader {
        seeAllChevron: true
        seeAllButton.visible: true
        titlePixelSize: MainCtx.libraryPane ? VLCStyle.fontSize_xlarge : VLCStyle.fontSize_sectionTitle
    }

    // the Library pane (Figma 1106:5633) lists the rows without column titles
    component PaneRowsWithoutTitles: Binding {
        required property Item row

        target: row.currentItem
        property: "tableHeaderVisible"
        value: false
        // the list, not the grid
        when: MainCtx.libraryPane && row.currentItem?.tableHeaderVisible !== undefined
    }

    PaneRowsWithoutTitles { row: continueWatchingRow }
    PaneRowsWithoutTitles { row: listenAgainRow }
    PaneRowsWithoutTitles { row: foldersRow }
    PaneRowsWithoutTitles { row: playlistsRow }
    PaneRowsWithoutTitles { row: artistsRow }
    PaneRowsWithoutTitles { row: streamsRow }
    PaneRowsWithoutTitles { row: favoritePlaylistsRow }
    PaneRowsWithoutTitles { row: favoritesRow }

    // the Library pane (Figma 1106:5633) counts the media of the library
    itemCount: (MainCtx.libraryPane && !_libraryMedia.loading) ? _libraryMedia.maximumCount : -1

    contentItem: Flickable {
        id: flickable

        implicitWidth: mediaRows.implicitWidth
        implicitHeight: mediaRows.implicitHeight

        contentWidth: width
        contentHeight: root._hasRows ? mediaRows.implicitHeight
                                     : height - topMargin - bottomMargin

        flickableDirection: Flickable.AutoFlickIfNeeded

        boundsBehavior: Flickable.StopAtBounds

        // FIXME: make the media rows positioners consider the flickable margins when adjusting flickable `contentY`.
        bottomMargin: VLCStyle.margin_large

        pixelAligned: (MainCtx.qtVersion() >= MainCtx.qtVersionCheck(6, 2, 5)) // QTBUG-103996
                      && (Screen.pixelDensity >= VLCStyle.highPixelDensityThreshold) // no need for sub-pixel alignment with high pixel density

        T.ScrollBar.vertical: Widgets.ScrollBarExt {}

        // This behavior allows to have similar "smooth" animation
        // that Qt views have with `highlightFollowsCurrentItem`.
        Behavior on contentY {
            id: contentYBehavior

            enabled: false

            // NOTE: Usage of `SmoothedAnimation` is intentional here.
            SmoothedAnimation {
                duration: VLCStyle.duration_veryLong
                easing.type: Easing.InOutSine
            }
        }

        property Component implicitFlickableScrollHandler: DefaultFlickableScrollHandler { }

        // NOTE: This property can be set to null to prevent using a scroll handler:
        property FlickableScrollHandler scrollHandler: {
            if (interactive) {
                // JS ownership:
                return implicitFlickableScrollHandler.createObject(null, { target: flickable })
            } else {
                return null
            }
        }

        Component.onCompleted: {
            if (!usingTouch) {
                // Flickable filters child mouse events for flicking (even when
                // the delegate is grabbed). However, this is not a useful
                // feature for non-touch cases, so disable it here and enable
                // it if touch is detected through the hover handler:
                MainCtx.setFiltersChildMouseEvents(flickable, false)
            }
        }

        readonly property bool usingTouch: MainCtx.usingTouch

        onUsingTouchChanged: {
            if (usingTouch)
                MainCtx.setFiltersChildMouseEvents(flickable, true)
            // We do not disable filtering child mouse events
            // because Qt currently has a bug that the flickable
            // jumps when it is enabled again later on.
        }

        // An empty library offers a single action: adding a folder
        FocusScope {
            id: emptyLibrary

            anchors.centerIn: parent

            visible: root._libraryEmpty
            focus: visible

            implicitWidth: emptyLibraryColumn.implicitWidth
            implicitHeight: emptyLibraryColumn.implicitHeight

            Column {
                id: emptyLibraryColumn

                spacing: VLCStyle.margin_large

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter

                    sourceSize: Qt.size(0, VLCStyle.colWidth(1))

                    source: MainCtx.useXmasCone() ? "qrc:///logo/vlc48-xmas.png"
                                                  : "qrc:///misc/cone.svg"
                }

                Widgets.SubtitleLabel {
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: qsTr("Your library is empty")

                    color: theme.fg.primary
                }

                Widgets.ActionButtonPrimary {
                    id: addFolderButton

                    anchors.horizontalCenter: parent.horizontalCenter

                    AccessibleCompat.id: "addFolderButton"

                    focus: true

                    text: qsTr("Add a folder")

                    Navigation.parentItem: root
                    Navigation.upItem: root.header

                    onClicked: DialogsProvider.addLibraryFolderDialog()
                }
            }
        }

        Widgets.SubtitleLabel {
            anchors.centerIn: parent

            visible: !root._libraryEmpty && !root._rowsLoading && !root._hasRows

            text: root.search.pattern === "" ? qsTr("What you play shows up here")
                                             : qsTr("No results")

            color: theme.fg.secondary
        }

        Widgets.NavigableCol {
            id: mediaRows

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            visible: !root._libraryEmpty
            focus: visible

            spacing: 0 // NOTE: We depend on the top padding of view header currently.

            Navigation.parentItem: root
            Navigation.upItem: root.header

            RowHeader {
                visible: continueWatchingRow.visible
                text: qsTr("Continue Watching")
                view: continueWatchingRow

                onSeeAllButtonClicked: (reason) => root.seeAllButtonClicked("continueWatching", reason)
            }

            VideoAll {
                id: continueWatchingRow

                AccessibleCompat.id: "continueWatching"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                sectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLRecentVideoModel {
                    ml: MediaLib

                    searchPattern: root.search.pattern

                    limit: root._rowLimit(continueWatchingRow)
                }

                contextMenu: MLContextMenu {
                    model: continueWatchingRow.model

                    showPlayAsAudioAction: true
                }

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }

            RowHeader {
                visible: listenAgainRow.visible
                text: qsTr("Listen Again")
                view: listenAgainRow

                onSeeAllButtonClicked: (reason) => root.seeAllButtonClicked("listenAgain", reason)
            }

            MediaView {
                id: listenAgainRow

                AccessibleCompat.id: "listenAgain"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                listHeaderPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                listSectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLRecentAudioModel {
                    ml: MediaLib

                    searchPattern: root.search.pattern

                    limit: root._rowLimit(listenAgainRow)
                }

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }

            RowHeader {
                visible: foldersRow.visible
                text: qsTr("Frequently Visited Folders")
                view: foldersRow

                onSeeAllButtonClicked: (reason) => root.seeAllButtonClicked("folders", reason)
            }

            FrequentFoldersView {
                id: foldersRow

                AccessibleCompat.id: "frequentFolders"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                searchPattern: root.search.pattern

                limit: root._rowLimit(foldersRow)

                onFolderRequested: (model, reason) => root.folderRequested(model, reason)

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }

            RowHeader {
                visible: playlistsRow.visible
                text: qsTr("Frequently Visited Playlists")
                view: playlistsRow

                onSeeAllButtonClicked: (reason) => root.seeAllButtonClicked("playlists", reason)
            }

            PlaylistMediaList {
                id: playlistsRow

                AccessibleCompat.id: "frequentPlaylists"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false

                interactive: false

                emptyLabel: null

                playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL

                sortCriteria: "playcount"
                sortOrder: Qt.DescendingOrder
                searchPattern: root.search.pattern

                limit: root._rowLimit(playlistsRow)

                Navigation.parentItem: mediaRows

                onShowList: (model, reason) => root.playlistRequested(model, reason)

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }

            RowHeader {
                visible: artistsRow.visible
                text: qsTr("Frequently Listened Artists")
                view: artistsRow

                onSeeAllButtonClicked: (reason) => root.seeAllButtonClicked("artists", reason)
            }

            MusicAllArtists {
                id: artistsRow

                AccessibleCompat.id: "frequentArtists"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false

                interactive: false

                emptyLabel: null

                sortCriteria: "playcount"
                sortOrder: Qt.DescendingOrder
                searchPattern: root.search.pattern

                limit: root._rowLimit(artistsRow)

                Navigation.parentItem: mediaRows

                onArtistAlbumViewRequested: (id, reason) => root.artistRequested(id, reason)

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }

            RowHeader {
                visible: streamsRow.visible
                text: qsTr("Latest Streams")
                view: streamsRow

                onSeeAllButtonClicked: (reason) => root.seeAllButtonClicked("streams", reason)
            }

            MediaView {
                id: streamsRow

                AccessibleCompat.id: "latestStreams"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                listHeaderPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                listSectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLRecentMediaModel {
                    ml: MediaLib

                    historyType: MLRecentMediaModel.NETWORK

                    searchPattern: root.search.pattern

                    limit: root._rowLimit(streamsRow)
                }

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }

            // the Favorites: the Playlists, then the media
            RowHeader {
                visible: favoritePlaylistsRow.visible || favoritesRow.visible
                text: qsTr("Favorites")
                view: favoritePlaylistsRow.visible ? favoritePlaylistsRow : favoritesRow

                onSeeAllButtonClicked: (reason) => root.seeAllButtonClicked("favorites", reason)
            }

            PlaylistMediaList {
                id: favoritePlaylistsRow

                AccessibleCompat.id: "favouritePlaylists"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false

                interactive: false

                emptyLabel: null

                playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL

                favoriteOnly: true
                searchPattern: root.search.pattern

                limit: root._rowLimit(favoritePlaylistsRow)

                Navigation.parentItem: mediaRows

                onShowList: (model, reason) => root.playlistRequested(model, reason)

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }

            MediaView {
                id: favoritesRow

                AccessibleCompat.id: "favourites"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                listHeaderPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                listSectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLMediaModel {
                    favoriteOnly: true

                    ml: MediaLib

                    sortCriteria: "insertion"
                    sortOrder: Qt.DescendingOrder
                    searchPattern: root.search.pattern

                    limit: root._rowLimit(favoritesRow)
                }

                onActiveFocusChanged: root._onRowActiveFocusChanged(this)
                onCurrentIndexChanged: root._onRowCurrentIndexChanged(this)
            }
        }
    }
}
