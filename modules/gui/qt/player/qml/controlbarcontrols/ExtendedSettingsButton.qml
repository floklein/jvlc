/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs
import VLC.Util

// The adjustments button of the Control bar: opens the Adjustments sidebar
Widgets.IconToolButton {
    id: extdSettingsBtn

    // folds into the VLC menu in a narrow window
    readonly property int foldStep: ControlBarFolding.TracksFolded

    // Figma "playback settings" (2230:9222), filled while the sidebar shows
    text: checked ? VLCIcons.playback_settings_active : VLCIcons.playback_settings

    description: qsTr("Adjustments")
    shortcutAction: "adjustments"

    // Figma "Icon Wrapper": 24 px icon, 4 px padding, 30 px wide
    leftPadding: VLCStyle.dp(3, VLCStyle.scale)
    rightPadding: VLCStyle.dp(3, VLCStyle.scale)
    topPadding: VLCStyle.spacing_4
    bottomPadding: VLCStyle.spacing_4

    AccessibleCompat.id: "extendedSettingsButton"

    checked: MainCtx.adjustmentsOpened

    toggle: true

    onClicked: MainCtx.toggleAdjustments()
}
