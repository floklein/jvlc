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

// The Audio tab of the Adjustments sidebar (Figma 536:1059): default
// language, stereo mode, delay and filters. Every change applies at once;
// hovering an equalizer preset or a stereo mode tries it.
FocusScope {
    id: root

    readonly property AudioAdjustments audio: Player.audioAdjustments

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    readonly property ColorContext _fieldContext: ColorContext {
        id: fieldTheme
        colorSet: ColorContext.TextField
    }

    readonly property ColorContext _panelContext: ColorContext {
        id: panelTheme
        colorSet: ColorContext.Window
    }

    // the keyboard order of the controls, hidden ones are skipped
    readonly property var _focusChain: [
        languageCombo, stereoCombo,
        delayField, delaySlider, soundHeard, soundSpotted,
        equalizerCombo, equalizerBox, twoPassRow,
        compressorRow, compressorBox,
        spatializerRow, spatializerBox,
        widenerRow, widenerBox,
        pitchSlider
    ]

    function _neighbour(item : Item, step : int) : Item {
        const chain = root._focusChain
        for (let i = chain.indexOf(item) + step; i >= 0 && i < chain.length; i += step) {
            const next = chain[i]
            if (next && next !== item && next.visible && next.enabled)
                return next
        }
        return null
    }

    // "1.250 s"
    function _formatDelay(ms : int) : string {
        return qsTr("%1 s").arg((ms / 1000).toFixed(3))
    }

    // a number typed in a field, with any unit after it
    function _parseNumber(text : string) : real {
        return parseFloat(text.replace(",", "."))
    }

    function _formatValue(value : real, scale : real, unit : string) : string {
        const v = (value * scale).toFixed(1)
        return unit.length > 0 ? v + " " + unit : v
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
            }

            ChoiceRow {
                Layout.topMargin: VLCStyle.spacing_10

                text: qsTr("Audio Language")
                presetKeys: ["audio-language"]

                ChoiceCombo {
                    id: languageCombo

                    Layout.preferredWidth: VLCStyle.dp(160, VLCStyle.scale)

                    focus: true
                    Accessible.name: qsTr("Default audio language")

                    model: root.audio.languages
                    textRole: "name"

                    currentIndex: root.audio.languages.findIndex((l) => l.code === root.audio.language)

                    onActivated: (index) => root.audio.setLanguage(root.audio.languages[index].code)

                    Navigation.parentItem: root
                    Navigation.downItem: root._neighbour(languageCombo, 1)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10

                RowLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: VLCStyle.spacing_10
                    Layout.bottomMargin: VLCStyle.spacing_10
                    text: qsTr("Stereo Mode")

                    PresetMark {
                        anchors.right: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        keys: ["stereo-mode"]
                    }
                }

                // the output picks its own mode until one is chosen
                ResetButton {
                    group: "stereo"
                    label: qsTr("Stereo Mode")
                }
            }

            // tried on hover; the output ignores a mode its audio cannot use
            ChoiceCombo {
                id: stereoCombo

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.bottomMargin: VLCStyle.spacing_20

                Accessible.name: qsTr("Stereo Mode")

                model: root.audio.stereoModes
                textRole: "name"

                Binding on currentIndex {
                    when: !stereoCombo.popup.visible
                    value: root.audio.stereoModes.findIndex((m) => m.value === root.audio.stereoMode)
                }

                // after onActivated, which keeps the choice
                Connections {
                    target: stereoCombo.popup

                    function onAboutToHide() {
                        Qt.callLater(() => root.audio.endStereoPreview())
                    }
                }

                onHighlightedIndexChanged: {
                    if (popup.visible && highlightedIndex >= 0)
                        root.audio.previewStereoMode(root.audio.stereoModes[highlightedIndex].value)
                }

                onActivated: (index) => root.audio.setStereoMode(root.audio.stereoModes[index].value)

                Navigation.parentItem: root
                Navigation.upItem: root._neighbour(stereoCombo, -1)
                Navigation.downItem: root._neighbour(stereoCombo, 1)
            }

            Divider { }

            // Synchronization

            RowLayout {
                Layout.fillWidth: true
                Layout.rightMargin: VLCStyle.spacing_20

                SectionHeader {
                    Layout.fillWidth: true
                    text: qsTr("Synchronization")
                }

                ResetButton {
                    Layout.topMargin: VLCStyle.spacing_20
                    group: "delay"
                    label: qsTr("Synchronization")
                    visible: Player.audioDelayMS !== 0
                    onClicked: delayEstimator.reset()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10

                spacing: VLCStyle.spacing_10

                RowLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: VLCStyle.spacing_10
                    Layout.bottomMargin: VLCStyle.spacing_10
                    text: qsTr("Audio Track Delay")

                    PresetMark {
                        anchors.right: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        keys: ["audio-delay"]
                    }
                }

                ValueField {
                    id: delayField

                    Accessible.name: qsTr("Audio track delay")

                    enabled: Player.isStarted

                    shownText: root._formatDelay(Player.audioDelayMS)

                    onValueEntered: (value) => { Player.audioDelayMS = Math.round(value * 1000) }

                    Navigation.parentItem: root
                    Navigation.upItem: root._neighbour(delayField, -1)
                    Navigation.downItem: root._neighbour(delayField, 1)
                }
            }

            SettingsSlider {
                id: delaySlider

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_8

                enabled: Player.isStarted

                from: -10
                to: 10
                stepSize: 0.05
                unit: "s"
                value: Player.audioDelayMS / 1000
                accessibleName: qsTr("Audio track delay")

                tickLabels: [qsTr("-10 s"), qsTr("0 s"), qsTr("+10 s")]

                onMoved: (value) => { Player.audioDelayMS = Math.round(value * 1000) }

                Navigation.parentItem: root
                Navigation.upItem: root._neighbour(delaySlider, -1)
                Navigation.downItem: root._neighbour(delaySlider, 1)
            }

            // the delay from a sound and the moment it shows
            DelayEstimator {
                id: delayEstimator
                onDelayChanged: Player.addAudioDelay(delayEstimator.delay)
            }

            RowLabel {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_14

                wrapMode: Text.WordWrap
                font.pixelSize: VLCStyle.fontSize_small
                text: qsTr("Press Sound Heard when you hear a distinct sound, and Sound Spotted when you see what makes it, in either order.")
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10
                Layout.bottomMargin: VLCStyle.spacing_20

                spacing: VLCStyle.spacing_10

                Widgets.TrackDelayButton {
                    id: soundHeard

                    Layout.fillWidth: true

                    enabled: Player.isStarted

                    text: qsTr("Sound Heard")
                    iconTxt: VLCIcons.check
                    selected: delayEstimator.isSpottedTimeMarked

                    onClicked: {
                        // the estimator names the two moments the other way round
                        delayEstimator.markSpottedTime()
                        if (!delayEstimator.isHeardTimeMarked && delayEstimator.isSpottedTimeMarked)
                            soundSpotted.animate()
                    }

                    Navigation.parentItem: root
                    Navigation.upItem: root._neighbour(soundHeard, -1)
                    Navigation.rightItem: soundSpotted
                    Navigation.downItem: root._neighbour(soundSpotted, 1)
                }

                Widgets.TrackDelayButton {
                    id: soundSpotted

                    Layout.fillWidth: true

                    enabled: Player.isStarted

                    text: qsTr("Sound Spotted")
                    iconTxt: VLCIcons.check
                    selected: delayEstimator.isHeardTimeMarked

                    onClicked: {
                        delayEstimator.markHeardTime()
                        if (!delayEstimator.isSpottedTimeMarked && delayEstimator.isHeardTimeMarked)
                            soundHeard.animate()
                    }

                    Navigation.parentItem: root
                    Navigation.upItem: root._neighbour(soundHeard, -1)
                    Navigation.leftItem: soundHeard
                    Navigation.downItem: root._neighbour(soundSpotted, 1)
                }
            }

            Divider { }

            // Filters

            SectionHeader {
                text: qsTr("Filters")
            }

            GroupHeader {
                Layout.topMargin: VLCStyle.spacing_10
                text: qsTr("Equalizer")
                group: "equalizer"
            }

            EqualizerCombo {
                id: equalizerCombo

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10

                Navigation.parentItem: root
                Navigation.upItem: root._neighbour(equalizerCombo, -1)
                Navigation.downItem: root._neighbour(equalizerCombo, 1)
            }

            ExpandBox {
                id: equalizerBox

                Layout.topMargin: VLCStyle.spacing_10

                accessibleName: qsTr("Equalizer settings")

                // the preamp, then one row per band
                Repeater {
                    model: [qsTr("Preamp")].concat(root.audio.bandNames)

                    delegate: BandRow {
                        required property string modelData
                        required property int index

                        text: modelData
                        value: index === 0 ? root.audio.preamp : root.audio.bands[index - 1]

                        onMoved: (value) => {
                            if (index === 0)
                                root.audio.setPreamp(value)
                            else
                                root.audio.setBand(index - 1, value)
                        }
                    }
                }
            }

            ToggleRow {
                id: twoPassRow

                Layout.topMargin: VLCStyle.spacing_5
                Layout.bottomMargin: VLCStyle.spacing_10

                text: qsTr("2 Pass")
                checked: root.audio.twoPass
                onToggled: root.audio.setTwoPass(!root.audio.twoPass)

                Navigation.parentItem: root
                Navigation.upItem: root._neighbour(twoPassRow, -1)
                Navigation.downItem: root._neighbour(twoPassRow, 1)
            }

            Divider { }

            FilterGroup {
                id: compressorGroup

                text: qsTr("Compressor")
                filter: "compressor"

                GroupHeader {
                    id: compressorRow
                    text: compressorGroup.text
                    group: compressorGroup.filter
                    toggle: true
                }

                FilterBox {
                    id: compressorBox
                    filter: compressorGroup.filter
                    accessibleName: qsTr("Compressor settings")
                }
            }

            Divider { }

            FilterGroup {
                id: spatializerGroup

                text: qsTr("Spatializer")
                filter: "spatializer"

                GroupHeader {
                    id: spatializerRow
                    text: spatializerGroup.text
                    group: spatializerGroup.filter
                    toggle: true
                }

                FilterBox {
                    id: spatializerBox
                    filter: spatializerGroup.filter
                    accessibleName: qsTr("Spatializer settings")
                }
            }

            Divider { }

            FilterGroup {
                id: widenerGroup

                text: qsTr("Stereo Widener")
                filter: "stereo_widen"

                GroupHeader {
                    id: widenerRow
                    text: widenerGroup.text
                    group: widenerGroup.filter
                    toggle: true
                }

                FilterBox {
                    id: widenerBox
                    filter: widenerGroup.filter
                    accessibleName: qsTr("Stereo Widener settings")
                }
            }

            Divider { }

            // the pitch turns its filter on away from 0
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10

                spacing: VLCStyle.spacing_10

                RowLabel {
                    Layout.fillWidth: true
                    Layout.topMargin: VLCStyle.spacing_10
                    Layout.bottomMargin: VLCStyle.spacing_10
                    text: qsTr("Adjust Pitch")

                    PresetMark {
                        anchors.right: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        keys: ["pitch-shift"]
                    }
                }

                ResetButton {
                    group: "pitch"
                    label: qsTr("Adjust Pitch")
                }

                RowLabel {
                    //: a pitch shift, "-2.5 semitones"
                    text: qsTr("%1 semitones").arg(Number(root.audio.values["pitch-shift"] ?? 0).toFixed(1))
                }
            }

            FilterSlider {
                id: pitchSlider

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_5
                Layout.bottomMargin: VLCStyle.spacing_20

                from: -12
                to: 12
                stepSize: 0.5
                tickCount: 3
                value: root.audio.values["pitch-shift"] ?? 0
                accessibleName: qsTr("Adjust Pitch")
                accessibleValue: qsTr("%1 semitones").arg(value.toFixed(1))

                onMoved: (value) => root.audio.setValue("pitch-shift", value)

                Navigation.parentItem: root
                Navigation.upItem: root._neighbour(pitchSlider, -1)
            }
        }
    }

    // Components

    // Figma "Frame 101": a section title
    component SectionHeader : T.Label {
        Layout.fillWidth: true
        leftPadding: VLCStyle.dp(24, VLCStyle.scale)
        topPadding: VLCStyle.spacing_20

        color: theme.fg.secondary
        font.pixelSize: VLCStyle.fontSize_xlarge
        font.weight: Font.Medium
        font.family: VLCStyle.fontFamily
        elide: Text.ElideRight
    }

    // a line between sections
    component Divider : Rectangle {
        Layout.fillWidth: true
        implicitHeight: VLCStyle.border
        color: theme.separator
    }

    // A label, left of its control
    component RowLabel : T.Label {
        leftPadding: VLCStyle.spacing_4
        color: theme.fg.secondary
        font.pixelSize: VLCStyle.fontSize_normal
        font.family: VLCStyle.fontFamily
        elide: Text.ElideRight
    }

    // puts a group back to its defaults, shown while it differs from them
    component ResetButton : Widgets.TextToolButton {
        property string group
        // the name of the group, for accessibility
        property string label

        visible: root.audio.modifiedGroups.includes(group)

        text: qsTr("Reset")
        //: "Reset Compressor"
        Accessible.name: qsTr("Reset %1").arg(label)

        focusPolicy: Qt.TabFocus

        onClicked: root.audio.reset(group)
    }

    // the title of a filter group, its Reset and its ON / OFF toggle
    component GroupHeader : FocusScope {
        id: groupHeader

        property alias text: groupLabel.text
        property string group
        property bool toggle: false

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20

        implicitHeight: Math.max(groupLabel.implicitHeight + 2 * VLCStyle.spacing_10,
                                 groupToggle.visible ? groupToggle.implicitHeight : 0)

        activeFocusOnTab: false

        RowLabel {
            id: groupLabel

            anchors.left: parent.left
            anchors.right: groupReset.left
            anchors.rightMargin: VLCStyle.spacing_10
            anchors.verticalCenter: parent.verticalCenter

            // the group's keys come from the Preset
            PresetMark {
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
                prefixes: groupHeader.group === "stereo_widen" ? ["stereo_widen", "stereowiden-"]
                                                               : [groupHeader.group]
            }
        }

        ResetButton {
            id: groupReset

            anchors.right: groupToggle.visible ? groupToggle.left : parent.right
            anchors.rightMargin: groupToggle.visible ? VLCStyle.spacing_10 : 0
            anchors.verticalCenter: parent.verticalCenter

            group: groupHeader.group
            label: groupLabel.text
        }

        SettingsToggle {
            id: groupToggle

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            visible: groupHeader.toggle
            focus: true

            Accessible.name: groupLabel.text

            checked: root.audio.enabledFilters.includes(groupHeader.group)
            onClicked: root.audio.setFilterEnabled(groupHeader.group, !checked)

            Navigation.parentItem: root
            Navigation.upItem: root._neighbour(groupHeader, -1)
            Navigation.downItem: root._neighbour(groupHeader, 1)
        }
    }

    // a filter group: its header, then its settings
    component FilterGroup : ColumnLayout {
        property string text
        property string filter

        Layout.fillWidth: true
        Layout.topMargin: VLCStyle.spacing_10
        Layout.bottomMargin: VLCStyle.spacing_20

        spacing: VLCStyle.spacing_10
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

        implicitHeight: Math.max(toggleLabel.implicitHeight, toggle.implicitHeight) + 2 * VLCStyle.spacing_8

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

    // a label and a dropdown
    component ChoiceRow : RowLayout {
        property alias text: choiceLabel.text
        // the Preset keys of the row: a dot shows that the Preset sets it
        property alias presetKeys: choiceMark.keys

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20

        spacing: VLCStyle.spacing_10

        RowLabel {
            id: choiceLabel
            Layout.fillWidth: true

            PresetMark {
                id: choiceMark
                anchors.right: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Figma "EQ": the dropdown of the sidebar
    component ChoiceCombo : Widgets.ComboBoxExt {
        id: combo

        implicitHeight: VLCStyle.dp(48, VLCStyle.scale)

        font.pixelSize: VLCStyle.fontSize_normal
        leftPadding: VLCStyle.spacing_14 + VLCStyle.dp(2, VLCStyle.scale)
        rightPadding: VLCStyle.spacing_14

        color: theme.fg.secondary

        // Figma "option": the choices, the current one checked
        delegate: T.ItemDelegate {
            id: choiceDelegate

            required property var model
            required property var modelData
            required property int index

            width: combo.width
            implicitHeight: VLCStyle.dp(44, VLCStyle.scale)
            leftPadding: VLCStyle.spacing_14
            rightPadding: VLCStyle.spacing_14

            text: !combo.textRole ? modelData
                  : (Helpers.isArray(combo.model) ? modelData[combo.textRole] : model[combo.textRole])
            highlighted: combo.highlightedIndex === index

            Accessible.name: text

            contentItem: RowLayout {
                spacing: VLCStyle.spacing_10

                T.Label {
                    Layout.fillWidth: true
                    text: choiceDelegate.text
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Widgets.IconLabel {
                    visible: choiceDelegate.index === combo.currentIndex
                    text: VLCIcons.check
                    color: theme.accent
                    font.pixelSize: VLCStyle.icon_normal
                }
            }

            background: Rectangle {
                radius: VLCStyle.radius_small
                color: choiceDelegate.highlighted ? theme.bg.highlight : "transparent"
            }
        }

        // assistive tools open the list like a click
        Accessible.onPressAction: {
            if (combo.popup.visible)
                combo.popup.close()
            else
                combo.popup.open()
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
    }

    // Figma 2372:8487: None, then the presets with their curve; the
    // highlighted one plays until the list closes
    component EqualizerCombo : ChoiceCombo {
        id: eqCombo

        Accessible.name: qsTr("Equalizer preset")

        model: [qsTr("None")].concat(root.audio.equalizerPresets)

        // the row of the current state: None, a preset, or none for custom bands
        readonly property int _stateIndex: !root.audio.equalizerEnabled ? 0
                                           : (root.audio.equalizerPreset >= 0 ? root.audio.equalizerPreset + 1 : -1)

        Binding on currentIndex {
            when: !eqCombo.popup.visible
            value: eqCombo._stateIndex
        }

        displayText: _stateIndex >= 0 ? model[_stateIndex] : qsTr("Custom")

        onHighlightedIndexChanged: {
            if (popup.visible && highlightedIndex >= 0)
                root.audio.previewEqualizerPreset(highlightedIndex - 1)
        }

        // after onActivated, which keeps the preset
        Connections {
            target: eqCombo.popup

            function onAboutToHide() {
                Qt.callLater(() => root.audio.endEqualizerPreview())
            }
        }

        onActivated: (index) => root.audio.applyEqualizerPreset(index - 1)

        delegate: T.ItemDelegate {
            id: eqDelegate

            required property string modelData
            required property int index

            width: eqCombo.width
            implicitHeight: VLCStyle.dp(44, VLCStyle.scale)
            leftPadding: VLCStyle.spacing_14
            rightPadding: VLCStyle.spacing_14

            highlighted: eqCombo.highlightedIndex === index

            Accessible.name: modelData

            contentItem: RowLayout {
                spacing: VLCStyle.spacing_10

                // generated from the preset's band values
                EqualizerCurve {
                    Layout.preferredWidth: VLCStyle.icon_normal
                    Layout.preferredHeight: VLCStyle.icon_normal

                    visible: eqDelegate.index > 0
                    bands: eqDelegate.index > 0 ? root.audio.equalizerPresetBands(eqDelegate.index - 1) : []
                    color: theme.accent
                    lineColor: theme.fg.secondary
                }

                T.Label {
                    Layout.fillWidth: true
                    text: eqDelegate.modelData
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Widgets.IconLabel {
                    visible: eqDelegate.index === eqCombo._stateIndex
                    text: VLCIcons.check
                    color: theme.accent
                    font.pixelSize: VLCStyle.icon_normal
                }
            }

            background: Rectangle {
                radius: VLCStyle.radius_small
                color: eqDelegate.highlighted ? theme.bg.highlight : "transparent"
            }
        }
    }

    // Figma "Audio - Equalizer": "Expand Settings", then the settings
    component ExpandBox : FocusScope {
        id: box

        property bool expanded: false
        property string accessibleName
        default property alias contents: boxContent.data

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20

        implicitHeight: boxColumn.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: VLCStyle.radius_normal
            // Figma: BG/B2 folded, BG/A3 unfolded
            color: box.expanded ? panelTheme.bg.primary : fieldTheme.bg.primary
            border.width: expandButton.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: theme.accent
        }

        ColumnLayout {
            id: boxColumn

            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0

            T.AbstractButton {
                id: expandButton

                Layout.fillWidth: true
                implicitHeight: VLCStyle.dp(48, VLCStyle.scale)

                focus: true
                focusPolicy: Qt.StrongFocus

                leftPadding: VLCStyle.dp(16, VLCStyle.scale)
                rightPadding: VLCStyle.spacing_10

                Accessible.role: Accessible.Button
                Accessible.name: box.accessibleName
                Accessible.description: box.expanded ? qsTr("Collapse Settings") : qsTr("Expand Settings")
                Accessible.onPressAction: expandButton.clicked()

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                onClicked: box.expanded = !box.expanded

                Navigation.parentItem: root
                Navigation.upItem: root._neighbour(box, -1)
                Navigation.downItem: root._neighbour(box, 1)

                contentItem: RowLayout {
                    T.Label {
                        Layout.fillWidth: true
                        text: box.expanded ? qsTr("Collapse Settings") : qsTr("Expand Settings")
                        color: expandButton.hovered ? theme.fg.primary : theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_normal
                        font.family: VLCStyle.fontFamily
                    }

                    Widgets.IconLabel {
                        text: box.expanded ? VLCIcons.chevron_down : VLCIcons.chevron_right
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.icon_normal
                    }
                }
            }

            ColumnLayout {
                id: boxContent

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.dp(16, VLCStyle.scale)
                Layout.rightMargin: VLCStyle.dp(16, VLCStyle.scale)
                Layout.bottomMargin: VLCStyle.spacing_10

                visible: box.expanded
                spacing: VLCStyle.spacing_5
            }
        }
    }

    // the settings of a filter, from its parameters
    component FilterBox : ExpandBox {
        id: filterBox

        property string filter

        Repeater {
            model: root.audio.parameters(filterBox.filter)

            delegate: ParameterRow {
                required property var modelData

                text: modelData.text
                from: modelData.from
                to: modelData.to
                stepSize: modelData.stepSize
                value: root.audio.values[modelData.name] ?? modelData.defaultValue
                valueText: root._formatValue(value, modelData.scale, modelData.unit)

                onMoved: (value) => root.audio.setValue(modelData.name, value)
            }
        }
    }

    // Figma 536:1513: a name and its value over a slider
    component ParameterRow : ColumnLayout {
        id: parameterRow

        property alias text: parameterLabel.text
        property alias from: parameterSlider.from
        property alias to: parameterSlider.to
        property alias stepSize: parameterSlider.stepSize
        property alias value: parameterSlider.value
        property string valueText

        signal moved(real value)

        Layout.fillWidth: true
        spacing: VLCStyle.spacing_5

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.spacing_10

            T.Label {
                id: parameterLabel
                Layout.fillWidth: true
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
                elide: Text.ElideRight
            }

            T.Label {
                text: parameterRow.valueText
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
            }
        }

        FilterSlider {
            id: parameterSlider

            Layout.fillWidth: true

            accessibleName: parameterLabel.text
            accessibleValue: parameterRow.valueText

            onMoved: (value) => parameterRow.moved(value)

            Navigation.parentItem: root
        }
    }

    // Figma 6114:13610: a band name and its gain field over a slider
    component BandRow : ColumnLayout {
        id: bandRow

        property alias text: bandLabel.text
        property real value

        signal moved(real value)

        Layout.fillWidth: true
        spacing: VLCStyle.spacing_5

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.spacing_5

            spacing: VLCStyle.spacing_10

            T.Label {
                id: bandLabel
                Layout.fillWidth: true
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
                elide: Text.ElideRight
            }

            ValueField {
                Accessible.name: bandLabel.text

                //: a gain in decibels, "-3.5 dB"
                shownText: qsTr("%1 dB").arg(bandRow.value.toFixed(1))

                onValueEntered: (value) => bandRow.moved(Math.max(-20, Math.min(20, value)))

                Navigation.parentItem: root
            }
        }

        FilterSlider {
            Layout.fillWidth: true

            from: -20
            to: 20
            stepSize: 0.1
            value: bandRow.value
            accessibleName: bandLabel.text
            accessibleValue: qsTr("%1 dB").arg(bandRow.value.toFixed(1))

            onMoved: (value) => bandRow.moved(value)

            Navigation.parentItem: root
        }
    }

    // Figma "text_input": a number with its unit, typed in
    component ValueField : Widgets.TextFieldExt {
        id: field

        // the value as shown while not typed in
        property string shownText

        signal valueEntered(real value)

        Layout.preferredWidth: VLCStyle.dp(140, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
        radius: VLCStyle.radius_normal

        Binding on text {
            when: !field.activeFocus
            value: field.shownText
            restoreMode: Binding.RestoreNone
        }

        onEditingFinished: {
            const value = root._parseNumber(text)
            if (!isNaN(value))
                field.valueEntered(value)
            text = field.shownText
        }
    }

    // Figma "range": a slider without labels, with optional ticks
    component FilterSlider : T.Slider {
        id: filterSlider

        property string accessibleName
        property string accessibleValue
        // ticks spread over the range, none with 0
        property int tickCount: 0

        implicitHeight: VLCStyle.dp(24, VLCStyle.scale)

        snapMode: T.Slider.SnapAlways
        live: true
        focusPolicy: Qt.StrongFocus

        Accessible.name: accessibleName
        Accessible.description: accessibleValue

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

        readonly property ColorContext colorContext: ColorContext {
            id: sliderTheme
            colorSet: ColorContext.Slider
            enabled: filterSlider.enabled
            focused: filterSlider.visualFocus
            hovered: filterSlider.hovered
        }

        T.ToolTip.visible: filterSlider.pressed || filterSlider.visualFocus
        T.ToolTip.text: accessibleValue

        background: Rectangle {
            id: sliderTrack

            y: (filterSlider.height - height) / 2
            width: filterSlider.width
            height: VLCStyle.dp(16, VLCStyle.scale)
            radius: VLCStyle.radius_normal
            color: sliderTheme.bg.primary
            border.width: filterSlider.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: sliderTheme.visualFocus

            Rectangle {
                x: VLCStyle.dp(2, VLCStyle.scale)
                y: VLCStyle.dp(2, VLCStyle.scale)
                height: parent.height - 2 * y
                width: filterSlider.visualPosition * (parent.width - 2 * x)
                radius: VLCStyle.radius_small
                color: sliderTheme.bg.highlight
            }

            Repeater {
                model: filterSlider.tickCount

                Rectangle {
                    required property int index

                    x: VLCStyle.dp(2, VLCStyle.scale)
                       + index * (sliderTrack.width - VLCStyle.dp(4, VLCStyle.scale)) / Math.max(1, filterSlider.tickCount - 1)
                       - width / 2
                    y: (sliderTrack.height - height) / 2
                    width: VLCStyle.border
                    height: VLCStyle.dp(6, VLCStyle.scale)
                    color: sliderTheme.bg.secondary
                }
            }
        }

        handle: Rectangle {
            x: filterSlider.visualPosition * (filterSlider.availableWidth - width)
            y: (filterSlider.height - height) / 2
            width: VLCStyle.dp(8, VLCStyle.scale)
            height: VLCStyle.dp(24, VLCStyle.scale)
            radius: VLCStyle.radius_small
            color: sliderTheme.fg.primary
        }
    }
}
