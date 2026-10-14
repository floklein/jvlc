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
    readonly property real _minimumBarWidth: VLCStyle.dp(64, VLCStyle.scale)

    // in a narrow window the seek bar stays: the duration goes first, then
    // the current time
    readonly property bool _showDuration: width >= (_timeWidth + spacing) * 2 + _minimumBarWidth
    readonly property bool _showTime: width >= _timeWidth + spacing + _minimumBarWidth

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

        T.Label {
            id: timeLabel

            visible: root._showTime

            Layout.preferredWidth: root._timeWidth
            Layout.fillHeight: true

            text: {
                if (root.paintOnly)
                    return "00:02:10"
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

        SliderBar {
            id: slider

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            barHeight: root.implicitHeight

            focus: true
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
                return showRemainingTime ? "-" + Player.remainingTime.formatHMS()
                                         : Player.length.formatHMS()
            }

            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_large
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Accessible.ignored: true

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
                enabled: !root.paintOnly
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
    }
}
