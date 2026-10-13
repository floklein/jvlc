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
import VLC.Widgets as Widgets

// Two-tone icon: the base and accent glyphs of a two-tone icon of the icon
// font, stacked at the same size. A glyph has a single color, so each layer
// is its own glyph (<name>_tt_base and <name>_tt_accent, see VLCIcons.json).
//
// Two-tone icons are only for icons that aren't clickable (menus,
// decorations). Clickable icons use the monochrome glyphs.
Item {
    id: root

    // Icon name in VLCIcons, without the "_tt_base" or "_tt_accent" suffix.
    // Some two-tone icons (playback, next, stop...) only have an accent layer.
    property string name

    property real pixelSize: VLCStyle.icon_normal

    property color baseColor: theme.fg.secondary
    property color accentColor: theme.accent

    readonly property ColorContext colorContext: ColorContext {
        id: theme
    }

    implicitWidth: pixelSize
    implicitHeight: pixelSize

    Accessible.ignored: true

    Widgets.IconLabel {
        anchors.fill: parent

        font.pixelSize: root.pixelSize
        color: root.baseColor
        text: VLCIcons[root.name + "_tt_base"] ?? ""
    }

    Widgets.IconLabel {
        anchors.fill: parent

        font.pixelSize: root.pixelSize
        color: root.accentColor
        text: VLCIcons[root.name + "_tt_accent"] ?? ""
    }
}
