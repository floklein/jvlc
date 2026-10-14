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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Network
import VLC.Menus
import VLC.Playlist

// The streams of a Network Playlist, read again from its URLs each time it
// is opened. Play starts at the clicked stream and queues the following ones.
Widgets.PageExt {
    id: root

    // the id of the Network Playlist in SavedStreams
    property string playlistId: ""

    readonly property var _stream: {
        SavedStreams.count // refreshed when the list changes
        return SavedStreams.get(playlistId)
    }

    title: _stream.name ?? ""

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Network Playlist view")

    function setCurrentItemFocus(reason) {
        if (list.count > 0) {
            list.currentIndex = Math.max(list.currentIndex, 0)
            list.currentItem?.forceActiveFocus(reason)
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    NetworkPlaylistModel {
        id: playlistModel

        reader: NetworkPlaylistReader
        urls: root._stream.urls ?? []
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_left_margin
        anchors.rightMargin: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_right_margin

        spacing: VLCStyle.spacing_10

        RowLayout {
            spacing: VLCStyle.spacing_10
            Layout.fillWidth: true

            Widgets.CaptionLabel {
                text: playlistModel.loading ? qsTr("Loading…")
                                            : qsTr("%1 item", "%1 items", playlistModel.count).arg(playlistModel.count)
                color: theme.fg.secondary
                Layout.fillWidth: true
            }

            Widgets.IconToolButton {
                text: VLCIcons.ic_fluent_arrow_sync_24_regular
                description: qsTr("Read the playlist again")
                enabled: !playlistModel.loading
                font.pixelSize: VLCStyle.icon_normal
                onClicked: playlistModel.reload()
            }

            Widgets.ButtonExt {
                text: qsTr("Play")
                iconTxt: VLCIcons.play
                enabled: playlistModel.count > 0
                colorContext.colorSet: ColorContext.ButtonAccent
                onClicked: MainPlaylistController.playMedia(playlistModel.mrlsFrom(0))
            }
        }

        Rectangle {
            implicitHeight: VLCStyle.border
            color: theme.separator
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list

                anchors.fill: parent

                model: playlistModel
                clip: true
                focus: true
                keyNavigationEnabled: true
                boundsBehavior: Flickable.StopAtBounds

                T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                delegate: NetworkRow {
                    width: list.width

                    text: model.name
                    subtitle: model.name !== model.mrl ? model.mrl : ""
                    artwork: model.artwork ?? ""
                    iconText: VLCIcons.stream
                    hasMenu: true

                    onClicked: MainPlaylistController.playMedia(playlistModel.mrlsFrom(index))
                    onMenuRequested: (globalPos) => itemMenu.show(index, globalPos)
                }
            }

            Widgets.BusyIndicatorExt {
                anchors.centerIn: parent
                runningDelayed: playlistModel.loading && playlistModel.count === 0
            }

            T.Label {
                anchors.centerIn: parent
                visible: !playlistModel.loading && playlistModel.count === 0
                text: qsTr("This playlist could not be read.")
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_large
            }
        }
    }

    // Play, Add to Queue
    QtObject {
        id: itemMenu

        property int row: -1

        function show(index, globalPos) {
            row = index
            menu.popup(globalPos, [qsTr("Play"), qsTr("Add to Queue")])
        }

        readonly property StringListMenu menu: StringListMenu {
            ctx: MainCtx

            onSelected: (index, _) => {
                if (index === 0)
                    MainPlaylistController.playMedia(playlistModel.mrlsFrom(itemMenu.row))
                else
                    MainPlaylistController.append(playlistModel.mrlsFrom(itemMenu.row).slice(0, 1), false)
            }
        }
    }
}
