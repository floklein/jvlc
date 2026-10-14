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
import QtQuick.Layouts
import QtQml.Models
import QtQuick.Templates as T


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util
import VLC.PlayerControls

// The editing area of Customize Player: the zones of the Top bar and of the
// Control bar, with the seek bar on its own row between them when the Layout
// does not place it inline, and the palette of the items not placed yet.
Item {
    id: root

    // the edited player (video or music)
    property int identifier: PlayerControlbarModel.Videoplayer

    readonly property PlayerControlbarModel bars: MainCtx.controlbarProfileModel.currentModel?.getModel(identifier) ?? null

    property bool dragActive: !!_viewThatContainsDrag || buttonDragItem.Drag.active

    property alias removeInfoRectVisible: buttonList.removeInfoRectVisible

    property EditorDNDView _viewThatContainsDrag: null

    // the zone Enter adds palette items to: the last zone selected
    property var _targetZone: null

    signal dragStarted(int controlId)
    signal dragStopped(int controlId)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    onBarsChanged: _targetZone = null

    function _views() {
        const views = []
        for (const repeater of [topZones, controlZones]) {
            for (let i = 0; i < repeater.count; ++i) {
                const item = repeater.itemAt(i)
                if (item)
                    views.push(item.view)
            }
        }
        return views
    }

    // selects an item, with the keyboard focus
    function select(zone, index) {
        for (const view of _views()) {
            if (view.model === zone) {
                view.currentIndex = index
                view.positionViewAtIndex(index, ListView.Contain)
                view.forceActiveFocus(Qt.TabFocusReason)
                return
            }
        }
    }

    // adds a palette item to the bars and selects it
    function add(control) {
        if (!bars)
            return

        let zone = _targetZone
        if (control === ControlListModel.SEEK_BAR)
            zone = bars.center
        else if (!zone || !bars.canPlace(control, zone))
            zone = bars.right

        if (bars.addControl(zone, zone.count, control))
            select(zone, zone.count - 1)
    }

    // One zone: its items in a dashed box
    component Zone: Item {
        id: zone

        property ControlListModel zoneModel: null
        property string placeholder

        readonly property alias view: dndView

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 1
        Layout.preferredHeight: VLCStyle.dp(48, VLCStyle.scale)

        EditorDashedBorder {
            anchors.fill: parent

            color: dndView.activeFocus ? theme.visualFocus : theme.border
            lineWidth: dndView.activeFocus ? VLCStyle.focus_border : VLCStyle.border
        }

        Widgets.MenuCaption {
            anchors.centerIn: parent

            visible: dndView.count === 0
            text: zone.placeholder
            color: theme.fg.secondary
        }

        EditorDNDView {
            id: dndView

            anchors.fill: parent
            anchors.margins: VLCStyle.margin_xxsmall

            model: zone.zoneModel
            bars: root.bars

            // the center zones keep their natural width
            extraWidthAvailable: zone.zoneModel !== root.bars?.center
                                 && zone.zoneModel !== root.bars?.topCenter

            Accessible.name: zone.placeholder

            onActiveFocusChanged: {
                if (activeFocus)
                    root._targetZone = model
            }

            onSelectRequested: (zone, index) => root.select(zone, index)

            onContainsDragChanged: {
                if (containsDrag)
                    root._viewThatContainsDrag = this
                else if (root._viewThatContainsDrag === this)
                    root._viewThatContainsDrag = null
            }
        }
    }

    // One bar: its name and its three zones
    component Bar: ColumnLayout {
        id: bar

        property string title
        property var zones: []
        property alias repeater: zoneRepeater

        spacing: VLCStyle.margin_xxsmall

        Widgets.SubtitleLabel {
            text: bar.title
            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: VLCStyle.dp(48, VLCStyle.scale)
            Layout.maximumHeight: VLCStyle.dp(48, VLCStyle.scale)

            spacing: VLCStyle.margin_xsmall

            Repeater {
                id: zoneRepeater

                model: bar.zones.length

                delegate: Zone {
                    required property int index

                    zoneModel: bar.zones[index] ?? null
                    placeholder: [qsTr("Left"), qsTr("Center"), qsTr("Right")][index]
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent

        spacing: VLCStyle.margin_small

        Bar {
            id: topBar

            Layout.fillWidth: true

            Layout.fillHeight: false

            title: qsTr("Top bar")
            zones: root.bars ? [root.bars.topLeft, root.bars.topCenter, root.bars.topRight] : []
        }

        // the seek bar, on its own row above the Control bar
        ColumnLayout {
            Layout.fillWidth: true

            visible: !!root.bars && !root.bars.seekBarInline

            spacing: VLCStyle.margin_xxsmall

            Widgets.SubtitleLabel {
                text: qsTr("Seek bar row")
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.dp(40, VLCStyle.scale)

                EditorDashedBorder {
                    anchors.fill: parent
                    color: theme.border
                }

                SeekBarWidget {
                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_xxsmall

                    paintOnly: true
                    enabled: false
                }
            }
        }

        Bar {
            id: controlBar

            Layout.fillWidth: true

            Layout.fillHeight: false

            title: qsTr("Control bar")
            zones: root.bars ? [root.bars.left, root.bars.center, root.bars.right] : []
        }

        Widgets.MenuCaption {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.margin_xsmall

            text: qsTr("Drag items around and out of the box below to re-order them. Drag items into the box to remove them from the player.")
            wrapMode: Text.WordWrap
            color: theme.fg.secondary
        }

        Widgets.MenuCaption {
            Layout.fillWidth: true

            text: qsTr("Keyboard: select an item and press Enter to add or remove it, Alt+Left or Alt+Right to move it.")
            wrapMode: Text.WordWrap
            color: theme.fg.secondary
        }

        ToolbarEditorButtonList {
            id: buttonList

            Layout.fillWidth: true
            Layout.fillHeight: true

            bars: root.bars

            onAddRequested: (id) => root.add(id)

            Component.onCompleted: {
                controlDragStarted.connect(root.dragStarted)
                controlDragStopped.connect(root.dragStopped)
            }
        }
    }

    readonly property Repeater topZones: topBar.repeater
    readonly property Repeater controlZones: controlBar.repeater

    EditorDummyButton {
        id: buttonDragItem

        visible: Drag.active
        color: theme.fg.primary
        opacity: 0.75

        x: -1
        y: -1

        Drag.onActiveChanged: {
            dragAutoScrollHandler.dragItem = Drag.active ? this : null
        }
    }

    ViewDragAutoScrollHandler {
        id: dragAutoScrollHandler

        view: _viewThatContainsDrag ?? null
    }
}
