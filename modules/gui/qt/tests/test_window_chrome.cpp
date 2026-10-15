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

#include "../maininterface/window_chrome.hpp"

using namespace WindowChrome;

// The window title, the side of the window buttons and the
// Picture-in-picture geometry
class TestWindowChrome : public QObject
{
    Q_OBJECT

private slots:
    void testWindowTitleWhilePlaying()
    {
        QCOMPARE(windowTitle(QStringLiteral("The Wand S01E02")),
                 QStringLiteral("The Wand S01E02 · jVLC"));
    }

    void testWindowTitleWhenNothingPlays()
    {
        QCOMPARE(windowTitle(QString()), QStringLiteral("jVLC"));
    }

    // GNOME: org.gnome.desktop.wm.preferences button-layout, as gsettings prints it
    void testGnomeButtonsOnTheRight()
    {
        QCOMPARE(buttonsSideFromGnomeLayout(QStringLiteral("'appmenu:minimize,maximize,close'\n")),
                 ButtonsSide::Right);
        QCOMPARE(buttonsSideFromGnomeLayout(QStringLiteral("appmenu:close")), ButtonsSide::Right);
    }

    void testGnomeButtonsOnTheLeft()
    {
        QCOMPARE(buttonsSideFromGnomeLayout(QStringLiteral("'close,minimize,maximize:'")),
                 ButtonsSide::Left);
        QCOMPARE(buttonsSideFromGnomeLayout(QStringLiteral("close:appmenu")), ButtonsSide::Left);
    }

    // without close, the other window buttons tell the side
    void testGnomeLayoutWithoutClose()
    {
        QCOMPARE(buttonsSideFromGnomeLayout(QStringLiteral("minimize,maximize:appmenu")),
                 ButtonsSide::Left);
    }

    void testGnomeLayoutUnknownDefaultsToTheRight()
    {
        QCOMPARE(buttonsSideFromGnomeLayout(QString()), ButtonsSide::Right);
        QCOMPARE(buttonsSideFromGnomeLayout(QStringLiteral("appmenu")), ButtonsSide::Right);
    }

    // KDE: kwinrc [org.kde.kdecoration2] ButtonsOnLeft / ButtonsOnRight, X is close
    void testKdeButtons()
    {
        QCOMPARE(buttonsSideFromKdeButtons(QStringLiteral("MS"), QStringLiteral("HIAX")),
                 ButtonsSide::Right);
        QCOMPARE(buttonsSideFromKdeButtons(QStringLiteral("XIA"), QStringLiteral("SM")),
                 ButtonsSide::Left);
        QCOMPARE(buttonsSideFromKdeButtons(QString(), QString()), ButtonsSide::Right);
    }

    // Picture-in-picture: about 480 px wide at the video's aspect ratio, in
    // the bottom-right corner of the screen
    void testPictureInPictureAtTheVideoAspectRatio()
    {
        const QRect screen(0, 0, 1920, 1040);
        QCOMPARE(pictureInPictureGeometry(screen, QSize(1920, 800), QRect()),
                 QRect(1424, 824, 480, 200));
    }

    void testPictureInPictureWithoutVideoIs16By9()
    {
        const QRect screen(0, 0, 1920, 1040);
        QCOMPARE(pictureInPictureGeometry(screen, QSize(), QRect()),
                 QRect(1424, 754, 480, 270));
    }

    void testPictureInPictureOfAPortraitVideoFitsHalfTheScreenHeight()
    {
        const QRect screen(0, 0, 1920, 1040);
        QCOMPARE(pictureInPictureGeometry(screen, QSize(1000, 2000), QRect()),
                 QRect(1644, 504, 260, 520));
    }

    void testPictureInPictureOnASecondScreen()
    {
        const QRect screen(1920, 0, 1280, 984);
        QCOMPARE(pictureInPictureGeometry(screen, QSize(1280, 720), QRect()),
                 QRect(2704, 698, 480, 270));
    }

    // it remembers its last size and position
    void testPictureInPictureRemembersItsGeometry()
    {
        const QRect screen(0, 0, 1920, 1040);
        const QRect saved(100, 80, 640, 360);
        QCOMPARE(pictureInPictureGeometry(screen, QSize(1920, 800), saved), saved);
    }

    void testPictureInPictureKeepsItsSavedGeometryOnTheScreen()
    {
        const QRect screen(0, 0, 1920, 1040);
        QCOMPARE(pictureInPictureGeometry(screen, QSize(), QRect(1700, 900, 480, 270)),
                 QRect(1440, 770, 480, 270));
    }

    // a geometry saved on another screen is not reused
    void testPictureInPictureIgnoresAGeometryOffTheScreen()
    {
        const QRect screen(0, 0, 1920, 1040);
        QCOMPARE(pictureInPictureGeometry(screen, QSize(), QRect(3000, 100, 480, 270)),
                 QRect(1424, 754, 480, 270));
    }
};

QTEST_GUILESS_MAIN(TestWindowChrome)
#include "test_window_chrome.moc"
