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
import QtQuick.Window


import VLC.MainInterface
import VLC.Style
import VLC.Player
import VLC.PlayerControls
import VLC.Widgets as Widgets
import VLC.Menus as Menus
import VLC.Util
import VLC.Dialogs

// The player's Top bar (Figma 247:447): the cone, the left, center and right
// zones of the Layout (by default the VLC menu, the library toggle and the
// folder button, then the media title), and the window buttons with
// fullscreen on the right. It is also the title bar of the window when VLC
// draws its own decorations.
FocusScope {
    id: root

    // Properties

    property int topMargin: 0
    property int sideMargin: 0

    // the Layout of the video or the music player
    property int identifier: PlayerControlbarModel.Videoplayer

    // the Top bar is the window's title bar: drag, double-click and the window buttons
    property bool showCSD: false
    property bool pinControls: false

    // height the Top bar covers over the content
    readonly property int reservedHeight: implicitHeight

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    // Figma: 40 px tall, 6 px padding
    readonly property int _barHeight: VLCStyle.dp(40, VLCStyle.scale)
    readonly property int _padding: VLCStyle.dp(6, VLCStyle.scale) + sideMargin

    readonly property bool _fullscreen: MainCtx.intfMainWindow.visibility === Window.FullScreen

    // Signals

    signal requestLockUnlockAutoHide(bool lock)

    ///incomming signal to request all menu and popup to close
    signal forceUnlock()

    // Settings

    implicitHeight: topMargin
                    + _barHeight

    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Top bar")
    AccessibleCompat.id: "playerTopBar"

    // Functions

    // The fullscreen button follows the video output while there is one, as
    // double-click and the F key do, and the window otherwise.
    function toggleFullscreen() {
        if (Player.fullscreen)
            Player.fullscreen = false
        else if (root._fullscreen)
            MainCtx.toggleInterfaceFullScreen()
        else if (Player.hasVideoOutput)
            Player.fullscreen = true
        else
            MainCtx.toggleInterfaceFullScreen()
    }

    // Children

    //drag and dbl click the titlebar in CSD mode
    Loader {
        id: tapNDrag

        anchors.fill: bar
        active: root.showCSD
        source: "qrc:///qt/qml/VLC/Widgets/CSDTitlebarTapNDrapHandler.qml"

        Connections {
            target: tapNDrag.item
            enabled: tapNDrag.status === Loader.Ready
            function onCsdMenuVisibleChanged() {
                root.requestLockUnlockAutoHide(tapNDrag.item.csdMenuVisible)
            }
        }
    }

    Item {
        id: bar

        anchors.top: parent.top
        anchors.topMargin: root.topMargin
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // the cone opens the window's system menu
        T.Button {
            id: cone

            anchors.left: parent.left
            anchors.leftMargin: root._padding
            anchors.verticalCenter: parent.verticalCenter

            width: VLCStyle.dp(38, VLCStyle.scale)
            height: VLCStyle.dp(28, VLCStyle.scale)

            focusPolicy: Qt.TabFocus

            Accessible.role: Accessible.Button
            Accessible.name: Helpers.accessibleName(qsTr("Window menu"),
                                                    root.showCSD ? qsTr("Alt+Space") : "")
            AccessibleCompat.id: "topBarWindowMenuButton"

            Navigation.parentItem: root
            Navigation.rightItem: zones

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

            onClicked: {
                if (csdMenuLoader.item)
                    csdMenuLoader.item.popup(cone.mapToGlobal(0, cone.height))
            }

            // the cone logo stays orange, whatever the Accent color
            contentItem: Widgets.BannerCone {
                color: theme.accent

                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(VLCStyle.icon_normal, VLCStyle.icon_normal)
                horizontalAlignment: Image.AlignHCenter
                verticalAlignment: Image.AlignVCenter
            }

            readonly property ColorContext colorContext: ColorContext {
                id: coneTheme
                colorSet: ColorContext.ToolButton

                focused: cone.visualFocus
                hovered: cone.hovered
            }

            background: Widgets.AnimatedBackground {
                enabled: coneTheme.initialized
                radius: VLCStyle.radius_normal
                border.color: coneTheme.visualFocus
            }

            Loader {
                id: csdMenuLoader

                active: root.showCSD
                sourceComponent: CSDMenu {
                    ctx: MainCtx

                    onMenuVisibleChanged: root.requestLockUnlockAutoHide(menuVisible)
                }
            }
        }

        // The Layout's Top bar zones: by default the VLC menu, the library
        // toggle and the folder button on the left, and the media title in
        // the center (Customize Player)
        PlayerControlLayout {
            id: zones

            readonly property real _leftEdge: cone.x + cone.width + anchors.leftMargin
            readonly property real _rightEdge: parent.width - rightGroup.x + anchors.rightMargin

            anchors.left: cone.right
            anchors.leftMargin: VLCStyle.dp(7, VLCStyle.scale)
            anchors.right: rightGroup.left
            anchors.rightMargin: VLCStyle.margin_normal
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            focus: true

            topBar: true
            identifier: root.identifier

            spacing: VLCStyle.spacing_10
            layoutSpacing: VLCStyle.margin_normal

            // the title stays centered on the window
            centerOffset: (_rightEdge - _leftEdge) / 2

            Navigation.parentItem: root
            Navigation.leftItem: cone

            onRequestLockUnlockAutoHide: (lock) => root.requestLockUnlockAutoHide(lock)

            Component.onCompleted: root.forceUnlock.connect(zones.forceUnlock)
        }

        // Right: window buttons, with fullscreen next to maximize
        Row {
            id: rightGroup

            anchors.right: parent.right
            anchors.rightMargin: root._padding
            anchors.verticalCenter: parent.verticalCenter

            height: VLCStyle.dp(24, VLCStyle.scale)

            // the background Conversions, while there are some
            ConversionsIndicator {
                height: parent.height
                width: VLCStyle.dp(36, VLCStyle.scale)
                font.pixelSize: VLCStyle.icon_normal

                Navigation.parentItem: root

                onRequestLockUnlockAutoHide: (lock) => root.requestLockUnlockAutoHide(lock)
                onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
            }

            // without our window buttons (fullscreen, system decorations),
            // fullscreen stands alone
            Widgets.CSDWindowButton {
                id: fullscreenButton

                height: parent.height

                visible: !csdButtons.visible

                focusPolicy: Qt.TabFocus

                color: theme.fg.primary
                hoverColor: theme.bg.primary.alpha(0.5)
                isThemeDark: theme.palette.isDark

                iconTxt: root._fullscreen ? VLCIcons.fullscreen_active : VLCIcons.fullscreen
                description: root._fullscreen ? qsTr("Exit fullscreen") : qsTr("Fullscreen")
                shortcut: MainCtx.actionShortcut("toggle-fullscreen")

                AccessibleCompat.id: "topBarFullscreenButton"

                Navigation.parentItem: root

                onClicked: root.toggleFullscreen()

                onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
            }

            Widgets.CSDWindowButtonSet {
                id: csdButtons

                height: parent.height

                visible: root.showCSD
                enabled: root.showCSD

                showFullscreen: true
                fullscreenChecked: root._fullscreen
                fullscreenShortcut: MainCtx.actionShortcut("toggle-fullscreen")

                onFullscreenClicked: root.toggleFullscreen()

                onButtonHoveredChanged: root.requestLockUnlockAutoHide(buttonHovered)
            }
        }
    }
}
