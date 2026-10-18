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

// The Subtitles tab of the Adjustments sidebar (Figma 763:1958): the default
// language, the synchronization and the look of the subtitles. Every change
// applies at once, including on the line on screen.
FocusScope {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // the fields' stroke and fill, for the dropdowns drawn as fields
    readonly property ColorContext _fieldContext: ColorContext {
        id: fieldTheme
        colorSet: ColorContext.TextField
    }

    readonly property SubtitleAppearance appearance: Player.subtitleAppearance

    // styled ASS subtitles draw themselves: their look can't be changed
    readonly property bool styled: Player.subtitleTracks.styledSelected

    // the controls, top to bottom, for the keyboard navigation
    readonly property var _controls: [
        trackField, languageCombo,
        delayField, delaySlider, voiceHeardButton, textSeenButton,
        secondaryDelayField, secondaryDelaySlider,
        speedField, speedSlider, durationField, durationSlider,
        keepStylingRow, fontCombo, textColorRow, outlineColorRow, offsetField,
        sizeModeSegments, relativeSizeSlider, pixelSizeField,
        outlineSlider, backgroundSlider,
    ]

    function _usable(item) : bool {
        if (!item.enabled)
            return false
        for (let i = item; i && i !== root; i = i.parent)
            if (!i.visible)
                return false
        return true
    }

    function _neighbour(item, step : int) {
        let i = root._controls.indexOf(item) + step
        for (; i >= 0 && i < root._controls.length; i += step) {
            if (root._usable(root._controls[i]))
                return root._controls[i]
        }
        return null
    }

    // Up/Down: the next usable control, else leave the tab
    function _move(item, step : int) {
        const next = root._neighbour(item, step)
        if (next)
            next.forceActiveFocus(Qt.TabFocusReason)
        else if (step < 0)
            root.Navigation.defaultNavigationUp()
        else
            root.Navigation.defaultNavigationDown()
    }

    // the fields show seconds with milliseconds, "-4.000 s"
    function _formatSeconds(ms : int) : string {
        return (ms / 1000).toFixed(3) + " s"
    }

    // "1.5", "1.5 s", "-200 ms": milliseconds, or NaN
    function _parseMilliseconds(text : string) : real {
        const match = /^\s*([+-]?\d*[.,]?\d+)\s*(ms|s)?\s*$/.exec(text)
        if (!match)
            return NaN
        const value = parseFloat(match[1].replace(",", "."))
        return match[2] === "ms" ? value : value * 1000
    }

    function _parseNumber(text : string) : real {
        const match = /^\s*([+-]?\d*[.,]?\d+)/.exec(text)
        return match ? parseFloat(match[1].replace(",", ".")) : NaN
    }

    // "#B8B8B8", or an invalid color
    function _parseColor(text : string) {
        const match = /^\s*#?([0-9a-fA-F]{6})\s*$/.exec(text)
        return match ? Qt.color("#" + match[1]) : undefined
    }

    function _colorText(color) : string {
        return color.toString().toUpperCase()
    }

    readonly property var _relativeSizes: [
        { value: SubtitleAppearance.Smaller, text: qsTr("Smaller") },
        { value: SubtitleAppearance.Small, text: qsTr("Small") },
        { value: SubtitleAppearance.Normal, text: qsTr("Normal") },
        { value: SubtitleAppearance.Large, text: qsTr("Large") },
        { value: SubtitleAppearance.Larger, text: qsTr("Larger") },
    ]

    // the steps of the Outline Thickness range, in their order
    readonly property var _outlines: [
        { value: SubtitleAppearance.NoOutline, text: qsTr("None") },
        { value: SubtitleAppearance.ThinOutline, text: qsTr("Thin") },
        { value: SubtitleAppearance.NormalOutline, text: qsTr("Normal") },
        { value: SubtitleAppearance.ThickOutline, text: qsTr("Thick") },
    ]

    readonly property var _fonts: [qsTr("Default")].concat(Qt.fontFamilies())

    // the colors of the text renderer's list
    readonly property var _palette: [
        "#FFFFFF", "#C0C0C0", "#808080", "#000000", "#FFFF00", "#FF0000",
        "#FF00FF", "#800080", "#0000FF", "#000080", "#00FFFF", "#008080",
        "#00FF00", "#008000", "#808000", "#800000",
    ]

    DelayEstimator {
        id: delayEstimator

        onDelayChanged: Player.addSubtitleDelay(delayEstimator.delay)
    }

    Flickable {
        id: flickable

        anchors.fill: parent

        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // Figma has none: it shows while scrolling
        T.ScrollBar.vertical: AdjustmentsScrollBar { }

        ColumnLayout {
            id: content

            width: flickable.width

            spacing: 0

            // Defaults

            SectionHeader {
                text: qsTr("Defaults")
                divider: false
            }

            // Figma "Frame 92" (845:1597): the track and the language the next
            // media shows, in 48 px rows 5 px apart
            FieldRow {
                Layout.topMargin: VLCStyle.spacing_10

                text: qsTr("Subtitle Track")

                TrackField {
                    id: trackField

                    focus: true

                    Accessible.name: qsTr("Default subtitle track")

                    track: root.appearance.defaultTrack
                    onTrackEntered: (track) => { root.appearance.defaultTrack = track }
                }
            }

            FieldRow {
                Layout.topMargin: VLCStyle.spacing_5
                Layout.bottomMargin: VLCStyle.spacing_10

                text: qsTr("Subtitle Language")
                presetKeys: ["sub-language"]

                FieldCombo {
                    id: languageCombo

                    Accessible.name: qsTr("Default subtitle language")

                    model: root.appearance.languages
                    textRole: "name"

                    Binding on currentIndex {
                        value: root.appearance.languages.findIndex(
                                   (l) => l.code === root.appearance.defaultLanguage)
                    }

                    onActivated: (index) => {
                        root.appearance.defaultLanguage = root.appearance.languages[index].code
                    }
                }
            }

            // Synchronization (Figma "Frame 80", 767:2196)

            SectionHeader {
                text: qsTr("Synchronization")
            }

            FieldRow {
                Layout.topMargin: VLCStyle.spacing_10
                Layout.preferredHeight: VLCStyle.dp(56, VLCStyle.scale)

                text: qsTr("Subtitle Track Delay")
                presetKeys: ["sub-delay"]

                ValueField {
                    id: delayField

                    Accessible.name: qsTr("Subtitle delay")

                    enabled: Player.isStarted

                    Binding on text {
                        when: !delayField.activeFocus
                        value: root._formatSeconds(Player.subtitleDelayMS)
                        restoreMode: Binding.RestoreNone
                    }

                    onEditingFinished: {
                        const ms = root._parseMilliseconds(text)
                        if (!isNaN(ms))
                            Player.subtitleDelayMS = Math.round(ms)
                        text = root._formatSeconds(Player.subtitleDelayMS)
                    }
                }
            }

            // Figma 1905:7365: the ends and the middle 10 px under the track
            RowSlider {
                id: delaySlider

                enabled: Player.isStarted

                from: -10
                to: 10
                stepSize: 0.05
                value: Math.min(Math.max(Player.subtitleDelayMS / 1000, from), to)
                accessibleName: qsTr("Subtitle delay")
                accessibleValue: root._formatSeconds(Player.subtitleDelayMS)
                edgeLabels: ["-10 s", "0 s", "+10 s"]
                labelGap: VLCStyle.spacing_6

                onMoved: (value) => { Player.subtitleDelayMS = Math.round(value * 1000) }
            }

            // not in Figma: "Voice heard" then "Text seen" (or the reverse) sets
            // the delay
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10
                Layout.bottomMargin: VLCStyle.spacing_10

                spacing: VLCStyle.spacing_10

                Widgets.ButtonExt {
                    id: voiceHeardButton

                    Layout.fillWidth: true

                    enabled: Player.isStarted
                    checked: delayEstimator.isHeardTimeMarked
                    text: qsTr("Voice Heard")

                    onClicked: delayEstimator.markHeardTime()

                    Navigation.parentItem: root
                    Navigation.upAction: () => root._move(voiceHeardButton, -1)
                    Navigation.rightItem: textSeenButton
                    Navigation.downAction: () => root._move(voiceHeardButton, 1)
                }

                Widgets.ButtonExt {
                    id: textSeenButton

                    Layout.fillWidth: true

                    enabled: Player.isStarted
                    checked: delayEstimator.isSpottedTimeMarked
                    text: qsTr("Text Seen")

                    onClicked: delayEstimator.markSpottedTime()

                    Navigation.parentItem: root
                    Navigation.upAction: () => root._move(voiceHeardButton, -1)
                    Navigation.leftItem: voiceHeardButton
                    Navigation.downAction: () => root._move(textSeenButton, 1)
                }
            }

            HelpText {
                text: delayEstimator.isHeardTimeMarked
                      ? qsTr("Now press Text Seen when the line shows.")
                      : delayEstimator.isSpottedTimeMarked
                        ? qsTr("Now press Voice Heard when the line is spoken.")
                        : qsTr("Press Voice Heard when you hear a line and Text Seen when it shows, in any order.")
            }

            // the second subtitle (Track menu, "Show as second subtitle")
            FieldRow {
                visible: Player.hasSecondarySubtitle

                Layout.preferredHeight: VLCStyle.dp(56, VLCStyle.scale)

                text: qsTr("Second Subtitle Delay")

                ValueField {
                    id: secondaryDelayField

                    Accessible.name: qsTr("Second subtitle delay")

                    Binding on text {
                        when: !secondaryDelayField.activeFocus
                        value: root._formatSeconds(Player.secondarySubtitleDelayMS)
                        restoreMode: Binding.RestoreNone
                    }

                    onEditingFinished: {
                        const ms = root._parseMilliseconds(text)
                        if (!isNaN(ms))
                            Player.secondarySubtitleDelayMS = Math.round(ms)
                        text = root._formatSeconds(Player.secondarySubtitleDelayMS)
                    }
                }
            }

            RowSlider {
                id: secondaryDelaySlider

                visible: Player.hasSecondarySubtitle

                from: -10
                to: 10
                stepSize: 0.05
                value: Math.min(Math.max(Player.secondarySubtitleDelayMS / 1000, from), to)
                accessibleName: qsTr("Second subtitle delay")
                accessibleValue: root._formatSeconds(Player.secondarySubtitleDelayMS)
                edgeLabels: ["-10 s", "0 s", "+10 s"]
                labelGap: VLCStyle.spacing_6

                onMoved: (value) => { Player.secondarySubtitleDelayMS = Math.round(value * 1000) }
            }

            FieldRow {
                Layout.topMargin: VLCStyle.spacing_5
                Layout.preferredHeight: VLCStyle.dp(56, VLCStyle.scale)

                text: qsTr("Subtitle Speed")

                ValueField {
                    id: speedField

                    Accessible.name: qsTr("Subtitle speed")

                    enabled: Player.isStarted

                    Binding on text {
                        when: !speedField.activeFocus
                        value: Player.subtitleFPS.toFixed(3) + " fps"
                        restoreMode: Binding.RestoreNone
                    }

                    onEditingFinished: {
                        const fps = root._parseNumber(text)
                        if (!isNaN(fps))
                            Player.subtitleFPS = Math.min(Math.max(fps, 1), 100)
                        text = Player.subtitleFPS.toFixed(3) + " fps"
                    }
                }
            }

            // Figma 804:1174: no labels, no ticks
            RowSlider {
                id: speedSlider

                enabled: Player.isStarted

                from: 1
                to: 60
                stepSize: 0.5
                intervals: 0
                value: Math.min(Math.max(Player.subtitleFPS, from), to)
                accessibleName: qsTr("Subtitle speed")
                accessibleValue: Player.subtitleFPS.toFixed(3) + " fps"

                onMoved: (value) => { Player.subtitleFPS = value }
            }

            FieldRow {
                Layout.topMargin: VLCStyle.spacing_5
                Layout.preferredHeight: VLCStyle.dp(56, VLCStyle.scale)

                text: qsTr("Subtitle Duration Factor")

                ValueField {
                    id: durationField

                    Accessible.name: qsTr("Subtitle duration factor")
                    Accessible.description: qsTr("Multiplies how long each line stays on screen. 0 keeps the subtitles' own durations.")

                    Binding on text {
                        when: !durationField.activeFocus
                        value: Player.subtitleDurationFactor.toFixed(3)
                        restoreMode: Binding.RestoreNone
                    }

                    onEditingFinished: {
                        const factor = root._parseNumber(text)
                        if (!isNaN(factor))
                            Player.subtitleDurationFactor = factor
                        text = Player.subtitleDurationFactor.toFixed(3)
                    }
                }
            }

            RowSlider {
                id: durationSlider

                Layout.bottomMargin: VLCStyle.spacing_20

                from: 0
                to: 5
                stepSize: 0.1
                intervals: 0
                value: Math.min(Player.subtitleDurationFactor, to)
                accessibleName: qsTr("Subtitle duration factor")
                accessibleValue: Player.subtitleDurationFactor.toFixed(3)

                onMoved: (value) => { Player.subtitleDurationFactor = value }
            }

            // Appearance (Figma "Frame 91", 800:1126)

            SectionHeader {
                text: qsTr("Appearance")
            }

            // styled ASS subtitles keep their look
            HelpText {
                visible: root.styled
                Layout.topMargin: VLCStyle.spacing_10
                Layout.bottomMargin: 0
                text: qsTr("This subtitle uses its own styling.")
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.spacing_10
                Layout.bottomMargin: VLCStyle.spacing_10

                spacing: VLCStyle.spacing_5

                enabled: !root.styled
                opacity: enabled ? 1 : 0.5

                // the bold, italic and colors set by the subtitles
                ToggleRow {
                    id: keepStylingRow

                    text: qsTr("Embedded Sub Format")
                    presetKeys: ["sub-keep-styling"]
                    description: qsTr("Bold, italic and colors set by the subtitles")
                    checked: root.appearance.keepFileStyling
                    onToggled: root.appearance.keepFileStyling = !root.appearance.keepFileStyling
                }

                FieldRow {
                    text: qsTr("Subtitle Font")
                    presetKeys: ["sub-font"]

                    FieldCombo {
                        id: fontCombo

                        Accessible.name: qsTr("Subtitle font")

                        model: root._fonts

                        Binding on currentIndex {
                            value: root.appearance.font === "" ? 0
                                                               : root._fonts.indexOf(root.appearance.font)
                        }
                        displayText: root.appearance.font === "" ? root._fonts[0] : root.appearance.font

                        onActivated: (index) => {
                            root.appearance.font = index === 0 ? "" : root._fonts[index]
                        }
                    }
                }

                ColorRow {
                    id: textColorRow

                    text: qsTr("Text Color")
                    presetKeys: ["sub-text-color"]
                    color: root.appearance.textColor
                    onPicked: (color) => { root.appearance.textColor = color }
                }

                ColorRow {
                    id: outlineColorRow

                    text: qsTr("Outline Color")
                    presetKeys: ["sub-outline-color"]
                    color: root.appearance.outlineColor
                    onPicked: (color) => { root.appearance.outlineColor = color }
                }

                FieldRow {
                    text: qsTr("Subtitle Offset")
                    presetKeys: ["sub-vertical-offset"]

                    ValueField {
                        id: offsetField

                        Accessible.name: qsTr("Subtitle vertical offset in pixels")

                        Binding on text {
                            when: !offsetField.activeFocus
                            //: a subtitle offset in pixels, "0px"
                            value: qsTr("%1px").arg(root.appearance.verticalOffset)
                            restoreMode: Binding.RestoreNone
                        }

                        onEditingFinished: {
                            const pixels = root._parseNumber(text)
                            if (!isNaN(pixels))
                                root.appearance.verticalOffset = Math.round(pixels)
                            text = qsTr("%1px").arg(root.appearance.verticalOffset)
                        }
                    }
                }

                // Figma "Frame 61" (800:1136): relative sizes or pixels
                FieldRow {
                    Layout.preferredHeight: VLCStyle.dp(50, VLCStyle.scale)

                    text: qsTr("Size Values")
                    presetKeys: ["sub-size-mode"]

                    SettingsSegmented {
                        id: sizeModeSegments

                        Layout.preferredWidth: VLCStyle.dp(180, VLCStyle.scale)
                        implicitHeight: VLCStyle.dp(38, VLCStyle.scale)

                        accessibleName: qsTr("Subtitle size")
                        choices: [
                            { value: SubtitleAppearance.Relative, label: qsTr("Relative") },
                            { value: SubtitleAppearance.Pixels, label: qsTr("Pixels") },
                        ]
                        value: root.appearance.sizeMode

                        onActivated: (value) => { root.appearance.sizeMode = value }

                        Navigation.parentItem: root
                        Navigation.upAction: () => root._move(sizeModeSegments, -1)
                        Navigation.downAction: () => root._move(sizeModeSegments, 1)
                    }
                }

                // Figma "Frame 96" (804:1341): the relative size on a range
                ValueRange {
                    visible: root.appearance.sizeMode === SubtitleAppearance.Relative

                    text: qsTr("Font Size")
                    presetKeys: ["sub-relative-size"]
                    valueText: root._relativeSizes[root.appearance.relativeSize]?.text ?? ""

                    RowSlider {
                        id: relativeSizeSlider

                        Layout.leftMargin: 0
                        Layout.rightMargin: 0
                        Layout.topMargin: 0
                        Layout.bottomMargin: 0

                        from: 0
                        to: root._relativeSizes.length - 1
                        stepSize: 1
                        value: root.appearance.relativeSize
                        accessibleName: qsTr("Relative subtitle size")
                        accessibleValue: root._relativeSizes[root.appearance.relativeSize]?.text ?? ""

                        onMoved: (value) => {
                            root.appearance.relativeSize = root._relativeSizes[Math.round(value)].value
                        }
                    }
                }

                FieldRow {
                    visible: root.appearance.sizeMode === SubtitleAppearance.Pixels

                    text: qsTr("Font Size")
                    presetKeys: ["sub-pixel-size"]

                    ValueField {
                        id: pixelSizeField

                        Accessible.name: qsTr("Subtitle size in pixels")

                        Binding on text {
                            when: !pixelSizeField.activeFocus
                            value: root.appearance.pixelSize + " px"
                            restoreMode: Binding.RestoreNone
                        }

                        onEditingFinished: {
                            const pixels = Math.round(root._parseNumber(text))
                            if (pixels > 0)
                                root.appearance.pixelSize = pixels
                            text = root.appearance.pixelSize + " px"
                        }
                    }
                }

                // Figma "Frame 97" (804:1342): None, Thin, Normal and Thick
                ValueRange {
                    text: qsTr("Outline Thickness")
                    presetKeys: ["sub-outline-thickness"]
                    valueText: root._outlines[root.appearance.outlineThickness]?.text ?? ""

                    RowSlider {
                        id: outlineSlider

                        Layout.leftMargin: 0
                        Layout.rightMargin: 0
                        Layout.topMargin: 0
                        Layout.bottomMargin: 0

                        from: 0
                        to: root._outlines.length - 1
                        stepSize: 1
                        value: root.appearance.outlineThickness
                        accessibleName: qsTr("Outline thickness")
                        accessibleValue: root._outlines[root.appearance.outlineThickness]?.text ?? ""

                        onMoved: (value) => {
                            root.appearance.outlineThickness = root._outlines[Math.round(value)].value
                        }
                    }
                }

                // Figma "Frame 98" (848:1686): ten ticks, the value above
                ValueRange {
                    text: qsTr("Background Opacity")
                    presetKeys: ["sub-background-opacity"]
                    //: a background opacity, "0 %"
                    valueText: qsTr("%1 %").arg(root.appearance.backgroundOpacity)

                    RowSlider {
                        id: backgroundSlider

                        Layout.leftMargin: 0
                        Layout.rightMargin: 0
                        Layout.topMargin: 0
                        Layout.bottomMargin: 0

                        from: 0
                        to: 100
                        stepSize: 5
                        intervals: 10
                        value: root.appearance.backgroundOpacity
                        accessibleName: qsTr("Background opacity")
                        accessibleValue: qsTr("%1 %").arg(root.appearance.backgroundOpacity)

                        onMoved: (value) => { root.appearance.backgroundOpacity = Math.round(value) }
                    }
                }
            }
        }
    }

    // Figma "Frame 101" (845:1677): a section title, 16 Medium FG/A1, 24 px
    // in, 20 px under the line above it; the line takes no room (845:1375)
    component SectionHeader : ColumnLayout {
        property alias text: sectionLabel.text
        property bool divider: true

        Layout.fillWidth: true

        spacing: 0

        Item {
            Layout.fillWidth: true
            implicitHeight: 0

            Rectangle {
                visible: parent.parent.divider
                width: parent.width
                height: VLCStyle.border
                color: theme.separator
            }
        }

        T.Label {
            id: sectionLabel

            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.dp(24, VLCStyle.scale)
            Layout.rightMargin: VLCStyle.dp(24, VLCStyle.scale)
            Layout.topMargin: VLCStyle.spacing_20
            // Figma's 19 px line (845:1678); Qt's is a pixel taller
            Layout.preferredHeight: VLCStyle.dp(19, VLCStyle.scale)

            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_xlarge
            font.weight: Font.Medium
            font.family: VLCStyle.fontFamily
            elide: Text.ElideRight

            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
    }

    component AdjustmentsScrollBar : Widgets.ScrollBarExt {
        opacity: (active || interacting) ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: VLCStyle.duration_short }
        }
    }

    // A label, left of its control: Figma 14 Regular FG/A3, 4 px in
    component RowLabel : T.Label {
        leftPadding: VLCStyle.spacing_4
        color: theme.fg.label
        font.pixelSize: VLCStyle.fontSize_normal
        font.family: VLCStyle.fontFamily
        elide: Text.ElideRight
    }

    // a line of explanation (not in Figma)
    component HelpText : T.Label {
        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20 + VLCStyle.spacing_4
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.bottomMargin: VLCStyle.spacing_10

        color: theme.fg.label
        font.pixelSize: VLCStyle.fontSize_small
        font.family: VLCStyle.fontFamily
        wrapMode: Text.WordWrap
    }

    // Figma "Frame 60" (800:1127): a label and its 140x36 control in a 48 px
    // row
    component FieldRow : RowLayout {
        property alias text: fieldLabel.text
        // the Preset keys of the row: a dot shows that the Preset sets it
        property alias presetKeys: fieldMark.keys

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.preferredHeight: VLCStyle.dp(48, VLCStyle.scale)

        spacing: VLCStyle.spacing_10

        RowLabel {
            id: fieldLabel
            Layout.fillWidth: true
            verticalAlignment: Text.AlignVCenter

            PresetMark {
                id: fieldMark
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Figma "Frame 96" (804:1341): the name and the value over a range,
    // 10 px around, the range 5 px under the 37 px row
    component ValueRange : ColumnLayout {
        property alias text: rangeLabel.text
        property alias presetKeys: rangeMark.keys
        property alias valueText: rangeValue.text

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.topMargin: VLCStyle.spacing_10
        Layout.bottomMargin: VLCStyle.spacing_10

        spacing: VLCStyle.spacing_5

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: VLCStyle.dp(37, VLCStyle.scale)

            RowLabel {
                id: rangeLabel
                Layout.fillWidth: true

                PresetMark {
                    id: rangeMark
                    anchors.right: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            RowLabel {
                id: rangeValue
                rightPadding: VLCStyle.spacing_4
            }
        }
    }

    // Figma "text_input": a 140x36 field of the sidebar
    component ValueField : Widgets.TextFieldExt {
        id: valueField

        Layout.preferredWidth: VLCStyle.dp(140, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
        radius: VLCStyle.radius_normal

        Keys.onEscapePressed: (event) => {
            valueField.undo()
            root.forceActiveFocus(Qt.TabFocusReason)
            event.accepted = true
        }

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(valueField, -1)
        Navigation.downAction: () => root._move(valueField, 1)
    }

    // Figma "text_input" (4046:12353): a track number from 1, empty to let
    // the media choose
    component TrackField : ValueField {
        id: trackInput

        property int track: 0

        signal trackEntered(int track)

        //: the default track: the media chooses
        placeholderText: qsTr("Auto")
        validator: IntValidator { bottom: 0; top: 99 }

        Binding on text {
            when: !trackInput.activeFocus
            value: trackInput.track > 0 ? String(trackInput.track) : ""
            restoreMode: Binding.RestoreNone
        }

        onEditingFinished: {
            const track = parseInt(text, 10)
            trackInput.trackEntered(isNaN(track) ? 0 : track)
            text = trackInput.track > 0 ? String(trackInput.track) : ""
        }
    }

    // a range under its field row, 5 px under it
    component RowSlider : AdjustmentsRange {
        id: rowSlider

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.topMargin: VLCStyle.spacing_5

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(rowSlider, -1)
        Navigation.downAction: () => root._move(rowSlider, 1)
    }

    // Figma "Frame 69" (2255:9846): a label and an ON / OFF toggle, 10 px
    // above and 6 px under the toggle
    component ToggleRow : FocusScope {
        id: toggleRow

        property alias text: toggleLabel.text
        // read by screen readers, not shown (Figma has none)
        property string description
        property alias checked: toggle.checked
        property alias presetKeys: toggleMark.keys

        signal toggled()

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20

        implicitHeight: toggle.implicitHeight + VLCStyle.spacing_10 + VLCStyle.spacing_6

        RowLabel {
            id: toggleLabel

            anchors.left: parent.left
            anchors.right: toggle.left
            anchors.rightMargin: VLCStyle.spacing_10
            anchors.verticalCenter: toggle.verticalCenter

            PresetMark {
                id: toggleMark
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        SettingsToggle {
            id: toggle

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: VLCStyle.spacing_10

            focus: true

            Accessible.name: toggleLabel.text
            Accessible.description: toggleRow.description

            onClicked: toggleRow.toggled()

            Navigation.parentItem: toggleRow
        }

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(toggleRow, -1)
        Navigation.downAction: () => root._move(toggleRow, 1)
    }

    // Figma "text_input" (4046:12356, 4046:12371): a choice drawn as the
    // 140x36 fields of the tab, its 2 px BG/A4 stroke round a BG/B2 fill; the
    // chevron says that it opens a list
    component FieldCombo : Widgets.ComboBoxExt {
        id: combo

        Layout.preferredWidth: VLCStyle.dp(140, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)

        leftPadding: frame + VLCStyle.spacing_12
        rightPadding: frame + VLCStyle.spacing_4

        bgColor: fieldTheme.border
        fieldColor: hovered ? colorContext.bg.highlight : fieldTheme.bg.primary

        // the list is as wide as the other dropdowns, right-aligned; long
        // lists (languages, fonts) scroll
        Binding {
            target: combo.popup
            property: "width"
            value: VLCStyle.dp(220, VLCStyle.scale)
        }
        Binding {
            target: combo.popup
            property: "x"
            value: combo.width - VLCStyle.dp(220, VLCStyle.scale)
        }
        Binding {
            target: combo.popup
            property: "height"
            value: Math.min(combo.popup.contentItem.implicitHeight + 2,
                            VLCStyle.dp(320, VLCStyle.scale))
        }

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(combo, -1)
        Navigation.downAction: () => root._move(combo, 1)
    }

    // Figma "Text Color" (800:1141): a label, the hex code and a 22 px swatch
    // opening the palette, in a 140x36 field of a 48 px row
    component ColorRow : FocusScope {
        id: colorRow

        property alias text: colorLabel.text
        property alias presetKeys: colorMark.keys
        property color color

        signal picked(color color)

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20

        implicitHeight: VLCStyle.dp(48, VLCStyle.scale)

        RowLabel {
            id: colorLabel

            anchors.left: parent.left
            anchors.right: hexField.left
            anchors.rightMargin: VLCStyle.spacing_10
            anchors.verticalCenter: parent.verticalCenter

            PresetMark {
                id: colorMark
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Widgets.TextFieldExt {
            id: hexField

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: VLCStyle.dp(140, VLCStyle.scale)
            height: VLCStyle.dp(36, VLCStyle.scale)
            radius: VLCStyle.radius_normal
            rightPadding: VLCStyle.dp(22, VLCStyle.scale) + VLCStyle.spacing_10 // the swatch

            focus: true

            Accessible.name: colorLabel.text

            Binding on text {
                when: !hexField.activeFocus
                value: root._colorText(colorRow.color)
                restoreMode: Binding.RestoreNone
            }

            onEditingFinished: {
                const color = root._parseColor(text)
                if (color !== undefined)
                    colorRow.picked(color)
                text = root._colorText(colorRow.color)
            }

            Keys.onEscapePressed: (event) => {
                hexField.text = root._colorText(colorRow.color)
                root.forceActiveFocus(Qt.TabFocusReason)
                event.accepted = true
            }

            Navigation.parentItem: colorRow
            Navigation.rightItem: swatch
        }

        T.AbstractButton {
            id: swatch

            // NOTE: The swatch is 22 dp, the target 24 dp (WCAG 2.5.8): the
            //       swatch's right edge stays 7 dp from the field's (Figma).
            anchors.right: hexField.right
            anchors.rightMargin: VLCStyle.dp(7, VLCStyle.scale) - (width - VLCStyle.dp(22, VLCStyle.scale)) / 2
            anchors.verticalCenter: parent.verticalCenter
            width: VLCStyle.icon_normal
            height: width

            focusPolicy: Qt.StrongFocus

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Pick the %1").arg(colorLabel.text.toLowerCase())

            onClicked: palettePopup.open()

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

            background: Item {
                // Figma 4046:12377: r2, a 1.5 px white stroke at 21 %
                Rectangle {
                    anchors.centerIn: parent
                    width: VLCStyle.dp(22, VLCStyle.scale)
                    height: width

                    radius: VLCStyle.radius_xxsmall
                    color: colorRow.color
                    border.width: swatch.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 1.5 * VLCStyle.scale
                    border.color: swatch.visualFocus ? theme.accent : Qt.rgba(1, 1, 1, 0.21)
                }
            }

            Navigation.parentItem: colorRow
            Navigation.leftItem: hexField
        }

        T.Popup {
            id: palettePopup

            x: hexField.x + hexField.width - width
            y: hexField.y + hexField.height + VLCStyle.spacing_4

            padding: VLCStyle.spacing_6
            modal: false
            focus: true
            closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

            onOpened: paletteGrid.children[0]?.forceActiveFocus(Qt.TabFocusReason)
            onClosed: swatch.forceActiveFocus(Qt.TabFocusReason)

            background: Rectangle {
                radius: VLCStyle.radius_normal
                color: theme.bg.secondary
                border.width: VLCStyle.border
                border.color: theme.border
            }

            contentItem: Grid {
                id: paletteGrid

                columns: 8
                spacing: VLCStyle.spacing_4

                Repeater {
                    model: root._palette

                    T.AbstractButton {
                        id: paletteSwatch

                        required property string modelData
                        required property int index

                        width: VLCStyle.dp(22, VLCStyle.scale)
                        height: width

                        focusPolicy: Qt.StrongFocus

                        Accessible.role: Accessible.Button
                        Accessible.name: modelData

                        onClicked: {
                            colorRow.picked(Qt.color(modelData))
                            palettePopup.close()
                        }

                        Keys.onLeftPressed: paletteGrid.children[Math.max(index - 1, 0)].forceActiveFocus(Qt.TabFocusReason)
                        Keys.onRightPressed: paletteGrid.children[Math.min(index + 1, root._palette.length - 1)].forceActiveFocus(Qt.TabFocusReason)
                        Keys.onUpPressed: paletteGrid.children[Math.max(index - paletteGrid.columns, 0)].forceActiveFocus(Qt.TabFocusReason)
                        Keys.onDownPressed: paletteGrid.children[Math.min(index + paletteGrid.columns, root._palette.length - 1)].forceActiveFocus(Qt.TabFocusReason)

                        background: Rectangle {
                            radius: VLCStyle.radius_xxsmall
                            color: paletteSwatch.modelData
                            border.width: paletteSwatch.visualFocus || paletteSwatch.hovered
                                          ? VLCStyle.dp(2, VLCStyle.scale) : VLCStyle.border
                            border.color: paletteSwatch.visualFocus || paletteSwatch.hovered
                                          ? theme.accent : theme.border
                        }
                    }
                }
            }
        }

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(colorRow, -1)
        Navigation.downAction: () => root._move(colorRow, 1)
    }
}
