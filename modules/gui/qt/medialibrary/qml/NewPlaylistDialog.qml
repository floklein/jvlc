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
import VLC.Util
import VLC.Style

// New Playlist: a name (required), an optional thumbnail and optional files
ModalDialog {
    id: root

    // items given by the caller (Save Queue as Playlist, Add to Playlist…)
    property var items: []
    // files the user adds here
    property var files: []
    property url thumbnail: ""

    readonly property int _itemCount: (items?.length ?? 0) + (files?.length ?? 0)
    readonly property int _dash: VLCStyle.dp(4, VLCStyle.scale)
    // the dark "well" of the text field, also behind the header, thumbnail and drop zone
    readonly property color _well: nameField.colorContext.bg.primary
    readonly property color _dashColor: Qt.alpha(colorContext.fg.secondary, 0.5)

    title: qsTr("New Playlist")

    padding: VLCStyle.margin_large
    topPadding: VLCStyle.margin_large

    width: Math.min(VLCStyle.dp(744, VLCStyle.scale), (parent ? parent.width : Infinity) - VLCStyle.margin_large * 2)
    // NOTE: In a small window, the drop zone shrinks.
    height: Math.min(implicitHeight, (parent ? parent.height : Infinity) - VLCStyle.margin_large * 2)

    function openWith(newItems) {
        items = newItems ?? []
        files = []
        thumbnail = ""
        nameField.text = ""
        open()
        nameField.forceActiveFocus(Qt.PopupFocusReason)
    }

    function save() {
        const name = nameField.text.trim()
        if (name.length === 0)
            return
        PlaylistManager.create(name, items.concat(files), thumbnail)
        close()
    }

    function _addFiles(urls) {
        const list = files.slice()
        for (const url of urls)
            list.push(url)
        files = list
    }

    function _pickFiles() {
        _addFiles(PlaylistManager.pickFiles())
    }

    header: Rectangle {
        // NOTE: Popup sections are outside the item tree of the dialog colorContext.
        readonly property ColorContext colorContext: ColorContext {
            palette: root.colorContext.palette
            colorSet: ColorContext.Window
        }

        implicitHeight: VLCStyle.dp(50, VLCStyle.scale)
        color: root._well

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

        spacing: VLCStyle.margin_normal

        RowLayout {
            Layout.fillWidth: true
            spacing: VLCStyle.margin_large

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: VLCStyle.margin_xsmall

                T.Label {
                    text: qsTr("Playlist Details")
                    color: root.colorContext.fg.primary
                    font.pixelSize: VLCStyle.fontSize_xlarge
                }

                T.Label {
                    Layout.fillWidth: true
                    text: qsTr("The playlist will be saved and displayed with this title and thumbnail.")
                    color: root.colorContext.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    wrapMode: Text.Wrap
                }

                Widgets.TextFieldExt {
                    id: nameField

                    Layout.fillWidth: true
                    Layout.topMargin: VLCStyle.margin_small

                    placeholderText: qsTr("e.g., Favorite Videos")
                    radius: VLCStyle.radius_small

                    Accessible.name: qsTr("Playlist name")

                    onAccepted: root.save()
                }
            }

            // the thumbnail picker
            Widgets.ButtonExt {
                id: thumbnailButton

                Layout.preferredWidth: VLCStyle.dp(108, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(108, VLCStyle.scale)

                iconTxt: root.thumbnail.toString() === "" ? VLCIcons.add : ""
                text: ""
                radius: VLCStyle.radius_small

                Accessible.name: qsTr("Choose a thumbnail")

                onClicked: {
                    const picked = PlaylistManager.pickThumbnail()
                    if (picked.toString() !== "")
                        root.thumbnail = picked
                }

                Rectangle {
                    anchors.fill: parent
                    z: -1
                    radius: VLCStyle.radius_small
                    color: root._well
                }

                Image {
                    anchors.fill: parent
                    anchors.margins: VLCStyle.border
                    visible: root.thumbnail.toString() !== ""
                    source: root.thumbnail
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize: Qt.size(width, height)
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_xsmall
            Layout.bottomMargin: VLCStyle.margin_xsmall
            implicitHeight: VLCStyle.border
            color: root.colorContext.separator
        }

        T.Label {
            text: qsTr("Select Files")
            color: root.colorContext.fg.primary
            font.pixelSize: VLCStyle.fontSize_xlarge
        }

        T.Label {
            Layout.fillWidth: true
            text: qsTr("These files will be added to the playlist.")
            color: root.colorContext.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
            wrapMode: Text.Wrap
        }

        // the drop zone: drag files in, or click to pick them
        Rectangle {
            id: dropZone

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: VLCStyle.dp(184, VLCStyle.scale)
            Layout.minimumHeight: VLCStyle.dp(96, VLCStyle.scale)

            color: dropArea.containsDrag ? root.colorContext.bg.highlight : root._well
            radius: VLCStyle.radius_small

            // the dashed border
            Repeater {
                model: Math.ceil(dropZone.width / (root._dash * 2))

                delegate: Item {
                    required property int index

                    Rectangle {
                        x: index * root._dash * 2
                        width: Math.min(root._dash, dropZone.width - x)
                        height: VLCStyle.border
                        color: root._dashColor
                    }

                    Rectangle {
                        x: index * root._dash * 2
                        y: dropZone.height - height
                        width: Math.min(root._dash, dropZone.width - x)
                        height: VLCStyle.border
                        color: root._dashColor
                    }
                }
            }

            Repeater {
                model: Math.ceil(dropZone.height / (root._dash * 2))

                delegate: Item {
                    required property int index

                    Rectangle {
                        y: index * root._dash * 2
                        width: VLCStyle.border
                        height: Math.min(root._dash, dropZone.height - y)
                        color: root._dashColor
                    }

                    Rectangle {
                        x: dropZone.width - width
                        y: index * root._dash * 2
                        width: VLCStyle.border
                        height: Math.min(root._dash, dropZone.height - y)
                        color: root._dashColor
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root._pickFiles()
            }

            DropArea {
                id: dropArea

                anchors.fill: parent

                onEntered: (drag) => {
                    drag.accepted = drag.hasUrls || Helpers.isValidInstanceOf(drag.source, Widgets.DragItem)
                }

                onDropped: (drop) => {
                    if (Helpers.isValidInstanceOf(drop.source, Widgets.DragItem)) {
                        drop.accepted = true
                        drop.source.getSelectedInputItem().then((inputItems) => root._addFiles(inputItems))
                    } else if (drop.hasUrls) {
                        drop.accepted = true
                        root._addFiles(drop.urls)
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - VLCStyle.margin_large * 2
                spacing: VLCStyle.margin_small

                T.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: root._itemCount === 0
                          ? qsTr("Drag files into the box or click here to select files.")
                          : qsTr("%1 item will be added.", "%1 items will be added.", root._itemCount).arg(root._itemCount)
                    color: root.colorContext.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                }

                T.Label {
                    Layout.fillWidth: true
                    visible: root.files.length > 0
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                    maximumLineCount: 3
                    wrapMode: Text.Wrap
                    text: root.files.map((file) => decodeURIComponent(String(file).split("/").pop())).join(", ")
                    color: root.colorContext.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }

                Widgets.ButtonExt {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: VLCStyle.dp(84, VLCStyle.scale)
                    iconTxt: VLCIcons.add
                    text: ""
                    radius: VLCStyle.radius_small

                    Accessible.name: qsTr("Select files")

                    onClicked: root._pickFiles()
                }
            }
        }
    }

    footer: Item {
        readonly property ColorContext colorContext: ColorContext {
            palette: root.colorContext.palette
            colorSet: ColorContext.Window
        }

        implicitHeight: buttons.implicitHeight + VLCStyle.margin_large * 2

        RowLayout {
            id: buttons

            anchors.fill: parent
            anchors.margins: VLCStyle.margin_large

            Widgets.ButtonExt {
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                text: qsTr("Cancel")

                onClicked: root.close()
            }

            Item {
                Layout.fillWidth: true
            }

            Widgets.ActionButtonPrimary {
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                text: qsTr("Save")
                enabled: nameField.text.trim().length > 0

                onClicked: root.save()
            }
        }
    }
}
