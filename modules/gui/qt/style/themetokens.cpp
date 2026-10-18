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
// alpha of the Tint over the Video blur, at least
constexpr float TINT_OVER_BLUR_ALPHA = 0.75f;
// The blurred video, clamped so that the Tint over the blur stays about 75%:
// at most #525252 under the dark Theme, at least #3D3D3D under the light one
constexpr double DARK_VIDEO_BLUR_BRIGHTEST = 0.32;
constexpr double LIGHT_VIDEO_BLUR_DARKEST = 0.24;
// alpha of the scrim behind the bars right over the video, at least
constexpr float SCRIM_ALPHA = 0.8f;

// WCAG 2.2 AA
constexpr double TEXT_CONTRAST = 4.5;
constexpr double ICON_CONTRAST = 3.0;

double relativeLuminance(const QColor& color)
{
    const auto channel = [](double v) {
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    const QColor c = color.toRgb();
    return 0.2126 * channel(c.redF())
         + 0.7152 * channel(c.greenF())
         + 0.0722 * channel(c.blueF());
}

/*
 * The lowest alpha of layer, from atLeast, at which every text color reaches
 * 4.5:1 and every icon color 3:1 over a video from darkest to brightest (a
 * black and a white video by default).
 */
float readableAlpha(QColor layer, float atLeast,
                    const QList<QColor>& text, const QList<QColor>& icons,
                    double darkest = 0., double brightest = 1.)
{
    const QColor darkestVideo = QColor::fromRgbF(darkest, darkest, darkest);
    const QColor brightestVideo = QColor::fromRgbF(brightest, brightest, brightest);

    const auto readable = [&](const QColor& candidate) {
        for (const QColor& backdrop : { darkestVideo, brightestVideo })
        {
            const QColor bg = composite(candidate, backdrop);
            for (const QColor& c : text)
                if (contrastRatio(c, bg) < TEXT_CONTRAST)
                    return false;
            for (const QColor& c : icons)
                if (contrastRatio(c, bg) < ICON_CONTRAST)
                    return false;
        }
        return true;
    };

    for (int percent = std::lround(atLeast * 100); percent < 100; ++percent)
    {
        layer.setAlphaF(percent / 100.f);
        if (readable(layer))
            return percent / 100.f;
    }
    return 1.f;
}

}

