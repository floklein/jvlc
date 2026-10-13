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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input_item.h>

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>

#include "dialogs/open/disc_model.hpp"

#ifdef _WIN32
# define TEST_DEVICE "D:\\"
# define TEST_DEVICE_URI "/D:/"
#else
# define TEST_DEVICE "/dev/sr0"
# define TEST_DEVICE_URI "/dev/sr0"
#endif

using Drive = DiscModel::Drive;

class TestDiscModel : public QObject
{
    Q_OBJECT

private:
    // Creates the given entries (a trailing '/' makes a folder) under a new
    // temporary folder and returns its path.
    QString makeRoot(const QStringList &entries)
    {
        auto dir = std::make_unique<QTemporaryDir>();
        if (!dir->isValid())
            return {};
        for (const QString &entry : entries)
        {
            const QString path = dir->filePath(entry);
            if (entry.endsWith('/'))
                QDir().mkpath(path);
            else
            {
                QFile file(path);
                if (!file.open(QIODevice::WriteOnly))
                    return {};
            }
        }
        QString root = dir->path();
        m_dirs.push_back(std::move(dir));
        return root;
    }

    std::vector<std::unique_ptr<QTemporaryDir>> m_dirs;

private slots:
    void cleanup()
    {
        m_dirs.clear();
    }

    void testDetectType_data()
    {
        QTest::addColumn<QStringList>("entries");
        QTest::addColumn<DiscModel::Type>("type");

        QTest::newRow("dvd") << QStringList{"VIDEO_TS/", "VIDEO_TS/VIDEO_TS.IFO", "AUDIO_TS/"}
                             << DiscModel::DVD;
        QTest::newRow("dvd-lowercase") << QStringList{"video_ts/"} << DiscModel::DVD;
        QTest::newRow("bluray") << QStringList{"BDMV/", "BDMV/index.bdmv", "CERTIFICATE/"}
                                << DiscModel::BLURAY;
        QTest::newRow("audio-cd") << QStringList{"Track01.cda", "Track02.cda"}
                                  << DiscModel::AUDIO_CD;
        QTest::newRow("vcd") << QStringList{"MPEGAV/", "VCD/", "VCD/INFO.VCD"} << DiscModel::VCD;
        QTest::newRow("svcd") << QStringList{"MPEG2/", "SVCD/"} << DiscModel::VCD;
        QTest::newRow("data") << QStringList{"photos/", "readme.txt"} << DiscModel::UNKNOWN;
        QTest::newRow("empty") << QStringList{} << DiscModel::UNKNOWN;
    }

    void testDetectType()
    {
        QFETCH(QStringList, entries);
        QFETCH(DiscModel::Type, type);

        const QString root = makeRoot(entries);
        QVERIFY(!root.isEmpty());
        QCOMPARE(DiscModel::detectType(root), type);
    }

    void testDetectTypeMissingRoot()
    {
        const QString root = makeRoot({});
        QCOMPARE(DiscModel::detectType(root + "/missing"), DiscModel::NO_DISC);
    }

    void testRefreshListsDrives()
    {
        const QString dvdRoot = makeRoot({"VIDEO_TS/"});
        const QString blurayRoot = makeRoot({"BDMV/"});

        DiscModel model;
        QAbstractItemModelTester tester(&model);
        model.setDriveLister([=]() {
            return QVector<Drive>{
                Drive{"E:", {}, {}, DiscModel::NO_DISC},
                Drive{"F:", dvdRoot, "MOVIE", DiscModel::UNKNOWN},
                Drive{"G:", {}, {}, DiscModel::AUDIO_CD},
                Drive{"H:", blurayRoot, {}, DiscModel::UNKNOWN},
            };
        });

        QSignalSpy scanningSpy(&model, &DiscModel::scanningChanged);
        model.refresh();
        QVERIFY(model.isScanning());
        QTRY_VERIFY(!model.isScanning());
        QCOMPARE(scanningSpy.count(), 2);

        QCOMPARE(model.rowCount(), 4);
        auto type = [&](int row) {
            return static_cast<DiscModel::Type>(model.data(model.index(row), DiscModel::TYPE).toInt());
        };
        QCOMPARE(type(0), DiscModel::NO_DISC);
        QCOMPARE(type(1), DiscModel::DVD);
        QCOMPARE(type(2), DiscModel::AUDIO_CD);
        QCOMPARE(type(3), DiscModel::BLURAY);

        QCOMPARE(model.data(model.index(1), DiscModel::DEVICE).toString(), QStringLiteral("F:"));
        QCOMPARE(model.data(model.index(1), DiscModel::LABEL).toString(), QStringLiteral("MOVIE"));
        QVERIFY(model.data(model.index(1), DiscModel::NAME).toString().contains("MOVIE"));
        QVERIFY(model.data(model.index(1), DiscModel::NAME).toString().contains("F:"));
        QCOMPARE(model.data(model.index(1), DiscModel::TYPE_NAME).toString(), QStringLiteral("DVD"));
        QCOMPARE(model.data(model.index(3), DiscModel::TYPE_NAME).toString(), QStringLiteral("Blu-ray"));

        // the first drive holding a disc
        QCOMPARE(model.defaultIndex(), 1);
    }

