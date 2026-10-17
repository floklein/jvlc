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
import VLC.Player
import VLC.Util

// The subtitle timeline (Figma 3744:12361): the thick seek bar shows one span
// of the media with the subtitle lines as blocks. Clicking seeks; dragging
// moves every line, which sets the subtitle delay.
T.Control {
    id: control

    property real barHeight: VLCStyle.dp(28, VLCStyle.scale)

    readonly property SubtitleTimeline timeline: Player.subtitleTimeline

    // the media, in milliseconds; a live stream has no length
    readonly property real length: Player.length.valid() ? Player.length.toMilliseconds() : -1
    readonly property real time: Player.time.toMilliseconds()

    // the shown span: a page that turns when playback leaves it
    readonly property real windowStart: timeline.windowStart(time, length)
    readonly property real windowEnd: windowStart + (length > 0 ? Math.min(timeline.span, length)
                                                                : timeline.span)

    // the room the keys over each end take (SeekBarWidget): the span is drawn
    // between them
    property real leftReserve: 0
    property real rightReserve: 0

    readonly property real _inset: VLCStyle.dp(2, VLCStyle.scale)
    readonly property real _trackLeft: leftReserve + _inset
    readonly property real _trackWidth: Math.max(1, width - _trackLeft - rightReserve - _inset)

    function _timeAt(x : real) : real {
        return windowStart + Helpers.clamp(x - _trackLeft, 0, _trackWidth) / _trackWidth * (windowEnd - windowStart)
    }

    function _xOf(ms : real) : real {
        return _trackLeft + (ms - windowStart) / (windowEnd - windowStart) * _trackWidth
    }

    implicitHeight: barHeight
    height: implicitHeight

    padding: 0
    hoverEnabled: false
    focusPolicy: Qt.StrongFocus

    function _jump(forward) {
        _accessibleSeeking = true
        if (forward)
            Player.jumpFwd()
        else
            Player.jumpBwd()
    }

    Keys.onRightPressed: _jump(true)
    Keys.onLeftPressed: _jump(false)

    // read as the seek bar: "12:34 of 45:00", updated on focus and on keyboard
    // seeks rather than every second
    property string _accessibleName: ""
    property bool _accessibleSeeking: false

    function _updateAccessibleName() {
        _accessibleName = Helpers.seekBarName(Player.time.formatHMS(0), Player.length.formatHMS(0))
    }

    onActiveFocusChanged: {
        if (activeFocus)
            _updateAccessibleName()
    }

    Connections {
        target: Player

        function onTimeChanged() {
            // screen readers only read the name changes of the focused bar
            if (control._accessibleSeeking || !control.activeFocus) {
                control._accessibleSeeking = false
                control._updateAccessibleName()
            }
        }
    }

    Accessible.role: Accessible.Slider
    Accessible.name: _accessibleName
    Accessible.description: qsTr("Subtitle timeline")
    Accessible.onIncreaseAction: _jump(true)
    Accessible.onDecreaseAction: _jump(false)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Slider

        enabled: control.enabled
        focused: control.visualFocus
        hovered: hoverHandler.hovered
    }

    // the Accent/Main of the playhead
    ColorContext {
        id: accentTheme
        palette: theme.palette
        colorSet: ColorContext.ButtonAccent
    }

    HoverHandler {
        id: hoverHandler
        cursorShape: dragHandler.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
    }

    // a click seeks
    TapHandler {
        acceptedButtons: Qt.LeftButton

        onTapped: (eventPoint) => {
            // the keys over the ends have their own actions
            if (eventPoint.position.x < control.leftReserve
                    || eventPoint.position.x > control.width - control.rightReserve)
                return
            control.forceActiveFocus(Qt.MouseFocusReason)
            if (control.length > 0)
                Player.position = control._timeAt(eventPoint.position.x) / control.length
        }
    }

    // dragging shifts the whole track: the lines follow the pointer
    DragHandler {
        id: dragHandler

        acceptedButtons: Qt.LeftButton
        target: null
        xAxis.enabled: true
        yAxis.enabled: false

        property int _startDelay: 0

        onActiveChanged: {
            if (active) {
                control.forceActiveFocus(Qt.MouseFocusReason)
                _startDelay = Player.subtitleDelayMS
            }
        }

        onTranslationChanged: {
            if (!active)
                return
            const delay = control.timeline.draggedDelay(_startDelay, translation.x, control._trackWidth)
            if (delay !== Player.subtitleDelayMS)
                Player.subtitleDelayMS = delay
        }
    }

    background: Rectangle {
        color: theme.bg.primary
        radius: VLCStyle.radius_normal

        // keyboard focus ring
        Rectangle {
            anchors.fill: parent
            anchors.margins: -VLCStyle.focus_border

            radius: parent.radius + VLCStyle.focus_border

            color: "transparent"
            border.width: VLCStyle.focus_border
            border.color: theme.visualFocus

            visible: control.visualFocus
        }
    }

    contentItem: Item {
        clip: true

        // the played part of the span, from the start of the strip as the
        // seek bar's (Figma 3744:12372)
        Rectangle {
            x: control._inset
            y: control._inset
            width: Helpers.clamp(control._xOf(control.time) - control._inset, 0, control.width - control._inset * 2)
            height: parent.height - control._inset * 2

            radius: VLCStyle.radius_small
            color: theme.bg.highlight
        }

        // the lines: translucent, so the played ones take the played color
        SubtitleTimelineBlocks {
            x: control._trackLeft
            y: control._inset
            width: control._trackWidth
            height: parent.height - control._inset * 2

            timeline: control.timeline
            from: control.windowStart
            to: control.windowEnd
            delay: Player.subtitleDelayMS

            // white at 14 % (Figma 3749:12473), so the played ones take the
            // played color; black in the light Theme, over a white strip
            color: dragHandler.active ? theme.accent
                                      : Qt.alpha((theme.palette?.isDark ?? true) ? "white" : "black", 0.14)
            radius: VLCStyle.dp(1, VLCStyle.scale)
        }

        // the playhead: the seek bar's handle
        Rectangle {
            x: Math.round(control._xOf(control.time) - width / 2)
            y: control._inset
            width: VLCStyle.dp(8, VLCStyle.scale)
            height: parent.height - control._inset * 2

            radius: VLCStyle.radius_small
            // Accent/Main, as the seek bar's handle
            color: accentTheme.bg.primary
        }
    }
}
