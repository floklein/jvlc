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

// Videos: one level of the folder tree of the library, the folders the user
// added at the top, then their subfolders and videos.
Widgets.PageExt {
    id: root

    // the folder listed (MLItemId), the top of the tree when unset
    property var parentId: undefined

    property alias initialIndex: videoAll.initialIndex
    property alias currentIndex: videoAll.currentIndex

    title: qsTr("Videos")

    breadcrumb: [qsTr("Videos"), ...treeModel.path.map((folder) => folder.title)]

    itemCount: treeModel.count

    paneAddAction: ({ "text": qsTr("Add"), "action": () => DialogsProvider.addLibraryFolderDialog() })

    headerActions: Component {
        Row {
            spacing: VLCStyle.pageHeader_actionSpacing // Figma 2139:8878
            Widgets.IconToolButton {
                font.pixelSize: VLCStyle.icon_banner
                width: VLCStyle.bannerButton_width
                height: VLCStyle.bannerButton_height
                color: VLCStyle.pageToolColor(this)

                // the Library pane has a "+ Add" row instead
                visible: !MainCtx.libraryPane

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
        type: FolderActions.Videos
    }

    // Functions

    // opens a folder of the tree, its top when id is undefined
    function showFolder(id, reason) {
        History.push([...root.pagePrefix], (id !== undefined) ? { parentId: id } : {}, reason)
    }

    // Events

    onBreadcrumbClicked: (index, reason) => {
        root.showFolder(index > 0 ? treeModel.path[index - 1].id : undefined, reason)
    }

    // folders default to Title A-Z
    Component.onCompleted: {
        root.sort.setDefault("title", Qt.AscendingOrder)
    }

    // Children

    VideoAll {
        id: videoAll

        anchors.fill: parent

        focus: true

        model: MLFolderTreeModel {
            id: treeModel

            ml: MediaLib

            parentId: root.parentId

            searchPattern: root.search.pattern
            sortCriteria: root.sort.criteria
            sortOrder: root.sort.order
        }

        sortModel: [
            { text: qsTr("Unwatched First"), criteria: "unwatched", order: Qt.AscendingOrder },
            { text: qsTr("Watched First"), criteria: "unwatched", order: Qt.DescendingOrder },
            { text: qsTr("Newest First"), criteria: "insertion", order: Qt.DescendingOrder },
            { text: qsTr("Oldest First"), criteria: "insertion", order: Qt.AscendingOrder },
            { text: qsTr("Title A-Z"), criteria: "title", order: Qt.AscendingOrder },
            { text: qsTr("Title Z-A"), criteria: "title", order: Qt.DescendingOrder }
        ]

        // folders and videos are mixed, there is no section
        sectionProperty: ""

        contextMenu: MLContextMenu { model: treeModel; showPlayAsAudioAction: true }

        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd
        enableBeginningFade: root.enableBeginningFade
        enableEndFade: root.enableEndFade

        onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

        // a folder opens, a video plays with the videos after it (the Play rule)
        function onAction(indexes) {
            if (indexes.length === 1) {
                const folder = treeModel.getDataAt(indexes[0])
                if (folder.isFolder) {
                    root.showFolder(folder.id, Qt.TabFocusReason)
                    return
                }
            }

            treeModel.addAndPlay(indexes)
            MainCtx.playerView = true
        }

        function onDoubleClick(object) {
            if (object.isFolder)
                root.showFolder(object.id, Qt.MouseFocusReason)
            else
                onAction([treeModel.index(object.index, 0)])
        }

        function isInfoExpandPanelAvailable(modelIndexData) {
            return !modelIndexData.isFolder
        }
    }
}
