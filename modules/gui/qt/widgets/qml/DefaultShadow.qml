
/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import VLC.Util

// The drop shadow of Figma's "bg blur + shadow" effect style: black at 46 %
// (the Shadow color), 0 px x, 8 px y, blur 15 px. A Figma (CSS) blur radius
// is twice the Gaussian sigma the shader takes.
DoubleShadow {
    id: root

    primaryColor: colorContext.shadow
    primaryVerticalOffset: VLCStyle.dp(8, VLCStyle.scale)
    primaryBlurRadius: VLCStyle.dp(15, VLCStyle.scale) / 2

    // a single shadow
    secondaryColor: "transparent"
    secondaryVerticalOffset: 0
    secondaryBlurRadius: 0

    z: -1
}
