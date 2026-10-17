/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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


import VLC.Style

// Figma's text field (6259:14966, 3180:18103): filled BG/B2 with a 2 px BG/A4
// inside stroke, r6, 14 px FG/A2 text 14 px from the edge, FG/A4 placeholder
TextField {
    id: control

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.TextField

        focused: control.activeFocus
        hovered: control.hovered
        enabled: control.enabled
    }

    selectedTextColor : theme.fg.highlight
    selectionColor : theme.bg.highlight
    color : theme.fg.primary
    placeholderTextColor: theme.fg.muted

    font.pixelSize: VLCStyle.fontSize_normal
    font.family: VLCStyle.fontFamily

    verticalAlignment: Text.AlignVCenter

    leftPadding: stroked ? VLCStyle.spacing_14 : VLCStyle.spacing_12
    rightPadding: VLCStyle.dp(18, VLCStyle.scale)

    property real radius: VLCStyle.radius_normal

    // Figma draws some fields without the stroke (the speed of the
    // Adjustments sidebar 5187:24542, the color fields 3191:18737), with
    // the text 12 px from the edge; the focus ring still shows
    property bool stroked: true

    // the fill: BG/B2; the search fields of the headers use BG/A5 without a
    // stroke (Figma 1040:2512, 2437:14840)
    property color bgColor: theme.bg.primary

    background: Rectangle {
        implicitWidth: VLCStyle.dp(200, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
        border.width: (control.stroked || control.activeFocus) ? VLCStyle.dp(2, VLCStyle.scale) : 0
        color: control.bgColor
        border.color: theme.border
        border.pixelAligned: (radius < Number.EPSILON)
        radius: control.radius
    }

}
