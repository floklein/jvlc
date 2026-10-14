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
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Playlist
import VLC.Util

// The folder of the playing media (its Show), named after it. Hidden for
// streams and discs.
T.Button {
    id: root

    property bool paintOnly: false

    readonly property string folderName: paintOnly ? qsTr("Folder")
                                                   : Helpers.folderName(Player.url)

    signal requestLockUnlockAutoHide(bool lock)

    implicitWidth: Math.min(implicitContentWidth + leftPadding + rightPadding,
                            VLCStyle.dp(240, VLCStyle.scale))
    implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

    // In a narrow window the folder name truncates once the title is gone
    // (and in the Control bar, once the controls have folded), then the
    // button hides. The bar counts it at its full width.
    Layout.minimumWidth: paintOnly ? -1 : Math.min(implicitWidth, VLCStyle.dp(80, VLCStyle.scale))
    readonly property var foldWidths: [implicitWidth]

    // an empty folder button takes no room
    visible: folderName !== ""

    rightPadding: VLCStyle.spacing_4
    spacing: VLCStyle.spacing_4

    enabled: !paintOnly
    focusPolicy: Qt.TabFocus
    hoverEnabled: true

    text: folderName

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Folder %1").arg(folderName)
    AccessibleCompat.id: "topBarFolderButton"

    T.ToolTip.visible: (hovered || visualFocus)
    T.ToolTip.delay: VLCStyle.delayToolTipAppear
    T.ToolTip.text: qsTr("Show %1 in the library").arg(folderName)

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    onHoveredChanged: root.requestLockUnlockAutoHide(hovered)

    // opens the Library pane at the folder of the playing file
    onClicked: {
        LibraryPages.showFolder(mediaFolder.folderId, Player.hasVideoOutput, Qt.MouseFocusReason)
        LibraryPages.paneOpen = true
    }

    // the folder in the library, found by its path
    MLMediaFolder {
        id: mediaFolder

        ml: MainCtx.mediaLibrary
        url: root.paintOnly ? "" : Player.url
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        colorSet: ColorContext.ToolButton

        enabled: root.enabled || root.paintOnly
        focused: root.visualFocus
        hovered: root.hovered
        pressed: root.down
    }

    contentItem: RowLayout {
        spacing: root.spacing

        Widgets.IconLabel {
            Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
            Layout.fillHeight: true

            text: VLCIcons.folder
            color: theme.fg.primary
            font.pixelSize: VLCStyle.icon_normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        T.Label {
            Layout.fillWidth: true

            text: root.text
            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        radius: VLCStyle.radius_normal
        color: theme.bg.primary
        border.color: root.visualFocus ? theme.visualFocus : "transparent"
    }
}
