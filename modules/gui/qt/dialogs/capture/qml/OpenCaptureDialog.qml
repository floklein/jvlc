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
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Dialogs
import VLC.Playlist
import VLC.Widgets as Widgets
import VLC.Style

// Open Capture Device: pick a camera, screen, TV tuner or capture card,
// tune it, preview it and play it.
Window {
    id: root

    readonly property var capture: OpenCaptureModel

    transientParent: MainCtx.intfMainWindow
    flags: Qt.Dialog
    modality: Qt.ApplicationModal

    title: qsTr("Open Capture Device")

    width: VLCStyle.dp(720, VLCStyle.scale)
    height: Math.min(VLCStyle.dp(760, VLCStyle.scale), Screen.desktopAvailableHeight * 0.9)
    minimumWidth: VLCStyle.dp(480, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(400, VLCStyle.scale)

    color: theme.bg.primary

    function open() {
        capture.refreshDevices()
        show()
        raise()
        requestActivate()
    }

    // index of the entry of `list` whose `role` is `value`, -1 if none;
    // unlike ComboBox.indexOfValue(), it follows changes of the list
    function indexIn(list, role, value) {
        for (let i = 0; i < list.length; ++i) {
            if (list[i][role] == value)
                return i
        }
        return -1
    }

    function play() {
        MainPlaylistController.append([capture.inputItem()], true)
        close()
    }

    Component.onCompleted: {
        if (MainCtx.createWindowWithoutRedirectionSurface) { // Win32
            const ret = MainCtx.createWindowWithoutRedirectionSurface(this)
            if (!ret)
                console.debug("MainCtx::createWindowWithoutRedirectionSurface(): returned false for window", this)
        }
    }

    // grab frames only while the dialog is shown; a tuner takes long to
    // lock, so it gets a single frame
    Binding {
        target: root.capture.preview
        property: "active"
        value: root.visible
    }

    Binding {
        target: root.capture.preview
        property: "refreshInterval"
        value: root.capture.deviceType === OpenCaptureModel.TvTuner ? 0 : 1000
    }

    Shortcut {
        sequences: [ StandardKey.Cancel ]
        onActivated: root.close()
    }

    // the colors come from the ColorContext of the closest parent item
    component FieldLabel: Widgets.LabelExt {
        readonly property ColorContext colorContext: ColorContext {
            id: fieldTheme
            colorSet: ColorContext.Window
        }
        color: fieldTheme.fg.primary
        font.pixelSize: VLCStyle.fontSize_normal
        Layout.preferredWidth: VLCStyle.dp(160, VLCStyle.scale)
    }

    component SectionTitle: Widgets.LabelExt {
        readonly property ColorContext colorContext: ColorContext {
            id: titleTheme
            colorSet: ColorContext.Window
        }
        color: titleTheme.fg.primary
        font.pixelSize: VLCStyle.fontSize_xlarge
    }

    component Hint: Widgets.CaptionLabel {
        readonly property ColorContext colorContext: ColorContext {
            id: hintTheme
            colorSet: ColorContext.Window
        }
        color: hintTheme.fg.secondary
        wrapMode: Text.Wrap
        elide: Text.ElideNone
        Layout.fillWidth: true
    }

    component NumberField: Widgets.SpinBoxExt {
        editable: true
        Layout.preferredWidth: VLCStyle.dp(150, VLCStyle.scale)
    }

    component Separator: Rectangle {
        readonly property ColorContext colorContext: ColorContext {
            id: separatorTheme
            colorSet: ColorContext.Window
        }
        Layout.fillWidth: true
        implicitHeight: VLCStyle.border
        color: separatorTheme.separator
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        readonly property ColorContext colorContext: ColorContext {
            id: theme
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        Flickable {
            id: flickable

            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            contentWidth: width
            contentHeight: content.implicitHeight + 2 * VLCStyle.margin_large
            boundsBehavior: Flickable.StopAtBounds

            T.ScrollBar.vertical: Widgets.ScrollBarExt { }

            // keep the field that has the keyboard focus in view
            Connections {
                target: root

                function onActiveFocusItemChanged() {
                    const item = root.activeFocusItem
                    if (!item || !flickable.contentItem.contains(flickable.contentItem.mapFromItem(item, 0, 0)))
                        return
                    const top = flickable.contentItem.mapFromItem(item, 0, 0).y - VLCStyle.margin_normal
                    const bottom = top + item.height + 2 * VLCStyle.margin_normal
                    if (top < flickable.contentY)
                        flickable.contentY = Math.max(0, top)
                    else if (bottom > flickable.contentY + flickable.height)
                        flickable.contentY = Math.min(bottom - flickable.height,
                                                      flickable.contentHeight - flickable.height)
                }
            }

            ColumnLayout {
                id: content

                x: VLCStyle.margin_large
                y: VLCStyle.margin_large
                width: flickable.width - 2 * VLCStyle.margin_large

                spacing: VLCStyle.margin_normal

                // Device type and preview
                SectionTitle {
                    text: qsTr("Device type")
                }

                Widgets.ComboBoxExt {
                    id: typeCombo

                    Layout.fillWidth: true

                    model: root.capture.deviceTypes
                    textRole: "text"
                    valueRole: "type"
                    currentIndex: root.indexIn(root.capture.deviceTypes, "type", root.capture.deviceType)
                    onActivated: root.capture.deviceType = currentValue

                    Accessible.name: qsTr("Device type")
                }

                Hint {
                    visible: typeCombo.count === 0
                    text: qsTr("No capture module is installed.")
                }

                Rectangle {
                    id: previewFrame

                    Layout.fillWidth: true
                    Layout.preferredHeight: width * 9 / 16
                    Layout.maximumHeight: VLCStyle.dp(240, VLCStyle.scale)

                    color: "black"
                    radius: VLCStyle.margin_xxsmall
                    clip: true

                    Image {
                        id: previewImage

                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        asynchronous: false
                        source: root.capture.preview.image
                        visible: root.capture.preview.status === CapturePreview.Ready

                        Accessible.role: Accessible.Graphic
                        Accessible.name: qsTr("Preview")
                    }

                    Widgets.BusyIndicatorExt {
                        anchors.centerIn: parent
                        running: visible
                        visible: root.capture.preview.status === CapturePreview.Loading
                    }

                    Widgets.LabelExt {
                        anchors.centerIn: parent
                        visible: root.capture.preview.status === CapturePreview.Unavailable
                        text: qsTr("No preview")
                        color: "white"
                        opacity: 0.7
                    }
                }

                Separator { }

                // Basic fields of the device type
                SectionTitle {
                    text: qsTr("Device")
                }

                GridLayout {
                    id: cameraFields

                    Layout.fillWidth: true
                    visible: root.capture.deviceType === OpenCaptureModel.Camera
                    columns: 2
                    columnSpacing: VLCStyle.margin_normal
                    rowSpacing: VLCStyle.margin_small

                    FieldLabel { text: qsTr("Video device") }

                    Widgets.ComboBoxExt {
                        Layout.fillWidth: true
                        model: root.capture.videoDevices
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: root.indexIn(root.capture.videoDevices, "value", root.capture.videoDevice)
                        onActivated: root.capture.videoDevice = currentValue
                        Accessible.name: qsTr("Video device")
                    }

                    FieldLabel { text: qsTr("Audio device") }

                    Widgets.ComboBoxExt {
                        Layout.fillWidth: true
                        model: root.capture.audioDevices
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: root.indexIn(root.capture.audioDevices, "value", root.capture.audioDevice)
                        onActivated: root.capture.audioDevice = currentValue
                        Accessible.name: qsTr("Audio device")
                    }
                }

                GridLayout {
                    id: screenFields

                    Layout.fillWidth: true
                    visible: root.capture.deviceType === OpenCaptureModel.Screen
                    columns: 2
                    columnSpacing: VLCStyle.margin_normal
                    rowSpacing: VLCStyle.margin_small

                    FieldLabel { text: qsTr("Area") }

                    Widgets.ComboBoxExt {
                        id: areaCombo

                        Layout.fillWidth: true
                        model: [ qsTr("Entire screen"), qsTr("Region") ]
                        currentIndex: (root.capture.screenRegion.width > 0 && root.capture.screenRegion.height > 0) ? 1 : 0
                        onActivated: (index) => {
                            if (index === 0)
                                root.capture.screenRegion = Qt.rect(0, 0, 0, 0)
                            else
                                root.capture.screenRegion = Qt.rect(0, 0, Screen.width, Screen.height)
                        }
                        Accessible.name: qsTr("Area")
                    }

                    FieldLabel {
                        visible: areaCombo.currentIndex === 1
                        text: qsTr("Position (x, y)")
                    }

                    RowLayout {
                        visible: areaCombo.currentIndex === 1
                        spacing: VLCStyle.margin_small

                        NumberField {
                            from: 0; to: 32767; editable: true
                            value: root.capture.screenRegion.x
                            onValueModified: root.capture.screenRegion = Qt.rect(value, root.capture.screenRegion.y,
                                                                                 root.capture.screenRegion.width,
                                                                                 root.capture.screenRegion.height)
                            Accessible.name: qsTr("Left")
                        }

                        NumberField {
                            from: 0; to: 32767; editable: true
                            value: root.capture.screenRegion.y
                            onValueModified: root.capture.screenRegion = Qt.rect(root.capture.screenRegion.x, value,
                                                                                 root.capture.screenRegion.width,
                                                                                 root.capture.screenRegion.height)
                            Accessible.name: qsTr("Top")
                        }
                    }

                    FieldLabel {
                        visible: areaCombo.currentIndex === 1
                        text: qsTr("Size (width × height)")
                    }

                    RowLayout {
                        visible: areaCombo.currentIndex === 1
                        spacing: VLCStyle.margin_small

                        NumberField {
                            from: 1; to: 32767; editable: true
                            value: root.capture.screenRegion.width
                            onValueModified: root.capture.screenRegion = Qt.rect(root.capture.screenRegion.x,
                                                                                 root.capture.screenRegion.y,
                                                                                 value, root.capture.screenRegion.height)
                            Accessible.name: qsTr("Width")
                        }

                        NumberField {
                            from: 1; to: 32767; editable: true
                            value: root.capture.screenRegion.height
                            onValueModified: root.capture.screenRegion = Qt.rect(root.capture.screenRegion.x,
                                                                                 root.capture.screenRegion.y,
                                                                                 root.capture.screenRegion.width, value)
                            Accessible.name: qsTr("Height")
                        }
                    }

                    FieldLabel { text: qsTr("Frame rate") }

                    RowLayout {
                        spacing: VLCStyle.margin_small

                        NumberField {
                            from: 1; to: 120; editable: true
                            value: Math.round(root.capture.screenFps)
                            onValueModified: root.capture.screenFps = value
                            Accessible.name: qsTr("Frame rate")
                        }

                        Widgets.LabelExt {
                            /* xgettext: frames per second */
                            text: qsTr("fps")
                            color: theme.fg.secondary
                        }
                    }
                }

                GridLayout {
                    id: tunerFields

                    Layout.fillWidth: true
                    visible: root.capture.deviceType === OpenCaptureModel.TvTuner
                    columns: 2
                    columnSpacing: VLCStyle.margin_normal
                    rowSpacing: VLCStyle.margin_small

                    FieldLabel { text: qsTr("Delivery system") }

                    Widgets.ComboBoxExt {
                        Layout.fillWidth: true
                        model: root.capture.tunerSystems
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: root.indexIn(root.capture.tunerSystems, "value", root.capture.tunerSystem)
                        onActivated: root.capture.tunerSystem = currentValue
                        Accessible.name: qsTr("Delivery system")
                    }

                    FieldLabel { text: qsTr("Frequency") }

                    RowLayout {
                        spacing: VLCStyle.margin_small

                        NumberField {
                            from: 0; to: 107999999; stepSize: 1000; editable: true
                            value: root.capture.tunerFrequency
                            onValueModified: root.capture.tunerFrequency = value
                            Accessible.name: qsTr("Frequency")
                        }

                        Widgets.LabelExt {
                            text: qsTr("kHz")
                            color: theme.fg.secondary
                        }
                    }

                    FieldLabel {
                        visible: root.capture.tunerHasSymbolRate
                        text: qsTr("Symbol rate")
                    }

                    RowLayout {
                        visible: root.capture.tunerHasSymbolRate
                        spacing: VLCStyle.margin_small

                        NumberField {
                            from: 0; to: 2147483647; stepSize: 1000; editable: true
                            value: root.capture.tunerSymbolRate
                            onValueModified: root.capture.tunerSymbolRate = value
                            Accessible.name: qsTr("Symbol rate")
                        }

                        Widgets.LabelExt {
                            text: qsTr("bauds")
                            color: theme.fg.secondary
                        }
                    }

                    FieldLabel {
                        visible: root.capture.tunerModulations.length > 0
                        text: qsTr("Modulation")
                    }

                    Widgets.ComboBoxExt {
                        Layout.fillWidth: true
                        visible: root.capture.tunerModulations.length > 0
                        model: root.capture.tunerModulations
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: Math.max(0, root.indexIn(root.capture.tunerModulations, "value", root.capture.tunerModulation))
                        onActivated: root.capture.tunerModulation = currentValue
                        Accessible.name: qsTr("Modulation")
                    }

                    FieldLabel {
                        visible: root.capture.tunerHasBandwidth
                        text: qsTr("Bandwidth")
                    }

                    Widgets.ComboBoxExt {
                        Layout.fillWidth: true
                        visible: root.capture.tunerHasBandwidth
                        model: root.capture.tunerBandwidths
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: Math.max(0, root.indexIn(root.capture.tunerBandwidths, "value", root.capture.tunerBandwidth))
                        onActivated: root.capture.tunerBandwidth = currentValue
                        Accessible.name: qsTr("Bandwidth")
                    }

                    FieldLabel { text: qsTr("Tuner card") }

                    NumberField {
                        from: -1; to: 255; editable: true
                        value: root.capture.tunerAdapter
                        onValueModified: root.capture.tunerAdapter = value
                        Accessible.name: qsTr("Tuner card")
                    }
                }

                GridLayout {
                    id: cardFields

                    Layout.fillWidth: true
                    visible: root.capture.deviceType === OpenCaptureModel.CaptureCard
                    columns: 2
                    columnSpacing: VLCStyle.margin_normal
                    rowSpacing: VLCStyle.margin_small

                    FieldLabel { text: qsTr("Card") }

                    NumberField {
                        from: 0; to: 255; editable: true
                        value: root.capture.cardIndex
                        onValueModified: root.capture.cardIndex = value
                        Accessible.name: qsTr("Card")
                    }

                    FieldLabel { text: qsTr("Video input") }

                    Widgets.ComboBoxExt {
                        Layout.fillWidth: true
                        model: root.capture.cardVideoConnections
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: Math.max(0, root.indexIn(root.capture.cardVideoConnections, "value", root.capture.cardVideoConnection))
                        onActivated: root.capture.cardVideoConnection = currentValue
                        Accessible.name: qsTr("Video input")
                    }
                }

                Separator {
                    visible: root.capture.advancedOptions.count > 0
                }

                // Advanced options: the other capture options of the type
                Widgets.ButtonExt {
                    id: advancedButton

                    visible: root.capture.advancedOptions.count > 0
                    checkable: true
                    text: qsTr("Advanced options")
                    iconTxt: checked ? VLCIcons.chevron_up : VLCIcons.chevron_down

                    Accessible.name: text
                }

                Hint {
                    visible: advancedButton.visible && advancedButton.checked
                    text: qsTr("Options of the capture module. Changed options apply to this capture only.")
                }

                ModuleOptionsView {
                    Layout.fillWidth: true
                    visible: advancedButton.visible && advancedButton.checked
                    options: root.capture.advancedOptions
                }

                Widgets.ButtonExt {
                    visible: advancedButton.checked && root.capture.advancedOptions.modified
                    text: qsTr("Reset advanced options")
                    onClicked: root.capture.advancedOptions.reset()
                }
            }
        }

        Separator { }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: VLCStyle.margin_normal

            Widgets.ButtonExt {
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            Item { Layout.fillWidth: true }

            Widgets.ActionButtonPrimary {
                text: qsTr("Play")
                enabled: typeCombo.count > 0 && root.capture.mrl.length > 0
                onClicked: root.play()
            }
        }
    }
}
