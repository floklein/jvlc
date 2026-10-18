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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../style/menustyle.hpp"
#include "../style/themetokens.hpp"

#include <QApplication>
#include <QImage>
#include <QMenu>
#include <QPixmap>
#include <QStyleFactory>
#include <QTest>
#include <QtPlugin>
#include <qconfig.h>

#include <memory>

#ifdef QT_STATIC
// static Qt (Windows contribs): the menus are drawn offscreen
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
#endif

namespace {

bool near(const QColor &a, const QColor &b, int tolerance = 6)
{
    return std::abs(a.red() - b.red()) <= tolerance
        && std::abs(a.green() - b.green()) <= tolerance
        && std::abs(a.blue() - b.blue()) <= tolerance;
}

// the right end of the ink of a row: the last column, from the right, with
// a pixel lighter than the background
int inkRight(const QImage &image, const QRect &row, const QColor &background)
{
    for (int x = row.right(); x >= row.left(); --x)
        for (int y = row.top(); y <= row.bottom(); ++y)
            if (!near(image.pixelColor(x, y), background, 12))
                return x;
    return -1;
}

}

/*
 * The native menus as Figma's (6062:13354, 856:2215, 6648:20666): the rows,
 * the widths, the shortcut column, the check mark and the colors of the
 * Theme, drawn by MenuStyle from the Theme's style sheet.
 */
class TestMenuStyle : public QObject
{
    Q_OBJECT

private:
    ThemeTokens m_tokens = makeThemeTokens(true);

    std::unique_ptr<QMenu> menu(const QString &name = {})
    {
        auto m = std::make_unique<QMenu>();
        if (!name.isEmpty())
            m->setObjectName(name);
        return m;
    }

    QImage render(QMenu *m)
    {
        m->ensurePolished();
        m->resize(m->sizeHint());
        return m->grab().toImage().convertToFormat(QImage::Format_RGB32);
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle(new MenuStyle(QStyleFactory::create(QStringLiteral("Fusion"))));
        qApp->setStyleSheet(menuStyleSheet(m_tokens));
    }

    void testKeyText_data()
    {
        QTest::addColumn<QString>("shortcut");
        QTest::addColumn<QString>("text");
        QTest::newRow("one modifier") << "Ctrl+O" << "Ctrl + O";
        QTest::newRow("two modifiers") << "Ctrl+Shift+O" << "Ctrl + Shift + O";
        QTest::newRow("plus key") << "Ctrl++" << "Ctrl + +";
        QTest::newRow("single key") << "Space" << "Space";
        QTest::newRow("function key") << "Shift+F1" << "Shift + F1";
    }

    void testKeyText()
    {
        QFETCH(QString, shortcut);
        QFETCH(QString, text);
        QCOMPARE(MenuStyle::keyText(shortcut), text);
    }

    void testLabelText()
    {
        QCOMPARE(MenuStyle::labelText(QStringLiteral("Open &File(s)...")), QStringLiteral("Open &File(s)"));
        QCOMPARE(MenuStyle::labelText(QStringLiteral("Convert…")), QStringLiteral("Convert"));
        QCOMPARE(MenuStyle::labelText(QStringLiteral("Play")), QStringLiteral("Play"));
    }

    void testCheckPlace()
    {
        using P = MenuStyle::CheckPlace;
        QCOMPARE(MenuStyle::checkPlace(false, false, true), P::None);
        // Profile ▸: in the icon column of a menu with icons
        QCOMPARE(MenuStyle::checkPlace(true, false, true), P::Leading);
        // sorting: at the end of a menu without icons
        QCOMPARE(MenuStyle::checkPlace(true, false, false), P::Trailing);
        // View Options: at the end of an entry with its own icon
        QCOMPARE(MenuStyle::checkPlace(true, true, true), P::Trailing);
    }

    void testMetricsFollowTheScale()
    {
        const MenuStyle::Metrics m = MenuStyle::metrics(1.);
        QCOMPARE(m.padding, 4);
        QCOMPARE(m.rowHeight, 48);
        QCOMPARE(m.rowPadding, 14);
        QCOMPARE(m.iconSize, 24);
        QCOMPARE(m.gap, 10);
        QCOMPARE(m.radius, 10);
        QCOMPARE(m.rowRadius, 6);
        QCOMPARE(m.separator, 1);

        const MenuStyle::Metrics scaled = MenuStyle::metrics(1.5);
        QCOMPARE(scaled.rowHeight, 72);
        QCOMPARE(scaled.rowPadding, 21);
        QCOMPARE(scaled.separator, 2);
    }

