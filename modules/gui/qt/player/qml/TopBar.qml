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
import VLC.Widgets as Widgets
import VLC.Menus as Menus
import VLC.Util

// The player's Top bar (Figma 247:447): the cone, the VLC menu, the library
// toggle and the folder button on the left, the media title in the center,
// and the window buttons with fullscreen on the right. It is also the title
// bar of the window when VLC draws its own decorations.
FocusScope {
    id: root

    // Properties

    property int topMargin: 0
    property int sideMargin: 0

    property string title

    // the Top bar is the window's title bar: drag, double-click and the window buttons
    property bool showCSD: false
    // the classic menu bar (F10), above the Top bar
    property bool showToolbar: false
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

    // Aliases

    property alias resumeVisible: resumeDialog.visible

    // Signals

    signal requestLockUnlockAutoHide(bool lock)
    // the library toggle and the folder button leave the player for now
    // TODO: open the Library pane (#65) at the playing Show (#59)
    signal backRequested()

    ///incomming signal to request all menu and popup to close
    signal forceUnlock()

    // Settings

    implicitHeight: topMargin
                    + (menubar.visible ? menubar.implicitHeight : 0)
                    + Math.max(_barHeight,
                               resumeDialog.visible ? resumeDialog.implicitHeight + VLCStyle.dp(12, VLCStyle.scale) : 0)

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

    Menus.Menubar {
        id: menubar

        anchors.top: parent.top
        anchors.topMargin: root.topMargin
        anchors.left: parent.left
        anchors.leftMargin: root.sideMargin
        anchors.right: parent.right

        visible: root.showToolbar
        enabled: root.showToolbar

        onHoveredChanged: menu => root.requestLockUnlockAutoHide(hovered)
        onMenuOpenedChanged: menu => root.requestLockUnlockAutoHide(menuOpened)
    }

    Item {
        id: bar

        anchors.top: menubar.visible ? menubar.bottom : parent.top
        anchors.topMargin: menubar.visible ? 0 : root.topMargin
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // Left: cone, VLC menu, library toggle and folder button
        RowLayout {
            id: leftGroup

            anchors.left: parent.left
            anchors.leftMargin: root._padding
            anchors.verticalCenter: parent.verticalCenter

            spacing: VLCStyle.dp(7, VLCStyle.scale)

            visible: !root.resumeVisible

            focus: true

            // the cone opens the window's system menu
            T.Button {
                id: cone

                Layout.preferredWidth: VLCStyle.dp(38, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)

                focusPolicy: Qt.TabFocus

                Accessible.role: Accessible.Button
                Accessible.name: Helpers.accessibleName(qsTr("Window menu"),
                                                        root.showCSD ? qsTr("Alt+Space") : "")
                AccessibleCompat.id: "topBarWindowMenuButton"

                Navigation.parentItem: root
                Navigation.rightItem: menuButton

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

            RowLayout {
                spacing: VLCStyle.spacing_10

                Widgets.IconToolButton {
                    id: menuButton

                    Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)

                    focus: true

                    text: VLCIcons.more
                    description: qsTr("VLC menu")

                    AccessibleCompat.id: "topBarMenuButton"

                    checked: vlcMenu.shown

                    Navigation.parentItem: root
                    Navigation.leftItem: cone
                    Navigation.rightItem: libraryButton

                    onClicked: vlcMenu.popup(this.mapToGlobal(0, height))

                    onHoveredChanged: root.requestLockUnlockAutoHide(hovered)

                    // TODO: the redesign's VLC menu (#40)
                    Menus.QmlGlobalMenu {
                        id: vlcMenu

                        ctx: MainCtx

                        onAboutToShow: root.requestLockUnlockAutoHide(true)
                        onAboutToHide: root.requestLockUnlockAutoHide(false)
                    }

                    Connections {
                        target: root

                        function onForceUnlock() {
                            vlcMenu.close()
                        }
                    }
                }

                Widgets.IconToolButton {
                    id: libraryButton

                    Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)

                    text: VLCIcons.sidebar_left
                    description: qsTr("Library")

                    AccessibleCompat.id: "topBarLibraryButton"

                    Navigation.parentItem: root
                    Navigation.leftItem: menuButton
                    Navigation.rightItem: folderButton.visible ? folderButton : null

                    onClicked: root.backRequested()

                    onHoveredChanged: root.requestLockUnlockAutoHide(hovered)
                }
            }

            // Divider
            Item {
                Layout.preferredWidth: VLCStyle.icon_normal
                Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)

                visible: folderButton.visible

                Rectangle {
                    anchors.centerIn: parent

                    width: VLCStyle.border
                    height: VLCStyle.dp(16, VLCStyle.scale)

                    color: theme.separator
                }
            }

            // the folder of the playing media (its Show)
            T.Button {
                id: folderButton

                readonly property string folderName: Helpers.folderName(Player.url)

                Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)
                Layout.maximumWidth: VLCStyle.dp(240, VLCStyle.scale)

                visible: folderName !== ""

                rightPadding: VLCStyle.spacing_4
                spacing: VLCStyle.spacing_4

                focusPolicy: Qt.TabFocus
                hoverEnabled: true

                implicitWidth: implicitContentWidth + leftPadding + rightPadding
                implicitHeight: implicitContentHeight + topPadding + bottomPadding

                text: folderName

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Folder %1").arg(folderName)
                AccessibleCompat.id: "topBarFolderButton"

                T.ToolTip.visible: (hovered || visualFocus)
                T.ToolTip.delay: VLCStyle.delayToolTipAppear
                T.ToolTip.text: qsTr("Show %1 in the library").arg(folderName)

                Navigation.parentItem: root
                Navigation.leftItem: libraryButton

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                onClicked: root.backRequested()

                onHoveredChanged: root.requestLockUnlockAutoHide(hovered)

                readonly property ColorContext colorContext: ColorContext {
                    id: folderTheme

                    colorSet: ColorContext.ToolButton

                    focused: folderButton.visualFocus
                    hovered: folderButton.hovered
                    pressed: folderButton.down
                }

                contentItem: RowLayout {
                    spacing: folderButton.spacing

                    Widgets.IconLabel {
                        Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                        Layout.fillHeight: true

                        text: VLCIcons.folder
                        color: folderTheme.fg.primary
                        font.pixelSize: VLCStyle.icon_normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    T.Label {
                        Layout.fillWidth: true

                        text: folderButton.text
                        color: folderTheme.fg.primary
                        font.pixelSize: VLCStyle.fontSize_normal
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                background: Widgets.AnimatedBackground {
                    enabled: folderTheme.initialized
                    radius: VLCStyle.radius_normal
                    color: folderTheme.bg.primary
                    border.color: folderButton.visualFocus ? folderTheme.visualFocus : "transparent"
                }
            }
        }

        ResumeDialog {
            id: resumeDialog

            anchors.left: parent.left
            anchors.leftMargin: root._padding + VLCStyle.margin_xsmall
            anchors.verticalCenter: parent.verticalCenter

            maxWidth: rightGroup.x - anchors.leftMargin - VLCStyle.margin_large

            colorContext.palette: theme.palette

            Navigation.parentItem: rootPlayer

            onHidden: {
                if (activeFocus) {
                    menuButton.forceActiveFocus()
                }
            }

            onVisibleChanged: {
                root.requestLockUnlockAutoHide(visible)
            }
        }

        // the media title, centered on the window, as long as it doesn't
        // overlap the side groups
        T.Label {
            id: titleText

            readonly property real _leftLimit: leftGroup.x + leftGroup.width + VLCStyle.margin_normal
            readonly property real _rightLimit: rightGroup.x - VLCStyle.margin_normal

            x: Helpers.clamp((parent.width - width) / 2, _leftLimit, Math.max(_leftLimit, _rightLimit - width))
            anchors.verticalCenter: parent.verticalCenter

            width: Math.max(0, Math.min(implicitWidth, _rightLimit - _leftLimit))

            visible: !root.resumeVisible

            text: root.title !== "" ? root.title : qsTr("VLC Media Player")
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter

            Accessible.role: Accessible.TitleBar
            Accessible.name: text
        }

        // Right: window buttons, with fullscreen next to maximize
        Row {
            id: rightGroup

            anchors.right: parent.right
            anchors.rightMargin: root._padding
            anchors.verticalCenter: parent.verticalCenter

            height: VLCStyle.dp(24, VLCStyle.scale)

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
