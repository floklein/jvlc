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

// The background of a panel that can sit over the video: the Theme's Tint
// there, `baseColor` elsewhere. Where the compositor draws the Video blur
// (Windows, DirectComposition), the blurred video shows beneath a lighter
// Tint; everywhere else the Tint stays as is.
Rectangle {
    id: root

    // the panel sits over the video (or the player): the Tint
    property bool overVideo: true

    property ColorContext colorContext: ColorContext {
        colorSet: ColorContext.Window
    }

    // the color off the video
    property color baseColor: colorContext.bg.primary

    // without a video, there is nothing to blur
    readonly property bool videoBlur: overVideo
                                      && MainCtx.hasEmbededVideo
                                      && !!MainCtx.videoBlurProvider

    color: {
        if (!overVideo)
            return baseColor
        return videoBlur ? colorContext.tintOverBlur : colorContext.tint
    }

    // paints nothing: the compositor draws the blur under the interface
    VideoBlur {
        anchors.fill: parent

        radius: root.radius
        deviation: VLCStyle.videoBlurDeviation

        // the Theme keeps the blurred video readable under its Tint
        darkest: root.colorContext.palette?.videoBlurDarkest ?? 0
        brightest: root.colorContext.palette?.videoBlurBrightest ?? 1

        provider: root.videoBlur ? MainCtx.videoBlurProvider : null
    }
}
