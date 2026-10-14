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

// How the Control bar folds in a narrow window: the duration, then the
// current time, then the volume cone collapses to the speaker, then audio,
// subtitles and adjustments fold into the VLC menu.
TestCase {
    id: root
    name: "ControlBarFolding"

    // The default video Layout at 100% scale, 8 px between items and 20 px
    // between zones. An item is a width, or its widths at each fold level.
    //
    // left:   previous 30, play 40, next 30                     = 116
    // center: seek bar 64 + time 76 + 8 + duration 76 + 8       = 232
    // right:  volume 30 + 8 + 88, audio, subtitles, adjustments
    //         30 each, Queue 30, 4 spaces                        = 278
    // full width: 116 + 232 + 278 + 2 * 20                       = 666
    // without the duration (- 84)                                = 582
    // without the current time (- 84)                            = 498
    // with the speaker only (- 96)                               = 402
    // with audio, subtitles and adjustments folded (- 3 * 38)    = 288
    function defaultLayout(scale) {
        const s = (w) => w * scale
        const seekBar = [s(232), s(148), s(64)]
        const volume = [s(126), s(126), s(126), s(30)]
        const trackButton = [s(30), s(30), s(30), s(30), 0]
        return [
            [s(30), s(40), s(30)],
            [seekBar],
            [volume, trackButton, trackButton, trackButton, s(30)]
        ]
    }

    ControlBarFolding {
        id: folding

        spacing: 8
        zoneSpacing: 20
        zones: root.defaultLayout(1)
    }

    function init() {
        folding.spacing = 8
        folding.zoneSpacing = 20
        folding.centered = false
        folding.zones = root.defaultLayout(1)
    }

    function test_requiredWidthOfEachLevel_data() {
        return [
            { tag: "full", level: ControlBarFolding.Full, width: 666 },
            { tag: "no duration", level: ControlBarFolding.NoDuration, width: 582 },
            { tag: "no current time", level: ControlBarFolding.NoTime, width: 498 },
            { tag: "speaker only", level: ControlBarFolding.CompactVolume, width: 402 },
            { tag: "tracks folded", level: ControlBarFolding.TracksFolded, width: 288 },
        ]
    }

    function test_requiredWidthOfEachLevel(data) {
        compare(folding.requiredWidth(data.level), data.width)
    }

    function test_foldsInTheDecidedOrder_data() {
        return [
            { tag: "wide", availableWidth: 1200, level: ControlBarFolding.Full },
            { tag: "just fits", availableWidth: 666, level: ControlBarFolding.Full },
            { tag: "one pixel short", availableWidth: 665, level: ControlBarFolding.NoDuration },
            { tag: "no room for the time", availableWidth: 497, level: ControlBarFolding.CompactVolume },
            { tag: "no room for the cone", availableWidth: 401, level: ControlBarFolding.TracksFolded },
            { tag: "between time and duration", availableWidth: 500, level: ControlBarFolding.NoTime },
            { tag: "narrowest", availableWidth: 100, level: ControlBarFolding.TracksFolded },
        ]
    }

    function test_foldsInTheDecidedOrder(data) {
        folding.availableWidth = data.availableWidth
        compare(folding.level, data.level)
    }

    // at 200% the controls are twice as wide: the same window folds earlier
    // instead of truncating
    function test_largerScaleFoldsEarlier() {
        folding.availableWidth = 1000
        compare(folding.level, ControlBarFolding.Full)

        folding.zones = root.defaultLayout(2)
        folding.spacing = 16
        folding.zoneSpacing = 40
        // 1332, 1164, 996: only the duration and the time fold
        compare(folding.level, ControlBarFolding.NoTime)

        folding.availableWidth = 800
        // 804 at CompactVolume
        compare(folding.level, ControlBarFolding.TracksFolded)
    }

    // an empty zone takes no room and no spacing
    function test_emptyZoneTakesNoRoom() {
        folding.zones = [[30, 40, 30], [], [30]]
        // 100 + 2 * 8 + 30 + 20
        compare(folding.requiredWidth(ControlBarFolding.Full), 166)
    }

    // an item folded away at a level takes no spacing either
    function test_foldedItemTakesNoSpacing() {
        folding.zones = [[30, [30, 30, 30, 30, 0], 30]]
        compare(folding.requiredWidth(ControlBarFolding.CompactVolume), 106)
        compare(folding.requiredWidth(ControlBarFolding.TracksFolded), 68)
    }

    // a center zone that doesn't stretch stays centered: both sides get the
    // room of the wider one
    function test_centeredZoneNeedsTheWiderSideTwice() {
        folding.zones = [[30, 40, 30], [64], [30, 30]]
        // 116 + 64 + 68 + 2 * 20
        compare(folding.requiredWidth(ControlBarFolding.Full), 288)

        folding.centered = true
        // 64 + 2 * (116 + 20)
        compare(folding.requiredWidth(ControlBarFolding.Full), 336)
    }

    // a Layout without foldable items never folds what it doesn't have
    function test_layoutWithoutFoldableItems() {
        folding.zones = [[30, 40, 30]]
        folding.availableWidth = 50
        compare(folding.requiredWidth(ControlBarFolding.TracksFolded), 116)
        compare(folding.level, ControlBarFolding.TracksFolded)
    }
}
