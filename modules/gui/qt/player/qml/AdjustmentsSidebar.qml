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

    // every tab back to its defaults, except the default languages
    function resetAll() {
        Player.videoAdjustments.resetAll()
        Player.audioAdjustments.resetAll()
        if (Player.rate !== 1)
            Player.rate = 1
        // the subtitle synchronization; the look is a preference
        Player.subtitleDelayMS = 0
        if (Player.hasSecondarySubtitle)
            Player.secondarySubtitleDelayMS = 0
        Player.subtitleDurationFactor = 0
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

            sourceComponent: {
                switch (root.fsm.currentTab) {
                case RightPanelFSM.Playback:
                    return playbackTab
                case RightPanelFSM.Video:
                    return videoTab
                case RightPanelFSM.Audio:
                    return audioTab
                default:
                    return subtitlesTab
                }
            }

            Component {
                id: playbackTab

                AdjustmentsPlaybackTab {
                    Navigation.parentItem: root
                    Navigation.upItem: tabBar.itemAt(root.fsm.tabs.indexOf(root.fsm.currentTab))
                }
            }

            Component {
                id: videoTab

                AdjustmentsVideoTab {
                    Navigation.parentItem: root
                    Navigation.upItem: tabBar.itemAt(root.fsm.tabs.indexOf(root.fsm.currentTab))
                }
            }

            Component {
                id: audioTab

                AdjustmentsAudioTab {
                    Navigation.parentItem: root
                    Navigation.upItem: tabBar.itemAt(root.fsm.tabs.indexOf(root.fsm.currentTab))
                }
            }

            Component {
                id: subtitlesTab

                AdjustmentsSubtitlesTab {
                    Navigation.parentItem: root
                    Navigation.upItem: tabBar.itemAt(root.fsm.tabs.indexOf(root.fsm.currentTab))
                }
            }
        }

        // Figma "Frame 68": the footer (the Preset selector comes with #56)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: resetAllButton.implicitHeight + 2 * VLCStyle.spacing_10

            color: theme.bg.secondary
            radius: VLCStyle.radius_xxsmall

            Widgets.TextToolButton {
                id: resetAllButton

                anchors.centerIn: parent

                text: qsTr("Reset All Adjustments")

                onClicked: root.resetAll()

                Navigation.parentItem: root
                Navigation.upItem: tabContent.item
            }
        }
    }
}
