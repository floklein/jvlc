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

// Tells a single click from the first click of a double click.
//
// Feed it the raw events of one button: `move` for every pointer move (hover
// included), `press`, `release` and `doubleClick`. A press-release that is not
// a drag emits `singleClicked` once the double-click interval has passed
// without a second click.
QtObject {
    id: root

    property bool enabled: true

    // maximal delay between the clicks of a double click
    property int interval: Qt.styleHints.mouseDoubleClickInterval

    // a press that moves further than this is a drag, not a click
    property real dragThreshold: Qt.styleHints.startDragDistance

    signal singleClicked()

    function move(x, y) {
        _x = x
        _y = y

        if (_pressed && !_dragged
                && Math.hypot(_x - _pressX, _y - _pressY) > dragThreshold)
            _dragged = true
    }

    function press() {
        if (!enabled)
            return

        _pressed = true
        _dragged = false
        _pressX = _x
        _pressY = _y

        // second press within the interval: a double click
        if (_timer.running) {
            _timer.stop()
            _secondClick = true
        }
    }

    function release() {
        if (!_pressed)
            return

        _pressed = false

        if (_secondClick) {
            _secondClick = false
            return
        }

        if (enabled && !_dragged)
            _timer.restart()
    }

    function doubleClick() {
        _timer.stop()
        _secondClick = true
    }

    function cancel() {
        _timer.stop()
        _pressed = false
        _dragged = false
        _secondClick = false
    }

    onEnabledChanged: {
        if (!enabled)
            cancel()
    }

    // Private

    property real _x: 0
    property real _y: 0
    property real _pressX: 0
    property real _pressY: 0
    property bool _pressed: false
    property bool _dragged: false
    property bool _secondClick: false

    property Timer _timer: Timer {
        interval: root.interval
        repeat: false

        onTriggered: root.singleClicked()
    }
}
