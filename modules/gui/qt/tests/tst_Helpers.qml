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

TestCase {
    name: "Helpers"

    // The Top bar's folder button is named after the folder of the playing file
    function test_folderName_data() {
        return [
            { tag: "windows path", url: "file:///C:/Videos/The%20Wand/S01/The_Wand_S01E04.mp4", folder: "S01" },
            { tag: "unix path", url: "file:///home/me/Videos/The%20Wand/ep.mkv", folder: "The Wand" },
            { tag: "unicode", url: "file:///home/me/S%C3%A9ries/ep.mkv", folder: "Séries" },
            { tag: "drive root", url: "file:///C:/ep.mkv", folder: "" },
            { tag: "filesystem root", url: "file:///ep.mkv", folder: "" },
            { tag: "network stream", url: "https://example.org/live/stream.m3u8", folder: "" },
            { tag: "disc", url: "dvd:///D:/", folder: "" },
            { tag: "empty", url: "", folder: "" },
        ]
    }

    function test_folderName(data) {
        compare(Helpers.folderName(data.url), data.folder)
        compare(Helpers.folderName(Qt.url(data.url)), data.folder)
    }

    // Icon-only buttons are named by their tooltip plus shortcut ("Play, Space")
    function test_accessibleName_data() {
        return [
            { tag: "with shortcut", name: "Play", shortcut: "Space", result: "Play, Space" },
            { tag: "without shortcut", name: "Minimize", shortcut: "", result: "Minimize" },
        ]
    }

    function test_accessibleName(data) {
        compare(Helpers.accessibleName(data.name, data.shortcut), data.result)
    }

    Item {
        id: notice
    }

    // Notices are announced to screen readers, the focus stays where it is
    function test_announce() {
        // both platforms build with Qt >= 6.8, which has Accessible.announce
        verify(Helpers.announce(notice, "Added to the Queue"))
        verify(!Helpers.announce(notice, ""))
        verify(!Helpers.announce(null, "Added to the Queue"))
    }

    // The seek bar reads as "12:34 of 45:00"
    function test_seekBarName() {
        compare(Helpers.seekBarName("12:34", "45:00"), "12:34 of 45:00")
    }

    // Home and End go to the start and the end of the seek bar, Page Up and
    // Page Down move a tenth of it; other keys are not the seek bar's
    function test_seekKeyPosition_data() {
        return [
            { tag: "home", key: Qt.Key_Home, position: 0.5, result: 0 },
            { tag: "end", key: Qt.Key_End, position: 0.5, result: 1 },
            { tag: "page up", key: Qt.Key_PageUp, position: 0.5, result: 0.6 },
            { tag: "page down", key: Qt.Key_PageDown, position: 0.5, result: 0.4 },
            { tag: "page up near the end", key: Qt.Key_PageUp, position: 0.95, result: 1 },
            { tag: "page down near the start", key: Qt.Key_PageDown, position: 0.05, result: 0 },
            { tag: "arrow", key: Qt.Key_Right, position: 0.5, result: -1 },
            { tag: "letter", key: Qt.Key_A, position: 0.5, result: -1 },
        ]
    }

    function test_seekKeyPosition(data) {
        fuzzyCompare(Helpers.seekKeyPosition(data.key, data.position), data.result, 1e-9)
    }

    // The Top bar's title stays centered on the window, slides aside when it
    // meets a side zone, then truncates: the side zones keep their room until
    // the title has none left
    function test_centeredSpan_data() {
        return [
            { tag: "centered", center: 500, width: 200, left: 100, right: 900, x: 400, spanWidth: 200 },
            { tag: "slides right of the left zone", center: 250, width: 200, left: 200, right: 900, x: 200, spanWidth: 200 },
            { tag: "slides left of the right zone", center: 850, width: 200, left: 100, right: 900, x: 700, spanWidth: 200 },
            { tag: "truncates", center: 500, width: 300, left: 300, right: 500, x: 300, spanWidth: 200 },
            { tag: "no room", center: 500, width: 300, left: 400, right: 400, x: 400, spanWidth: 0 },
            // the side zones overlap: the left one gives way up to the right one
            { tag: "sides overlap", center: 500, width: 300, left: 500, right: 300, x: 300, spanWidth: 0 },
        ]
    }

    function test_centeredSpan(data) {
        const span = Helpers.centeredSpan(data.center, data.width, data.left, data.right)
        compare(span.x, data.x)
        compare(span.width, data.spanWidth)
    }

    // The Mini player goes to the corner nearest to where it is dropped
    function test_nearestCorner_data() {
        // a 400x300 Mini player in a 1920x1040 library
        return [
            { tag: "bottom right", x: 1400, y: 700, corner: Qt.BottomRightCorner },
            { tag: "bottom left", x: 100, y: 650, corner: Qt.BottomLeftCorner },
            { tag: "top left", x: 10, y: 20, corner: Qt.TopLeftCorner },
            { tag: "top right", x: 1500, y: 30, corner: Qt.TopRightCorner },
            { tag: "dropped past the edge", x: 2000, y: -100, corner: Qt.TopRightCorner },
            { tag: "left of the middle", x: 700, y: 300, corner: Qt.TopLeftCorner },
            { tag: "right of the middle", x: 800, y: 300, corner: Qt.TopRightCorner },
            { tag: "dead center", x: 760, y: 370, corner: Qt.BottomRightCorner },
        ]
    }

    function test_nearestCorner(data) {
        compare(Helpers.nearestCorner(Qt.rect(data.x, data.y, 400, 300), Qt.rect(0, 0, 1920, 1040)),
                data.corner)
    }

    function test_cornerPosition_data() {
        // a 400x300 Mini player, 8 px from the edges of a library below a 40 px header
        return [
            { tag: "bottom right", corner: Qt.BottomRightCorner, x: 1512, y: 772 },
            { tag: "bottom left", corner: Qt.BottomLeftCorner, x: 8, y: 772 },
            { tag: "top left", corner: Qt.TopLeftCorner, x: 8, y: 48 },
            { tag: "top right", corner: Qt.TopRightCorner, x: 1512, y: 48 },
        ]
    }

    function test_cornerPosition(data) {
        const pos = Helpers.cornerPosition(data.corner, Qt.size(400, 300), Qt.rect(0, 40, 1920, 1040), 8)
        compare(pos.x, data.x)
        compare(pos.y, data.y)
    }

    // The speed field of the Playback tab accepts 0.25x to 4x
    function test_parseSpeed_data() {
        return [
            { tag: "plain", text: "1.5", speed: 1.5 },
            { tag: "with x", text: "1.25x", speed: 1.25 },
            { tag: "with multiplication sign", text: " 3 × ", speed: 3 },
            { tag: "decimal comma", text: "0,75", speed: 0.75 },
            { tag: "maximum", text: "4", speed: 4 },
            { tag: "above the maximum", text: "8", speed: 4 },
            { tag: "below the minimum", text: "0.1", speed: 0.25 },
            { tag: "zero", text: "0", speed: 0.25 },
            { tag: "not a number", text: "fast", speed: -1 },
            { tag: "empty", text: "", speed: -1 },
        ]
    }

    function test_parseSpeed(data) {
        compare(Helpers.parseSpeed(data.text), data.speed)
    }

    function test_formatSpeed_data() {
        return [
            { tag: "normal", speed: 1, text: "1x" },
            { tag: "quarter", speed: 0.25, text: "0.25x" },
            { tag: "half", speed: 0.5, text: "0.5x" },
            { tag: "fine", speed: 1.04, text: "1.04x" },
            { tag: "rounded", speed: 1.2999999, text: "1.3x" },
            { tag: "fast", speed: 4, text: "4x" },
        ]
    }

    function test_formatSpeed(data) {
        compare(Helpers.formatSpeed(data.speed), data.text)
    }

    // F6 and Shift+F6 go through the areas of the player, wrapping around
    function test_nextFocusArea_data() {
        return [
            { tag: "next", current: 1, count: 4, backward: false, next: 2 },
            { tag: "previous", current: 1, count: 4, backward: true, next: 0 },
            { tag: "wraps forward", current: 3, count: 4, backward: false, next: 0 },
            { tag: "wraps backward", current: 0, count: 4, backward: true, next: 3 },
            { tag: "from none, forward", current: -1, count: 4, backward: false, next: 0 },
            { tag: "from none, backward", current: -1, count: 4, backward: true, next: 3 },
            { tag: "single area", current: 0, count: 1, backward: false, next: 0 },
            { tag: "no area", current: -1, count: 0, backward: false, next: -1 },
        ]
    }

    function test_nextFocusArea(data) {
        compare(Helpers.nextFocusArea(data.current, data.count, data.backward), data.next)
    }
}
