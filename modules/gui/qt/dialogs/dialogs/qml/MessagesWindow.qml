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
import VLC.Style
import VLC.Dialogs

WindowDialog {
    id: root

    title: qsTr("Messages")

    width: VLCStyle.dp(760, VLCStyle.scale)
    height: VLCStyle.dp(520, VLCStyle.scale)
    minimumWidth: VLCStyle.dp(480, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(320, VLCStyle.scale)

    standardButtons: T.DialogButtonBox.Close

    contentComponent: ColumnLayout {
        id: content

        spacing: VLCStyle.margin_small

        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.View
        }

        RowLayout {
            Layout.fillWidth: true

            spacing: VLCStyle.margin_xsmall

            // level filter: each level includes the previous ones
            Row {
                id: levelBox

                spacing: VLCStyle.margin_xxxsmall

                Accessible.role: Accessible.Grouping
                Accessible.name: qsTr("Level")

                Repeater {
                    // in the order of MessagesModel.Verbosity
                    model: [ qsTr("Errors"), qsTr("Warnings"), qsTr("Info"), qsTr("Debug") ]

                    delegate: Widgets.ButtonExt {
                        required property int index
                        required property string modelData

                        text: modelData
                        checked: MessagesModel.verbosity === index

                        Navigation.parentItem: content
                        Navigation.downItem: listView

                        onClicked: MessagesModel.verbosity = index
                    }
                }
            }

            Widgets.TextFieldExt {
                id: searchField

                Layout.fillWidth: true

                placeholderText: qsTr("Search")
                text: MessagesModel.searchPattern
                focus: true

                onTextEdited: MessagesModel.searchPattern = text

                onActiveFocusChanged: MainCtx.useGlobalShortcuts = !activeFocus

                Accessible.name: qsTr("Search messages")

                Navigation.parentItem: content
                Navigation.leftItem: levelBox
                Navigation.rightItem: copyButton
                Navigation.downItem: listView
            }

            Widgets.ButtonExt {
                id: copyButton

                text: qsTr("Copy")
                enabled: MessagesModel.count > 0

                Navigation.parentItem: content
                Navigation.leftItem: searchField
                Navigation.rightItem: saveButton
                Navigation.downItem: listView

                onClicked: MessagesModel.copy()
            }

            Widgets.ButtonExt {
                id: saveButton

                text: qsTr("Save…")
                enabled: MessagesModel.count > 0

                Navigation.parentItem: content
                Navigation.leftItem: copyButton
                Navigation.rightItem: clearButton
                Navigation.downItem: listView

                onClicked: DialogsProvider.saveMessages()
            }

            Widgets.ButtonExt {
                id: clearButton

                text: qsTr("Clear")

                Navigation.parentItem: content
                Navigation.leftItem: saveButton
                Navigation.downItem: listView

                onClicked: MessagesModel.clear()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true

            color: theme.bg.secondary
            radius: VLCStyle.radius_normal

            ListView {
                id: listView

                // follow new messages while the end of the log is shown
                property bool followEnd: true

                anchors.fill: parent
                anchors.margins: VLCStyle.margin_xsmall

                clip: true
                model: MessagesModel
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true

                T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                onMovementEnded: followEnd = atYEnd
                onCountChanged: {
                    if (followEnd)
                        Qt.callLater(listView.positionViewAtEnd)
                }
                Component.onCompleted: positionViewAtEnd()

                Navigation.parentItem: content
                Navigation.upItem: searchField

                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                delegate: T.Label {
                    required property int type
                    required property string line

                    width: ListView.view.width - VLCStyle.margin_small

                    text: line
                    wrapMode: Text.WrapAnywhere
                    font.family: VLCStyle.fontFamily_mono
                    font.pixelSize: VLCStyle.fontSize_small
                    color: {
                        // VLC_MSG_INFO 0, VLC_MSG_ERR 1, VLC_MSG_WARN 2, VLC_MSG_DBG 3
                        switch (type) {
                        case 1:
                            return theme.fg.negative
                        case 2:
                            return theme.fg.neutral
                        case 3:
                            return theme.fg.secondary
                        default:
                            return theme.fg.primary
                        }
                    }
                }
            }

            T.Label {
                anchors.centerIn: parent

                visible: listView.count === 0
                text: MessagesModel.searchPattern.length > 0 ? qsTr("No message matches the search")
                                                             : qsTr("No message")
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
            }
        }
    }
}
