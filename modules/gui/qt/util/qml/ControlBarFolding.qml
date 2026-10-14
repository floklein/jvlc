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
import QtQml

// How far the Control bar folds in a narrow window. Less important controls
// fold in a decided order until the bar fits:
// the duration, the current time, the volume cone (down to the speaker),
// then audio, subtitles and adjustments into the VLC menu.
// Previous, play, next, the seek bar and Queue never fold.
//
// The bar's zones (left, center, right) list their controls. A control is a
// width, or its widths at each level from Full (the last one repeats); 0 means
// folded away. Widths are in pixels, so a larger interface scale folds earlier.
QtObject {
    id: root

    enum Level {
        Full,
        NoDuration,
        NoTime,
        CompactVolume,
        TracksFolded
    }

    // Properties

    property real availableWidth: Number.MAX_VALUE

    // [zone][control] -> width, or [width at each level]
    property var zones: []

    // between the controls of a zone
    property real spacing: 0
    // between two zones
    property real zoneSpacing: 0

    // the center zone (zones[1]) is centered on the bar instead of filling
    // the room between the sides
    property bool centered: false

    // the first level at which the bar fits, the last one if none does
    readonly property int level: {
        for (let level = ControlBarFolding.Full; level < ControlBarFolding.TracksFolded; ++level) {
            if (requiredWidth(level) <= availableWidth)
                return level
        }
        return ControlBarFolding.TracksFolded
    }

    // Functions

    // the width the bar needs at a fold level
    function requiredWidth(level) {
        const widths = zones.map((zone) => _zoneWidth(zone, level))

        if (centered && widths.length === 3) {
            // each side, with its spacing to the center, gets half of the room
            // the center leaves
            const side = (width) => width > 0 ? width + zoneSpacing : 0
            return widths[1] + 2 * Math.max(side(widths[0]), side(widths[2]))
        }

        const used = widths.filter((width) => width > 0)
        if (used.length === 0)
            return 0
        return used.reduce((total, width) => total + width, 0)
               + (used.length - 1) * zoneSpacing
    }

    // Private

    function _controlWidth(control, level) {
        if (Array.isArray(control))
            return control.length > 0 ? control[Math.min(level, control.length - 1)] : 0
        return control
    }

    function _zoneWidth(zone, level) {
        let width = 0
        let count = 0
        for (const control of zone) {
            const controlWidth = _controlWidth(control, level)
            if (controlWidth <= 0)
                continue
            width += controlWidth
            ++count
        }
        return count > 0 ? width + (count - 1) * spacing : 0
    }
}
