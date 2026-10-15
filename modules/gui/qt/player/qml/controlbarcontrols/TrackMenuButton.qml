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

import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

// A Control bar button that opens the Track menu of the player on its tab
Widgets.IconToolButton {
    id: root

    // Properties

    // TrackMenuFSM.Tab
    required property int tab

    // the glyphs when the menu is closed and when it shows this tab
    required property string glyph
    required property string activeGlyph

    // folds into the VLC menu in a narrow window
    readonly property int foldStep: ControlBarFolding.TracksFolded

    readonly property var _trackMenu: ((typeof rootPlayer !== 'undefined') && (rootPlayer !== null))
                                      ? rootPlayer.trackMenu
                                      : null

    // Settings

    // the tab shown in the open menu
    checked: !!_trackMenu && _trackMenu.visible && _trackMenu.tab === tab

    text: checked ? activeGlyph : glyph

    enabled: !!_trackMenu

    // Figma "Icon Wrapper": 24 px icon, 4 px padding, 30 px wide
    leftPadding: VLCStyle.dp(3, VLCStyle.scale)
    rightPadding: VLCStyle.dp(3, VLCStyle.scale)
    topPadding: VLCStyle.spacing_4
    bottomPadding: VLCStyle.spacing_4

    Accessible.role: Accessible.ButtonMenu

    // Events

    onPressed: _trackMenu.buttonPressed(tab)

    onClicked: _trackMenu.buttonClicked(tab)
}
