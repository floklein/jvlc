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
import VLC.MediaLibrary
import VLC.Dialogs

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

// Songs: one level of the music folder tree of the library, the folders the
// user added at the top, then their subfolders and tracks.
Widgets.PageExt {
    id: root

    // the folder listed (MLItemId), the top of the tree when unset
    property var parentId: undefined

    property int initialIndex: 0

    readonly property alias currentIndex: trackList.currentIndex

    title: qsTr("Songs")

    breadcrumb: [qsTr("Songs"), ...treeModel.path.map((folder) => folder.title)]

    itemCount: treeModel.count

    hasGridListMode: false
    isSearchable: true

    // as in Videos, with Unplayed and Played for Unwatched and Watched
    sortModel: [
        { text: qsTr("Unplayed First"), criteria: "unplayed", order: Qt.AscendingOrder },
        { text: qsTr("Played First"), criteria: "played", order: Qt.AscendingOrder },
        { text: qsTr("Title A-Z"), criteria: "title", order: Qt.AscendingOrder },
        { text: qsTr("Title Z-A"), criteria: "title", order: Qt.DescendingOrder },
        { text: qsTr("Artist"), criteria: "main_artist", order: Qt.AscendingOrder },
        { text: qsTr("Album"), criteria: "album_title", order: Qt.AscendingOrder },
        { text: qsTr("Track"), criteria: "track_number", order: Qt.AscendingOrder },
        { text: qsTr("Disc"), criteria: "disc_number", order: Qt.AscendingOrder },
        { text: qsTr("Newest First"), criteria: "insertion", order: Qt.DescendingOrder },
        { text: qsTr("Oldest First"), criteria: "insertion", order: Qt.AscendingOrder }
    ]

    headerActions: Component {
        Row {
            spacing: VLCStyle.pageHeader_actionSpacing // Figma 2139:8878
            Widgets.IconToolButton {
                font.pixelSize: VLCStyle.icon_banner
                width: VLCStyle.bannerButton_width
                height: VLCStyle.bannerButton_height
                color: VLCStyle.pageToolColor(this)

                text: VLCIcons.add
                description: qsTr("Add a folder to the library")

                onClicked: DialogsProvider.addLibraryFolderDialog()
            }

            Widgets.IconToolButton {
                font.pixelSize: VLCStyle.icon_banner
                width: VLCStyle.bannerButton_width
                height: VLCStyle.bannerButton_height
                color: VLCStyle.pageToolColor(this)

                text: VLCIcons.more
                description: qsTr("Folder menu")

                checked: pageMenu.visible

                onClicked: pageMenu.popup([], mapToGlobal(0, height))
            }
        }
    }

    // the page "…" menu
    FolderPageMenu {
        id: pageMenu

        folderId: root.parentId
        type: FolderActions.Songs
    }

    // Functions

    // opens a folder of the tree, its top when id is undefined
    function showFolder(id, reason) {
        History.push([...root.pagePrefix], (id !== undefined) ? { parentId: id } : {}, reason)
    }

    // a folder opens, a track plays with the tracks after it (the Play rule)
    function _actionAt(indexes) {
        if (indexes.length === 1) {
            const row = treeModel.getDataAt(indexes[0])
            if (row?.isFolder) {
                root.showFolder(row.id, Qt.TabFocusReason)
                return
            }
        }

        treeModel.addAndPlay(indexes)
        MainCtx.playerView = true
    }

    function setCurrentItemFocus(reason) {
        if (trackList.visible)
            trackList.setCurrentItemFocus(reason)
        else
            emptyLabel.forceActiveFocus(reason)
    }

    function positionContentAtBeginning() {
        trackList.positionViewAtBeginning()
    }

    // Events

    onBreadcrumbClicked: (index, reason) => {
        root.showFolder(index > 0 ? treeModel.path[index - 1].id : undefined, reason)
    }

    // albums on disk read best in their order: by disc, then track
    Component.onCompleted: {
        if (root.sort.criteria === "") {
            root.sort.criteria = "disc_number"
            root.sort.order = Qt.AscendingOrder
        }
    }

    // Children

    MLMusicFolderTreeModel {
        id: treeModel

        ml: MediaLib

        parentId: root.parentId

        searchPattern: root.search.pattern
        sortCriteria: root.sort.criteria
        sortOrder: root.sort.order

        onLoadingChanged: {
            if (!loading && root.initialIndex > 0 && root.initialIndex < count) {
                trackList.positionViewAtIndex(root.initialIndex, ItemView.Contain)
                trackList.currentIndex = root.initialIndex
                root.initialIndex = 0
            }
        }
    }

    MLContextMenu {
        id: contextMenu

        model: treeModel
    }

    Widgets.TableViewExt {
        id: trackList

        // Private

        readonly property var _lineTitle: ({
            criteria: "title",
            text: qsTr("Title"),
            isSortable: false,
            colDelegate: tableColumns.titleDelegate,
            headerDelegate: tableColumns.titleHeaderDelegate,
            placeHolder: VLCStyle.noArtAlbumCover
        })

        readonly property var _lineAlbum: ({
            criteria: "album_title",
            text: qsTr("Album"),
            isSortable: false
        })

        readonly property var _lineArtist: ({
            criteria: "main_artist",
            text: qsTr("Artist"),
            isSortable: false
        })

        readonly property var _lineDuration: ({
            criteria: "duration",
            text: qsTr("Duration"),
            isSortable: false,
            colDelegate: tableColumns.timeColDelegate,
            headerDelegate: tableColumns.timeHeaderDelegate
        })

        readonly property var _modelLarge: [
            { weight: 1, model: _lineTitle },
            { weight: 1, model: _lineAlbum },
            { weight: 1, model: _lineArtist },
            { size: 1, model: _lineDuration }
        ]

        readonly property var _modelSmall: [{
            weight: 1,
            model: ({
                criteria: "title",
                subCriterias: [ "main_artist", "duration" ],
                text: qsTr("Title"),
                isSortable: false,
                colDelegate: tableColumns.titleDelegate,
                headerDelegate: tableColumns.titleHeaderDelegate,
                placeHolder: VLCStyle.noArtAlbumCover
            })
        }]

        anchors.fill: parent

        visible: treeModel.count > 0
        focus: treeModel.count > 0

        model: treeModel

        sortModel: (_availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall : _modelLarge

        // the column headers don't sort, the page header does
        sortingFromHeader: false

        // folders and tracks are mixed, there is no section
        section.property: ""

        rowHeight: VLCStyle.tableCoverRow_height

        dragItem: tableDragItem

        rowContextMenu: contextMenu

        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd

        fadingEdge.enableBeginningFade: root.enableBeginningFade
        fadingEdge.enableEndFade: root.enableEndFade

        Navigation.parentItem: root
        Navigation.upItem: root.header

        Navigation.cancelAction: function() {
            if (trackList.currentIndex <= 0)
                root.Navigation.defaultNavigationCancel()
            else
                trackList.currentIndex = 0
        }

        onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

        onActionForSelection: (selection) => root._actionAt(selection)

        onItemDoubleClicked: (index, model) => root._actionAt([treeModel.index(index, 0)])

        onRightClick: (_, _, globalMousePos) => {
            contextMenu.popup(selectionModel.selectedIndexes, globalMousePos)
        }

        MLDragItem {
            id: tableDragItem

            indexes: indexesFlat ? trackList.selectionModel.selectedIndexesFlat
                                 : trackList.selectionModel.selectedIndexes
            indexesFlat: !!trackList.selectionModel.selectedIndexesFlat

            view: trackList
        }

        MLTableColumns {
            id: tableColumns

            showCriterias: (trackList.sortModel === trackList._modelSmall)

            viewModel: treeModel
        }
    }

    Widgets.EmptyLabelButton {
        id: emptyLabel

        anchors.centerIn: parent

        visible: !treeModel.loading && (treeModel.count <= 0)
        focus: visible

        text: (root.parentId === undefined && root.search.pattern === "")
              ? qsTr("No music found\nPlease try adding sources")
              : qsTr("No music found")

        cover: VLCStyle.noArtAlbumCover

        Navigation.parentItem: root
        Navigation.upItem: root.header
    }
}
