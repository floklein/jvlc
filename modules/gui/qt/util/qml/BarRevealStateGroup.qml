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

/**
 * Shows and hides a bar following its `state` ("visible" or "hidden").
 *
 * The bar slides out through its `edge` while fading. With `reducedMotion` it
 * only fades. Without `animate` the change is instant.
 *
 * The slide is a transform: add `translate` to the bar's `transform`, its
 * geometry does not change.
 */
StateGroup {
    id: root

    required property Item target

    // Qt.TopEdge or Qt.BottomEdge
    property int edge: Qt.TopEdge

    property bool animate: true
    property bool reducedMotion: false

    property int duration: 150

    readonly property Translate translate: Translate { }

    readonly property real _hiddenOffset: {
        if (root.reducedMotion)
            return 0
        return (root.edge === Qt.TopEdge) ? -root.target.height : root.target.height
    }

    state: target?.state ?? ""

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: root.target

                visible: false
                opacity: 0.0
            }

            PropertyChanges {
                target: root.translate

                y: root._hiddenOffset
            }
        },
        State {
            name: "visible"

            PropertyChanges {
                target: root.target

                visible: true
                opacity: 1.0
            }

            PropertyChanges {
                target: root.translate

                y: 0
            }
        }
    ]

    // cubic-bezier(0.23, 1, 0.32, 1)
    readonly property var _easing: [0.23, 1, 0.32, 1, 1, 1]

    transitions: [
        Transition {
            to: "hidden"

            enabled: root.animate

            SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation {
                        target: root.target
                        property: "opacity"
                        duration: root.duration
                        easing.type: Easing.BezierSpline
                        easing.bezierCurve: root._easing
                    }

                    NumberAnimation {
                        target: root.translate
                        property: "y"
                        duration: root.duration
                        easing.type: Easing.BezierSpline
                        easing.bezierCurve: root._easing
                    }
                }

                PropertyAction {
                    target: root.target
                    property: "visible"
                }
            }
        },
        Transition {
            to: "visible"

            enabled: root.animate

            SequentialAnimation {
                PropertyAction {
                    target: root.target
                    property: "visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: root.target
                        property: "opacity"
                        duration: root.duration
                        easing.type: Easing.BezierSpline
                        easing.bezierCurve: root._easing
                    }

                    NumberAnimation {
                        target: root.translate
                        property: "y"
                        duration: root.duration
                        easing.type: Easing.BezierSpline
                        easing.bezierCurve: root._easing
                    }
                }
            }
        }
    ]
}
