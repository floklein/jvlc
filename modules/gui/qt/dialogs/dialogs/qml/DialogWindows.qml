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

// The dialogs that are windows of their own. They live as long as the main
// interface, whatever view it shows, and load on first use.
Item {
    id: root

    function _show(loader) {
        loader.active = true
        loader.item.show()
        loader.item.raise()
        loader.item.requestActivate()
    }

    Loader {
        id: aboutLoader
        active: false
        source: "qrc:///qt/qml/VLC/Dialogs/AboutWindow.qml"
    }

    Loader {
        id: messagesLoader
        active: false
        source: "qrc:///qt/qml/VLC/Dialogs/MessagesWindow.qml"
    }

    Loader {
        id: programGuideLoader
        active: false
        source: "qrc:///qt/qml/VLC/Dialogs/ProgramGuideWindow.qml"
    }

    Connections {
        target: DialogsProvider

        function onShowAbout() { root._show(aboutLoader) }
        function onShowMessages() { root._show(messagesLoader) }
        function onShowProgramGuide() { root._show(programGuideLoader) }
    }
}
