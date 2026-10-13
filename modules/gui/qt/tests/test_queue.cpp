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

#include <vlc_modules.h>

#include "../playlist/playlist_controller.hpp"
#include "../playlist/queue_store.hpp"

#include <memory>

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

using vlc::playlist::Media;
using vlc::playlist::PlaylistController;
using vlc::playlist::QueueStore;
using vlc::playlist::vlc_playlist_locker;

/* An item that plays for a long time, so the Queue does not advance by
 * itself while a test runs */
static const QString LONG_MRL = QStringLiteral("vlc://pause:600");

static QVector<Media> medias(const QStringList &names)
{
    QVector<Media> list;
    for (const QString &name : names)
        list.push_back(Media(LONG_MRL, name));
    return list;
}

class TestQueue : public QObject
{
    Q_OBJECT

private:
    vlc_playlist_t *playlist() const
    {
        return m_env->intf->p_playlist;
    }

    /* The Queue as the core sees it: the names of its items */
    QStringList queueNames() const
    {
        QStringList names;
        vlc_playlist_locker locker(playlist());
        for (size_t i = 0; i < vlc_playlist_Count(playlist()); ++i)
        {
            input_item_t *media =
                vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist(), i));
            names.push_back(qfu(media->psz_name));
        }
        return names;
    }

    int currentIndex() const
    {
        vlc_playlist_locker locker(playlist());
        return static_cast<int>(vlc_playlist_GetCurrentIndex(playlist()));
    }

    bool isPlaying() const
    {
        vlc_playlist_locker locker(playlist());
        vlc_player_t *player = vlc_playlist_GetPlayer(playlist());
        return vlc_player_IsStarted(player) && !vlc_player_IsPaused(player);
    }

    /* Fill the Queue with a, b and c, with b as the current item */
    void fillQueue()
    {
        m_controller->append(medias({"a", "b", "c"}), false);
        m_controller->goTo(1);
        QCOMPARE(queueNames(), QStringList({"a", "b", "c"}));
        QCOMPARE(currentIndex(), 1);
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
        m_controller = std::make_unique<PlaylistController>(playlist());
        QTRY_VERIFY(m_controller->isInitialized());
    }

    void cleanup()
    {
        {
            vlc_playlist_locker locker(playlist());
            vlc_playlist_Clear(playlist());
            vlc_playlist_SetPlaybackRepeat(playlist(), VLC_PLAYLIST_PLAYBACK_REPEAT_NONE);
        }
        m_controller.reset();
    }

    // Play rule

    void testPlayReplacesQueue()
    {
        fillQueue();
        QSignalSpy replaced(m_controller.get(), &PlaylistController::queueReplaced);

        m_controller->playMedia(medias({"x", "y"}));

        QCOMPARE(queueNames(), QStringList({"x", "y"}));
        QCOMPARE(currentIndex(), 0);
        QTRY_VERIFY(isPlaying());
        QCOMPARE(replaced.count(), 1);
        QVERIFY(m_controller->canUndoReplace());
    }

    void testPlayOnEmptyQueueHasNoUndo()
    {
        QSignalSpy replaced(m_controller.get(), &PlaylistController::queueReplaced);

        m_controller->playMedia(medias({"x"}));

        QCOMPARE(queueNames(), QStringList({"x"}));
        QCOMPARE(replaced.count(), 0);
        QVERIFY(!m_controller->canUndoReplace());
    }

    void testUndoRestoresQueue()
    {
        fillQueue();
        m_controller->playMedia(medias({"x", "y"}));

        m_controller->undoReplace();

        QCOMPARE(queueNames(), QStringList({"a", "b", "c"}));
        QCOMPARE(currentIndex(), 1);
        QVERIFY(!m_controller->canUndoReplace());
    }

    void testUndoResumesPlayback()
    {
        fillQueue();
        m_controller->play();
        QTRY_VERIFY(isPlaying());
        m_controller->playMedia(medias({"x"}));

        m_controller->undoReplace();

        QCOMPARE(queueNames(), QStringList({"a", "b", "c"}));
        QCOMPARE(currentIndex(), 1);
        QTRY_VERIFY(isPlaying());
    }

    void testUndoExpires()
    {
        fillQueue();
        m_controller->setUndoReplaceTimeout(std::chrono::milliseconds(50));
        m_controller->playMedia(medias({"x"}));
        QVERIFY(m_controller->canUndoReplace());

        QTRY_VERIFY(!m_controller->canUndoReplace());

        m_controller->undoReplace();
        QCOMPARE(queueNames(), QStringList({"x"}));
    }

    void testUndoReplaceTimeoutIsFiveSeconds()
    {
        QCOMPARE(m_controller->undoReplaceTimeout(), std::chrono::milliseconds(5000));
    }

    void testPlayAddsToQueueSetting()
    {
        fillQueue();
        QSignalSpy replaced(m_controller.get(), &PlaylistController::queueReplaced);
        m_controller->setPlayAddsToQueue(true);

        m_controller->playMedia(medias({"x", "y"}));

        QCOMPARE(queueNames(), QStringList({"a", "b", "c", "x", "y"}));
        QCOMPARE(currentIndex(), 3);
        QTRY_VERIFY(isPlaying());
        QCOMPARE(replaced.count(), 0);
        QVERIFY(!m_controller->canUndoReplace());
    }

    // Play next and Add to Queue

    void testPlayNextInsertsAfterCurrent()
    {
        fillQueue();

        m_controller->playNext(medias({"x", "y"}));

        QCOMPARE(queueNames(), QStringList({"a", "b", "x", "y", "c"}));
        QCOMPARE(currentIndex(), 1);
    }

    void testPlayNextWithoutCurrentInsertsFirst()
    {
        m_controller->append(medias({"a", "b"}), false);
        QCOMPARE(currentIndex(), -1);

        m_controller->playNext(medias({"x"}));

        QCOMPARE(queueNames(), QStringList({"x", "a", "b"}));
    }

    void testPlayNextOnLastItemAppends()
    {
        fillQueue();
        m_controller->goTo(2);

        m_controller->playNext(medias({"x"}));

        QCOMPARE(queueNames(), QStringList({"a", "b", "c", "x"}));
        QCOMPARE(currentIndex(), 2);
    }

    void testAddToQueueAppends()
    {
        fillQueue();

        m_controller->append(medias({"x"}), false);

        QCOMPARE(queueNames(), QStringList({"a", "b", "c", "x"}));
        QCOMPARE(currentIndex(), 1);
    }

    // End of the Queue

    void testEndOfQueuePausesOnLastFrame()
    {
        if (!module_exists("demux_mock"))
            QSKIP("needs the mock demux (not installed)");

        const QString mrl = QStringLiteral("mock://video_track_count=0;audio_track_count=1;length=100000");
        m_controller->playMedia(QVector<Media>{Media(mrl, "a"), Media(mrl, "b")});

        /* The last item ends paused, and the Queue keeps its items */
        QTRY_VERIFY_WITH_TIMEOUT([this] {
            vlc_playlist_locker locker(playlist());
            return vlc_player_GetState(vlc_playlist_GetPlayer(playlist()))
                == VLC_PLAYER_STATE_PAUSED;
        }(), 4000);
        QCOMPARE(queueNames(), QStringList({"a", "b"}));
        QCOMPARE(currentIndex(), 1);
    }

    void testPlayAfterEndOfQueueRestartsFromFirstItem()
    {
        if (!module_exists("demux_mock"))
            QSKIP("needs the mock demux (not installed)");

        const QString mrl = QStringLiteral("mock://video_track_count=0;audio_track_count=1;length=100000");
        m_controller->playMedia(QVector<Media>{Media(mrl, "a"), Media(mrl, "b")});
        const auto ended = [this] {
            vlc_playlist_locker locker(playlist());
            return vlc_player_IsEnded(vlc_playlist_GetPlayer(playlist()));
        };
        QTRY_VERIFY_WITH_TIMEOUT(ended(), 4000);
        QCOMPARE(currentIndex(), 1);

        m_controller->togglePlayPause();

        QCOMPARE(currentIndex(), 0);
        QVERIFY(!ended());
        /* the Queue plays through again and ends on its last item */
        QTRY_COMPARE_WITH_TIMEOUT(currentIndex(), 1, 4000);
        QTRY_VERIFY_WITH_TIMEOUT(ended(), 4000);
    }

    void testEndOfQueueWithRepeatAllLoops()
    {
        if (!module_exists("demux_mock"))
            QSKIP("needs the mock demux (not installed)");

        m_controller->setRepeatMode(PlaylistController::PLAYBACK_REPEAT_ALL);
        const QString mrl = QStringLiteral("mock://video_track_count=0;audio_track_count=1;length=500000");
        m_controller->playMedia(QVector<Media>{Media(mrl, "a"), Media(mrl, "b")});

        /* After b, the Queue goes back to a */
        QTRY_COMPARE_WITH_TIMEOUT(currentIndex(), 1, 4000);
        QTRY_COMPARE_WITH_TIMEOUT(currentIndex(), 0, 4000);
        QTRY_VERIFY(isPlaying());
    }

    // Persistence

    void testQueueIsRestoredAfterRestart()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        fillQueue();
        {
            QueueStore store(m_controller.get(), file);
            store.save();
        }

        /* restart */
        cleanup();
        init();
        QueueStore store(m_controller.get(), file);

        QVERIFY(store.restore());

        QCOMPARE(queueNames(), QStringList({"a", "b", "c"}));
        QCOMPARE(currentIndex(), 1);
        QVERIFY(!isPlaying());
    }

    void testRestoreKeepsMrlButNotInputOptions()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        m_controller->append(QVector<Media>{Media(LONG_MRL, "a", {":no-video"})}, false);
        QueueStore(m_controller.get(), file).save();
        cleanup();
        init();

        QVERIFY(QueueStore(m_controller.get(), file).restore());

        vlc_playlist_locker locker(playlist());
        QCOMPARE(vlc_playlist_Count(playlist()), size_t{1});
        input_item_t *media = vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist(), 0));
        QCOMPARE(qfu(media->psz_uri), LONG_MRL);
        QCOMPARE(media->i_options, 0);
    }

    void testCredentialsAreNotSaved()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        const QString mrl = QStringLiteral("http://user:secret@example.com/a.mp4");
        m_controller->append(QVector<Media>{Media(mrl, mrl)}, false);

        QueueStore(m_controller.get(), file).save();

        QFile saved(file);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QByteArray content = saved.readAll();
        QVERIFY(content.contains("example.com/a.mp4"));
        QVERIFY(!content.contains("secret"));
        QVERIFY(!content.contains("user"));
    }

    void testRestoreValidatesTheFile()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        {
            QFile crafted(file);
            QVERIFY(crafted.open(QIODevice::WriteOnly));
            crafted.write("{\"version\": 1, \"current\": 4, \"items\": ["
                          "{\"mrl\": 42},"
                          "{\"mrl\": \"relative/path.mkv\"},"
                          "{\"mrl\": \"file:///a.mkv\", \"name\": \"a\", \"options\": [\":sout=#file{dst=x}\"]},"
                          "{\"name\": \"no mrl\"},"
                          "{\"mrl\": \"http://user:secret@example.com/b.mp4\", \"name\": \"b\", \"duration\": -5},"
                          "{\"mrl\": \"file:///c.mkv\", \"name\": [\"not\", \"a\", \"string\"]}"
                          "]}");
        }

        QVERIFY(QueueStore(m_controller.get(), file).restore());

        vlc_playlist_locker locker(playlist());
        QCOMPARE(vlc_playlist_Count(playlist()), size_t{3});
        input_item_t *a = vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist(), 0));
        QCOMPARE(qfu(a->psz_uri), QStringLiteral("file:///a.mkv"));
        QCOMPARE(a->i_options, 0);
        input_item_t *b = vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist(), 1));
        QCOMPARE(qfu(b->psz_uri), QStringLiteral("http://example.com/b.mp4"));
        QVERIFY(b->i_duration <= 0);
        input_item_t *c = vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist(), 2));
        QCOMPARE(qfu(c->psz_uri), QStringLiteral("file:///c.mkv"));
        /* the saved current item (b) follows the skipped entries */
        QCOMPARE(vlc_playlist_GetCurrentIndex(playlist()), ssize_t{1});
    }

    void testRestoreRejectsMalformedFile()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        {
            QFile crafted(file);
            QVERIFY(crafted.open(QIODevice::WriteOnly));
            crafted.write("[{\"mrl\": \"file:///a.mkv\"}]");
        }

        QVERIFY(!QueueStore(m_controller.get(), file).restore());
        QCOMPARE(queueNames(), QStringList());
    }

    void testNothingIsKeptWhenHistoryIsOff()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        fillQueue();
        QueueStore(m_controller.get(), file).save();
        QVERIFY(QFile::exists(file));

        QueueStore store(m_controller.get(), file);
        store.setEnabled(false);

        /* the saved Queue is forgotten, and not written again */
        QVERIFY(!QFile::exists(file));
        store.save();
        QVERIFY(!QFile::exists(file));
        cleanup();
        init();
        QueueStore restoring(m_controller.get(), file);
        restoring.setEnabled(false);
        QVERIFY(!restoring.restore());
    }

    void testQueueIsSavedWhenItChanges()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        {
            QueueStore store(m_controller.get(), file);
            store.setSaveDelay(std::chrono::milliseconds(10));
            fillQueue();
            m_controller->goTo(2);
            QTRY_VERIFY(QFile::exists(file));
            /* let the last change (the current item) reach the file */
            QTest::qWait(100);
        }
        cleanup();
        init();

        QVERIFY(QueueStore(m_controller.get(), file).restore());

        QCOMPARE(queueNames(), QStringList({"a", "b", "c"}));
        QCOMPARE(currentIndex(), 2);
    }

    void testPendingChangesAreSavedOnExit()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        {
            QueueStore store(m_controller.get(), file);
            fillQueue();
            /* let the controller see the changes */
            QTRY_COMPARE(m_controller->count(), 3);
            QTRY_COMPARE(m_controller->currentIndex(), 1);
        }
        cleanup();
        init();

        QVERIFY(QueueStore(m_controller.get(), file).restore());

        QCOMPARE(queueNames(), QStringList({"a", "b", "c"}));
        QCOMPARE(currentIndex(), 1);
    }

    void testEmptyQueueIsRestoredEmpty()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        fillQueue();
        QueueStore(m_controller.get(), file).save();
        m_controller->clear();
        QueueStore(m_controller.get(), file).save();

        QVERIFY(!QueueStore(m_controller.get(), file).restore());
        QCOMPARE(queueNames(), QStringList());
    }

    void testRestoreDoesNotReplaceItemsAlreadyQueued()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("queue.json");
        fillQueue();
        QueueStore(m_controller.get(), file).save();
        cleanup();
        init();
        /* e.g. files given on the command line */
        m_controller->append(medias({"x"}), false);

        QVERIFY(!QueueStore(m_controller.get(), file).restore());

        QCOMPARE(queueNames(), QStringList({"x"}));
    }

    void testRestoreWithoutFile()
    {
        QTemporaryDir dir;

        QVERIFY(!QueueStore(m_controller.get(), dir.filePath("queue.json")).restore());

        QCOMPARE(queueNames(), QStringList());
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<PlaylistController> m_controller;
};

QTEST_GUILESS_MAIN(TestQueue)
#include "test_queue.moc"
