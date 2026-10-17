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
import QtQuick.Window


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Menus as Menus
import VLC.Util
import VLC.Dialogs

T.ToolBar {
    id: root

    Accessible.name: qsTr("Local top bar")
    AccessibleCompat.id: "localTopbar"

    property bool useAcrylic: true

    property bool _showCSD: MainCtx.clientSideDecoration
        && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    // the desktop puts the window buttons on the left (Linux)
    readonly property bool _csdOnLeft: _showCSD && MainCtx.windowButtonsOnLeft

    // the title, centered on the window
    property string title: ""

    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                             implicitContentWidth + leftPadding + rightPadding + (_showCSD ? csdButtons.implicitWidth : 0))

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Navigation
    }

    // Figma 6261:15080: "BG/A3 Transparent"
    background:  Widgets.AcrylicBackground {
        enabled: root.useAcrylic
        tintColor: theme.bg.secondary
    }

    contentItem: Item {

        implicitHeight: VLCStyle.globalToolbar_height
        implicitWidth: navigationButtons.implicitWidth
                     + playqueueButtonsTop.implicitWidth
                     + VLCStyle.titleBar_padding * 2

        Item {
            id: navigationButtons

            implicitWidth: navigationButtonsRow.implicitWidth

            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
                leftMargin: VLCStyle.titleBar_padding
                    + ((root._csdOnLeft && root.topPadding < csdButtons.height)
                        ? Math.max(csdButtons.width + VLCStyle.titleBar_padding - root.leftPadding, 0)
                        : 0)
            }

            Navigation.parentItem: root

            // Figma 6261:15081: the cone 7 px inside its 38 px box, then the
            // jVLC menu in a 28 px box, 7 px apart
            Row {
                id: navigationButtonsRow

                anchors.verticalCenter: parent.verticalCenter
                spacing: VLCStyle.titleBar_spacing

                Navigation.parentItem: root
                Navigation.rightItem: playqueueButtonsTop

                // the cone, then the VLC menu, as in the Top bar of the
                // player (Figma 6261:15078, 970:3308)
                Item {
                    anchors.verticalCenter: parent.verticalCenter

                    width: VLCStyle.titleBar_coneWidth
                    height: VLCStyle.titleBar_buttonSize

                    // Figma 6261:15083: the flat cone, "Accent/Logo"
                    Widgets.IconLabel {
                        anchors.centerIn: parent

                        visible: !MainCtx.useXmasCone()

                        text: VLCIcons.cone
                        color: theme.fg.link
                        font.pixelSize: VLCStyle.icon_normal
                    }

                    Widgets.BannerCone {
                        id: logo

                        anchors.centerIn: parent

                        visible: MainCtx.useXmasCone()

                        sourceSize.width: VLCStyle.icon_normal
                        sourceSize.height: VLCStyle.icon_normal
                        color: theme.accent
                    }
                }

                Widgets.IconToolButton {
                    id: menuBtn

                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: VLCStyle.icon_banner
                    text: VLCIcons.more
                    description: qsTr("jVLC menu")
                    width: VLCStyle.titleBar_buttonSize
                    height: VLCStyle.titleBar_buttonSize
                    checked: contextMenu.shown

                    focus: true

                    Navigation.parentItem: navigationButtons
                    Navigation.rightItem: playqueueButtonsTop

                    onClicked: contextMenu.popup(this.mapToGlobal(0, height))

                    Menus.QmlGlobalMenu {
                        id: contextMenu
                        ctx: MainCtx
                    }
                }

                // NOTE: Back and forward are in the header of the library
                //       pages; the sidebar shows and hides from the jVLC menu.
            }
        }

        Item {
            id: playqueueButtonsTop

            implicitWidth: playqueueButtonsTopRow.implicitWidth

            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
                //put the buttons below the CSD if we have enough space
                rightMargin: VLCStyle.titleBar_padding
                    + ((root._showCSD && !root._csdOnLeft && root.topPadding < csdButtons.height)
                        ? Math.max(csdButtons.width + VLCStyle.titleBar_padding - root.rightPadding, 0)
                        : 0)
            }

            Navigation.parentItem: root

            Row {
                id: playqueueButtonsTopRow

                anchors.verticalCenter: parent.verticalCenter
                spacing: VLCStyle.titleBar_spacing

                Navigation.parentItem: root
                Navigation.leftItem: navigationButtons

                // Figma 6261:15088 has only the window buttons: the Queue
                // opens from the Mini player and the player
                ConversionsIndicator { }
            }
        }

        // the library location, centered on the window, as long as it
        // doesn't overlap the buttons
        T.Label {
            id: titleText

            readonly property real _leftLimit: navigationButtons.x + navigationButtons.width + VLCStyle.margin_normal
            readonly property real _rightLimit: playqueueButtonsTop.x - VLCStyle.margin_normal
            // the content item starts at the left padding of the bar
            readonly property real _center: (root.width / 2) - root.leftPadding

            x: Helpers.clamp(_center - width / 2, _leftLimit, Math.max(_leftLimit, _rightLimit - width))
            anchors.verticalCenter: parent.verticalCenter

            width: Math.max(0, Math.min(implicitWidth, _rightLimit - _leftLimit))

            visible: text !== ""

            text: root.title
            // Figma 6261:15087: FG/A2
            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter

            Accessible.role: Accessible.TitleBar
            Accessible.name: text
        }

        Loader {
            z: -1

            anchors.fill: parent
            active: root._showCSD
            sourceComponent: Widgets.CSDTitlebarTapNDrapHandler {
            }
        }
    }

    Loader {
        id: csdButtons

        //csd button are
        parent:  root
        // Figma 6261:15088: inside the 6 px padding of the bar
        anchors {
            left: root._csdOnLeft ? parent.left : undefined
            right: root._csdOnLeft ? undefined : parent.right
            leftMargin: VLCStyle.titleBar_padding
            rightMargin: VLCStyle.titleBar_padding
            top: parent.top
        }

        height: VLCStyle.globalToolbar_height
        active: root._showCSD
        source: VLCStyle.palette.hasCSDImage
                  ? "qrc:///qt/qml/VLC/Widgets/CSDThemeButtonSet.qml"
                  : "qrc:///qt/qml/VLC/Widgets/CSDWindowButtonSet.qml"

        // fullscreen next to maximize, as in the player's Top bar (Figma
        // 6261:15080)
        readonly property bool _fullscreen: MainCtx.intfMainWindow.visibility === Window.FullScreen

        onLoaded: {
            if (item.showFullscreen === undefined)
                return
            item.showFullscreen = true
            item.fullscreenChecked = Qt.binding(() => csdButtons._fullscreen)
            item.fullscreenShortcut = Qt.binding(() => MainCtx.keyBindingsRevision >= 0
                                                       ? MainCtx.actionShortcut("toggle-fullscreen") : "")
            item.fullscreenClicked.connect(MainCtx.toggleInterfaceFullScreen)
        }
    }
}
