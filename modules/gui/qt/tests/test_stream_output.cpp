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

#include <vlc_playlist.h>
#include <vlc_player.h>
#include <vlc_renderer_discovery.h>

#include "../dialogs/sout/stream_output.hpp"

#include <cstring>
#include <memory>

#include <QSignalSpy>
#include <QTest>

using vlc_playlist_locker = vlc_locker<vlc_playlist_t, vlc_playlist_Lock, vlc_playlist_Unlock>;

/* items that play for a long time, so the Queue does not advance by itself
 * while a test runs. "vlc:" items never open a stream output, so the tests
 * need no network. */
static const char LONG_MRL[] = "vlc://pause:600";

/* a profile from Convert's list, with H.264 video and AAC audio in MP4 */
static const QString MP4_PROFILE = QStringLiteral(
    "video_enable=yes;video_codec=h264;vcodec_bitrate=800;"
    "audio_enable=yes;audio_codec=mp4a;acodec_bitrate=128;acodec_channels=2;"
    "acodec_samplerate=44100;muxer_mux=mp4");

/* a profile with a streamable muxer */
static const QString WEBM_PROFILE = QStringLiteral(
    "video_enable=yes;video_codec=VP80;vcodec_bitrate=2000;"
    "audio_enable=yes;audio_codec=vorb;acodec_bitrate=128;acodec_channels=2;"
    "acodec_samplerate=44100;muxer_mux=webm");

class TestStreamOutput : public QObject
{
    Q_OBJECT

private:
    vlc_playlist_t *playlist() const
    {
        return m_env->intf->p_playlist;
    }

    vlc_player_t *player() const
    {
        return vlc_playlist_GetPlayer(playlist());
    }

    /* the stream output the player opens its next media with */
    QString playerSout() const
    {
        vlc_playlist_locker locker(playlist());
        char *sout = vlc_player_GetStreamOutput(player());
        QString ret = qfu(sout);
        free(sout);
        return ret;
    }

    int currentIndex() const
    {
        vlc_playlist_locker locker(playlist());
        return static_cast<int>(vlc_playlist_GetCurrentIndex(playlist()));
    }

    bool isStarted() const
    {
        vlc_playlist_locker locker(playlist());
        return vlc_player_IsStarted(player());
    }

