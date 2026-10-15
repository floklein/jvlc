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

// Convert: turns files into another format in the background, on a hidden
// player, without touching playback or the Queue.
WindowDialog {
    id: root

    // the MRLs of the media to convert
    property var sources: []

    // the local file chosen with "Save As…", for a single file
    property string saveAs: ""

    property bool deinterlace: false

    readonly property string extension: profileModel.fileExtension(profileModel.selectedValue)

    readonly property string destination: {
        if (sources.length !== 1)
            return ""
        if (saveAs !== "")
            return saveAs
        return Conversions.convertedPath(sources[0], extension)
    }

    readonly property bool canConvert: sources.length > 0 && profileModel.selectedValue !== ""

    title: qsTr("Convert")

    width: VLCStyle.dp(560, VLCStyle.scale)
    height: VLCStyle.dp(600, VLCStyle.scale)
    minimumWidth: VLCStyle.dp(440, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(480, VLCStyle.scale)

    standardButtons: DialogButtonBox.NoButton

    function setSources(mrls) {
        saveAs = ""
        sources = mrls
    }

    function addSources(mrls) {
        const list = sources.slice()
        for (const mrl of mrls) {
            if (list.indexOf(mrl) < 0)
                list.push(mrl)
        }
        saveAs = ""
        sources = list
    }

    function removeSource(index) {
        const list = sources.slice()
        list.splice(index, 1)
        saveAs = ""
        sources = list
    }

    function convert() {
        if (!canConvert)
            return
        Conversions.convert(sources, profileModel.selectedValue, saveAs, deinterlace)
        hide()
    }

    function editProfile() {
        editorLoader.active = true
        editorLoader.item.edit(profileModel.selectedName, profileModel.selectedValue,
                               profileModel.selectedBuiltIn)
    }

    function displayName(mrl) {
        let decoded = mrl
        try {
            decoded = decodeURIComponent(mrl)
        } catch (e) {
        }
        const slash = decoded.lastIndexOf("/")
        return (slash >= 0 && slash < decoded.length - 1) ? decoded.substring(slash + 1) : decoded
    }

    function nativePath(path) {
        return Qt.platform.os === "windows" ? path.replace(/\//g, "\\") : path
    }

    component SectionLabel: Widgets.SubtitleLabel {
        font.pixelSize: VLCStyle.fontSize_large
        color: root.colorContext.fg.primary
    }

    ConversionProfileModel {
        id: profileModel
    }

    Loader {
        id: editorLoader

        active: false

        sourceComponent: ConversionProfileEditor {
            transientParent: root
            profiles: profileModel
        }
    }

    contentComponent: Item {
        implicitHeight: content.implicitHeight

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: VLCStyle.margin_small

            spacing: VLCStyle.margin_small

            // Source

            SectionLabel {
                text: qsTr("Source")
                color: root.colorContext.fg.primary
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: VLCStyle.dp(72, VLCStyle.scale)

                color: root.colorContext.bg.secondary
                radius: VLCStyle.radius_normal

                ListView {
                    id: sourceList

                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_xxsmall

                    clip: true
                    model: root.sources

                    ScrollBar.vertical: Widgets.ScrollBarExt { }

                    delegate: Item {
                        required property int index
                        required property string modelData

                        width: sourceList.width
                        height: VLCStyle.dp(36, VLCStyle.scale)

                        Widgets.ListLabel {
                            anchors.left: parent.left
                            anchors.right: removeButton.left
                            anchors.leftMargin: VLCStyle.margin_xsmall
                            anchors.verticalCenter: parent.verticalCenter

                            text: root.displayName(modelData)
                            color: root.colorContext.fg.primary
                            elide: Text.ElideMiddle
                        }

                        Widgets.IconToolButton {
                            id: removeButton

                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter

                            text: VLCIcons.close
                            description: qsTr("Remove")
                            font.pixelSize: VLCStyle.icon_small

                            // NOTE: The glyph stays small, the target is 24 dp (WCAG 2.5.8).
                            implicitWidth: VLCStyle.icon_normal
                            implicitHeight: VLCStyle.icon_normal

                            onClicked: root.removeSource(index)
                        }
                    }
                }

                Widgets.ListLabel {
                    anchors.centerIn: parent

                    visible: root.sources.length === 0
                    text: qsTr("No files to convert")
                    color: root.colorContext.fg.secondary
                }
            }

            Widgets.ButtonExt {
                text: qsTr("Add Files…")

                onClicked: root.addSources(Conversions.chooseMedia())
            }

            // Profile

            SectionLabel {
                Layout.topMargin: VLCStyle.margin_small

                text: qsTr("Profile")
                color: root.colorContext.fg.primary
            }

            RowLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_small

                Widgets.ComboBoxExt {
                    Layout.fillWidth: true
                    Layout.preferredHeight: VLCStyle.combobox_height_normal

                    model: profileModel
                    textRole: "name"

                    currentIndex: profileModel.selected

                    onActivated: (index) => {
                        profileModel.selected = index
                    }
                }

                Widgets.ButtonExt {
                    text: qsTr("Custom…")

                    onClicked: root.editProfile()
                }
            }

            // Destination

            SectionLabel {
                Layout.topMargin: VLCStyle.margin_small

                text: qsTr("Destination")
                color: root.colorContext.fg.primary
            }

            RowLayout {
                Layout.fillWidth: true

                visible: root.sources.length === 1
                spacing: VLCStyle.margin_small

                Widgets.TextFieldExt {
                    Layout.fillWidth: true

                    readOnly: true
                    text: root.nativePath(root.destination)
                }

                Widgets.ButtonExt {
                    text: qsTr("Save As…")

                    onClicked: {
                        const path = Conversions.chooseDestination(root.destination, root.extension)
                        if (path !== "")
                            root.saveAs = path
                    }
                }
            }

            Widgets.ListLabel {
                Layout.fillWidth: true

                visible: root.sources.length !== 1
                wrapMode: Text.Wrap
                text: qsTr("Each file is saved next to its original, with “-converted” added to its name.")
                color: root.colorContext.fg.secondary
            }

            // More options

            Widgets.CheckBoxExt {
                id: moreOptions

                Layout.topMargin: VLCStyle.margin_small

                text: qsTr("More options")
            }

            Widgets.CheckBoxExt {
                Layout.leftMargin: VLCStyle.margin_large

                visible: moreOptions.checked
                text: qsTr("Deinterlace")
                checked: root.deinterlace

                onToggled: root.deinterlace = checked
            }

            // Buttons

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_small

                spacing: VLCStyle.margin_small

                Item { Layout.fillWidth: true }

                Widgets.ButtonExt {
                    text: qsTr("Cancel")

                    onClicked: root.hide()
                }

                Widgets.ActionButtonPrimary {
                    text: qsTr("Convert")
                    enabled: root.canConvert

                    onClicked: root.convert()
                }
            }
        }
    }
}
