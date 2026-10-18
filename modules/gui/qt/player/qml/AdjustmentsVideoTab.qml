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

// The Video tab of the Adjustments sidebar (Figma 498:2937): flip, rotation,
// zoom, HDR, resolution, aspect ratio and crop, then the Essential, Crop and
// Colors filters. Every change applies at once; hovering a zoom, aspect ratio
// or crop choice previews it.
FocusScope {
    id: root

    readonly property VideoAdjustments video: Player.videoAdjustments

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // the dark wells of the tab: collapsed groups, fields, toggles
    readonly property ColorContext wellContext: ColorContext {
        id: wellTheme
        colorSet: ColorContext.Switch
    }

    // which filter groups show their settings
    property bool _essentialExpanded: false
    property bool _cropExpanded: false
    property bool _colorsExpanded: false

    readonly property var _flipChoices: [
        { label: qsTr("None"), value: VideoAdjustments.NoFlip },
        { label: qsTr("Horizontal"), value: VideoAdjustments.FlipHorizontal },
        { label: qsTr("Vertical"), value: VideoAdjustments.FlipVertical },
    ]

    //: rotation of the video, clockwise
    readonly property var _rotationChoices: [0, 90, 180, 270].map((angle) => ({
        label: qsTr("%1°").arg(angle), value: angle
    }))

    // d3d11-hdr-mode
    readonly property var _hdrChoices: [
        { text: qsTr("Auto"), value: "auto" },
        { text: qsTr("Off"), value: "never" },
        { text: qsTr("Always HDR"), value: "always" },
        { text: qsTr("HDR from SDR"), value: "generate" },
    ]

    // adaptive-maxheight
    readonly property var _resolutionChoices: [
        { text: qsTr("Best Available"), value: 0 },
        { text: "2160p", value: 2160 },
        { text: "1440p", value: 1440 },
        { text: "1080p", value: 1080 },
        { text: "720p", value: 720 },
        { text: "480p", value: 480 },
        { text: "360p", value: 360 },
    ]

    readonly property var _gradientModes: [
        { text: qsTr("Gradient"), value: "gradient" },
        { text: qsTr("Edge"), value: "edge" },
        { text: qsTr("Hough"), value: "hough" },
    ]

    // Functions

    function _value(key : string) : var {
        return root.video.values[key]
    }

    function _set(key : string, value : var) {
        root.video.setValue(key, value)
    }

    function _angleText(angle : real) : string {
        //: a rotation angle in degrees, "90 deg"
        return qsTr("%1 deg").arg(Math.round(angle * 10) / 10)
    }

    function _colorText(rgb : int) : string {
        return "#" + (rgb & 0xFFFFFF).toString(16).toUpperCase().padStart(6, "0")
    }

    // "#FF0000", "ff0000": -1 when not a color
    function _parseColor(text : string) : int {
        const hex = text.trim().replace(/^#/, "")
        if (!/^[0-9a-fA-F]{6}$/.test(hex))
            return -1
        return parseInt(hex, 16)
    }

    function _indexOf(choices : var, value : var) : int {
        return choices.findIndex((choice) => choice.value === value)
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

            SectionHeader {
                text: qsTr("Defaults")
            }

            // Figma "Frame 106": the label, then the three choices
            RowLabel {
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                // Figma's 17 px line (6114:13609) 26 px down: Qt draws Inter a
                // pixel higher in its box
                Layout.topMargin: VLCStyle.spacing_20 + VLCStyle.spacing_6 + VLCStyle.dp(1, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(16, VLCStyle.scale)

                text: qsTr("Flip")

                PresetMark {
                    anchors.right: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    keys: ["video-flip"]
                }
            }

            SettingsSegmented {
                id: flipChoices

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_20
                implicitHeight: VLCStyle.dp(44, VLCStyle.scale)

                focus: true

                choices: root._flipChoices
                value: root.video.flip
                accessibleName: qsTr("Flip")

                onActivated: (value) => { root.video.flip = value }

                Navigation.parentItem: root
                Navigation.downItem: rotationField
            }

            // Figma "Frame 110": the label and the free angle in a 48 px row,
            // 21 px under the flips; then the quarter turns (not in Figma)
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.dp(21, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(48, VLCStyle.scale)

                spacing: VLCStyle.spacing_10

                RowLabel {
                    Layout.fillWidth: true
                    text: qsTr("Rotate")

                    PresetMark {
                        anchors.right: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        keys: ["video-rotation"]
                    }
                }

                Widgets.TextFieldExt {
                    id: rotationField

                    Layout.preferredWidth: VLCStyle.dp(140, VLCStyle.scale)
                    implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
                    radius: VLCStyle.radius_normal

                    Accessible.name: qsTr("Rotation angle, in degrees")

                    validator: DoubleValidator { bottom: -360; top: 360; decimals: 1 }

                    // follows the rotation, except while typed in
                    Binding on text {
                        when: !rotationField.activeFocus
                        value: root._angleText(root.video.rotation)
                        restoreMode: Binding.RestoreNone
                    }

                    onActiveFocusChanged: {
                        if (activeFocus)
                            text = String(Math.round(root.video.rotation * 10) / 10)
                    }

                    onEditingFinished: {
                        const angle = parseFloat(text)
                        if (!isNaN(angle))
                            root.video.rotation = angle
                        text = root._angleText(root.video.rotation)
                    }

                    Navigation.parentItem: root
                    Navigation.upItem: flipChoices
                    Navigation.downItem: rotationChoices
                }
            }

            SettingsSegmented {
                id: rotationChoices

                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_5

                choices: root._rotationChoices
                // a free angle selects none
                value: root.video.rotation
                accessibleName: qsTr("Rotate")

                onActivated: (value) => { root.video.rotation = value }

                Navigation.parentItem: root
                Navigation.upItem: rotationField
                Navigation.downItem: zoomCombo
            }

            ChoiceRow {
                Layout.topMargin: VLCStyle.spacing_20

                text: qsTr("Zoom")

                VarChoiceCombo {
                    id: zoomCombo

                    Accessible.name: qsTr("Zoom")
                    model: Player.zoom

                    Navigation.parentItem: root
                    Navigation.upItem: rotationChoices
                    Navigation.downItem: hdrRow.visible ? hdrCombo : (resolutionRow.visible ? resolutionCombo : aspectRatioCombo)
                }
            }

            // Direct3D 11 only, read when the video output starts
            ChoiceRow {
                id: hdrRow

                visible: root.video.hdrAvailable

                text: qsTr("HDR")

                ChoiceCombo {
                    id: hdrCombo

                    Accessible.name: qsTr("HDR")
                    Accessible.description: qsTr("Applies to the next video")

                    model: root._hdrChoices
                    textRole: "text"

                    Binding on currentIndex {
                        value: root._indexOf(root._hdrChoices, root.video.hdrMode)
                    }

                    onActivated: (index) => { root.video.hdrMode = root._hdrChoices[index].value }

                    Navigation.parentItem: root
                    Navigation.upItem: zoomCombo
                    Navigation.downItem: resolutionRow.visible ? resolutionCombo : aspectRatioCombo
                }
            }

            // adaptive streams (HLS, DASH): read when the stream opens
            ChoiceRow {
                id: resolutionRow

                visible: root.video.resolutionAvailable

                text: qsTr("Resolution")

                ChoiceCombo {
                    id: resolutionCombo

                    Accessible.name: qsTr("Resolution")
                    Accessible.description: qsTr("Highest resolution of adaptive streams, applies to the next stream")

                    model: root._resolutionChoices
                    textRole: "text"

                    Binding on currentIndex {
                        value: root._indexOf(root._resolutionChoices, root.video.maxResolution)
                    }

                    onActivated: (index) => { root.video.maxResolution = root._resolutionChoices[index].value }

                    Navigation.parentItem: root
                    Navigation.upItem: hdrRow.visible ? hdrCombo : zoomCombo
                    Navigation.downItem: aspectRatioCombo
                }
            }

            ChoiceRow {
                text: qsTr("Aspect Ratio")

                VarChoiceCombo {
                    id: aspectRatioCombo

                    Accessible.name: qsTr("Aspect Ratio")
                    model: Player.aspectRatio

                    Navigation.parentItem: root
                    Navigation.upItem: resolutionRow.visible ? resolutionCombo : (hdrRow.visible ? hdrCombo : zoomCombo)
                    Navigation.downItem: cropCombo
                }
            }

            ChoiceRow {
                text: qsTr("Crop")

                VarChoiceCombo {
                    id: cropCombo

                    Accessible.name: qsTr("Crop")
                    model: Player.crop

                    // a ratio replaces the borders of the Crop filter group
                    onPicked: root.video.clearCropBorders()

                    Navigation.parentItem: root
                    Navigation.upItem: aspectRatioCombo
                    Navigation.downItem: essentialGroup
                }
            }

            Divider { }

            SectionHeader {
                text: qsTr("Filters")
            }

            FilterGroup {
                id: essentialGroup

                title: qsTr("Essential")
                presetPrefixes: ["video-hue", "video-brightness", "video-contrast", "video-saturation",
                                 "video-gamma", "video-sharpen", "video-banding", "video-grain"]
                modified: root.video.essentialModified
                expanded: root._essentialExpanded

                onToggled: root._essentialExpanded = !root._essentialExpanded
                onResetRequested: root.video.reset(VideoAdjustments.Essential)

                Navigation.parentItem: root
                Navigation.upItem: cropCombo
                Navigation.downItem: cropGroup

                FilterSlider { key: "hue"; text: qsTr("Hue"); unit: "°" }
                FilterSlider { key: "brightness"; text: qsTr("Brightness") }
                FilterSlider { key: "contrast"; text: qsTr("Contrast") }
                FilterSlider { key: "saturation"; text: qsTr("Saturation") }
                FilterSlider { key: "gamma"; text: qsTr("Gamma") }
                FilterSlider { key: "sharpenSigma"; text: qsTr("Sharpen Sigma") }
                FilterSlider { key: "bandingRadius"; text: qsTr("Banding Removal Radius") }
                FilterSlider { key: "grainVariance"; text: qsTr("Film Grain Variance") }
            }

            FilterGroup {
                id: cropGroup

                title: qsTr("Crop")
                presetPrefixes: ["video-crop"]
                modified: root.video.cropModified
                expanded: root._cropExpanded

                onToggled: root._cropExpanded = !root._cropExpanded
                onResetRequested: root.video.reset(VideoAdjustments.Crop)

                Navigation.parentItem: root
                Navigation.upItem: essentialGroup
                Navigation.downItem: colorsGroup

                CropRow {
                    firstKey: "cropTop"
                    firstText: qsTr("Top")
                    secondKey: "cropBottom"
                    secondText: qsTr("Bottom")
                    linkKey: "cropLinkTopBottom"
                    linkText: qsTr("Same top and bottom")
                }

                CropRow {
                    firstKey: "cropLeft"
                    firstText: qsTr("Left")
                    secondKey: "cropRight"
                    secondText: qsTr("Right")
                    linkKey: "cropLinkLeftRight"
                    linkText: qsTr("Same left and right")
                }
            }

            FilterGroup {
                id: colorsGroup

                title: qsTr("Colors")
                presetPrefixes: ["video-extract", "video-colorThreshold", "video-negate",
                                 "video-posterize", "video-gradient", "video-sepia"]
                modified: root.video.colorsModified
                expanded: root._colorsExpanded

                onToggled: root._colorsExpanded = !root._colorsExpanded
                onResetRequested: root.video.reset(VideoAdjustments.Colors)

                Navigation.parentItem: root
                Navigation.upItem: cropGroup

                ColorRow {
                    text: qsTr("Color Extraction")
                    enableKey: "extract"
                    colorKey: "extractColor"
                }

                GroupDivider { }

                ColorRow {
                    text: qsTr("Color Threshold")
                    enableKey: "colorThreshold"
                    colorKey: "colorThresholdColor"
                }

                FilterSlider { key: "colorThresholdSaturation"; text: qsTr("Saturation") }
                FilterSlider { key: "colorThresholdSimilarity"; text: qsTr("Similarity") }

                GroupDivider { }

                ToggleRow { key: "negate"; text: qsTr("Negate Colors") }
                ToggleRow { key: "posterize"; text: qsTr("Posterize") }

                GroupDivider { }

                ToggleRow { key: "gradient"; text: qsTr("Gradient") }

                ChoiceCombo {
                    Layout.fillWidth: true
                    Layout.preferredWidth: -1

                    Accessible.name: qsTr("Gradient mode")

                    model: root._gradientModes
                    textRole: "text"

                    Binding on currentIndex {
                        value: root._indexOf(root._gradientModes, root._value("gradientMode"))
                    }

                    onActivated: (index) => root._set("gradientMode", root._gradientModes[index].value)

                    Navigation.parentItem: root
                }

                ToggleRow { key: "gradientColor"; text: qsTr("Color") }
                ToggleRow { key: "gradientCartoon"; text: qsTr("Cartoon") }

                GroupDivider { }

                FilterSlider { key: "sepiaIntensity"; text: qsTr("Sepia Intensity") }
            }

            Item { implicitHeight: VLCStyle.spacing_20 }
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

    // Figma "Frame 104" (1711:4103): the title of a section, 16 Medium FG/A1,
    // 24 px in, 20 px under the previous one
    component SectionHeader : T.Label {
        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.dp(24, VLCStyle.scale)
        Layout.rightMargin: VLCStyle.dp(24, VLCStyle.scale)
        Layout.topMargin: VLCStyle.spacing_20
        // Figma's 19 px line (1711:4104); Qt's is a pixel taller
        Layout.preferredHeight: VLCStyle.dp(19, VLCStyle.scale)

        color: theme.fg.secondary
        font.pixelSize: VLCStyle.fontSize_xlarge
        font.weight: Font.Medium
        font.family: VLCStyle.fontFamily
        elide: Text.ElideRight

        Accessible.role: Accessible.Heading
    }

    component AdjustmentsScrollBar : Widgets.ScrollBarExt {
        opacity: (active || interacting) ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: VLCStyle.duration_short }
        }
    }

    // Figma's lines between sections take no room (a 0 px high vector,
    // 1711:4124): drawn over the top of what follows
    component Divider : Item {
        Layout.fillWidth: true
        implicitHeight: 0
        z: 1

        Rectangle {
            width: parent.width
            height: VLCStyle.border
            color: theme.separator
        }
    }

    // a separator between the filters of a group
    component GroupDivider : Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: VLCStyle.spacing_4
        Layout.bottomMargin: VLCStyle.spacing_4
        implicitHeight: VLCStyle.border
        color: theme.separator
    }

    // a label and a 220 px dropdown
    component ChoiceRow : RowLayout {
        property alias text: choiceLabel.text

        Layout.fillWidth: true
        Layout.leftMargin: VLCStyle.spacing_20
        Layout.rightMargin: VLCStyle.spacing_20
        Layout.bottomMargin: VLCStyle.spacing_20

        spacing: VLCStyle.spacing_10

        RowLabel {
            id: choiceLabel
            Layout.fillWidth: true
        }
    }

    // Figma "EQ" (6114:13510): the dropdown of the sidebar, 220x48, its text
    // 18 px and its chevron 14 px from the edges
    component ChoiceCombo : Widgets.ComboBoxExt {
        id: combo

        Layout.preferredWidth: VLCStyle.dp(220, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(48, VLCStyle.scale)

        leftPadding: frame + VLCStyle.dp(16, VLCStyle.scale)
        rightPadding: frame + VLCStyle.spacing_12
    }

    // a dropdown over a variable of the video output (zoom, aspect ratio,
    // crop): hovering or arrowing through the open list previews a choice,
    // closing it without a pick goes back
    component VarChoiceCombo : ChoiceCombo {
        id: varCombo

        // a choice was picked (not previewed)
        signal picked()

        textRole: "display"
        // empty without a video output
        enabled: count > 0

        Binding on currentIndex {
            value: varCombo.model ? varCombo.model.currentRow : -1
        }

        ChoicePreview {
            id: preview

            onApplyRequested: (row) => varCombo.model.toggleIndex(row)
        }

        Connections {
            target: varCombo.popup

            function onAboutToShow() {
                preview.begin(varCombo.model.currentRow)
            }

            function onClosed() {
                preview.cancel()
            }
        }

        onHighlightedIndexChanged: {
            if (varCombo.popup.visible)
                preview.hover(varCombo.highlightedIndex)
        }

        onActivated: (index) => {
            preview.commit(index)
            varCombo.picked()
        }
    }

    // Figma "Frame 72": the title of a group and its Reset, then the card
    // that expands to the settings
    component FilterGroup : FocusScope {
        id: group

        property string title
        // the Preset keys of the group, by prefix
        property var presetPrefixes: []
        property bool modified: false
        property bool expanded: false

        default property alias settings: settingsColumn.data

        signal toggled()
        signal resetRequested()

        Layout.fillWidth: true
        implicitHeight: groupColumn.implicitHeight

        ColumnLayout {
            id: groupColumn

            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10
                Layout.preferredHeight: VLCStyle.dp(37, VLCStyle.scale)

                RowLabel {
                    Layout.fillWidth: true
                    text: group.title
                    Accessible.role: Accessible.Heading

                    PresetMark {
                        anchors.right: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        prefixes: group.presetPrefixes
                    }
                }

                Widgets.TextToolButton {
                    id: resetButton

                    visible: group.modified

                    text: qsTr("Reset")
                    Accessible.name: qsTr("Reset %1").arg(group.title)

                    onClicked: group.resetRequested()

                    Navigation.parentItem: group
                    Navigation.downItem: expandButton
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.spacing_20
                Layout.rightMargin: VLCStyle.spacing_20
                Layout.topMargin: VLCStyle.spacing_10
                Layout.bottomMargin: VLCStyle.spacing_20

                implicitHeight: cardColumn.implicitHeight
                radius: VLCStyle.radius_normal
                color: group.expanded ? theme.bg.secondary : wellTheme.bg.primary

                ColumnLayout {
                    id: cardColumn

                    width: parent.width
                    spacing: 0

                    // Figma "Frame 76": Expand / Collapse Settings
                    T.AbstractButton {
                        id: expandButton

                        Layout.fillWidth: true
                        implicitHeight: VLCStyle.dp(48, VLCStyle.scale)

                        focus: true
                        focusPolicy: Qt.StrongFocus
                        leftPadding: VLCStyle.spacing_14 + VLCStyle.dp(2, VLCStyle.scale)
                        rightPadding: VLCStyle.spacing_10

                        text: group.expanded ? qsTr("Collapse Settings") : qsTr("Expand Settings")

                        // screen readers read it as on (expanded) or off: a real
                        // checkable, the attached Accessible.checkable does not hold
                        checkable: true
                        checked: group.expanded

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("%1 settings").arg(group.title)
                        Accessible.onPressAction: group.toggled()

                        Keys.priority: Keys.AfterItem
                        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                        onClicked: group.toggled()

                        Navigation.parentItem: group
                        Navigation.upItem: resetButton.visible ? resetButton : null

                        background: Rectangle {
                            radius: VLCStyle.radius_normal
                            color: "transparent"
                            border.width: expandButton.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                            border.color: theme.accent
                        }

                        contentItem: RowLayout {
                            spacing: VLCStyle.spacing_10

                            Text {
                                Layout.fillWidth: true
                                text: expandButton.text
                                color: expandButton.hovered ? theme.fg.value : theme.fg.label
                                font.pixelSize: VLCStyle.fontSize_normal
                                font.family: VLCStyle.fontFamily
                                elide: Text.ElideRight
                                Accessible.ignored: true
                            }

                            Widgets.IconLabel {
                                text: group.expanded ? VLCIcons.chevron_down : VLCIcons.chevron_right
                                color: theme.fg.label
                                font.pixelSize: VLCStyle.icon_normal
                                Accessible.ignored: true
                            }
                        }
                    }

                    ColumnLayout {
                        id: settingsColumn

                        visible: group.expanded

                        Layout.fillWidth: true
                        Layout.leftMargin: VLCStyle.spacing_14 + VLCStyle.dp(2, VLCStyle.scale)
                        Layout.rightMargin: VLCStyle.spacing_14 + VLCStyle.dp(2, VLCStyle.scale)
                        Layout.bottomMargin: VLCStyle.spacing_14 + VLCStyle.dp(2, VLCStyle.scale)

                        spacing: VLCStyle.spacing_10
                    }
                }
            }

            Divider { }
        }
    }

    // a value of a filter: its name, then the slider
    component FilterSlider : ColumnLayout {
        id: filterSlider

        required property string key
        property alias text: sliderLabel.text
        property string unit

        readonly property var _range: root.video.range(key)

        Layout.fillWidth: true
        spacing: VLCStyle.spacing_10

        RowLabel {
            id: sliderLabel
            Layout.fillWidth: true
            leftPadding: 0
        }

        SettingsSlider {
            Layout.fillWidth: true

            from: filterSlider._range.from
            to: filterSlider._range.to
            stepSize: filterSlider._range.step
            unit: filterSlider.unit
            value: root._value(filterSlider.key)
            accessibleName: sliderLabel.text

            onMoved: (value) => root._set(filterSlider.key, value)

            Navigation.parentItem: root
        }
    }

    // a label and an ON / OFF toggle
    component ToggleRow : RowLayout {
        id: toggleRow

        required property string key
        property alias text: toggleLabel.text

        Layout.fillWidth: true
        spacing: VLCStyle.spacing_10

        RowLabel {
            id: toggleLabel
            Layout.fillWidth: true
            leftPadding: 0
        }

        SettingsToggle {
            checked: root._value(toggleRow.key) === true
            Accessible.name: toggleLabel.text

            onClicked: root._set(toggleRow.key, !checked)

            Navigation.parentItem: root
        }
    }

    // Figma "Color Extraction": a filter with a color, its hex code, a swatch
    // and its toggle
    component ColorRow : RowLayout {
        id: colorRow

        required property string enableKey
        required property string colorKey
        property alias text: colorLabel.text

        Layout.fillWidth: true
        spacing: VLCStyle.spacing_10

        RowLabel {
            id: colorLabel
            Layout.fillWidth: true
            leftPadding: 0
        }

        Widgets.TextFieldExt {
            id: colorField

            Layout.preferredWidth: VLCStyle.dp(116, VLCStyle.scale)
            implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
            radius: VLCStyle.radius_normal
            rightPadding: swatch.width + 2 * VLCStyle.spacing_4

            Accessible.name: qsTr("%1 color").arg(colorLabel.text)

            validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }

            Binding on text {
                when: !colorField.activeFocus
                value: root._colorText(root._value(colorRow.colorKey))
                restoreMode: Binding.RestoreNone
            }

            onEditingFinished: {
                const rgb = root._parseColor(text)
                if (rgb >= 0)
                    root._set(colorRow.colorKey, rgb)
                text = root._colorText(root._value(colorRow.colorKey))
            }

            Navigation.parentItem: root

            Rectangle {
                id: swatch

                anchors.right: parent.right
                anchors.rightMargin: VLCStyle.spacing_4
                anchors.verticalCenter: parent.verticalCenter
                width: VLCStyle.dp(22, VLCStyle.scale)
                height: width
                radius: VLCStyle.radius_xsmall
                color: root._colorText(root._value(colorRow.colorKey))
            }
        }

        SettingsToggle {
            checked: root._value(colorRow.enableKey) === true
            Accessible.name: colorLabel.text

            onClicked: root._set(colorRow.enableKey, !checked)

            Navigation.parentItem: root
        }
    }

    // a field in pixels: "Top 0 px"
    component CropField : Rectangle {
        id: cropField

        required property string key
        property alias text: cropLabel.text

        Layout.fillWidth: true
        implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
        radius: VLCStyle.radius_normal
        color: wellTheme.bg.primary
        border.width: pixelsInput.activeFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
        border.color: theme.accent

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.spacing_12
            anchors.rightMargin: VLCStyle.spacing_12
            spacing: VLCStyle.spacing_4

            T.Label {
                id: cropLabel
                Layout.fillWidth: true
                color: theme.fg.label
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
                elide: Text.ElideRight
            }

            TextInput {
                id: pixelsInput

                Layout.preferredWidth: VLCStyle.dp(40, VLCStyle.scale)
                horizontalAlignment: TextInput.AlignRight
                color: theme.fg.value
                selectionColor: theme.accent
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
                selectByMouse: true
                activeFocusOnTab: true

                validator: IntValidator { bottom: 0; top: 10000 }

                Accessible.role: Accessible.EditableText
                Accessible.name: qsTr("%1 crop, in pixels").arg(cropLabel.text)

                Binding on text {
                    when: !pixelsInput.activeFocus
                    value: String(root._value(cropField.key))
                    restoreMode: Binding.RestoreNone
                }

                onEditingFinished: {
                    const pixels = parseInt(text, 10)
                    if (!isNaN(pixels))
                        root._set(cropField.key, pixels)
                    text = String(root._value(cropField.key))
                }

                Keys.onUpPressed: root._set(cropField.key, root._value(cropField.key) + 1)
                Keys.onDownPressed: root._set(cropField.key, Math.max(0, root._value(cropField.key) - 1))
            }

            T.Label {
                text: qsTr("px")
                color: theme.fg.label
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
            }
        }
    }

    // Figma "Crop": two borders and the link that keeps them equal
    component CropRow : RowLayout {
        id: cropRow

        required property string firstKey
        required property string firstText
        required property string secondKey
        required property string secondText
        required property string linkKey
        required property string linkText

        readonly property bool linked: root._value(linkKey) === true

        Layout.fillWidth: true
        spacing: VLCStyle.spacing_8

        CropField {
            key: cropRow.firstKey
            text: cropRow.firstText
        }

        CropField {
            key: cropRow.secondKey
            text: cropRow.secondText
            enabled: !cropRow.linked
            opacity: enabled ? 1 : 0.6
        }

        Widgets.IconToolButton {
            description: cropRow.linkText
            text: cropRow.linked ? VLCIcons.link_active : VLCIcons.link
            checked: cropRow.linked
            toggle: true

            onClicked: root._set(cropRow.linkKey, !cropRow.linked)

            Navigation.parentItem: root
        }
    }
}
