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
import VLC.Style

//we want the progress bar to match the radius of the of the video thumbnail
//so we generarte two rectangles with the right radius and we clip the part we
//want to hide
Item {
    id: progressBar

    implicitHeight: VLCStyle.dp(4, VLCStyle.scale)

    clip :true

    property real value: 0
    property int radius: implicitHeight

    // the played part, and the rest (the cards draw no track: transparent)
    property color color: theme.fg.primary
    property color trackColor: theme.bg.primary

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Slider
    }

    //to have at least the proper radius applied, the top radius section is always clipped
    readonly property real _shapeHeight: Math.max(radius * 2, height + radius)

    Rectangle {
        id: track

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        height: progressBar._shapeHeight

        visible: progressBar.trackColor.a > 0
        color: progressBar.trackColor
        radius: progressBar.radius
    }

    // NOTE: The played part is the same rounded shape, clipped at the value
    //       (a gradient with a transparent end draws nothing with the Qt of
    //       the win64 builds)
    Item {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        width: Math.round(progressBar.width * Math.min(Math.max(progressBar.value, 0), 1))

        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom

            width: progressBar.width
            height: progressBar._shapeHeight

            color: progressBar.color
            radius: progressBar.radius
        }
    }
}
