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

import VLC.Style

// A thin vertical line between items. A Layout may hold several.
Item {
    id: root

    property bool paintOnly: false

    // whether a control follows it: before nothing (the folder button hidden
    // in a narrow window) it draws no line, but keeps its room so that the
    // controls around it don't move
    property bool separating: true

    // not focusable: the Control bar gives focus to enabled items
    enabled: false

    // the zone spacing on both sides makes the 24 px of Figma's divider
    // (247:457), which sits right against its neighbours
    implicitWidth: VLCStyle.dp(4, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // Figma "divider" (71:863): an 18 px stroke of 1.5 px, Divider/Bright
    Rectangle {
        anchors.centerIn: parent

        width: 1.5 * VLCStyle.scale
        height: VLCStyle.dp(18, VLCStyle.scale)
        radius: width / 2

        color: theme.border

        visible: root.separating || root.paintOnly
    }
}
