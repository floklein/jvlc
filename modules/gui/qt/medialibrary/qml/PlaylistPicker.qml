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
import VLC.MediaLibrary
import VLC.Dialogs
import VLC.Widgets as Widgets
import VLC.Style

// "More…" of the Add to Playlist submenu: every Playlist, with search
ModalDialog {
    id: root

    // what to add
    property var items: []

    signal newPlaylistRequested(var items)

    title: qsTr("Add to Playlist")

    padding: VLCStyle.margin_normal

    width: Math.min(VLCStyle.dp(420, VLCStyle.scale), (parent ? parent.width : Infinity) - VLCStyle.margin_large * 2)
    height: Math.min(VLCStyle.dp(520, VLCStyle.scale), (parent ? parent.height : Infinity) - VLCStyle.margin_large * 2)

    function openWith(newItems) {
        items = newItems ?? []
        searchField.text = ""
        open()
        searchField.forceActiveFocus(Qt.PopupFocusReason)
    }

    function _choose(playlistId) {
        PlaylistManager.append(playlistId, items)
        close()
    }

    header: Rectangle {
        // NOTE: Popup sections are outside the item tree of the dialog colorContext.
        readonly property ColorContext colorContext: ColorContext {
            palette: root.colorContext.palette
            colorSet: ColorContext.Window
        }

        implicitHeight: VLCStyle.dp(50, VLCStyle.scale)
        color: root.colorContext.bg.secondary

        T.Label {
            anchors.left: parent.left
            anchors.leftMargin: VLCStyle.margin_normal
            anchors.verticalCenter: parent.verticalCenter

            text: root.title
            color: root.colorContext.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        Widgets.IconToolButton {
            anchors.right: parent.right
            anchors.rightMargin: VLCStyle.margin_xsmall
            anchors.verticalCenter: parent.verticalCenter

            text: VLCIcons.close
            description: qsTr("Close")

            onClicked: root.close()
        }
    }

    contentItem: ColumnLayout {
        readonly property ColorContext colorContext: ColorContext {
            palette: root.colorContext.palette
            colorSet: ColorContext.Window
        }

        spacing: VLCStyle.margin_small

        Widgets.TextFieldExt {
            id: searchField

            Layout.fillWidth: true

            placeholderText: qsTr("Search playlists")
            radius: VLCStyle.radius_small

            Accessible.name: placeholderText

            Keys.onDownPressed: list.forceActiveFocus(Qt.TabFocusReason)
            onAccepted: {
                if (list.count > 0)
                    root._choose(playlists.getItemId(0))
            }
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            keyNavigationEnabled: true

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Playlists")

            model: MLPlaylistListModel {
                id: playlists

                ml: MediaLib
                playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL
                searchPattern: searchField.text
                sortCriteria: "name"
            }

            header: PickerRow {
                width: list.width
                text: qsTr("+ New Playlist…")

                onChosen: {
                    root.close()
                    root.newPlaylistRequested(root.items)
                }
            }

            delegate: PickerRow {
                required property int index
                required property var model

                width: list.width
                text: model.name
                highlighted: ListView.isCurrentItem && list.activeFocus

                onChosen: root._choose(model.id)
            }

            Keys.onReturnPressed: {
                if (currentIndex >= 0)
                    root._choose(playlists.getItemId(currentIndex))
            }

            T.Label {
                anchors.centerIn: parent
                visible: list.count === 0 && !playlists.loading
                text: searchField.text.length > 0 ? qsTr("No playlist matches") : qsTr("No playlists yet")
                color: root.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
            }
        }
    }

    component PickerRow: T.ItemDelegate {
        id: row

        signal chosen()

        implicitHeight: VLCStyle.dp(40, VLCStyle.scale)
        leftPadding: VLCStyle.margin_small

        Accessible.role: Accessible.ListItem
        Accessible.name: text

        background: Rectangle {
            radius: VLCStyle.radius_small
            color: (row.hovered || row.highlighted) ? root.colorContext.bg.highlight : "transparent"
        }

        contentItem: T.Label {
            text: row.text
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            color: root.colorContext.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        onClicked: chosen()
    }
}
