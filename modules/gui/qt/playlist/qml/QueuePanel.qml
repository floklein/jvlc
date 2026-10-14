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
import VLC.Playlist
import VLC.Style
import VLC.Util

// The player's right-hand panel: Queue, Bookmarks and Chapters tabs
// (Figma 491:1781, 6320:13763, 6321:14024)
T.Pane {
    id: root

    enum Tab {
        Queue,
        Bookmarks,
        Chapters
    }

    property int currentTab: QueuePanel.Tab.Queue

    readonly property PlaylistPane queuePane: queuePane

    padding: 0

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Queue panel")

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // Functions

    function showTab(tab) {
        root.currentTab = tab
        tabBar.itemAt(tab)?.forceActiveFocus(Qt.TabFocusReason)
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

                    model: [
                        { tab: QueuePanel.Tab.Queue, icon: "queue", text: qsTr("Queue") },
                        { tab: QueuePanel.Tab.Bookmarks, icon: "bookmarks", text: qsTr("Bookmarks") },
                        { tab: QueuePanel.Tab.Chapters, icon: "chapters", text: qsTr("Chapters") }
                    ]

                    delegate: Widgets.PanelTab {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        iconName: modelData.icon
                        text: modelData.text
                        checked: root.currentTab === modelData.tab

                        focus: checked

                        onClicked: root.currentTab = modelData.tab

                        Navigation.parentItem: root
                        Navigation.leftItem: index > 0 ? tabBar.itemAt(index - 1) : null
                        Navigation.rightItem: index < tabBar.count - 1 ? tabBar.itemAt(index + 1) : null
                        Navigation.downItem: stack.children[root.currentTab] ?? null
                    }
                }
            }
        }

        StackLayout {
            id: stack

            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: root.currentTab

            PlaylistPane {
                id: queuePane

                showTitle: false
                useAcrylic: false
                focus: root.currentTab === QueuePanel.Tab.Queue

                padding: 0

                // the panel draws the background
                background: Rectangle {
                    color: Qt.alpha(theme.bg.primary, 0.0)
                }

                Navigation.parentItem: root
            }

            Loader {
                active: MainCtx.mediaLibraryAvailable
                focus: root.currentTab === QueuePanel.Tab.Bookmarks

                source: "qrc:///qt/qml/VLC/MediaLibrary/BookmarksPane.qml"

                onLoaded: item.Navigation.parentItem = root

                // without the media library there is nowhere to keep bookmarks
                T.Label {
                    anchors.centerIn: parent

                    visible: !MainCtx.mediaLibraryAvailable

                    text: qsTr("Bookmarks need the media library")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                }
            }

            ChapterList {
                focus: root.currentTab === QueuePanel.Tab.Chapters

                Navigation.parentItem: root
            }
        }
    }

    // The Chapters tab: number, title and start time of each chapter
    component ChapterList : FocusScope {
        id: chapterScope

        readonly property ColorContext colorContext: ColorContext {
            id: chapterListTheme
            colorSet: ColorContext.View
        }

        ListView {
            id: chapterList

            anchors.fill: parent
            anchors.topMargin: VLCStyle.spacing_10

            focus: true
            clip: true

            model: Player.chapters

            Keys.onPressed: (event) => {
                if (KeyHelper.matchOk(event) && currentIndex >= 0) {
                    Player.chapters.selectChapter(currentIndex)
                    event.accepted = true
                    return
                }
                Navigation.defaultKeyAction(event)
            }

            Navigation.parentItem: chapterScope

            delegate: T.ItemDelegate {
                id: chapterDelegate

                required property int index
                required property var model

                // "display" and "checked" are buttons' own properties
                readonly property string title: model.display ?? ""
                readonly property bool isCurrentChapter: !!model.checked
                // the first chapter starts at 0, which VLCTime holds as invalid
                readonly property string timeText: (model.time !== undefined && model.time.valid())
                                                   ? model.time.formatHMS() : "00:00"

                width: ListView.view.width

                leftPadding: VLCStyle.spacing_20
                rightPadding: VLCStyle.spacing_20
                topPadding: VLCStyle.spacing_10
                bottomPadding: VLCStyle.spacing_10

                implicitHeight: implicitContentHeight + topPadding + bottomPadding

                hoverEnabled: true

                Accessible.name: qsTr("Chapter %1, %2, %3").arg(index + 1).arg(title).arg(timeText)

                readonly property ColorContext colorContext: ColorContext {
                    id: chapterTheme
                    colorSet: ColorContext.Item

                    focused: chapterDelegate.activeFocus
                    hovered: chapterDelegate.hovered
                }

                background: Widgets.AnimatedBackground {
                    enabled: chapterTheme.initialized
                    color: chapterDelegate.ListView.isCurrentItem && chapterList.activeFocus
                           ? chapterTheme.bg.highlight : chapterTheme.bg.primary
                    radius: VLCStyle.radius_small
                    border.color: chapterDelegate.visualFocus ? chapterTheme.visualFocus : Qt.alpha(chapterTheme.visualFocus, 0.0)
                }

                contentItem: RowLayout {
                    spacing: VLCStyle.spacing_10

                    Widgets.ListLabel {
                        Layout.preferredWidth: VLCStyle.dp(30, VLCStyle.scale)

                        font.weight: Font.Normal
                        text: chapterDelegate.index + 1
                        color: chapterTheme.accent
                    }

                    Widgets.ListLabel {
                        Layout.fillWidth: true

                        font.weight: Font.Normal
                        text: chapterDelegate.title || qsTr("Chapter %1").arg(chapterDelegate.index + 1)
                        color: chapterDelegate.isCurrentChapter ? chapterTheme.accent : chapterTheme.fg.secondary
                    }

                    Widgets.ListLabel {
                        font.weight: Font.Normal
                        text: chapterDelegate.timeText
                        color: chapterDelegate.isCurrentChapter ? chapterTheme.accent : chapterTheme.fg.secondary
                    }
                }

                onClicked: {
                    chapterList.currentIndex = index
                    Player.chapters.selectChapter(index)
                }
            }

            T.Label {
                anchors.centerIn: parent

                visible: chapterList.count === 0

                text: qsTr("No chapters")
                color: chapterListTheme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_large
            }
        }
    }
}
