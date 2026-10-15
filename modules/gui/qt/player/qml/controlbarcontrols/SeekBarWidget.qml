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
    // ‹ − start [lines] end + ›
    readonly property bool _timeline: !paintOnly && Player.subtitleTimeline.shown
    readonly property real _timelineButtonsWidth: _timeline ? (implicitHeight + spacing) * 4 : 0
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

    // "01:50", or "01:01:50", as VLCTick.formatHMS
    function _formatMS(ms : real) : string {
        const total = Math.floor(Math.max(ms, 0) / 1000)
        const hours = Math.floor(total / 3600)
        const pad = (n) => String(n).padStart(2, "0")
        const mmss = pad(Math.floor(total / 60) % 60) + ":" + pad(total % 60)
        return hours > 0 ? pad(hours) + ":" + mmss : mmss
    }

    // ‹ › move the subtitles by 100 ms, as the subtitle delay
    readonly property int _shiftStep: 100

    component TimelineButton : Widgets.IconToolButton {
        visible: root._timeline

        Layout.preferredWidth: root.implicitHeight
        Layout.preferredHeight: root.implicitHeight
        Layout.alignment: Qt.AlignVCenter

        Navigation.parentItem: root
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton
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

        TimelineButton {
            id: shiftEarlierButton

            text: VLCIcons.chevron_left
            description: qsTr("Show subtitles %1 ms earlier").arg(root._shiftStep)
            onClicked: Player.subtitleDelayMS = Player.subtitleDelayMS - root._shiftStep

            Navigation.leftItem: root.Navigation.leftItem
            Navigation.rightItem: zoomOutButton
        }

        TimelineButton {
            id: zoomOutButton

            text: VLCIcons.remove
            description: qsTr("Zoom out")
            enabled: Player.subtitleTimeline.canZoomOut
            onClicked: Player.subtitleTimeline.zoomOut()

            Navigation.leftItem: shiftEarlierButton
            Navigation.rightItem: root._timeline ? timelineBar : slider
        }

        T.Label {
            id: timeLabel

            visible: root._showTime

            Layout.preferredWidth: root._timeWidth
            Layout.fillHeight: true

            text: {
                if (root.paintOnly)
                    return "00:02:10"
                // the timeline: where its span starts
                if (root._timeline)
                    return root._formatMS(timelineBar.windowStart)
                const length = Player.length
                return Player.time.formatHMS(length.isSubSecond() ? VLCTick.SubSecondFormattedAsMS : 0)
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

        SubtitleTimelineBar {
            id: timelineBar

            visible: root._timeline

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            barHeight: root.implicitHeight

            focus: root._timeline

            Navigation.parentItem: root
            Navigation.leftItem: zoomOutButton
            Navigation.rightItem: zoomInButton

            Keys.onPressed: (event) => {
                Navigation.defaultKeyAction(event)
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
                    return root._formatMS(timelineBar.windowEnd)
                return showRemainingTime ? "-" + Player.remainingTime.formatHMS()
                                         : Player.length.formatHMS()
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
                        .arg(adjustedRemainingTime.formatHMS())
                        .arg(+Player.rate.toFixed(2))
                }

                x: (parent.width - width) / 2
            }
        }

        TimelineButton {
            id: zoomInButton

            text: VLCIcons.add
            description: qsTr("Zoom in")
            enabled: Player.subtitleTimeline.canZoomIn
            onClicked: Player.subtitleTimeline.zoomIn()

            Navigation.leftItem: timelineBar
            Navigation.rightItem: shiftLaterButton
        }

        TimelineButton {
            id: shiftLaterButton

            text: VLCIcons.chevron_right
            description: qsTr("Show subtitles %1 ms later").arg(root._shiftStep)
            onClicked: Player.subtitleDelayMS = Player.subtitleDelayMS + root._shiftStep

            Navigation.leftItem: zoomInButton
            Navigation.rightItem: root.Navigation.rightItem
        }
    }
}
