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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

// "Queue replaced · Undo", shown while the replaced Queue can be restored
Rectangle {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    visible: opacity > 0
    opacity: MainPlaylistController.canUndoReplace ? 1 : 0

    Connections {
        target: MainPlaylistController

        function onCanUndoReplaceChanged() {
            if (MainPlaylistController.canUndoReplace)
                Helpers.announce(root, qsTr("Queue replaced. Undo with the Undo button"))
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: VLCStyle.duration_short
            easing.type: Easing.InOutQuad
        }
    }

    implicitWidth: layout.implicitWidth + VLCStyle.margin_normal * 2
    implicitHeight: layout.implicitHeight + VLCStyle.margin_xsmall * 2

    radius: height / 2
    color: theme.bg.primary

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Widgets.DefaultShadow {
    }

    RowLayout {
        id: layout

        anchors.centerIn: parent

        spacing: VLCStyle.margin_small

        T.Label {
            id: label

            text: qsTr("Queue replaced")

            font.pixelSize: VLCStyle.fontSize_normal
            color: theme.fg.primary
        }

        T.Label {
            text: "·"

            font.pixelSize: VLCStyle.fontSize_normal
            color: theme.fg.secondary
        }

        Widgets.TextToolButton {
            text: qsTr("Undo")

            colorContext.colorSet: ColorContext.ButtonAccent

            onClicked: MainPlaylistController.undoReplace()
        }
    }
}
