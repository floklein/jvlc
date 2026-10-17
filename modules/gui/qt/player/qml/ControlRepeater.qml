/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

import VLC.MainInterface
import VLC.Util
import VLC.PlayerControls

Repeater {
    id: repeater

    property int alignment: Qt.AlignVCenter

    property real availableWidth: Number.MAX_VALUE
    property real availableHeight: Number.MAX_VALUE

    // how far the bar folds in a narrow window (ControlBarFolding.Level). The
    // controls that fold read it (foldLevel) or fold away at their foldStep.
    property int foldLevel: ControlBarFolding.Full

    // The Control bar spaces its controls by group (Figma 491:1779): `spacing`
    // inside the transport group (previous, play, next...), `buttonSpacing`
    // between the buttons of a group (audio, subtitles, adjustments...),
    // `groupSpacing` between two groups (transport | seek bar | volume |
    // buttons | Queue). Off, every control is `spacing` apart.
    property bool groupGaps: false
    property real spacing: 0
    property real buttonSpacing: 0
    property real groupSpacing: 0

    // the group of a control: "" for the ones that sit between groups (the
    // seek bar, spacers and dividers keep `spacing` on both sides)
    function controlGroup(controlId : int) : string {
        switch (controlId) {
        case ControlListModel.PREVIOUS_BUTTON:
        case ControlListModel.PLAY_BUTTON:
        case ControlListModel.NEXT_BUTTON:
        case ControlListModel.SKIP_BACK_BUTTON:
        case ControlListModel.SKIP_FW_BUTTON:
        case ControlListModel.STOP_BUTTON:
        case ControlListModel.SLOWER_BUTTON:
        case ControlListModel.FASTER_BUTTON:
        case ControlListModel.FRAME_PREV_BUTTON:
        case ControlListModel.FRAME_NEXT_BUTTON:
        case ControlListModel.REVERSE_BUTTON:
        case ControlListModel.CHAPTER_PREVIOUS_BUTTON:
        case ControlListModel.CHAPTER_NEXT_BUTTON:
            return "transport"
        case ControlListModel.VOLUME:
            return "volume"
        case ControlListModel.PLAYLIST_BUTTON:
            return "queue"
        case ControlListModel.SEEK_BAR:
        case ControlListModel.WIDGET_SPACER:
        case ControlListModel.WIDGET_SPACER_EXTEND:
        case ControlListModel.DIVIDER:
            return ""
        default:
            return "buttons"
        }
    }

    // the room between two neighbor controls
    function gapBetween(previousId : int, controlId : int) : real {
        return Helpers.controlBarGap(controlGroup(previousId), controlGroup(controlId),
                                     spacing, buttonSpacing, groupSpacing)
    }

    // the extra room a control takes before it, beyond `spacing`: the gap to
    // the nearest shown control before it
    function _leadingMargin(index : int) : real {
        if (!groupGaps)
            return 0
        void _itemsRevision
        const loader = itemAt(index)
        if (!loader)
            return 0
        for (let i = index - 1; i >= 0; --i) {
            const previous = itemAt(i)
            if (previous && previous.visible)
                return gapBetween(previous.controlId, loader.controlId) - spacing
        }
        return 0
    }

    // the same, with the control before it in the Layout whether it shows or
    // not: the folding computes the room the bar needs from it, without
    // depending on what the folding hides
    function _foldMargin(index : int) : real {
        if (!groupGaps || index <= 0)
            return 0
        void _itemsRevision
        const loader = itemAt(index)
        const previous = itemAt(index - 1)
        if (!loader || !previous)
            return 0
        return gapBetween(previous.controlId, loader.controlId) - spacing
    }

    // for each control, its width at each fold level (see ControlBarFolding),
    // with the room its group takes before it
    readonly property var foldWidths: {
        const widths = []
        for (let i = 0; i < count; ++i) {
            const loader = itemAt(i)
            const item = loader?.item
            if (!item)
                continue

            const margin = _foldMargin(i)
            const withMargin = (width) => width > 0 ? width + margin : 0

            if (item.foldWidths !== undefined) {
                widths.push(item.foldWidths.map(withMargin))
                continue
            }

            const width = withMargin(Math.max(loader.Layout.minimumWidth, 0))
            const foldStep = item.foldStep ?? 0
            if (foldStep > 0) {
                const steps = []
                for (let level = 0; level <= ControlBarFolding.TracksFolded; ++level)
                    steps.push(level >= foldStep ? 0 : width)
                widths.push(steps)
            } else {
                widths.push(width)
            }
        }
        return widths
    }

    // the ids of the controls folded away, or left out because they don't
    // fit, which the VLC menu shows instead
    readonly property var foldedControls: {
        const controls = []
        for (let i = 0; i < count; ++i) {
            const loader = itemAt(i)
            if (loader && (loader.folded || !loader.fits))
                controls.push(loader.controlId)
        }
        return controls
    }

    signal requestLockUnlockAutoHide(bool lock)
    signal menuOpened(var menu)
    ///incomming signal to request all menu and popup to close
    signal forceUnlock()

    // bumped when a delegate comes or goes: itemAt() does not notify, and the
    // delegates are created one by one after `count` is set
    property int _itemsRevision: 0

    // NOTE: We apply the 'navigation chain' after adding the item.
    onItemAdded: (index, item) => {
        item.applyNavigation()
        ++_itemsRevision
    }

    onItemRemoved: (index, item) => {
        ++_itemsRevision
        // NOTE: We update the 'navigation chain' after removing the item.
        item.removeNavigation()

        item.recoverFocus(index)
    }

    delegate: Loader {
        id: loader

        // Settings

        source: PlayerControlbarControls.control(model.id).source

        readonly property int controlId: model.id

        // folded into the VLC menu in a narrow window
        readonly property bool folded: !!item && ((item.foldStep ?? 0) > 0)
                                       && (repeater.foldLevel >= item.foldStep)

        // whether the control fits in what is left of the bar, when even
        // folding could not make room (an unusual Layout in a narrow window)
        property bool fits: true

        visible: !folded && fits

        focus: (index === 0)

        Layout.fillWidth: (item && item.Layout.minimumWidth > 0)
        Layout.minimumWidth: Layout.fillWidth ? item.Layout.minimumWidth : (item ? item.implicitWidth : -1)
        Layout.maximumWidth: Layout.fillWidth ? (item ? item.implicitWidth : -1) : -1
        // a control that only takes the room the others leave (the title)
        // prefers its minimum width
        Layout.preferredWidth: (Layout.fillWidth && item.Layout.preferredWidth >= 0) ? item.Layout.preferredWidth : -1

        Layout.minimumHeight: 0
        Layout.maximumHeight: Math.min((item && item.implicitHeight <= 0) ? Number.MAX_VALUE : item.implicitHeight, repeater.availableHeight)
        Layout.fillHeight: true

        Layout.alignment: repeater.alignment

        // the room its group takes before it (groupGaps)
        Layout.leftMargin: repeater._leadingMargin(index)

        Binding on fits {
            delayed: true // this is important
            when: condition
            value: {
                if (condition)
                    return Helpers.controlFits(loader.x, loader.Layout.minimumWidth, repeater.availableWidth)
                else
                    return true;
            }

            property bool condition: false

            Binding on condition {
                delayed: true
                value: (loader.status === Loader.Ready) &&
                       (loader.Layout.minimumWidth > 0) &&
                       (repeater.availableWidth < Number.MAX_VALUE)
            }
        }

        // Events

        onActiveFocusChanged: {
            if (activeFocus && !item?.focus) {
                recoverFocus()
            }
        }

        onLoaded: {
            // control should not request focus if they are not enabled:
            item.focus = Qt.binding(function() { return item.enabled && item.visible })

            // navigation parent of control is always controlLayout
            // so it can be set here unlike leftItem and rightItem:
            item.Navigation.parentItem = repeater

            if (typeof item.activeFocusOnTab === "boolean")
                item.activeFocusOnTab = true

            item.visible = Qt.binding(function() { return loader.visible })

            if (item.foldLevel !== undefined)
                item.foldLevel = Qt.binding(function() { return repeater.foldLevel })

            // a divider separates nothing once what follows it is hidden
            if (item.separating !== undefined) {
                item.separating = Qt.binding(function() {
                    void repeater._itemsRevision
                    const next = repeater.itemAt(index + 1)
                    return !!next && next.visible
                })
            }

            if (item.requestLockUnlockAutoHide)
                item.requestLockUnlockAutoHide.connect(repeater.requestLockUnlockAutoHide)

            if (item.menuOpened)
                item.menuOpened.connect(repeater.menuOpened)

            if (item.forceUnlock)
                repeater.forceUnlock.connect(item.forceUnlock)

            //can't connect to enabledChanged in a Connections
            item.onEnabledChanged.connect(() => {
                if (loader.activeFocus && !item.enabled) // Loader has focus but item is not enabled
                    recoverFocus()
            })
        }

        // Connections

        Connections {
            target: item

            enabled: loader.status === Loader.Ready

            function onVisibleChanged() {
                if (loader.activeFocus && !item.visible)
                    recoverFocus()
            }
        }

        // Functions

        function applyNavigation() {
            if (item === null) return

            const itemLeft  = repeater.itemAt(index - 1)
            const itemRight = repeater.itemAt(index + 1)

            if (itemLeft) {
                const componentLeft = itemLeft.item

                if (componentLeft)
                {
                    item.Navigation.leftItem = componentLeft

                    componentLeft.Navigation.rightItem = item
                }
            }

            if (itemRight) {
                const componentRight = itemRight.item

                if (componentRight)
                {
                    item.Navigation.rightItem = componentRight

                    componentRight.Navigation.leftItem = item
                }
            }
        }

        function removeNavigation() {
            if (item === null) return

            const itemLeft = repeater.itemAt(index - 1)

            // NOTE: The current item was removed from the repeater so we test against the
            //       same index.
            const itemRight = repeater.itemAt(index)

            if (itemLeft) {
                if (itemRight) {
                    itemLeft.item.Navigation.rightItem = itemRight.item
                    itemRight.item.Navigation.leftItem = itemLeft.item
                }
                else
                    itemLeft.item.Navigation.rightItem = null
            }
            else if (itemRight) {
                itemRight.item.Navigation.leftItem = null
            }
        }

        function recoverFocus(_index) {
            if (item === null) return

            const controlLayout = repeater.Navigation.parentItem

            if (!controlLayout || !controlLayout.visible)
                return

            if (_index === undefined)
                _index = index

            for (let i = 1; i <= Math.max(_index, repeater.count - (_index + 1)); ++i) {
                 if (i <= _index) {
                     const leftItem = repeater.itemAt(_index - i)

                     if (_focusIfFocusable(leftItem))
                         return
                 }

                 if (_index + i <= repeater.count - 1) {
                     const rightItem = repeater.itemAt(_index + i)

                     if (_focusIfFocusable(rightItem))
                         return
                 }
            }

            // focus to other alignment if focusable control
            // in the same alignment is not found:
            if (!!controlLayout.Navigation.rightItem) {
                controlLayout.Navigation.defaultNavigationRight()
            } else if (!!controlLayout.Navigation.leftItem) {
                controlLayout.Navigation.defaultNavigationLeft()
            } else if (controlLayout.altFocusAction) {
                controlLayout.altFocusAction()
            }
        }

        // Private

        function _focusIfFocusable(_loader) {
            if (_loader?.item?.focus) {
                if (item.focusReason !== undefined)
                    _loader.item.forceActiveFocus(item.focusReason)
                else {
                    console.warn("focusReason is not available in %1!".arg(item))
                    _loader.item.forceActiveFocus()
                }
                return true
            } else {
                return false
            }
        }
    }
}
