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

#include <QTest>
#include <QRegularExpression>

#include <cmath>

#include "../style/themetokens.hpp"

// Figma paint styles of the redesign (file QRb0F0V7fSTZCIoOyeV0Km)
#define QCOMPARE_COLOR(actual, expected) \
    QCOMPARE((actual).name(QColor::HexArgb), QColor(expected).name(QColor::HexArgb))

static QColor withAlpha(const char* rgb, float alpha)
{
    QColor c(rgb);
    c.setAlphaF(alpha);
    return c;
}

class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void testDarkPaletteFollowsFigma()
    {
        const ThemeTokens t = makeThemeTokens(true);
        QVERIFY(t.dark);

        QCOMPARE_COLOR(t.bgA1, "#222222");
        QCOMPARE_COLOR(t.bgA2, "#1f1f1f");
        QCOMPARE_COLOR(t.bgA3, "#1b1b1b");
        QCOMPARE_COLOR(t.bgA4, "#191919");
        QCOMPARE_COLOR(t.bgA5, "#151515");
        QCOMPARE_COLOR(t.bgB1, "#0e0e0e");
        QCOMPARE_COLOR(t.bgB2, "#090909");
        QCOMPARE_COLOR(t.bgA3Transparent, withAlpha("#191919", 0.95));
        QCOMPARE_COLOR(t.bgB1Transparent, withAlpha("#0e0e0e", 0.95));
        QCOMPARE_COLOR(t.bgB2Transparent, withAlpha("#000000", 0.80));
        QCOMPARE_COLOR(t.bgAccentBright, "#371f06");
        QCOMPARE_COLOR(t.bgAccentDark, "#1a1611");

        QCOMPARE_COLOR(t.fg1, "#eaeaea");
        QCOMPARE_COLOR(t.fg2, "#c6c6c6");
        QCOMPARE_COLOR(t.fgA1, "#969696");
        QCOMPARE_COLOR(t.fgA2, "#8f8f8f");
        QCOMPARE_COLOR(t.fgA3, "#707070");
        QCOMPARE_COLOR(t.fgA4, "#404040");
        QCOMPARE_COLOR(t.fgB1, "#161616");

        QCOMPARE_COLOR(t.accentMain, "#d37918");
        QCOMPARE_COLOR(t.accentBright, "#fc9729");
        QCOMPARE_COLOR(t.accentDim, "#ab6314");
        QCOMPARE_COLOR(t.accentLogo, "#d37918");

        QCOMPARE_COLOR(t.dividerBright, "#373737");
        QCOMPARE_COLOR(t.dividerDim, "#222222");
    }

    void testLightPaletteFollowsFigma()
    {
        const ThemeTokens t = makeThemeTokens(false);
        QVERIFY(!t.dark);

        QCOMPARE_COLOR(t.bgA1, "#f7f7f7");
        QCOMPARE_COLOR(t.bgA2, "#f1f1f1");
        QCOMPARE_COLOR(t.bgA3, "#ebebeb");
        QCOMPARE_COLOR(t.bgA4, "#e5e5e5");
        QCOMPARE_COLOR(t.bgA5, "#dfdfdf");
        // main surfaces are near-white
        QCOMPARE_COLOR(t.bgB1, "#fafafa");
        QCOMPARE_COLOR(t.bgB2, "#ffffff");
        QCOMPARE_COLOR(t.bgA3Transparent, withAlpha("#ebebeb", 0.90));
        QCOMPARE_COLOR(t.bgB1Transparent, withAlpha("#fafafa", 0.90));
        QCOMPARE_COLOR(t.bgB2Transparent, withAlpha("#000000", 0.40));
        QCOMPARE_COLOR(t.bgAccentBright, "#d8ba9a");
        QCOMPARE_COLOR(t.bgAccentDark, "#c6ab89");

        QCOMPARE_COLOR(t.fg1, "#121212");
        QCOMPARE_COLOR(t.fg2, "#262626");
        QCOMPARE_COLOR(t.fgA1, "#3c3c3c");
        QCOMPARE_COLOR(t.fgA2, "#2e2e2e");
        QCOMPARE_COLOR(t.fgA3, "#5f5f5f");
        QCOMPARE_COLOR(t.fgA4, "#b5b5b5");
        QCOMPARE_COLOR(t.fgB1, "#161616");

        QCOMPARE_COLOR(t.accentMain, "#e8851a");
        QCOMPARE_COLOR(t.accentBright, "#ffa13b");
        QCOMPARE_COLOR(t.accentDim, "#965916");
        QCOMPARE_COLOR(t.accentLogo, "#c2711a");

        QCOMPARE_COLOR(t.dividerBright, "#bdbdbd");
        QCOMPARE_COLOR(t.dividerDim, "#d0d0d0");
    }

    void testEveryTokenIsSet_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("dark") << true;
        QTest::newRow("light") << false;
    }

    void testEveryTokenIsSet()
    {
        QFETCH(bool, dark);
        const ThemeTokens t = makeThemeTokens(dark);
        const QColor tokens[] = {
            t.bgA1, t.bgA2, t.bgA3, t.bgA4, t.bgA5, t.bgB1, t.bgB2,
            t.bgA3Transparent, t.bgB1Transparent, t.bgB2Transparent,
            t.bgAccentBright, t.bgAccentDark,
            t.fg1, t.fg2, t.fgA1, t.fgA2, t.fgA3, t.fgA4, t.fgB1,
            t.accentMain, t.accentBright, t.accentDim, t.accentLogo,
            t.dividerBright, t.dividerDim, t.tint, t.tintOverBlur,
            t.positive, t.neutral, t.negative,
            t.positiveBg, t.neutralBg, t.negativeBg,
            t.critical, t.shadow,
            t.accentText, t.accentButton, t.accentButtonHovered,
            t.accentButtonPressed, t.onAccent, t.badgeBg, t.badgeFg, t.scrim,
        };
        for (const QColor& c : tokens)
            QVERIFY(c.isValid());
    }

    void testTintIsTheThemeBackgroundAt95Percent_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<QString>("tint");
        QTest::newRow("dark") << true << withAlpha("#191919", 0.95).name(QColor::HexArgb);
        QTest::newRow("light") << false << withAlpha("#ebebeb", 0.95).name(QColor::HexArgb);
    }

    void testTintIsTheThemeBackgroundAt95Percent()
    {
        QFETCH(bool, dark);
        QFETCH(QString, tint);
        QCOMPARE_COLOR(makeThemeTokens(dark).tint, tint);
    }

    void testTintOverTheVideoBlurIsLighter_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("dark") << true;
        QTest::newRow("light") << false;
    }

    void testTintOverTheVideoBlurIsLighter()
    {
        QFETCH(bool, dark);
        const ThemeTokens t = makeThemeTokens(dark);
        QCOMPARE(t.tintOverBlur.rgb(), t.tint.rgb());
        QVERIFY(t.tintOverBlur.alphaF() < t.tint.alphaF());
        // about 75%: the clamped video keeps the text readable
        QVERIFY2(t.tintOverBlur.alphaF() > 0.745f && t.tintOverBlur.alphaF() < 0.765f,
                 qPrintable(QString::number(t.tintOverBlur.alphaF())));
    }

    // the blurred video is kept from getting too bright under the dark
    // Theme's Tint, and too dark under the light one's
    void testVideoBlurIsClampedPerTheme()
    {
        const ThemeTokens dark = makeThemeTokens(true);
        QCOMPARE(dark.videoBlurDarkest, 0.);
        QVERIFY(dark.videoBlurBrightest > 0.25 && dark.videoBlurBrightest < 0.5);

        const ThemeTokens light = makeThemeTokens(false);
        QVERIFY(light.videoBlurDarkest > 0.1 && light.videoBlurDarkest < 0.5);
        QCOMPARE(light.videoBlurBrightest, 1.);
    }

    // WCAG 2.2 contrast ratios (https://www.w3.org/TR/WCAG22/#dfn-contrast-ratio)
    void testContrastRatioFollowsWcag_data()
    {
        QTest::addColumn<QString>("a");
        QTest::addColumn<QString>("b");
        QTest::addColumn<double>("ratio");
        QTest::newRow("black on white") << "#000000" << "#ffffff" << 21.0;
        QTest::newRow("white on black") << "#ffffff" << "#000000" << 21.0;
        QTest::newRow("same color") << "#777777" << "#777777" << 1.0;
        QTest::newRow("#767676 on white") << "#767676" << "#ffffff" << 4.54;
        QTest::newRow("#777777 on white") << "#777777" << "#ffffff" << 4.48;
        QTest::newRow("#0000ff on white") << "#0000ff" << "#ffffff" << 8.59;
    }

    void testContrastRatioFollowsWcag()
    {
        QFETCH(QString, a);
        QFETCH(QString, b);
        QFETCH(double, ratio);
        QVERIFY2(std::abs(contrastRatio(QColor(a), QColor(b)) - ratio) < 0.01,
                 qPrintable(QString::number(contrastRatio(QColor(a), QColor(b)))));
    }

    void testCompositeBlendsOverTheBackdrop()
    {
        QCOMPARE_COLOR(composite(withAlpha("#000000", 0.6), QColor("#ffffff")), "#666666");
        QCOMPARE_COLOR(composite(QColor("#123456"), QColor("#ffffff")), "#123456");
        QCOMPARE_COLOR(composite(withAlpha("#ffffff", 0.0), QColor("#123456")), "#123456");
    }

    // readable text reaches 4.5:1 and icons 3:1, in both Themes, with any
    // Accent color, on every surface and over any video
    void testReadability_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<QString>("accent");
        const char* accents[] = {
            "", "#0078d4", "#ffd400", "#1a237e", "#808080",
            "#e81123", "#00b294", "#000000", "#ffffff",
        };
        for (bool dark : { true, false })
            for (const char* accent : accents)
                QTest::addRow("%s %s", dark ? "dark" : "light", *accent ? accent : "default")
                    << dark << QString(accent);
    }

    void testReadability()
    {
        QFETCH(bool, dark);
        QFETCH(QString, accent);
        const ThemeTokens t = makeThemeTokens(dark, accent.isEmpty() ? QColor() : QColor(accent));

        const QColor black("#000000");
        const QColor white("#ffffff");

        struct Surface { QByteArray name; QColor color; };
        QList<Surface> surfaces = {
            { "A1", t.bgA1 }, { "A2", t.bgA2 }, { "A3", t.bgA3 }, { "A4", t.bgA4 },
            { "A5", t.bgA5 }, { "B1", t.bgB1 }, { "B2", t.bgB2 },
            { "Tint over a black video", composite(t.tint, black) },
            { "Tint over a white video", composite(t.tint, white) },
        };
        // the Video blur is clamped to [darkest, brightest]: from the darkest
        // to the brightest clamped video, in steps
        for (int step = 0; step <= 10; ++step)
        {
            const double level = t.videoBlurDarkest
                + (t.videoBlurBrightest - t.videoBlurDarkest) * step / 10.;
            surfaces.append({ "blur Tint over a clamped video at " + QByteArray::number(level),
                              composite(t.tintOverBlur, QColor::fromRgbF(level, level, level)) });
        }
        struct Ink { const char* name; QColor color; double ratio; };
        const Ink inks[] = {
            { "FG/1", t.fg1, 4.5 }, { "FG/2", t.fg2, 4.5 },
            { "FG/A1", t.fgA1, 4.5 }, { "FG/A2", t.fgA2, 4.5 },
            { "accent text", t.accentText, 4.5 },
            { "FG/A3 icons", t.fgA3, 3.0 },
        };

        for (const Surface& s : surfaces)
            for (const Ink& ink : inks)
            {
                // accent text over the Video blur is only drawn as icons,
                // checked states and focus rings
                const bool blur = s.name.startsWith("blur");
                const double ratio = (blur && ink.color == t.accentText) ? 3.0 : ink.ratio;
                const double actual = contrastRatio(ink.color, s.color);
                QVERIFY2(actual >= ratio,
                         qPrintable(QString("%1 on %2: %3").arg(ink.name, QString(s.name)).arg(actual)));
            }

        // accent buttons
        const QColor buttons[] = { t.accentButton, t.accentButtonHovered, t.accentButtonPressed };
        for (const QColor& button : buttons)
            QVERIFY2(contrastRatio(t.onAccent, button) >= 4.5,
                     qPrintable(QString("%1 on %2").arg(t.onAccent.name(), button.name())));
    }

    void testDefaultAccentKeepsFigmaButtons_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("dark") << true;
        QTest::newRow("light") << false;
    }

    void testDefaultAccentKeepsFigmaButtons()
    {
        QFETCH(bool, dark);
        const ThemeTokens t = makeThemeTokens(dark);
        QCOMPARE_COLOR(t.accentButton, t.accentMain.name());
        QCOMPARE_COLOR(t.accentButtonHovered, t.accentBright.name());
        QCOMPARE_COLOR(t.onAccent, t.fgB1.name());
        // the dark accent already reads on every dark surface
        if (dark)
            QCOMPARE_COLOR(t.accentText, t.accentMain.name());
    }

    void testBadgesReadOverAnyArtwork_data()
    {
        testTintOverTheVideoBlurIsLighter_data();
    }

    void testBadgesReadOverAnyArtwork()
    {
        QFETCH(bool, dark);
        const ThemeTokens t = makeThemeTokens(dark);
        for (const char* artwork : { "#000000", "#ffffff", "#808080" })
        {
            const double actual = contrastRatio(t.badgeFg, composite(t.badgeBg, QColor(artwork)));
            QVERIFY2(actual >= 4.5, qPrintable(QString("over %1: %2").arg(artwork).arg(actual)));
        }
    }

    // the bars drawn right over the video, on the dark Theme's scrim
    void testScrimKeepsTheBarsReadable()
    {
        const ThemeTokens t = makeThemeTokens(true);
        for (const char* video : { "#000000", "#ffffff", "#808080" })
        {
            const QColor bg = composite(t.scrim, QColor(video));
            for (const QColor& text : { t.fg1, t.fg2, t.fgA1, t.fgA2, t.accentText })
                QVERIFY2(contrastRatio(text, bg) >= 4.5,
                         qPrintable(QString("%1 over %2").arg(text.name(), video)));
            QVERIFY(contrastRatio(t.fgA3, bg) >= 3.0);
        }
        // a scrim, not an opaque bar
        QVERIFY(t.scrim.alphaF() < 1.f);
    }

    void testStatusColorsAreReadable_data()
    {
        testTintOverTheVideoBlurIsLighter_data();
    }

    void testStatusColorsAreReadable()
    {
        QFETCH(bool, dark);
        const ThemeTokens t = makeThemeTokens(dark);
        QVERIFY(contrastRatio(t.positive, t.positiveBg) >= 4.5);
        QVERIFY(contrastRatio(t.neutral, t.neutralBg) >= 4.5);
        QVERIFY(contrastRatio(t.negative, t.negativeBg) >= 4.5);
        for (const QColor& bg : { t.bgB1, t.bgB2 })
        {
            QVERIFY(contrastRatio(t.positive, bg) >= 4.5);
            QVERIFY(contrastRatio(t.neutral, bg) >= 4.5);
            QVERIFY(contrastRatio(t.negative, bg) >= 4.5);
        }
    }

    void testDefaultAccentGivesFigmaShades_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<QString>("main");
        QTest::addColumn<QString>("bright");
        QTest::addColumn<QString>("dim");
        QTest::addColumn<QString>("bgBright");
        QTest::addColumn<QString>("bgDark");
        QTest::newRow("dark") << true
            << QString("#d37918") << QString("#fc9729") << QString("#ab6314")
            << QString("#371f06") << QString("#1a1611");
        QTest::newRow("light") << false
            << QString("#e8851a") << QString("#ffa13b") << QString("#965916")
            << QString("#d8ba9a") << QString("#c6ab89");
    }

    void testDefaultAccentGivesFigmaShades()
    {
        QFETCH(bool, dark);
        QFETCH(QString, main);
        QFETCH(QString, bright);
        QFETCH(QString, dim);
        QFETCH(QString, bgBright);
        QFETCH(QString, bgDark);

        QCOMPARE_COLOR(defaultAccentColor(), "#d37918");

        const AccentShades shades = deriveAccentShades(defaultAccentColor(), dark);
        QCOMPARE_COLOR(shades.main, main);
        QCOMPARE_COLOR(shades.bright, bright);
        QCOMPARE_COLOR(shades.dim, dim);
        QCOMPARE_COLOR(shades.bgBright, bgBright);
        QCOMPARE_COLOR(shades.bgDark, bgDark);
    }

    void testUserAccentIsTheDarkMainShade()
    {
        const QColor blue("#1f6fd1");
        QCOMPARE_COLOR(deriveAccentShades(blue, true).main, blue);
        QCOMPARE_COLOR(makeThemeTokens(true, blue).accentMain, blue);
    }

    void testUserAccentDerivesEveryShade_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("dark") << true;
        QTest::newRow("light") << false;
    }

    void testUserAccentDerivesEveryShade()
    {
        QFETCH(bool, dark);

        // a blue accent gives blue shades, ordered like the Figma ones
        const QColor blue("#1f6fd1");
        const AccentShades s = deriveAccentShades(blue, dark);
        for (const QColor& c : { s.main, s.bright, s.dim, s.bgBright, s.bgDark })
        {
            QVERIFY2(c.isValid(), "invalid shade");
            QVERIFY2(c.blue() > c.red(), qPrintable(c.name()));
            QVERIFY2(qAbs(c.hslHue() - blue.hslHue()) <= 12, qPrintable(c.name()));
        }
        QVERIFY(s.dim.lightness() < s.main.lightness());
        QVERIFY(s.main.lightness() < s.bright.lightness());

        // the tokens use the derived shades
        const ThemeTokens t = makeThemeTokens(dark, blue);
        QCOMPARE_COLOR(t.accentMain, s.main);
        QCOMPARE_COLOR(t.accentBright, s.bright);
        QCOMPARE_COLOR(t.accentDim, s.dim);
        QCOMPARE_COLOR(t.bgAccentBright, s.bgBright);
        QCOMPARE_COLOR(t.bgAccentDark, s.bgDark);
    }

    void testGreyAccentGivesGreyShades()
    {
        const AccentShades s = deriveAccentShades(QColor("#808080"), false);
        for (const QColor& c : { s.main, s.bright, s.dim, s.bgBright, s.bgDark })
            QCOMPARE(c.hslSaturation(), 0);
    }

    void testInvalidAccentMeansDefault()
    {
        const ThemeTokens t = makeThemeTokens(true, QColor());
        QCOMPARE_COLOR(t.accentMain, "#d37918");
    }

    void testConeLogoIgnoresTheAccent_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<QString>("logo");
        QTest::newRow("dark") << true << QString("#d37918");
        QTest::newRow("light") << false << QString("#c2711a");
    }

    void testConeLogoIgnoresTheAccent()
    {
        QFETCH(bool, dark);
        QFETCH(QString, logo);
        QCOMPARE_COLOR(makeThemeTokens(dark, QColor("#1f6fd1")).accentLogo, logo);
    }

    void testMenusFollowTheTheme_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("dark") << true;
        QTest::newRow("light") << false;
    }

    void testMenusFollowTheTheme()
    {
        QFETCH(bool, dark);
        const ThemeTokens t = makeThemeTokens(dark);
        const MenuColors c = menuColors(t);

        // Figma 6062:13354: BG/A3 Transparent, FG/A2 text, FG/A3 shortcuts,
        // Divider/Dim lines, BG/A2 hovered and checked rows, FG/A2 + Accent/Dim
        // icons, Accent/Main check (856:2215)
        QCOMPARE(c.background, composite(t.bgA3Transparent, t.bgB1));
        QCOMPARE(c.text, t.fgA2);
        QCOMPARE(c.shortcut, t.fgA3);
        QCOMPARE(c.separator, t.dividerDim);
        QCOMPARE(c.hoveredBackground, t.bgA2);
        QCOMPARE(c.hoveredText, t.fgA2);
        QCOMPARE(c.disabledText, t.fgA4);
        QCOMPARE(c.check, t.accentMain);
        QCOMPARE(c.iconBase, t.fgA2);
        QCOMPARE(c.iconAccent, t.accentDim);
        if (dark)
            QCOMPARE(c.background.name(), QStringLiteral("#181818"));

        // a dark menu in the dark Theme, a light one in the light Theme
        QCOMPARE(c.background.lightnessF() < 0.5, dark);

        // the entries read on the menu, hovered or not
        QVERIFY(contrastRatio(c.text, c.background) >= 4.5);
        QVERIFY(contrastRatio(c.hoveredText, c.hoveredBackground) >= 4.5);
    }

    void testMenuStyleSheetDrawsFigmaRows()
    {
        const ThemeTokens t = makeThemeTokens(true);
        const MenuColors c = menuColors(t);

        const QString sheet = menuStyleSheet(t);
        QVERIFY(sheet.contains(QStringLiteral("alternate-background-color: ") + c.background.name()));
        QVERIFY(sheet.contains(QStringLiteral("color: ") + c.text.name()));
        QVERIFY(sheet.contains(QStringLiteral("selection-background-color: ") + c.hoveredBackground.name()));
        QVERIFY(sheet.contains(QStringLiteral("placeholder-text-color: ") + c.shortcut.name()));
        QVERIFY(sheet.contains(QStringLiteral("accent-color: ") + c.check.name()));
        QVERIFY(sheet.contains(QStringLiteral("font-size: 14px")));
        // nothing that would make the style sheet code draw or size the menus
        // itself instead of MenuStyle: no box, border, background or size
        QVERIFY(!sheet.contains(QStringLiteral("padding")));
        QVERIFY(!sheet.contains(QStringLiteral("border")));
        QVERIFY(!sheet.contains(QStringLiteral("width")));
        // (the separator's line only)
        QCOMPARE(sheet.count(QRegularExpression(QStringLiteral("[{;] background-color"))), 1);
        QVERIFY(sheet.contains(QStringLiteral("QMenu::separator { background-color: ") + c.separator.name()));
        QVERIFY(!sheet.contains(QStringLiteral("min-height")));
        QVERIFY(!sheet.contains(QLatin1Char('%')));

        // the text follows the interface scale (and MenuStyle with it)
        const QString scaled = menuStyleSheet(t, 1.5);
        QVERIFY(scaled.contains(QStringLiteral("font-size: 21px")));
    }

    void testThemeResolution_data()
    {
        QTest::addColumn<ColorSchemeModel::ColorScheme>("theme");
        QTest::addColumn<bool>("systemIsDark");
        QTest::addColumn<bool>("dark");

        QTest::newRow("System on a dark OS") << ColorSchemeModel::System << true << true;
        QTest::newRow("System on a light OS") << ColorSchemeModel::System << false << false;
        QTest::newRow("Light on a dark OS") << ColorSchemeModel::Light << true << false;
        QTest::newRow("Light on a light OS") << ColorSchemeModel::Light << false << false;
        QTest::newRow("Dark on a dark OS") << ColorSchemeModel::Dark << true << true;
        QTest::newRow("Dark on a light OS") << ColorSchemeModel::Dark << false << true;
    }

    void testThemeResolution()
    {
        QFETCH(ColorSchemeModel::ColorScheme, theme);
        QFETCH(bool, systemIsDark);
        QFETCH(bool, dark);
        QCOMPARE(resolveDarkTheme(theme, systemIsDark), dark);
    }

    void testThemeChoicesKeepTheirSavedIndex()
    {
        // the Theme is saved as a row index: rows must keep their order
        ColorSchemeModel model;
        QCOMPARE(model.rowCount({}), 3);

        const QStringList names { "System", "Light", "Dark" };
        const QList<ColorSchemeModel::ColorScheme> themes {
            ColorSchemeModel::System, ColorSchemeModel::Light, ColorSchemeModel::Dark
        };
        for (int row = 0; row < 3; ++row)
        {
            model.setCurrentIndex(row);
            QCOMPARE(model.data(model.index(row), Qt::DisplayRole).toString(), names[row]);
            QCOMPARE(model.currentScheme(), themes[row]);
        }
    }
};

QTEST_GUILESS_MAIN(TestTheme)
#include "test_theme.moc"
