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
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Player
import VLC.Style

// "Show Statistics" of the Playback tab: bitrate and dropped frames of the
// current media, over the player
Rectangle {
    id: root

    readonly property PlaybackStatistics statistics: Player.statistics

    implicitWidth: column.implicitWidth + 2 * VLCStyle.spacing_10
    implicitHeight: column.implicitHeight + 2 * VLCStyle.spacing_10

    radius: VLCStyle.radius_normal
    color: theme.tint

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Statistics")
    Accessible.description: _lines.join(", ")

    // the overlay sits over the video: use the dark Tint
    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.darkPalette
        colorSet: ColorContext.Window
    }

    readonly property var _lines: [
        qsTr("Input bitrate: %1 kb/s").arg(Math.round(statistics.inputBitrate)),
        qsTr("Content bitrate: %1 kb/s").arg(Math.round(statistics.contentBitrate)),
        qsTr("Displayed frames: %1").arg(statistics.displayedFrames),
        qsTr("Dropped frames: %1").arg(statistics.droppedFrames),
        qsTr("Late frames: %1").arg(statistics.lateFrames),
        qsTr("Lost audio buffers: %1").arg(statistics.lostAudioBuffers),
    ]

    Column {
        id: column

        anchors.centerIn: parent

        spacing: VLCStyle.spacing_4

        Repeater {
            model: root._lines

            T.Label {
                required property string modelData

                text: modelData
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
                font.features: { "tnum": 1 }

                Accessible.ignored: true
            }
        }
    }
}
