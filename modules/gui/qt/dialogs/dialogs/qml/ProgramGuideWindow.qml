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
import VLC.Player

WindowDialog {
    id: root

    title: qsTr("Program Guide")

    width: VLCStyle.dp(640, VLCStyle.scale)
    height: VLCStyle.dp(560, VLCStyle.scale)
    minimumWidth: VLCStyle.dp(400, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(360, VLCStyle.scale)

    standardButtons: T.DialogButtonBox.Close

    contentComponent: ColumnLayout {
        id: content

        spacing: VLCStyle.margin_small

        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.View
        }

        function timeRange(start, end) {
            const now = new Date()
            const startText = (start.toDateString() === now.toDateString())
                ? start.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                : start.toLocaleString(Qt.locale(), Locale.ShortFormat)
            return qsTr("%1 – %2").arg(startText).arg(end.toLocaleTimeString(Qt.locale(), Locale.ShortFormat))
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true

            color: theme.bg.secondary
            radius: VLCStyle.radius_normal

            ListView {
                id: listView

                anchors.fill: parent
                anchors.margins: VLCStyle.margin_xsmall

                clip: true
                model: ProgramGuideModel
                boundsBehavior: Flickable.StopAtBounds
                focus: true
                keyNavigationEnabled: true
                highlightMoveDuration: 0

                T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                section.property: "channel"
                section.criteria: ViewSection.FullString
                section.delegate: T.Label {
                    required property string section

                    width: ListView.view.width
                    topPadding: VLCStyle.margin_small
                    bottomPadding: VLCStyle.margin_xxsmall
                    leftPadding: VLCStyle.margin_xsmall

                    text: section
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_large
                    font.weight: Font.DemiBold

                    Accessible.role: Accessible.Heading
                }

                delegate: T.ItemDelegate {
                    id: eventDelegate

                    required property int index
                    required property string name
                    required property string description
                    required property string channel
                    required property int programId
                    required property date start
                    required property date end
                    required property bool current

                    width: ListView.view.width - VLCStyle.margin_small
                    padding: VLCStyle.margin_xsmall
                    highlighted: ListView.isCurrentItem

                    readonly property ColorContext colorContext: ColorContext {
                        id: itemTheme
                        colorSet: ColorContext.Item
                        hovered: eventDelegate.hovered
                        focused: eventDelegate.activeFocus
                    }

                    background: Rectangle {
                        radius: VLCStyle.radius_small
                        color: eventDelegate.highlighted ? itemTheme.bg.highlight
                                                         : (eventDelegate.hovered ? itemTheme.bg.secondary
                                                                                  : "transparent")
                    }

                    contentItem: RowLayout {
                        spacing: VLCStyle.margin_small

                        T.Label {
                            Layout.preferredWidth: VLCStyle.dp(150, VLCStyle.scale)

                            text: content.timeRange(eventDelegate.start, eventDelegate.end)
                            color: eventDelegate.highlighted ? itemTheme.fg.highlight : itemTheme.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_small
                            elide: Text.ElideRight
                        }

                        T.Label {
                            Layout.fillWidth: true

                            text: eventDelegate.name
                            color: eventDelegate.highlighted ? itemTheme.fg.highlight : itemTheme.fg.primary
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.weight: eventDelegate.current ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }

                        T.Label {
                            visible: eventDelegate.current
                            text: qsTr("On air")
                            color: theme.accent
                            font.pixelSize: VLCStyle.fontSize_small
                            font.weight: Font.DemiBold
                        }
                    }

                    Accessible.name: eventDelegate.current
                                     ? qsTr("%1, %2, on air").arg(eventDelegate.name).arg(content.timeRange(eventDelegate.start, eventDelegate.end))
                                     : qsTr("%1, %2").arg(eventDelegate.name).arg(content.timeRange(eventDelegate.start, eventDelegate.end))

                    onClicked: listView.currentIndex = index
                    onDoubleClicked: Player.changeProgram(programId)
                }

                Keys.onReturnPressed: {
                    if (currentItem)
                        Player.changeProgram(currentItem.programId)
                }
            }

            T.Label {
                anchors.centerIn: parent
                width: parent.width - VLCStyle.margin_large * 2

                visible: listView.count === 0
                text: qsTr("No program guide for this media")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
            }
        }

        ColumnLayout {
            Layout.fillWidth: true

            visible: !!listView.currentItem
            spacing: VLCStyle.margin_xxsmall

            RowLayout {
                Layout.fillWidth: true

                T.Label {
                    Layout.fillWidth: true

                    text: listView.currentItem?.name ?? ""
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_large
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }

                Widgets.ActionButtonPrimary {
                    text: qsTr("Watch this channel")

                    onClicked: Player.changeProgram(listView.currentItem.programId)
                }
            }

            T.Label {
                Layout.fillWidth: true
                Layout.maximumHeight: VLCStyle.dp(120, VLCStyle.scale)

                text: listView.currentItem?.description ?? ""
                visible: text.length > 0
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }
        }
    }
}
