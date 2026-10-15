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

// The keyboard of a list of lines that is not a ListView, like the lyrics:
// Up and Down, Home and End move the current line, Enter activates it. Up on
// the first line and Down on the last leave the list. The other keys, Space
// included, are left to the line and the player's hotkeys.
QtObject {
    id: navigation

    property int count: 0
    property int currentIndex: 0

    signal activated(int index)
    signal leftUp()
    signal leftDown()

    onCountChanged: {
        if (currentIndex >= count)
            currentIndex = count - 1
        else if (currentIndex < 0 && count > 0)
            currentIndex = 0
    }

    // returns whether it took the key
    function handleKey(key: int) : bool {
        switch (key) {
        case Qt.Key_Up:
            if (currentIndex > 0)
                currentIndex = currentIndex - 1
            else
                navigation.leftUp()
            return true
        case Qt.Key_Down:
            if (currentIndex < count - 1)
                currentIndex = currentIndex + 1
            else
                navigation.leftDown()
            return true
        case Qt.Key_Home:
            if (count > 0)
                currentIndex = 0
            return true
        case Qt.Key_End:
            if (count > 0)
                currentIndex = count - 1
            return true
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (currentIndex >= 0 && currentIndex < count)
                navigation.activated(currentIndex)
            return true
        default:
            return false
        }
    }
}
