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
import VLC.Player
import VLC.Playlist
import VLC.Style
import VLC.Util
import VLC.Menus

// Picture-in-picture: the whole window shrunk into a small always-on-top
// video, or the cover art for music. Its controls show while the pointer is
// over it: play/pause, a thin seek strip, "Back to full window" and close.
// Drag anywhere to move it; double-click, Esc or Ctrl+Shift+P bring the full
// window back.
FocusScope {
    id: root

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Picture-in-picture")

    // the controls show while the pointer moves over the window or the
    // keyboard is in them
    readonly property bool controlsVisible: (pointer.containsMouse && !_idle)
                                            || controls.activeFocus
                                            || playButton.hovered || backButton.hovered
                                            || closeButton.hovered || seekStrip.hovered
                                            || seekStrip.pressed

    property bool _idle: false

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        // only the video stays dark
        palette: VLCStyle.darkPalette
        colorSet: ColorContext.Window
    }

    readonly property ColorContext _badgeContext: ColorContext {
        id: badgeTheme

        palette: VLCStyle.darkPalette
        colorSet: ColorContext.Badge
    }

    function backToFullWindow() {
        MainCtx.pictureInPicture = false
    }

    // close stops playback and gives the full window back, VLC keeps running
    function close() {
        MainPlaylistController.stop()
        MainCtx.pictureInPicture = false
    }

    function _wake() {
        _idle = false
        idleTimer.restart()
    }

    // the window buttons of the full window are gone: their hit areas must
    // not catch the clicks of this small window (Windows snap layouts)
    Component.onCompleted: {
        const buttons = MainCtx.csdButtonModel.windowCSDButtons
        for (let i = 0; i < buttons.length; ++i)
            buttons[i].rect = Qt.rect(0, 0, 0, 0)
    }

    Keys.onPressed: (event) => {
        if (event.accepted)
            return
        if (event.key === Qt.Key_Escape) {
            root.backToFullWindow()
            event.accepted = true
            return
        }
        MainCtx.sendHotkey(event.key, event.modifiers)
    }

    Rectangle {
        anchors.fill: parent
        color: theme.bg.primary
    }

    VideoSurface {
        id: videoSurface

        anchors.fill: parent

        visible: MainCtx.hasEmbededVideo
        focus: visible

        videoSurfaceProvider: MainCtx.videoSurfaceProvider
    }

    // music: the cover art, or the cone without one
    Image {
        id: cover

        anchors.fill: parent
        anchors.margins: VLCStyle.margin_small

        visible: !MainCtx.hasEmbededVideo

        source: (Player.artwork && Player.artwork.toString())
                ? VLCAccessImage.uri(Player.artwork)
                : (MainCtx.useXmasCone() ? "qrc:///logo/vlc48-xmas.png" : "qrc:///misc/cone.svg")
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true

        Accessible.role: Accessible.Graphic
        Accessible.name: qsTr("Cover art")
    }

    // the pointer: drag moves the window, a click plays or pauses, a double
    // click gives the full window back, the right button opens the menu
    MouseArea {
        id: pointer

        anchors.fill: parent

        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: root._idle && !root.controlsVisible ? Qt.BlankCursor : Qt.ArrowCursor

        property point _pressPosition
        property bool _moving: false

        onPositionChanged: (mouse) => {
            root._wake()
            clickGesture.move(mouse.x, mouse.y)

            if (!(pressedButtons & Qt.LeftButton) || _moving)
                return

            const distance = Math.hypot(mouse.x - _pressPosition.x, mouse.y - _pressPosition.y)
            if (distance > Qt.styleHints.startDragDistance) {
                _moving = true
                clickGesture.cancel()
                MainCtx.intfMainWindow.startSystemMove()
            }
        }

        onPressed: (mouse) => {
            root._wake()
            if (mouse.button === Qt.RightButton) {
                contextMenu.popup(mapToGlobal(mouse.x, mouse.y))
                return
            }
            _pressPosition = Qt.point(mouse.x, mouse.y)
            _moving = false
            clickGesture.move(mouse.x, mouse.y)
            clickGesture.press()
        }

        onReleased: (mouse) => {
            if (mouse.button === Qt.LeftButton)
                clickGesture.release()
        }

        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            clickGesture.doubleClick()
            root.backToFullWindow()
        }

        onExited: clickGesture.cancel()
    }

    ClickGesture {
        id: clickGesture

        onSingleClicked: MainPlaylistController.togglePlayPause()
    }

    QmlAudioContextMenu {
        id: contextMenu
        ctx: MainCtx
    }

    Timer {
        id: idleTimer

        interval: Math.max(MainCtx.mouseHideTimeout, 1000)
        onTriggered: root._idle = true
    }

    ShortcutMessagePill {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: VLCStyle.margin_normal
    }

    // the hover controls
    FocusScope {
        id: controls

        anchors.fill: parent

        opacity: root.controlsVisible ? 1 : 0
        visible: opacity > 0

        // a fade, also with reduced motion
        Behavior on opacity {
            NumberAnimation { duration: VLCStyle.duration_short }
        }

        // keeps the controls readable over any picture
        Rectangle {
            anchors.fill: parent
            color: badgeTheme.shadow.alpha(0.35)
        }

        Widgets.IconButton {
            id: backButton

            anchors {
                top: parent.top
                left: parent.left
                margins: VLCStyle.margin_xsmall
            }

            font.pixelSize: VLCStyle.icon_PIP

            text: VLCIcons.maximize
            description: qsTr("Back to full window")

            Navigation.parentItem: controls
            Navigation.rightItem: closeButton
            Navigation.downItem: playButton

            onClicked: root.backToFullWindow()
        }

        Widgets.IconButton {
            id: closeButton

            anchors {
                top: parent.top
                right: parent.right
                margins: VLCStyle.margin_xsmall
            }

            font.pixelSize: VLCStyle.icon_PIP

            text: VLCIcons.close
            description: qsTr("Close")

            Navigation.parentItem: controls
            Navigation.leftItem: backButton
            Navigation.downItem: playButton

            onClicked: root.close()
        }

        Widgets.IconButton {
            id: playButton

            anchors.centerIn: parent

            focus: true

            font.pixelSize: VLCStyle.icon_large

            text: (Player.playingState === Player.PLAYING_STATE_PLAYING)
                  ? VLCIcons.pause_filled
                  : VLCIcons.play_filled
            description: (Player.playingState === Player.PLAYING_STATE_PLAYING)
                         ? qsTr("Pause")
                         : qsTr("Play")

            Navigation.parentItem: controls
            Navigation.upItem: backButton
            Navigation.downItem: seekStrip

            onClicked: MainPlaylistController.togglePlayPause()
        }

        // the thin seek strip along the bottom edge
        T.Slider {
            id: seekStrip

            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                leftMargin: VLCStyle.margin_small
                rightMargin: VLCStyle.margin_small
                bottomMargin: VLCStyle.margin_xsmall
            }

            implicitHeight: VLCStyle.dp(12, VLCStyle.scale)

            enabled: Player.seekable
            hoverEnabled: true
            focusPolicy: Qt.TabFocus

            from: 0
            to: 1
            value: Player.position
            live: true

            onMoved: Player.position = value

            Accessible.name: qsTr("Seek bar")

            Navigation.parentItem: controls
            Navigation.upItem: playButton

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

            background: Item {
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    height: (seekStrip.hovered || seekStrip.pressed || seekStrip.visualFocus)
                            ? VLCStyle.dp(4, VLCStyle.scale)
                            : VLCStyle.dp(2, VLCStyle.scale)
                    radius: height / 2

                    color: badgeTheme.fg.primary.alpha(0.3)

                    Rectangle {
                        width: parent.width * seekStrip.visualPosition
                        height: parent.height
                        radius: parent.radius

                        color: theme.accent
                    }
                }
            }

            handle: null
        }
    }
}
