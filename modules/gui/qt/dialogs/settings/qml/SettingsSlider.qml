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

// a range with ticks and its start, middle and end values below
Item {
    id: root

    property real from: 0
    property real to: 1
    property real stepSize: 0.1
    property real value: 0
    // the value is multiplied by scale for display in unit
    property real scale: 1
    property string unit
    property string accessibleName
    // apply while dragging, or only on release
    property bool liveUpdate: true
    // a label under each tick instead of the start, middle and end values
    property var tickLabels: []

    signal moved(real value)

    implicitWidth: VLCStyle.dp(320, VLCStyle.scale)
    implicitHeight: slider.implicitHeight + VLCStyle.spacing_8 + labels.implicitHeight

    function format(v: real) : string {
        if (root.unit === "%")
            return Math.round(v * root.scale) + "%"
        return Math.round(v * root.scale * 100) / 100 + " " + root.unit
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Slider
        enabled: slider.enabled
        focused: slider.visualFocus
        hovered: slider.hovered
    }

    readonly property ColorContext textContext: ColorContext {
        id: textTheme
        colorSet: ColorContext.View
    }

    T.Slider {
        id: slider

        width: parent.width
        implicitHeight: VLCStyle.dp(24, VLCStyle.scale)

        from: root.from
        to: root.to
        stepSize: root.stepSize
        snapMode: T.Slider.SnapAlways
        live: true

        // moving the slider breaks a plain binding, keep following the setting
        Binding on value {
            value: root.value
            when: !slider.pressed
        }

        focusPolicy: Qt.StrongFocus

        Accessible.name: root.accessibleName
        Accessible.description: root.format(slider.value)

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

        onMoved: {
            if (root.liveUpdate || !slider.pressed)
                root.moved(slider.value)
        }
        onPressedChanged: {
            if (!slider.pressed && !root.liveUpdate)
                root.moved(slider.value)
        }

        T.ToolTip.visible: slider.pressed || slider.visualFocus
        T.ToolTip.text: root.format(slider.value)

        background: Rectangle {
            id: track

            y: (slider.height - height) / 2
            width: slider.width
            height: VLCStyle.dp(16, VLCStyle.scale)
            radius: VLCStyle.radius_xxsmall
            color: theme.bg.primary
            border.width: slider.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: theme.visualFocus

            Rectangle {
                x: VLCStyle.dp(2, VLCStyle.scale)
                y: VLCStyle.dp(2, VLCStyle.scale)
                height: parent.height - 2 * y
                width: slider.visualPosition * (parent.width - 2 * x)
                radius: VLCStyle.radius_xxsmall
                color: theme.bg.highlight
            }

            // one tick per step, or ten when there are too many steps
            Repeater {
                id: ticks

                readonly property int steps: Math.round((slider.to - slider.from) / slider.stepSize)
                model: steps > 0 && steps <= 40 ? steps + 1 : 11

                Rectangle {
                    required property int index

                    x: VLCStyle.dp(2, VLCStyle.scale)
                       + index * (track.width - VLCStyle.dp(4, VLCStyle.scale)) / (ticks.count - 1)
                       - width / 2
                    y: (track.height - height) / 2
                    width: VLCStyle.border
                    height: VLCStyle.dp(6, VLCStyle.scale)
                    color: theme.bg.secondary
                }
            }
        }

        handle: Rectangle {
            x: slider.visualPosition * (slider.availableWidth - width)
            y: (slider.height - height) / 2
            width: VLCStyle.dp(8, VLCStyle.scale)
            height: VLCStyle.dp(24, VLCStyle.scale)
            radius: VLCStyle.radius_xxsmall
            color: theme.fg.primary
        }
    }

    Item {
        id: labels

        anchors.top: slider.bottom
        anchors.topMargin: VLCStyle.spacing_4
        width: parent.width
        implicitHeight: startLabel.implicitHeight

        Text {
            id: startLabel
            visible: root.tickLabels.length === 0
            text: root.format(root.from)
            color: textTheme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
            font.family: VLCStyle.fontFamily
        }

        Text {
            visible: root.tickLabels.length === 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.format((root.from + root.to) / 2)
            color: textTheme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
            font.family: VLCStyle.fontFamily
        }

        Text {
            visible: root.tickLabels.length === 0
            anchors.right: parent.right
            text: root.format(root.to)
            color: textTheme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
            font.family: VLCStyle.fontFamily
        }

        // centered under their tick, the first and last ones flush with the ends
        Repeater {
            model: root.tickLabels

            Text {
                required property string modelData
                required property int index

                readonly property real _tickX: VLCStyle.dp(2, VLCStyle.scale)
                                               + index * (labels.width - VLCStyle.dp(4, VLCStyle.scale))
                                                 / Math.max(1, root.tickLabels.length - 1)

                x: index === 0 ? 0
                   : (index === root.tickLabels.length - 1 ? labels.width - width
                                                           : _tickX - width / 2)
                text: modelData
                color: textTheme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
            }
        }
    }
}
