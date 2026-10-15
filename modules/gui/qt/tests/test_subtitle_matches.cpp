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

/* Add Subtitles to a file and the Subtitle matches: the files listed for a
 * video, Save attaching them and remembering them, the playback hook loading
 * the remembered ones, and removing an external track forgetting its match. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_stub_modules.hpp"

#include <vlc_url.h>

#include "../dialogs/subtitles/add_subtitles_model.hpp"
#include "../medialibrary/medialib.hpp"
#include "../player/input_models.hpp"
#include "../player/player_controller.hpp"
#include "../playlist/playlist_controller.hpp"

#include <memory>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using vlc::playlist::PlaylistController;

/* Feeds a TrackListModel from the player, on the Qt thread, like
 * PlayerController does */
class TrackFeeder
{
public:
    TrackFeeder(vlc_player_t *player, TrackListModel *model, es_format_category_e cat = SPU_ES)
        : m_player(player), m_model(model), m_cat(cat)
    {
        static const struct vlc_player_cbs cbs = [] {
            struct vlc_player_cbs c{};
            c.on_track_list_changed = onTrackListChanged;
            c.on_track_selection_changed = onTrackSelectionChanged;
            return c;
        }();
        vlc_player_locker locker{ m_player };
        m_listener = vlc_player_AddListener(m_player, &cbs, this);
    }

    ~TrackFeeder()
    {
        vlc_player_locker locker{ m_player };
        vlc_player_RemoveListener(m_player, m_listener);
    }

private:
    static void onTrackListChanged(vlc_player_t *, enum vlc_player_list_action action,
                                   const struct vlc_player_track *track, void *data)
    {
        auto that = static_cast<TrackFeeder *>(data);
        if (track->fmt.i_cat != that->m_cat)
            return;
        std::shared_ptr<vlc_player_track> dup{ vlc_player_track_Dup(track),
                                               vlc_player_track_Delete };
        TrackListModel *model = that->m_model;
        QMetaObject::invokeMethod(model, [model, action, dup]() {
            model->updateTracks(action, dup.get());
        }, Qt::QueuedConnection);
    }

    static void onTrackSelectionChanged(vlc_player_t *, vlc_es_id_t *unselected,
                                        vlc_es_id_t *selected, void *data)
    {
        auto that = static_cast<TrackFeeder *>(data);
        TrackListModel *model = that->m_model;
        using EsId = std::shared_ptr<vlc_es_id_t>;
        auto hold = [](vlc_es_id_t *id) {
            return id ? EsId{ vlc_es_id_Hold(id), vlc_es_id_Release } : EsId{};
        };
        EsId off = hold(unselected), on = hold(selected);
        QMetaObject::invokeMethod(model, [model, off, on]() {
            model->updateTrackSelection(off.get(), false);
            model->updateTrackSelection(on.get(), true);
        }, Qt::QueuedConnection);
    }

    vlc_player_t *m_player;
    TrackListModel *m_model;
    vlc_player_listener_id *m_listener = nullptr;
    es_format_category_e m_cat;
};

static QString toMrl(const QString &path)
{
    char *uri = vlc_path2uri(qtu(QDir::toNativeSeparators(path)), "file");
    QString mrl = qfu(uri);
    free(uri);
    return mrl;
}

class TestSubtitleMatches : public QObject
{
    Q_OBJECT

private:
    vlc_player_t *player() const
    {
        return m_env->intf->p_player;
    }

