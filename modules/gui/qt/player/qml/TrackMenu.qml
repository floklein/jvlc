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
import VLC.Player
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Menus
import VLC.Dialogs

// The Track menu: a popover with Video, Audio and Subtitles tabs, opened by
// the audio and subtitles buttons of the Control bar (Figma 491:2216,
// 496:2495, 498:2674). Picking a track applies it and keeps the menu open.
T.Popup {
    id: root

    // Properties

    readonly property int tab: fsm.tab

    // the room the popover may take
    property real maximumWidth: VLCStyle.dp(400, VLCStyle.scale)
    property real maximumHeight: VLCStyle.dp(524, VLCStyle.scale)

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.View
    }

    // Private

    property bool _removeMode: false

    // Signals

    // "More settings": open the Adjustments sidebar on this tab
    signal requestAdjustments(int tab)

    // Settings

    width: Math.min(VLCStyle.dp(400, VLCStyle.scale), maximumWidth)
    height: Math.min(VLCStyle.dp(524, VLCStyle.scale), maximumHeight)

    padding: 0

    focus: true

    // NOTE: it closes on release so that a press on the button of the open tab
    //       can tell the FSM that its click closes the menu
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnReleaseOutside

    // Functions

    // the bar buttons call these with their tab
    function buttonPressed(tab) {
        fsm.buttonPressed(tab)
    }

    function buttonClicked(tab) {
        fsm.buttonClicked(tab)
    }

    // the VLC menu opens it when the audio and subtitles buttons are folded
    function openOnTab(tab) {
        if (fsm.opened)
            fsm.selectTab(tab)
        else
            fsm.buttonClicked(tab)
    }

    // Events

    // the devices plugged in since the last time
    onAboutToShow: Player.audioDevices.refresh()

    onClosed: {
        root._removeMode = false

        if (fsm.opened)
            fsm.dismiss()
    }

    // Children

    TrackMenuFSM {
        id: fsm

        onOpenedChanged: {
            if (fsm.opened)
                root.open()
            else
                root.close()
        }

        onRequestAdjustments: (tab) => root.requestAdjustments(tab)
    }

    StringListMenu {
        id: trackContextMenu

        property var tracks: null
        property int row: -1
        property bool secondary: false

        ctx: MainCtx

        onSelected: (index, str) => {
            if (secondary)
                tracks.unselectSecondary()
            else
                tracks.selectSecondary(row)

            fsm.trackPicked()
        }

        function show(tracks, row, secondary, selected, globalPos) {
            // the main subtitle can't be its own second subtitle
            if (selected && !secondary)
                return

            trackContextMenu.tracks = tracks
            trackContextMenu.row = row
            trackContextMenu.secondary = secondary
            popup(globalPos, [secondary ? qsTr("Hide second subtitle")
                                        : qsTr("Show as second subtitle")])
        }
    }

    background: Widgets.TintBackground {
        radius: VLCStyle.radius_large

        colorContext: theme
    }

    contentItem: ColumnLayout {
        spacing: 0

        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Track menu")

        // the Tab bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: VLCStyle.dp(50, VLCStyle.scale)

            radius: VLCStyle.radius_large

            color: theme.bg.secondary

            // square bottom corners
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                height: parent.radius

                color: parent.color
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.dp(6, VLCStyle.scale)
                anchors.rightMargin: VLCStyle.dp(6, VLCStyle.scale)

                spacing: VLCStyle.dp(1, VLCStyle.scale)

                TabButton {
                    tab: TrackMenuFSM.Video
                    glyph: VLCIcons.video
                    text: qsTr("Video")
                }

                TabButton {
                    tab: TrackMenuFSM.Audio
                    glyph: VLCIcons.audio
                    text: qsTr("Audio")
                }

                TabButton {
                    tab: TrackMenuFSM.Subtitles
                    glyph: VLCIcons.subtitles
                    text: qsTr("Subtitles")
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: fsm.tab

            // Video
            Page {
                Section {
                    title: qsTr("Embedded Tracks")
                }

                TrackList {
                    tracks: Player.videoTracks
                }
            }

            // Audio
            Page {
                Section {
                    title: qsTr("Embedded Tracks")
                }

                TrackList {
                    tracks: Player.audioTracks
                }

                Separator { }

                Section {
                    title: qsTr("Output Device")

                    HeaderButton {
                        text: VLCIcons.refresh
                        description: qsTr("Refresh the output devices")

                        onClicked: Player.audioDevices.refresh()
                    }
                }

                Column {
                    width: parent.width

                    leftPadding: VLCStyle.dp(20, VLCStyle.scale)
                    rightPadding: leftPadding
                    bottomPadding: VLCStyle.margin_normal

                    spacing: VLCStyle.margin_small

                    Repeater {
                        model: Player.audioDevices

                        delegate: TrackRow {
                            required property var model

                            width: parent.width - parent.leftPadding - parent.rightPadding

                            text: model.display
                            checked: model.checked

                            onClicked: {
                                model.checked = true
                                fsm.trackPicked()
                            }
                        }
                    }
                }
            }

            // Subtitles
            Page {
                Section {
                    title: qsTr("Embedded Tracks")
                }

                TrackList {
                    tracks: Player.subtitleTracks
                    external: false
                    secondSubtitle: true
                }

                Separator { }

                Section {
                    title: qsTr("External Tracks")

                    HeaderButton {
                        text: VLCIcons.edit
                        description: root._removeMode ? qsTr("Stop removing external tracks")
                                                      : qsTr("Remove external tracks")
                        checked: root._removeMode

                        onClicked: root._removeMode = !root._removeMode
                    }

                    HeaderButton {
                        text: VLCIcons.add
                        description: qsTr("Add subtitles")

                        onClicked: {
                            fsm.dismiss()
                            DialogsProvider.addSubtitlesDialog("")
                        }
                    }
                }

                TrackList {
                    tracks: Player.subtitleTracks
                    external: true
                    secondSubtitle: true
                    showNone: false
                    removable: root._removeMode
                }
            }
        }

        // footer
        T.Button {
            id: moreSettings

            Layout.fillWidth: true
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
            Layout.margins: VLCStyle.dp(20, VLCStyle.scale)

            text: qsTr("More settings")

            Accessible.name: text

            onClicked: fsm.openMoreSettings()

            background: Rectangle {
                radius: VLCStyle.radius_small

                color: moreSettings.hovered || moreSettings.down ? theme.bg.highlight
                                                                 : theme.bg.secondary

                border.width: VLCStyle.focus_border
                border.color: moreSettings.visualFocus ? theme.accent : "transparent"
            }

            contentItem: Widgets.LabelExt {
                text: moreSettings.text

                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                font.pixelSize: VLCStyle.fontSize_normal

                color: moreSettings.hovered ? theme.fg.primary : theme.fg.secondary
            }
        }
    }

    // Components

    // a tab of the Track menu
    component TabButton: T.AbstractButton {
        id: tabButton

        required property int tab
        required property string glyph

        readonly property bool current: (fsm.tab === tab)

        Layout.fillWidth: true
        Layout.fillHeight: true

        focusPolicy: Qt.StrongFocus

        Accessible.role: Accessible.PageTab
        Accessible.name: text
        Accessible.selected: current

        onClicked: fsm.selectTab(tab)

        background: Item {
            Rectangle {
                anchors.fill: parent
                anchors.margins: VLCStyle.dp(4, VLCStyle.scale)

                radius: VLCStyle.radius_small

                color: "transparent"

                border.width: VLCStyle.focus_border
                border.color: tabButton.visualFocus ? theme.accent : "transparent"
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: VLCStyle.margin_xsmall
                anchors.rightMargin: VLCStyle.margin_xsmall

                height: VLCStyle.dp(2, VLCStyle.scale)

                color: tabButton.current ? theme.accent : "transparent"
            }
        }

        contentItem: Item {
            Row {
                anchors.centerIn: parent

                spacing: VLCStyle.margin_xsmall

                Widgets.IconLabel {
                    anchors.verticalCenter: parent.verticalCenter

                    text: tabButton.glyph

                    font.pixelSize: VLCStyle.dp(20, VLCStyle.scale)

                    color: tabButton.current ? theme.accent : theme.indicator
                }

                Widgets.LabelExt {
                    anchors.verticalCenter: parent.verticalCenter

                    text: tabButton.text

                    font.pixelSize: VLCStyle.fontSize_small

                    color: (tabButton.current || tabButton.hovered) ? theme.fg.primary
                                                                   : theme.fg.secondary
                }
            }
        }
    }

    // the scrolled content of a tab
    component Page: Flickable {
        id: page

        default property alias content: pageColumn.data

        contentWidth: width
        contentHeight: pageColumn.implicitHeight

        clip: true

        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: Widgets.ScrollBarExt {
            colorContext.palette: theme.palette
        }

        Column {
            id: pageColumn

            width: page.width
        }
    }

    // a section title, with its buttons on the right
    component Section: Item {
        id: section

        required property string title

        default property alias buttons: buttonRow.data

        width: parent.width
        height: VLCStyle.dp(60, VLCStyle.scale)

        Widgets.LabelExt {
            anchors.left: parent.left
            anchors.leftMargin: VLCStyle.dp(28, VLCStyle.scale)
            anchors.verticalCenter: parent.verticalCenter

            text: section.title

            font.pixelSize: VLCStyle.fontSize_normal

            color: theme.fg.secondary

            Accessible.role: Accessible.Heading
        }

        Row {
            id: buttonRow

            anchors.right: parent.right
            anchors.rightMargin: VLCStyle.dp(28, VLCStyle.scale) - VLCStyle.dp(3, VLCStyle.scale)
            anchors.verticalCenter: parent.verticalCenter

            spacing: VLCStyle.dp(14, VLCStyle.scale)
        }
    }

    component HeaderButton: Widgets.IconToolButton {
        // the content of a Popup does not inherit the palette
        colorContext.palette: theme.palette

        font.pixelSize: VLCStyle.icon_normal

        padding: VLCStyle.dp(3, VLCStyle.scale)
    }

    component Separator: Rectangle {
        width: parent.width
        height: VLCStyle.dp(1, VLCStyle.scale)

        color: theme.separator
    }

    // the tracks of a category, "None" first
    component TrackList: Column {
        id: trackList

        required property var tracks

        // when set, only the external (true) or the embedded (false) tracks
        property var external: undefined

        property bool showNone: true

        // right-click shows a track as the second subtitle
        property bool secondSubtitle: false

        property bool removable: false

        width: parent.width

        leftPadding: VLCStyle.dp(20, VLCStyle.scale)
        rightPadding: leftPadding
        bottomPadding: VLCStyle.margin_normal

        spacing: VLCStyle.margin_small

        TrackRow {
            visible: trackList.showNone

            width: parent.width - parent.leftPadding - parent.rightPadding

            text: qsTr("None")
            checked: !trackList.tracks.hasSelection

            onClicked: {
                trackList.tracks.unselectAll()
                fsm.trackPicked()
            }
        }

        Repeater {
            model: trackList.tracks

            delegate: TrackRow {
                id: trackRow

                required property var model
                required property int index

                visible: (trackList.external === undefined)
                         || (model.external === trackList.external)

                width: parent.width - parent.leftPadding - parent.rightPadding

                text: model.display
                checked: model.checked
                secondary: model.secondary
                removable: trackList.removable && model.external

                onClicked: {
                    trackList.tracks.select(index)
                    fsm.trackPicked()
                }

                onRemoveClicked: trackList.tracks.remove(index)

                onContextMenuRequested: (globalPos) => {
                    if (trackList.secondSubtitle)
                        trackContextMenu.show(trackList.tracks, index, model.secondary,
                                              model.checked, globalPos)
                }
            }
        }
    }

    // a track or a device: a check mark when picked
    component TrackRow: T.AbstractButton {
        id: trackRow

        property bool secondary: false
        property bool removable: false

        signal removeClicked()
        signal contextMenuRequested(point globalPos)

        implicitHeight: VLCStyle.dp(32, VLCStyle.scale)

        focusPolicy: Qt.StrongFocus

        Accessible.role: Accessible.RadioButton
        Accessible.checkable: true
        Accessible.checked: checked
        Accessible.name: secondary ? qsTr("%1, second subtitle").arg(text) : text

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Menu
                || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                trackRow.contextMenuRequested(trackRow.mapToGlobal(0, trackRow.height))
                event.accepted = true
            } else if (trackRow.removable
                       && (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace)) {
                trackRow.removeClicked()
                event.accepted = true
            }
        }

        MouseArea {
            anchors.fill: parent

            acceptedButtons: Qt.RightButton

            onClicked: (mouse) => {
                trackRow.contextMenuRequested(trackRow.mapToGlobal(mouse.x, mouse.y))
            }
        }

        background: Rectangle {
            radius: VLCStyle.radius_small

            color: trackRow.hovered ? theme.bg.highlight : "transparent"

            border.width: VLCStyle.focus_border
            border.color: trackRow.visualFocus ? theme.accent : "transparent"
        }

        contentItem: RowLayout {
            spacing: VLCStyle.dp(10, VLCStyle.scale)

            Widgets.IconLabel {
                Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                Layout.fillHeight: true

                text: trackRow.checked ? VLCIcons.check : ""

                font.pixelSize: VLCStyle.icon_normal

                color: theme.accent
            }

            Widgets.LabelExt {
                Layout.fillWidth: true

                text: trackRow.text

                elide: Text.ElideRight

                font.pixelSize: VLCStyle.fontSize_normal

                color: (trackRow.checked || trackRow.hovered) ? theme.fg.primary
                                                              : theme.fg.secondary
            }

            Widgets.LabelExt {
                visible: trackRow.secondary

                text: qsTr("Second subtitle")

                font.pixelSize: VLCStyle.fontSize_small

                color: theme.fg.secondary
            }

            Widgets.IconToolButton {
                visible: trackRow.removable

                colorContext.palette: theme.palette

                text: VLCIcons.close
                description: qsTr("Remove %1").arg(trackRow.text)

                font.pixelSize: VLCStyle.icon_normal

                padding: VLCStyle.dp(4, VLCStyle.scale)

                onClicked: trackRow.removeClicked()
            }
        }
    }
}
