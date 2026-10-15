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
import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style

// The dialogs of the folder menus, opened from anywhere through FolderActions:
// Properties, the confirmation of Remove from Library and Exclude from
// Library, and the name of a "+ New Preset…" of Preset ▸
Item {
    id: host

    // the item blurred behind the dialogs
    property Item rootWindow: null

    Connections {
        target: FolderActions

        function onPropertiesRequested(description) {
            properties.show(description)
        }

        function onRemovalRequested(description) {
            removal.ask(description)
        }

        function onNewPresetRequested(path, name) {
            newPreset.ask(qsTr("New Preset"),
                          qsTr("It applies to the files of %1. The adjustments you make while they play go into it.").arg(name),
                          "",
                          (presetName) => {
                              const id = Presets.model.create(presetName)
                              if (id !== "")
                                  Presets.model.attach(path, id)
                          })
        }
    }

    TextInputDialog {
        id: newPreset

        rootWindow: host.rootWindow

        acceptText: qsTr("Create")
        placeholderText: qsTr("e.g., Anime")
        validate: (text) => Presets.model.isNameAvailable(text)
                            ? "" : qsTr("A Preset with this name already exists.")
    }

    // Remove from Library (a folder the user added) or Exclude from Library
    // (a subfolder)
    FolderDialog {
        id: removal

        rootWindow: host.rootWindow

        property var _folder: ({})

        readonly property bool _added: _folder.added === true

        function ask(description) {
            _folder = description
            open()
        }

        title: _added ? qsTr("Remove from Library") : qsTr("Exclude from Library")

        acceptText: _added ? qsTr("Remove") : qsTr("Exclude")

        onAccepted: {
            if (_added)
                FolderActions.removeFromLibrary(_folder.mrl)
            else
                FolderActions.excludeFromLibrary(_folder.mrl)
        }

        contentItem: T.Label {
            text: removal._added
                  ? qsTr("Remove “%1” from the library? Its media leave the library; the files stay on disk.").arg(removal._folder.name ?? "")
                  : qsTr("Exclude “%1” from the library? The library skips this folder from now on; the files stay on disk.").arg(removal._folder.name ?? "")
            color: removal.colorContext.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
            wrapMode: Text.Wrap
        }
    }

    // Properties of a folder
    FolderDialog {
        id: properties

        rootWindow: host.rootWindow

        property var _folder: ({})

        function show(description) {
            _folder = description
            open()
        }

        title: qsTr("Properties")

        acceptText: ""

        readonly property bool _songs: _folder.type === FolderActions.Songs

        readonly property var _rows: {
            const folder = properties._folder
            const rows = [
                [qsTr("Name"), folder.name ?? ""],
                [qsTr("Location"), folder.path || folder.mrl || ""],
                [qsTr("Contents"), properties._contents(folder)],
            ]
            if (!properties._songs && (folder.media ?? 0) > 0)
                rows.push([qsTr("Watched"), qsTr("%1 of %2").arg(folder.watched).arg(folder.media)])
            if ((folder.duration ?? 0) > 0)
                rows.push([qsTr("Duration"), properties._duration(folder.duration)])
            if ((folder.size ?? 0) > 0)
                rows.push([qsTr("Size"), properties._size(folder.size)])
            if (!properties._songs && (folder.path ?? "") !== "") {
                const id = Presets.model.presetOf(folder.path)
                rows.push([qsTr("Preset"), id === "" ? qsTr("Default") : Presets.model.name(id)])
            }
            rows.push([qsTr("Library"), folder.added ? qsTr("A folder you added")
                                                     : qsTr("Inside a folder you added")])
            return rows
        }

        function _contents(folder) {
            const media = folder.media ?? 0
            const folders = folder.folders ?? 0
            let text
            if (properties._songs)
                text = media === 1 ? qsTr("1 song") : qsTr("%1 songs").arg(media)
            else if (folder.type === FolderActions.Videos)
                text = media === 1 ? qsTr("1 video") : qsTr("%1 videos").arg(media)
            else
                text = media === 1 ? qsTr("1 file") : qsTr("%1 files").arg(media)
            if (folders === 0)
                return text
            return folders === 1 ? qsTr("%1 in 1 folder").arg(text)
                                 : qsTr("%1 in %2 folders").arg(text).arg(folders)
        }

        function _duration(ms) {
            const minutes = Math.round(ms / 60000)
            const hours = Math.floor(minutes / 60)
            if (hours === 0)
                return qsTr("%1 min").arg(minutes)
            return qsTr("%1 h %2 min").arg(hours).arg(minutes % 60)
        }

        function _size(bytes) {
            const units = [qsTr("%1 B"), qsTr("%1 KB"), qsTr("%1 MB"), qsTr("%1 GB"), qsTr("%1 TB")]
            let value = bytes
            let unit = 0
            while (value >= 1024 && unit < units.length - 1) {
                value /= 1024
                ++unit
            }
            return units[unit].arg(unit === 0 ? value : value.toFixed(1))
        }

        contentItem: GridLayout {
            columns: 2
            columnSpacing: VLCStyle.margin_normal
            rowSpacing: VLCStyle.margin_xsmall

            Repeater {
                model: properties._rows.length * 2

                T.Label {
                    required property int index

                    readonly property bool _isName: index % 2 === 0

                    Layout.fillWidth: !_isName
                    Layout.alignment: Qt.AlignTop
                    Layout.maximumWidth: _isName ? implicitWidth : -1

                    text: properties._rows[Math.floor(index / 2)][index % 2]
                    color: _isName ? properties.colorContext.fg.secondary : properties.colorContext.fg.primary
                    font.pixelSize: VLCStyle.fontSize_normal
                    wrapMode: _isName ? Text.NoWrap : Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }
    }

    // a dialog in the look of the other library dialogs: a title bar with
    // Close, the content, then Cancel and the accept button (none without
    // acceptText)
    component FolderDialog: ModalDialog {
        id: dialog

        property string acceptText: qsTr("OK")


        width: Math.min(VLCStyle.dp(480, VLCStyle.scale),
                        (T.Overlay.overlay?.width ?? Infinity) - 2 * VLCStyle.margin_large)

        padding: VLCStyle.spacing_20
        spacing: 0

        background: Rectangle {
            color: dialog.colorContext.bg.primary
            radius: VLCStyle.radius_xlarge

            Widgets.RoundedRectangleShadow {
                yOffset: VLCStyle.dp(8, VLCStyle.scale)
                blurRadius: VLCStyle.dp(15, VLCStyle.scale)
                radius: parent.radius
                opacity: 0.46
                color: dialog.colorContext.shadow.alpha(1.0)
            }
        }

        header: Rectangle {
            id: headerBar

            readonly property ColorContext colorContext: ColorContext {
                palette: VLCStyle.palette
                colorSet: ColorContext.View
            }

            implicitHeight: VLCStyle.dp(50, VLCStyle.scale)

            // only the top corners are rounded
            radius: VLCStyle.radius_xlarge
            color: headerBar.colorContext.bg.secondary

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.radius
                color: parent.color
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: VLCStyle.spacing_8
                anchors.leftMargin: VLCStyle.spacing_20

                T.Label {
                    text: dialog.title
                    color: headerBar.colorContext.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Widgets.IconToolButton {
                    text: VLCIcons.close
                    description: qsTr("Close")
                    font.pixelSize: VLCStyle.icon_normal
                    Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                    focusPolicy: Qt.NoFocus
                    onClicked: dialog.close()
                }
            }
        }

        footer: RowLayout {
            spacing: VLCStyle.spacing_10

            readonly property ColorContext colorContext: ColorContext {
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            Item { Layout.fillWidth: true }

            Widgets.ButtonExt {
                focus: dialog.acceptText === ""
                text: dialog.acceptText === "" ? qsTr("Close") : qsTr("Cancel")
                colorContext.colorSet: ColorContext.Window
                radius: VLCStyle.radius_normal
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                Layout.topMargin: VLCStyle.spacing_20
                Layout.bottomMargin: VLCStyle.spacing_20
                Layout.rightMargin: dialog.acceptText === "" ? VLCStyle.spacing_20 : 0
                onClicked: dialog.reject()
            }

            Widgets.ButtonExt {
                visible: dialog.acceptText !== ""
                focus: visible
                text: dialog.acceptText
                colorContext.colorSet: ColorContext.ButtonAccent
                radius: VLCStyle.radius_normal
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                Layout.margins: VLCStyle.spacing_20
                Layout.leftMargin: 0
                onClicked: dialog.accept()
            }
        }
    }
}
