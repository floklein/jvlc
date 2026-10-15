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
import VLC.Widgets as Widgets
import VLC.Player
import VLC.Dialogs
import VLC.Style
import VLC.Util

// The Playback tab of the Adjustments sidebar (Figma 5187:24308): statistics,
// waveform, sleep timer, audio visualizer and speed. Every change applies
// at once.
FocusScope {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // the choices of the sleep timer
    readonly property var _sleepChoices: [
        { text: qsTr("Off"), mode: SleepTimer.Off },
        { text: qsTr("15 min"), mode: SleepTimer.Duration, minutes: 15 },
        { text: qsTr("30 min"), mode: SleepTimer.Duration, minutes: 30 },
        { text: qsTr("45 min"), mode: SleepTimer.Duration, minutes: 45 },
        { text: qsTr("60 min"), mode: SleepTimer.Duration, minutes: 60 },
        { text: qsTr("90 min"), mode: SleepTimer.Duration, minutes: 90 },
        { text: qsTr("End of this item"), mode: SleepTimer.EndOfItem },
        { text: qsTr("End of Queue"), mode: SleepTimer.EndOfQueue },
        { text: qsTr("Custom…"), mode: SleepTimer.Duration, minutes: -1 },
    ]

    readonly property int _customIndex: _sleepChoices.length - 1

    // "Custom…" was picked: the minutes field shows until a duration starts
    property bool _customEditing: false

    // the choice matching the running timer
    function _sleepIndex() : int {
        const timer = Player.sleepTimer
        if (_customEditing)
            return _customIndex
        switch (timer.mode) {
        case SleepTimer.EndOfItem:
            return 6
        case SleepTimer.EndOfQueue:
            return 7
        case SleepTimer.Duration: {
            const i = _sleepChoices.findIndex((c) => c.minutes === timer.minutes)
            return i >= 0 ? i : _customIndex
        }
        }
        return 0
    }

    function _formatTimeLeft(ms : real) : string {
        const total = Math.ceil(ms / 1000)
        const h = Math.floor(total / 3600)
        const m = Math.floor(total / 60) % 60
        const s = total % 60
        const mmss = String(m).padStart(h > 0 ? 2 : 1, "0") + ":" + String(s).padStart(2, "0")
        return h > 0 ? h + ":" + mmss : mmss
    }

    function _sleepText() : string {
        const timer = Player.sleepTimer
        if (_customEditing || timer.mode === SleepTimer.Off)
            return _sleepChoices[_sleepIndex()].text
        let label
        if (timer.mode === SleepTimer.Duration)
            label = qsTr("%1 min").arg(timer.minutes)
        else
            label = _sleepChoices[_sleepIndex()].text
        if (timer.timeLeft < 0)
            return label
        //: sleep timer: the choice and the time before the pause, "30 min · 12:04 left"
        return qsTr("%1 · %2 left").arg(label).arg(_formatTimeLeft(timer.timeLeft))
    }

    function _pickSleep(index : int) {
        const choice = _sleepChoices[index]
        _customEditing = false
        switch (choice.mode) {
        case SleepTimer.Off:
            Player.sleepTimer.cancel()
            break
        case SleepTimer.EndOfItem:
            Player.sleepTimer.startEndOfItem()
            break
        case SleepTimer.EndOfQueue:
            Player.updateSleepTimerQueue()
            Player.sleepTimer.startEndOfQueue()
            break
        default:
            if (choice.minutes > 0) {
                Player.sleepTimer.start(choice.minutes)
            } else {
                _customEditing = true
                Qt.callLater(() => customField.forceActiveFocus(Qt.TabFocusReason))
            }
        }
    }

    // music plays: the sidebar has only the Playback and Audio tabs
    readonly property bool _music: Player.isStarted && !Player.hasVideoOutput

    function _timelineUsable() : bool {
        return subtitleTimelineRow.visible && subtitleTimelineRow.enabled
    }

    function _belowStatistics() : Item {
        if (waveformRow.visible)
            return waveformRow
        return _timelineUsable() ? subtitleTimelineRow : sleepCombo
    }

    function _aboveSleepTimer() : Item {
        if (_timelineUsable())
            return subtitleTimelineRow
        return waveformRow.visible ? waveformRow : statisticsRow
    }

    function _applySpeed(text : string) {
        const speed = Helpers.parseSpeed(text)
        if (speed > 0)
            Player.rate = speed
    }

    Flickable {
        id: flickable

        anchors.fill: parent

        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

        ColumnLayout {
            id: content

            width: flickable.width

            spacing: 0

            // Figma: 20 px on the sides, 10 px above the first row
            Item { implicitHeight: VLCStyle.spacing_10 }

            ToggleRow {
                id: statisticsRow

                text: qsTr("Show Statistics")
                checked: MainCtx.showStatistics
                onToggled: MainCtx.showStatistics = !MainCtx.showStatistics

                focus: true

                Navigation.parentItem: root
                Navigation.downItem: root._belowStatistics()
            }

            // local music only (#39)
            ToggleRow {
                id: waveformRow

                visible: Player.waveform.available

                text: qsTr("Show Waveform on Seek Bar")
                checked: MainCtx.showWaveform
                onToggled: MainCtx.showWaveform = !MainCtx.showWaveform

                Navigation.parentItem: root
                Navigation.upItem: statisticsRow
                Navigation.downItem: root._timelineUsable() ? subtitleTimelineRow : sleepCombo
            }

            // the subtitle timeline (#54): not for music; external text
            // subtitles only, the toggle says why otherwise
            ToggleRow {
                id: subtitleTimelineRow

                visible: !root._music

                enabled: Player.subtitleTimeline.unsupportedReason === ""
                opacity: enabled ? 1 : 0.5

                text: qsTr("Show Subtitle Timeline on Seek Bar")
                checked: MainCtx.showSubtitleTimeline
                onToggled: MainCtx.showSubtitleTimeline = !MainCtx.showSubtitleTimeline

                Navigation.parentItem: root
                Navigation.upItem: waveformRow.visible ? waveformRow : statisticsRow
                Navigation.downItem: sleepCombo
            }

            T.Label {
                id: subtitleTimelineReason

                visible: subtitleTimelineRow.visible && text !== ""

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20 + VLCStyle.spacing_4
                Layout.rightMargin: VLCStyle.spacing_20

                text: Player.subtitleTimeline.unsupportedReason
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
                wrapMode: Text.WordWrap
            }

            ChoiceRow {
                Layout.topMargin: VLCStyle.spacing_10
                Layout.bottomMargin: VLCStyle.spacing_20

                text: qsTr("Sleep Timer")

                ChoiceCombo {
                    id: sleepCombo

                    Accessible.name: qsTr("Sleep Timer")

                    model: root._sleepChoices
                    textRole: "text"

                    Binding on currentIndex {
                        value: root._sleepIndex()
                    }
                    displayText: root._sleepText()

                    onActivated: (index) => root._pickSleep(index)

                    Navigation.parentItem: root
                    Navigation.upItem: root._aboveSleepTimer()
                    Navigation.downItem: customRow.visible ? customField : visualizerCombo
                }
            }

            // the minutes of "Custom…"
            RowLayout {
                id: customRow

                visible: root._customEditing

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.bottomMargin: VLCStyle.spacing_20

                spacing: VLCStyle.spacing_10

                Item { Layout.fillWidth: true }

                Widgets.TextFieldExt {
                    id: customField

                    Layout.preferredWidth: VLCStyle.dp(120, VLCStyle.scale)
                    implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
                    radius: VLCStyle.radius_normal

                    placeholderText: qsTr("Minutes")
                    Accessible.name: qsTr("Sleep timer minutes")

                    validator: IntValidator { bottom: 1; top: 24 * 60 }

                    onAccepted: {
                        root._customEditing = false
                        Player.sleepTimer.start(parseInt(text, 10))
                        sleepCombo.forceActiveFocus(Qt.TabFocusReason)
                    }

                    Keys.onEscapePressed: (event) => {
                        root._customEditing = false
                        sleepCombo.forceActiveFocus(Qt.TabFocusReason)
                        event.accepted = true
                    }

                    Navigation.parentItem: root
                    Navigation.upItem: sleepCombo
                    Navigation.downItem: visualizerCombo
                }

                T.Label {
                    text: qsTr("min")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }
            }

            ChoiceRow {
                Layout.bottomMargin: VLCStyle.spacing_20

                text: qsTr("Audio Visualizer")

                ChoiceCombo {
                    id: visualizerCombo

                    Accessible.name: qsTr("Audio Visualizer")

                    // empty while there is no audio output
                    model: Player.audioVisualization
                    textRole: "display"
                    enabled: count > 0

                    Binding on currentIndex {
                        value: Player.audioVisualization.currentRow
                    }

                    onActivated: (index) => Player.audioVisualization.toggleIndex(index)

                    Navigation.parentItem: root
                    Navigation.upItem: customRow.visible ? customField : sleepCombo
                    Navigation.downItem: speedField
                }
            }

            // Figma "Frame 105": speed label and field, then the slider
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20

                spacing: VLCStyle.spacing_10

                RowLabel {
                    Layout.fillWidth: true
                    text: qsTr("Speed")

                    PresetMark {
                        anchors.right: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        keys: ["rate"]
                    }
                }

                Widgets.TextFieldExt {
                    id: speedField

                    Layout.preferredWidth: VLCStyle.dp(140, VLCStyle.scale)
                    implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
                    radius: VLCStyle.radius_normal

                    Accessible.name: qsTr("Playback speed")

                    enabled: Player.ratechangable

                    // follows the player, except while typed in
                    Binding on text {
                        when: !speedField.activeFocus
                        value: Helpers.formatSpeed(Player.rate)
                        restoreMode: Binding.RestoreNone
                    }

                    onEditingFinished: {
                        root._applySpeed(text)
                        text = Helpers.formatSpeed(Player.rate)
                    }

                    Navigation.parentItem: root
                    Navigation.upItem: visualizerCombo
                    Navigation.downItem: speedSlider
                }
            }

            SettingsSlider {
                id: speedSlider

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_14
                Layout.bottomMargin: VLCStyle.dp(15, VLCStyle.scale)

                enabled: Player.ratechangable

                from: 0.25
                to: 2
                stepSize: 0.25
                unit: "x"
                // a speed typed above 2x keeps the handle at the end
                value: Math.min(Math.max(Player.rate, from), to)
                accessibleName: qsTr("Playback speed")

                tickLabels: ["0.25", "0.5", "0.75", "1", "1.25", "1.5", "1.75", "2"]

                onMoved: (value) => { Player.rate = value }

                Navigation.parentItem: root
                Navigation.upItem: speedField
            }
        }
    }

    // A label, left of its control
    component RowLabel : T.Label {
        leftPadding: VLCStyle.spacing_4
        color: theme.fg.secondary
        font.pixelSize: VLCStyle.fontSize_normal
        font.family: VLCStyle.fontFamily
        elide: Text.ElideRight
    }

    // Figma "Frame 69": a label and an ON / OFF toggle
    component ToggleRow : FocusScope {
        id: toggleRow

        property alias text: toggleLabel.text
        property alias checked: toggle.checked

        signal toggled()

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20

        implicitHeight: Math.max(toggleLabel.implicitHeight, toggle.implicitHeight) + 2 * VLCStyle.spacing_10

        RowLabel {
            id: toggleLabel

            anchors.left: parent.left
            anchors.right: toggle.left
            anchors.rightMargin: VLCStyle.spacing_10
            anchors.verticalCenter: parent.verticalCenter
        }

        SettingsToggle {
            id: toggle

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            focus: true

            Accessible.name: toggleLabel.text

            onClicked: toggleRow.toggled()

            Navigation.parentItem: toggleRow
        }
    }

    // a label and a 220 px dropdown
    component ChoiceRow : RowLayout {
        property alias text: choiceLabel.text

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20

        spacing: VLCStyle.spacing_10

        RowLabel {
            id: choiceLabel
            Layout.fillWidth: true
        }
    }

    // Figma "EQ": the dropdown of the sidebar
    component ChoiceCombo : Widgets.ComboBoxExt {
        id: combo

        Layout.preferredWidth: VLCStyle.dp(220, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(48, VLCStyle.scale)

        font.pixelSize: VLCStyle.fontSize_normal
        leftPadding: VLCStyle.spacing_14 + VLCStyle.dp(2, VLCStyle.scale)
        rightPadding: VLCStyle.spacing_14

        color: theme.fg.secondary

        contentItem: Text {
            rightPadding: combo.spacing + combo.indicator.width
            text: combo.displayText
            color: combo.color
            font.pixelSize: VLCStyle.fontSize_normal
            font.family: VLCStyle.fontFamily
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: VLCStyle.radius_normal
            color: combo.bgColor
            border.width: combo.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: theme.accent

            Rectangle {
                anchors.fill: parent
                anchors.margins: VLCStyle.dp(2, VLCStyle.scale)
                radius: VLCStyle.radius_small
                color: combo.hovered ? theme.bg.highlight : combo.colorContext.bg.secondary
            }
        }
    }
}
