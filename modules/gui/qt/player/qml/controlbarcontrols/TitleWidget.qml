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
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Style
import VLC.Player
import VLC.Util

// The media title: the metadata title, else the file name
Item {
    id: root

    property bool paintOnly: false

    readonly property string text: {
        if (paintOnly)
            return qsTr("Title")
        const title = Player.title || Player.name
        return title !== "" ? title : qsTr("jVLC Media Player")
    }

    // not focusable: the Control bar gives focus to enabled items
    enabled: false

    implicitWidth: Math.min(Math.ceil(metrics.advanceWidth), VLCStyle.dp(400, VLCStyle.scale))

    // In a narrow window the title truncates first: it takes the room the
    // other controls leave, and hides below a few characters. It folds
    // nothing, so the bar counts it at its minimum.
    readonly property real _minimumWidth: Math.min(implicitWidth, VLCStyle.dp(32, VLCStyle.scale))

    Layout.minimumWidth: paintOnly ? -1 : _minimumWidth
    Layout.preferredWidth: paintOnly ? -1 : _minimumWidth
    implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

    Accessible.role: Accessible.TitleBar
    Accessible.name: text

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    TextMetrics {
        id: metrics

        font: label.font
        text: root.text
    }

    T.Label {
        id: label

        anchors.fill: parent

        text: root.text
        color: theme.fg.secondary
        font.pixelSize: VLCStyle.fontSize_normal
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter

        Accessible.ignored: true
    }
}
