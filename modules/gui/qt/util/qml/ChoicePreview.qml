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

/**
 * Previews the choices of an open list: the hovered (or arrowed to) row
 * applies at once, closing the list without a pick goes back to the row
 * applied before.
 *
 * The list calls begin(current) when it opens, hover(row) when its
 * highlighted row changes, commit(row) on a pick and cancel() when it closes.
 * It applies a row on applyRequested(row).
 */
QtObject {
    id: preview

    // apply this row
    signal applyRequested(int row)

    // the list is open
    property bool previewing: false

    // the row applied before the list opened, -1 for none
    property int _committed: -1
    // the row applied now
    property int _applied: -1

    function begin(row : int) {
        previewing = true
        _committed = row
        _applied = row
    }

    function hover(row : int) {
        if (!previewing || row < 0 || row === _applied)
            return
        _applied = row
        applyRequested(row)
    }

    function commit(row : int) {
        const applied = previewing ? _applied : _committed
        previewing = false
        _committed = row
        _applied = row
        if (row !== applied)
            applyRequested(row)
    }

    function cancel() {
        if (!previewing)
            return
        previewing = false
        // nothing to go back to: the preview stays
        if (_committed >= 0 && _applied !== _committed)
            applyRequested(_committed)
        _applied = _committed
    }
}
