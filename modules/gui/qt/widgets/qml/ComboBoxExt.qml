/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Controls.Basic // Popup, ScrollIndicator


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

// Figma's "Dropdown" (2372:8300, 3046:9863): a 2 px BG/B2 frame round a BG/A4
// option, r6 and r4, no stroke; 14 px FG/A2 text and a 24 px chevron, both
// 16 px from the edges. Open, the list covers the control (2372:8301).
T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + spacing + implicitIndicatorWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    // the frame, then the option's own padding
    readonly property real frame: VLCStyle.dp(2, VLCStyle.scale)

    padding: frame
    leftPadding: frame + VLCStyle.spacing_14
    rightPadding: frame + VLCStyle.spacing_14
    spacing: VLCStyle.spacing_10

    font.pixelSize: VLCStyle.fontSize_normal
    font.family: VLCStyle.fontFamily
    font.weight: Font.Normal

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ComboBox

        focused: control.activeFocus
        enabled: control.enabled
        hovered: control.hovered
        pressed: control.pressed
    }

    property color color: theme.fg.primary
    // the frame
    property color bgColor: theme.bg.primary
    // the option inside the frame
    property color fieldColor: control.hovered ? theme.bg.highlight : theme.bg.secondary
    // NOTE: Figma's pickers have no stroke; it only draws the focus ring
    property color borderColor: "transparent"

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    delegate: T.ItemDelegate {
        id: delegateItem

        width: ListView.view ? ListView.view.width : control.width
        // as tall as the closed option
        height: Math.max(control.height - 2 * control.frame, implicitContentHeight + topPadding + bottomPadding)
        leftPadding: VLCStyle.spacing_14
        rightPadding: VLCStyle.spacing_14
        spacing: VLCStyle.spacing_10

        readonly property bool current: control.currentIndex === index

        contentItem: Item {
            implicitHeight: delegateLabel.implicitHeight

            Widgets.ListLabel {
                id: delegateLabel

                anchors.left: parent.left
                anchors.right: check.visible ? check.left : parent.right
                anchors.rightMargin: check.visible ? delegateItem.spacing : 0
                anchors.verticalCenter: parent.verticalCenter

                text: control.textRole ? (Helpers.isArray(control.model) ? modelData[control.textRole] : model[control.textRole]) : modelData
                color: control.color
                font.pixelSize: control.font.pixelSize
                font.family: control.font.family
                font.weight: control.font.weight
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            // the current option
            Widgets.IconLabel {
                id: check

                visible: delegateItem.current
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: VLCStyle.icon_normal
                text: VLCIcons.check
                color: theme.accent
            }
        }

        background: Rectangle {
            radius: VLCStyle.radius_small
            color: delegateItem.current || control.highlightedIndex === index ? theme.bg.highlight : "transparent"
        }

        highlighted: control.highlightedIndex === index
    }

    indicator: Widgets.IconLabel {
        x: control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        font.pixelSize: VLCStyle.icon_normal
        text: VLCIcons.chevron_down
        color: control.color
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    contentItem: Widgets.ListLabel {
        rightPadding: control.spacing + control.indicator.width

        text: control.displayText
        color: control.color
        font.pixelSize: control.font.pixelSize
        font.family: control.font.family
        font.weight: control.font.weight
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: VLCStyle.dp(320, VLCStyle.scale)
        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)

        radius: VLCStyle.radius_normal
        color: control.bgColor
        // the accent ring of the keyboard focus, as on the other controls
        border.width: control.visualFocus ? VLCStyle.focus_border : 0
        border.color: control.visualFocus ? theme.visualFocus : control.borderColor

        Widgets.AnimatedBackground {
            anchors.fill: parent
            anchors.margins: control.frame

            enabled: theme.initialized
            radius: VLCStyle.radius_small
            color: control.fieldColor
            border.width: 0
        }
    }

    popup: Popup {
        // over the control, as Figma's open list
        y: 0

        // NOTE: This Popup should be on top of other Popup(s) most of the time.
        z: 100

        width: control.width
        padding: control.frame

        // stay in the window: a list taller than the window scrolls
        topMargin: VLCStyle.margin_xsmall
        bottomMargin: VLCStyle.margin_xsmall
        height: Math.min(implicitHeight, control.Window.height - topMargin - bottomMargin)

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            spacing: control.frame
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: theme.bg.secondary
            radius: VLCStyle.radius_normal

            // Figma: 0, 4, 13.8 at 35 %
            Widgets.RoundedRectangleShadow {
                yOffset: VLCStyle.dp(4, VLCStyle.scale)
                // a Figma blur radius is twice the sigma the shader takes
                blurRadius: 13.8 * VLCStyle.scale / 2
                color: Qt.rgba(0, 0, 0, 0.35)
            }
        }
    }
}
