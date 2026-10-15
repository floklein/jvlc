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

WindowDialog {
    id: root

    readonly property string helpUrl: "https://docs.videolan.me/vlc-user/"
    readonly property string donateUrl: "https://www.videolan.org/contribute.html#money"

    title: qsTr("About jVLC media player")

    width: VLCStyle.dp(560, VLCStyle.scale)
    height: VLCStyle.dp(600, VLCStyle.scale)
    minimumWidth: VLCStyle.dp(420, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(420, VLCStyle.scale)

    standardButtons: T.DialogButtonBox.Close

    contentComponent: ColumnLayout {
        id: content

        // 0: authors, 1: credits, 2: license
        property int page: 0

        spacing: VLCStyle.margin_normal

        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.View
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_small

            spacing: VLCStyle.margin_normal

            Image {
                Layout.preferredWidth: VLCStyle.icon_large
                Layout.preferredHeight: VLCStyle.icon_large

                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                source: MainCtx.useXmasCone() ? "qrc:///logo/vlc48-xmas.png"
                                              : "qrc:///misc/cone.svg"
            }

            ColumnLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_xxxsmall

                T.Label {
                    Layout.fillWidth: true

                    text: qsTr("jVLC media player")
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_xxlarge
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight

                    Accessible.role: Accessible.Heading
                }

                T.Label {
                    Layout.fillWidth: true

                    text: AboutModel.version
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    wrapMode: Text.Wrap
                }
            }
        }

        T.Label {
            Layout.fillWidth: true

            text: qsTr("jVLC media player is a free and open source media player, encoder and streamer, "
                       + "based on VLC media player by the volunteers of the VideoLAN community.")
            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true

            spacing: VLCStyle.margin_small

            Widgets.ButtonExt {
                id: helpButton

                text: qsTr("Online help")
                focus: true

                Navigation.parentItem: content
                Navigation.rightItem: donateButton
                Navigation.downItem: authorsTab

                onClicked: Qt.openUrlExternally(root.helpUrl)
            }

            Widgets.ActionButtonPrimary {
                id: donateButton

                text: qsTr("Donate to VideoLAN")

                Navigation.parentItem: content
                Navigation.leftItem: helpButton
                Navigation.downItem: authorsTab

                onClicked: Qt.openUrlExternally(root.donateUrl)
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true

            spacing: VLCStyle.margin_xsmall

            Widgets.ButtonExt {
                id: authorsTab

                text: qsTr("Authors")
                checked: content.page === 0

                Navigation.parentItem: content
                Navigation.upItem: helpButton
                Navigation.rightItem: creditsTab
                Navigation.downItem: pageText

                onClicked: content.page = 0
            }

            Widgets.ButtonExt {
                id: creditsTab

                text: qsTr("Credits")
                checked: content.page === 1

                Navigation.parentItem: content
                Navigation.upItem: helpButton
                Navigation.leftItem: authorsTab
                Navigation.rightItem: licenseTab
                Navigation.downItem: pageText

                onClicked: content.page = 1
            }

            Widgets.ButtonExt {
                id: licenseTab

                text: qsTr("License")
                checked: content.page === 2

                Navigation.parentItem: content
                Navigation.upItem: helpButton
                Navigation.leftItem: creditsTab
                Navigation.downItem: pageText

                onClicked: content.page = 2
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true

            color: theme.bg.secondary
            radius: VLCStyle.radius_normal

            Flickable {
                id: flickable

                anchors.fill: parent
                anchors.margins: VLCStyle.margin_small

                clip: true
                contentWidth: width
                contentHeight: pageText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                TextEdit {
                    id: pageText

                    width: flickable.width - VLCStyle.margin_small

                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText

                    color: theme.fg.primary
                    selectionColor: theme.bg.highlight
                    selectedTextColor: theme.fg.highlight
                    font.pixelSize: VLCStyle.fontSize_small
                    font.family: (content.page === 2) ? VLCStyle.fontFamily_mono : VLCStyle.fontFamily

                    text: {
                        switch (content.page) {
                        case 1:
                            return AboutModel.thanks
                        case 2:
                            return AboutModel.license
                        default:
                            return AboutModel.authors
                        }
                    }

                    onTextChanged: flickable.contentY = 0

                    Navigation.parentItem: content
                    Navigation.upItem: authorsTab

                    Accessible.role: Accessible.StaticText
                    Accessible.name: [authorsTab.text, creditsTab.text, licenseTab.text][content.page]
                }
            }
        }
    }
}