double contrastRatio(const QColor& a, const QColor& b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor composite(const QColor& color, const QColor& backdrop)
{
    const QColor c = color.toRgb();
    const QColor b = backdrop.toRgb();
    const float alpha = c.alphaF();
    return QColor::fromRgbF(c.redF() * alpha + b.redF() * (1.f - alpha),
                            c.greenF() * alpha + b.greenF() * (1.f - alpha),
                            c.blueF() * alpha + b.blueF() * (1.f - alpha));
}

QColor withContrast(const QColor& color, const QList<QColor>& backgrounds, double ratio)
{
    const auto reaches = [&](const QColor& c) {
        for (const QColor& bg : backgrounds)
            if (contrastRatio(c, bg) < ratio)
                return false;
        return true;
    };

    if (reaches(color))
        return color;

    // the smallest change of lightness that reaches the ratio
    float h, s, l;
    color.toHsl().getHslF(&h, &s, &l);
    h = std::max(h, 0.f); // achromatic
    for (int step = 1; step <= 200; ++step)
    {
        for (float direction : { -1.f, 1.f })
        {
            const float lightness = l + direction * step / 200.f;
            if (lightness < 0.f || lightness > 1.f)
                continue;
            const QColor c = QColor::fromHslF(h, s, lightness).toRgb();
            if (reaches(c))
                return c;
        }
    }

    const QColor black(Qt::black);
    return reaches(black) ? black : QColor(Qt::white);
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

    const QColor black(Qt::black);
    const QColor white(Qt::white);

    // the Figma accent shades are too light for text on the light surfaces,
    // and a user Accent color can be too close to any surface
    t.accentText = withContrast(t.accentMain, {
        t.bgA1, t.bgA2, t.bgA3, t.bgA4, t.bgA5, t.bgB1, t.bgB2,
        composite(t.tint, black), composite(t.tint, white),
    }, TEXT_CONTRAST);

    const QColor lightText = t.dark ? t.fg1 : t.bgB2;
    t.onAccent = contrastRatio(t.fgB1, t.accentMain) >= contrastRatio(lightText, t.accentMain)
        ? t.fgB1 : lightText;
    t.accentButton = withContrast(t.accentMain, { t.onAccent }, TEXT_CONTRAST);
    t.accentButtonHovered = withContrast(t.accentBright, { t.onAccent }, TEXT_CONTRAST);
    t.accentButtonPressed = withContrast(t.accentDim, { t.onAccent }, TEXT_CONTRAST);

    const QList<QColor> text { t.fg1, t.fg2, t.fgA1, t.fgA2 };

    // the compositor clamps the blurred video before the Tint
    t.videoBlurDarkest = dark ? 0. : LIGHT_VIDEO_BLUR_DARKEST;
    t.videoBlurBrightest = dark ? DARK_VIDEO_BLUR_BRIGHTEST : 1.;
    t.tintOverBlur = t.tint;
    t.tintOverBlur.setAlphaF(readableAlpha(t.tint, TINT_OVER_BLUR_ALPHA,
                                           text, { t.fgA3, t.accentText },
                                           t.videoBlurDarkest, t.videoBlurBrightest));

    t.scrim = t.bgB2;
    t.scrim.setAlphaF(readableAlpha(t.bgB2, SCRIM_ALPHA,
                                    text + QList<QColor>{ t.accentText }, { t.fgA3 }));

    t.badgeFg = t.dark ? t.fg1 : t.bgB2;
    t.badgeBg = t.bgB2Transparent;
    t.badgeBg.setAlphaF(readableAlpha(t.bgB2Transparent, t.bgB2Transparent.alphaF(),
                                      { t.badgeFg }, {}));

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

MenuColors menuColors(const ThemeTokens& t)
{
    MenuColors c;
    // Figma's "BG/A3 Transparent", made opaque over the window: a native
    // menu has no translucent background on every platform
    c.background = composite(t.bgA3Transparent, t.bgB1);
    c.text = t.fgA2;
    c.shortcut = t.fgA3;
    c.disabledText = t.fgA4;
    // the hovered row and the checked one: BG/A2, the text stays FG/A2
    c.hoveredBackground = t.bgA2;
    c.hoveredText = t.fgA2;
    c.separator = t.dividerDim;
    c.check = t.accentMain;
    c.iconBase = t.fgA2;
    c.iconAccent = t.accentDim;
    return c;
}

QString menuStyleSheet(const ThemeTokens& t, double scale)
{
    const MenuColors c = menuColors(t);
    const auto px = [scale](double value) {
        return QStringLiteral("%1px").arg(std::max(1L, std::lround(value * scale)));
    };
    const auto name = [](const QColor& color) {
        return color.name(QColor::HexRgb);
    };

    // Only colors and the font: with no box, border, background or size,
    // the style sheet code leaves the drawing of the menus to MenuStyle,
    // which reads these colors from the menu's palette
    // (alternate-background-color is the menu's background) and its scale
    // from the font size
    return QStringLiteral(
        "QMenu {"
        " color: %1; selection-color: %2; selection-background-color: %3;"
        " alternate-background-color: %4; placeholder-text-color: %5; accent-color: %6;"
        " font-family: \"Inter\"; font-size: %7; }"
        "QMenu::item:disabled { color: %8; }"
        "QMenu::separator { background-color: %9; }")
        .arg(name(c.text), name(c.hoveredText), name(c.hoveredBackground),
             name(c.background), name(c.shortcut), name(c.check),
             px(14), name(c.disabledText), name(c.separator));
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
