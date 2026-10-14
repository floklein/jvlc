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

// The Adjustments sidebar (Figma 2238:9432): Playback, Video, Audio and
// Subtitles tabs in the player's right-hand slot. Changes apply live.
T.Pane {
    id: root

    // which tabs are shown and which one is current
    required property RightPanelFSM fsm

    padding: 0

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Adjustments")

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // Functions

    function showTab() {
        tabBar.itemAt(root.fsm.tabs.indexOf(root.fsm.currentTab))?.forceActiveFocus(Qt.TabFocusReason)
    }

    function _tabInfo(tab) {
        switch (tab) {
        case RightPanelFSM.Playback:
            return { icon: "playback", twoTone: "playback_selected", text: qsTr("Playback") }
        case RightPanelFSM.Video:
            return { icon: "video", twoTone: "video", text: qsTr("Video") }
        case RightPanelFSM.Audio:
            return { icon: "audio", twoTone: "audio", text: qsTr("Audio") }
        default:
            return { icon: "subtitles", twoTone: "subtitles", text: qsTr("Subtitles") }
        }
    }

    contentItem: ColumnLayout {
        spacing: VLCStyle.border

        // Figma "Frame 59": the tab bar
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: VLCStyle.dp(50, VLCStyle.scale)

            color: theme.bg.secondary
            radius: VLCStyle.radius_xxsmall

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.spacing_6
                anchors.rightMargin: VLCStyle.spacing_6

                spacing: VLCStyle.border

                Repeater {
                    id: tabBar

                    model: root.fsm.tabs

                    delegate: Widgets.PanelTab {
                        required property int modelData
                        required property int index

                        readonly property var _info: root._tabInfo(modelData)

                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        iconName: _info.icon
                        twoToneName: _info.twoTone
                        text: _info.text
                        checked: root.fsm.currentTab === modelData

                        focus: checked

                        onClicked: root.fsm.selectTab(modelData)

                        Navigation.parentItem: root
                        Navigation.leftItem: index > 0 ? tabBar.itemAt(index - 1) : null
                        Navigation.rightItem: index < tabBar.count - 1 ? tabBar.itemAt(index + 1) : null
                        Navigation.downItem: tabContent.item
                    }
                }
            }
        }

        Loader {
            id: tabContent

            Layout.fillWidth: true
            Layout.fillHeight: true

            focus: true

            sourceComponent: root.fsm.currentTab === RightPanelFSM.Playback ? playbackTab
                                                                             : pendingTab

            Component {
                id: playbackTab

                AdjustmentsPlaybackTab {
                    Navigation.parentItem: root
                    Navigation.upItem: tabBar.itemAt(root.fsm.tabs.indexOf(root.fsm.currentTab))
                }
            }

            // The Video, Audio and Subtitles tabs are built by later tickets
            // (#51, #52, #53): until then they open the old dialog
            Component {
                id: pendingTab

                FocusScope {
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: VLCStyle.spacing_20

                        spacing: VLCStyle.spacing_14

                        T.Label {
                            Layout.fillWidth: true

                            text: qsTr("These adjustments are still in the Adjustments and Effects dialog.")
                            wrapMode: Text.WordWrap
                            color: theme.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.family: VLCStyle.fontFamily
                        }

                        Widgets.ButtonExt {
                            id: dialogButton

                            focus: true

                            text: qsTr("Open Adjustments and Effects")

                            Navigation.parentItem: root
                            Navigation.upItem: tabBar.itemAt(root.fsm.tabs.indexOf(root.fsm.currentTab))

                            // the dialog's tabs: 0 audio, 1 video, 2 synchronization
                            onClicked: {
                                switch (root.fsm.currentTab) {
                                case RightPanelFSM.Video:
                                    DialogsProvider.showExtendedDialogTab(1)
                                    break
                                case RightPanelFSM.Audio:
                                    DialogsProvider.showExtendedDialogTab(0)
                                    break
                                default:
                                    DialogsProvider.showExtendedDialogTab(2)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
