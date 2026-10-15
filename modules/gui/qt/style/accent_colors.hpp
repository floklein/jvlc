/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef VLC_QT_ACCENT_COLORS_HPP
#define VLC_QT_ACCENT_COLORS_HPP

#include <QVariantList>
#include <QVariantMap>

#include "qt.hpp"

/**
 * The Accent colors offered by Settings and Onboarding:
 * { value: qt-accent-color value, color: swatch color, label }.
 * The default one comes first.
 */
inline QVariantList accentColorChoices()
{
    const struct { const char* value; const char* color; QString label; } swatches[] = {
        { "default", "#d37918", qtr("Orange") },
        { "#e15032", "#e15032", qtr("Red") },
        { "#c49e23", "#c49e23", qtr("Yellow") },
        { "#19a234", "#19a234", qtr("Green") },
        { "#18aad3", "#18aad3", qtr("Cyan") },
        { "#185dd3", "#185dd3", qtr("Blue") },
        { "#8e42ff", "#8e42ff", qtr("Purple") },
        { "#f052b3", "#f052b3", qtr("Pink") },
        { "#afafaf", "#afafaf", qtr("Gray") },
    };

    QVariantList choices;
    for (const auto& swatch : swatches)
    {
        choices.append(QVariantMap{
            { QStringLiteral("value"), QString::fromLatin1(swatch.value) },
            { QStringLiteral("label"), swatch.label },
            { QStringLiteral("color"), QString::fromLatin1(swatch.color) },
        });
    }
    return choices;
}

#endif // VLC_QT_ACCENT_COLORS_HPP
