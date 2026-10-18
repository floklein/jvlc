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

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

// A tab of the tab bar of the player's right-hand panels (Queue panel,
// Adjustments sidebar, Track menu): icon and label, an accent line when
// checked (Figma 463:833): the current tab's label FG/A2 and its icon two-tone
// (FG/A3 and Accent/Dim), the other tabs' labels FG/A3 and icons FG/A4
T.TabButton {
    id: tab

    // the monochrome glyph in VLCIcons
    property string iconName
    // the two-tone glyph shown when checked, without its "_tt_base" suffix
    property string twoToneName: iconName

    // the accent line's distance to the tab's edges (Figma 8 px, 7.5 px on
    // the fixed 85 px tabs of the Adjustments sidebar)
    property real lineInset: VLCStyle.spacing_8

    hoverEnabled: true

    topPadding: VLCStyle.spacing_14
    leftPadding: VLCStyle.spacing_8
    rightPadding: VLCStyle.spacing_8

    Accessible.role: Accessible.PageTab
    Accessible.name: text

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    readonly property ColorContext colorContext: ColorContext {
        id: tabTheme
        colorSet: ColorContext.TabButton

        focused: tab.visualFocus
        hovered: tab.hovered
        pressed: tab.down
    }

    background: Item {
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: tab.lineInset
            anchors.rightMargin: tab.lineInset

            height: VLCStyle.dp(2, VLCStyle.scale)

            // Figma: the top corners rounded by 1 px
            topLeftRadius: VLCStyle.dp(1, VLCStyle.scale)
            topRightRadius: VLCStyle.dp(1, VLCStyle.scale)

            color: tab.checked ? tabTheme.accent : Qt.alpha(tabTheme.accent, 0.0)
        }

        Rectangle {
            anchors.fill: parent
            radius: VLCStyle.radius_small
            color: Qt.alpha(tabTheme.accent, 0.0)
            border.width: VLCStyle.focus_border
            border.color: tab.visualFocus ? tabTheme.visualFocus : Qt.alpha(tabTheme.visualFocus, 0.0)
        }
    }

    contentItem: Item {
        // the label's full width, not the elided one: no binding loop with the tab's width
        implicitWidth: iconLoader.implicitWidth + tabRow.spacing + tabLabel.implicitWidth
        implicitHeight: tabRow.implicitHeight

        Row {
            id: tabRow

            anchors.horizontalCenter: parent.horizontalCenter

            spacing: VLCStyle.spacing_8

            Loader {
                id: iconLoader

                anchors.verticalCenter: parent.verticalCenter

                sourceComponent: tab.checked ? twoToneIcon : plainIcon

                Component {
                    id: twoToneIcon

                    Widgets.TwoToneIcon {
                        name: tab.twoToneName
                        pixelSize: VLCStyle.dp(20, VLCStyle.scale)
                        baseColor: tabTheme.fg.label
                        accentColor: tabTheme.fg.highlight
                    }
                }

                Component {
                    id: plainIcon

                    Widgets.IconLabel {
                        text: VLCIcons[tab.iconName]
                        color: tab.hovered ? tabTheme.fg.label : tabTheme.fg.muted
                        font.pixelSize: VLCStyle.dp(20, VLCStyle.scale)
                    }
                }
            }

            Widgets.ListLabel {
                id: tabLabel

                anchors.verticalCenter: parent.verticalCenter
                // Figma puts the 15 px line 2.5 px down the 20 px row: the half
                // pixel goes down, as Figma renders it
                anchors.verticalCenterOffset: 1

                // elided rather than spilling out of the tab (large scales)
                width: Math.max(0, Math.min(implicitWidth,
                                            tab.availableWidth - iconLoader.width - tabRow.spacing))

                text: tab.text
                font.pixelSize: VLCStyle.fontSize_small
                font.weight: Font.Normal
                color: (tab.checked || tab.hovered) ? tabTheme.fg.value : tabTheme.fg.label
            }
        }
    }
}
