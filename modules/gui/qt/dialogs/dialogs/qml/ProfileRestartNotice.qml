/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
import VLC.Dialogs
import VLC.Widgets as Widgets
import VLC.Style

// "Restart VLC to apply · Restart VLC", after a switch of Profile changed
// options that VLC only reads when it starts
Rectangle {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    readonly property bool _required: MainCtx.profiles?.restartRequired ?? false
    property bool _dismissed: false

    // a new switch shows it again
    Connections {
        target: MainCtx.profiles

        function onActiveChanged() {
            root._dismissed = false
        }
    }

    visible: opacity > 0
    opacity: (_required && !_dismissed) ? 1 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: VLCStyle.duration_short
            easing.type: Easing.InOutQuad
        }
    }

    implicitWidth: layout.implicitWidth + VLCStyle.margin_normal + VLCStyle.margin_xsmall
    implicitHeight: layout.implicitHeight + VLCStyle.margin_xsmall * 2

    radius: height / 2
    color: theme.bg.primary

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Widgets.DefaultShadow {
    }

    RowLayout {
        id: layout

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: VLCStyle.margin_normal

        spacing: VLCStyle.margin_small

        T.Label {
            id: label

            text: qsTr("Restart VLC to apply")

            font.pixelSize: VLCStyle.fontSize_normal
            color: theme.fg.primary
        }

        T.Label {
            text: "·"

            font.pixelSize: VLCStyle.fontSize_normal
            color: theme.fg.secondary
        }

        Widgets.TextToolButton {
            text: qsTr("Restart VLC")

            colorContext.colorSet: ColorContext.ButtonAccent

            onClicked: DialogsProvider.restartVLC()
        }

        Widgets.IconToolButton {
            text: VLCIcons.close
            description: qsTr("Later")
            font.pixelSize: VLCStyle.icon_small

            onClicked: root._dismissed = true
        }
    }
}