    /* a subtitle file in the test folder */
    QString writeSubtitle(const QString &relativePath)
    {
        const QString path = m_dir->filePath(relativePath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile srt(path);
        if (!srt.open(QIODevice::WriteOnly))
            return {};
        srt.write("1\n00:00:00,000 --> 00:10:00,000\na line\n\n");
        srt.close();
        return path;
    }

    void play(const QString &mrl)
    {
        input_item_t *media = input_item_New(qtu(mrl), "media");
        QVERIFY(media);
        {
            vlc_player_locker locker{ player() };
            QCOMPARE(vlc_player_SetCurrentMedia(player(), media), VLC_SUCCESS);
            QCOMPARE(vlc_player_Start(player()), VLC_SUCCESS);
        }
        input_item_Release(media);
    }

    void stop()
    {
        {
            vlc_player_locker locker{ player() };
            vlc_player_SetCurrentMedia(player(), nullptr);
        }
        QTRY_VERIFY(!hasMedia());
        QTRY_COMPARE(m_tracks->getCount(), 0);
        QTRY_COMPARE(m_audioTracks->getCount(), 0);
    }

    /* stop and play the same media again, as the Queue does */
    void restart()
    {
        {
            vlc_player_locker locker{ player() };
            vlc_player_Stop(player());
        }
        QTRY_COMPARE(m_tracks->getCount(), 0);
        QTRY_COMPARE(m_audioTracks->getCount(), 0);
        QTRY_VERIFY(!isStarted());
        {
            vlc_player_locker locker{ player() };
            QCOMPARE(vlc_player_Start(player()), VLC_SUCCESS);
        }
    }

    bool isStarted() const
    {
        vlc_player_locker locker{ player() };
        return vlc_player_IsStarted(player());
    }

    bool hasMedia() const
    {
        vlc_player_locker locker{ player() };
        return vlc_player_GetCurrentMedia(player()) != nullptr
            || vlc_player_IsStarted(player());
    }


    bool trackChecked(int row) const
    {
        return m_tracks->data(m_tracks->index(row), Qt::CheckStateRole).toBool();
    }

    bool trackExternal(int row) const
    {
        return m_tracks->data(m_tracks->index(row), TrackListModel::ExternalRole).toBool();
    }

    int externalCount() const
    {
        int count = 0;
        for (int i = 0; i < m_tracks->rowCount(); ++i)
            if (trackExternal(i))
                ++count;
        return count;
    }

    /* the row of the first external track, in the order they were added */
    int firstExternal() const
    {
        for (int i = 0; i < m_tracks->rowCount(); ++i)
            if (trackExternal(i))
                return i;
        return -1;
    }

    /* the rows of the external tracks that are shown */
    QList<int> shownExternal() const
    {
        QList<int> rows;
        for (int i = 0; i < m_tracks->rowCount(); ++i)
            if (trackExternal(i) && trackChecked(i))
                rows.push_back(i);
        return rows;
    }

    QStringList names(const AddSubtitlesModel &model) const
    {
        QStringList list;
        for (int i = 0; i < model.rowCount(); ++i)
            list.push_back(model.data(model.index(i), AddSubtitlesModel::NameRole).toString());
        return list;
    }

    QStringList checkedNames(const AddSubtitlesModel &model) const
    {
        QStringList list;
        for (int i = 0; i < model.rowCount(); ++i)
            if (model.data(model.index(i), AddSubtitlesModel::CheckedRole).toBool())
                list.push_back(model.data(model.index(i), AddSubtitlesModel::NameRole).toString());
        return list;
    }

    QStringList savedMatches(const QString &mediaMrl) const
    {
        QStringList list;
        for (const std::string &mrl : m_env->subtitleMatches(qtu(mediaMrl)))
            list.push_back(qfu(mrl.c_str()));
        return list;
    }

    /* a WAV file of silence in the test folder, long enough to stay
     * listed while a test runs */
    QString writeWav(const QString &relativePath)
    {
        const QString path = m_dir->filePath(relativePath);
        QFile wav(path);
        if (!wav.open(QIODevice::WriteOnly))
            return {};
        const quint32 rate = 8000;
        const quint32 size = rate * 30; /* 30 s, 8-bit mono */
        auto le32 = [&wav](quint32 v) {
            const char b[4] = { char(v), char(v >> 8), char(v >> 16), char(v >> 24) };
            wav.write(b, 4);
        };
        auto le16 = [&wav](quint16 v) {
            const char b[2] = { char(v), char(v >> 8) };
            wav.write(b, 2);
        };
        wav.write("RIFF", 4); le32(36 + size); wav.write("WAVE", 4);
        wav.write("fmt ", 4); le32(16); le16(1); le16(1); le32(rate); le32(rate); le16(1); le16(8);
        wav.write("data", 4); le32(size);
        wav.write(QByteArray(size, char(0x80)));
        wav.close();
        return path;
    }

    int audioExternalCount() const
    {
        int count = 0;
        for (int i = 0; i < m_audioTracks->rowCount(); ++i)
            if (m_audioTracks->data(m_audioTracks->index(i), TrackListModel::ExternalRole).toBool())
                ++count;
        return count;
    }

    bool audioExternalShown() const
    {
        for (int i = 0; i < m_audioTracks->rowCount(); ++i)
            if (m_audioTracks->data(m_audioTracks->index(i), TrackListModel::ExternalRole).toBool()
             && m_audioTracks->data(m_audioTracks->index(i), Qt::CheckStateRole).toBool())
                return true;
        return false;
    }

    QStringList savedAudioTracks(const QString &mediaMrl) const
    {
        QStringList list;
        for (const std::string &mrl : m_env->audioTrackMatches(qtu(mediaMrl)))
            list.push_back(qfu(mrl.c_str()));
        return list;
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
        m_env->clearMediaLibrary();
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_controller = std::make_unique<PlaylistController>(m_env->intf->p_playlist);
        QTRY_VERIFY(m_controller->isInitialized());
        m_medialib = std::make_unique<MediaLib>(m_env->intf, m_controller.get());
        m_tracks = std::make_unique<TrackListModel>(player());
        m_feeder = std::make_unique<TrackFeeder>(player(), m_tracks.get());
        m_audioTracks = std::make_unique<TrackListModel>(player());
        m_audioFeeder = std::make_unique<TrackFeeder>(player(), m_audioTracks.get(), AUDIO_ES);
    }

    void cleanup()
    {
        stop();
        m_audioFeeder.reset();
        m_audioTracks.reset();
        m_feeder.reset();
        m_tracks.reset();
        m_medialib.reset();
        m_controller.reset();
        m_dir.reset();
    }

    // To This File, From Computer

    void testListsSubtitlesNextToTheVideoAndInSubs()
    {
        const QString video = m_dir->filePath("The Wand S01E01.mkv");
        QFile(video).open(QIODevice::WriteOnly);
        writeSubtitle("The Wand S01E01.French.srt");
        writeSubtitle("The Wand S01E01.English.ass");
        writeSubtitle("Subs/The Wand S01E01.Spanish.srt");
        writeSubtitle("Other/The Wand S01E01.German.srt");
        writeSubtitle("notes.txt");
        writeSubtitle("cover.jpg");

        AddSubtitlesModel model(m_env->intf, m_medialib.get(), toMrl(video));

        QCOMPARE(names(model), QStringList({ "The Wand S01E01.English.ass",
                                             "The Wand S01E01.French.srt",
                                             "The Wand S01E01.Spanish.srt" }));
        QCOMPARE(checkedNames(model), QStringList());
        QCOMPARE(model.checkedCount(), 0);
        QCOMPARE(model.data(model.index(2), AddSubtitlesModel::UrlRole).toString(),
                 toMrl(m_dir->filePath("Subs/The Wand S01E01.Spanish.srt")));
        QVERIFY(model.folder().endsWith(" / " + QDir(m_dir->path()).dirName()));
    }

    void testMultiSelectAndBrowse()
    {
        const QString video = m_dir->filePath("movie.mkv");
        QFile(video).open(QIODevice::WriteOnly);
        writeSubtitle("movie.en.srt");
        writeSubtitle("movie.fr.srt");
        const QString elsewhere = writeSubtitle("Downloads/movie.de.srt");

        AddSubtitlesModel model(m_env->intf, m_medialib.get(), toMrl(video));
        QSignalSpy checkedSpy(&model, &AddSubtitlesModel::checkedCountChanged);

        model.toggle(0);
        model.toggle(1);
        QCOMPARE(checkedNames(model), QStringList({ "movie.en.srt", "movie.fr.srt" }));
        QCOMPARE(model.checkedCount(), 2);
        QVERIFY(checkedSpy.count() >= 2);

        model.toggle(0);
        QCOMPARE(checkedNames(model), QStringList({ "movie.fr.srt" }));

        /* Browse…: the picked files are added and checked */
        model.addFiles({ QUrl::fromLocalFile(elsewhere) });
        QCOMPARE(names(model), QStringList({ "movie.en.srt", "movie.fr.srt", "movie.de.srt" }));
        QCOMPARE(checkedNames(model), QStringList({ "movie.fr.srt", "movie.de.srt" }));

        /* a file already listed is checked, not listed twice */
        model.addFiles({ QUrl::fromLocalFile(m_dir->filePath("movie.en.srt")) });
        QCOMPARE(names(model), QStringList({ "movie.en.srt", "movie.fr.srt", "movie.de.srt" }));
        QCOMPARE(model.checkedCount(), 3);
    }

    void testSaveAttachesAndTurnsOnTheFirst()
    {
        /* a media with one embedded subtitle track, nothing shown */
        const QString media = "stubtracks://1";
        play(media);
        QTRY_COMPARE(m_tracks->getCount(), 1);
        QVERIFY(!m_tracks->hasSelection());

        const QString first = writeSubtitle("first.srt");
        const QString second = writeSubtitle("second.srt");

        AddSubtitlesModel model(m_env->intf, m_medialib.get(), media);
        QCOMPARE(model.rowCount(), 0);
        model.addFiles({ QUrl::fromLocalFile(first), QUrl::fromLocalFile(second) });
        model.save();

        QTRY_COMPARE(externalCount(), 2);
        /* the first one turns on */
        QTRY_COMPARE(shownExternal(), QList<int>({ firstExternal() }));

        /* and they are remembered as Subtitle matches */
        QTRY_COMPARE(savedMatches(media), QStringList({ toMrl(first), toMrl(second) }));
    }

    void testSaveKeepsTheShownSubtitle()
    {
        const QString media = "stubtracks://1";
        play(media);
        QTRY_COMPARE(m_tracks->getCount(), 1);
        m_tracks->select(0);
        QTRY_VERIFY(trackChecked(0));

        AddSubtitlesModel model(m_env->intf, m_medialib.get(), media);
        model.addFiles({ QUrl::fromLocalFile(writeSubtitle("other.srt")) });
        model.save();

        QTRY_COMPARE(externalCount(), 1);
        QTRY_COMPARE(savedMatches(media).size(), 1);
        QVERIFY(trackChecked(0));
        QCOMPARE(shownExternal(), QList<int>());
    }

    void testSaveForAMediaNotPlaying()
    {
        const QString video = m_dir->filePath("later.mkv");
        QFile(video).open(QIODevice::WriteOnly);
        writeSubtitle("later.srt");

        AddSubtitlesModel model(m_env->intf, m_medialib.get(), toMrl(video));
        model.toggle(0);
        model.save();

        QTRY_COMPARE(savedMatches(toMrl(video)), QStringList({ toMrl(m_dir->filePath("later.srt")) }));
        QCOMPARE(m_tracks->getCount(), 0);
    }

    // The Subtitle match hook

    void testSavedMatchesLoadWhenTheMediaPlays()
    {
        const QString media = "stubtracks://1";
        const QString english = toMrl(writeSubtitle("english.srt"));
        const QString spanish = toMrl(writeSubtitle("spanish.srt"));

        QSignalSpy spy(m_medialib.get(), &MediaLib::subtitleMatchesChanged);
        m_medialib->addSubtitleMatches(media, { english, spanish });
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(savedMatches(media), QStringList({ english, spanish }));

        play(media);
        QTRY_COMPARE(externalCount(), 2);
        /* the first match shows, like a subtitle found next to the video */
        QTRY_COMPARE(shownExternal(), QList<int>({ firstExternal() }));

        /* playing it again doesn't load them twice */
        restart();
        QTRY_COMPARE(externalCount(), 2);
        QTest::qWait(100);
        QCOMPARE(externalCount(), 2);
    }

    void testRemovingAnExternalTrackForgetsItsMatch()
    {
        const QString media = "stubtracks://1";
        const QString path = writeSubtitle("wrong.srt");
        const QString wrong = toMrl(path);

        QSignalSpy changed(m_medialib.get(), &MediaLib::subtitleMatchesChanged);
        m_medialib->addSubtitleMatches(media, { wrong });
        QTRY_COMPARE(changed.count(), 1);

        play(media);
        QTRY_COMPARE(externalCount(), 1);
        int row = -1;
        for (int i = 0; i < m_tracks->rowCount(); ++i)
            if (trackExternal(i))
                row = i;

        /* the Track menu forgets the match of a removed track */
        QSignalSpy removed(m_tracks.get(), &TrackListModel::externalTrackRemoved);
        QVERIFY(m_tracks->remove(row));
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.first().at(1).toString(), media);
        QCOMPARE(removed.first().at(2).toString(), wrong);

        m_medialib->removeSubtitleMatch(media, wrong);
        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(savedMatches(media), QStringList());

        /* the file stays */
        QVERIFY(QFile::exists(path));

        /* and the track doesn't come back the next time it plays */
        restart();
        QTRY_COMPARE(m_tracks->getCount(), 1);
        QTest::qWait(100);
        QCOMPARE(externalCount(), 0);
    }