    QStringList queueMrls() const
    {
        vlc_playlist_locker locker(playlist());
        QStringList mrls;
        for (size_t i = 0; i < vlc_playlist_Count(playlist()); ++i)
        {
            input_item_t *media = vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist(), i));
            mrls.push_back(qfu(media->psz_uri));
        }
        return mrls;
    }

    /* Fill the Queue with a, b and c, with b as the current item */
    void fillQueue(const char *mrlOfB = LONG_MRL)
    {
        vlc_playlist_locker locker(playlist());
        for (const char *name : { "a", "b", "c" })
        {
            input_item_t *media = input_item_New(
                strcmp(name, "b") ? LONG_MRL : mrlOfB, name);
            QVERIFY(media);
            vlc_playlist_AppendOne(playlist(), media);
            input_item_Release(media);
        }
        vlc_playlist_GoTo(playlist(), 1);
    }

    StreamOutput::Settings http() const
    {
        StreamOutput::Settings settings;
        settings.protocol = StreamOutput::HTTP;
        settings.port = 8080;
        return settings;
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
        m_stream = std::make_unique<StreamOutput>(playlist());
    }

    void cleanup()
    {
        m_stream.reset();
        vlc_playlist_locker locker(playlist());
        vlc_playlist_Stop(playlist());
        vlc_playlist_Clear(playlist());
        vlc_player_SetRenderer(player(), nullptr);
    }

    // The stream output chain of each protocol

    void testChain_data()
    {
        QTest::addColumn<int>("protocol");
        QTest::addColumn<QString>("address");
        QTest::addColumn<int>("port");
        QTest::addColumn<QString>("profile");
        QTest::addColumn<bool>("alsoPlayHere");
        QTest::addColumn<QString>("chain");

        QTest::newRow("http on every address")
            << int(StreamOutput::HTTP) << "" << 8080 << "" << false
            << "#http{mux=ts,dst=:8080/}";
        QTest::newRow("http on one address")
            << int(StreamOutput::HTTP) << "192.168.1.10" << 8081 << "" << false
            << "#http{mux=ts,dst=192.168.1.10:8081/}";
        QTest::newRow("http with a streamable profile keeps its muxer")
            << int(StreamOutput::HTTP) << "" << 8080 << WEBM_PROFILE << false
            << "#transcode{vcodec=VP80,vb=2000,acodec=vorb,ab=128,channels=2,samplerate=44100,scodec=none}"
               ":http{mux=webm,dst=:8080/}";
        QTest::newRow("http turns mp4 into a transport stream")
            << int(StreamOutput::HTTP) << "" << 8080 << MP4_PROFILE << false
            << "#transcode{vcodec=h264,vb=800,acodec=mp4a,ab=128,channels=2,samplerate=44100,scodec=none}"
               ":http{mux=ts,dst=:8080/}";
        QTest::newRow("rtsp")
            << int(StreamOutput::RTSP) << "" << 8554 << "" << false
            << "#rtp{sdp=rtsp://:8554/}";
        QTest::newRow("rtsp on one address")
            << int(StreamOutput::RTSP) << "10.0.0.2" << 554 << "" << false
            << "#rtp{sdp=rtsp://10.0.0.2:554/}";
        QTest::newRow("rtp")
            << int(StreamOutput::RTP) << "239.255.0.1" << 5004 << "" << false
            << "#rtp{dst=239.255.0.1,port=5004,mux=ts}";
        QTest::newRow("srt")
            << int(StreamOutput::SRT) << "example.org" << 7001 << "" << false
            << "#srt{mux=ts,dst=example.org:7001}";
        QTest::newRow("also play here")
            << int(StreamOutput::HTTP) << "" << 8080 << "" << true
            << "#duplicate{dst=display,dst=http{mux=ts,dst=:8080/}}";
        QTest::newRow("also play here, transcoded")
            << int(StreamOutput::SRT) << "example.org" << 7001 << WEBM_PROFILE << true
            << "#transcode{vcodec=VP80,vb=2000,acodec=vorb,ab=128,channels=2,samplerate=44100,scodec=none}"
               ":duplicate{dst=display,dst=srt{mux=ts,dst=example.org:7001}}";
    }

    void testChain()
    {
        QFETCH(int, protocol);
        QFETCH(QString, address);
        QFETCH(int, port);
        QFETCH(QString, profile);
        QFETCH(bool, alsoPlayHere);
        QFETCH(QString, chain);

        StreamOutput::Settings settings;
        settings.protocol = static_cast<StreamOutput::Protocol>(protocol);
        settings.address = address;
        settings.port = port;
        settings.profile = profile;
        settings.alsoPlayHere = alsoPlayHere;
        QCOMPARE(StreamOutput::soutChain(settings), chain);
    }

    // RTP and SRT send to a receiver, so they need its address

    void testAddressRequired_data()
    {
        QTest::addColumn<int>("protocol");
        QTest::addColumn<bool>("required");

        QTest::newRow("http") << int(StreamOutput::HTTP) << false;
        QTest::newRow("rtsp") << int(StreamOutput::RTSP) << false;
        QTest::newRow("rtp") << int(StreamOutput::RTP) << true;
        QTest::newRow("srt") << int(StreamOutput::SRT) << true;
    }

    void testAddressRequired()
    {
        QFETCH(int, protocol);
        QFETCH(bool, required);

        const auto p = static_cast<StreamOutput::Protocol>(protocol);
        QCOMPARE(StreamOutput::addressRequired(p), required);

        StreamOutput::Settings settings;
        settings.protocol = p;
        settings.port = StreamOutput::defaultPort(p);
        QCOMPARE(StreamOutput::soutChain(settings).isEmpty(), required);
    }

    void testDefaultPorts()
    {
        QCOMPARE(StreamOutput::defaultPort(StreamOutput::HTTP), 8080);
        QCOMPARE(StreamOutput::defaultPort(StreamOutput::RTSP), 8554);
        QCOMPARE(StreamOutput::defaultPort(StreamOutput::RTP), 5004);
        QCOMPARE(StreamOutput::defaultPort(StreamOutput::SRT), 7001);
    }

    void testInvalidPort()
    {
        StreamOutput::Settings settings = http();
        settings.port = 0;
        QVERIFY(StreamOutput::soutChain(settings).isEmpty());
        settings.port = 65536;
        QVERIFY(StreamOutput::soutChain(settings).isEmpty());
    }

    // Streaming takes over the player

    void testStreamQueue()
    {
        fillQueue();
        QSignalSpy streaming(m_stream.get(), &StreamOutput::streamingChanged);

        QVERIFY(m_stream->start(StreamOutput::Queue, http()));

        QVERIFY(m_stream->isStreaming());
        QCOMPARE(streaming.count(), 1);
        QCOMPARE(playerSout(), QString("#http{mux=ts,dst=:8080/}"));
        /* the Queue, from its current item on, as it plays */
        QCOMPARE(currentIndex(), 1);
        QTRY_VERIFY(isStarted());
    }

    void testStreamCurrentItem()
    {
        fillQueue();

        QVERIFY(m_stream->start(StreamOutput::CurrentItem, http()));

        QVERIFY(m_stream->isStreaming());
        QCOMPARE(playerSout(), QString("#http{mux=ts,dst=:8080/}"));
        QCOMPARE(currentIndex(), 1);
        QTRY_VERIFY(isStarted());
    }

    void testCurrentItemStopsAtItsEnd()
    {
        /* b ends by itself after a moment */
        fillQueue("vlc://pause:0.5");
        QVERIFY(m_stream->start(StreamOutput::CurrentItem, http()));

        /* the end of the item stops the Queue instead of going on to c */
        QTRY_VERIFY_WITH_TIMEOUT(!m_stream->isStreaming(), 10000);
        QCOMPARE(playerSout(), QString());
        QCOMPARE(currentIndex(), 1);
        QVERIFY(!isStarted());
    }

    // Stream & Cast opened on a file of the library

    void testStreamAFile()
    {
        fillQueue();

        QVERIFY(m_stream->startMedia(QStringLiteral("vlc://pause:601"), http()));

        QVERIFY(m_stream->isStreaming());
        QCOMPARE(playerSout(), QString("#http{mux=ts,dst=:8080/}"));
        /* the file goes after the current item, and only it streams */
        QCOMPARE(queueMrls(), QStringList({ LONG_MRL, LONG_MRL, "vlc://pause:601", LONG_MRL }));
        QCOMPARE(currentIndex(), 2);
        QTRY_VERIFY(isStarted());
    }

    void testStreamAFileStopsAtItsEnd()
    {
        fillQueue();
        QVERIFY(m_stream->startMedia(QStringLiteral("vlc://pause:0.5"), http()));

        QTRY_VERIFY_WITH_TIMEOUT(!m_stream->isStreaming(), 10000);
        QCOMPARE(currentIndex(), 2);
        QVERIFY(!isStarted());
    }

    void testStreamAFileWithAnEmptyQueue()
    {
        QVERIFY(m_stream->startMedia(QStringLiteral("vlc://pause:601"), http()));

        QCOMPARE(queueMrls(), QStringList({ "vlc://pause:601" }));
        QCOMPARE(currentIndex(), 0);
        QTRY_VERIFY(isStarted());
    }

    void testStreamAFileWithInvalidSettings()
    {
        fillQueue();
        StreamOutput::Settings settings;
        settings.protocol = StreamOutput::SRT;
        settings.port = 7001;

        QVERIFY(!m_stream->startMedia(QStringLiteral("vlc://pause:601"), settings));
        QCOMPARE(queueMrls().size(), 3);
        QCOMPARE(currentIndex(), 1);
    }

    void testPlayAFile()
    {
        /* Cast: the file plays after the current item, on the device */
        fillQueue();

        QVERIFY(m_stream->playMedia(QStringLiteral("vlc://pause:601")));

        QVERIFY(!m_stream->isStreaming());
        QCOMPARE(playerSout(), QString());
        QCOMPARE(queueMrls(), QStringList({ LONG_MRL, LONG_MRL, "vlc://pause:601", LONG_MRL }));
        QCOMPARE(currentIndex(), 2);
        QTRY_VERIFY(isStarted());
    }

    void testNothingToStream()
    {
        QVERIFY(!m_stream->start(StreamOutput::Queue, http()));
        QVERIFY(!m_stream->start(StreamOutput::CurrentItem, http()));
        QVERIFY(!m_stream->isStreaming());
        QCOMPARE(playerSout(), QString());
    }

    void testInvalidSettings()
    {
        fillQueue();
        StreamOutput::Settings settings;
        settings.protocol = StreamOutput::SRT;
        settings.port = 7001;

        QVERIFY(!m_stream->start(StreamOutput::Queue, settings));
        QVERIFY(!m_stream->isStreaming());
        QCOMPARE(currentIndex(), 1);
    }

    void testStop()
    {
        fillQueue();
        QVERIFY(m_stream->start(StreamOutput::Queue, http()));
        QTRY_VERIFY(isStarted());

        m_stream->stop();

        QVERIFY(!m_stream->isStreaming());
        QCOMPARE(playerSout(), QString());
        QTRY_VERIFY(!isStarted());
        /* nothing streamed afterwards */
        {
            vlc_playlist_locker locker(playlist());
            vlc_playlist_Start(playlist());
        }
        QTRY_VERIFY(isStarted());
        QCOMPARE(playerSout(), QString());
    }

    void testStopRestoresTheEndOfQueueAction()
    {
        /* b ends by itself after a moment */
        fillQueue("vlc://pause:0.5");
        QVERIFY(m_stream->start(StreamOutput::CurrentItem, http()));
        m_stream->stop();
        QTRY_VERIFY(!isStarted());

        /* the Queue goes on after b again */
        {
            vlc_playlist_locker locker(playlist());
            vlc_playlist_GoTo(playlist(), 1);
            vlc_playlist_Start(playlist());
        }
        QTRY_COMPARE_WITH_TIMEOUT(currentIndex(), 2, 10000);
        QTRY_VERIFY(isStarted());
    }

    void testPlaybackStopEndsTheStream()
    {
        fillQueue();
        QVERIFY(m_stream->start(StreamOutput::Queue, http()));
        QTRY_VERIFY(isStarted());

        {
            vlc_playlist_locker locker(playlist());
            vlc_playlist_Stop(playlist());
        }

        QTRY_VERIFY(!m_stream->isStreaming());
        QCOMPARE(playerSout(), QString());
    }

    // Cast and Stream both use the player's output

    void testStreamingStopsCasting()
    {
        fillQueue();
        vlc_renderer_item_t *device = vlc_renderer_item_new(
            "type", "Living room", "dummy://1.1.1.1:1", nullptr, nullptr, nullptr, 0);
        QVERIFY(device);
        {
            vlc_playlist_locker locker(playlist());
            vlc_player_SetRenderer(player(), device);
        }

        QVERIFY(m_stream->start(StreamOutput::Queue, http()));

        {
            vlc_playlist_locker locker(playlist());
            QCOMPARE(vlc_player_GetRenderer(player()), nullptr);
        }
        vlc_renderer_item_release(device);
    }

    void testCastingEndsTheStream()
    {
        fillQueue();
        QVERIFY(m_stream->start(StreamOutput::Queue, http()));
        QTRY_VERIFY(isStarted());

        vlc_renderer_item_t *device = vlc_renderer_item_new(
            "type", "Living room", "dummy://1.1.1.1:1", nullptr, nullptr, nullptr, 0);
        QVERIFY(device);
        {
            vlc_playlist_locker locker(playlist());
            vlc_player_SetRenderer(player(), device);
        }
        vlc_renderer_item_release(device);

        QTRY_VERIFY(!m_stream->isStreaming());
        QCOMPARE(playerSout(), QString());
        /* the playback moves to the device, it does not stop */
        QVERIFY(isStarted());
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<StreamOutput> m_stream;
};

QTEST_GUILESS_MAIN(TestStreamOutput)
#include "test_stream_output.moc"
