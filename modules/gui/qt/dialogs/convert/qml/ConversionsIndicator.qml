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
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs

// The Conversions indicator of the Top bar: the progress of the background
// conversions, with Cancel. Shown while there are conversions to report.
Widgets.IconToolButton {
    id: root

    // emitted while the list is open, so that the bars stay visible
    signal requestLockUnlockAutoHide(bool lock)

    readonly property bool active: Conversions.activeCount > 0

    visible: Conversions.count > 0

    font.pixelSize: VLCStyle.icon_banner
    width: VLCStyle.bannerButton_width
    height: VLCStyle.bannerButton_height

    text: active ? VLCIcons.ic_fluent_arrow_sync_24_regular : VLCIcons.check
    description: active
        ? qsTr("Conversions: %1%").arg(Math.round(Conversions.progress * 100))
        : qsTr("Conversions finished")

    checked: popup.visible

    onClicked: popup.visible ? popup.close() : popup.open()

    function stateText(state, progress, destination) {
        switch (state) {
        case Conversions.Pending:
            return qsTr("Waiting")
        case Conversions.Running:
            return progress > 0 ? qsTr("Converting… %1%").arg(Math.round(progress * 100))
                                : qsTr("Converting…")
        case Conversions.Done:
            return qsTr("Saved as %1").arg(destination.replace(/^.*[\\\/]/, ""))
        case Conversions.Failed:
            return qsTr("Failed")
        case Conversions.Cancelled:
            return qsTr("Cancelled")
        }
        return ""
    }

    // overall progress, under the icon
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: VLCStyle.margin_xxxsmall

        visible: root.active
        width: parent.width * 0.6
        height: VLCStyle.heightBar_xxxsmall
        radius: height / 2
        color: root.colorContext.bg.secondary

        Rectangle {
            width: parent.width * Conversions.progress
            height: parent.height
            radius: parent.radius
            color: root.colorContext.accent
        }
    }

    Popup {
        id: popup

        x: root.width - width
        y: root.height + VLCStyle.margin_xxxsmall

        width: VLCStyle.dp(340, VLCStyle.scale)
        padding: VLCStyle.margin_small

        // stay inside the window
        margins: MainCtx.windowExtendedMargin

        onOpened: root.requestLockUnlockAutoHide(true)
        onClosed: root.requestLockUnlockAutoHide(false)

        background: Rectangle {
            radius: VLCStyle.dp(8, VLCStyle.scale)
            color: popupTheme.bg.primary
            border.color: popupTheme.border
            border.width: VLCStyle.border
        }

        contentItem: ColumnLayout {
            spacing: VLCStyle.margin_xsmall

            // the popup is out of the Top bar's item tree: give its
            // controls the palette
            readonly property ColorContext colorContext: ColorContext {
                id: popupTheme
                palette: root.colorContext.palette
                colorSet: ColorContext.Window
            }

            Widgets.SubtitleLabel {
                font.pixelSize: VLCStyle.fontSize_large
                text: qsTr("Conversions")
                color: popupTheme.fg.primary
            }

            ListView {
                id: list

                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, VLCStyle.dp(300, VLCStyle.scale))

                clip: true
                spacing: VLCStyle.margin_xxsmall
                model: Conversions

                ScrollBar.vertical: Widgets.ScrollBarExt { }

                delegate: RowLayout {
                    id: row

                    required property int index
                    required property string name
                    required property string destination
                    required property int status
                    required property double progress

                    readonly property bool active: status === Conversions.Pending
                                                   || status === Conversions.Running

                    width: list.width
                    spacing: VLCStyle.margin_xsmall

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Widgets.ListLabel {
                            Layout.fillWidth: true

                            text: row.name
                            elide: Text.ElideMiddle
                            color: popupTheme.fg.primary
                        }

                        Widgets.CaptionLabel {
                            Layout.fillWidth: true

                            text: root.stateText(row.status, row.progress, row.destination)
                            elide: Text.ElideRight
                            color: row.status === Conversions.Failed ? popupTheme.fg.negative
                                                                    : popupTheme.fg.secondary
                        }

                        Widgets.ProgressBarExt {
                            Layout.fillWidth: true

                            visible: row.status === Conversions.Running
                            padding: 0
                            background: null
                            from: 0
                            to: 1
                            value: row.progress
                            indeterminate: row.progress <= 0
                        }
                    }

                    Widgets.IconToolButton {
                        visible: row.active

                        text: VLCIcons.close
                        description: qsTr("Cancel")
                        font.pixelSize: VLCStyle.icon_small

                        onClicked: Conversions.cancel(row.index)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                spacing: VLCStyle.margin_small

                Item { Layout.fillWidth: true }

                Widgets.ButtonExt {
                    visible: Conversions.count > Conversions.activeCount
                    text: qsTr("Clear")

                    onClicked: {
                        Conversions.clearFinished()
                        if (Conversions.count === 0)
                            popup.close()
                    }
                }

                Widgets.ButtonExt {
                    visible: root.active
                    text: qsTr("Cancel All")

                    onClicked: Conversions.cancelAll()
                }
            }
        }
    }
}