    // Add Audio Track, To This File

    void testAddAudioTrackAttachesAndPlaysIt()
    {
        const QString media = "stubtracks://1";
        play(media);
        QTRY_COMPARE(m_tracks->getCount(), 1);

        const QString dub = toMrl(writeWav("dub.wav"));
        attachAudioTracks(m_env->intf, m_medialib.get(), media, { dub });

        /* it plays now, and every time the file plays */
        QTRY_COMPARE(audioExternalCount(), 1);
        QTRY_VERIFY(audioExternalShown());
        QTRY_COMPARE(savedAudioTracks(media), QStringList({ dub }));
        QCOMPARE(savedMatches(media), QStringList());
    }

    void testAddAudioTrackForAMediaNotPlaying()
    {
        const QString video = m_dir->filePath("later.mkv");
        QFile(video).open(QIODevice::WriteOnly);
        const QString dub = toMrl(writeWav("later.wav"));

        attachAudioTracks(m_env->intf, m_medialib.get(), toMrl(video), { dub });

        QTRY_COMPARE(savedAudioTracks(toMrl(video)), QStringList({ dub }));
        QCOMPARE(m_audioTracks->getCount(), 0);
    }

    void testSavedAudioTracksLoadWhenTheMediaPlays()
    {
        const QString media = "stubtracks://1";
        const QString dub = toMrl(writeWav("dub.wav"));
        m_medialib->addAudioTracks(media, { dub });
        QTRY_COMPARE(savedAudioTracks(media), QStringList({ dub }));

        play(media);
        QTRY_COMPARE(audioExternalCount(), 1);

        /* playing it again doesn't load it twice */
        restart();
        QTRY_COMPARE(audioExternalCount(), 1);
        QTest::qWait(100);
        QCOMPARE(audioExternalCount(), 1);
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<PlaylistController> m_controller;
    std::unique_ptr<MediaLib> m_medialib;
    std::unique_ptr<TrackListModel> m_tracks;
    std::unique_ptr<TrackFeeder> m_feeder;
    std::unique_ptr<TrackListModel> m_audioTracks;
    std::unique_ptr<TrackFeeder> m_audioFeeder;
};

QTEST_GUILESS_MAIN(TestSubtitleMatches)
#include "test_subtitle_matches.moc"
