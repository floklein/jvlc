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

import VLC.Dialogs

// Opens Keyboard Shortcuts over the window it is in. The main window, Settings
// and Onboarding each have one: the dialog opens over the window that asks
// for it, since a modal window (Onboarding) blocks the main window. The keys
// are loaded again each time it opens.
Loader {
    id: root

    // the window this one opens the dialog for
    property var window: Window.window
    // whether it also opens it when no window asks (none was focused)
    property bool fallback: false

    // what the dialog blurs behind it
    property Item backdrop: null

    signal aboutToHide()

    active: false
    source: "qrc:///qt/qml/VLC/Dialogs/KeyboardShortcutsDialog.qml"

    onLoaded: {
        item.rootWindow = Qt.binding(() => root.backdrop)
        item.aboutToHide.connect(root.aboutToHide)
    }

    Connections {
        target: DialogsProvider

        function onShowKeyboardShortcutsDialog(action, window) {
            if (window !== root.window && !(root.fallback && !window))
                return
            root.active = true
            // a popup can't open in the event loop iteration that creates it
            Qt.callLater(() => root.item?.openOn(action))
        }
    }

    Connections {
        target: root.item

        function onClosed() {
            // drops the model and its unsaved changes
            Qt.callLater(() => root.active = false)
        }
    }
}
