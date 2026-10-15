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
import VLC.Dialogs
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

// A non-blocking notice for the last engine error, shown in a corner.
// "Details" opens Messages on the errors.
FocusScope {
    id: root

    readonly property bool shown: (state === "visible")

    implicitWidth: VLCStyle.dp(360, VLCStyle.scale)
    implicitHeight: card.implicitHeight

    function show() {
        state = "visible"
        hideTimer.restart()
        Helpers.announce(card, card.Accessible.name)
    }

    function dismiss() {
        hideTimer.stop()
        state = "hidden"
        DialogErrorModel.resetRepeatedMessageCount()
    }

    state: "hidden"
    states: [
        State {
            name: "visible"
            PropertyChanges { target: root; visible: true; opacity: 1.0 }
        },
        State {
            name: "hidden"
            PropertyChanges { target: root; visible: false; opacity: 0.0 }
        }
    ]
    transitions: [
        Transition {
            to: "visible"
            SequentialAnimation {
                PropertyAction { property: "visible" }
                NumberAnimation { property: "opacity"; duration: VLCStyle.duration_short; easing.type: Easing.OutCubic }
            }
        },
        Transition {
            to: "hidden"
            SequentialAnimation {
                NumberAnimation { property: "opacity"; duration: VLCStyle.duration_short; easing.type: Easing.OutCubic }
                PropertyAction { property: "visible" }
            }
        }
    ]

    Timer {
        id: hideTimer
        interval: 5000
        // keep the notice while the user reads it
        running: false
        onTriggered: {
            if (hover.hovered || root.activeFocus)
                restart()
            else
                root.dismiss()
        }
    }

    HoverHandler {
        id: hover
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    // the semantic colors are defined for views
    ColorContext {
        id: semantic
        palette: VLCStyle.palette
        colorSet: ColorContext.View
    }

    Rectangle {
        id: card

        width: root.width
        implicitHeight: layout.implicitHeight + VLCStyle.margin_normal * 2

        color: theme.bg.primary
        radius: VLCStyle.radius_large
        border.color: theme.border

        Widgets.RoundedRectangleShadow {
            yOffset: VLCStyle.dp(4, VLCStyle.scale)
            blurRadius: VLCStyle.dp(6, VLCStyle.scale)
            opacity: 0.25
            color: theme.shadow
        }

        Accessible.role: Accessible.AlertMessage
        Accessible.name: qsTr("Error: %1").arg(DialogErrorModel.notificationText)
        Accessible.description: DialogErrorModel.notificationDescription

        RowLayout {
            id: layout

            anchors.fill: parent
            anchors.margins: VLCStyle.margin_normal

            spacing: VLCStyle.margin_small

            Widgets.IconLabel {
                Layout.alignment: Qt.AlignTop

                text: VLCIcons.info
                color: semantic.fg.negative
                font.pixelSize: VLCStyle.icon_normal
            }

            ColumnLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_xxsmall

                RowLayout {
                    Layout.fillWidth: true

                    spacing: VLCStyle.margin_xsmall

                    T.Label {
                        Layout.fillWidth: true

                        text: DialogErrorModel.notificationText
                        color: theme.fg.primary
                        font.pixelSize: VLCStyle.fontSize_normal
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    // how many times this error happened in a row
                    T.Label {
                        Layout.alignment: Qt.AlignTop

                        visible: DialogErrorModel.repeatedMessageCount > 1
                        text: qsTr("×%1").arg(DialogErrorModel.repeatedMessageCount)
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                }

                T.Label {
                    Layout.fillWidth: true

                    visible: text.length > 0
                    text: DialogErrorModel.notificationDescription
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.topMargin: VLCStyle.margin_xxsmall

                    spacing: VLCStyle.margin_xsmall

                    Widgets.ButtonExt {
                        id: detailsButton

                        text: qsTr("Details")
                        focus: true

                        Navigation.parentItem: root
                        Navigation.rightItem: dismissButton

                        onClicked: {
                            root.dismiss()
                            DialogsProvider.messagesDialog(1)
                        }
                    }

                    Widgets.ButtonExt {
                        id: dismissButton

                        text: qsTr("Dismiss")

                        Navigation.parentItem: root
                        Navigation.leftItem: detailsButton

                        onClicked: root.dismiss()
                    }
                }
            }
        }
    }
}
