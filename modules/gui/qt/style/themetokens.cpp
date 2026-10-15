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
#include "themetokens.hpp"

#include <algorithm>
#include <cmath>

namespace {

QColor rgb(QRgb value)
{
    return QColor::fromRgb(value);
}

QColor rgba(QRgb value, float alpha)
{
    QColor c = QColor::fromRgb(value);
    c.setAlphaF(alpha);
    return c;
}

// alpha of the Tint of panels over the video
constexpr float TINT_ALPHA = 0.95f;
// alpha of the Tint over the Video blur
constexpr float TINT_OVER_BLUR_ALPHA = 0.75f;

}

ThemeTokens makeThemeTokens(bool dark, const QColor& accentColor)
{
    ThemeTokens t;
    t.dark = dark;

    if (dark)
    {
        t.bgA1 = rgb(0x222222);
        t.bgA2 = rgb(0x1F1F1F);
        t.bgA3 = rgb(0x1B1B1B);
        t.bgA4 = rgb(0x191919);
        t.bgA5 = rgb(0x151515);
        t.bgB1 = rgb(0x0E0E0E);
        t.bgB2 = rgb(0x090909);
        t.bgA3Transparent = rgba(0x191919, 0.95f);
        t.bgB1Transparent = rgba(0x0E0E0E, 0.95f);
        t.bgB2Transparent = rgba(0x000000, 0.80f);

        t.fg1 = rgb(0xEAEAEA);
        t.fg2 = rgb(0xC6C6C6);
        t.fgA1 = rgb(0x969696);
        t.fgA2 = rgb(0x8F8F8F);
        t.fgA3 = rgb(0x707070);
        t.fgA4 = rgb(0x404040);
        t.fgB1 = rgb(0x161616);

        t.accentLogo = rgb(0xD37918);

        t.dividerBright = rgb(0x373737);
        t.dividerDim = rgb(0x222222);

        t.positive = rgb(0x6CCB5F);
        t.neutral = rgb(0xFCE100);
        t.negative = rgb(0xFF99A4);
        t.positiveBg = rgb(0x393D1B);
        t.neutralBg = rgb(0x433519);
        t.negativeBg = rgb(0x442726);
    }
    else
    {
        t.bgA1 = rgb(0xF7F7F7);
        t.bgA2 = rgb(0xF1F1F1);
        t.bgA3 = rgb(0xEBEBEB);
        t.bgA4 = rgb(0xE5E5E5);
        t.bgA5 = rgb(0xDFDFDF);
        t.bgB1 = rgb(0xFAFAFA);
        t.bgB2 = rgb(0xFFFFFF);
        t.bgA3Transparent = rgba(0xEBEBEB, 0.90f);
        t.bgB1Transparent = rgba(0xFAFAFA, 0.90f);
        t.bgB2Transparent = rgba(0x000000, 0.40f);

        t.fg1 = rgb(0x121212);
        t.fg2 = rgb(0x262626);
        t.fgA1 = rgb(0x3C3C3C);
        t.fgA2 = rgb(0x2E2E2E);
        t.fgA3 = rgb(0x5F5F5F);
        t.fgA4 = rgb(0xB5B5B5);
        t.fgB1 = rgb(0x161616);

        t.accentLogo = rgb(0xC2711A);

        t.dividerBright = rgb(0xBDBDBD);
        t.dividerDim = rgb(0xD0D0D0);

        t.positive = rgb(0x0F7B0F);
        t.neutral = rgb(0x9D5D00);
        t.negative = rgb(0xC42B1C);
        t.positiveBg = rgb(0xDFF6DD);
        t.neutralBg = rgb(0xFFF4CE);
        t.negativeBg = rgb(0xFDE7E9);
    }

    t.critical = rgb(0xC42B1C);
    t.shadow = rgba(0x000000, 0.46f);

    const AccentShades accent = deriveAccentShades(accentColor, dark);
    t.accentMain = accent.main;
    t.accentBright = accent.bright;
    t.accentDim = accent.dim;
    t.bgAccentBright = accent.bgBright;
    t.bgAccentDark = accent.bgDark;

    t.tint = t.bgA3Transparent;
    t.tint.setAlphaF(TINT_ALPHA);
    t.tintOverBlur = t.bgA3Transparent;
    t.tintOverBlur.setAlphaF(TINT_OVER_BLUR_ALPHA);

    return t;
}

QColor defaultAccentColor()
{
    return rgb(0xD37918);
}

namespace {

/*
 * Apply to accent the offsets that figmaShade has from the default Accent
 * color: same hue shift, same saturation ratio, same lightness shift.
 */
QColor shiftLike(const QColor& accent, const QColor& figmaShade)
{
    if (accent.rgb() == defaultAccentColor().rgb())
        return figmaShade;

    float refH, refS, refL;
    defaultAccentColor().getHslF(&refH, &refS, &refL);

    float shadeH, shadeS, shadeL;
    figmaShade.getHslF(&shadeH, &shadeS, &shadeL);

    float h, s, l, a;
    accent.toHsl().getHslF(&h, &s, &l, &a);
    if (h < 0) // achromatic
        h = 0;

    h = std::fmod(h + (shadeH - refH) + 1.f, 1.f);
    s = std::clamp(s * (shadeS / refS), 0.f, 1.f);
    l = std::clamp(l + (shadeL - refL), 0.f, 1.f);

    return QColor::fromHslF(h, s, l).toRgb();
}

}

AccentShades deriveAccentShades(const QColor& accent, bool dark)
{
    // Figma's accent styles, made from the default Accent color
    const AccentShades figma = dark
        ? AccentShades { rgb(0xD37918), rgb(0xFC9729), rgb(0xAB6314), rgb(0x371F06), rgb(0x1A1611) }
        : AccentShades { rgb(0xE8851A), rgb(0xFFA13B), rgb(0x965916), rgb(0xD8BA9A), rgb(0xC6AB89) };

    const QColor base = accent.isValid() ? accent : defaultAccentColor();

    return {
        shiftLike(base, figma.main),
        shiftLike(base, figma.bright),
        shiftLike(base, figma.dim),
        shiftLike(base, figma.bgBright),
        shiftLike(base, figma.bgDark),
    };
}

bool resolveDarkTheme(ColorSchemeModel::ColorScheme theme, bool systemIsDark)
{
    switch (theme)
    {
    case ColorSchemeModel::Light:
        return false;
    case ColorSchemeModel::Dark:
        return true;
    case ColorSchemeModel::System:
    default:
        return systemIsDark;
    }
}
