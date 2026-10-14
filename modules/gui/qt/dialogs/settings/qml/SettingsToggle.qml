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
import VLC.Style

// ON / OFF switch of the Settings window. It does not change by itself: set
// `checked` from the setting and write the setting on `clicked`.
T.AbstractButton {
    id: control

    checkable: false

    implicitWidth: VLCStyle.dp(68, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(32, VLCStyle.scale)

    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: control.checked
    Accessible.onPressAction: control.clicked()
    // UI Automation toggles a check box rather than pressing it
    Accessible.onToggleAction: control.clicked()

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Switch

        enabled: control.enabled
        focused: control.visualFocus
        hovered: control.hovered
    }

    readonly property real _inset: VLCStyle.dp(3, VLCStyle.scale)

    background: Rectangle {
        radius: VLCStyle.radius_normal
        color: theme.bg.primary
        border.width: control.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
        border.color: theme.visualFocus
    }

    contentItem: Item {
        // the side showing the state
        Rectangle {
            id: pad

            width: VLCStyle.dp(33, VLCStyle.scale)
            height: VLCStyle.dp(26, VLCStyle.scale)
            radius: VLCStyle.radius_xsmall
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? control._inset - control.leftPadding
                               : control.width - width - control._inset - control.leftPadding

            color: control.checked ? theme.bg.secondary : theme.bg.highlight

            Text {
                anchors.centerIn: parent
                text: control.checked ? qsTr("ON") : qsTr("OFF")
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
                color: control.checked ? theme.accent : theme.fg.primary
                Accessible.ignored: true
            }
        }

        // the knob
        Rectangle {
            width: VLCStyle.dp(26, VLCStyle.scale)
            height: width
            radius: VLCStyle.radius_xsmall
            anchors.verticalCenter: parent.verticalCenter
            x: control.checked ? control.width - width - control._inset - control.leftPadding
                               : control._inset - control.leftPadding

            color: control.checked ? theme.fg.secondary : theme.indicator
        }
    }
}
