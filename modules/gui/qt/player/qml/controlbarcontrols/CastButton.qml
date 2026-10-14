/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Dialogs
import VLC.Util

// The Cast button: shows the device the playback is cast to, with
// "Stop casting". When nothing is cast, it offers Stream & Cast.
Widgets.PopupIconToolButton {
    id: root

    readonly property bool casting: Player.rendererManager.useRenderer

    text: VLCIcons.renderer
    description: casting ? qsTr("Casting to %1").arg(Player.rendererManager.rendererName)
                         : qsTr("Cast")

    // the accent tells that the playback is not here
    color: (casting || popup.visible) ? colorContext.accent : colorContext.fg.primary

    AccessibleCompat.id: "castButton"

    popup.contentItem: ColumnLayout {
        spacing: VLCStyle.margin_small

        Widgets.ListLabel {
            Layout.fillWidth: true
            Layout.maximumWidth: VLCStyle.dp(280, VLCStyle.scale)

            text: root.casting ? qsTr("Casting to %1").arg(Player.rendererManager.rendererName)
                               : qsTr("Playing on this computer")
            color: root.colorContext.fg.primary
        }

        Widgets.ButtonExt {
            Layout.fillWidth: true

            visible: root.casting
            text: qsTr("Stop casting")

            Navigation.parentItem: root

            onClicked: {
                root.popup.close()
                Player.rendererManager.disableRenderer()
            }
        }

        Widgets.ButtonExt {
            Layout.fillWidth: true

            visible: !root.casting
            text: qsTr("Cast to a device…")

            Navigation.parentItem: root

            onClicked: {
                root.popup.close()
                DialogsProvider.streamAndCastDialog(DialogsProvider.CAST_TAB)
            }
        }
    }
}
