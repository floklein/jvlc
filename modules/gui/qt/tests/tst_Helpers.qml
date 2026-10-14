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
}
