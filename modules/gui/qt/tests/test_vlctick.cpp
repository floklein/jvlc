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

/* The time and duration formats. A media's duration reads the same in the
 * library, the Queue and the player: the player's running time never goes
 * past it, so every format drops what is under its last unit. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <QTest>

#include "../util/vlctick.hpp"

class TestVLCTick : public QObject
{
    Q_OBJECT

private slots:
    void aDurationReadsTheSameEverywhere_data()
    {
        QTest::addColumn<qint64>("ms");
        QTest::addColumn<QString>("hms");
        QTest::addColumn<QString>("shortText");
        QTest::addColumn<QString>("longText");

        // Big Buck Bunny's trailer: 32.995 s
        QTest::newRow("just under a second") << qint64(32995)
            << "00:32" << "00:32" << "32 sec";
        QTest::newRow("half a second") << qint64(85500)
            << "01:25" << "01:25" << "1 min 25 s";
        QTest::newRow("just under a minute") << qint64(59999)
            << "00:59" << "00:59" << "59 sec";
        QTest::newRow("whole") << qint64(61000)
            << "01:01" << "01:01" << "1 min 1 s";
        QTest::newRow("just under the next minute, over an hour") << qint64((3600 + 2 * 60 + 59) * 1000 + 900)
            << "01:02:59" << "1h02" << "1 h 2 min";
    }

    void aDurationReadsTheSameEverywhere()
    {
        QFETCH(qint64, ms);
        QFETCH(QString, hms);
        QFETCH(QString, shortText);
        QFETCH(QString, longText);

        const VLCDuration duration = VLCDuration::fromMS(ms);
        QCOMPARE(duration.formatHMS(), hms);
        QCOMPARE(duration.formatShort(), shortText);
        QCOMPARE(duration.formatLong(), longText);

        // the player's length and time are the same media time
        const VLCTime time(duration);
        QCOMPARE(time.formatHMS(), hms);
        QCOMPARE(time.formatShort(), shortText);
    }

    void underASecond()
    {
        QCOMPARE(VLCDuration::fromMS(430).formatShort(), QStringLiteral("430ms"));
        QCOMPARE(VLCDuration::fromMS(430).formatLong(), QStringLiteral("430 ms"));
        QCOMPARE(VLCDuration::fromMS(430).formatShort(0), QStringLiteral("00:00"));
    }

    // the player's bars show the hours even under an hour (Figma 491:1779:
    // "00:02:10" / "00:25:40")
    void thePlayerShowsTheHours()
    {
        QCOMPARE(VLCDuration::fromMS(130 * 1000).formatHMS(VLCTick::WithHours),
                 QStringLiteral("00:02:10"));
        QCOMPARE(VLCTime(VLCDuration::fromMS(1540 * 1000 + 999)).formatHMS(VLCTick::WithHours),
                 QStringLiteral("00:25:40"));
        QCOMPARE(VLCDuration::fromMS((3600 + 2 * 60 + 59) * 1000).formatHMS(VLCTick::WithHours),
                 QStringLiteral("01:02:59"));
        QCOMPARE(VLCDuration::fromMS(0).formatHMS(VLCTick::WithHours),
                 QStringLiteral("00:00:00"));
        // under a second, the milliseconds still win when asked for
        QCOMPARE(VLCDuration::fromMS(430).formatHMS(VLCTick::WithHours | VLCTick::SubSecondFormattedAsMS),
                 QStringLiteral("430 ms"));
        // without a duration
        QCOMPARE(VLCDuration().formatHMS(VLCTick::WithHours), QStringLiteral("--:--:--"));
        // the flag changes nothing else
        QCOMPARE(VLCDuration::fromMS(130 * 1000).formatHMS(0), QStringLiteral("02:10"));
    }

    // the bookmarks of the Queue panel show the milliseconds (Figma 6320:13816:
    // "00:00:42:023")
    void bookmarksShowTheMilliseconds()
    {
        const int flags = VLCTick::WithHours | VLCTick::WithMilliseconds;
        QCOMPARE(VLCDuration::fromMS(42023).formatHMS(flags), QStringLiteral("00:00:42:023"));
        QCOMPARE(VLCDuration::fromMS(44125).formatHMS(flags), QStringLiteral("00:00:44:125"));
        QCOMPARE(VLCDuration::fromMS((3600 + 61) * 1000 + 5).formatHMS(flags),
                 QStringLiteral("01:01:01:005"));
        // a bookmark at the start, and one under a second
        QCOMPARE(VLCDuration::fromMS(0).formatHMS(flags), QStringLiteral("00:00:00:000"));
        QCOMPARE(VLCDuration::fromMS(430).formatHMS(flags), QStringLiteral("00:00:00:430"));
        // without the hours
        QCOMPARE(VLCDuration::fromMS(42023).formatHMS(VLCTick::WithMilliseconds),
                 QStringLiteral("00:42:023"));
    }
};

QTEST_GUILESS_MAIN(TestVLCTick)
#include "test_vlctick.moc"
