/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Controls.Basic
import QtQuick.Window
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs
import VLC.Player
import VLC.PlayerControls

// Customize Player (Figma 2107:5604): a dialog over the player. Drag items
// between the palette and the Top bar and Control bar of the video or the
// music player. The bars of the player follow the changes live; Save keeps
// them, Cancel (or Esc) brings back the saved Layout.
ModalDialog {
    id: root

    parent: T.Overlay.overlay

    readonly property ControlbarProfileModel layouts: MainCtx.controlbarProfileModel

    // the edited player: the one playing, until the user picks one
    property int _pickedIdentifier: -1
    readonly property int identifier: {
        if (_pickedIdentifier >= 0)
            return _pickedIdentifier
        return (Player.isStarted && !Player.hasVideoOutput) ? PlayerControlbarModel.Audioplayer
                                                           : PlayerControlbarModel.Videoplayer
    }

    readonly property PlayerControlbarModel bars: layouts.currentModel?.getModel(identifier) ?? null

    // within the window, at large scales or in a small window too
    width: Math.min(VLCStyle.dp(1000, VLCStyle.scale), (parent?.width ?? 0) - 2 * VLCStyle.margin_large)
    height: Math.min(VLCStyle.dp(720, VLCStyle.scale), (parent?.height ?? 0) - 2 * VLCStyle.margin_large)

    title: (identifier === PlayerControlbarModel.Audioplayer) ? qsTr("Customize music player")
                                                              : qsTr("Customize video player")

    Accessible.role: Accessible.Dialog
    Accessible.name: title

    signal unload()

    // whether the dialog closes through Save, else it discards the changes
    property bool _saving: false

    function openEditor() {
        // what was saved, in case of Cancel
        layouts.save(false)

        _pickedIdentifier = -1
        _saving = false
        open()
    }

    function save() {
        _saving = true
        layouts.save()
        close()
    }

    function cancel() {
        close()
    }

    // Cancel, Esc or a click away: load what was saved to discard the changes
    onClosed: {
        if (!_saving)
            layouts.reload()
        unload()
    }

    contentItem: FocusScope {
        focus: true

        // the popup's content takes no palette from the dialog
        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_small

            spacing: VLCStyle.margin_normal

            RowLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_normal

                // the video and the music player each have their Layout
                Widgets.ComboBoxExt {
                    id: playerBox

                    Layout.preferredWidth: VLCStyle.dp(180, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.combobox_height_normal

                    model: PlayerListModel.model
                    textRole: "name"
                    valueRole: "identifier"

                    currentIndex: Math.max(0, PlayerListModel.model.findIndex((entry) => entry.identifier === root.identifier))

                    onActivated: root._pickedIdentifier = currentValue

                    Accessible.name: qsTr("Player")
                }

                Widgets.MenuLabel {
                    text: qsTr("Layout")
                    color: root.colorContext.fg.primary
                }

                Widgets.ComboBoxExt {
                    id: layoutBox

                    Layout.preferredWidth: VLCStyle.dp(160, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.combobox_height_normal

                    model: root.layouts
                    textRole: "name"

                    currentIndex: root.layouts.selectedProfile

                    onActivated: (index) => root.layouts.selectedProfile = index

                    Accessible.name: qsTr("Layout")
                }

                Widgets.CheckBoxExt {
                    text: qsTr("Seek bar on its own row")

                    enabled: !!root.bars
                    checked: !!root.bars && !root.bars.seekBarInline

                    onToggled: root.bars.seekBarInline = !checked
                }

                Item { Layout.fillWidth: true }
            }

            ToolbarEditor {
                id: toolbarEditor

                Layout.fillWidth: true
                Layout.fillHeight: true

                identifier: root.identifier

                focus: true
            }

            // Buttons
            RowLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_small

                Widgets.ButtonExt {
                    text: qsTr("Cancel")

                    onClicked: root.cancel()
                }

                Widgets.ButtonExt {
                    text: qsTr("Reset to default")

                    enabled: !!root.bars

                    onClicked: root.layouts.resetToDefault(root.identifier)
                }

                Item { Layout.fillWidth: true }

                Widgets.ActionButtonPrimary {
                    text: qsTr("Save")

                    // only after a change
                    enabled: root.layouts.dirty

                    onClicked: root.save()
                }
            }
        }
    }
}
