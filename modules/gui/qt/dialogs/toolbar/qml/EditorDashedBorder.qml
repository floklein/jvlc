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

// The dashed outline Customize Player draws around zones and items (Figma)
Item {
    id: root

    // a colour of the theme (border, visual focus)
    required property color color
    property real dash: VLCStyle.dp(3, VLCStyle.scale)
    property real lineWidth: VLCStyle.border

    readonly property real _step: dash * 2

    component Dashes: Repeater {
        id: dashes

        required property bool horizontal
        required property real length
        required property real offset

        model: Math.max(0, Math.ceil(length / root._step))

        delegate: Rectangle {
            required property int index

            x: dashes.horizontal ? index * root._step : dashes.offset
            y: dashes.horizontal ? dashes.offset : index * root._step
            width: dashes.horizontal ? Math.min(root.dash, dashes.length - x) : root.lineWidth
            height: dashes.horizontal ? root.lineWidth : Math.min(root.dash, dashes.length - y)

            color: root.color
        }
    }

    Dashes { horizontal: true; length: root.width; offset: 0 }
    Dashes { horizontal: true; length: root.width; offset: root.height - root.lineWidth }
    Dashes { horizontal: false; length: root.height; offset: 0 }
    Dashes { horizontal: false; length: root.height; offset: root.width - root.lineWidth }
}
