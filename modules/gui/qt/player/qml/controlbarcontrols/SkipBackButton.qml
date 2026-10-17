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


import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Util

Widgets.IconToolButton {
    id: stepBackBtn

    // the music player skips 15 s (Figma 4778:26181), the video player the
    // short jump size
    readonly property bool _music: !Player.hasVideoOutput

    enabled: Player.seekable

    text: _music ? VLCIcons.backward_15_sec : VLCIcons.skip_back
    onClicked: {
        if (_music)
            Player.jumpSeconds(-15)
        else
            Player.jumpBwd()
    }
    description: _music ? qsTr("Back 15 seconds") : qsTr("Step back")
    // the short jump's key; the music player's 15 s has none
    shortcutAction: _music ? "" : "jump-short"

    // Figma 4778:26506: 24 px icon in a 30x28 slot, as Previous and Next
    leftPadding: VLCStyle.dp(3, VLCStyle.scale)
    rightPadding: VLCStyle.dp(3, VLCStyle.scale)
    topPadding: VLCStyle.dp(2, VLCStyle.scale)
    bottomPadding: VLCStyle.dp(2, VLCStyle.scale)

    AccessibleCompat.id: "skipBackButton"
}