    void testRefreshReplacesDrives()
    {
        QVector<Drive> drives{ Drive{"E:", {}, {}, DiscModel::NO_DISC} };

        DiscModel model;
        QAbstractItemModelTester tester(&model);
        model.setDriveLister([&drives]() { return drives; });

        model.refresh();
        QTRY_VERIFY(!model.isScanning());
        QCOMPARE(model.rowCount(), 1);
        // no disc anywhere: the first drive
        QCOMPARE(model.defaultIndex(), 0);

        drives = {};
        QSignalSpy defaultSpy(&model, &DiscModel::defaultIndexChanged);
        model.refresh();
        QTRY_VERIFY(!model.isScanning());
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.defaultIndex(), -1);
        QCOMPARE(defaultSpy.count(), 1);
    }

    void testMedia_data()
    {
        QTest::addColumn<DiscModel::Type>("type");
        QTest::addColumn<bool>("useMenus");
        QTest::addColumn<int>("title");
        QTest::addColumn<int>("chapter");
        QTest::addColumn<int>("audioTrack");
        QTest::addColumn<int>("subtitleTrack");
        QTest::addColumn<QString>("uri");
        QTest::addColumn<QStringList>("options");

        const QString dev = QStringLiteral(TEST_DEVICE_URI);

        QTest::newRow("dvd-menus") << DiscModel::DVD << true << 0 << 0 << 0 << 0
                                   << "dvd://" + dev << QStringList{};
        QTest::newRow("dvd-no-menus") << DiscModel::DVD << false << 0 << 0 << 0 << 0
                                      << "dvdsimple://" + dev << QStringList{};
        QTest::newRow("dvd-title-chapter-tracks") << DiscModel::DVD << true << 2 << 3 << 2 << 1
                                                  << "dvd://" + dev + "#2:3"
                                                  << QStringList{":audio-track=1", ":sub-track=0"};
        QTest::newRow("dvd-title") << DiscModel::DVD << false << 4 << 0 << 0 << 0
                                   << "dvdsimple://" + dev + "#4" << QStringList{};
        // a chapter without a title has no meaning
        QTest::newRow("dvd-chapter-only") << DiscModel::DVD << true << 0 << 5 << 0 << 0
                                          << "dvd://" + dev << QStringList{};
        QTest::newRow("bluray-menus") << DiscModel::BLURAY << true << 0 << 0 << 0 << 0
                                      << "bluray://" + dev << QStringList{":bluray-menu"};
        QTest::newRow("bluray-no-menus") << DiscModel::BLURAY << false << 3 << 2 << 0 << 3
                                         << "bluray://" + dev + "#3:2"
                                         << QStringList{":no-bluray-menu", ":sub-track=2"};
        QTest::newRow("audio-cd") << DiscModel::AUDIO_CD << true << 0 << 0 << 0 << 0
                                  << "cdda://" + dev << QStringList{};
        QTest::newRow("audio-cd-track") << DiscModel::AUDIO_CD << true << 5 << 2 << 1 << 1
                                        << "cdda://" + dev << QStringList{":cdda-track=5"};
        QTest::newRow("vcd") << DiscModel::VCD << true << 2 << 4 << 1 << 0
                             << "vcd://" + dev + "#2" << QStringList{":audio-track=0"};
    }

    void testMedia()
    {
        QFETCH(DiscModel::Type, type);
        QFETCH(bool, useMenus);
        QFETCH(int, title);
        QFETCH(int, chapter);
        QFETCH(int, audioTrack);
        QFETCH(int, subtitleTrack);
        QFETCH(QString, uri);
        QFETCH(QStringList, options);

        const vlc::playlist::Media media = DiscModel::media(QStringLiteral(TEST_DEVICE), type,
                                                            useMenus, title, chapter,
                                                            audioTrack, subtitleTrack);
        QVERIFY(media);
        input_item_t *item = media.raw();
        QCOMPARE(QString::fromUtf8(item->psz_uri), uri);

        QStringList itemOptions;
        for (int i = 0; i < item->i_options; ++i)
            itemOptions << QString::fromUtf8(item->ppsz_options[i]);
        QCOMPARE(itemOptions, options);
    }

    void testMediaName()
    {
        const auto named = DiscModel::media(QStringLiteral(TEST_DEVICE), DiscModel::DVD,
                                            true, 0, 0, 0, 0, QStringLiteral("MOVIE"));
        QVERIFY(named);
        QCOMPARE(QString::fromUtf8(named.raw()->psz_name), QStringLiteral("MOVIE"));

        // without a label, the type names the item
        const auto unnamed = DiscModel::media(QStringLiteral(TEST_DEVICE), DiscModel::AUDIO_CD,
                                              true, 0, 0, 0, 0);
        QVERIFY(unnamed);
        QCOMPARE(QString::fromUtf8(unnamed.raw()->psz_name), QStringLiteral("Audio CD"));
    }

    void testMediaNeedsADiscType()
    {
        QVERIFY(!DiscModel::media(QStringLiteral(TEST_DEVICE), DiscModel::UNKNOWN,
                                  true, 0, 0, 0, 0));
        QVERIFY(!DiscModel::media(QStringLiteral(TEST_DEVICE), DiscModel::NO_DISC,
                                  true, 0, 0, 0, 0));
    }
};

QTEST_GUILESS_MAIN(TestDiscModel)
#include "test_disc_model.moc"
