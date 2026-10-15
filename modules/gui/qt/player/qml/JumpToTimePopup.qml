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
import VLC.Widgets as Widgets
import VLC.Player
import VLC.Util

// Jump to Time: a small popover over the seek bar. Enter jumps.
T.Popup {
    id: root

    // whether the typed time can be reached
    readonly property bool _valid: {
        const seconds = Helpers.parseTime(timeField.text)
        if (seconds < 0)
            return false
        return !Player.length.valid() || seconds <= Player.length.toSeconds()
    }

    padding: VLCStyle.margin_small

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: implicitContentHeight + topPadding + bottomPadding

    // centered over the current position, inside the seek bar
    x: parent ? Helpers.clamp(parent.width * Player.position - width / 2,
                              0, Math.max(0, parent.width - width))
              : 0
    y: -height - VLCStyle.margin_small

    modal: false
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    function jump() {
        if (!_valid)
            return
        Player.jumpToTime(Helpers.parseTime(timeField.text))
        close()
    }

    onAboutToShow: {
        timeField.text = Player.time.formatHMS()
        timeField.selectAll()
        timeField.forceActiveFocus(Qt.PopupFocusReason)
    }

    onOpened: MainCtx.useGlobalShortcuts = false
    onClosed: MainCtx.useGlobalShortcuts = true

    // a popup is not in the item tree: its palette is set by its user
    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    // the semantic colors are defined for views
    ColorContext {
        id: semantic
        palette: theme.palette
        colorSet: ColorContext.View
    }

    background: Rectangle {
        color: theme.bg.primary
        radius: VLCStyle.radius_large
        border.color: theme.border

        Widgets.RoundedRectangleShadow {
            yOffset: VLCStyle.dp(4, VLCStyle.scale)
            blurRadius: VLCStyle.dp(6, VLCStyle.scale)
            opacity: 0.25
            color: theme.shadow
        }
    }

    contentItem: RowLayout {
        spacing: VLCStyle.margin_xsmall

        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Jump to Time")

        T.Label {
            text: qsTr("Jump to")
            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        Widgets.TextFieldExt {
            id: timeField

            Layout.preferredWidth: VLCStyle.dp(96, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.combobox_height_normal

            horizontalAlignment: Text.AlignHCenter
            font.family: VLCStyle.fontFamily_mono
            inputMethodHints: Qt.ImhPreferNumbers
            validator: RegularExpressionValidator { regularExpression: /[0-9:\s]*/ }
            color: root._valid ? theme.fg.primary : semantic.fg.negative

            Accessible.name: qsTr("Time to jump to, as hours:minutes:seconds")

            Keys.onReturnPressed: root.jump()
            Keys.onEnterPressed: root.jump()
        }

        T.Label {
            visible: Player.length.valid()
            text: qsTr("of %1").arg(Player.length.formatHMS())
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        Widgets.IconToolButton {
            text: VLCIcons.check
            description: qsTr("Jump")
            enabled: root._valid

            onClicked: root.jump()
        }
    }
}
