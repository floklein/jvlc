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
#include "../medialibrary/mlrecentmediamodel.hpp"
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
        m_env->setHistory({{3, daysAgo(0)}, {1, daysAgo(1)}, {2, daysAgo(9)}});

        auto model = newModel();
        QTRY_COMPARE(titles(*model), QStringList({"ml:3", "ml:1", "ml:2"}));

        model->clearRecent();

        QTRY_COMPARE(model->getCount(), 0u);
        QCOMPARE(m_env->historyClears(), 0);

        // what is played next shows up
        m_env->setHistory({{2, daysAgo(0) + 10}, {3, daysAgo(0)}, {1, daysAgo(1)}});
        QTRY_COMPARE(titles(*model), QStringList({"ml:2"}));

        model.reset();
        restart();
        auto restarted = newModel();
        QTRY_COMPARE(titles(*restarted), QStringList({"ml:2"}));
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
