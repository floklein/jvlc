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
import VLC.Style

// a few choices side by side: text segments, or color swatches when the
// choices have a "color"
FocusScope {
    id: root

    property var choices: []
    property var value
    property string accessibleName

    readonly property bool swatches: choices.length > 0 && choices[0].color !== undefined

    signal activated(var value)

    implicitWidth: VLCStyle.dp(320, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(36, VLCStyle.scale)

    // text segments never truncate: the control is at least as wide as its
    // labels, and the segments share the room equally when they can
    readonly property real _margin: VLCStyle.dp(2, VLCStyle.scale)
    readonly property real _spacing: root.swatches ? VLCStyle.spacing_4 : VLCStyle.dp(2, VLCStyle.scale)
    readonly property real _labelPadding: VLCStyle.spacing_6

    function _segmentWidth(label) {
        return Math.ceil(fontMetrics.advanceWidth(label)) + 2 * _labelPadding
    }

    readonly property real _widestSegment: {
        let widest = 0
        for (let i = 0; i < choices.length; ++i)
            widest = Math.max(widest, _segmentWidth(choices[i].label))
        return widest
    }

    Layout.minimumWidth: {
        if (swatches || choices.length === 0)
            return -1
        let width = 2 * _margin + (choices.length - 1) * _spacing
        for (let i = 0; i < choices.length; ++i)
            width += _segmentWidth(choices[i].label)
        return width
    }

    FontMetrics {
        id: fontMetrics

        font.pixelSize: VLCStyle.fontSize_normal
        font.family: VLCStyle.fontFamily
    }

    Accessible.role: Accessible.PageTabList
    Accessible.name: accessibleName

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Switch
    }

    readonly property ColorContext textContext: ColorContext {
        id: textTheme
        colorSet: ColorContext.View
    }

    Rectangle {
        anchors.fill: parent
        radius: VLCStyle.radius_normal
        color: theme.bg.primary
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root._margin
        spacing: root._spacing

        Repeater {
            model: root.choices

            delegate: T.AbstractButton {
                id: segment

                required property var modelData
                required property int index

                readonly property bool selected: modelData.value === root.value

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: root.swatches ? -1 : root._segmentWidth(modelData.label)
                Layout.preferredWidth: root.swatches ? -1 : root._widestSegment

                focus: selected
                focusPolicy: Qt.StrongFocus
                checkable: false
                checked: selected

                text: modelData.label

                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                Accessible.checkable: true
                Accessible.checked: selected
                Accessible.onPressAction: root.activated(modelData.value)

                T.ToolTip.visible: (root.swatches || segmentLabel.truncated) && (hovered || visualFocus)
                T.ToolTip.delay: VLCStyle.delayToolTipAppear
                T.ToolTip.text: modelData.label

                Keys.onLeftPressed: (event) => {
                    if (index > 0)
                        root.activated(root.choices[index - 1].value)
                }
                Keys.onRightPressed: (event) => {
                    if (index < root.choices.length - 1)
                        root.activated(root.choices[index + 1].value)
                }
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                onClicked: root.activated(modelData.value)

                background: Rectangle {
                    radius: VLCStyle.radius_small
                    color: segment.selected ? theme.bg.secondary
                                            : (root.swatches ? "transparent" : theme.bg.highlight)
                    border.width: segment.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                    border.color: textTheme.accent
                }

                contentItem: Item {
                    Text {
                        id: segmentLabel

                        visible: !root.swatches
                        anchors.horizontalCenter: parent.horizontalCenter
                        // centered, a half pixel rounded down as Figma does
                        y: Math.ceil((parent.height - height) / 2)
                        // elided rather than spilling out of the segment (large scales)
                        width: Math.max(0, Math.min(implicitWidth, parent.width - 2 * VLCStyle.spacing_4))
                        elide: Text.ElideRight
                        text: segment.text
                        font.pixelSize: VLCStyle.fontSize_normal
                        font.family: VLCStyle.fontFamily
                        // Figma 3046:9870: Accent/Main when picked, FG/A3 otherwise
                        color: segment.selected ? theme.accent
                                                : (segment.hovered ? textTheme.fg.value : textTheme.fg.label)
                    }

                    Rectangle {
                        visible: root.swatches
                        anchors.fill: parent
                        anchors.margins: VLCStyle.spacing_6
                        radius: VLCStyle.dp(1, VLCStyle.scale)
                        color: root.swatches ? segment.modelData.color : "transparent"
                        opacity: segment.selected || segment.hovered ? 1.0 : 0.7
                    }
                }
            }
        }
    }
}
