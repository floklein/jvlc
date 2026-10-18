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
#ifndef GLYPH_ICON_HPP
#define GLYPH_ICON_HPP

#include <QIcon>
#include <QChar>
#include <QColor>

class QWidget;

/**
 * @brief an icon of the VLCIcons font, for native widgets such as menus
 *
 * Two-tone icons stack a base glyph and an accent glyph (VLCIcons
 * "<name>_tt_base" and "<name>_tt_accent"); either one may be null.
 *
 * @param base glyph drawn with the text color of @a widget
 * @param accent glyph drawn with the Accent color
 * @param widget the widget showing the icon, for its palette
 * @param accentBelow draw the accent glyph first, under the base one: a
 *        single-color glyph as the accent shows only where the base glyph
 *        leaves it (Hide UI: the diagonal of "ui" inside the frame of
 *        "ui_tt_base", Figma 6065:13616)
 */
QIcon glyphIcon( QChar base, QChar accent, const QWidget *widget, bool accentBelow = false );

/**
 * @brief the accent shade of the menu icons (Accent/Dim) of the user's
 * Accent color for a light or dark surface
 */
QColor accentColorFor( const QColor &background );

#endif // GLYPH_ICON_HPP
