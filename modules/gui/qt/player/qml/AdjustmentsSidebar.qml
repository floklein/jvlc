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
import VLC.Style
import VLC.Util
import VLC.Dialogs

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
        Player.resetVideoView()
        Player.audioAdjustments.resetAll()
        if (Player.rate !== 1)
            Player.rate = 1
        // the subtitle synchronization; the look is a preference
        Player.subtitleDelayMS = 0
        if (Player.hasSecondarySubtitle)
            Player.secondarySubtitleDelayMS = 0
        Player.subtitleDurationFactor = 0
        // the subtitle file's own speed
        if (Player.subtitleFPS !== 0)
            Player.subtitleFPS = 0
    }

    function _tabInfo(tab) {
        switch (tab) {
        case RightPanelFSM.Playback:
            // Figma 5187:24312: the outline of the two-tone glyph when not current
            return { icon: "playback_selected_tt_base", twoTone: "playback_selected", text: qsTr("Playback") }
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
            // the panel's r10 top corners (Figma 5187:24309 is square, the
            // panel clips it)
            topLeftRadius: VLCStyle.radius_large
            topRightRadius: VLCStyle.radius_large

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

                        // Figma 5187:24309: Playback and Subtitles share the room
                        // (107.5 px), Video and Audio are 85 px with 12 px aside
                        readonly property bool _fixed: modelData === RightPanelFSM.Video
                                                       || modelData === RightPanelFSM.Audio

                        Layout.fillWidth: !_fixed
                        // the flexible ones grow from 0: equal whatever their labels
                        Layout.preferredWidth: _fixed ? VLCStyle.dp(85, VLCStyle.scale) : 0
                        Layout.fillHeight: true

                        leftPadding: _fixed ? VLCStyle.spacing_12 : VLCStyle.spacing_8
                        rightPadding: leftPadding
                        lineInset: _fixed ? 7.5 * VLCStyle.scale : VLCStyle.spacing_8

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

        // Figma "Frame 68" (812:1580): the footer of the Video, Audio and
        // Subtitles tabs (Playback has none), over a 1 px Divider/Dim line:
        // the Preset of the playing file's folder (with Reset All in its
        // list) and Open Advanced Settings, on the tab's Settings page
        Rectangle {
            id: footerBox

            visible: root.fsm.currentTab !== RightPanelFSM.Playback

            Layout.fillWidth: true
            implicitHeight: footer.implicitHeight + VLCStyle.border + 2 * VLCStyle.spacing_20

            // BG/B1 Transparent over the panel
            color: Qt.alpha(theme.bg.primary, 0.95)
            // the panel's r10 bottom corners
            bottomLeftRadius: VLCStyle.radius_large
            bottomRightRadius: VLCStyle.radius_large

            Rectangle {
                width: parent.width
                height: VLCStyle.border
                color: theme.separator
            }

            ColumnLayout {
                id: footer

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: VLCStyle.spacing_20
                anchors.rightMargin: VLCStyle.spacing_20
                anchors.topMargin: VLCStyle.border + VLCStyle.spacing_20

                spacing: VLCStyle.spacing_10

                PresetSelector {
                    id: presetSelector

                    Layout.fillWidth: true

                    onResetAllRequested: root.resetAll()

                    Navigation.parentItem: root
                    Navigation.upItem: tabContent.item
                    Navigation.downItem: advancedButton
                }

                // Figma 812:1585: a gear and the label, FG/A3, centered
                T.Button {
                    id: advancedButton

                    Layout.fillWidth: true
                    implicitHeight: VLCStyle.dp(32, VLCStyle.scale)

                    text: qsTr("Open Advanced Settings")

                    Accessible.name: text

                    // the Settings page of the tab, its Advanced Options open
                    onClicked: {
                        switch (root.fsm.currentTab) {
                        case RightPanelFSM.Video:
                            DialogsProvider.showSettings("video", true)
                            break
                        case RightPanelFSM.Audio:
                            DialogsProvider.showSettings("audio", true)
                            break
                        default:
                            DialogsProvider.showSettings("subtitles", true)
                        }
                    }

                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                    Navigation.parentItem: root
                    Navigation.upItem: presetSelector

                    background: Rectangle {
                        radius: VLCStyle.radius_small
                        color: advancedButton.hovered || advancedButton.down ? theme.bg.highlight
                                                                             : "transparent"
                        border.width: VLCStyle.focus_border
                        border.color: advancedButton.visualFocus ? theme.visualFocus : "transparent"
                    }

                    contentItem: Item {
                        implicitHeight: advancedRow.implicitHeight

                        Row {
                            id: advancedRow

                            anchors.centerIn: parent

                            spacing: VLCStyle.spacing_6

                            Widgets.IconLabel {
                                anchors.verticalCenter: parent.verticalCenter

                                text: VLCIcons.settings
                                font.pixelSize: VLCStyle.dp(20, VLCStyle.scale)
                                color: advancedLabel.color
                            }

                            Widgets.LabelExt {
                                id: advancedLabel

                                anchors.verticalCenter: parent.verticalCenter

                                text: advancedButton.text
                                font.pixelSize: VLCStyle.fontSize_normal
                                color: advancedButton.hovered ? theme.fg.value : theme.fg.label
                            }
                        }
                    }
                }
            }
        }
    }
}
