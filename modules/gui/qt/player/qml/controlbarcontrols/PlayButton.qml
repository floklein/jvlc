/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import VLC.Playlist
import VLC.Util

// The wide play button of the Control bar (Figma 848:1712)
T.Control {
    id: root

    // ControlBar.focusPlayButton() looks for it
    objectName: "playButton"

    // Properties

    property bool paintOnly: false

    readonly property bool _playing: !paintOnly
                                     && Player.playingState !== Player.PLAYING_STATE_PAUSED
                                     && Player.playingState !== Player.PLAYING_STATE_STOPPED

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.ToolButton

        enabled: root.enabled || root.paintOnly
        focused: root.visualFocus
    }

    readonly property ColorContext accentContext: ColorContext {
        id: accentTheme

        colorSet: ColorContext.ButtonAccent

        enabled: root.enabled || root.paintOnly
        hovered: root.hovered
        pressed: mouseArea.pressed
    }

    property bool _keyOkPressed: false

    // Aliases

    property alias cursorInside: mouseArea.containsMouse

    // Settings

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
    implicitHeight: implicitBackgroundHeight + topInset + bottomInset

    hoverEnabled: true

    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.Button
    Accessible.name: Helpers.accessibleName(_playing ? qsTr("Pause") : qsTr("Play"),
                                            MainCtx.actionShortcut("play-pause"))
    AccessibleCompat.id: "playButton"
    Accessible.onPressAction: MainPlaylistController.togglePlayPause()

    // Tooltip

    T.ToolTip.visible: (hovered || visualFocus)
    T.ToolTip.delay: VLCStyle.delayToolTipAppear
    T.ToolTip.text: {
        const name = _playing ? qsTr("Pause") : qsTr("Play")
        const key = MainCtx.actionShortcut("play-pause")
        return key ? qsTr("%1 (%2)").arg(name).arg(key) : name
    }

    // Keys

    Keys.onPressed: (event) => {
        if (KeyHelper.matchOk(event) ) {
            if (!event.isAutoRepeat) {
                _keyOkPressed = true
                keyHoldTimer.restart()
            }
            event.accepted = true
        }
        Navigation.defaultKeyAction(event)
    }

    Keys.onReleased: (event) => {
        if (KeyHelper.matchOk(event)) {
            if (!event.isAutoRepeat && _keyOkPressed) {
                _keyOkPressed = false
                keyHoldTimer.stop()
                if (Player.playingState !== Player.PLAYING_STATE_STOPPED)
                    MainPlaylistController.togglePlayPause()
            }
            event.accepted = true
        }
    }

    // Functions

    function _pressAndHoldAction() {
        _keyOkPressed = false
        MainPlaylistController.stop()
    }

    // Children

    // holding the button stops playback
    Timer {
        id: keyHoldTimer

        interval: mouseArea.pressAndHoldInterval
        repeat: false

        onTriggered: root._pressAndHoldAction()
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent

        enabled: !root.paintOnly

        onClicked: (mouse) => {
            MainPlaylistController.togglePlayPause()
            mouse.accepted = true
        }

        onPressAndHold: (mouse) => {
            root._pressAndHoldAction()
            mouse.accepted = true
        }
    }

    contentItem: Widgets.IconLabel {
        text: root._playing ? VLCIcons.pause : VLCIcons.play

        color: accentTheme.fg.primary

        font.pixelSize: VLCStyle.icon_normal

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter

        Accessible.ignored: true
    }

    background: Rectangle {
        implicitWidth: VLCStyle.dp(54, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

        radius: VLCStyle.radius_normal

        color: accentTheme.bg.primary

        // keyboard focus ring, outside the accent shape
        Rectangle {
            anchors.fill: parent
            anchors.margins: -(VLCStyle.focus_border * 2)

            radius: parent.radius + VLCStyle.focus_border * 2

            color: "transparent"
            border.width: VLCStyle.focus_border
            border.color: theme.visualFocus

            visible: root.visualFocus
        }
    }
}
