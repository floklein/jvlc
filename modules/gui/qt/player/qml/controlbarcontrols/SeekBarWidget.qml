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
import VLC.Player
import VLC.Util

// The inline seek bar of the Control bar: current time, thick seek bar and
// duration (Figma 1908:7385). It takes all the room left by the other items.
FocusScope {
    id: root

    property bool paintOnly: false

    // fill the zone: ControlRepeater stretches items with a minimum width up
    // to their implicit width
    Layout.minimumWidth: _minimumBarWidth
    implicitWidth: paintOnly ? VLCStyle.dp(240, VLCStyle.scale)
                             : VLCStyle.dp(10000, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

    readonly property real spacing: VLCStyle.spacing_8

    readonly property real _timeWidth: VLCStyle.dp(76, VLCStyle.scale)

    // the subtitle timeline replaces the seek bar (Show subtitle timeline):
    // start [‹ − lines + ›] end, the buttons over the ends of the strip
    readonly property bool _timeline: !paintOnly && Player.subtitleTimeline.shown
    readonly property real _timelineButtonsWidth: _timeline ? VLCStyle.dp(84, VLCStyle.scale) : 0
    readonly property real _minimumBarWidth: VLCStyle.dp(64, VLCStyle.scale) + _timelineButtonsWidth

    // how far the Control bar folds (ControlBarFolding.Level)
    property int foldLevel: ControlBarFolding.Full

    // the room the seek bar needs at each fold level
    readonly property var foldWidths: [
        _minimumBarWidth + (_timeWidth + spacing) * 2,
        _minimumBarWidth + _timeWidth + spacing,
        _minimumBarWidth
    ]

    // in a narrow window the seek bar stays: the duration goes first, then
    // the current time
    readonly property bool _showDuration: foldLevel < ControlBarFolding.NoDuration
                                          && width >= (_timeWidth + spacing) * 2 + _minimumBarWidth
    readonly property bool _showTime: foldLevel < ControlBarFolding.NoTime
                                      && width >= _timeWidth + spacing + _minimumBarWidth

    // where the timeline's span starts or ends: "00:01:50*", the hours shown
    // as the seek bar's (Figma 3744:12370); the star tells it from the time
    function _formatSpanMS(ms : real) : string {
        const total = Math.floor(Math.max(ms, 0) / 1000)
        const pad = (n) => String(n).padStart(2, "0")
        return pad(Math.floor(total / 3600)) + ":" + pad(Math.floor(total / 60) % 60)
               + ":" + pad(total % 60) + "*"
    }

    // ‹ › move the subtitles by 100 ms, as the subtitle delay
    readonly property int _shiftStep: 100

    // the glyph of a timeline key, 2 px strokes with round ends (Figma 3750:12502)
    enum Glyph { ChevronLeft, ChevronRight, Minus, Plus }

    // a 2 px stroke with round ends from (x1, y1) to (x2, y2), in its
    // parent's coordinates
    component GlyphStroke : Rectangle {
        required property real x1
        required property real y1
        required property real x2
        required property real y2

        readonly property real _thickness: VLCStyle.dp(2, VLCStyle.scale)
        readonly property real _length: Math.hypot(x2 - x1, y2 - y1)

        x: (x1 + x2) / 2 - width / 2
        y: (y1 + y2) / 2 - height / 2
        width: _thickness
        height: _length + _thickness
        radius: _thickness / 2
        rotation: Math.atan2(x1 - x2, y2 - y1) * 180 / Math.PI
        antialiasing: true
    }

    // a small accent key over an end of the timeline strip (Figma 3750:12502):
    // Accent/Dim, in a 2 px ring of the strip's color, with a vector glyph
    component TimelineButton : Widgets.IconToolButton {
        id: timelineButton

        required property int glyph

        visible: root._timeline

        // over the strip
        z: 1
        y: VLCStyle.dp(2, VLCStyle.scale)
        width: VLCStyle.dp(18, VLCStyle.scale)
        height: root.implicitHeight - y * 2

        // the glyph is drawn, not a font icon
        text: " "

        radius: VLCStyle.radius_small
        // the strip's own colors
        backgroundColor: (hovered || visualFocus) ? timelineBar.colorContext.fg.primary
                                                  : timelineBar.colorContext.fg.highlight
        color: timelineBar.colorContext.bg.primary
        opacity: enabled ? 1 : 0.5

        // the ring round the key
        Rectangle {
            z: -1

            anchors.fill: parent
            anchors.margins: -_ring

            readonly property real _ring: VLCStyle.dp(2, VLCStyle.scale)

            radius: timelineButton.radius + _ring
            color: "transparent"
            border.width: _ring
            border.color: timelineBar.colorContext.bg.primary
        }

        contentItem: Item {
            // Figma 3750:12503 draws the right chevron 1 px right of the center
            readonly property real _cx: timelineButton.width / 2
                                        + ((timelineButton.glyph === SeekBarWidget.Glyph.ChevronRight) ? VLCStyle.dp(1, VLCStyle.scale) : 0)
            readonly property real _cy: timelineButton.height / 2
            // the chevron's reach: 4x12 px; the minus and plus span 10 px
            readonly property real _dx: VLCStyle.dp(2, VLCStyle.scale)
            readonly property real _dy: VLCStyle.dp(6, VLCStyle.scale)
            readonly property real _arm: VLCStyle.dp(5, VLCStyle.scale)

            GlyphStroke {
                color: timelineButton.color
                visible: timelineButton.glyph !== SeekBarWidget.Glyph.Plus
                         && timelineButton.glyph !== SeekBarWidget.Glyph.Minus
                readonly property real _tip: (timelineButton.glyph === SeekBarWidget.Glyph.ChevronLeft) ? -parent._dx : parent._dx
                x1: parent._cx - _tip; y1: parent._cy - parent._dy
                x2: parent._cx + _tip; y2: parent._cy
            }
            GlyphStroke {
                color: timelineButton.color
                visible: timelineButton.glyph !== SeekBarWidget.Glyph.Plus
                         && timelineButton.glyph !== SeekBarWidget.Glyph.Minus
                readonly property real _tip: (timelineButton.glyph === SeekBarWidget.Glyph.ChevronLeft) ? -parent._dx : parent._dx
                x1: parent._cx + _tip; y1: parent._cy
                x2: parent._cx - _tip; y2: parent._cy + parent._dy
            }
            GlyphStroke {
                color: timelineButton.color
                visible: timelineButton.glyph === SeekBarWidget.Glyph.Plus
                         || timelineButton.glyph === SeekBarWidget.Glyph.Minus
                x1: parent._cx - parent._arm; y1: parent._cy
                x2: parent._cx + parent._arm; y2: parent._cy
            }
            GlyphStroke {
                color: timelineButton.color
                visible: timelineButton.glyph === SeekBarWidget.Glyph.Plus
                x1: parent._cx; y1: parent._cy - parent._arm
                x2: parent._cx; y2: parent._cy + parent._arm
            }
        }

        Navigation.parentItem: root
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
    }

    // the Current Time's key when hovered (Figma 1904:7239: BG/B2)
    ColorContext {
        id: viewTheme
        palette: theme.palette
        colorSet: ColorContext.View
    }

    signal requestLockUnlockAutoHide(bool lock)

    // the Current Time, clicked: a field to type the time to jump to (Figma
    // 1904:7241); Enter jumps, Escape or a click elsewhere leaves it
    property bool _editingTime: false

    function _startEditingTime() {
        if (root.paintOnly || root._timeline || !Player.seekable)
            return
        timeField.text = Player.time.formatHMS(VLCTick.WithHours)
        root._editingTime = true
        timeField.cursorPosition = timeField.text.length
        timeField.forceActiveFocus(Qt.MouseFocusReason)
    }

    function _stopEditingTime(jump : bool) {
        if (!root._editingTime)
            return
        if (jump) {
            const seconds = Helpers.parseTime(timeField.text)
            if (seconds >= 0 && (!Player.length.valid() || seconds <= Player.length.toSeconds()))
                Player.jumpToTime(seconds)
        }
        root._editingTime = false
        if (timeField.activeFocus)
            slider.forceActiveFocus(Qt.OtherFocusReason)
    }

    on_EditingTimeChanged: {
        // the bars stay and the hotkeys wait while the time is typed
        MainCtx.useGlobalShortcuts = !_editingTime
        root.requestLockUnlockAutoHide(_editingTime)
    }

    function showChapterMarks() {
        slider.showChapterMarks()
    }

    // flash the chapter marks whenever the bar appears
    onVisibleChanged: {
        if (visible)
            showChapterMarks()
    }

    Accessible.ignored: true

    RowLayout {
        anchors.fill: parent

        spacing: root.spacing

        // the Current Time (Figma 1904:7238)
        Item {
            id: timeBox

            visible: root._showTime

            Layout.preferredWidth: root._timeWidth
            Layout.fillHeight: true

            // a click types a time to jump to; not on the timeline, whose
            // label is where its span starts
            readonly property bool _editable: !root.paintOnly && !root._timeline && Player.seekable

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Jump to Time")
            Accessible.ignored: !_editable || root._editingTime
            Accessible.onPressAction: root._startEditingTime()

            // hovered or typed in: a BG/B2 key (Figma 1904:7239)
            Rectangle {
                anchors.fill: parent

                visible: timeBox._editable && (timeHover.hovered || root._editingTime)

                radius: VLCStyle.radius_normal
                color: viewTheme.bg.secondary
            }

            T.Label {
                id: timeLabel

                anchors.fill: parent

                visible: !root._editingTime

                text: {
                    if (root.paintOnly)
                        return "00:02:10"
                    // the timeline: where its span starts
                    if (root._timeline)
                        return root._formatSpanMS(timelineBar.windowStart)
                    const length = Player.length
                    return Player.time.formatHMS((length.isSubSecond() ? VLCTick.SubSecondFormattedAsMS : 0)
                                                 | VLCTick.WithHours)
                }

                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_large
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                Accessible.ignored: true

                // TODO: Do not use `Binding` when minimum Qt is 6.12:
                Binding {
                    target: timeLabel
                    property: "mutabilityGroup"
                    when: (timeLabel.mutabilityGroup !== undefined)
                    value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.ModerateMutabilityGroup
                                                                                  : Item.StaticMutabilityGroup
                }
            }

            // the time typed: the digits FG/1 (Figma 1904:7242)
            TextInput {
                id: timeField

                anchors.fill: parent

                visible: root._editingTime
                enabled: root._editingTime

                color: viewTheme.fg.highlight
                selectionColor: theme.accent
                selectedTextColor: viewTheme.bg.secondary
                font.pixelSize: VLCStyle.fontSize_large
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                inputMethodHints: Qt.ImhPreferNumbers
                validator: RegularExpressionValidator { regularExpression: /[0-9:]*/ }

                Accessible.name: qsTr("Time to jump to, as hours:minutes:seconds")

                onAccepted: root._stopEditingTime(true)

                onActiveFocusChanged: {
                    if (!activeFocus)
                        root._stopEditingTime(false)
                }

                // the keys stay in the field: none reaches the player's hotkeys
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Escape) {
                        root._stopEditingTime(false)
                        event.accepted = true
                        return
                    }
                    if (event.key !== Qt.Key_Tab && event.key !== Qt.Key_Backtab)
                        event.accepted = true
                }
            }

            HoverHandler {
                id: timeHover

                enabled: timeBox._editable
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                enabled: timeBox._editable && !root._editingTime
                onTapped: root._startEditingTime()
            }
        }

        SubtitleTimelineBar {
            id: timelineBar

            visible: root._timeline

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            barHeight: root.implicitHeight

            // the lines stay clear of the keys over both ends
            leftReserve: zoomOutButton.x + zoomOutButton.width
            rightReserve: timelineBar.width - zoomInButton.x

            focus: root._timeline

            Navigation.parentItem: root
            Navigation.leftItem: zoomOutButton
            Navigation.rightItem: zoomInButton

            Keys.onPressed: (event) => {
                Navigation.defaultKeyAction(event)
            }

            TimelineButton {
                id: shiftEarlierButton

                x: VLCStyle.dp(2, VLCStyle.scale)

                glyph: SeekBarWidget.Glyph.ChevronLeft
                description: qsTr("Show subtitles %1 ms earlier").arg(root._shiftStep)
                onClicked: Player.subtitleDelayMS = Player.subtitleDelayMS - root._shiftStep

                Navigation.leftItem: root.Navigation.leftItem
                Navigation.rightItem: zoomOutButton
            }

            TimelineButton {
                id: zoomOutButton

                x: shiftEarlierButton.x + shiftEarlierButton.width + VLCStyle.dp(2, VLCStyle.scale)
                width: VLCStyle.dp(22, VLCStyle.scale)

                glyph: SeekBarWidget.Glyph.Minus
                description: qsTr("Zoom out")
                enabled: Player.subtitleTimeline.canZoomOut
                onClicked: Player.subtitleTimeline.zoomOut()

                Navigation.leftItem: shiftEarlierButton
                Navigation.rightItem: timelineBar
            }

            TimelineButton {
                id: zoomInButton

                x: shiftLaterButton.x - width - VLCStyle.dp(2, VLCStyle.scale)
                width: VLCStyle.dp(22, VLCStyle.scale)

                glyph: SeekBarWidget.Glyph.Plus
                description: qsTr("Zoom in")
                enabled: Player.subtitleTimeline.canZoomIn
                onClicked: Player.subtitleTimeline.zoomIn()

                Navigation.leftItem: timelineBar
                Navigation.rightItem: shiftLaterButton
            }

            TimelineButton {
                id: shiftLaterButton

                // Figma 3750:12503: flush with the strip's end
                x: timelineBar.width - width

                glyph: SeekBarWidget.Glyph.ChevronRight
                description: qsTr("Show subtitles %1 ms later").arg(root._shiftStep)
                onClicked: Player.subtitleDelayMS = Player.subtitleDelayMS + root._shiftStep

                Navigation.leftItem: zoomInButton
                Navigation.rightItem: root.Navigation.rightItem
            }
        }

        SliderBar {
            id: slider

            visible: !root._timeline

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            barHeight: root.implicitHeight

            // local music with Show waveform on draws the waveform seek bar
            peaks: root.paintOnly ? [] : Player.waveform.peaks

            focus: !root._timeline
            activeFocusOnTab: true

            enabled: !root.paintOnly
                     && (Player.playingState === Player.PLAYING_STATE_PLAYING
                         || Player.playingState === Player.PLAYING_STATE_PAUSED)

            AccessibleCompat.id: "progressSlider"

            Navigation.parentItem: root
            Navigation.leftItem: root.Navigation.leftItem
            Navigation.rightItem: root.Navigation.rightItem

            Keys.onPressed: (event) => {
                Navigation.defaultKeyAction(event)
            }
        }

        T.Label {
            id: durationLabel

            visible: root._showDuration

            Layout.preferredWidth: root._timeWidth
            Layout.fillHeight: true

            readonly property bool showRemainingTime: (MainCtx.showRemainingTime && Player.remainingTime.valid())

            text: {
                if (root.paintOnly)
                    return "00:25:40"
                // the timeline: where its span ends
                if (root._timeline)
                    return root._formatSpanMS(timelineBar.windowEnd)
                return showRemainingTime ? "-" + Player.remainingTime.formatHMS(VLCTick.SubSecondFormattedAsMS | VLCTick.WithHours)
                                         : Player.length.formatHMS(VLCTick.SubSecondFormattedAsMS | VLCTick.WithHours)
            }

            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_large
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            // a button, but on the timeline: a click, Enter or Space switches
            // between the duration and the remaining time
            readonly property bool _toggles: !root.paintOnly && !root._timeline

            Accessible.ignored: !_toggles
            Accessible.role: Accessible.Button
            Accessible.name: MainCtx.showRemainingTime ? qsTr("Show the duration")
                                                       : qsTr("Show the remaining time")
            Accessible.focusable: true
            Accessible.onPressAction: MainCtx.showRemainingTime = !MainCtx.showRemainingTime

            activeFocusOnTab: _toggles

            Navigation.parentItem: root
            Navigation.leftItem: slider
            Navigation.rightItem: root.Navigation.rightItem

            Keys.onPressed: (event) => {
                if (durationLabel._toggles && KeyHelper.matchOk(event)) {
                    event.accepted = true
                    MainCtx.showRemainingTime = !MainCtx.showRemainingTime
                    return
                }
                Navigation.defaultKeyAction(event)
            }

            // the keyboard focus ring
            Rectangle {
                anchors.fill: parent
                anchors.margins: - VLCStyle.focus_border

                visible: durationLabel.activeFocus

                radius: VLCStyle.radius_xxsmall
                color: "transparent"
                border.width: VLCStyle.focus_border
                // the focus ring color: theme.visualFocus is only set on a focused context
                border.color: theme.accent
            }

            // TODO: Do not use `Binding` when minimum Qt is 6.12:
            Binding {
                target: durationLabel
                property: "mutabilityGroup"
                when: (durationLabel.mutabilityGroup !== undefined)
                value: ((Player.playingState === Player.PLAYING_STATE_PLAYING) &&
                        durationLabel.showRemainingTime) ? Item.ModerateMutabilityGroup
                                                         : Item.StaticMutabilityGroup
            }

            // clicking the duration shows the remaining time instead
            TapHandler {
                enabled: !root.paintOnly && !root._timeline
                onTapped: MainCtx.showRemainingTime = !MainCtx.showRemainingTime
            }

            HoverHandler {
                id: durationHover
            }

            Widgets.ToolTipExt {
                colorContext.palette: theme.palette

                visible: durationHover.hovered && MainCtx.showRemainingTime && Player.rate !== 1
                delay: VLCStyle.delayToolTipAppear
                text: {
                    if (!visible)
                        return ""

                    const playbackSpeed = Math.abs(Player.rate)
                    const adjustedRemainingTime = Player.remainingTime.scale(1 / playbackSpeed)
                        ? Player.remainingTime.scale(1 / playbackSpeed)
                        : Player.remainingTime

                    return qsTr("%1 (%2x)")
                        .arg(adjustedRemainingTime.formatHMS(VLCTick.SubSecondFormattedAsMS | VLCTick.WithHours))
                        .arg(+Player.rate.toFixed(2))
                }

                x: (parent.width - width) / 2
            }
        }
    }
}
