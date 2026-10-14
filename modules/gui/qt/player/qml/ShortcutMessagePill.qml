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
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player

// The Shortcut message: "Subtitle delay: 50 ms · H", for 1.5 s
Rectangle {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.Window
    }

    visible: opacity > 0
    opacity: ShortcutMessage.visible ? 1 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: VLCStyle.duration_short
            easing.type: Easing.InOutQuad
        }
    }

    implicitWidth: label.implicitWidth + VLCStyle.margin_normal * 2
    implicitHeight: label.implicitHeight + VLCStyle.margin_xsmall * 2

    radius: height / 2
    color: theme.bg.primary

    // a live region: screen readers read each message, the focus stays
    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Connections {
        target: ShortcutMessage

        function onShown(text) {
            // Qt >= 6.8
            if (typeof root.Accessible.announce === "function")
                root.Accessible.announce(text)
        }
    }

    Widgets.DefaultShadow {
    }

    T.Label {
        id: label

        anchors.centerIn: parent

        text: ShortcutMessage.text

        font.pixelSize: VLCStyle.fontSize_normal
        color: theme.fg.primary
    }
}
