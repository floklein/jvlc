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
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Playlist
import VLC.Util
import VLC.Dialogs

// Stream & Cast: the Cast tab moves the playback to a device on the network
// (Chromecast), the Stream tab serves the current item or the Queue over
// the network. MainInterface.qml hosts it, so it survives switching between
// the library and the player.
WindowDialog {
    id: root

    property int currentTab: DialogsProvider.CAST_TAB

    readonly property var rendererManager: Player.rendererManager

    title: qsTr("Stream & Cast")

    width: VLCStyle.dp(520, VLCStyle.scale)
    height: VLCStyle.dp(560, VLCStyle.scale)
    minimumWidth: VLCStyle.dp(420, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(440, VLCStyle.scale)

    color: colorContext.bg.primary

    standardButtons: DialogButtonBox.Close

    // the Cast tab keeps looking for devices while it is shown
    readonly property bool _discovering: visible && currentTab === DialogsProvider.CAST_TAB

    on_DiscoveringChanged: {
        if (_discovering)
            rendererManager.StartScan()
        else
            rendererManager.StopScan()
    }

    Connections {
        target: root.rendererManager

        // a scan lasts 20 s: start another one, so that the list stays live
        function onStatusChanged() {
            if (root._discovering && root.rendererManager.status === RendererManager.IDLE)
                root.rendererManager.StartScan()
        }
    }

    readonly property var _tabs: [
        { tab: DialogsProvider.CAST_TAB, text: qsTr("Cast") },
        { tab: DialogsProvider.STREAM_TAB, text: qsTr("Stream") }
    ]

    readonly property var _protocols: [
        { text: "HTTP", value: StreamOutput.HTTP },
        { text: "RTSP", value: StreamOutput.RTSP },
        { text: "RTP / UDP", value: StreamOutput.RTP },
        { text: "SRT", value: StreamOutput.SRT }
    ]

    function protocolName(protocol) {
        const entry = _protocols.find((p) => p.value === protocol)
        return entry ? entry.text : ""
    }

    // where the clients find the running stream
    function streamDescription() {
        const address = StreamOutput.address
        const port = StreamOutput.port
        switch (StreamOutput.protocol) {
        case StreamOutput.HTTP:
            return address !== "" ? qsTr("Streaming at http://%1:%2/").arg(address).arg(port)
                                  : qsTr("Streaming over HTTP on port %1").arg(port)
        case StreamOutput.RTSP:
            return address !== "" ? qsTr("Streaming at rtsp://%1:%2/").arg(address).arg(port)
                                  : qsTr("Streaming over RTSP on port %1").arg(port)
        default:
            return qsTr("Streaming over %1 to %2:%3").arg(protocolName(StreamOutput.protocol))
                                                    .arg(address).arg(port)
        }
    }

    component SectionLabel: T.Label {
        color: root.colorContext.fg.primary
        font.pixelSize: VLCStyle.fontSize_normal
        font.weight: Font.DemiBold
        textFormat: Text.PlainText
        elide: Text.ElideRight
    }

    component HintLabel: T.Label {
        color: root.colorContext.fg.secondary
        font.pixelSize: VLCStyle.fontSize_small
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    ConversionProfileModel {
        id: profileModel
    }

    contentComponent: FocusScope {
        focus: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_xsmall
            spacing: VLCStyle.margin_normal

            // Tab bar
            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.margin_xsmall

                Repeater {
                    model: root._tabs

                    delegate: T.TabButton {
                        id: tabButton

                        required property var modelData

                        text: modelData.text
                        checked: root.currentTab === modelData.tab

                        padding: VLCStyle.margin_xsmall
                        implicitWidth: implicitContentWidth + leftPadding + rightPadding
                        implicitHeight: implicitContentHeight + topPadding + bottomPadding

                        onClicked: root.currentTab = modelData.tab

                        ColorContext {
                            id: tabTheme
                            colorSet: ColorContext.TabButton
                            focused: tabButton.visualFocus
                            hovered: tabButton.hovered
                            pressed: tabButton.down
                            enabled: tabButton.enabled
                        }

                        contentItem: T.Label {
                            text: tabButton.text
                            color: tabButton.checked ? tabTheme.fg.primary : tabTheme.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.weight: tabButton.checked ? Font.DemiBold : Font.Normal
                            textFormat: Text.PlainText
                        }

                        background: Rectangle {
                            color: "transparent"
                            border.width: tabButton.visualFocus ? VLCStyle.dp(1, VLCStyle.scale) : 0
                            border.color: tabTheme.visualFocus
                            radius: VLCStyle.radius_small

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: VLCStyle.dp(2, VLCStyle.scale)
                                visible: tabButton.checked
                                color: root.colorContext.accent
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                currentIndex: Math.max(0, root._tabs.findIndex((t) => t.tab === root.currentTab))

                // Cast
                ColumnLayout {
                    spacing: VLCStyle.margin_small

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: VLCStyle.margin_small

                        SectionLabel {
                            Layout.fillWidth: true
                            text: qsTr("Devices on your network")
                        }

                        Widgets.BusyIndicatorExt {
                            Layout.preferredWidth: VLCStyle.icon_small
                            Layout.preferredHeight: VLCStyle.icon_small

                            runningDelayed: root.rendererManager.status === RendererManager.RUNNING
                            color: root.colorContext.fg.secondary
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        color: root.colorContext.bg.secondary
                        radius: VLCStyle.dp(6, VLCStyle.scale)

                        ListView {
                            id: deviceList

                            anchors.fill: parent
                            anchors.margins: VLCStyle.margin_xxsmall

                            clip: true
                            model: root.rendererManager

                            ScrollBar.vertical: Widgets.ScrollBarExt { }

                            // playing here, which also stops casting
                            header: DeviceDelegate {
                                width: deviceList.width

                                name: qsTr("This computer")
                                iconText: VLCIcons.tv
                                current: !root.rendererManager.useRenderer
                                currentText: qsTr("Playing")

                                onPicked: root.rendererManager.disableRenderer()
                            }

                            delegate: DeviceDelegate {
                                id: device

                                required property var model

                                width: deviceList.width

                                name: model.name
                                iconText: VLCIcons.renderer
                                current: model.selected

                                // moves the playback to the device
                                onPicked: device.model.selected = true
                            }
                        }

                        HintLabel {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: VLCStyle.margin_small

                            horizontalAlignment: Text.AlignHCenter
                            visible: deviceList.count === 0

                            text: {
                                if (root.rendererManager.status === RendererManager.FAILED)
                                    return qsTr("Looking for devices is not available.")
                                if (root.rendererManager.status === RendererManager.RUNNING)
                                    return qsTr("Looking for devices…")
                                return qsTr("No devices found")
                            }
                        }
                    }

                    HintLabel {
                        Layout.fillWidth: true
                        text: qsTr("Picking a device moves the playback there. A cast button in the Control bar stops casting.")
                    }
                }

                // Stream
                ColumnLayout {
                    id: streamTab

                    spacing: VLCStyle.margin_small

                    readonly property int protocol: root._protocols[protocolBox.currentIndex].value
                    readonly property bool canStart: StreamOutput.isValid(protocol, addressField.text, portBox.value)
                                                     && (sourceQueue.checked ? !MainPlaylistController.empty
                                                                             : Player.isStarted)

                    function start() {
                        const source = sourceQueue.checked ? StreamOutput.Queue : StreamOutput.CurrentItem
                        StreamOutput.start(source, protocol, addressField.text, portBox.value,
                                           profileModel.selectedValue, alsoPlayHere.checked)
                    }

                    // Source
                    SectionLabel {
                        text: qsTr("Stream")
                    }

                    RowLayout {
                        spacing: VLCStyle.margin_normal

                        ButtonGroup { id: sourceGroup }

                        Widgets.RadioButtonExt {
                            id: sourceCurrent

                            ButtonGroup.group: sourceGroup
                            text: qsTr("Current item")
                            checked: true
                            enabled: Player.isStarted
                        }

                        Widgets.RadioButtonExt {
                            id: sourceQueue

                            ButtonGroup.group: sourceGroup
                            text: qsTr("Queue")
                            checked: !Player.isStarted
                        }
                    }

                    // Destination
                    SectionLabel {
                        Layout.topMargin: VLCStyle.margin_xsmall
                        text: qsTr("Protocol")
                    }

                    Widgets.ComboBoxExt {
                        id: protocolBox

                        Layout.fillWidth: true
                        Layout.preferredHeight: VLCStyle.combobox_height_normal

                        model: root._protocols
                        textRole: "text"

                        onActivated: portBox.value = StreamOutput.defaultPort(streamTab.protocol)
                    }

                    GridLayout {
                        Layout.fillWidth: true

                        columns: 2
                        columnSpacing: VLCStyle.margin_small
                        rowSpacing: VLCStyle.margin_xxsmall

                        SectionLabel {
                            id: addressLabel

                            Layout.fillWidth: true
                            text: StreamOutput.addressRequired(streamTab.protocol) ? qsTr("Receiver's address")
                                                                                   : qsTr("Address")
                        }

                        SectionLabel {
                            text: qsTr("Port")
                        }

                        Widgets.TextFieldExt {
                            id: addressField

                            Layout.fillWidth: true

                            Accessible.name: addressLabel.text

                            placeholderText: StreamOutput.addressRequired(streamTab.protocol) ? qsTr("e.g. 239.255.0.1")
                                                                                              : qsTr("All addresses")
                        }

                        Widgets.SpinBoxExt {
                            id: portBox

                            Accessible.name: qsTr("Port")

                            from: 1
                            to: 65535
                            editable: true
                            value: StreamOutput.defaultPort(StreamOutput.HTTP)

                            textFromValue: (value, locale) => String(value)
                            valueFromText: (text, locale) => parseInt(text)
                        }
                    }

                    // Profile
                    SectionLabel {
                        Layout.topMargin: VLCStyle.margin_xsmall
                        text: qsTr("Profile")
                    }

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

                    Widgets.CheckBoxExt {
                        id: alsoPlayHere

                        Layout.topMargin: VLCStyle.margin_xsmall
                        text: qsTr("Also play here")
                        checked: false
                    }

                    Item { Layout.fillHeight: true }

                    // The running stream
                    HintLabel {
                        Layout.fillWidth: true

                        visible: StreamOutput.streaming
                        color: root.colorContext.fg.primary
                        text: root.streamDescription()
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: VLCStyle.margin_small

                        Item { Layout.fillWidth: true }

                        Widgets.ButtonExt {
                            visible: StreamOutput.streaming
                            text: qsTr("Stop Streaming")

                            onClicked: StreamOutput.stop()
                        }

                        Widgets.ActionButtonPrimary {
                            text: StreamOutput.streaming ? qsTr("Restart") : qsTr("Start")
                            enabled: streamTab.canStart

                            onClicked: streamTab.start()
                        }
                    }
                }
            }
        }
    }

    // A device of the Cast tab
    component DeviceDelegate: T.ItemDelegate {
        id: delegate

        property string name
        property string iconText
        property bool current: false
        property string currentText: qsTr("Casting")

        signal picked()

        height: VLCStyle.dp(44, VLCStyle.scale)

        padding: VLCStyle.margin_small

        onClicked: picked()

        ColorContext {
            id: delegateTheme
            colorSet: ColorContext.Item
            hovered: delegate.hovered
            focused: delegate.visualFocus
            pressed: delegate.down
        }

        background: Rectangle {
            radius: VLCStyle.dp(4, VLCStyle.scale)
            color: delegate.hovered ? delegateTheme.bg.highlight : "transparent"
        }

        contentItem: RowLayout {
            spacing: VLCStyle.margin_small

            Widgets.IconLabel {
                text: delegate.iconText
                font.pixelSize: VLCStyle.icon_small
                color: delegate.current ? root.colorContext.accent : delegateTheme.fg.secondary
            }

            Widgets.ListLabel {
                Layout.fillWidth: true

                text: delegate.name
                color: delegateTheme.fg.primary
            }

            Widgets.ListLabel {
                visible: delegate.current
                text: delegate.currentText
                color: root.colorContext.accent
            }
        }
    }
}
