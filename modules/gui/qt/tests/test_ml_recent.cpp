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

#include "../medialibrary/medialib.hpp"
#include "../medialibrary/mlrecentaudiomodel.hpp"
#include "../medialibrary/mlrecentmediamodel.hpp"
#include "../medialibrary/mlrecentvideomodel.hpp"
#include "../playlist/playlist_controller.hpp"

#include <memory>

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using vlc::playlist::PlaylistController;

/* Recent: the play history minus what was removed from Recent or cleared */
class TestMLRecent : public QObject
{
    Q_OBJECT

private:
    static int64_t daysAgo(int days, int seconds = 0)
    {
        return QDateTime::currentDateTime().addDays(-days).toSecsSinceEpoch() - seconds;
    }

    std::unique_ptr<MLRecentMediaModel> newModel(int limit = 0)
    {
        auto model = std::make_unique<MLRecentMediaModel>();
        model->classBegin();
        model->setMl(m_medialib.get());
        if (limit > 0)
            model->setLimit(limit);
        model->componentComplete();
        return model;
    }

    /* the titles of the rows, "ml:<id>" for media <id> */
    static QStringList titles(const MLRecentMediaModel &model)
    {
        QStringList result;
        for (int i = 0; i < model.rowCount(); ++i)
            result.push_back(model.data(model.index(i), MLMediaModel::MEDIA_TITLE).toString());
        return result;
    }

    static QModelIndexList rows(const MLRecentMediaModel &model, std::initializer_list<int> list)
    {
        QModelIndexList indexes;
        for (int row : list)
            indexes.push_back(model.index(row));
        return indexes;
    }

