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
import VLC.MediaLibrary
import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

// The Bookmarks tab of the Queue panel: the bookmarks of the current media
// (Figma 6320:13763)
FocusScope {
    id: root

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    MLBookmarkModel {
        id: bookmarkModel

        player: Player
        ml: MediaLib
    }

    ColumnLayout {
        anchors.fill: parent

        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.dp(28, VLCStyle.scale)
            Layout.rightMargin: VLCStyle.spacing_20
            Layout.topMargin: VLCStyle.spacing_20
            Layout.bottomMargin: VLCStyle.spacing_20

            spacing: VLCStyle.spacing_12

            Widgets.ListLabel {
                Layout.fillWidth: true

                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                font.weight: Font.Normal

                text: (bookmarkList.count === 1) ? qsTr("1 Bookmark")
                                                 : qsTr("%1 Bookmarks").arg(bookmarkList.count)
            }

            Widgets.IconToolButton {
                id: removeButton

                font.pixelSize: VLCStyle.icon_normal

                enabled: bookmarkList.currentIndex >= 0 && bookmarkList.count > 0
                text: VLCIcons.remove
                description: qsTr("Remove bookmark")

                onClicked: bookmarkModel.removeBookmarks([bookmarkList.currentIndex])

                Navigation.parentItem: root
                Navigation.rightItem: addButton
                Navigation.downItem: bookmarkList
            }

            Widgets.IconToolButton {
                id: addButton

                font.pixelSize: VLCStyle.icon_normal

                enabled: Player.isStarted
                text: VLCIcons.add
                description: qsTr("Add bookmark")
                shortcutAction: "add-bookmark"

                // the same action as the Add Bookmark key and menu entries
                onClicked: MainCtx.addBookmark()

                Navigation.parentItem: root
                Navigation.leftItem: removeButton
                Navigation.downItem: bookmarkList
            }
        }

        ListView {
            id: bookmarkList

            // Figma 6320:13811: 20 px aside, 32 px rows 12 px apart
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: VLCStyle.spacing_20
            Layout.rightMargin: VLCStyle.spacing_20

            spacing: VLCStyle.spacing_12

            focus: true
            clip: true

            model: bookmarkModel

            currentIndex: -1

            Keys.onDeletePressed: {
                if (currentIndex >= 0)
                    bookmarkModel.removeBookmarks([currentIndex])
            }

            Keys.onPressed: (event) => {
                if (KeyHelper.matchOk(event) && currentIndex >= 0) {
                    bookmarkModel.select(bookmarkModel.index(currentIndex, 0))
                    event.accepted = true
                    return
                }
                Navigation.defaultKeyAction(event)
            }

            Navigation.parentItem: root
            Navigation.upItem: addButton

            delegate: T.ItemDelegate {
                id: bookmarkDelegate

                required property int index
                required property string name
                required property string time

                width: ListView.view.width

                rightPadding: VLCStyle.spacing_4

                implicitHeight: Math.max(VLCStyle.dp(32, VLCStyle.scale),
                                         implicitContentHeight + topPadding + bottomPadding)

                hoverEnabled: true

                readonly property bool isCurrent: ListView.isCurrentItem

                Accessible.name: qsTr("%1, %2").arg(name).arg(time)

                readonly property ColorContext colorContext: ColorContext {
                    id: itemTheme
                    colorSet: ColorContext.Item

                    focused: bookmarkDelegate.activeFocus
                    hovered: bookmarkDelegate.hovered
                }

                background: Widgets.AnimatedBackground {
                    enabled: itemTheme.initialized
                    color: bookmarkDelegate.isCurrent ? itemTheme.bg.highlight : itemTheme.bg.primary
                    radius: VLCStyle.radius_small
                    border.color: bookmarkDelegate.visualFocus ? itemTheme.visualFocus : Qt.alpha(itemTheme.visualFocus, 0.0)
                }

                contentItem: RowLayout {
                    spacing: VLCStyle.spacing_10

                    Widgets.ListLabel {
                        Layout.fillWidth: true

                        font.weight: Font.Normal
                        text: bookmarkDelegate.name || qsTr("Bookmark at %1").arg(bookmarkDelegate.time)
                        color: itemTheme.fg.secondary
                    }

                    Widgets.ListLabel {
                        font.weight: Font.Normal
                        text: bookmarkDelegate.time
                        color: itemTheme.fg.secondary
                    }
                }

                onClicked: {
                    bookmarkList.currentIndex = index
                    bookmarkModel.select(bookmarkModel.index(index, 0))
                }
            }

            Column {
                anchors.centerIn: parent

                visible: bookmarkList.count === 0

                spacing: VLCStyle.margin_small

                T.Label {
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: qsTr("No bookmarks")
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_large
                }

                T.Label {
                    anchors.horizontalCenter: parent.horizontalCenter

                    text: qsTr("Press %1 or + to add one").arg(MainCtx.keyBindingsRevision >= 0 ? MainCtx.actionShortcut("add-bookmark") : "")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                }
            }
        }
    }
}
