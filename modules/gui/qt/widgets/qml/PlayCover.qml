/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

import VLC.Style
import VLC.Widgets as Widgets

Item {
    id: root

    // Settings

    width: VLCStyle.play_cover_normal
    height: width

    // NOTE: This is the same scaling than the PlayButton, except we make it bigger.
    //       Maybe we could crank this to 1.1.
    scale: (hoverHandler.hovered && !tapHandler.pressed) ? 1.05 : 1.0

    activeFocusOnTab: false

    signal tapped(var point)

    // Figma's Play key (848:1712): Accent/Main, 6 px corners, the filled
    // play glyph in FG/B1; 54 x 28 for a 48 px cover key
    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ButtonAccent

        hovered: hoverHandler.hovered
        pressed: tapHandler.pressed
    }

    // Animations

    Behavior on scale {
        // NOTE: We disable the animation while pressing to make it more impactful.
        enabled: (!tapHandler.pressed)

        NumberAnimation {
            duration: VLCStyle.duration_short

            easing.type: Easing.OutQuad
        }
    }

    // Children

    TapHandler {
        id: tapHandler

        grabPermissions: TapHandler.CanTakeOverFromHandlersOfDifferentType | TapHandler.ApprovesTakeOverByAnything

        onSingleTapped: (eventPoint, button) => {
            root.tapped(point)
        }
    }

    HoverHandler {
        id: hoverHandler

        grabPermissions: PointerHandler.CanTakeOverFromHandlersOfDifferentType | PointerHandler.ApprovesTakeOverByAnything
    }

    Rectangle {
        id: key

        anchors.centerIn: parent

        width: Math.round(root.width * 54 / 48)
        height: Math.round(root.width * 28 / 48)

        radius: Math.round(root.width * VLCStyle.radius_normal / VLCStyle.play_cover_normal)

        color: theme.bg.primary

        Widgets.IconLabel {
            anchors.centerIn: parent
            // Figma pads the glyph 17 px on the left, 15 on the right
            anchors.horizontalCenterOffset: Math.round(root.width / 48)

            text: VLCIcons.play_filled

            color: theme.fg.primary

            font.pixelSize: Math.round(root.width / 2)
        }
    }
}
