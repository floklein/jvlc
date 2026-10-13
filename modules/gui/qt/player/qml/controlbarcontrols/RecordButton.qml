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
import QtQuick.Templates as T


import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Util

Widgets.IconToolButton {
    id: control

    enabled: !paintOnly && Player.isStarted

    // the active (accent) icon means recording
    color: Player.recording ? control.colorContext.accent : control.colorContext.fg.primary
    text: Player.recording ? VLCIcons.record_active : VLCIcons.record
    description: qsTr("record")

    AccessibleCompat.id: "recordButton"

    onClicked: Player.toggleRecord()

    //IconToolButton already contains a color animation that would conflict
    contentItem: Widgets.IconLabel {
        id: recordLabel

        text: control.text

        color: control.color

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: control.enabled && Player.recording

            NumberAnimation {
                from: 1
                to: 0
                //this is an animation and not a transisition, we explicitly want a long duration
                duration: 1000
                easing.type: Easing.InSine
            }

            NumberAnimation {
                from: 0
                to: 1
                //this is an animation and not a transisition, we explicitly want a long duration
                duration: 1000
                easing.type: Easing.OutSine
            }

            onStopped: {
                recordLabel.opacity = 1
            }
        }
        font: control.font
    }


}
