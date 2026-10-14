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

#include "vlc_stub_modules.hpp"

#include "../network/saved_streams_model.hpp"
#include "../network/network_playlist_model.hpp"
#include "../network/preparser_playlist_reader.hpp"
#include "../network/podcast_feeds.hpp"
#include <vlc_cxx_helpers.hpp>

#include <vlc_configuration.h>

#include <QCoreApplication>
#include <QUrl>
#include <QAbstractItemModelTester>
#include <QFile>
#include <QHash>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QTest>

/* Answers with the lists it is given, from the event loop */
class FakeReader : public NetworkPlaylistReader
{
public:
    QHash<QString, QVector<NetworkPlaylistItem>> lists;
    QStringList requests;
    /* keep the answers until answerAll() */
    bool answerLater = false;

    void read(const QString &url, QObject *receiver, Callback done) override
    {
        requests.push_back(url);
        auto answer = [receiver = QPointer<QObject>(receiver), done, items = lists.value(url)]() {
            if (receiver)
                done(items);
        };
        if (answerLater)
            m_answers.push_back(answer);
        else
            QTimer::singleShot(0, this, answer);
    }

    void answerAll()
    {
        const auto answers = std::exchange(m_answers, {});
        for (const auto &answer : answers)
            answer();
    }

private:
    QVector<std::function<void()>> m_answers;
};

class TestSavedStreams : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<QTemporaryDir> m_dir;

    QString storePath() const
    {
        return m_dir->filePath(QStringLiteral("network.json"));
    }

    static QVariant data(const SavedStreamsModel &model, int row, int role)
    {
        return model.data(model.index(row), role);
    }

