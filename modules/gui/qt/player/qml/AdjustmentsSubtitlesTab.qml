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

    readonly property SubtitleAppearance appearance: Player.subtitleAppearance

    // styled ASS subtitles draw themselves: their look can't be changed
    readonly property bool styled: Player.subtitleTracks.styledSelected

    property bool _advancedOpen: false

    // the controls, top to bottom, for the keyboard navigation
    readonly property var _controls: [
        languageCombo,
        delayField, delaySlider, voiceHeardButton, textSeenButton,
        secondaryDelayField, secondaryDelaySlider,
        advancedButton,
        speedField, speedSlider, durationField, durationSlider,
        keepStylingRow, fontCombo, sizeModeSegments, relativeSizeCombo, pixelSizeField,
        textColorRow, outlineColorRow, outlineSegments, backgroundSlider, offsetField,
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

        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

        ColumnLayout {
            id: content

            width: flickable.width

            spacing: 0

            // Defaults

            SectionHeader {
                text: qsTr("Defaults")
                divider: false
            }

            ChoiceRow {
                text: qsTr("Subtitle Language")
                presetKeys: ["sub-language"]

                ChoiceCombo {
                    id: languageCombo

                    focus: true

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

            // Synchronization

            SectionHeader {
                text: qsTr("Synchronization")
            }

            FieldRow {
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

            RowSlider {
                id: delaySlider

                enabled: Player.isStarted

                from: -10
                to: 10
                stepSize: 0.05
                unit: "s"
                value: Math.min(Math.max(Player.subtitleDelayMS / 1000, from), to)
                accessibleName: qsTr("Subtitle delay")
                tickLabels: ["-10 s", "0 s", "+10 s"]

                onMoved: (value) => { Player.subtitleDelayMS = Math.round(value * 1000) }
            }

            // "Voice heard" then "Text seen" (or the reverse) sets the delay
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
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
                unit: "s"
                value: Math.min(Math.max(Player.secondarySubtitleDelayMS / 1000, from), to)
                accessibleName: qsTr("Second subtitle delay")
                tickLabels: ["-10 s", "0 s", "+10 s"]

                onMoved: (value) => { Player.secondarySubtitleDelayMS = Math.round(value * 1000) }
            }

            // Advanced: subtitle speed and duration factor
            Widgets.ButtonExt {
                id: advancedButton

                Layout.leftMargin: VLCStyle.spacing_10
                Layout.bottomMargin: VLCStyle.spacing_10

                text: qsTr("Advanced")
                iconTxt: root._advancedOpen ? VLCIcons.expand_inverted : VLCIcons.expand

                Accessible.role: Accessible.Button
                Accessible.description: root._advancedOpen ? qsTr("Expanded") : qsTr("Collapsed")

                onClicked: root._advancedOpen = !root._advancedOpen

                Navigation.parentItem: root
                Navigation.upAction: () => root._move(advancedButton, -1)
                Navigation.downAction: () => root._move(advancedButton, 1)
            }

            ColumnLayout {
                visible: root._advancedOpen

                Layout.fillWidth: true

                spacing: 0

                FieldRow {
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

                RowSlider {
                    id: speedSlider

                    enabled: Player.isStarted

                    from: 1
                    to: 60
                    stepSize: 0.5
                    unit: "fps"
                    value: Math.min(Math.max(Player.subtitleFPS, from), to)
                    accessibleName: qsTr("Subtitle speed")

                    onMoved: (value) => { Player.subtitleFPS = value }
                }

                FieldRow {
                    text: qsTr("Subtitle Duration Factor")

                    ValueField {
                        id: durationField

                        Accessible.name: qsTr("Subtitle duration factor")

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

                    from: 0
                    to: 5
                    stepSize: 0.1
                    unit: "×"
                    value: Math.min(Player.subtitleDurationFactor, to)
                    accessibleName: qsTr("Subtitle duration factor")

                    onMoved: (value) => { Player.subtitleDurationFactor = value }
                }

                HelpText {
                    text: qsTr("Multiplies how long each line stays on screen. 0 keeps the subtitles' own durations.")
                }
            }

            // Appearance

            SectionHeader {
                text: qsTr("Appearance")
            }

            // styled ASS subtitles keep their look
            HelpText {
                visible: root.styled
                text: qsTr("This subtitle uses its own styling.")
            }

            ColumnLayout {
                Layout.fillWidth: true

                spacing: 0

                enabled: !root.styled
                opacity: enabled ? 1 : 0.5

                ToggleRow {
                    id: keepStylingRow

                    text: qsTr("Keep the File's Styling")
                    presetKeys: ["sub-keep-styling"]
                    description: qsTr("Bold, italic and colors set by the subtitles")
                    checked: root.appearance.keepFileStyling
                    onToggled: root.appearance.keepFileStyling = !root.appearance.keepFileStyling
                }

                ChoiceRow {
                    text: qsTr("Font")
                    presetKeys: ["sub-font"]

                    ChoiceCombo {
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

                FieldRow {
                    text: qsTr("Size")
                    presetKeys: ["sub-size-mode"]

                    SettingsSegmented {
                        id: sizeModeSegments

                        Layout.preferredWidth: VLCStyle.dp(220, VLCStyle.scale)

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

                ChoiceRow {
                    visible: root.appearance.sizeMode === SubtitleAppearance.Relative

                    text: qsTr("Relative Size")
                    presetKeys: ["sub-relative-size"]

                    ChoiceCombo {
                        id: relativeSizeCombo

                        Accessible.name: qsTr("Relative subtitle size")

                        model: root._relativeSizes
                        textRole: "text"

                        Binding on currentIndex {
                            value: root.appearance.relativeSize
                        }

                        onActivated: (index) => {
                            root.appearance.relativeSize = root._relativeSizes[index].value
                        }
                    }
                }

                FieldRow {
                    visible: root.appearance.sizeMode === SubtitleAppearance.Pixels

                    text: qsTr("Size in Pixels")
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
                    text: qsTr("Outline")
                    presetKeys: ["sub-outline-thickness"]

                    SettingsSegmented {
                        id: outlineSegments

                        Layout.preferredWidth: VLCStyle.dp(220, VLCStyle.scale)

                        accessibleName: qsTr("Outline thickness")
                        choices: [
                            { value: SubtitleAppearance.NoOutline, label: qsTr("None") },
                            { value: SubtitleAppearance.ThinOutline, label: qsTr("Thin") },
                            { value: SubtitleAppearance.NormalOutline, label: qsTr("Normal") },
                            { value: SubtitleAppearance.ThickOutline, label: qsTr("Thick") },
                        ]
                        value: root.appearance.outlineThickness

                        onActivated: (value) => { root.appearance.outlineThickness = value }

                        Navigation.parentItem: root
                        Navigation.upAction: () => root._move(outlineSegments, -1)
                        Navigation.downAction: () => root._move(outlineSegments, 1)
                    }
                }

                FieldRow {
                    text: qsTr("Background Opacity")
                    presetKeys: ["sub-background-opacity"]
                }

                RowSlider {
                    id: backgroundSlider

                    from: 0
                    to: 100
                    stepSize: 5
                    unit: "%"
                    value: root.appearance.backgroundOpacity
                    accessibleName: qsTr("Background opacity")

                    onMoved: (value) => { root.appearance.backgroundOpacity = Math.round(value) }
                }

                FieldRow {
                    Layout.bottomMargin: VLCStyle.spacing_20

                    text: qsTr("Vertical Offset")
                    presetKeys: ["sub-vertical-offset"]

                    ValueField {
                        id: offsetField

                        Accessible.name: qsTr("Subtitle vertical offset in pixels")

                        Binding on text {
                            when: !offsetField.activeFocus
                            value: root.appearance.verticalOffset + " px"
                            restoreMode: Binding.RestoreNone
                        }

                        onEditingFinished: {
                            const pixels = root._parseNumber(text)
                            if (!isNaN(pixels))
                                root.appearance.verticalOffset = Math.round(pixels)
                            text = root.appearance.verticalOffset + " px"
                        }
                    }
                }
            }
        }
    }

    // Figma "Defaults": a section title, under a divider
    component SectionHeader : ColumnLayout {
        property alias text: sectionLabel.text
        property bool divider: true

        Layout.fillWidth: true

        spacing: 0

        Rectangle {
            visible: parent.divider
            Layout.fillWidth: true
            implicitHeight: VLCStyle.border
            color: theme.border
        }

        T.Label {
            id: sectionLabel

            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.spacing_20
            Layout.rightMargin: VLCStyle.spacing_20
            Layout.topMargin: VLCStyle.spacing_14
            Layout.bottomMargin: VLCStyle.spacing_10

            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_large
            font.weight: Font.DemiBold
            font.family: VLCStyle.fontFamily
            elide: Text.ElideRight

            Accessible.role: Accessible.Heading
            Accessible.name: text
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

    // a line of explanation
    component HelpText : T.Label {
        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20 + VLCStyle.spacing_4
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.bottomMargin: VLCStyle.spacing_10

        color: theme.fg.secondary
        font.pixelSize: VLCStyle.fontSize_small
        font.family: VLCStyle.fontFamily
        wrapMode: Text.WordWrap
    }

    // a label and a control at the right
    component FieldRow : RowLayout {
        property alias text: fieldLabel.text
        // the Preset keys of the row: a dot shows that the Preset sets it
        property alias presetKeys: fieldMark.keys

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.bottomMargin: VLCStyle.spacing_10

        spacing: VLCStyle.spacing_10

        RowLabel {
            id: fieldLabel
            Layout.fillWidth: true
            Layout.minimumHeight: VLCStyle.dp(32, VLCStyle.scale)
            verticalAlignment: Text.AlignVCenter

            PresetMark {
                id: fieldMark
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Figma: a 140 px field of the sidebar
    component ValueField : Widgets.TextFieldExt {
        id: valueField

        Layout.preferredWidth: VLCStyle.dp(140, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
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

    // a slider under its field row
    component RowSlider : SettingsSlider {
        id: rowSlider

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.bottomMargin: VLCStyle.spacing_14

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(rowSlider, -1)
        Navigation.downAction: () => root._move(rowSlider, 1)
    }

    // Figma "Frame 69": a label and an ON / OFF toggle
    component ToggleRow : FocusScope {
        id: toggleRow

        property alias text: toggleLabel.text
        property alias description: toggleDescription.text
        property alias checked: toggle.checked
        property alias presetKeys: toggleMark.keys

        signal toggled()

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.bottomMargin: VLCStyle.spacing_4

        implicitHeight: Math.max(labels.implicitHeight, toggle.implicitHeight) + VLCStyle.spacing_10

        ColumnLayout {
            id: labels

            anchors.left: parent.left
            anchors.right: toggle.left
            anchors.rightMargin: VLCStyle.spacing_10
            anchors.verticalCenter: parent.verticalCenter

            spacing: 0

            RowLabel {
                id: toggleLabel
                Layout.fillWidth: true

                PresetMark {
                    id: toggleMark
                    anchors.right: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            RowLabel {
                id: toggleDescription
                Layout.fillWidth: true
                visible: text !== ""
                font.pixelSize: VLCStyle.fontSize_small
                wrapMode: Text.WordWrap
            }
        }

        SettingsToggle {
            id: toggle

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            focus: true

            Accessible.name: toggleLabel.text
            Accessible.description: toggleDescription.text

            onClicked: toggleRow.toggled()

            Navigation.parentItem: toggleRow
        }

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(toggleRow, -1)
        Navigation.downAction: () => root._move(toggleRow, 1)
    }

    // a label and a 220 px dropdown
    component ChoiceRow : FieldRow { }

    // Figma "EQ": the dropdown of the sidebar
    component ChoiceCombo : Widgets.ComboBoxExt {
        id: combo

        Layout.preferredWidth: VLCStyle.dp(220, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(40, VLCStyle.scale)

        font.pixelSize: VLCStyle.fontSize_normal
        leftPadding: VLCStyle.spacing_14 + VLCStyle.dp(2, VLCStyle.scale)
        rightPadding: VLCStyle.spacing_14

        color: theme.fg.secondary

        // long lists (languages, fonts) scroll
        Binding {
            target: combo.popup
            property: "height"
            value: Math.min(combo.popup.contentItem.implicitHeight + 2,
                            VLCStyle.dp(320, VLCStyle.scale))
        }

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

        Navigation.parentItem: root
        Navigation.upAction: () => root._move(combo, -1)
        Navigation.downAction: () => root._move(combo, 1)
    }

    // Figma "Text Color": a label, the hex code and a swatch opening the
    // palette
    component ColorRow : FocusScope {
        id: colorRow

        property alias text: colorLabel.text
        property alias presetKeys: colorMark.keys
        property color color

        signal picked(color color)

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.bottomMargin: VLCStyle.spacing_10

        implicitHeight: VLCStyle.dp(32, VLCStyle.scale)

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
            height: parent.height
            radius: VLCStyle.radius_normal
            rightPadding: swatch.width + VLCStyle.spacing_10

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

            anchors.right: hexField.right
            anchors.rightMargin: VLCStyle.spacing_6
            anchors.verticalCenter: parent.verticalCenter
            width: VLCStyle.dp(18, VLCStyle.scale)
            height: width

            focusPolicy: Qt.StrongFocus

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Pick the %1").arg(colorLabel.text.toLowerCase())

            onClicked: palettePopup.open()

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

            background: Rectangle {
                radius: VLCStyle.dp(2, VLCStyle.scale)
                color: colorRow.color
                border.width: swatch.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : VLCStyle.border
                border.color: swatch.visualFocus ? theme.accent : theme.fg.secondary
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
                            radius: VLCStyle.dp(2, VLCStyle.scale)
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
