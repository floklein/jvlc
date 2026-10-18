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

// Figma "range" of the Adjustments sidebar (5187:24546, 1905:7366, 804:1285):
// the Settings slider's track, fill, ticks and thumb (SettingsSlider), with
// the sidebar's labels: none, the start, middle and end values aligned on the
// track's ends and middle (the delays, 10 px under the track), or one under
// each tick
FocusScope {
    id: root

    property real from: 0
    property real to: 1
    property real stepSize: 0.1
    property real value: 0
    property string accessibleName
    // read with the value by screen readers and in the tooltip
    property string accessibleValue: String(Math.round(slider.value * 100) / 100)
    // apply while dragging, or only on release
    property bool liveUpdate: true

    // the ticks: one per step, ten at most (-1); none on a range around 0
    // (the delays) unless given; the ends never have one
    property int intervals: -1

    // the start, middle and end values, under the ends and the middle
    property var edgeLabels: []
    // a label under each tick, spread as Figma's boxes (5187:24558)
    property var tickLabels: []
    // between the bottom of the 24 px slider and the labels
    property real labelGap: VLCStyle.spacing_10

    readonly property bool _labeled: edgeLabels.length > 0 || tickLabels.length > 0

    signal moved(real value)

    implicitWidth: VLCStyle.dp(320, VLCStyle.scale)
    implicitHeight: slider.implicitHeight + (_labeled ? labelGap + labels.implicitHeight : 0)

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

    // a range around 0, as the delays: filled from 0
    readonly property bool bipolar: from < 0 && to > 0
    readonly property real _zeroPosition: (0 - from) / (to - from)

    // Figma's "screen" blend of color over backdrop
    function _screen(source, backdrop) {
        return Qt.rgba(1 - (1 - source.r) * (1 - backdrop.r),
                       1 - (1 - source.g) * (1 - backdrop.g),
                       1 - (1 - source.b) * (1 - backdrop.b),
                       1)
    }

    T.Slider {
        id: slider

        width: parent.width
        implicitHeight: VLCStyle.dp(24, VLCStyle.scale)

        // the scope gives it the focus
        focus: true

        from: root.from
        to: root.to
        stepSize: root.stepSize
        snapMode: T.Slider.SnapAlways
        live: true

        // moving the slider breaks a plain binding, keep following the value
        Binding on value {
            value: root.value
            when: !slider.pressed
        }

        focusPolicy: Qt.StrongFocus

        Accessible.name: root.accessibleName
        Accessible.description: root.accessibleValue

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)

        onMoved: {
            if (root.liveUpdate || !slider.pressed)
                root.moved(slider.value)
        }
        onPressedChanged: {
            if (!slider.pressed && !root.liveUpdate)
                root.moved(slider.value)
        }

        T.ToolTip.visible: slider.pressed || slider.visualFocus
        T.ToolTip.text: root.accessibleValue

        // a BG/B2 track r6 16 px high, a BG/Accent Bright fill r4 inset 2 px,
        // ticks of BG/Accent Dark in the "screen" blend, an Accent/Dim thumb
        background: Rectangle {
            id: track

            readonly property real inset: VLCStyle.dp(2, VLCStyle.scale)
            readonly property real thumbCenter: slider.visualPosition * (slider.availableWidth - thumb.width)
                                                + thumb.width / 2
            readonly property real fillStart: root.bipolar ? root._zeroPosition * (slider.availableWidth - thumb.width)
                                                             + thumb.width / 2
                                                           : inset
            readonly property real fillEnd: Math.min(Math.max(thumbCenter + (root.bipolar ? 0 : VLCStyle.dp(1, VLCStyle.scale)),
                                                              inset),
                                                     width - inset)

            y: (slider.height - height) / 2
            width: slider.width
            height: VLCStyle.dp(16, VLCStyle.scale)
            radius: VLCStyle.radius_normal
            color: theme.bg.primary
            border.width: slider.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: theme.visualFocus

            Rectangle {
                id: fill

                x: Math.min(track.fillStart, track.fillEnd)
                y: track.inset
                height: parent.height - 2 * y
                width: Math.abs(track.fillEnd - track.fillStart)
                radius: VLCStyle.radius_small
                color: theme.bg.highlight
            }

            Repeater {
                id: ticks

                readonly property int steps: Math.round((slider.to - slider.from) / slider.stepSize)
                readonly property int count: root.intervals >= 0 ? root.intervals
                                             : (root.bipolar ? 0 : (steps > 0 && steps <= 10 ? steps : 10))

                model: Math.max(0, count - 1)

                Rectangle {
                    required property int index

                    // Figma "degrees" spans the track less 1.95 px on each side
                    readonly property real _margin: 1.95 * VLCStyle.scale
                    readonly property real _center: _margin + (index + 1) * (track.width - 2 * _margin) / ticks.count
                    readonly property bool _onFill: _center >= fill.x && _center <= fill.x + fill.width

                    x: _center - width / 2
                    y: (track.height - height) / 2
                    width: VLCStyle.dp(2, VLCStyle.scale)
                    height: VLCStyle.dp(8, VLCStyle.scale)
                    radius: width / 2
                    color: root._screen(theme.indicator, _onFill ? fill.color : track.color)
                }
            }
        }

        handle: Rectangle {
            id: thumb

            x: slider.visualPosition * (slider.availableWidth - width)
            y: (slider.height - height) / 2
            width: VLCStyle.dp(8, VLCStyle.scale)
            height: VLCStyle.dp(24, VLCStyle.scale)
            radius: VLCStyle.radius_small
            color: theme.fg.highlight
        }
    }

    Item {
        id: labels

        visible: root._labeled

        anchors.top: slider.bottom
        anchors.topMargin: root.labelGap
        width: parent.width
        implicitHeight: labelMetrics.height

        FontMetrics {
            id: labelMetrics
            font.pixelSize: VLCStyle.fontSize_small
            font.family: VLCStyle.fontFamily
        }

        // Figma 1905:7369: on the ends and the middle
        Repeater {
            model: root.edgeLabels

            Text {
                required property string modelData
                required property int index

                readonly property int _count: root.edgeLabels.length

                x: index === 0 ? 0
                   : (index === _count - 1 ? labels.width - width
                                           : Math.round((labels.width - width) / 2))
                text: modelData
                color: textTheme.fg.label
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
            }
        }

        // Figma 5187:24558: 18 px boxes at the ends (the text 2 px in from the
        // edge) and 26 px between them (the text centered), with equal gaps
        Repeater {
            model: root.tickLabels

            Text {
                required property string modelData
                required property int index

                readonly property int _count: root.tickLabels.length
                readonly property real _end: VLCStyle.dp(2, VLCStyle.scale)
                readonly property real _endBox: VLCStyle.dp(18, VLCStyle.scale)
                readonly property real _box: VLCStyle.dp(26, VLCStyle.scale)
                readonly property real _gap: (labels.width - 2 * _endBox - Math.max(0, _count - 2) * _box)
                                             / Math.max(1, _count - 1)
                readonly property real _center: _endBox + index * _gap + (index - 1) * _box + _box / 2

                x: index === 0 ? _end
                   : (index === _count - 1 ? labels.width - width - _end
                                           : Math.round(_center - width / 2))
                text: modelData
                color: textTheme.fg.label
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
            }
        }
    }
}
