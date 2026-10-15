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

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Menus as Menus
import VLC.Util

// The "…" VLC menu. Every Layout keeps it, so Settings stay reachable.
Widgets.IconToolButton {
    id: root

    signal requestLockUnlockAutoHide(bool lock)

    // Figma: 28 px square
    implicitWidth: VLCStyle.dp(28, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

    text: VLCIcons.more
    description: qsTr("jVLC menu")

    AccessibleCompat.id: "topBarMenuButton"

    checked: vlcMenu.shown

    onClicked: vlcMenu.popup(this.mapToGlobal(0, height))

    onHoveredChanged: root.requestLockUnlockAutoHide(hovered)

    function forceUnlock() {
        vlcMenu.close()
    }

    // the player, whose Control bar folds controls into this menu in a
    // narrow window
    readonly property var _player: ((typeof rootPlayer !== 'undefined') && (rootPlayer !== null))
                                   ? rootPlayer
                                   : null

    Menus.QmlGlobalMenu {
        id: vlcMenu

        ctx: MainCtx

        // the folded controls come first
        foldedControls: root.paintOnly ? [] : (root._player?.foldedControls ?? [])

        onAboutToShow: root.requestLockUnlockAutoHide(true)
        onAboutToHide: root.requestLockUnlockAutoHide(false)

        // after the menu closed, so that what it opens isn't closed by the
        // click that picked it
        onFoldedControlTriggered: (control) => {
            if (root._player)
                Qt.callLater(root._player.triggerFoldedControl, control)
        }
    }
}
