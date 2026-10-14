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

// Jump to Time reads what the user types with Helpers.parseTime()
TestCase {
    name: "JumpToTime"

    function test_parseTime_data() {
        return [
            { tag: "seconds", text: "42", seconds: 42 },
            { tag: "minutes and seconds", text: "12:34", seconds: 754 },
            { tag: "hours", text: "1:02:03", seconds: 3723 },
            { tag: "spaces", text: " 1:02:03 ", seconds: 3723 },
            { tag: "zero", text: "0:00", seconds: 0 },
            { tag: "minutes past an hour", text: "90:00", seconds: 5400 },
            { tag: "empty", text: "", seconds: -1 },
            { tag: "letters", text: "1:ab", seconds: -1 },
            { tag: "seconds out of range", text: "1:60", seconds: -1 },
            { tag: "minutes out of range", text: "1:60:00", seconds: -1 },
            { tag: "too many fields", text: "1:2:3:4", seconds: -1 },
            { tag: "empty field", text: "1::3", seconds: -1 },
            { tag: "negative", text: "-5", seconds: -1 },
        ]
    }

    function test_parseTime(data) {
        compare(Helpers.parseTime(data.text), data.seconds)
    }
}
