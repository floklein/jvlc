/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

pragma Singleton

import QtQuick

QtObject {

    function clamp(num : real, min : real, max : real) : real {
      return num <= min ? min : num >= max ? max : num;
    }

    function isValidInstanceOf(object: var, type: var) : bool {
        return (!!object && (object instanceof type))
    }

    function transferFocus(item: Item, reason: int) {
        if (item.activeFocus && item.focusReason === reason)
            return

        if (item.setCurrentItemFocus)
            item.setCurrentItemFocus(reason)
        else
            item.forceActiveFocus(reason)
    }

    // NOTE: This allows us to force another 'reason' even when the item has activeFocus.
    function enforceFocus(item: Item, reason: int) {
        if (item.activeFocus && item.focusReason === reason)
            return

        item.focus = false;

        item.forceActiveFocus(reason);

        if (item.focusReason !== undefined)
            item.focusReason = reason // Control's focus reason sometimes is not adjusted with `forceActiveFocus()`, Qt bug?
    }

    // name of the folder holding a local file, "" for anything else
    // (streams, discs, files at the root of a drive)
    function folderName(url: var) : string {
        const str = (url ?? "").toString()
        if (!str.startsWith("file://"))
            return ""

        let path
        try {
            path = decodeURIComponent(str.substring("file://".length).replace(/[?#].*$/, ""))
        } catch (e) {
            return ""
        }
        const segments = path.split("/").filter((s) => s.length > 0)
        if (segments.length < 2)
            return ""

        const folder = segments[segments.length - 2]
        // a Windows drive ("C:") is not a folder name
        if (/^[A-Za-z]:$/.test(folder))
            return ""

        return folder
    }

    // the corner of area nearest to rect (Qt.TopLeftCorner, Qt.TopRightCorner,
    // Qt.BottomLeftCorner or Qt.BottomRightCorner), by the center of rect;
    // the bottom and the right win a tie
    function nearestCorner(rect: rect, area: rect) : int {
        const left = (rect.x + rect.width / 2) < (area.x + area.width / 2)
        const top = (rect.y + rect.height / 2) < (area.y + area.height / 2)
        if (top)
            return left ? Qt.TopLeftCorner : Qt.TopRightCorner
        return left ? Qt.BottomLeftCorner : Qt.BottomRightCorner
    }

    // the position of an item of the given size in a corner of area, margin
    // away from its edges
    function cornerPosition(corner: int, size: size, area: rect, margin: real) : point {
        const left = (corner === Qt.TopLeftCorner || corner === Qt.BottomLeftCorner)
        const top = (corner === Qt.TopLeftCorner || corner === Qt.TopRightCorner)
        return Qt.point(left ? area.x + margin : area.x + area.width - size.width - margin,
                        top ? area.y + margin : area.y + area.height - size.height - margin)
    }

    // accessible name of an icon-only control: its tooltip plus its shortcut
    function accessibleName(name: string, shortcut: string) : string {
        if (!shortcut)
            return name
        return qsTr("%1, %2").arg(name).arg(shortcut)
    }

    // Where an item of a given width goes, centered on `center`, between a
    // left and a right limit: it slides aside rather than cross a limit, then
    // shrinks, down to nothing. When the limits cross, it sits at the right
    // one, so that what is on the left can only use the room up to there.
    // Returns {x, width}.
    function centeredSpan(center: real, width: real, left: real, right: real) : var {
        const spanWidth = Math.max(0, Math.min(width, right - left))
        const x = Math.min(Math.max(center - spanWidth / 2, left), right - spanWidth)
        return { x: x, width: spanWidth }
    }

    function pointInRadius(x: real, y: real, radius: real) : bool {
         return (x * x + y * y < radius * radius)
    }

    // checks if point `pos` lies in rect `rect`
    function contains(rect : rect, pos : point) : bool {
        return (clamp(pos.x, rect.x, rect.x + rect.width) === pos.x)
                && (clamp(pos.y, rect.y, rect.y + rect.height) === pos.y)
    }

    function isInteger(data : var) : bool {
        return (typeof data === 'number' && (data % 1) === 0)
    }

    function compareFloat(a: real, b: real) : bool {
        return (Math.abs(a - b) < Number.EPSILON)
    }

    function alignUp(a : real, b : int) : int {
        return Math.ceil(a / b) * b
    }

    function alignDown(a : real, b : int) : int {
        return Math.floor(a / b) * b
    }

    // Currently only supports .25, .5, .75
    function denominatorForFloat(number : real) : int {
        const fraction = number % 1

        if (fraction === 0)
            return 1
        else if (compareFloat(fraction, 0.25))
            return 4
        else if (compareFloat(fraction, 0.50))
            return 2
        else if (compareFloat(fraction, 0.75))
            return 4
        else {
            console.error("Can not find denominator for number", number, "!")
            return 1
        }
    }

    function isSortedIntegerArrayConsecutive(array : var) : bool {
        for (let i = 1; i < array.length; ++i) {
            if ((array[i] - array[i - 1]) !== 1)
                return false
        }

        return true
    }

    function itemsMovable(sortedItemIndexes : var, targetIndex : int) : bool {
        return !isSortedIntegerArrayConsecutive(sortedItemIndexes) ||
                (targetIndex > (sortedItemIndexes[sortedItemIndexes.length - 1] + 1) ||
                 targetIndex < sortedItemIndexes[0])
    }

    /**
     * calculate content y for flickable such that item with given param will be fully visible
     * @param type:Flickable flickable
     * @param type:real y
     * @param type:real height
     * @param type:real topMargin
     * @param type:real bottomMargin
     * @return type:real appropriate contentY for flickable
     */
    function flickablePositionContaining(flickable : Flickable,
                                         y : real,
                                         height : real,
                                         topMargin : real,
                                         bottomMargin : real) : real {
        const itemTopY = flickable.originY + y
        const itemBottomY = itemTopY + height

        const viewTopY = flickable.contentY
        const viewBottomY = viewTopY + flickable.height

        let newContentY

        if (itemTopY < viewTopY)
             //item above view
            newContentY = itemTopY - topMargin
        else if (itemBottomY > viewBottomY)
             //item below view
            newContentY = itemBottomY + bottomMargin - flickable.height
        else
            newContentY = flickable.contentY

        return getFlickableBoundedContentY(flickable, newContentY)
    }

    function isArray(obj : var) : bool {
        return (obj?.length !== undefined) ?? false
    }

    // Similar to Item::contains(), but accepts area (rectangle):
    function itemIntersects(item: Item, area: rect) : bool {
        if (!item)
            return false
        if (area.width <= 0.0 || area.height <= 0.0)
            return false
        return !(((item.x < (area.x + area.width)) && ((item.x + item.width) > area.x)) &&
                 ((item.y < (area.y + area.height)) && ((item.y + item.height) > area.y)))
    }

    // Similar to `ItemView::positionViewAtIndex()` with `ItemView.Contain`,
    // but only the works in the y-axis for now.
    // The behavior is undefined if the item is not a visual child of the
    // content item. However, it is not required for the item to be a direct
    // child of the content item.
    function positionFlickableToContainItem(flickable: Flickable, item: Item) {
        console.assert(flickable)
        console.assert(item)
        console.assert(item.parent)
        const mappedRect = flickable.mapFromItem(item.parent, Qt.rect(item.x, item.y, item.width, item.height))
        // Flickable does not fully contain the item:
        if ((mappedRect.y < 0) || ((mappedRect.y + mappedRect.height) > flickable.height))
            flickable.contentY = getFlickableBoundedContentY(flickable,
                                                             flickable.contentItem.mapFromItem(item.parent, item.x, item.y).y -
                                                             Math.max(0, ((flickable.height - item.height) / 2)))
    }

    function getFlickableBoundedContentX(flickable: Flickable, contentX: real) : real {
        console.assert(flickable)
        return clamp(contentX,
                     estimateFlickableMinXExtent(flickable),
                     estimateFlickableMaxXExtent(flickable))
    }

    function getFlickableBoundedContentY(flickable: Flickable, contentY: real) : real {
        console.assert(flickable)
        return clamp(contentY,
                     estimateFlickableMinYExtent(flickable),
                     estimateFlickableMaxYExtent(flickable))
    }

    /// <flickable-extents>

    // FIXME: Get rid of this in favor of `QQuickFlickable::minXExtent()`:
    function estimateFlickableMinXExtent(flickable: Flickable) : real {
        console.assert(flickable)
        return (flickable.originX - flickable.leftMargin)
    }

    // FIXME: Get rid of this in favor of `QQuickFlickable::minYExtent()`:
    function estimateFlickableMinYExtent(flickable: Flickable) : real {
        console.assert(flickable)
        return (flickable.originY - flickable.topMargin)
    }

    // FIXME: Get rid of this in favor of `QQuickFlickable::maxXExtent()`:
    function estimateFlickableMaxXExtent(flickable: Flickable) : real {
        console.assert(flickable)
        return (flickable.originX + flickable.contentWidth - flickable.width + flickable.rightMargin)
    }

    // FIXME: Get rid of this in favor of `QQuickFlickable::maxYExtent()`:
    function estimateFlickableMaxYExtent(flickable: Flickable) : real {
        console.assert(flickable)
        return (flickable.originY + flickable.contentHeight - flickable.height + flickable.bottomMargin)
    }

    /// </flickable-extents>

    // Reads a time typed as "s", "m:ss" or "h:mm:ss". The first field may
    // exceed 59 ("90:00"). Returns the time in seconds, or -1 if invalid.
    function parseTime(text: string) : int {
        const fields = text.trim().split(":")
        if (fields.length > 3)
            return -1

        let seconds = 0
        for (let i = 0; i < fields.length; ++i) {
            if (!/^[0-9]+$/.test(fields[i]))
                return -1
            const value = parseInt(fields[i], 10)
            if (i > 0 && value > 59)
                return -1
            seconds = seconds * 60 + value
        }
        return seconds
    }

    // Reads a playback speed typed as "1.5", "1.5x" or "1,5×", clamped to
    // 0.25x..4x. Returns -1 if it is not a number.
    function parseSpeed(text: string) : real {
        const value = text.trim().replace(/[x×]$/i, "").trim().replace(",", ".")
        if (!/^[0-9]*\.?[0-9]+$/.test(value))
            return -1
        return Math.min(Math.max(parseFloat(value), 0.25), 4)
    }

    // A playback speed as "1x", "0.25x", "1.04x"
    function formatSpeed(speed: real) : string {
        return (Math.round(speed * 100) / 100) + "x"
    }
}
