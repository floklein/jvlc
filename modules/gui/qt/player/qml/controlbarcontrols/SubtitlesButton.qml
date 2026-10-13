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

import VLC.MainInterface
import VLC.Style
import VLC.Util

// The subtitles button of the Control bar
// TODO: open the subtitles page of the Track menu (#49)
LangButton {
    text: VLCIcons.subtitles

    description: qsTr("Subtitles")
    shortcut: MainCtx.actionShortcut("subtitle-track")

    // Figma "Icon Wrapper": 24 px icon, 4 px padding, 30 px wide
    leftPadding: VLCStyle.dp(3, VLCStyle.scale)
    rightPadding: VLCStyle.dp(3, VLCStyle.scale)
    topPadding: VLCStyle.dp(4, VLCStyle.scale)
    bottomPadding: VLCStyle.dp(4, VLCStyle.scale)

    AccessibleCompat.id: "subtitlesButton"
}
