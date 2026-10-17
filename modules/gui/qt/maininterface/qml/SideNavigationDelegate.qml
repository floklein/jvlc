/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Layouts


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

// A row of the Nav sidebar (Figma 6261:15094): a 24 px icon in a 30 px wide
// box, then the label 8 px after it, in a 32 px high row. The row ends with
// the 14 px gap to the next one, so that a list of rows needs no spacing.
T.ItemDelegate {
    id: control

    // Properties

    property string iconTxt: ""

    // the icon of the current section: filled (Figma home, videos, …)
    property string checkedIconTxt: iconTxt

    property bool showText: true

    property real radius: VLCStyle.radius_small

    // Settings

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    bottomPadding: VLCStyle.navigationItem_spacing

    leftInset: implicitLeftInset
    rightInset: implicitRightInset

    topInset: implicitTopInset
    bottomInset: VLCStyle.navigationItem_spacing + implicitBottomInset

    property real implicitLeftInset: 0
    property real implicitRightInset: 0

    property real implicitTopInset: 0
    property real implicitBottomInset: 0

    // Accessible

    Accessible.onPressAction: control.clicked()
    // the current section reads as selected
    Accessible.selectable: true
    Accessible.selected: control.checked
    AccessibleCompat.id: {
        if (model.depth !== 0 || !model.uri || model.uri.length === 0) return "";
        const navBarIds = {
            "home": "homeNavBar",
            "video": "videoNavBar",
            "music": "musicNavBar",
            "network": "networkNavBar",
            "playlists": "playlistsNavBar",
            "recent": "recentNavBar"
        }
        const id = navBarIds[model.uri[0]]
        if (id !== undefined)
            return id
        else
            return ""
    }

    // Tooltip

    T.ToolTip.visible: (showText === false && T.ToolTip.text && (hovered || visualFocus))

    T.ToolTip.delay: VLCStyle.delayToolTipAppear

    T.ToolTip.text: text

    // Childs

    ColorContext {
        id: theme
        colorSet: ColorContext.Navigation

        focused: control.visualFocus
        hovered: control.hovered
        pressed: control.down
        enabled: control.enabled
    }

    // Figma draws no background behind the rows: only the keyboard focus
    // shows one, as a ring
    background: Rectangle {
        color: "transparent"
        border.width: control.visualFocus ? VLCStyle.focus_border : 0
        border.color: theme.visualFocus

        radius: control.radius
    }

    contentItem: Item {
        implicitWidth: iconBox.width + (label.visible ? VLCStyle.margin_xsmall + label.implicitWidth : 0)
        implicitHeight: VLCStyle.navigationItem_height

        Item {
            id: iconBox

            width: VLCStyle.navigationItem_iconWidth
            height: parent.height

            Widgets.IconLabel {
                id: iconLabel

                visible: text.length > 0

                anchors.centerIn: parent

                text: (control.highlighted || control.checked) ? control.checkedIconTxt : control.iconTxt

                color: (control.highlighted || control.checked) ? theme.accent : theme.fg.primary

                font.pixelSize: VLCStyle.icon_normal
            }
        }

        T.Label {
            id: label

            x: iconBox.width + VLCStyle.margin_xsmall
            width: Math.max(0, parent.width - x)
            height: parent.height

            text: control.text

            verticalAlignment: Text.AlignVCenter

            color: control.checked ? theme.fg.secondary : theme.fg.primary

            elide: Text.ElideRight

            visible: control.showText

            font.pixelSize: VLCStyle.fontSize_normal
            font.weight: Font.Normal

            //button text is already exposed
            Accessible.ignored: true

            Behavior on color {
                enabled: theme.initialized

                ColorAnimation {
                    duration: VLCStyle.duration_short
                }
            }
        }
    }
}
