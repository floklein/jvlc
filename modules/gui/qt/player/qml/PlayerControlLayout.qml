/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import VLC.Style
import VLC.Widgets as Widgets
import VLC.PlayerControls
import VLC.Util

FocusScope {
    id: playerControlLayout

    // Properties

    property real spacing: VLCStyle.margin_normal // spacing between controls

    property real layoutSpacing: VLCStyle.margin_xxlarge // spacing between layouts (left, center, and right)

    property int identifier: -1

    // In a narrow window, fold less important controls in a decided order:
    // the duration, the current time, the volume cone (down to the speaker),
    // then audio, subtitles and adjustments into the VLC menu. Only the
    // Control bar folds, as only it has a VLC menu to fold into.
    property bool folding: false

    readonly property int foldLevel: folding ? barFolding.level : ControlBarFolding.Full

    // the ids of the controls folded into the VLC menu
    readonly property var foldedControls: {
        if (!folding)
            return []
        if (loaderLeftRight.active)
            return loaderLeftRight.item?.foldedControls ?? []
        return [loaderLeft, loaderCenter, loaderRight].reduce((controls, loader) => {
            return (loader.active && loader.item) ? controls.concat(loader.item.foldedControls)
                                                  : controls
        }, [])
    }

    // the zones of the Top bar instead of the Control bar
    property bool topBar: false

    // shifts the center zone, to center it on the window rather than on the
    // room between the side zones
    property real centerOffset: 0

    readonly property PlayerControlbarModel model: {
        if (!!MainCtx.controlbarProfileModel.currentModel)
            return MainCtx.controlbarProfileModel.currentModel.getModel(identifier)
        else
            return null
    }

    readonly property ControlListModel _left: model ? (topBar ? model.topLeft : model.left) : null
    readonly property ControlListModel _center: model ? (topBar ? model.topCenter : model.center) : null
    readonly property ControlListModel _right: model ? (topBar ? model.topRight : model.right) : null

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    // Signals

    signal requestLockUnlockAutoHide(bool lock)
    signal menuOpened(var menu)
    ///incomming signal to request all menu and popup to close
    signal forceUnlock()

    // Settings

    implicitWidth: loaderLeftRight.active ? loaderLeftRight.implicitWidth
                                          : (loaderLeft.implicitWidth + loaderCenter.implicitWidth + loaderRight.implicitWidth)

    implicitHeight: loaderLeftRight.active ? loaderLeftRight.implicitHeight
                                           : Math.max(loaderLeft.implicitHeight, loaderCenter.implicitHeight, loaderRight.implicitHeight)

    // When the center zone holds an item that stretches (the inline seek bar),
    // the left and right zones keep their natural width and the center zone
    // takes the rest, instead of being centered.
    readonly property bool _centerExpands: !loaderLeftRight.active && !topBar
                                           && (loaderCenter.item?.expanding ?? false)

    // The Top bar's center zone (the title) stays centered on the window,
    // slides aside when it meets a side zone, then truncates: the side zones
    // keep their full width until the center has no room left.
    readonly property var _topBarCenterSpan: {
        const sideRoom = (loader) => {
            if (!loader.active || !loader.item)
                return 0
            // the zone's controls, its padding filler and the margin
            return loader.item.Layout.maximumWidth + layoutSpacing - spacing
        }

        return Helpers.centeredSpan(width / 2 + centerOffset,
                                    loaderCenter.item?.Layout.maximumWidth ?? 0,
                                    sideRoom(loaderLeft),
                                    width - sideRoom(loaderRight))
    }

    states: State {
        name: "centerExpands"
        when: playerControlLayout._centerExpands

        AnchorChanges {
            target: loaderLeft
            anchors.right: undefined
        }

        PropertyChanges {
            loaderLeft.width: loaderLeft.item?.Layout.maximumWidth ?? 0
        }

        AnchorChanges {
            target: loaderCenter
            anchors.horizontalCenter: undefined
            anchors.left: loaderLeft.right
            anchors.right: loaderRight.left
        }

        AnchorChanges {
            target: loaderRight
            anchors.left: undefined
            anchors.right: playerControlLayout.right
        }

        PropertyChanges {
            loaderRight.width: loaderRight.item?.Layout.maximumWidth ?? 0
        }
    }

    // Events

    Component.onCompleted: console.assert(identifier >= 0)

    // Children

    ControlBarFolding {
        id: barFolding

        availableWidth: playerControlLayout.width

        spacing: playerControlLayout.spacing

        // the left and right zones only: the filler between them sits
        // between two spacings
        zoneSpacing: loaderLeftRight.active ? playerControlLayout.layoutSpacing + 2 * playerControlLayout.spacing
                                            : playerControlLayout.layoutSpacing

        // a center zone that doesn't stretch stays centered on the bar
        centered: !loaderLeftRight.active && !playerControlLayout._centerExpands

        zones: {
            if (!playerControlLayout.folding)
                return []
            if (loaderLeftRight.active)
                return loaderLeftRight.item?.foldZones ?? []
            return [loaderLeft, loaderCenter, loaderRight].map((loader) => {
                return (loader.active && loader.item) ? loader.item.foldWidths : []
            })
        }
    }

    Loader {
        id: loaderLeftRight

        anchors.fill: parent

        active: !loaderCenter.active &&
                playerControlLayout.model &&
                ((playerControlLayout._left && (playerControlLayout._left.count > 0)) ||
                (playerControlLayout._right && (playerControlLayout._right.count > 0)))

        focus: active

        sourceComponent: RowLayout {
            spacing: playerControlLayout.spacing

            readonly property var foldZones: [leftRepeater.foldWidths, rightRepeater.foldWidths]
            readonly property var foldedControls: leftRepeater.foldedControls.concat(rightRepeater.foldedControls)

            focus: true

            // TODO: Qt >= 5.13 Use QConcatenateTablesProxyModel
            //       instead of multiple repeaters

            ControlRepeater {
                id: leftRepeater
                model: ControlListFilter {
                    sourceModel: playerControlLayout._left

                    player: Player
                    ctx: MainCtx
                }

                Navigation.parentItem: playerControlLayout
                Navigation.rightAction: function() {
                    const item = rightRepeater.itemAt(0)
                    if (item)
                        item.forceActiveFocus(Qt.TabFocusReason)
                    else
                        return false
                }

                availableWidth: loaderLeftRight.width
                availableHeight: loaderLeftRight.height

                foldLevel: playerControlLayout.foldLevel

                Component.onCompleted: {
                    requestLockUnlockAutoHide.connect(playerControlLayout.requestLockUnlockAutoHide)
                    menuOpened.connect(playerControlLayout.menuOpened)
                    playerControlLayout.forceUnlock.connect(leftRepeater.forceUnlock)
                }
            }

            Item {
                function containsVisibleItem(repeater) {
                    for (let i = 0; i < repeater.count; ++i) {
                        const item = repeater.itemAt(i)

                        if (item && item.visible)
                            return true
                    }

                    return false
                }

                Layout.minimumWidth: (containsVisibleItem(leftRepeater) && containsVisibleItem(rightRepeater)) ? playerControlLayout.layoutSpacing
                                                                                                               : 0

                Layout.fillWidth: true
                visible: true
            }

            ControlRepeater {
                id: rightRepeater
                model: ControlListFilter {
                    sourceModel: playerControlLayout._right

                    player: Player
                    ctx: MainCtx
                }

                Navigation.parentItem: playerControlLayout
                Navigation.leftAction: function() {
                    const item = leftRepeater.itemAt(leftRepeater.count - 1)
                    if (item)
                        item.forceActiveFocus(Qt.BacktabFocusReason)
                    else
                        return false
                }

                availableWidth: loaderLeftRight.width
                availableHeight: loaderLeftRight.height

                foldLevel: playerControlLayout.foldLevel

                Component.onCompleted: {
                    requestLockUnlockAutoHide.connect(playerControlLayout.requestLockUnlockAutoHide)
                    menuOpened.connect(playerControlLayout.menuOpened)
                    playerControlLayout.forceUnlock.connect(rightRepeater.forceUnlock)
                }
            }

        }
    }


    Loader {
        id: loaderLeft

        anchors {
            right: loaderCenter.left
            left: parent.left
            top: parent.top
            bottom: parent.bottom

            // Spacing for the filler item acts as padding
            rightMargin: layoutSpacing - spacing
        }

        active: !!playerControlLayout._left && (playerControlLayout._left.count > 0) &&
                !loaderLeftRight.active

        focus: active

        sourceComponent: ControlLayout {
            id: leftControlLayout

            model: ControlListFilter {
                sourceModel: playerControlLayout._left

                player: Player
                ctx: MainCtx
            }

            alignment: (Qt.AlignVCenter | Qt.AlignLeft)

            spacing: playerControlLayout.spacing

            foldLevel: playerControlLayout.foldLevel

            focus: true

            altFocusAction: () => Navigation.defaultNavigationRight()

            Navigation.parentItem: playerControlLayout
            Navigation.rightItem: loaderCenter.item

            Component.onCompleted: {
                requestLockUnlockAutoHide.connect(playerControlLayout.requestLockUnlockAutoHide)
                menuOpened.connect(playerControlLayout.menuOpened)
                playerControlLayout.forceUnlock.connect(leftControlLayout.forceUnlock)
            }
        }
    }

    Loader {
        id: loaderCenter

        anchors {
            horizontalCenter: parent.horizontalCenter
            horizontalCenterOffset: {
                if (!playerControlLayout.topBar)
                    return playerControlLayout.centerOffset
                const span = playerControlLayout._topBarCenterSpan
                return span.x + span.width / 2 - playerControlLayout.width / 2
            }
            top: parent.top
            bottom: parent.bottom
        }

        // TODO: "ControlListFilter"'s count......
        active: !!playerControlLayout.model && !!playerControlLayout._center && (playerControlLayout._center.count > 0)

        Binding on width {
            delayed: true
            when: loaderCenter._componentCompleted && !playerControlLayout._centerExpands
            value: {
                if (playerControlLayout.topBar)
                    return playerControlLayout._topBarCenterSpan.width

                const item = loaderCenter.item

                const minimumWidth = (item && item.Layout.minimumWidth > 0) ? item.Layout.minimumWidth : implicitWidth
                const maximumWidth = (item && item.Layout.maximumWidth > 0) ? item.Layout.maximumWidth : implicitWidth

                if ((loaderLeft.active && (loaderLeft.width > 0)) || (loaderRight.active && (loaderRight.width > 0))) {
                    return minimumWidth
                } else {
                    return Math.min(loaderCenter.parent.width, maximumWidth)
                }
            }
        }

        property bool _componentCompleted: false

        Component.onCompleted: {
            _componentCompleted = true
        }

        sourceComponent: ControlLayout {
            id: centerControlLayout

            model: ControlListFilter {
                sourceModel: playerControlLayout._center

                player: Player
                ctx: MainCtx
            }

            focus: true

            spacing: playerControlLayout.spacing

            foldLevel: playerControlLayout.foldLevel

            altFocusAction: () => Navigation.defaultNavigationUp()

            Navigation.parentItem: playerControlLayout
            Navigation.leftItem: loaderLeft.item
            Navigation.rightItem: loaderRight.item

            Component.onCompleted: {
                requestLockUnlockAutoHide.connect(playerControlLayout.requestLockUnlockAutoHide)
                menuOpened.connect(playerControlLayout.menuOpened)
                playerControlLayout.forceUnlock.connect(centerControlLayout.forceUnlock)
            }
        }
    }

    Loader {
        id: loaderRight

        anchors {
            left: loaderCenter.right
            right: parent.right
            top: parent.top
            bottom: parent.bottom

            // Spacing for the filler item acts as padding
            leftMargin: layoutSpacing - spacing
        }

        active: !!playerControlLayout.model && !!playerControlLayout._right && (playerControlLayout._right.count > 0) &&
                !loaderLeftRight.active

        sourceComponent: ControlLayout {
            id: rightControlLayout

            model: ControlListFilter {
                sourceModel: playerControlLayout._right

                player: Player
                ctx: MainCtx
            }

            alignment: (Qt.AlignVCenter | Qt.AlignRight)

            spacing: playerControlLayout.spacing

            foldLevel: playerControlLayout.foldLevel

            focus: true

            altFocusAction: () => Navigation.defaultNavigationLeft()

            Navigation.parentItem: playerControlLayout
            Navigation.leftItem: loaderCenter.item

            onRequestLockUnlockAutoHide: (lock) => playerControlLayout.requestLockUnlockAutoHide(lock)

            onMenuOpened: (menu) => playerControlLayout.menuOpened(menu)

            Component.onCompleted:{
                playerControlLayout.forceUnlock.connect(rightControlLayout.forceUnlock)
            }
        }
    }
}