    /* a new MediaLib reading the same settings, as after a restart */
    void restart()
    {
        m_medialib.reset();
        m_medialib = std::make_unique<MediaLib>(m_env->intf, m_controller.get());
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

    void testListsTheHistoryNewestFirst()
    {
        m_env->setHistory({{3, daysAgo(0)}, {1, daysAgo(1)}, {2, daysAgo(9)}});

        auto model = newModel();
        QTRY_COMPARE(titles(*model), QStringList({"ml:3", "ml:1", "ml:2"}));
    }

    void testGroupsByDay()
    {
        m_env->setHistory({{1, daysAgo(0)}, {2, daysAgo(1)}, {3, daysAgo(2)},
                           {4, daysAgo(6)}, {5, daysAgo(7)}, {6, daysAgo(400)}});

        auto model = newModel();
        const auto groups = [&model]() {
            QStringList result;
            for (int i = 0; i < model->rowCount(); ++i)
                result.push_back(model->data(model->index(i), MLRecentMediaModel::RECENT_GROUP).toString());
            return result;
        };
        QTRY_COMPARE(groups(), QStringList({"Today", "Yesterday", "This week", "This week",
                                            "Earlier", "Earlier"}));
    }

    void testRemoveFromRecentHidesOnlyThoseRows()
    {
        m_env->setHistory({{3, daysAgo(0)}, {1, daysAgo(1)}, {2, daysAgo(9)}});

        auto model = newModel();
        QTRY_COMPARE(titles(*model), QStringList({"ml:3", "ml:1", "ml:2"}));

        model->removeFromRecent(rows(*model, {1}));

        QTRY_COMPARE(titles(*model), QStringList({"ml:3", "ml:2"}));
        QCOMPARE(model->getCount(), 2u);
        // the media library history (play counts, progress) is untouched
        QCOMPARE(m_env->historyClears(), 0);
    }

    void testRemovedMediaComesBackWhenPlayedAgain()
    {
        m_env->setHistory({{3, daysAgo(0)}, {1, daysAgo(1)}});

        auto model = newModel();
        QTRY_COMPARE(titles(*model), QStringList({"ml:3", "ml:1"}));
        model->removeFromRecent(rows(*model, {1}));
        QTRY_COMPARE(titles(*model), QStringList({"ml:3"}));

        m_env->setHistory({{1, daysAgo(0) + 10}, {3, daysAgo(0)}});

        QTRY_COMPARE(titles(*model), QStringList({"ml:1", "ml:3"}));
    }

    void testRemoveFromRecentLastsAfterRestart()
    {
        m_env->setHistory({{3, daysAgo(0)}, {1, daysAgo(1)}, {2, daysAgo(9)}});
        {
            auto model = newModel();
            QTRY_COMPARE(titles(*model), QStringList({"ml:3", "ml:1", "ml:2"}));
            model->removeFromRecent(rows(*model, {0, 2}));
            QTRY_COMPARE(titles(*model), QStringList({"ml:1"}));
        }

        restart();

        auto model = newModel();
        QTRY_COMPARE(titles(*model), QStringList({"ml:1"}));
    }

    void testOpenRecentListsTheLastVisibleMedia()
    {
        m_env->setHistory({{5, daysAgo(0)}, {4, daysAgo(0, 1)}, {3, daysAgo(0, 2)},
                           {2, daysAgo(0, 3)}, {1, daysAgo(0, 4)}});
        auto all = newModel();
        QTRY_COMPARE(all->getCount(), 5u);
        all->removeFromRecent(rows(*all, {0, 2}));
        QTRY_COMPARE(all->getCount(), 3u);

        // like the VLC menu's Open Recent, with 2 in place of 10
        auto menu = newModel(2);
        QTRY_COMPARE(titles(*menu), QStringList({"ml:4", "ml:2"}));
    }

    void testClearRecentHidesEverythingPlayedSoFar()
    {
        // the same times when the history is given again below: a second
        // may pass meanwhile
        const int64_t today = daysAgo(0);
        const int64_t yesterday = daysAgo(1);
        m_env->setHistory({{3, today}, {1, yesterday}, {2, daysAgo(9)}});

        auto model = newModel();
        QTRY_COMPARE(titles(*model), QStringList({"ml:3", "ml:1", "ml:2"}));

        model->clearRecent();

        QTRY_COMPARE(model->getCount(), 0u);
        QCOMPARE(m_env->historyClears(), 0);

        // what is played next shows up, however long the reload took
        m_env->setHistory({{2, QDateTime::currentSecsSinceEpoch() + 60}, {3, today},
                           {1, yesterday}});
        QTRY_COMPARE(titles(*model), QStringList({"ml:2"}));

        model.reset();
        restart();
        auto restarted = newModel();
        QTRY_COMPARE(titles(*restarted), QStringList({"ml:2"}));
    }

    // Continue Watching: the videos stopped before their end, newest first;
    // Clear Recent hides them too
    void testContinueWatchingListsTheVideosInProgress()
    {
        const int64_t now = daysAgo(0);
        m_env->setHistory({
            {5, now, VLC_ML_MEDIA_TYPE_VIDEO, 0.4},
            {6, now - 10, VLC_ML_MEDIA_TYPE_VIDEO, -1.}, // watched to the end
            {7, now - 20, VLC_ML_MEDIA_TYPE_AUDIO, 0.5},
            {8, now - 30, VLC_ML_MEDIA_TYPE_VIDEO, 0.9},
        });

        auto continueWatching = std::make_unique<MLRecentVideoModel>();
        continueWatching->classBegin();
        continueWatching->setMl(m_medialib.get());
        continueWatching->componentComplete();
        const auto continueTitles = [&continueWatching]() {
            QStringList result;
            for (int i = 0; i < continueWatching->rowCount(); ++i)
                result.push_back(continueWatching->data(continueWatching->index(i),
                                                        MLMediaModel::MEDIA_TITLE).toString());
            return result;
        };
        QTRY_COMPARE(continueTitles(), QStringList({"ml:5", "ml:8"}));
        QCOMPARE(continueWatching->getCount(), 2u);

        auto recent = newModel();
        QTRY_COMPARE(recent->getCount(), 4u);
        recent->clearRecent();
        QTRY_COMPARE(continueWatching->getCount(), 0u);
    }

    // Home's rows list the last played first, as Recent: the media library
    // lists the history oldest first unless asked otherwise
    void testHomeRowsListTheLastPlayedFirst()
    {
        const int64_t now = daysAgo(0);
        m_env->setHistory({
            {5, now - 30, VLC_ML_MEDIA_TYPE_VIDEO, 0.4},
            {2, now - 25, VLC_ML_MEDIA_TYPE_AUDIO},
            {6, now, VLC_ML_MEDIA_TYPE_VIDEO, 0.2},
            {3, now - 5, VLC_ML_MEDIA_TYPE_AUDIO},
            {7, now - 10, VLC_ML_MEDIA_TYPE_VIDEO, 0.6},
            {4, now - 15, VLC_ML_MEDIA_TYPE_AUDIO},
        });

        const auto rowTitles = [](const MLMediaModel &model) {
            QStringList result;
            for (int i = 0; i < model.rowCount(); ++i)
                result.push_back(model.data(model.index(i), MLMediaModel::MEDIA_TITLE).toString());
            return result;
        };

        auto continueWatching = std::make_unique<MLRecentVideoModel>();
        continueWatching->classBegin();
        continueWatching->setMl(m_medialib.get());
        continueWatching->componentComplete();
        QTRY_COMPARE(rowTitles(*continueWatching), QStringList({"ml:6", "ml:7", "ml:5"}));

        auto listenAgain = std::make_unique<MLRecentAudioModel>();
        listenAgain->classBegin();
        listenAgain->setMl(m_medialib.get());
        listenAgain->componentComplete();
        QTRY_COMPARE(rowTitles(*listenAgain), QStringList({"ml:3", "ml:4", "ml:2"}));
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<PlaylistController> m_controller;
    std::unique_ptr<MediaLib> m_medialib;
};

QTEST_GUILESS_MAIN(TestMLRecent)
#include "test_ml_recent.moc"