private slots:
    void initTestCase()
    {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
    }

    void cleanupTestCase()
    {
        m_env.reset();
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup()
    {
        m_dir.reset();
    }

    void testOneUrlIsASavedStream()
    {
        SavedStreamsModel model(storePath());
        QAbstractItemModelTester tester(&model);

        const QString id = model.save(QStringLiteral("https://example.com/live.ts"), {});

        QVERIFY(!id.isEmpty());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(data(model, 0, SavedStreamsModel::ID).toString(), id);
        QCOMPARE(data(model, 0, SavedStreamsModel::IS_PLAYLIST).toBool(), false);
        QCOMPARE(data(model, 0, SavedStreamsModel::URLS).toStringList(),
                 QStringList { QStringLiteral("https://example.com/live.ts") });
        /* without a name, a Saved stream is named after its URL */
        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(),
                 QStringLiteral("https://example.com/live.ts"));
    }

    void testNamedSavedStream()
    {
        SavedStreamsModel model(storePath());

        model.save(QStringLiteral("https://example.com/live.ts"), QStringLiteral("News"));

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(), QStringLiteral("News"));
        QCOMPARE(data(model, 0, SavedStreamsModel::IS_PLAYLIST).toBool(), false);
    }

    void testSeveralUrlsAreANetworkPlaylist()
    {
        SavedStreamsModel model(storePath());
        QAbstractItemModelTester tester(&model);

        const QString id = model.save(QStringLiteral("https://example.com/one.ts\n"
                                                     "\n"
                                                     "  rtsp://camera.local/stream  \n"),
                                      QStringLiteral("Favorite TV Channels"));

        QVERIFY(!id.isEmpty());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(data(model, 0, SavedStreamsModel::IS_PLAYLIST).toBool(), true);
        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(), QStringLiteral("Favorite TV Channels"));
        QCOMPARE(data(model, 0, SavedStreamsModel::URLS).toStringList(),
                 QStringList({ QStringLiteral("https://example.com/one.ts"),
                               QStringLiteral("rtsp://camera.local/stream") }));
    }

    void testUnnamedNetworkPlaylist()
    {
        SavedStreamsModel model(storePath());

        model.save(QStringLiteral("https://example.com/one.ts\nhttps://example.com/two.ts"), {});

        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(), QStringLiteral("Network Playlist"));
    }

    void testPlaylistUrlIsANetworkPlaylist_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<QString>("name");

        QTest::newRow("m3u") << "https://iptv-org.github.io/iptv/index.m3u" << "index.m3u";
        QTest::newRow("query") << "http://example.com/lists/fr.m3u?token=1" << "fr.m3u";
        QTest::newRow("pls") << "http://radio.example.com/listen.pls" << "listen.pls";
        QTest::newRow("xspf") << "https://example.com/my%20list.xspf" << "my list.xspf";
    }

    void testPlaylistUrlIsANetworkPlaylist()
    {
        QFETCH(QString, url);
        QFETCH(QString, name);

        SavedStreamsModel model(storePath());

        model.save(url, {});

        QCOMPARE(data(model, 0, SavedStreamsModel::IS_PLAYLIST).toBool(), true);
        /* named after its file */
        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(), name);
        QCOMPARE(data(model, 0, SavedStreamsModel::URLS).toStringList(), QStringList { url });
    }

    void testHlsStreamIsNotAPlaylist()
    {
        /* .m3u8 is how HTTP Live Streaming streams end, not a list of streams */
        QVERIFY(!SavedStreamsModel::isPlaylistUrl(QStringLiteral("https://example.com/live/index.m3u8")));
        QVERIFY(SavedStreamsModel::isPlaylistUrl(QStringLiteral("https://example.com/INDEX.M3U")));
    }

    void testTextWithoutUrlSavesNothing()
    {
        SavedStreamsModel model(storePath());

        QCOMPARE(model.save(QStringLiteral("  \nnot a link\n"), QStringLiteral("Name")), QString {});
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(SavedStreamsModel::urlsFromText(QStringLiteral("not a link\n\nftp:/broken\n\"http://example.com/a b\"")),
                 QStringList {});
    }

    void testUrlsFromText()
    {
        QCOMPARE(SavedStreamsModel::urlsFromText(QStringLiteral("  http://a.example/x  \r\n"
                                                                "plain words\n"
                                                                "\"https://b.example/y\"\n"
                                                                "<udp://@239.0.0.1:1234>\n")),
                 QStringList({ QStringLiteral("http://a.example/x"),
                               QStringLiteral("https://b.example/y"),
                               QStringLiteral("udp://@239.0.0.1:1234") }));
    }

    void testNewestFirst()
    {
        SavedStreamsModel model(storePath());

        model.save(QStringLiteral("https://example.com/first.ts"), {});
        model.save(QStringLiteral("https://example.com/second.ts"), {});

        QCOMPARE(data(model, 0, SavedStreamsModel::URL).toString(), QStringLiteral("https://example.com/second.ts"));
        QCOMPARE(data(model, 1, SavedStreamsModel::URL).toString(), QStringLiteral("https://example.com/first.ts"));
    }

    void testSavingAStreamAgainKeepsOneRow()
    {
        SavedStreamsModel model(storePath());
        QAbstractItemModelTester tester(&model);

        const QString first = model.save(QStringLiteral("https://example.com/first.ts"), QStringLiteral("First"));
        model.save(QStringLiteral("https://example.com/second.ts"), {});

        /* saving it again moves it to the top and keeps its name */
        QCOMPARE(model.save(QStringLiteral("https://example.com/first.ts"), {}), first);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(data(model, 0, SavedStreamsModel::ID).toString(), first);
        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(), QStringLiteral("First"));

        /* a new name replaces it */
        model.save(QStringLiteral("https://example.com/first.ts"), QStringLiteral("Renamed"));
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(), QStringLiteral("Renamed"));
    }

    void testRename()
    {
        SavedStreamsModel model(storePath());
        QAbstractItemModelTester tester(&model);

        const QString stream = model.save(QStringLiteral("https://example.com/live.ts"), {});
        const QString playlist = model.save(QStringLiteral("https://example.com/list.m3u"), {});

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.rename(stream, QStringLiteral("Live"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(model.get(stream).value(QStringLiteral("name")).toString(), QStringLiteral("Live"));

        model.rename(playlist, QStringLiteral("Channels"));
        QCOMPARE(model.get(playlist).value(QStringLiteral("name")).toString(), QStringLiteral("Channels"));

        /* an empty name gives back the default one */
        model.rename(stream, QStringLiteral("  "));
        QCOMPARE(model.get(stream).value(QStringLiteral("name")).toString(),
                 QStringLiteral("https://example.com/live.ts"));
        model.rename(playlist, {});
        QCOMPARE(model.get(playlist).value(QStringLiteral("name")).toString(), QStringLiteral("list.m3u"));
    }

    void testRemove()
    {
        SavedStreamsModel model(storePath());
        QAbstractItemModelTester tester(&model);

        const QString first = model.save(QStringLiteral("https://example.com/first.ts"), {});
        const QString second = model.save(QStringLiteral("https://example.com/second.ts"), {});

        model.remove(first);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.indexOf(first), -1);
        QCOMPARE(model.indexOf(second), 0);
        QVERIFY(model.get(first).isEmpty());

        /* unknown ids are ignored */
        model.remove(first);
        QCOMPARE(model.rowCount(), 1);
    }

    void testKeptAcrossRestarts()
    {
        QString stream;
        QString playlist;
        {
            SavedStreamsModel model(storePath());
            stream = model.save(QStringLiteral("https://example.com/live.ts"), QStringLiteral("Live"));
            playlist = model.save(QStringLiteral("https://example.com/one.ts\nhttps://example.com/two.ts"),
                                  QStringLiteral("Channels"));
            model.save(QStringLiteral("https://example.com/gone.ts"), {});
            model.remove(model.data(model.index(0), SavedStreamsModel::ID).toString());
        }

        SavedStreamsModel model(storePath());

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(data(model, 0, SavedStreamsModel::ID).toString(), playlist);
        QCOMPARE(data(model, 0, SavedStreamsModel::NAME).toString(), QStringLiteral("Channels"));
        QCOMPARE(data(model, 0, SavedStreamsModel::IS_PLAYLIST).toBool(), true);
        QCOMPARE(data(model, 0, SavedStreamsModel::URLS).toStringList(),
                 QStringList({ QStringLiteral("https://example.com/one.ts"),
                               QStringLiteral("https://example.com/two.ts") }));
        QCOMPARE(data(model, 1, SavedStreamsModel::ID).toString(), stream);
        QCOMPARE(data(model, 1, SavedStreamsModel::NAME).toString(), QStringLiteral("Live"));
    }

    void testBrokenFileStartsEmpty()
    {
        QFile file(storePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ not json");
        file.close();

        SavedStreamsModel model(storePath());
        QCOMPARE(model.rowCount(), 0);

        model.save(QStringLiteral("https://example.com/live.ts"), {});
        QCOMPARE(SavedStreamsModel(storePath()).rowCount(), 1);
    }

    /* Network Playlist contents */

    void testPlainUrlsAreListedInOrder()
    {
        NetworkPlaylistModel model;
        QAbstractItemModelTester tester(&model);
        FakeReader reader;
        model.setReader(&reader);

        model.setUrls({ QStringLiteral("https://example.com/one.ts"),
                        QStringLiteral("rtsp://camera.local/stream") });

        QVERIFY(!model.loading());
        QCOMPARE(reader.requests, QStringList {});
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::MRL).toString(),
                 QStringLiteral("https://example.com/one.ts"));
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::NAME).toString(),
                 QStringLiteral("https://example.com/one.ts"));
        QCOMPARE(model.data(model.index(1), NetworkPlaylistModel::MRL).toString(),
                 QStringLiteral("rtsp://camera.local/stream"));
    }

    void testPlaylistUrlIsReadInPlace()
    {
        NetworkPlaylistModel model;
        QAbstractItemModelTester tester(&model);
        FakeReader reader;
        reader.lists[QStringLiteral("https://example.com/tv.m3u")] = {
            { QStringLiteral("Channel 1"), QStringLiteral("https://tv.example/1.m3u8"), QStringLiteral("https://tv.example/1.png") },
            { QStringLiteral("Channel 2"), QStringLiteral("https://tv.example/2.m3u8"), {} },
        };
        model.setReader(&reader);

        model.setUrls({ QStringLiteral("https://example.com/first.ts"),
                        QStringLiteral("https://example.com/tv.m3u"),
                        QStringLiteral("https://example.com/last.ts") });

        QVERIFY(model.loading());
        QTRY_VERIFY(!model.loading());
        QCOMPARE(reader.requests, QStringList { QStringLiteral("https://example.com/tv.m3u") });

        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::MRL).toString(), QStringLiteral("https://example.com/first.ts"));
        QCOMPARE(model.data(model.index(1), NetworkPlaylistModel::NAME).toString(), QStringLiteral("Channel 1"));
        QCOMPARE(model.data(model.index(1), NetworkPlaylistModel::MRL).toString(), QStringLiteral("https://tv.example/1.m3u8"));
        QCOMPARE(model.data(model.index(1), NetworkPlaylistModel::ARTWORK).toString(), QStringLiteral("https://tv.example/1.png"));
        QCOMPARE(model.data(model.index(2), NetworkPlaylistModel::NAME).toString(), QStringLiteral("Channel 2"));
        QCOMPARE(model.data(model.index(3), NetworkPlaylistModel::MRL).toString(), QStringLiteral("https://example.com/last.ts"));

        QCOMPARE(model.mrlsFrom(2), QStringList({ QStringLiteral("https://tv.example/2.m3u8"),
                                                  QStringLiteral("https://example.com/last.ts") }));
    }

    void testReloadReadsThePlaylistAgain()
    {
        NetworkPlaylistModel model;
        QAbstractItemModelTester tester(&model);
        FakeReader reader;
        const QString url = QStringLiteral("https://example.com/tv.m3u");
        reader.lists[url] = { { QStringLiteral("Channel 1"), QStringLiteral("https://tv.example/1.m3u8"), {} } };
        model.setReader(&reader);
        model.setUrls({ url });
        QTRY_COMPARE(model.rowCount(), 1);

        /* the list changed at its source */
        reader.lists[url].push_back({ QStringLiteral("Channel 2"), QStringLiteral("https://tv.example/2.m3u8"), {} });
        model.reload();

        QTRY_VERIFY(!model.loading());
        QCOMPARE(reader.requests.count(), 2);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(1), NetworkPlaylistModel::NAME).toString(), QStringLiteral("Channel 2"));
    }

    void testUnreadablePlaylistIsEmpty()
    {
        NetworkPlaylistModel model;
        FakeReader reader;
        model.setReader(&reader);

        model.setUrls({ QStringLiteral("https://example.com/missing.m3u"),
                        QStringLiteral("https://example.com/live.ts") });

        QTRY_VERIFY(!model.loading());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::MRL).toString(), QStringLiteral("https://example.com/live.ts"));
    }

    void testLateAnswerOfAPreviousReadIsIgnored()
    {
        NetworkPlaylistModel model;
        FakeReader reader;
        reader.answerLater = true;
        const QString url = QStringLiteral("https://example.com/tv.m3u");
        reader.lists[url] = { { QStringLiteral("Old"), QStringLiteral("https://tv.example/old.ts"), {} } };
        model.setReader(&reader);
        model.setUrls({ url });

        reader.lists[url] = { { QStringLiteral("New"), QStringLiteral("https://tv.example/new.ts"), {} } };
        model.reload();
        /* both requests answer now, the first one with the old list */
        reader.answerAll();

        QVERIFY(!model.loading());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::NAME).toString(), QStringLiteral("New"));
    }

    void testPreparserReadsAnM3uList()
    {
        QFile file(m_dir->filePath(QStringLiteral("channels.m3u")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#EXTM3U\n"
                   "#EXTINF:-1 tvg-logo=\"https://tv.example/one.png\",Channel One\n"
                   "https://tv.example/one.m3u8\n"
                   "#EXTINF:-1,Channel Two\n"
                   "http://tv.example/two.ts\n");
        file.close();

        PreparserPlaylistReader reader(VLC_OBJECT(m_env->intf));
        NetworkPlaylistModel model;
        model.setReader(&reader);

        model.setUrls({ QUrl::fromLocalFile(file.fileName()).toString() });

        QTRY_VERIFY_WITH_TIMEOUT(!model.loading(), 20000);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::NAME).toString(), QStringLiteral("Channel One"));
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::MRL).toString(), QStringLiteral("https://tv.example/one.m3u8"));
        QCOMPARE(model.data(model.index(0), NetworkPlaylistModel::ARTWORK).toString(), QStringLiteral("https://tv.example/one.png"));
        QCOMPARE(model.data(model.index(1), NetworkPlaylistModel::NAME).toString(), QStringLiteral("Channel Two"));
        QCOMPARE(model.data(model.index(1), NetworkPlaylistModel::MRL).toString(), QStringLiteral("http://tv.example/two.ts"));
    }

    void testPreparserReaderOutlivedByItsRequests()
    {
        QFile file(m_dir->filePath(QStringLiteral("channels.m3u")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#EXTM3U\nhttp://tv.example/two.ts\n");
        file.close();

        bool called = false;
        {
            PreparserPlaylistReader reader(VLC_OBJECT(m_env->intf));
            QObject receiver;
            reader.read(QUrl::fromLocalFile(file.fileName()).toString(), &receiver,
                        [&called](const QVector<NetworkPlaylistItem> &) { called = true; });
        }
        /* the answer of a destroyed reader is dropped */
        QCoreApplication::processEvents();
        QVERIFY(!called);
    }

    /* Podcasts */

    void testAddAndRemovePodcast()
    {
        if (!config_FindConfig("podcast-urls"))
            QSKIP("no podcast module");
        config_PutPsz("podcast-urls", "http://feeds.example/one.rss");

        PodcastFeeds podcasts(VLC_OBJECT(m_env->intf));
        QCOMPARE(podcasts.urls(), QStringList { QStringLiteral("http://feeds.example/one.rss") });

        QVERIFY(podcasts.add(QStringLiteral("  https://feeds.example/two.xml ")));
        QCOMPARE(podcasts.urls(), QStringList({ QStringLiteral("http://feeds.example/one.rss"),
                                                QStringLiteral("https://feeds.example/two.xml") }));
        QCOMPARE(QString::fromUtf8(vlc::wrap_cptr(config_GetPsz("podcast-urls")).get()),
                 QStringLiteral("http://feeds.example/one.rss|https://feeds.example/two.xml"));

        /* a feed is added once, and only links are feeds */
        QVERIFY(!podcasts.add(QStringLiteral("https://feeds.example/two.xml")));
        QVERIFY(!podcasts.add(QStringLiteral("not a link")));
        QCOMPARE(podcasts.urls().size(), 2);

        podcasts.remove(QStringLiteral("http://feeds.example/one.rss"));
        QCOMPARE(podcasts.urls(), QStringList { QStringLiteral("https://feeds.example/two.xml") });
        QVERIFY(podcasts.contains(QStringLiteral("https://feeds.example/two.xml")));
        QVERIFY(!podcasts.contains(QStringLiteral("http://feeds.example/one.rss")));

        config_PutPsz("podcast-urls", "");
    }
};

QTEST_GUILESS_MAIN(TestSavedStreams)
#include "test_saved_streams.moc"
