/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQml.Models
import QtQml

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Network
import VLC.Dialogs
import VLC.Menus

// The library's Network section and the pages it opens
Widgets.PageLoader {
    id: root

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    pageModel: [{
        name: "home",
        default: true,
        component: homeComponent
    }, {
        // a Network Playlist
        name: "playlist",
        component: playlistComponent,
        guard: function (prop) { return !!prop.playlistId }
    }, {
        // the root of a service: Podcasts, Radio…
        name: "service",
        component: serviceComponent,
        guard: function (prop) { return !!prop.source_name }
    }, {
        // a folder of a share or of a service
        name: "browse",
        component: browseComponent,
        guard: function (prop) { return !!prop.tree }
    }]

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Network view")

    function _showHome(reason) {
        History.push([...root.pagePrefix, "home"], reason)
    }

    function _showPlaylist(playlistId, reason) {
        History.push([...root.pagePrefix, "playlist"], { playlistId: playlistId }, reason)
    }

    function _showService(sourceName, reason) {
        History.push([...root.pagePrefix, "service"], { source_name: sourceName }, reason)
    }

    function _showBrowseNode(tree, sourceName, reason) {
        History.push([...root.pagePrefix, "browse"], { tree: tree, source_name: sourceName }, reason)
    }

    Component {
        id: homeComponent

        NetworkHomeDisplay {
            onShowPlaylist: (id, name, reason) => root._showPlaylist(id, reason)
            onShowService: (sourceName, title, reason) => root._showService(sourceName, reason)
            onBrowse: (tree, reason) => root._showBrowseNode(tree, "", reason)
        }
    }

    Component {
        id: playlistComponent

        NetworkPlaylistDisplay { }
    }

    Component {
        id: serviceComponent

        BrowseTreeDisplay {
            id: servicePage

            property alias source_name: deviceModel.source_name

            readonly property bool isPodcasts: source_name === "podcast"

            model: deviceModel
            contextMenu: isPodcasts ? podcastMenu : deviceMenu
            headerPath: [{display: deviceModel.name, tree: {}}]

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            // + Add podcast
            headerAction: isPodcasts ? addPodcastButton : null

            onHomeButtonClicked: reason => root._showHome(reason)
            onBrowse: (tree, reason) => root._showBrowseNode(tree, deviceModel.source_name, reason)

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            NetworkDeviceModel {
                id: deviceModel

                ctx: MainCtx
                sd_source: NetworkDeviceModel.CAT_INTERNET

                searchPattern: servicePage.search.pattern
                sortOrder: servicePage.sort.order
                sortCriteria: servicePage.sort.criteria
            }

            NetworkDeviceContextMenu {
                id: deviceMenu

                model: deviceModel
                ctx: MainCtx
            }

            // Play, Add to Queue, Remove Podcast
            QtObject {
                id: podcastMenu

                property var _indexes: []

                function popup(indexes, globalPos) {
                    _indexes = indexes
                    menu.popup(globalPos, [qsTr("Play"), qsTr("Add to Queue"), qsTr("Remove Podcast")])
                }

                function tableView_popup(index, indexes, globalPos) {
                    popup(indexes, globalPos)
                }

                readonly property StringListMenu menu: StringListMenu {
                    ctx: MainCtx

                    onSelected: (index, _) => {
                        const indexes = podcastMenu._indexes
                        switch (index) {
                        case 0:
                            deviceModel.addAndPlay(indexes)
                            break
                        case 1:
                            deviceModel.addToPlaylist(indexes)
                            break
                        case 2:
                            for (const modelIndex of indexes)
                                PodcastFeeds.remove(deviceModel.getDataAt(modelIndex.row).mrl)
                            break
                        }
                    }
                }
            }

            Component {
                id: addPodcastButton

                Widgets.ButtonExt {
                    iconTxt: VLCIcons.add
                    text: qsTr("Add Podcast")
                    onClicked: podcastDialog.ask(qsTr("Add Podcast"), qsTr("Address of the podcast feed"), "",
                                                 (url) => PodcastFeeds.add(url))
                }
            }

            TextInputDialog {
                id: podcastDialog

                acceptText: qsTr("Add")
                placeholderText: qsTr("e.g., https://www.example.com/podcast.rss")
            }
        }
    }

    Component {
        id: browseComponent

        BrowseTreeDisplay {
            id: browsePage

            property alias tree: mediaModel.tree
            // the service the folder belongs to, empty for a share
            property string source_name: ""

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade

            model: NetworkMediaModel {
                id: mediaModel

                ctx: MainCtx

                sortCriteria: browsePage.sort.criteria
                sortOrder: browsePage.sort.order
                searchPattern: browsePage.search.pattern
            }

            headerPath: {
                if (source_name === "")
                    return mediaModel.path
                // the root of the service, then the path within it
                return [{display: mediaModel.name, tree: {"source_name": source_name, "isRoot": true}}]
                       .concat(mediaModel.path)
            }

            contextMenu: NetworkMediaContextMenu {
                model: mediaModel
                ctx: MainCtx
            }

            Navigation.cancelAction: function() {
                History.previous(Qt.BacktabFocusReason)
            }

            onBrowse: (tree, reason) => {
                if (tree.isRoot)
                    root._showService(tree.source_name, reason)
                else
                    root._showBrowseNode(tree, source_name, reason)
            }

            onHomeButtonClicked: reason => root._showHome(reason)

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex
        }
    }
}
