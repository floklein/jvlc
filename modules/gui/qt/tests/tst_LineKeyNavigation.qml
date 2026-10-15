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
import QtTest

import VLC.Util

// The lyric lines are reached and activated from the keyboard (spec #30,
// ticket #84): the arrows move between the lines, Enter seeks to the line,
// and the arrows leave the list at its ends.
Item {
    width: 200
    height: 200

    // what the lines would get: the focused item passes its keys on
    Item {
        id: lines

        anchors.fill: parent

        focus: true

        Keys.onPressed: (event) => { event.accepted = navigation.handleKey(event.key) }
    }

    LineKeyNavigation {
        id: navigation

        count: 3
    }

    SignalSpy {
        id: activatedSpy

        target: navigation
        signalName: "activated"
    }

    SignalSpy {
        id: leftUpSpy

        target: navigation
        signalName: "leftUp"
    }

    SignalSpy {
        id: leftDownSpy

        target: navigation
        signalName: "leftDown"
    }

    TestCase {
        name: "LineKeyNavigation"

        when: windowShown

        function init() {
            navigation.count = 3
            navigation.currentIndex = 0
            activatedSpy.clear()
            leftUpSpy.clear()
            leftDownSpy.clear()
            lines.forceActiveFocus()
        }

        function test_arrowsMoveBetweenLines() {
            keyClick(Qt.Key_Down)
            compare(navigation.currentIndex, 1)
            keyClick(Qt.Key_Down)
            compare(navigation.currentIndex, 2)
            keyClick(Qt.Key_Up)
            compare(navigation.currentIndex, 1)
        }

        function test_homeAndEndGoToTheFirstAndLastLine() {
            keyClick(Qt.Key_End)
            compare(navigation.currentIndex, 2)
            keyClick(Qt.Key_Home)
            compare(navigation.currentIndex, 0)
        }

        function test_enterActivatesTheCurrentLine() {
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Return)
            compare(activatedSpy.count, 1)
            compare(activatedSpy.signalArguments[0][0], 1)

            keyClick(Qt.Key_Enter)
            compare(activatedSpy.count, 2)
        }

        function test_upOnTheFirstLineLeavesTheList() {
            keyClick(Qt.Key_Up)
            compare(leftUpSpy.count, 1)
            compare(navigation.currentIndex, 0)
        }

        function test_downOnTheLastLineLeavesTheList() {
            navigation.currentIndex = 2
            keyClick(Qt.Key_Down)
            compare(leftDownSpy.count, 1)
            compare(navigation.currentIndex, 2)
        }

        // the next line to reach when the current one is gone
        function test_currentLineStaysInTheList() {
            navigation.currentIndex = 2
            navigation.count = 1
            compare(navigation.currentIndex, 0)
        }

        function test_noLines() {
            navigation.count = 0
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Return)
            compare(activatedSpy.count, 0)
            compare(leftDownSpy.count, 1)
        }

        // the player's hotkeys keep the other keys
        function test_otherKeysAreNotTaken() {
            verify(!navigation.handleKey(Qt.Key_Left))
            verify(!navigation.handleKey(Qt.Key_Space))
            verify(!navigation.handleKey(Qt.Key_A))
            verify(navigation.handleKey(Qt.Key_Down))
        }
    }
}
