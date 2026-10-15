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
#ifndef VLC_QT_THEMETOKENS_HPP
#define VLC_QT_THEMETOKENS_HPP

#include <QColor>
#include <QList>

#include "util/color_scheme_model.hpp"

/**
 * Design system colors of one Theme, named after the Figma paint styles
 * ("Dark/BG/A1", "Light/FG/2", ...).
 *
 * Readable text only uses fg1, fg2, fgA1 and fgA2. fgA3 is for icons and
 * decoration, fgA4 for disabled elements and dividers.
 */
struct ThemeTokens
{
    bool dark = true;

    // backgrounds, A1 is the most elevated surface
    QColor bgA1, bgA2, bgA3, bgA4, bgA5;
    QColor bgB1, bgB2;
    QColor bgA3Transparent, bgB1Transparent, bgB2Transparent;
    QColor bgAccentBright, bgAccentDark;

    // foregrounds
    QColor fg1, fg2;
    QColor fgA1, fgA2, fgA3, fgA4;
    QColor fgB1; // text and glyphs drawn on the accent color

    // accent, derived from the user's Accent color
    QColor accentMain, accentBright, accentDim;
    // the cone logo, never affected by the Accent color
    QColor accentLogo;

    QColor dividerBright, dividerDim;

    // accent for text, icons and focus rings: the accent, darkened or
    // lightened until it reads on every surface (WCAG 2.2 AA)
    QColor accentText;
    // accent buttons and the text drawn on them
    QColor accentButton, accentButtonHovered, accentButtonPressed;
    QColor onAccent;

    // translucent layer of panels over the video
    QColor tint;
    // the lighter Tint over the Video blur, opaque enough for the text to
    // read over a bright video
    QColor tintOverBlur;
    // the dark scrim drawn behind the bars right over the video
    QColor scrim;

    // badges drawn over artwork
    QColor badgeBg, badgeFg;

    // status colors, not part of the Figma styles
    QColor positive, neutral, negative;
    QColor positiveBg, neutralBg, negativeBg;
    // strong red background, e.g. the hovered close window button
    QColor critical;

    // drop shadow of the "bg blur + shadow" effect
    QColor shadow;
};

/**
 * Accent shades of one Theme
 */
struct AccentShades
{
    QColor main;
    QColor bright;
    QColor dim;
    QColor bgBright;
    QColor bgDark;
};

/**
 * The Accent color VLC uses unless the user picks another one (Dark/Accent/Main)
 */
QColor defaultAccentColor();

/**
 * Derive the accent shades of a Theme from the user's Accent color.
 *
 * Each shade keeps the hue, saturation and lightness offsets that the Figma
 * shades have from the default Accent color, so the default Accent color
 * gives back the Figma styles.
 */
AccentShades deriveAccentShades(const QColor& accent, bool dark);

/**
 * Build the tokens of the dark or light Theme with the given Accent color.
 * An invalid accent means defaultAccentColor().
 */
ThemeTokens makeThemeTokens(bool dark, const QColor& accent = {});

/**
 * WCAG 2.2 contrast ratio of two opaque colors, from 1 to 21
 */
double contrastRatio(const QColor& a, const QColor& b);

/**
 * The opaque color seen when color is drawn over an opaque backdrop
 */
QColor composite(const QColor& color, const QColor& backdrop);

/**
 * color, made just lighter or darker enough to reach ratio against every
 * background. Black or white when it cannot.
 */
QColor withContrast(const QColor& color, const QList<QColor>& backgrounds, double ratio);

/**
 * Whether a Theme choice shows the dark palette. System follows the
 * operating system's light/dark setting.
 */
bool resolveDarkTheme(ColorSchemeModel::ColorScheme theme, bool systemIsDark);

#endif // VLC_QT_THEMETOKENS_HPP
