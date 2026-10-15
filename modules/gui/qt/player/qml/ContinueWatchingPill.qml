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
import VLC.Player
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

// Figma 1919:9580: "You already watched till 00:04:08. [Continue Watching] ✕",
// above the Control bar for about 10 s when a partly watched file reopens.
// It doesn't take the focus (Space keeps pausing), but it is announced and
// reachable with Tab and the arrows.
FocusScope {
    id: root

    // Properties

    // the saved position as text, "" when unknown
    readonly property string watchedTime: {
        if (Player.restorePlaybackPosition <= 0 || !Player.length.valid())
            return ""
        return Player.length.scale(Player.restorePlaybackPosition).formatHMS(0)
    }

    readonly property string message: watchedTime !== ""
                                      ? qsTr("You already watched till %1.").arg(watchedTime)
                                      : qsTr("You already watched part of this.")

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    // Settings

    implicitWidth: row.implicitWidth + row.anchors.leftMargin + row.anchors.rightMargin
    implicitHeight: VLCStyle.dp(52, VLCStyle.scale)

    visible: prompt.shown
    enabled: visible

    Accessible.role: Accessible.AlertMessage
    Accessible.name: message
    AccessibleCompat.id: "continueWatchingPill"

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    // the viewer resumes or dismisses: the pill is about to hide
    signal closing()

    // Functions

    function resume() {
        closing()
        prompt.hide()
        Player.restorePlaybackPos()
        Player.acknowledgeRestoreCallback()
    }

    function dismiss() {
        closing()
        prompt.hide()
        Player.acknowledgeRestoreCallback()
    }

    // Events

    onVisibleChanged: requestLockUnlockAutoHide(visible)

    Component.onCompleted: {
        if (Player.canRestorePlayback)
            prompt.show()
    }

    Navigation.cancelAction: function() { root.dismiss() }

    // Children

    TimedPrompt {
        id: prompt

        // held while pointed at or focused
        held: hoverHandler.hovered || root.activeFocus

        // the question goes away with the pill
        onExpired: Player.acknowledgeRestoreCallback()
    }

    Connections {
        target: Player

        function onPlaybackRestoreQueried() {
            if (Player.canRestorePlayback) {
                prompt.show()
                Helpers.announce(root, root.message)
            } else
                prompt.hide()
        }
    }

    HoverHandler {
        id: hoverHandler
    }

    // the Theme's Tint, as the other panels over the video
    Widgets.TintBackground {
        anchors.fill: parent

        radius: VLCStyle.dp(16, VLCStyle.scale)
        colorContext: theme
    }

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: VLCStyle.spacing_20
        anchors.rightMargin: VLCStyle.spacing_10
        anchors.topMargin: VLCStyle.spacing_10
        anchors.bottomMargin: VLCStyle.spacing_10

        spacing: VLCStyle.spacing_10

        T.Label {
            id: label

            Layout.fillWidth: true
            Layout.rightMargin: VLCStyle.dp(50, VLCStyle.scale) // 60 px from the buttons

            text: root.watchedTime !== ""
                  ? qsTr("You already watched till %1.")
                        .arg("<b>" + root.watchedTime + "</b>")
                  : root.message
            textFormat: Text.StyledText

            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_xlarge
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter

            // the pill itself reads the message
            Accessible.ignored: true
        }

        Widgets.ActionButtonPrimary {
            id: continueButton

            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)

            leftPadding: VLCStyle.spacing_8
            rightPadding: VLCStyle.spacing_8

            radius: VLCStyle.radius_normal

            text: qsTr("Continue Watching")

            AccessibleCompat.id: "continueWatchingButton"

            focus: true

            onClicked: root.resume()

            Navigation.parentItem: root
            Navigation.rightItem: dismissButton
        }

        Widgets.ButtonExt {
            id: dismissButton

            Layout.preferredWidth: VLCStyle.dp(44, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)

            radius: VLCStyle.radius_normal

            iconTxt: VLCIcons.close
            iconSize: VLCStyle.icon_normal

            text: qsTr("Dismiss")
            showText: false

            Accessible.name: text
            AccessibleCompat.id: "continueWatchingDismissButton"

            onClicked: root.dismiss()

            Navigation.parentItem: root
            Navigation.leftItem: continueButton

            // Figma: a filled neutral button (BG/A1 in dark) under the
            // standard button's hover and focus states
            Rectangle {
                z: -2 // under the button's own background

                anchors.fill: parent

                radius: dismissButton.radius
                color: fillTheme.bg.highlight

                readonly property ColorContext colorContext: ColorContext {
                    id: fillTheme

                    palette: theme.palette
                    colorSet: ColorContext.Item
                }
            }
        }
    }
}
