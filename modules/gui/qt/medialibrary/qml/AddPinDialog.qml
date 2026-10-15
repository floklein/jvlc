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
import VLC.Network
import VLC.Dialogs
import VLC.Widgets as Widgets
import VLC.Style

// "+ Add item" of the library sidebar: pick a folder, a Playlist or a stream to Pin
Item {
    id: host

    // the item blurred behind the dialog
    property Item rootWindow: null

    Connections {
        target: Pins

        function onPickerRequested() {
            root.openPicker()
        }
    }

    ModalDialog {
        id: root

        rootWindow: host.rootWindow

        title: qsTr("Pin to Sidebar")

        padding: VLCStyle.margin_normal

        width: Math.min(VLCStyle.dp(420, VLCStyle.scale), (parent ? parent.width : Infinity) - VLCStyle.margin_large * 2)
        height: Math.min(VLCStyle.dp(520, VLCStyle.scale), (parent ? parent.height : Infinity) - VLCStyle.margin_large * 2)

        // the kinds of item, in the tab order
        readonly property var _sources: [{
                "text": qsTr("Folders"),
                "type": Pins.FOLDER,
                "model": folders,
                "label": "title",
                "empty": qsTr("No folders yet")
            }, {
                // the folders of Music ▸ Songs
                "text": qsTr("Music Folders"),
                "type": Pins.MUSIC_FOLDER,
                "model": musicFolders,
                "label": "title",
                "empty": qsTr("No music folders yet")
            }, {
                "text": qsTr("Playlists"),
                "type": Pins.PLAYLIST,
                "model": playlists,
                "label": "name",
                "empty": qsTr("No playlists yet")
            }, {
                // Saved streams and Network Playlists, keyed by their id
                "text": qsTr("Streams"),
                "type": Pins.STREAM,
                "model": SavedStreams,
                "label": "name",
                "empty": qsTr("No saved streams yet")
            }
        ]

        property int currentSource: 0

        readonly property var _source: _sources[currentSource]

        function openPicker() {
            searchField.text = ""
            open()
            searchField.forceActiveFocus(Qt.PopupFocusReason)
        }

        function _choose(id, label) {
            Pins.pin(_source.type, id.toString(), label)
            close()
        }

        MLVideoFoldersModel {
            id: folders

            ml: MediaLib
            sortCriteria: "title"
            searchPattern: searchField.text
        }

        MLVideoFoldersModel {
            id: musicFolders

            ml: MediaLib
            music: true
            sortCriteria: "title"
            searchPattern: searchField.text
        }

        MLPlaylistListModel {
            id: playlists

            ml: MediaLib
            playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL
            searchPattern: searchField.text
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

            RowLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_xsmall

                Repeater {
                    model: root._sources

                    delegate: SourceTab {
                        required property var modelData
                        required property int index

                        text: modelData.text
                        checked: root.currentSource === index

                        onClicked: root.currentSource = index
                    }
                }
            }

            Widgets.TextFieldExt {
                id: searchField

                Layout.fillWidth: true

                placeholderText: qsTr("Search")
                radius: VLCStyle.radius_small

                Accessible.name: placeholderText

                Keys.onDownPressed: list.forceActiveFocus(Qt.TabFocusReason)
                onAccepted: {
                    if (list.count > 0)
                        list.itemAtIndex(0)?.chosen()
                }
            }

            ListView {
                id: list

                Layout.fillWidth: true
                Layout.fillHeight: true

                clip: true
                keyNavigationEnabled: true

                Accessible.role: Accessible.List
                Accessible.name: root._source.text

                model: root._source.model

                delegate: PickerRow {
                    required property int index
                    required property var model

                    readonly property string label: model[root._source.label] ?? ""
                    readonly property bool pinned: Pins.count >= 0 // re-evaluated on every pin change
                                                    && Pins.isPinned(root._source.type, model.id.toString())

                    // the Saved streams have no search of their own
                    readonly property bool matches: searchField.text.length === 0
                                                    || label.toLowerCase().includes(searchField.text.toLowerCase())

                    width: list.width
                    height: matches ? implicitHeight : 0
                    visible: matches
                    text: label
                    enabled: !pinned
                    pinnedState: pinned
                    highlighted: ListView.isCurrentItem && list.activeFocus

                    onChosen: root._choose(model.id, label)
                }

                Keys.onReturnPressed: {
                    if (currentItem)
                        currentItem.chosen()
                }

                T.Label {
                    anchors.centerIn: parent
                    visible: list.count === 0 && !root._source.model.loading
                    text: searchField.text.length > 0 ? qsTr("Nothing matches") : root._source.empty
                    color: root.colorContext.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                }
            }
        }
    }

    component SourceTab: T.TabButton {
        id: tab

        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        implicitHeight: VLCStyle.dp(32, VLCStyle.scale)

        padding: VLCStyle.margin_small

        Accessible.role: Accessible.PageTab
        Accessible.name: text

        background: Rectangle {
            radius: VLCStyle.radius_small
            color: (tab.checked || tab.hovered) ? root.colorContext.bg.highlight : "transparent"
        }

        contentItem: T.Label {
            text: tab.text
            verticalAlignment: Text.AlignVCenter
            color: tab.checked ? root.colorContext.accent : root.colorContext.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
        }
    }

    component PickerRow: T.ItemDelegate {
        id: row

        property bool pinnedState: false

        signal chosen()

        implicitHeight: VLCStyle.dp(40, VLCStyle.scale)
        leftPadding: VLCStyle.margin_small
        rightPadding: VLCStyle.margin_small

        Accessible.role: Accessible.ListItem
        Accessible.name: text

        background: Rectangle {
            radius: VLCStyle.radius_small
            color: (row.hovered || row.highlighted) ? root.colorContext.bg.highlight : "transparent"
        }

        contentItem: RowLayout {
            spacing: VLCStyle.margin_small

            T.Label {
                Layout.fillWidth: true

                text: row.text
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                color: row.pinnedState ? root.colorContext.fg.secondary : root.colorContext.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            T.Label {
                visible: row.pinnedState
                text: qsTr("Pinned")
                color: root.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
            }
        }

        onClicked: chosen()
    }
}
