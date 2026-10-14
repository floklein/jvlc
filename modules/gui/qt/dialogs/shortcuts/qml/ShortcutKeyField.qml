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
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style

// One key cell of Keyboard Shortcuts: the keys and a pencil. While it
// listens, it shows "Press a key…" and reports the next key press.
T.AbstractButton {
    id: root

    // the keys, for display
    property var keys: []
    // waiting for a key
    property bool listening: false
    // "Key of Stop"
    property string accessibleName

    // a key press while listening; Esc and Backspace included
    signal keyPressed(int key, int modifiers)

    implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
    implicitWidth: VLCStyle.dp(270, VLCStyle.scale)

    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: listening ? qsTr("%1: press a key").arg(accessibleName)
                               : qsTr("%1: %2").arg(accessibleName)
                                               .arg(keys.length > 0 ? keys.join(", ") : qsTr("none"))
    Accessible.description: qsTr("Change the key")
    Accessible.onPressAction: root.clicked()

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ComboBox
        enabled: root.enabled
        focused: root.visualFocus
        hovered: root.hovered
    }

    // keys go to the field, not to the shortcuts of the windows
    Keys.onShortcutOverride: (event) => event.accepted = root.listening
    Keys.onPressed: (event) => {
        if (!root.listening)
            return
        event.accepted = true
        if (event.isAutoRepeat)
            return
        root.keyPressed(event.key, event.modifiers)
    }

    background: Rectangle {
        radius: VLCStyle.radius_normal
        color: root.hovered && !root.listening ? theme.bg.highlight : theme.bg.primary
        border.width: root.listening || root.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
        border.color: theme.accent
    }

    contentItem: Item {
        Text {
            anchors.centerIn: parent
            width: parent.width - 2 * (pencil.width + VLCStyle.spacing_10)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: root.listening ? qsTr("Press a key…") : root.keys.join(", ")
            color: root.listening ? theme.fg.secondary : theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
            font.family: root.listening ? VLCStyle.fontFamily : VLCStyle.fontFamily_mono
        }

        Widgets.IconLabel {
            id: pencil
            anchors.right: parent.right
            anchors.rightMargin: VLCStyle.spacing_10
            anchors.verticalCenter: parent.verticalCenter
            visible: root.enabled
            text: VLCIcons.edit
            color: root.listening ? theme.accent : theme.fg.secondary
            font.pixelSize: VLCStyle.dp(18, VLCStyle.scale)
        }
    }
}
