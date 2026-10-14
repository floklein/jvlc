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
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQuick.Window

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style

// A row of the Network section: an icon (or an artwork) and a name.
// Up and Down move between the rows of the page.
T.ItemDelegate {
    id: root

    property string iconText: ""
    property string artwork: ""
    property string subtitle: ""
    // shows a "…" button that requests the row's menu
    property bool hasMenu: false

    signal menuRequested(point globalPos)

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: Math.max(VLCStyle.dp(44, VLCStyle.scale),
                             implicitContentHeight + topPadding + bottomPadding)

    leftPadding: VLCStyle.spacing_8
    rightPadding: VLCStyle.spacing_8
    spacing: VLCStyle.spacing_12

    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.ListItem
    Accessible.name: text
    Accessible.onPressAction: root.clicked()

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Item

        focused: root.visualFocus
        hovered: root.hovered
        pressed: root.down
        enabled: root.enabled
    }

    Keys.onUpPressed: (event) => root._moveFocus(false, event)
    Keys.onDownPressed: (event) => root._moveFocus(true, event)
    Keys.onMenuPressed: root.menuRequested(root.mapToGlobal(root.width / 2, root.height / 2))

    function _moveFocus(forward, event) {
        const next = root.nextItemInFocusChain(forward)
        // only between the rows of the page
        if (next && next !== root && next.objectName === root.objectName) {
            next.forceActiveFocus(forward ? Qt.TabFocusReason : Qt.BacktabFocusReason)
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    objectName: "networkRow"

    background: Rectangle {
        color: (root.hovered || root.visualFocus || root.down) ? theme.bg.highlight : "transparent"
        radius: VLCStyle.radius_small
        border.width: root.visualFocus ? VLCStyle.focus_border : 0
        border.color: theme.visualFocus
    }

    contentItem: RowLayout {
        spacing: root.spacing

        Item {
            Layout.preferredWidth: VLCStyle.icon_normal
            Layout.preferredHeight: VLCStyle.icon_normal

            Widgets.IconLabel {
                anchors.centerIn: parent
                visible: artworkImage.status !== Image.Ready
                text: root.iconText
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.icon_normal
            }

            Image {
                id: artworkImage

                anchors.fill: parent
                visible: status === Image.Ready
                source: root.artwork
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }

        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true

            Widgets.ListLabel {
                text: root.text
                color: theme.fg.primary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Widgets.CaptionLabel {
                visible: text !== ""
                text: root.subtitle
                color: theme.fg.secondary
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
        }

        Widgets.IconToolButton {
            id: menuButton

            visible: root.hasMenu
            opacity: (root.hovered || root.activeFocus || menuButton.hovered) ? 1 : 0
            text: VLCIcons.more
            description: qsTr("Menu")
            focusPolicy: Qt.NoFocus
            font.pixelSize: VLCStyle.icon_normal

            Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)

            onClicked: root.menuRequested(menuButton.mapToGlobal(0, menuButton.height))
        }
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => {
            root.forceActiveFocus(Qt.MouseFocusReason)
            root.menuRequested(root.mapToGlobal(eventPoint.position.x, eventPoint.position.y))
        }
    }
}
