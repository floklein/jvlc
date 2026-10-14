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
import VLC.Util
import VLC.Network
import VLC.Dialogs
import VLC.Menus
import VLC.Playlist

// The Network section: + Add Network Stream, the Saved streams and Network
// Playlists, the Podcasts and Radio services, then the local network shares.
Widgets.PageExt {
    id: root

    title: qsTr("Network")

    // a Network Playlist to open
    signal showPlaylist(string id, string name, int reason)
    // a service (Podcasts, Radio…) to browse
    signal showService(string sourceName, string title, int reason)
    // a share of the local network to browse
    signal browse(var tree, int reason)

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Network view")

    // the services list ends with an "Add a service" row, hidden here
    readonly property int itemCount: SavedStreams.count + Math.max(0, servicesRepeater.count - 1)
                                     + sharesRepeater.count

    function setCurrentItemFocus(reason) {
        addRow.forceActiveFocus(reason)
    }

    // the name shown for a service of VLC
    function serviceTitle(name, longName) {
        if (name === "podcast")
            return qsTr("Podcasts")
        if (name === "icecast")
            return qsTr("Radio")
        return longName
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    Flickable {
        id: flickable

        anchors.fill: parent
        anchors.leftMargin: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_left_margin
        anchors.rightMargin: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_right_margin

        contentWidth: width
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

        ColumnLayout {
            id: column

            width: flickable.width
            spacing: 0

            Widgets.CaptionLabel {
                text: qsTr("%1 item", "%1 items", root.itemCount).arg(root.itemCount)
                color: theme.fg.secondary
                Layout.bottomMargin: VLCStyle.spacing_10
            }

            Rectangle {
                implicitHeight: VLCStyle.border
                color: theme.separator
                Layout.fillWidth: true
                Layout.bottomMargin: VLCStyle.spacing_10
            }

            NetworkRow {
                id: addRow

                text: qsTr("Add Network Stream")
                iconText: VLCIcons.add
                focus: true

                Layout.fillWidth: true

                onClicked: DialogsProvider.openNetDialog()
            }

            // Saved streams and Network Playlists, newest first
            Repeater {
                model: SavedStreams

                delegate: NetworkRow {
                    text: model.name
                    subtitle: model.isPlaylist ? "" : (model.url !== model.name ? model.url : "")
                    iconText: model.isPlaylist ? VLCIcons.playlist : VLCIcons.stream
                    hasMenu: true

                    Layout.fillWidth: true

                    onClicked: {
                        if (model.isPlaylist)
                            root.showPlaylist(model.id, model.name, Qt.MouseFocusReason)
                        else
                            MainPlaylistController.playMedia(model.urls)
                    }

                    onMenuRequested: (globalPos) => streamMenu.show(model.id, globalPos)
                }
            }

            // Podcasts, Radio and the other services of VLC
            Repeater {
                id: servicesRepeater

                model: NetworkSourcesModel {
                    ctx: MainCtx
                }

                delegate: NetworkRow {
                    // "Add a service" is in Settings
                    visible: model.type !== NetworkSourcesModel.TYPE_DUMMY
                    text: root.serviceTitle(model.name, model.long_name)
                    iconText: VLCIcons.topbar_network

                    Layout.fillWidth: true

                    onClicked: root.showService(model.name, text, Qt.MouseFocusReason)
                }
            }

            // UPnP, SMB… shares of the local network
            Repeater {
                id: sharesRepeater

                model: NetworkDeviceModel {
                    ctx: MainCtx

                    sd_source: NetworkDeviceModel.CAT_LAN
                    source_name: "*"
                }

                delegate: NetworkRow {
                    text: model.name
                    iconText: VLCIcons.folder

                    Layout.fillWidth: true

                    onClicked: root.browse(model.tree, Qt.MouseFocusReason)
                }
            }

            Item {
                Layout.preferredHeight: VLCStyle.margin_normal
            }
        }
    }

    // Play, Add to Queue, Rename, Remove
    QtObject {
        id: streamMenu

        property string streamId: ""

        function show(id, globalPos) {
            streamId = id
            menu.popup(globalPos, [qsTr("Play"), qsTr("Add to Queue"), qsTr("Rename…"), qsTr("Remove")])
        }

        readonly property StringListMenu menu: StringListMenu {
            ctx: MainCtx

            onSelected: (index, _) => {
                const stream = SavedStreams.get(streamMenu.streamId)
                if (!stream.id)
                    return
                switch (index) {
                case 0:
                    MainPlaylistController.playMedia(stream.urls)
                    break
                case 1:
                    MainPlaylistController.append(stream.urls, false)
                    break
                case 2:
                    renameDialog.acceptEmpty = true
                    renameDialog.ask(stream.isPlaylist ? qsTr("Rename Playlist") : qsTr("Rename Stream"),
                                     qsTr("Name"), stream.name,
                                     (text) => SavedStreams.rename(stream.id, text))
                    break
                case 3:
                    SavedStreams.remove(stream.id)
                    break
                }
            }
        }
    }

    TextInputDialog {
        id: renameDialog

        acceptText: qsTr("Rename")
    }
}
