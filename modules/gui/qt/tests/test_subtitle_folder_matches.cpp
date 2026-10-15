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

/* Add Subtitles To This Folder / To This Playlist, From Computer: matching
 * the subtitle files of a folder or a .zip to every episode, the language
 * filter, the ⇆ overrides, saving the Subtitle matches, and the episodes
 * added later matched when they play. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_stub_modules.hpp"

#include <vlc_configuration.h>
#include <vlc_url.h>

#include "../dialogs/subtitles/subtitle_match_model.hpp"
#include "../dialogs/subtitles/subtitle_matching.hpp"
#include "../medialibrary/medialib.hpp"
#include "../medialibrary/mlqmltypes.hpp"
#include "../playlist/playlist_controller.hpp"

#include <memory>

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <QtCore/private/qzipwriter_p.h>

using vlc::playlist::PlaylistController;

static QString toMrl(const QString &path)
{
    char *uri = vlc_path2uri(qtu(QDir::toNativeSeparators(path)), "file");
    QString mrl = qfu(uri);
    free(uri);
    return mrl;
}

class TestSubtitleFolderMatches : public QObject
{
    Q_OBJECT

private:
    /* an empty file in the test folder */
    QString touch(const QString &relativePath)
    {
        const QString path = m_dir->filePath(relativePath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write("1\n00:00:00,000 --> 00:10:00,000\na line\n\n");
        return path;
    }

    QString touchAll(const QStringList &relativePaths)
    {
        for (const QString &path : relativePaths)
            touch(path);
        return m_dir->path();
    }

    /* episode name -> matched subtitle names */
    QStringList matches(const SubtitleMatchModel &model) const
    {
        QStringList list;
        for (int i = 0; i < model.rowCount(); ++i)
        {
            const QModelIndex index = model.index(i);
            list.push_back(model.data(index, SubtitleMatchModel::NameRole).toString()
                           + " -> "
                           + model.data(index, SubtitleMatchModel::MatchRole).toString());
        }
        return list;
    }

    int rowOf(const SubtitleMatchModel &model, const QString &name) const
    {
        for (int i = 0; i < model.rowCount(); ++i)
            if (model.data(model.index(i), SubtitleMatchModel::NameRole).toString() == name)
                return i;
        return -1;
    }

    QStringList candidateNames(const SubtitleMatchModel &model, int row) const
    {
        QStringList list;
        for (const QVariant &candidate : model.candidates(row))
            list.push_back(candidate.toMap().value("name").toString());
        return list;
    }

    int candidateId(const SubtitleMatchModel &model, int row, const QString &name) const
    {
        for (const QVariant &candidate : model.candidates(row))
            if (candidate.toMap().value("name").toString() == name)
                return candidate.toMap().value("id").toInt();
        return -2;
    }

    QStringList savedMatches(const QString &mediaMrl) const
    {
        QStringList list;
        for (const std::string &mrl : m_env->subtitleMatches(qtu(mediaMrl)))
            list.push_back(qfu(mrl.c_str()));
        return list;
    }

    QString writeZip(const QString &relativePath, const QStringList &entries)
    {
        const QString path = m_dir->filePath(relativePath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        {
            QZipWriter zip(path);
            for (const QString &entry : entries)
                zip.addFile(entry, QByteArray("1\n00:00:00,000 --> 00:10:00,000\n") + qtu(entry) + "\n\n");
            zip.close();
        }

        /* QZipWriter cleans the names: name an entry "a/../../evil…" by hand,
         * out of the folder of the .zip */
        QFile file(path);
        if (!file.open(QIODevice::ReadWrite))
            return {};
        QByteArray data = file.readAll();
        data.replace("a/zz/zz/evil", "a/../../evil");
        file.seek(0);
        file.write(data);
        return path;
    }

    std::unique_ptr<SubtitleMatchModel> newModel()
    {
        return std::make_unique<SubtitleMatchModel>(m_env->intf, m_medialib.get());
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
        config_PutPsz("sub-language", "");
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<QSettings>(m_dir->filePath("vlc-qt-interface.conf"),
                                                 QSettings::IniFormat);
        m_env->intf->mainSettings = m_settings.get();
        m_controller = std::make_unique<PlaylistController>(m_env->intf->p_playlist);
        QTRY_VERIFY(m_controller->isInitialized());
        m_medialib = std::make_unique<MediaLib>(m_env->intf, m_controller.get());
    }

    void cleanup()
    {
        m_medialib.reset();
        m_controller.reset();
        m_env->intf->mainSettings = nullptr;
        m_settings.reset();
        m_dir.reset();
    }

    // Matching

    void testMatchesByEpisodeNumber()
    {
        const QString show = touchAll({
            "The Wand S01E01.mkv",
            "The Wand S01E02.mkv",
            "Season 2/The Wand 2x01.avi",
            "The Wand S01E03.mkv",
            "notes.txt",
            "Subs/the.wand.s01e02.720p.srt",
            "Subs/the.wand.s01e01.720p.srt",
            "Subs/The Wand S02E01.ass",
        });

        auto model = newModel();
        model->setFolder(toMrl(show));

        QCOMPARE(model->target(), QDir(show).dirName());
        QVERIFY(!model->isPlaylist());
        QCOMPARE(matches(*model), QStringList({
            "The Wand S01E01.mkv -> the.wand.s01e01.720p.srt",
            "The Wand S01E02.mkv -> the.wand.s01e02.720p.srt",
            "The Wand S01E03.mkv -> ",
            "The Wand 2x01.avi -> The Wand S02E01.ass",
        }));
        QCOMPARE(model->data(model->index(3), SubtitleMatchModel::PathRole).toString(),
                 "Season 2/The Wand 2x01.avi");
        QVERIFY(!model->data(model->index(2), SubtitleMatchModel::MatchedRole).toBool());
        QCOMPARE(model->matchedCount(), 3);
        /* one language at most: no filter */
        QCOMPARE(model->languages(), QStringList());
    }

    void testEpisodeNumberFormats()
    {
        const QString show = touchAll({
            "Alpha.E04.mkv",
            "Bravo 1x05.mkv",
            "Charlie - Episode 6.mp4",
            "Delta 1920x1080.mkv",
            "subs/Alpha - 04.srt",
            "subs/Alpha E04.srt",
            "subs/bravo.s01e05.srt",
            "subs/charlie ep06.srt",
            "subs/charlie ep07.srt",
            "subs/delta.srt",
        });

        auto model = newModel();
        /* a path works as well as an MRL */
        model->setFolder(show);

        /* the episode number wins over a closer name */
        QCOMPARE(matches(*model), QStringList({
            "Alpha.E04.mkv -> Alpha E04.srt",
            "Bravo 1x05.mkv -> bravo.s01e05.srt",
            "Charlie - Episode 6.mp4 -> charlie ep06.srt",
            /* a resolution isn't an episode number */
            "Delta 1920x1080.mkv -> delta.srt",
        }));
    }

    void testMatchesByNameSimilarityWithoutNumbers()
    {
        const QString show = touchAll({
            "The Long Night.mkv",
            "A Short Day.mkv",
            "Nothing Alike.mkv",
            "the_long_night.srt",
            "a.short.day.srt",
            "zzz.srt",
        });

        auto model = newModel();
        model->setFolder(show);

        QCOMPARE(matches(*model), QStringList({
            "A Short Day.mkv -> a.short.day.srt",
            "Nothing Alike.mkv -> ",
            "The Long Night.mkv -> the_long_night.srt",
        }));
    }

    // ⇆

    void testCandidatesBestFirstAndOverride()
    {
        const QString show = touchAll({
            "Show S01E01.mkv",
            "Show S01E02.mkv",
            "subs/Show S01E02.srt",
            "subs/Show S01E01.srt",
            "subs/Show S01E01 alt.srt",
            "subs/Other S03E07.srt",
        });

        auto model = newModel();
        model->setFolder(show);
        QCOMPARE(matches(*model), QStringList({
            "Show S01E01.mkv -> Show S01E01.srt",
            "Show S01E02.mkv -> Show S01E02.srt",
        }));

        /* every file, the same episode first, then by name */
        QCOMPARE(candidateNames(*model, 0), QStringList({
            "Show S01E01.srt", "Show S01E01 alt.srt", "Show S01E02.srt", "Other S03E07.srt",
        }));
        QVariantMap first = model->candidates(0).first().toMap();
        QVERIFY(first.value("current").toBool());
        QCOMPARE(first.value("path").toString(), "subs/Show S01E01.srt");
        QVERIFY(!model->candidates(0).at(1).toMap().value("current").toBool());

        QSignalSpy matched(model.get(), &SubtitleMatchModel::matchedCountChanged);
        QSignalSpy changed(model.get(), &QAbstractItemModel::dataChanged);

        /* pick another one */
        model->setMatch(0, candidateId(*model, 0, "Show S01E01 alt.srt"));
        QCOMPARE(matches(*model).at(0), "Show S01E01.mkv -> Show S01E01 alt.srt");
        QVERIFY(model->data(model->index(0), SubtitleMatchModel::OverriddenRole).toBool());
        QVERIFY(model->candidates(0).at(1).toMap().value("current").toBool());
        QCOMPARE(changed.count(), 1);

        /* None */
        model->setMatch(1, -1);
        QCOMPARE(matches(*model).at(1), "Show S01E02.mkv -> ");
        QVERIFY(!model->data(model->index(1), SubtitleMatchModel::MatchedRole).toBool());
        QVERIFY(model->data(model->index(1), SubtitleMatchModel::OverriddenRole).toBool());
        QCOMPARE(model->matchedCount(), 1);
        QVERIFY(matched.count() >= 1);

        /* a wrong row or id changes nothing */
        model->setMatch(5, -1);
        model->setMatch(0, 42);
        QCOMPARE(matches(*model).at(0), "Show S01E01.mkv -> Show S01E01 alt.srt");
    }

    // Language filter

    void testLanguageFilterStartsOnThePreferredLanguage()
    {
        config_PutPsz("sub-language", "fre,eng");
        const QString show = touchAll({
            "Show S01E01.mkv",
            "Show S01E02.mkv",
            "Show S01E03.mkv",
            "subs/Show S01E01.en.srt",
            "subs/Show S01E01.fr.srt",
            "subs/Show S01E02.English.srt",
            "subs/French/Show S01E02.srt",
            "subs/Show S01E03.eng.srt",
        });

        auto model = newModel();
        model->setFolder(show);

        QCOMPARE(model->languages(), QStringList({ "English", "French", "All" }));
        QCOMPARE(model->languageIndex(), 1);
        /* an episode without the language has no match */
        QCOMPARE(matches(*model), QStringList({
            "Show S01E01.mkv -> Show S01E01.fr.srt",
            "Show S01E02.mkv -> Show S01E02.srt",
            "Show S01E03.mkv -> ",
        }));

        /* the files of the language come first in ⇆ */
        QCOMPARE(candidateNames(*model, 0).mid(0, 2),
                 QStringList({ "Show S01E01.fr.srt", "Show S01E01.en.srt" }));
        QCOMPARE(model->candidates(0).first().toMap().value("language").toString(), "French");

        /* an override stays when the language changes */
        model->setMatch(1, -1);
        QSignalSpy spy(model.get(), &SubtitleMatchModel::languageIndexChanged);
        model->setLanguageIndex(0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(matches(*model), QStringList({
            "Show S01E01.mkv -> Show S01E01.en.srt",
            "Show S01E02.mkv -> ",
            "Show S01E03.mkv -> Show S01E03.eng.srt",
        }));

        /* All: every language */
        model->setLanguageIndex(2);
        QCOMPARE(matches(*model), QStringList({
            "Show S01E01.mkv -> Show S01E01.en.srt, Show S01E01.fr.srt",
            "Show S01E02.mkv -> ",
            "Show S01E03.mkv -> Show S01E03.eng.srt",
        }));
    }

    void testLanguageFilterWithoutThePreferredLanguage()
    {
        config_PutPsz("sub-language", "ger");
        const QString show = touchAll({
            "Show S01E01.mkv",
            "subs/Show S01E01.es.srt",
            "subs/Show S01E01.it.srt",
        });

        auto model = newModel();
        model->setFolder(show);

        /* the first language */
        QCOMPARE(model->languages(), QStringList({ "Italian", "Spanish", "All" }));
        QCOMPARE(model->languageIndex(), 0);
        QCOMPARE(matches(*model), QStringList({ "Show S01E01.mkv -> Show S01E01.it.srt" }));
    }

    // Sources

    void testSourceIsAnotherFolder()
    {
        touchAll({
            "Show/Show S01E01.mkv",
            "Show/Show S01E02.mkv",
            "Downloads/subs/show.s01e02.srt",
            "Downloads/subs/show.s01e01.srt",
        });

        auto model = newModel();
        model->setFolder(m_dir->filePath("Show"));
        QVERIFY(model->source().endsWith(" / Show"));
        QCOMPARE(model->matchedCount(), 0);

        QSignalSpy spy(model.get(), &SubtitleMatchModel::sourceChanged);
        model->setSource(QUrl::fromLocalFile(m_dir->filePath("Downloads")));
        QCOMPARE(spy.count(), 1);
        QVERIFY(model->source().endsWith(" / Downloads"));
        QVERIFY(!model->sourceIsZip());
        QCOMPARE(matches(*model), QStringList({
            "Show S01E01.mkv -> show.s01e01.srt",
            "Show S01E02.mkv -> show.s01e02.srt",
        }));

        /* back to the Show's folder */
        model->resetSource();
        QVERIFY(model->source().endsWith(" / Show"));
        QCOMPARE(model->matchedCount(), 0);
    }

    void testSourceIsAZip()
    {
        touchAll({ "Show/Show S01E01.mkv", "Show/Show S01E02.mkv" });
        const QString zip = writeZip("Downloads/Show.Season.1.zip", {
            "Show S01E02.srt",
            "Show.S01E01/Show S01E01.srt",
            "readme.txt",
            "a/zz/zz/evil S01E01.srt",
        });

        auto model = newModel();
        model->setFolder(m_dir->filePath("Show"));
        model->setSource(QUrl::fromLocalFile(zip));
        QVERIFY(model->sourceIsZip());
        QVERIFY(model->source().endsWith(" / Show.Season.1.zip"));

        /* a file outside the .zip's folder is ignored */
        QCOMPARE(matches(*model), QStringList({
            "Show S01E01.mkv -> Show S01E01.srt",
            "Show S01E02.mkv -> Show S01E02.srt",
        }));
        QCOMPARE(candidateNames(*model, 0), QStringList({ "Show S01E01.srt", "Show S01E02.srt" }));

        /* Save extracts the matched files next to the .zip */
        model->save();
        const QString extracted = m_dir->filePath("Downloads/Show.Season.1/Show.S01E01/Show S01E01.srt");
        QTRY_COMPARE(savedMatches(toMrl(m_dir->filePath("Show/Show S01E01.mkv"))),
                     QStringList({ toMrl(extracted) }));
        QFile file(extracted);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("Show.S01E01/Show S01E01.srt"));
        QVERIFY(!QFile::exists(m_dir->filePath("Downloads/evil S01E01.srt")));
        QVERIFY(!QFile::exists(m_dir->filePath("evil S01E01.srt")));
    }

    // Save

    void testSaveStoresTheMatches()
    {
        const QString show = touchAll({
            "Show S01E01.mkv",
            "Show S01E02.mkv",
            "Show S01E03.mkv",
            "subs/Show S01E01.srt",
            "subs/Show S01E02.srt",
        });

        auto model = newModel();
        model->setFolder(show);
        model->setMatch(1, -1);
        model->save();

        QTRY_COMPARE(savedMatches(toMrl(m_dir->filePath("Show S01E01.mkv"))),
                     QStringList({ toMrl(m_dir->filePath("subs/Show S01E01.srt")) }));
        QCOMPARE(savedMatches(toMrl(m_dir->filePath("Show S01E02.mkv"))), QStringList());
        QCOMPARE(savedMatches(toMrl(m_dir->filePath("Show S01E03.mkv"))), QStringList());
    }

    // The Show's source

    void testTheShowRemembersItsSource()
    {
        touchAll({
            "Show/Show S01E01.mkv",
            "Show/Show S01E02.mkv",
            "Show/Season 2/Show S02E01.mkv",
            "Downloads/Show S01E01.srt",
            "Downloads/Show S01E02.srt",
            "Downloads/Show S01E03.srt",
        });

        auto model = newModel();
        model->setFolder(m_dir->filePath("Show"));
        model->setSource(QUrl::fromLocalFile(m_dir->filePath("Downloads")));
        model->setMatch(1, -1);
        model->save();
        QTRY_COMPARE(savedMatches(toMrl(m_dir->filePath("Show/Show S01E01.mkv"))).size(), 1);

        /* the dialog opens on it again */
        auto again = newModel();
        again->setFolder(m_dir->filePath("Show"));
        QVERIFY(again->source().endsWith(" / Downloads"));
        QCOMPARE(again->matchedCount(), 2);

        ShowSubtitleMatcher matcher(m_env->intf, m_medialib.get(), m_settings.get());
        QSignalSpy spy(&matcher, &ShowSubtitleMatcher::matched);

        /* an episode added later is matched when it plays */
        touch("Show/Show S01E03.mkv");
        const QString later = toMrl(m_dir->filePath("Show/Show S01E03.mkv"));
        matcher.matchOnPlay(later);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), later);
        QTRY_COMPARE(savedMatches(later),
                     QStringList({ toMrl(m_dir->filePath("Downloads/Show S01E03.srt")) }));

        /* not the episodes the dialog listed, even set to None */
        matcher.matchOnPlay(toMrl(m_dir->filePath("Show/Show S01E02.mkv")));
        matcher.matchOnPlay(toMrl(m_dir->filePath("Show/Season 2/Show S02E01.mkv")));
        /* nor a file outside the Show */
        touch("Elsewhere/Show S01E01.mkv");
        matcher.matchOnPlay(toMrl(m_dir->filePath("Elsewhere/Show S01E01.mkv")));
        /* and once only */
        m_medialib->removeSubtitleMatch(later, savedMatches(later).first());
        QTRY_COMPARE(savedMatches(later), QStringList());
        matcher.matchOnPlay(later);

        QTest::qWait(200);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(savedMatches(toMrl(m_dir->filePath("Show/Show S01E02.mkv"))), QStringList());
        QCOMPARE(savedMatches(later), QStringList());
    }

    void testLaterEpisodesUseTheShowsLanguage()
    {
        touchAll({
            "Show/Show S01E01.mkv",
            "Show/subs/Show S01E01.en.srt",
            "Show/subs/Show S01E01.fr.srt",
        });

        auto model = newModel();
        model->setFolder(m_dir->filePath("Show"));
        QCOMPARE(model->languages(), QStringList({ "English", "French", "All" }));
        model->setLanguageIndex(1);
        model->save();
        QTRY_COMPARE(savedMatches(toMrl(m_dir->filePath("Show/Show S01E01.mkv"))).size(), 1);

        touchAll({
            "Show/Show S01E02.mkv",
            "Show/subs/Show S01E02.en.srt",
            "Show/subs/Show S01E02.fr.srt",
        });
        ShowSubtitleMatcher matcher(m_env->intf, m_medialib.get(), m_settings.get());
        const QString later = toMrl(m_dir->filePath("Show/Show S01E02.mkv"));
        matcher.matchOnPlay(later);
        QTRY_COMPARE(savedMatches(later),
                     QStringList({ toMrl(m_dir->filePath("Show/subs/Show S01E02.fr.srt")) }));
    }

    // To This Playlist

    void testPlaylistItemsAcrossFolders()
    {
        touchAll({
            "A/Show S01E01.mkv",
            "B/Show S01E02.mkv",
            "A/Show S01E01.srt",
            "A/Show S01E02.srt",
        });
        const QString first = toMrl(m_dir->filePath("A/Show S01E01.mkv"));
        const QString second = toMrl(m_dir->filePath("B/Show S01E02.mkv"));

        vlc_medialibrary_t *ml = vlc_ml_instance_get(m_env->intf);
        QVERIFY(ml);
        int64_t ids[2];
        for (int i = 0; i < 2; ++i)
        {
            vlc_ml_media_t *media = vlc_ml_new_external_media(ml, qtu(i == 0 ? first : second));
            QVERIFY(media);
            ids[i] = media->i_id;
            vlc_ml_media_release(media);
        }
        const int64_t playlist = m_env->addPlaylist("Binge", { ids[0], ids[1] });

        auto model = newModel();
        model->loadPlaylist(MLItemId(playlist, VLC_ML_PARENT_PLAYLIST), "Binge");
        QTRY_COMPARE(model->count(), 2);
        QVERIFY(model->isPlaylist());
        QCOMPARE(model->target(), "Binge");

        /* in the Playlist's order, matched from the folder of the first one */
        QVERIFY(model->source().endsWith(" / A"));
        QCOMPARE(matches(*model), QStringList({
            "Show S01E01.mkv -> Show S01E01.srt",
            "Show S01E02.mkv -> Show S01E02.srt",
        }));

        model->save();
        QTRY_COMPARE(savedMatches(second), QStringList({ toMrl(m_dir->filePath("A/Show S01E02.srt")) }));
        QCOMPARE(savedMatches(first), QStringList({ toMrl(m_dir->filePath("A/Show S01E01.srt")) }));

        /* a Playlist isn't a Show: nothing to remember */
        QVERIFY(!m_settings->childGroups().contains("SubtitleSources"));
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<PlaylistController> m_controller;
    std::unique_ptr<MediaLib> m_medialib;
};

QTEST_GUILESS_MAIN(TestSubtitleFolderMatches)
#include "test_subtitle_folder_matches.moc"
