/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Window


import VLC.MainInterface
import VLC.Style

// The window buttons drawn by VLC: minimize, maximize and close, with the
// optional fullscreen button next to maximize (Figma 247:464)
Row {
    id: windowButtonGroup

    spacing: 0
    padding: 0

    width: implicitWidth

    property color color: theme.fg.primary
    property color hoverColor: theme.bg.primary.alpha(0.5)

    // the player's Top bar puts its fullscreen button among the window buttons
    property bool showFullscreen: false
    property bool fullscreenChecked: false
    property string fullscreenShortcut: ""

    signal fullscreenClicked()

    readonly property bool buttonHovered: {
        let h = false
        for (let i = 0; i < repeater.count; ++i) {
            const button = repeater.itemAt(i)
            h = h || button.anyHovered
        }

        return h
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Repeater {
        id: repeater

        model: MainCtx.csdButtonModel.windowCSDButtons

        Row {
            id: buttonSlot

            required property var modelData

            readonly property bool anyHovered: windowButton.hovered || windowButton.showHovered
                                               || fullscreenButton.hovered

            height: windowButtonGroup.height

            // the fullscreen button sits between maximize and close
            CSDWindowButton {
                id: fullscreenButton

                height: windowButtonGroup.height

                visible: windowButtonGroup.showFullscreen && (buttonSlot.modelData.type === CSDButton.Close)

                focusPolicy: Qt.TabFocus

                color: windowButtonGroup.color
                hoverColor: windowButtonGroup.hoverColor
                isThemeDark: theme.palette.isDark

                iconTxt: windowButtonGroup.fullscreenChecked ? VLCIcons.fullscreen_active : VLCIcons.fullscreen
                description: windowButtonGroup.fullscreenChecked ? qsTr("Exit fullscreen") : qsTr("Fullscreen")
                shortcut: windowButtonGroup.fullscreenShortcut

                onClicked: windowButtonGroup.fullscreenClicked()
            }

            CSDWindowButton {
                id: windowButton

                height: windowButtonGroup.height

                showHovered: buttonSlot.modelData.showHovered

                externalPressed: buttonSlot.modelData.externalPressed

                color: (buttonSlot.modelData.type === CSDButton.Close && (hovered || buttonSlot.modelData.showHovered))
                       ? theme.fg.negative
                       : windowButtonGroup.color

                hoverColor: (buttonSlot.modelData.type === CSDButton.Close) ? theme.bg.negative : windowButtonGroup.hoverColor

                isThemeDark: theme.palette.isDark

                iconTxt: {
                    switch (buttonSlot.modelData.type) {
                    case CSDButton.Minimize:
                        return VLCIcons.minimize

                    case CSDButton.MaximizeRestore:
                        return (MainCtx.intfMainWindow.visibility === Window.Maximized)
                                ? VLCIcons.window_restore
                                : VLCIcons.maximize

                    case CSDButton.Close:
                        return VLCIcons.close
                    }
                    console.assert(false, "unreachable")
                    return ""
                }

                description: {
                    switch (buttonSlot.modelData.type) {
                    case CSDButton.Minimize:
                        return qsTr("Minimize")
                    case CSDButton.MaximizeRestore:
                        return (MainCtx.intfMainWindow.visibility === Window.Maximized) ? qsTr("Restore")
                                                                                        : qsTr("Maximize")
                    case CSDButton.Close:
                        return qsTr("Close")
                    }
                    return ""
                }

                onClicked: buttonSlot.modelData.click()

                // handles VLCStyle.scale changes
                onWidthChanged: Qt.callLater(updateRect)
                onHeightChanged: Qt.callLater(updateRect)
                onXChanged: Qt.callLater(updateRect)

                Connections {
                    target: VLCStyle

                    // handle window resize
                    function onAppWidthChanged() { Qt.callLater(windowButton.updateRect) }
                    function onAppHeightChanged() { Qt.callLater(windowButton.updateRect) }
                }

                function updateRect() {
                    const point = mapToItem(null, 0, 0)
                    const rect = Qt.rect(point.x, point.y, width, height)

                    buttonSlot.modelData.rect = rect
                }
            }
        }
    }
}
