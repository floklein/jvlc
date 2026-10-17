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
#include <QTest>
#include <QAbstractItemModelTester>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <memory>

#include "vlc_stub_modules.hpp"

#include <vlc_playlist.h>

#include "../dialogs/convert/conversion_manager.hpp"

/* the "Audio - CD (uncompressed)" built-in profile */
static const char AUDIO_CD[] =
    "audio_enable=yes;audio_codec=s16l;acodec_bitrate=128;acodec_channels=2;"
    "acodec_samplerate=44100;muxer_mux=wav";

/* 2 s of audio */
static const char SHORT_MEDIA[] =
    "mock://video_track_count=0;audio_track_count=1;length=2000000";

/* 24 h of tiny audio blocks: long enough to be cancelled while it runs */
static const char LONG_MEDIA[] =
    "mock://video_track_count=0;audio_track_count=1;length=86400000000;"
    "audio_format=u8;audio_channels=1;audio_rate=8000;audio_sinewave=0";

static bool writeFile(const QString& file, const QByteArray& content)
{
    QFile out(file);
    return out.open(QIODevice::WriteOnly) && out.write(content) == content.size();
}

static QByteArray readFile(const QString& file)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly))
        return {};
    return in.readAll();
}

class TestConversionManager : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<ConversionManager> m_manager;
    std::unique_ptr<QAbstractItemModelTester> m_tester;

    QVariant role(int row, ConversionManager::Roles role) const
    {
        return m_manager->data(m_manager->index(row), role);
    }

    ConversionManager::State state(int row) const
    {
        return role(row, ConversionManager::STATE).value<ConversionManager::State>();
    }

    QString path(const char* name) const
    {
        return m_dir->filePath(name);
    }

    QString rawChain(const QString& destination) const
    {
        return QStringLiteral("#std{access=file,mux=raw,dst='%1'}").arg(destination);
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
        m_manager = std::make_unique<ConversionManager>(VLC_OBJECT(m_env->intf));
        m_tester = std::make_unique<QAbstractItemModelTester>(m_manager.get());
    }

    void cleanup()
    {
        m_tester.reset();
        m_manager.reset();
        m_dir.reset();
    }

    void testOutputGoesNextToTheOriginal()
    {
        const QString source = QUrl::fromLocalFile(path("clip.mkv")).toString();
        QCOMPARE(m_manager->convertedPath(source, "mp4"), path("clip-converted.mp4"));

        QFile existing(path("clip-converted.mp4"));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.close();
        QCOMPARE(m_manager->convertedPath(source, "mp4"), path("clip-converted (2).mp4"));
    }

    void testConversionRunsInTheBackground()
    {

        m_manager->convert({ SHORT_MEDIA }, AUDIO_CD,
                           path("out.wav"), false);

        QCOMPARE(m_manager->rowCount(), 1);
        QCOMPARE(m_manager->activeCount(), 1);
        QCOMPARE(role(0, ConversionManager::DESTINATION).toString(), path("out.wav"));

        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Done, 10000);
        QCOMPARE(m_manager->activeCount(), 0);
        QCOMPARE(role(0, ConversionManager::PROGRESS).toDouble(), 1.0);
        QVERIFY(QFileInfo(path("out.wav")).size() > 0);

        /* playback and the Queue are untouched */
        vlc_playlist_t* playlist = m_env->intf->p_playlist;
        vlc_playlist_Lock(playlist);
        QCOMPARE(vlc_playlist_Count(playlist), size_t(0));
        vlc_player_t* player = vlc_playlist_GetPlayer(playlist);
        QVERIFY(vlc_player_GetCurrentMedia(player) == nullptr);
        QCOMPARE(vlc_player_GetState(player), VLC_PLAYER_STATE_STOPPED);
        vlc_playlist_Unlock(playlist);
    }

    void testConversionsRunOneAfterTheOther()
    {
        m_manager->add(LONG_MEDIA, rawChain(path("a.raw")), path("a.raw"));
        m_manager->add(SHORT_MEDIA, rawChain(path("b.raw")), path("b.raw"));

        QTRY_COMPARE(state(0), ConversionManager::Running);
        QCOMPARE(state(1), ConversionManager::Pending);
        QCOMPARE(m_manager->activeCount(), 2);

        /* the running conversion reports its progress */
        QTRY_VERIFY(role(0, ConversionManager::PROGRESS).toDouble() > 0.);
        QVERIFY(m_manager->progress() > 0.);
        QCOMPARE(state(0), ConversionManager::Running);

        m_manager->cancel(0);
        QTRY_COMPARE_WITH_TIMEOUT(state(1), ConversionManager::Done, 10000);
        QCOMPARE(state(0), ConversionManager::Cancelled);
        QVERIFY(QFileInfo(path("b.raw")).size() > 0);
    }

    void testCancelRemovesThePartialFile()
    {
        m_manager->add(LONG_MEDIA, rawChain(path("a.raw")), path("a.raw"));
        m_manager->add(LONG_MEDIA, rawChain(path("b.raw")), path("b.raw"));
        QTRY_COMPARE(state(0), ConversionManager::Running);

        m_manager->cancel(1);
        QCOMPARE(state(1), ConversionManager::Cancelled);

        m_manager->cancel(0);
        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Cancelled, 10000);
        QCOMPARE(m_manager->activeCount(), 0);
        QVERIFY(!QFileInfo::exists(path("a.raw")));
        QVERIFY(!QFileInfo::exists(path("b.raw")));
    }

    void testUnreadableSourceFails()
    {
        m_manager->add("mock://error=1", rawChain(path("a.raw")), path("a.raw"));
        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Failed, 10000);
        QVERIFY(!QFileInfo::exists(path("a.raw")));
    }

    void testExistingFileIsNotOverwritten()
    {
        QFile existing(path("out.wav"));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("keep me");
        existing.close();

        m_manager->add(SHORT_MEDIA,
                       QStringLiteral("#transcode{acodec=s16l}:std{access=file{no-overwrite},"
                                      "mux=wav,dst='%1'}").arg(path("out.wav")),
                       path("out.wav"));
        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Failed, 10000);

        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), "keep me");
    }

    void testFailedSaveAsKeepsTheReplacedFile()
    {
        QVERIFY(writeFile(path("out.wav"), "keep me"));

        /* the user chose to replace it, but the source can't be read */
        m_manager->convert({ "mock://error=1" }, AUDIO_CD, path("out.wav"), false);
        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Failed, 10000);

        QCOMPARE(readFile(path("out.wav")), QByteArray("keep me"));
        QCOMPARE(QDir(m_dir->path()).entryList(QDir::Files), QStringList{ "out.wav" });
    }

    void testCancelledSaveAsKeepsTheReplacedFile()
    {
        QVERIFY(writeFile(path("out.wav"), "keep me"));

        m_manager->convert({ LONG_MEDIA }, AUDIO_CD, path("out.wav"), false);
        QTRY_COMPARE(state(0), ConversionManager::Running);
        QTRY_VERIFY(role(0, ConversionManager::PROGRESS).toDouble() > 0.);
        m_manager->cancel(0);
        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Cancelled, 10000);

        QCOMPARE(readFile(path("out.wav")), QByteArray("keep me"));
        QCOMPARE(QDir(m_dir->path()).entryList(QDir::Files), QStringList{ "out.wav" });
    }

    void testSaveAsReplacesTheFileOnceDone()
    {
        QVERIFY(writeFile(path("out.wav"), "replace me"));

        m_manager->convert({ SHORT_MEDIA }, AUDIO_CD, path("out.wav"), false);
        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Done, 10000);

        const QByteArray converted = readFile(path("out.wav"));
        QVERIFY(converted.startsWith("RIFF"));
        QCOMPARE(QDir(m_dir->path()).entryList(QDir::Files), QStringList{ "out.wav" });
    }

    void testFinishedConversionsAreCleared()
    {
        m_manager->add(LONG_MEDIA, rawChain(path("a.raw")), path("a.raw"));
        m_manager->add(LONG_MEDIA, rawChain(path("b.raw")), path("b.raw"));
        QTRY_COMPARE(state(0), ConversionManager::Running);
        m_manager->cancel(1);

        m_manager->clearFinished();
        QCOMPARE(m_manager->rowCount(), 1);
        QCOMPARE(state(0), ConversionManager::Running);

        m_manager->cancelAll();
        QTRY_COMPARE_WITH_TIMEOUT(state(0), ConversionManager::Cancelled, 10000);
    }
};

QTEST_GUILESS_MAIN(TestConversionManager)
#include "test_conversion_manager.moc"
