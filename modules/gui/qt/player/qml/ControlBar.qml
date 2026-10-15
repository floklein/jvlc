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
import QtQuick.Layouts
import QtQml.Models


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Player
import VLC.PlayerControls
import VLC.Util

T.Pane {
    id: root

    // the popups of its buttons open above it (PopupIconToolButton)
    objectName: "controlBar"

    enum TimeTextPosition {
        Hide,
        AboveSlider,
        LeftRightSlider
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    topInset: sliderY

    bottomPadding: VLCStyle.margin_xsmall

    // the Layout places the seek bar among the controls: the bar draws no
    // seek bar row of its own and its top is where the seek bar starts
    readonly property bool seekBarInline: contentItem.playerControlLayout.model?.seekBarInline ?? false

    // when the Layout puts the seek bar on its own row, draw the redesign's
    // seek bar (time, thick bar, duration) above the controls rather than the
    // classic slider (Figma 2160:7437)
    property bool seekBarOwnRow: false

    readonly property bool _seekBarRow: seekBarOwnRow && !seekBarInline
    // the rows of the redesign (Figma 491:1779 and 2160:7437)
    readonly property bool _redesignRows: seekBarInline || seekBarOwnRow

    readonly property real sliderY: seekBarInline ? 0 : mapFromItem(contentItem, 0, contentItem.sliderY).y
    property int textPosition: ControlBar.TimeTextPosition.AboveSlider
    property int identifier: -1
    property real sliderHeight: VLCStyle.heightBar_xxsmall
    property real bookmarksHeight: VLCStyle.controlBarBookmarksHeight
    property bool showRemainingTime: true

    property var menu: undefined

    // fold controls into the VLC menu in a narrow window (the player's
    // Control bar, which has a Top bar with the VLC menu)
    property bool folding: false

    // the ids of the controls folded into the VLC menu in a narrow window
    readonly property var foldedControls: contentItem.playerControlLayout.foldedControls

    signal requestLockUnlockAutoHide(bool lock)

    ///incomming signal to request all menu and popup to close
    signal forceUnlock()

    enabled: visible

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)
    Navigation.cancelAction: function() { History.previous(Qt.BacktabFocusReason) }

    Accessible.name: qsTr("Player controls")
    AccessibleCompat.id: "playerControlBar"

    function showChapterMarks() {
        // the inline seek bar flashes its marks by itself when shown
        if (_seekBarRow)
            contentItem.seekBarRowWidget.showChapterMarks()
        else if (contentItem.trackPositionSlider && !seekBarInline)
            contentItem.trackPositionSlider.showChapterMarks()
    }

    // Gives the keyboard focus to the Play button, else to the bar
    function focusPlayButton(reason) {
        const find = (item) => {
            if (item.objectName === "playButton")
                return item
            for (const child of item.children) {
                const found = find(child)
                if (found)
                    return found
            }
            return null
        }

        const button = find(root.contentItem)
        if (button && button.visible && button.enabled)
            button.forceActiveFocus(reason)
        else
            root.forceActiveFocus(reason)
    }

    function applyMenu(menu) {
        if (root.menu === menu)
            return

        // NOTE: When applying a new menu we hide the previous one.
        if (menu)
            dismiss()

        root.menu = menu
    }

    function dismiss() {
        if ((typeof menu === undefined) || !menu)
            return
        if (menu.hasOwnProperty("dismiss"))
            menu.dismiss()
        else if (menu.hasOwnProperty("close"))
            menu.close()
    }

    // Jump to Time, over the seek bar
    function openJumpToTime() {
        if (!Player.seekable || !contentItem.trackPositionSlider.enabled)
            return
        jumpToTimePopup.open()
    }

    onForceUnlock: jumpToTimePopup.close()

    JumpToTimePopup {
        id: jumpToTimePopup

        parent: root.contentItem.trackPositionSlider
        colorContext.palette: theme.palette

        onOpened: {
            root.applyMenu(jumpToTimePopup)
            root.requestLockUnlockAutoHide(true)
        }

        onClosed: {
            root.requestLockUnlockAutoHide(false)
            if (root.menu === jumpToTimePopup)
                root.menu = undefined
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    TextMetrics {
        id: timeTextMetrics
        font.pixelSize: (textPosition === ControlBar.TimeTextPosition.LeftRightSlider) ? VLCStyle.fontSize_small
                                                                                       : VLCStyle.fontSize_normal
        text: {
            // This text metrics is used for both the time and length, as we want them to have the same width.
            // Length might not be reported in some cases, for that reason we should check both and use the bigger one.

            // Some extra space to compensate non-monospaced fonts ("-" takes more space than ":", double "-" for guarantee)
            return (Player.time.isSubHour() && Player.length.isSubHour()) ? "00--00" : "00--00--00"
        }
    }

    background: Rectangle {
        color: theme.bg.primary
    }

    contentItem: ColumnLayout {
        spacing: VLCStyle.margin_xsmall

        readonly property real sliderY: row2.y

        property alias trackPositionSlider: trackPositionSlider

        property alias seekBarRowWidget: seekBarRowWidget

        property alias mediaRemainingTime: mediaRemainingTime

        property alias playerControlLayout: playerControlLayout

        readonly property list<Item> strayItems: [
            T.Label {
                id: mediaTime

                Layout.preferredWidth: (textPosition === ControlBar.TimeTextPosition.LeftRightSlider) ? timeTextMetrics.width : -1

                text: {
                    const length = Player.length
                    return Player.time.formatHMS(length.isSubSecond() ? VLCTick.SubSecondFormattedAsMS : 0)
                }
                color: theme.fg.primary
                font: timeTextMetrics.font
                horizontalAlignment: Text.AlignHCenter

                // TODO: Do not use `Binding` when minimum Qt is 6.12:
                Binding {
                    target: mediaTime
                    property: "mutabilityGroup"
                    when: (mediaTime.mutabilityGroup !== undefined)
                    value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.ModerateMutabilityGroup
                                                                                  : Item.StaticMutabilityGroup
                }

                anchors.left: (parent === pseudoRow) ? parent.left : undefined
                anchors.verticalCenter: (parent === pseudoRow) ? parent.verticalCenter : undefined
            },
            T.Label {
                id: mediaRemainingTime

                Layout.preferredWidth: (textPosition === ControlBar.TimeTextPosition.LeftRightSlider) ? timeTextMetrics.width : -1

                readonly property bool showRemainingTime: (MainCtx.showRemainingTime && Player.remainingTime.valid())

                text: showRemainingTime
                      ? "-" + Player.remainingTime.formatHMS()
                      : Player.length.formatHMS()
                color: mediaTime.color
                font: timeTextMetrics.font
                horizontalAlignment: Text.AlignHCenter

                visible: root.showRemainingTime

                // a button: a click, Enter or Space switches between the
                // duration and the remaining time
                Accessible.role: Accessible.Button
                Accessible.name: MainCtx.showRemainingTime ? qsTr("Show the duration")
                                                           : qsTr("Show the remaining time")
                Accessible.focusable: true
                Accessible.onPressAction: MainCtx.showRemainingTime = !MainCtx.showRemainingTime

                Navigation.parentItem: root
                Navigation.downItem: playerControlLayout

                activeFocusOnTab: true

                Keys.onPressed: (event) => {
                    if (KeyHelper.matchOk(event)) {
                        event.accepted = true
                        MainCtx.showRemainingTime = !MainCtx.showRemainingTime
                        return
                    }
                    Navigation.defaultKeyAction(event)
                }

                // the keyboard focus ring
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: - VLCStyle.focus_border

                    visible: mediaRemainingTime.activeFocus

                    radius: VLCStyle.radius_xxsmall
                    color: "transparent"
                    border.width: VLCStyle.focus_border
                    // the focus ring color: theme.visualFocus is only set on a focused context
                    border.color: theme.accent
                }

                // TODO: Do not use `Binding` when minimum Qt is 6.12:
                Binding {
                    target: mediaRemainingTime
                    property: "mutabilityGroup"
                    when: (mediaRemainingTime.mutabilityGroup !== undefined)
                    value: ((Player.playingState === Player.PLAYING_STATE_PLAYING) &&
                            mediaRemainingTime.showRemainingTime) ? Item.ModerateMutabilityGroup
                                                                  : Item.StaticMutabilityGroup
                }

                anchors.right: (parent === pseudoRow) ? parent.right : undefined
                anchors.verticalCenter: (parent === pseudoRow) ? parent.verticalCenter : undefined

                TapHandler {
                    onTapped: (eventPoint, button) => {
                        MainCtx.showRemainingTime = !MainCtx.showRemainingTime
                    }
                }

                HoverHandler {
                    id: mediaRemainingTimeHover
                }

                Widgets.ToolTipExt {
                    colorContext.palette: theme.palette

                    visible: mediaRemainingTimeHover.hovered && MainCtx.showRemainingTime && Player.rate !== 1
                    delay: VLCStyle.delayToolTipAppear
                    text: {
                        if (!visible)
                            return ""

                        const playbackSpeed = Math.abs(Player.rate)
                        const adjustedRemainingTime = Player.remainingTime.scale(1 / playbackSpeed)
                            ? Player.remainingTime.scale(1 / playbackSpeed)
                            : Player.remainingTime

                        return qsTr("%1 (%2x)")
                            .arg(adjustedRemainingTime.formatHMS())
                            .arg(+Player.rate.toFixed(2))
                    }

                    x: (parent.width - width) / 2
                }
            },
            SliderBar {
                id: trackPositionSlider

                barHeight: root.sliderHeight

                AccessibleCompat.id: "progressSlider"

                Layout.fillWidth: true
                enabled: Player.playingState === Player.PLAYING_STATE_PLAYING || Player.playingState === Player.PLAYING_STATE_PAUSED

                Navigation.parentItem: root
                Navigation.downItem: playerControlLayout

                activeFocusOnTab: true

                focus: true

                Keys.onPressed: (event) => {
                    Navigation.defaultKeyAction(event)
                }
            },
            SeekBarWidget {
                id: seekBarRowWidget

                Layout.fillWidth: true

                implicitWidth: VLCStyle.dp(240, VLCStyle.scale)

                Navigation.parentItem: root
                Navigation.downItem: playerControlLayout
            },
            Loader {
                id: bookmarksLoader

                parent: root
                // TODO: bookmarks on the redesign's seek bar
                active: MainCtx.mediaLibraryAvailable && !root.seekBarInline && !root._seekBarRow
                source: "qrc:///qt/qml/VLC/MediaLibrary/Bookmarks.qml"

                x: root.leftPadding + trackPositionSlider.x + row2.Layout.leftMargin
                y: row2.y + row2.height + VLCStyle.margin_xxsmall
                width: trackPositionSlider.width

                onLoaded: {
                   item.barHeight = Qt.binding(function() { return bookmarksHeight })
                   item.controlBarHovered = Qt.binding(function() { return root.hovered })
                   item.yShift = Qt.binding(function() { return row2.height + VLCStyle.margin_xxsmall })
                }
            }
        ]

        Item {
            // BUG: RowLayout can not be used here
            // because of a Qt bug. (Height is
            // incorrectly determined. Could be
            // about nested layouting).

            id: pseudoRow
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.leftMargin: VLCStyle.margin_normal
            Layout.rightMargin: VLCStyle.margin_normal

            implicitHeight: visible ? Math.max(mediaTime.implicitHeight, mediaRemainingTime.implicitHeight) : 0

            visible: children.length > 0

            children: {
                if (root.seekBarInline || root._seekBarRow)
                    return []

                switch (textPosition) {
                case ControlBar.TimeTextPosition.AboveSlider:
                    return [mediaTime, mediaRemainingTime]
                case ControlBar.TimeTextPosition.Hide:
                case ControlBar.TimeTextPosition.LeftRightSlider:
                default:
                    return []
                }
            }
        }

        RowLayout {
            id: row2

            children: {
                if (root.seekBarInline)
                    return []
                if (root._seekBarRow)
                    return [seekBarRowWidget]

                switch (textPosition) {
                case ControlBar.TimeTextPosition.Hide:
                case ControlBar.TimeTextPosition.AboveSlider:
                    return [trackPositionSlider]
                case ControlBar.TimeTextPosition.LeftRightSlider:
                    return [mediaTime, trackPositionSlider, mediaRemainingTime]
                default:
                    return []
                }
            }

            visible: children.length > 0

            spacing: VLCStyle.margin_xsmall

            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.leftMargin: (textPosition === ControlBar.TimeTextPosition.LeftRightSlider && !root._seekBarRow) ? VLCStyle.margin_xsmall
                                                                                              : 0

            Layout.rightMargin: Layout.leftMargin
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: root._redesignRows ? 0 : VLCStyle.margin_large
            Layout.rightMargin: root._redesignRows ? 0 : VLCStyle.margin_large

            spacing: playerControlLayout.spacing

            // not playerControlLayout.visible: that is false while the bar is
            // hidden (Hide UI starts so), and the row would then never show again
            visible: (!playerControlLayout.model?.empty ?? false) || castButtonLoader.active

            PlayerControlLayout {
                id: playerControlLayout

                Layout.fillWidth: true
                Layout.fillHeight: true

                identifier: root.identifier

                folding: root.folding

                // Figma 491:1779: one 32 px row, 8 px between items, 20 px between zones
                spacing: root._redesignRows ? VLCStyle.spacing_8 : VLCStyle.margin_normal
                layoutSpacing: root._redesignRows ? VLCStyle.spacing_20 : VLCStyle.margin_xxlarge

                implicitHeight: {
                    if (root._redesignRows)
                        return VLCStyle.dp(32, VLCStyle.scale)
                    return MainCtx.pinVideoControls ? VLCStyle.controlLayoutHeightPinned
                                                    : VLCStyle.controlLayoutHeight
                }

                visible: !model?.empty ?? false

                Navigation.upItem: {
                    if (root._seekBarRow)
                        return seekBarRowWidget
                    if (trackPositionSlider.enabled && !root.seekBarInline)
                        return trackPositionSlider
                    return root.Navigation.upItem
                }

                onRequestLockUnlockAutoHide: (lock) => root.requestLockUnlockAutoHide(lock)

                onMenuOpened: (menu) => root.applyMenu(menu)

                Component.onCompleted: {
                    root.forceUnlock.connect(playerControlLayout.forceUnlock)
                }
            }

            // While the playback is cast to a device, the Cast button shows in
            // the Control bar even if the Layout does not have it
            Loader {
                id: castButtonLoader

                Layout.alignment: Qt.AlignVCenter

                active: {
                    if (!Player.rendererManager.useRenderer)
                        return false
                    const model = playerControlLayout.model
                    if (!model)
                        return true
                    // re-evaluate when the zones change, in both bars
                    void (model.left.count + model.center.count + model.right.count
                          + model.topLeft.count + model.topCenter.count + model.topRight.count)
                    return !model.isPlaced(ControlListModel.RENDERER_BUTTON)
                }

                sourceComponent: CastButton {
                    onRequestLockUnlockAutoHide: (lock) => root.requestLockUnlockAutoHide(lock)
                    onMenuOpened: (menu) => root.applyMenu(menu)

                    Component.onCompleted: root.forceUnlock.connect(forceUnlock)
                }
            }
        }
    }
}
