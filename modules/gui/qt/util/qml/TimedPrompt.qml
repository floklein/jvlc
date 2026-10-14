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

// The lifetime of a prompt that goes away by itself after `duration`.
//
// While `held` (pointed at or focused), the countdown stops; once released,
// the prompt gets a full `duration` again.
QtObject {
    id: root

    property int duration: 10000

    property bool held: false

    readonly property bool shown: _shown

    // the prompt went away by itself
    signal expired()

    function show() {
        _shown = true
        _restart()
    }

    function hide() {
        _timer.stop()
        _shown = false
    }

    onHeldChanged: _restart()

    // Private

    property bool _shown: false

    function _restart() {
        if (_shown && !held)
            _timer.restart()
        else
            _timer.stop()
    }

    property Timer _timer: Timer {
        interval: root.duration
        repeat: false

        onTriggered: {
            root._shown = false
            root.expired()
        }
    }
}
