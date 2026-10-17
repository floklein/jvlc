/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Leon Vitanos <leon.vitanos@gmail.com>
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

import VLC.Style

QtObject{
    // NOTE: Base picture sizing
    required property int basePictureWidth
    required property int basePictureHeight

    required property int availableWidth

    property int titleTopMargin: VLCStyle.gridItemTitle_topMargin
    property int titleHeight: VLCStyle.gridItemTitle_height

    property int subtitleTopMargin: VLCStyle.gridItemSubtitle_topMargin
    property int subtitleHeight: VLCStyle.gridItemSubtitle_height

    // NOTE: The total text height of title and subtitle (the text boxes of
    //       the cards, Figma 6272:13852)
    readonly property int textHeight: (titleHeight ? titleHeight + titleTopMargin : 0) +
                                      (subtitleHeight ? subtitleHeight + subtitleTopMargin : 0)

    // Figma 6272:13818: the cards keep their size, 20 px apart, from the left
    property int horizontalSpacing: VLCStyle.gridView_horizontalSpacing

    property var maxNbItemPerRow
    readonly property int nbItemPerRow: {
        const _nbItemPerRow = Math.max(
            Math.floor(
                (availableWidth + horizontalSpacing) /
                (basePictureWidth + horizontalSpacing)
            ), 1
        )

        return maxNbItemPerRow ? Math.min(_nbItemPerRow, maxNbItemPerRow) : _nbItemPerRow
    }

    // NOTE: The cards are not stretched to fill the row; they only shrink
    //       when a single one does not fit
    readonly property int cellWidth: Math.max(Math.min(basePictureWidth, availableWidth), 1)
    readonly property int cellHeight: Math.round((basePictureHeight / basePictureWidth) * cellWidth) + textHeight

    // NOTE: The pictures are never drawn larger than their base size
    readonly property int maxPictureWidth: basePictureWidth
    readonly property int maxPictureHeight: basePictureHeight
}