    // Figma's 48 px rows 4 px inside the menu, separators of 1 px, no border
    void testRowsAndSeparators()
    {
        auto m = menu();
        QAction *first = m->addAction(QStringLiteral("Play"));
        QAction *separator = m->addSeparator();
        QAction *last = m->addAction(QStringLiteral("Stop"));
        m->ensurePolished();
        m->resize(m->sizeHint());

        QCOMPARE(m->actionGeometry(first), QRect(4, 4, 257, 48));
        QCOMPARE(m->actionGeometry(separator).height(), 1);
        QCOMPARE(m->actionGeometry(last).top(), 4 + 48 + 1);
        QCOMPARE(m->sizeHint().height(), 4 + 48 + 1 + 48 + 4);
    }

    // 265 for the item and sorting menus, 375 for the VLC menu, 292 for the
    // player's right-click menu
    void testWidths()
    {
        auto item = menu();
        item->addAction(QStringLiteral("Play"));
        item->ensurePolished();
        QCOMPARE(item->sizeHint().width(), 265);

        auto vlc = menu(QStringLiteral("vlcMenu"));
        vlc->addAction(QStringLiteral("Open File(s)"));
        vlc->ensurePolished();
        QCOMPARE(vlc->sizeHint().width(), 375);

        auto player = menu(QStringLiteral("playerMenu"));
        player->addAction(QStringLiteral("Play"));
        player->ensurePolished();
        QCOMPARE(player->sizeHint().width(), 292);
    }

    // BG/A3 Transparent over the window, the label FG/A2 from 18 px, the
    // shortcut FG/A3 ending 18 px from the right edge
    void testShortcutColumn()
    {
        auto m = menu(QStringLiteral("vlcMenu"));
        m->addAction(QStringLiteral("Open File(s)\tCtrl+O"));
        const QImage image = render(m.get());
        const MenuColors c = menuColors(m_tokens);

        QCOMPARE(image.width(), 375);
        QVERIFY(near(image.pixelColor(200, 10), c.background, 1));

        // the text ends 18 px from the edge: its ink ends inside the cell of
        // its last glyph (whatever font the platform falls back to when
        // Inter is not installed)
        const QRect row(4, 4, 367, 48);
        const int right = inkRight(image, row, c.background);
        const int lastGlyph = QFontMetrics(m->font()).horizontalAdvance(QLatin1Char('O'));
        QVERIFY2(right <= 375 - 18 && right >= 375 - 18 - lastGlyph,
                 qPrintable(QStringLiteral("%1 (last glyph %2)").arg(right).arg(lastGlyph)));

        // the shortcut's ink reaches FG/A3, never FG/A2
        int brightest = 0;
        for (int x = right - 30; x <= right; ++x)
            for (int y = row.top(); y <= row.bottom(); ++y)
                brightest = std::max(brightest, image.pixelColor(x, y).lightness());
        QVERIFY2(std::abs(brightest - c.shortcut.lightness()) <= 12, qPrintable(QString::number(brightest)));
    }

    // a checked entry of a menu without icons: BG/A2 row, Accent/Main check at
    // the end (856:2215)
    void testTrailingCheck()
    {
        auto m = menu();
        QAction *action = m->addAction(QStringLiteral("Unwatched First"));
        action->setCheckable(true);
        action->setChecked(true);
        const QImage image = render(m.get());
        const MenuColors c = menuColors(m_tokens);

        // the row's fill, inside its rounded corners
        QVERIFY(near(image.pixelColor(130, 8), c.hoveredBackground, 1));
        // the check: 24 px ending 18 px from the right edge
        bool accent = false;
        for (int x = 265 - 18 - 24; x < 265 - 18; ++x)
            for (int y = 16; y < 40; ++y)
                accent |= near(image.pixelColor(x, y), c.check, 20);
        QVERIFY(accent);
    }

    // without the Theme's style sheet (High Contrast), the platform draws
    void testPlatformMenusWithoutTheTheme()
    {
        auto m = menu();
        QVERIFY(MenuStyle::draws(m.get()));

        qApp->setStyleSheet(QString());
        QVERIFY(!MenuStyle::draws(m.get()));
        qApp->setStyleSheet(menuStyleSheet(m_tokens));
    }
};

int main(int argc, char **argv)
{
    // run offscreen, as the CI has no desktop environment
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestMenuStyle test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "test_menu_style.moc"
