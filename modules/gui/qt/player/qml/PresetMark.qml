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

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Player
import VLC.Menus
import VLC.Style

// The accent dot of a sidebar value that comes from the active Preset; it
// offers "Reset to Profile value"
T.AbstractButton {
    id: root

    // the Preset keys of the row, and key prefixes ("compressor" for the
    // compressor and its parameters)
    property var keys: []
    property var prefixes: []

    readonly property var _overridden: Presets.overriddenKeys.filter(
        (key) => root.keys.indexOf(key) >= 0 || root.prefixes.some((prefix) => key.startsWith(prefix)))

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    visible: _overridden.length > 0

    implicitWidth: VLCStyle.dp(16, VLCStyle.scale)
    implicitHeight: implicitWidth

    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Set by the Preset %1").arg(Presets.activeName)

    onClicked: menu.popup(root.mapToGlobal(0, root.height), [qsTr("Reset to Profile value")])

    background: Item {
        Rectangle {
            anchors.centerIn: parent

            width: VLCStyle.dp(8, VLCStyle.scale)
            height: width
            radius: width / 2

            color: theme.accent
            border.width: root.hovered ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: theme.fg.primary
        }
    }

    Widgets.ToolTipExt {
        colorContext.palette: theme.palette
        visible: root.hovered
        delay: VLCStyle.delayToolTipAppear
        text: qsTr("From the Preset %1").arg(Presets.activeName)
    }

    StringListMenu {
        id: menu

        ctx: MainCtx

        onSelected: Presets.resetToProfile(root._overridden)
    }
}
