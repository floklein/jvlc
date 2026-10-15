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
import QtQuick.Controls.Basic
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs

// The Conversion profile editor, opened by Convert's "Custom…"
WindowDialog {
    id: root

    required property ConversionProfileModel profiles

    // the profile settings, by "category_property" key (see ConversionProfileModel)
    property var fields: ({})

    // the name of the custom profile being edited, empty for a new one
    property string editedName: ""

    property string name: ""

    readonly property var muxers: profiles.muxers()
    readonly property var muxer: {
        for (const m of muxers) {
            if (m.value === fields.muxer_mux)
                return m
        }
        return null
    }

    title: editedName !== "" ? qsTr("Edit Conversion Profile") : qsTr("New Conversion Profile")

    modal: true

    width: VLCStyle.dp(560, VLCStyle.scale)
    height: VLCStyle.dp(640, VLCStyle.scale)
    minimumWidth: VLCStyle.dp(480, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(400, VLCStyle.scale)

    standardButtons: DialogButtonBox.NoButton

    // Edits the profile; a built-in one is the base of a new profile
    function edit(profileName, value, builtin) {
        fields = profiles.fields(value)
        editedName = builtin ? "" : profileName
        name = builtin ? "" : profileName
        open()
        raise()
        requestActivate()
    }

    function set(key, value) {
        const copy = Object.assign({}, fields)
        copy[key] = value
        fields = copy
    }

    function toggleFilter(key, filter, on) {
        const list = (fields[key] || []).filter((f) => f !== filter)
        if (on)
            list.push(filter)
        set(key, list)
    }

    function save() {
        if (name.trim() === "")
            return
        if (editedName !== "" && editedName !== name.trim())
            profiles.remove(profiles.indexOf(editedName))
        const row = profiles.save(name.trim(), profiles.value(fields))
        profiles.selected = row
        hide()
    }

    function remove() {
        profiles.remove(profiles.indexOf(editedName))
        hide()
    }

    component Section: Widgets.SubtitleLabel {
        Layout.topMargin: VLCStyle.margin_normal
        font.pixelSize: VLCStyle.fontSize_large
        color: root.colorContext.fg.primary
    }

    component FieldLabel: Widgets.ListLabel {
        Layout.minimumWidth: VLCStyle.dp(150, VLCStyle.scale)
        color: root.colorContext.fg.primary
    }

    component FieldCheck: Widgets.CheckBoxExt {
        required property string key

        checked: !!root.fields[key]
        onToggled: root.set(key, checked)
    }

    component FieldSpin: Widgets.SpinBoxExt {
        required property string key

        Layout.preferredWidth: VLCStyle.dp(150, VLCStyle.scale)

        editable: true
        from: 0
        to: 1000000
        value: root.fields[key] || 0
        onValueModified: root.set(key, value)
    }

    component FieldCombo: Widgets.ComboBoxExt {
        required property string key

        Layout.fillWidth: true
        Layout.preferredHeight: VLCStyle.combobox_height_normal

        textRole: "text"
        valueRole: "value"
        currentIndex: Math.max(indexOfValue(root.fields[key]), 0)
        onActivated: root.set(key, currentValue)
    }

    component FilterList: Column {
        id: filterList

        required property string key
        required property var filters

        Layout.fillWidth: true

        Repeater {
            model: filterList.filters

            delegate: Widgets.CheckBoxExt {
                required property var modelData

                text: modelData.text
                checked: (root.fields[filterList.key] || []).indexOf(modelData.value) >= 0
                onToggled: root.toggleFilter(filterList.key, modelData.value, checked)

                ToolTip.visible: hovered && modelData.help !== ""
                ToolTip.text: modelData.help
                ToolTip.delay: VLCStyle.delayToolTipAppear
            }
        }
    }

    contentComponent: Item {
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_small

            spacing: VLCStyle.margin_small

            RowLayout {
                Layout.fillWidth: true

                FieldLabel { text: qsTr("Profile name") }

                Widgets.TextFieldExt {
                    Layout.fillWidth: true

                    text: root.name
                    placeholderText: qsTr("Name of the new profile")
                    onTextEdited: root.name = text
                }
            }

            ScrollView {
                id: scrollView

                Layout.fillWidth: true
                Layout.fillHeight: true

                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: scrollView.availableWidth

                    spacing: VLCStyle.margin_xsmall

                    // Container

                    Section { text: qsTr("Container") }

                    RowLayout {
                        Layout.fillWidth: true

                        FieldLabel { text: qsTr("Format") }

                        FieldCombo {
                            key: "muxer_mux"
                            model: root.muxers
                        }
                    }

                    Widgets.CaptionLabel {
                        Layout.fillWidth: true

                        wrapMode: Text.Wrap
                        color: root.colorContext.fg.secondary
                        text: {
                            const m = root.muxer
                            if (!m)
                                return ""
                            const caps = []
                            if (m.video) caps.push(qsTr("Video"))
                            if (m.audio) caps.push(qsTr("Audio"))
                            if (m.subtitles) caps.push(qsTr("Subtitles"))
                            if (m.chapters) caps.push(qsTr("Chapters"))
                            if (m.menus) caps.push(qsTr("Menus"))
                            if (m.streamable) caps.push(qsTr("Streamable"))
                            let text = qsTr("Supports: %1").arg(caps.join(", "))
                            if (m.external)
                                text += "\n" + qsTr("This format is not provided by jVLC itself: it could be missing.")
                            else if (!m.available)
                                text += "\n" + qsTr("This format is missing: converting with this profile will fail.")
                            return text
                        }
                    }

                    // Video

                    Section { text: qsTr("Video") }

                    FieldCheck {
                        key: "video_enable"
                        text: qsTr("Video")
                    }

                    FieldCheck {
                        key: "video_copy"
                        text: qsTr("Keep original video track")
                        visible: !!root.fields.video_enable
                    }

                    GridLayout {
                        Layout.fillWidth: true

                        visible: !!root.fields.video_enable && !root.fields.video_copy
                        columns: 2

                        FieldLabel { text: qsTr("Codec") }
                        FieldCombo {
                            key: "video_codec"
                            model: root.profiles.videoCodecs()
                        }

                        FieldLabel { text: qsTr("Bitrate (kb/s, 0 for auto)") }
                        FieldSpin { key: "vcodec_bitrate" }

                        FieldLabel {
                            text: qsTr("Quality (0 for auto)")
                            visible: root.fields.video_codec === "h264"
                        }
                        FieldSpin {
                            key: "vcodec_qp"
                            to: 51
                            visible: root.fields.video_codec === "h264"
                        }

                        FieldLabel { text: qsTr("Frame rate (0 for the source's)") }
                        FieldSpin { key: "vcodec_framerate"; to: 240 }

                        FieldLabel { text: qsTr("Width (0 for auto)") }
                        FieldSpin { key: "vcodec_width"; to: 16384 }

                        FieldLabel { text: qsTr("Height (0 for auto)") }
                        FieldSpin { key: "vcodec_height"; to: 16384 }

                        FieldLabel { text: qsTr("Encoder options") }
                        Widgets.TextFieldExt {
                            Layout.fillWidth: true

                            text: root.fields.vcodec_custom || ""
                            onTextEdited: root.set("vcodec_custom", text)
                        }
                    }

                    Widgets.CheckBoxExt {
                        id: videoFiltersCheck

                        visible: !!root.fields.video_enable && !root.fields.video_copy
                        text: qsTr("Filters")
                    }

                    FilterList {
                        Layout.leftMargin: VLCStyle.margin_large

                        visible: videoFiltersCheck.visible && videoFiltersCheck.checked
                        key: "video_filters"
                        filters: visible ? root.profiles.videoFilters() : []
                    }

                    // Audio

                    Section { text: qsTr("Audio") }

                    FieldCheck {
                        key: "audio_enable"
                        text: qsTr("Audio")
                    }

                    FieldCheck {
                        key: "audio_copy"
                        text: qsTr("Keep original audio track")
                        visible: !!root.fields.audio_enable
                    }

                    GridLayout {
                        Layout.fillWidth: true

                        visible: !!root.fields.audio_enable && !root.fields.audio_copy
                        columns: 2

                        FieldLabel { text: qsTr("Codec") }
                        FieldCombo {
                            key: "audio_codec"
                            model: root.profiles.audioCodecs()
                        }

                        FieldLabel { text: qsTr("Bitrate (kb/s)") }
                        FieldSpin { key: "acodec_bitrate"; to: 10000 }

                        FieldLabel { text: qsTr("Channels") }
                        FieldSpin { key: "acodec_channels"; from: 1; to: 8 }

                        FieldLabel { text: qsTr("Sample rate") }
                        FieldCombo {
                            key: "acodec_samplerate"
                            model: root.profiles.sampleRates()
                        }

                        FieldLabel { text: qsTr("Volume (%, 0 for no change)") }
                        FieldSpin { key: "acodec_gain"; to: 400 }
                    }

                    Widgets.CheckBoxExt {
                        id: audioFiltersCheck

                        visible: !!root.fields.audio_enable && !root.fields.audio_copy
                        text: qsTr("Filters")
                    }

                    FilterList {
                        Layout.leftMargin: VLCStyle.margin_large

                        visible: audioFiltersCheck.visible && audioFiltersCheck.checked
                        key: "audio_filters"
                        filters: visible ? root.profiles.audioFilters() : []
                    }

                    // Subtitles

                    Section { text: qsTr("Subtitles") }

                    FieldCheck {
                        key: "subtitles_enable"
                        text: qsTr("Subtitles")
                    }

                    FieldCheck {
                        key: "subtitles_overlay"
                        text: qsTr("Overlay subtitles on the video")
                        visible: !!root.fields.subtitles_enable
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        visible: !!root.fields.subtitles_enable && !root.fields.subtitles_overlay

                        FieldLabel { text: qsTr("Codec") }
                        FieldCombo {
                            key: "subtitles_codec"
                            model: root.profiles.subtitleCodecs()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_small

                spacing: VLCStyle.margin_small

                Widgets.ButtonExt {
                    visible: root.editedName !== ""
                    text: qsTr("Delete Profile")

                    onClicked: root.remove()
                }

                Item { Layout.fillWidth: true }

                Widgets.ButtonExt {
                    text: qsTr("Cancel")

                    onClicked: root.hide()
                }

                Widgets.ActionButtonPrimary {
                    text: qsTr("Save")
                    enabled: root.name.trim() !== ""

                    onClicked: root.save()
                }
            }
        }
    }
}
