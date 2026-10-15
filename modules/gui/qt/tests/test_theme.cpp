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
        QTest::addColumn<QString>("tint");
        QTest::newRow("dark") << true << withAlpha("#191919", 0.75).name(QColor::HexArgb);
        QTest::newRow("light") << false << withAlpha("#ebebeb", 0.75).name(QColor::HexArgb);
    }

    void testTintOverTheVideoBlurIsLighter()
    {
        QFETCH(bool, dark);
        QFETCH(QString, tint);
        QCOMPARE_COLOR(makeThemeTokens(dark).tintOverBlur, tint);
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
