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

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util

Widgets.PageLoader {
    id: root

    pageModel: [{
        name: "base",
        component: homeComponent,
        default: true
    }, {
        name: "continueWatching",
        component: continueWatchingComponent
    }, {
        name: "listenAgain",
        component: listenAgainComponent
    }, {
        name: "folders",
        component: foldersComponent
    }, {
        name: "playlists",
        component: playlistsComponent
    }, {
        name: "artists",
        component: artistsComponent
    }, {
        name: "streams",
        component: streamsComponent
    }, {
        name: "favorites",
        component: favoritesComponent
    }]

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Home View")
    AccessibleCompat.id: "homeDisplay"

    // Home lists what lives in the other sections: open it there

    // a folder of songs only opens in Music ▸ Songs
    function showFolder(model, reason) {
        if (model.nb_videos === 0 && model.nb_audio > 0)
            History.push(["music", "tracks"], { parentId: model.id }, reason)
        else
            History.push(["video", "folders"], { parentId: model.id }, reason)
    }

    function showPlaylist(model, reason) {
        History.push(["playlists", "list"], { parentId: model.id, title: model.name }, reason)
    }

    function showArtist(id, reason) {
        History.push(["music", "artists", "albums"], { artistId: id }, reason)
    }


    Component {
        id: homeComponent

        HomePage {
            focus: true

            onSeeAllButtonClicked: (name, reason) => {
                History.push([...root.pagePrefix, name], reason)
            }

            onFolderRequested: (model, reason) => root.showFolder(model, reason)
            onPlaylistRequested: (model, reason) => root.showPlaylist(model, reason)
            onArtistRequested: (id, reason) => root.showArtist(id, reason)
        }
    }

    Component {
        id: continueWatchingComponent

        Widgets.PageExt {
            id: continueWatchingPage

            title: qsTr("Continue Watching")

            VideoAll {
                id: continueWatching

                anchors.fill: parent

                focus: true

                model: MLRecentVideoModel {
                    ml: MediaLib

                    sortCriteria: continueWatchingPage.sort.criteria
                    sortOrder: continueWatchingPage.sort.order
                    searchPattern: continueWatchingPage.search.pattern

                    readonly property bool needToNavigateBack: !loading && count === 0
                                                               && continueWatchingPage.search.pattern === ""

                    onNeedToNavigateBackChanged: {
                        if (needToNavigateBack)
                            History.previous(Qt.BacktabFocusReason)
                    }
                }

                sectionProperty: model.sortCriteria === "title" ? "title_first_symbol" : ""

                contextMenu: MLContextMenu {
                    model: continueWatching.model

                    showPlayAsAudioAction: true
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade
            }
        }
    }

    Component {
        id: listenAgainComponent

        Widgets.PageExt {
            id: listenAgainPage

            title: qsTr("Listen Again")

            MediaView {
                focus: true

                anchors.fill: parent

                showArtist: true

                model: MLRecentAudioModel {
                    ml: MediaLib

                    searchPattern: listenAgainPage.search.pattern
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade
            }
        }
    }

    Component {
        id: foldersComponent

        Widgets.PageExt {
            id: foldersPage

            title: qsTr("Frequently Visited Folders")

            FrequentFoldersView {
                focus: true

                anchors.fill: parent

                searchPattern: foldersPage.search.pattern

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade

                onFolderRequested: (model, reason) => root.showFolder(model, reason)
            }
        }
    }

    Component {
        id: playlistsComponent

        Widgets.PageExt {
            id: playlistsPage

            title: qsTr("Frequently Visited Playlists")

            PlaylistMediaList {
                focus: true

                anchors.fill: parent

                playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL

                sortCriteria: "playcount"
                sortOrder: Qt.DescendingOrder
                searchPattern: playlistsPage.search.pattern

                // ranked rows, the page offers no other sort
                sortModel: []

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade

                onShowList: (model, reason) => root.showPlaylist(model, reason)
            }
        }
    }

    Component {
        id: artistsComponent

        Widgets.PageExt {
            id: artistsPage

            title: qsTr("Frequently Listened Artists")

            MusicAllArtists {
                focus: true

                anchors.fill: parent

                sortCriteria: "playcount"
                sortOrder: Qt.DescendingOrder
                searchPattern: artistsPage.search.pattern

                // ranked rows, the page offers no other sort
                sortModel: []

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade

                onArtistAlbumViewRequested: (id, reason) => root.showArtist(id, reason)
            }
        }
    }

    Component {
        id: streamsComponent

        Widgets.PageExt {
            id: streamsPage

            title: qsTr("Latest Streams")

            MediaView {
                focus: true

                anchors.fill: parent

                model: MLRecentMediaModel {
                    ml: MediaLib

                    historyType: MLRecentMediaModel.NETWORK

                    searchPattern: streamsPage.search.pattern
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade
            }
        }
    }

    Component {
        id: favoritesComponent

        Widgets.PageExt {
            id: favoritesPage

            title: qsTr("Favorites")

            // the favorite Playlists first, as in the Home row
            PlaylistMediaList {
                id: favoritePlaylists

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                // at most half the page: the media follow
                height: visible ? Math.min(currentItem?.contentHeight ?? implicitHeight,
                                           parent.height / 2)
                                : 0

                visible: model.count !== 0

                playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL
                favoriteOnly: true
                searchPattern: favoritesPage.search.pattern

                emptyLabel: null

                displayMarginBeginning: root.displayMarginBeginning
                enableBeginningFade: root.enableBeginningFade
                enableEndFade: false

                Navigation.downItem: favoriteMedia

                onShowList: (model, reason) => root.showPlaylist(model, reason)
            }

            MediaView {
                id: favoriteMedia

                focus: true

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: favoritePlaylists.bottom
                anchors.bottom: parent.bottom

                Navigation.upItem: favoritePlaylists.visible ? favoritePlaylists : null

                model: MLMediaModel {
                    favoriteOnly: true

                    ml: MediaLib

                    sortCriteria: favoritesPage.sort.criteria || "insertion"
                    sortOrder: favoritesPage.sort.order
                    searchPattern: favoritesPage.search.pattern
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                enableBeginningFade: root.enableBeginningFade
                enableEndFade: root.enableEndFade
            }
        }
    }
}
