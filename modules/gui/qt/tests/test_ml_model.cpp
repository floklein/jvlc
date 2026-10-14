/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include "../medialibrary/mlbasemodel.hpp"
#include "../medialibrary/mlfoldertreemodel.hpp"
#include "../medialibrary/mlmediafolder.hpp"
#include "../medialibrary/mlmusicfoldertreemodel.hpp"
#include "../medialibrary/mlplaycountrank.hpp"
#include "../medialibrary/medialib.hpp"
#include "../playlist/playlist_controller.hpp"
#include "vlc_stub_modules.hpp"

#include <memory>
#include <optional>
#include <vector>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

struct vlc_medialibrary_t;

using vlc::playlist::PlaylistController;

class MLTestModel : public MLBaseModel
{
    Q_OBJECT

public:
    virtual QVariant itemRoleData(const MLItem *item, int role) const override
    {
        VLC_UNUSED(role);
        if (!item)
            return {};

        return QVariant::fromValue(item->getId());
    }

    void appendRange(int64_t min, int64_t max)
    {
        for (int64_t i = min; i <= max; ++i)
            m_items.emplace_back(i, VLC_ML_PARENT_UNKNOWN);
        emit resetRequested();
    }

    void updateItem(size_t position, int64_t id)
    {
        MLItemId itemId{id, VLC_ML_PARENT_UNKNOWN};
        m_items[position] = itemId;
        updateItemInCache(itemId);
    }


protected:
    std::unique_ptr<MLListCacheLoader> createMLLoader() const override
    {
        return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLTestModel::Loader>(*this, *this));
    }

    struct Loader : public MLListCacheLoader::MLOp
    {
        Loader(const MLBaseModel& model, const MLTestModel& parent)
            : MLOp(model)
            , m_mlTestModel(parent)
        {}

        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override
        {
            VLC_UNUSED(ml);
            VLC_UNUSED(queryParams);
            return m_mlTestModel.m_items.size();
        }

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override
        {
            VLC_UNUSED(ml);

            if (m_mlTestModel.m_items.empty())
                return {};

            uint32_t offset = queryParams->i_offset;
            uint32_t count = queryParams->i_nbResults;
            size_t maxIndex = std::min(
                static_cast<size_t>(offset + count),
                m_mlTestModel.m_items.size() - 1);
            std::vector<std::unique_ptr<MLItem>> ret;
            for (size_t i = offset; i <= maxIndex; ++i) {
                ret.emplace_back(std::make_unique<MLItem>(m_mlTestModel.m_items[i]));
            }
            return ret;
        }

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override
        {
            VLC_UNUSED(ml);
            return std::make_unique<MLItem>(itemId);
        }

        const MLTestModel& m_mlTestModel;
    };


    std::vector<MLItemId> m_items;
};

// Folders (or playlists, artists) ranked by the play counts of their media.
// The play counts are given per entity instead of being read from the media
// library.
class MLRankTestModel : public MLBaseModel
{
    Q_OBJECT

public:
    QVariant itemRoleData(const MLItem *item, int role) const override
    {
        VLC_UNUSED(role);
        if (!item)
            return {};
        return QVariant::fromValue(item->getId());
    }

    void addFolder(int64_t id, uint64_t playCount, time_t lastPlayed = 0)
    {
        m_folders.emplace_back(id, VLC_ML_PARENT_FOLDER);
        m_playCounts[id] = { playCount, lastPlayed };
    }

    QVector<int64_t> rows() const
    {
        QVector<int64_t> ids;
        for (int row = 0; row < rowCount(); ++row)
            ids.push_back(data(index(row), Qt::DisplayRole).value<MLItemId>().id);
        return ids;
    }

protected:
    std::unique_ptr<MLListCacheLoader> createMLLoader() const override
    {
        auto playCounts = m_playCounts;
        auto counter = [playCounts](vlc_medialibrary_t*, const MLItemId& id) {
            return playCounts.value(id.id);
        };
        return std::make_unique<MLListCacheLoader>(m_mediaLib,
            std::make_shared<MLPlayCountRank>(*this, std::make_shared<Loader>(*this), counter));
    }

    struct Loader : public MLListCacheLoader::MLOp
    {
        Loader(const MLRankTestModel& model)
            : MLOp(model)
            , m_folders(model.m_folders)
        {}

        size_t count(vlc_medialibrary_t*, const vlc_ml_query_params_t*) const override
        {
            return m_folders.size();
        }

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t*, const vlc_ml_query_params_t* queryParams) const override
        {
            size_t first = std::min<size_t>(queryParams->i_offset, m_folders.size());
            size_t last = queryParams->i_nbResults == 0
                ? m_folders.size()
                : std::min<size_t>(first + queryParams->i_nbResults, m_folders.size());
            std::vector<std::unique_ptr<MLItem>> ret;
            for (size_t i = first; i < last; ++i)
                ret.emplace_back(std::make_unique<MLItem>(m_folders[i]));
            return ret;
        }

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t*, MLItemId itemId) const override
        {
            return std::make_unique<MLItem>(itemId);
        }

        std::vector<MLItemId> m_folders;
    };

    std::vector<MLItemId> m_folders;
    QHash<int64_t, MLPlayCountRank::PlayCount> m_playCounts;
};

class TestVLCMLModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
    }

    void cleanupTestCase() {
        m_env.reset();
    }


    void init() {
        m_medialib = std::make_unique<MediaLib>(m_env->intf, nullptr);
        m_model = std::make_unique<MLTestModel>();
        m_model->classBegin();
        m_model->setMl(m_medialib.get());
    }

    void cleanup() {
        m_model.reset();
        m_queueMedialib.reset();
        m_medialib.reset();
        m_env->mlFolders.clear();
        m_env->mlMedia.clear();
        resetQueue();
    }

    /**
     * @brief testModelUpdatedFromCallback
     * test for !6537 scenario
     *
     * - Count&Load task issued (task n°1)
     * - model need reset (thumbnail is updated for instance) => `needReload = true`
     * - Count&Load task (n°1) is resolved
     *    * diff util insert/remove rows
     *    - Callback from model change ask to refer an item beyond the cache
     *    - refer issue a new 'fetch more' request (n°2) as m_countTask is 0
     *    - as `needReload await true`, cache is moved to oldCache, new count&load request
     *       is issued (n° 3)
     * - task n°2 resolve, cache is null, assertion fails
     *
     */
    void testModelRecuseUpdateFromSlot()
    {
        connect(m_model.get(), &BaseModel::countChanged, this, [this](){
            m_model->updateItem(13, 37);
            //model will initially only load 100 elements fetching beyond the loaded range
            // will trigger "fetch more" request
            m_model->data(m_model->index(500), Qt::DisplayRole);
        }, Qt::SingleShotConnection);

        m_model->appendRange(1, 1000);
        m_model->componentComplete();
        QTRY_COMPARE(m_model->getCount(), 1000u);
    }


    void testGetIndexFromID()
    {
        const int low = 1;
        const int high = 300;
        m_model->appendRange(low, high);
        m_model->componentComplete();

        // let loading complete, getIndexFromID won't work for 'loading' model
        QVERIFY(QTest::qWaitFor([this] () { return !m_model->loading(); }));

        std::optional<int> row = -1;
        auto cb = [&row](std::optional<int> index)
        {
            row = index;
        };

        // bug of MR6926, here model tries to get index for data outside the initial chunk size
        m_model->getIndexFromId2(MLItemId{high, VLC_ML_PARENT_UNKNOWN}, cb);

        // NOTE: CI fails with QTRY_COMPARE_WITH_TIMEOUT
        QVERIFY(QTest::qWaitFor([&row]() { return row != -1; }));
        QCOMPARE(row, high - 1);

        // above we must have loaded all the data, following this we should not need to wait for results
        m_model->getIndexFromId2(MLItemId{low - 1, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, std::nullopt);

        m_model->getIndexFromId2(MLItemId{low, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, low - 1);

        m_model->getIndexFromId2(MLItemId{high + 1, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, std::nullopt);

        const int mid = (low + high) / 2;
        m_model->getIndexFromId2(MLItemId{mid, VLC_ML_PARENT_UNKNOWN}, cb);
        QCOMPARE(row, mid - 1);
    }

    void testPlayCountRankMostPlayedFirst()
    {
        MLRankTestModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.setSortOrder(Qt::DescendingOrder);
        model.addFolder(1, 2);
        model.addFolder(2, 0);
        model.addFolder(3, 5);
        model.addFolder(4, 1);
        model.addFolder(5, 0);
        model.componentComplete();

        QVERIFY(QTest::qWaitFor([&model]() { return !model.loading(); }));
        // folders never played are left out
        QCOMPARE(model.getCount(), 3u);
        QCOMPARE(model.rows(), (QVector<int64_t>{ 3, 1, 4 }));
    }

    void testPlayCountRankTieMostRecentFirst()
    {
        MLRankTestModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.setSortOrder(Qt::DescendingOrder);
        model.addFolder(1, 2, 100);
        model.addFolder(2, 2, 300);
        model.addFolder(3, 2, 200);
        model.componentComplete();

        QVERIFY(QTest::qWaitFor([&model]() { return !model.loading(); }));
        QCOMPARE(model.rows(), (QVector<int64_t>{ 2, 3, 1 }));
    }

    void testPlayCountRankAscending()
    {
        MLRankTestModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.setSortOrder(Qt::AscendingOrder);
        model.addFolder(1, 2);
        model.addFolder(2, 0);
        model.addFolder(3, 5);
        model.addFolder(4, 1);
        model.componentComplete();

        QVERIFY(QTest::qWaitFor([&model]() { return !model.loading(); }));
        QCOMPARE(model.rows(), (QVector<int64_t>{ 4, 1, 3 }));
    }

    void testPlayCountRankLimit()
    {
        MLRankTestModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.setSortOrder(Qt::DescendingOrder);
        model.setLimit(2);
        model.addFolder(1, 2);
        model.addFolder(2, 0);
        model.addFolder(3, 5);
        model.addFolder(4, 1);
        model.componentComplete();

        QVERIFY(QTest::qWaitFor([&model]() { return !model.loading(); }));
        QCOMPARE(model.getMaximumCount(), 3u);
        QCOMPARE(model.getCount(), 2u);
        QCOMPARE(model.rows(), (QVector<int64_t>{ 3, 1 }));
    }

    void testPlayCountRankNothingPlayed()
    {
        MLRankTestModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.addFolder(1, 0);
        model.addFolder(2, 0);
        model.componentComplete();

        QVERIFY(QTest::qWaitFor([&model]() { return !model.loading(); }));
        QCOMPARE(model.getCount(), 0u);
    }

    // The Play rule from the library

    void testPlayMediaPlaysRestOfView()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 10);

        model->addAndPlay({model->index(6)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:7", "ml:8", "ml:9", "ml:10"}));
        QCOMPARE(currentIndex(), 0);
    }

    void testPlaySelectionPlaysOnlySelection()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 10);

        model->addAndPlay({model->index(1), model->index(4)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:2", "ml:5"}));
    }

    void testPlayReplacesQueueWithUndo()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 3);
        controller.append(QVariantList{QStringLiteral("vlc://pause:600")});

        model->addAndPlay({model->index(1)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:2", "ml:3"}));
        QVERIFY(controller.canUndoReplace());
    }

    void testPlayNextFromLibrary()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 0);
        controller.append(QVariantList{QStringLiteral("vlc://pause:600"), QStringLiteral("vlc://pause:601")});
        controller.goTo(0);

        m_queueMedialib->playNext({QVariant::fromValue(MLItemId{3, VLC_ML_PARENT_UNKNOWN})});

        QTRY_COMPARE(queueNames().size(), 3);
        QCOMPARE(queueNames().at(1), QStringLiteral("ml:3"));
        QCOMPARE(currentIndex(), 0);
    }

    void testAddToQueueFromLibrary()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 0);
        controller.append(QVariantList{QStringLiteral("vlc://pause:600")});

        m_queueMedialib->addToPlaylist(QVariantList{QVariant::fromValue(MLItemId{3, VLC_ML_PARENT_UNKNOWN})});

        QTRY_COMPARE(queueNames().size(), 2);
        QCOMPARE(queueNames().at(1), QStringLiteral("ml:3"));
    }

    // The Videos folder tree

    void testTreeTopListsAddedFoldersWithVideos()
    {
        m_env->mlFolders = {
            {1, 0, "Shows"}, {2, 1, "The Wand"},
            {3, 0, "Music"},
            {4, 0, "Downloads"}, // added, not scanned yet
        };
        m_env->mlMedia = {
            {10, 2, "The Wand S01E01"},
            {11, 3, "Song", -1., 0, VLC_ML_MEDIA_TYPE_AUDIO},
        };

        auto model = makeTreeModel(std::nullopt);

        QCOMPARE(titles(*model), QStringList({"Downloads", "Shows"}));
        QCOMPARE(roleValues(*model, "isFolder"), QVariantList({true, true}));
        QVERIFY(model->path().isEmpty());
    }

    void testTreeFolderListsSubfoldersThenVideos()
    {
        m_env->mlFolders = {
            {1, 0, "Shows"}, {2, 1, "The Wand"},
            {3, 2, "S10"}, {4, 2, "S2"}, {5, 2, "Extras"}, {6, 5, "Empty"},
        };
        m_env->mlMedia = {
            {10, 3, "The Wand S10E01"},
            {11, 4, "The Wand S02E01"},
            {12, 2, "Trailer"},
        };

        auto model = makeTreeModel(2);

        // Extras holds no video at any depth
        QCOMPARE(titles(*model), QStringList({"S2", "S10", "Trailer"}));
        QCOMPARE(roleValues(*model, "isFolder"), QVariantList({true, true, false}));
        QCOMPARE(model->getCount(), 3u);
    }

    void testTreeShowsVideosBelowAFolder()
    {
        m_env->mlFolders = { {1, 0, "Shows"}, {2, 1, "The Wand"}, {3, 2, "S01"} };
        m_env->mlMedia = { {10, 3, "The Wand S01E01"} };

        auto model = makeTreeModel(1);

        QCOMPARE(titles(*model), QStringList({"The Wand"}));
    }

    void testTreePathFromTheTop()
    {
        m_env->mlFolders = { {1, 0, "Shows"}, {2, 1, "The Wand"}, {3, 2, "S01"} };
        m_env->mlMedia = { {10, 3, "The Wand S01E01"} };

        auto model = makeTreeModel(3);

        QTRY_COMPARE(model->path().size(), 3);
        QStringList pathTitles;
        QVector<int64_t> pathIds;
        for (const QVariant &crumb : model->path())
        {
            pathTitles.push_back(crumb.toMap().value("title").toString());
            pathIds.push_back(crumb.toMap().value("id").value<MLItemId>().id);
        }
        QCOMPARE(pathTitles, QStringList({"Shows", "The Wand", "S01"}));
        QCOMPARE(pathIds, QVector<int64_t>({1, 2, 3}));

        model->setParentId(MLItemId(1, VLC_ML_PARENT_FOLDER));
        QTRY_COMPARE(model->path().size(), 1);

        model->unsetParentId();
        QTRY_VERIFY(model->path().isEmpty());
    }

    void testUnwatchedFirst()
    {
        setUpEpisodes();

        auto model = makeTreeModel(1, "unwatched", Qt::AscendingOrder);

        // folders first; then in progress, unwatched, watched, each by title
        QCOMPARE(titles(*model), QStringList({"Extras", "E5", "E1", "E4", "E10", "E2", "E3"}));
    }

    void testWatchedFirst()
    {
        setUpEpisodes();

        auto model = makeTreeModel(1, "unwatched", Qt::DescendingOrder);

        QCOMPARE(titles(*model), QStringList({"Extras", "E2", "E3", "E5", "E1", "E4", "E10"}));
    }

    void testTitleZToA()
    {
        setUpEpisodes();
        m_env->mlFolders.push_back({4, 1, "Bonus"});
        m_env->mlMedia.push_back({30, 4, "Making of"});

        auto model = makeTreeModel(1, "title", Qt::DescendingOrder);

        QCOMPARE(titles(*model), QStringList({"Extras", "Bonus", "E10", "E5", "E4", "E3", "E2", "E1"}));
    }

    void testNewestFirst()
    {
        setUpEpisodes();

        auto model = makeTreeModel(1, "insertion", Qt::DescendingOrder);

        QCOMPARE(titles(*model), QStringList({"Extras", "E10", "E5", "E4", "E3", "E2", "E1"}));
    }

    void testSearchFiltersTheLevel()
    {
        setUpEpisodes();

        auto model = makeTreeModel(1, "title", Qt::AscendingOrder, "e1");

        QCOMPARE(titles(*model), QStringList({"E1", "E10"}));
    }

    void testTreeRowsPage()
    {
        setUpEpisodes();

        auto model = makeTreeModel(1, "title", Qt::AscendingOrder);
        model->setLimit(3);
        QTRY_COMPARE(model->getCount(), 3u);

        QCOMPARE(titles(*model), QStringList({"Extras", "E1", "E2"}));
        QCOMPARE(model->getMaximumCount(), 7u);
    }

    // The library folder of the playing file (the folder button)

    void testMediaFolderOfAFile()
    {
        m_env->mlFolders = { {1, 0, "Shows"}, {2, 1, "The Wand"}, {3, 2, "S01"} };
        m_env->mlMedia = { {10, 3, "The Wand S01E04"} };

        MLMediaFolder folder;
        folder.setMl(m_medialib.get());
        folder.setUrl(QUrl("file:///library/Shows/The%20Wand/S01/The%20Wand%20S01E04.mkv"));

        QTRY_VERIFY(folder.found());
        QCOMPARE(folder.folderId().id, 3);
        QCOMPARE(folder.folderId().type, VLC_ML_PARENT_FOLDER);
        QVERIFY(!folder.loading());
    }

    void testMediaFolderOutsideTheLibrary()
    {
        m_env->mlFolders = { {1, 0, "Shows"} };

        MLMediaFolder folder;
        folder.setMl(m_medialib.get());
        folder.setUrl(QUrl("file:///elsewhere/clip.mkv"));

        QVERIFY(folder.loading());
        QTRY_VERIFY(!folder.loading());
        QVERIFY(!folder.found());
    }

    void testMediaFolderOfAStream()
    {
        m_env->mlFolders = { {1, 0, "Shows"} };

        MLMediaFolder folder;
        folder.setMl(m_medialib.get());
        folder.setUrl(QUrl("https://example.com/live/stream.m3u8"));

        // nothing to look up
        QVERIFY(!folder.loading());
        QVERIFY(!folder.found());
    }

    void testMediaFolderFollowsTheUrl()
    {
        m_env->mlFolders = { {1, 0, "The Wand"}, {2, 1, "S01"}, {3, 1, "S02"} };

        MLMediaFolder folder;
        folder.setMl(m_medialib.get());
        QSignalSpy changes(&folder, &MLMediaFolder::folderChanged);

        folder.setUrl(QUrl("file:///library/The%20Wand/S01/E01.mkv"));
        QTRY_VERIFY(folder.found());
        QCOMPARE(folder.folderId().id, 2);

        folder.setUrl(QUrl("file:///library/The%20Wand/S02/E01.mkv"));
        // the previous folder is not shown for the new file
        QVERIFY(!folder.found());
        QTRY_VERIFY(folder.found());
        QCOMPARE(folder.folderId().id, 3);

        folder.setUrl(QUrl());
        QVERIFY(!folder.found());
        QVERIFY(!folder.loading());
        QCOMPARE(changes.count(), 4);
    }

    void testMediaFolderWaitsForTheLibrary()
    {
        m_env->mlFolders = { {1, 0, "Shows"} };

        MLMediaFolder folder;
        folder.setUrl(QUrl("file:///library/Shows/E01.mkv"));
        QVERIFY(!folder.found());

        folder.setMl(m_medialib.get());
        QTRY_VERIFY(folder.found());
        QCOMPARE(folder.folderId().id, 1);
    }

    // Songs: the music folder tree

    void testSongsTopListsAddedFoldersWithMusic()
    {
        m_env->mlFolders = {
            {1, 0, "Shows"}, {2, 1, "The Wand"},
            {3, 0, "Music"}, {4, 3, "Juxtopposed"},
            {5, 0, "Downloads"}, // added, not scanned yet
        };
        m_env->mlMedia = {
            {10, 2, "The Wand S01E01"},
            {11, 4, "untangle", -1., 0, VLC_ML_MEDIA_TYPE_AUDIO},
        };

        auto model = makeSongsModel(std::nullopt);

        QCOMPARE(titles(*model), QStringList({"Downloads", "Music"}));
        QCOMPARE(roleValues(*model, "isFolder"), QVariantList({true, true}));
    }

    void testSongsFolderListsSubfoldersThenTracks()
    {
        m_env->mlFolders = {
            {1, 0, "Music"}, {2, 1, "Disc 10"}, {3, 1, "Disc 2"},
            {4, 1, "Clips"}, // videos only
        };
        m_env->mlMedia = {
            {10, 2, "Ten", -1., 0, VLC_ML_MEDIA_TYPE_AUDIO},
            {11, 3, "Two", -1., 0, VLC_ML_MEDIA_TYPE_AUDIO},
            {12, 4, "Clip"},
            {13, 1, "moonlight", -1., 0, VLC_ML_MEDIA_TYPE_AUDIO, "juxtopposed", "Moons", 2, 1},
            {14, 1, "A video"},
        };

        auto model = makeSongsModel(1);

        QCOMPARE(titles(*model), QStringList({"Disc 2", "Disc 10", "moonlight"}));
        QCOMPARE(roleValues(*model, "isFolder"), QVariantList({true, true, false}));
        QCOMPARE(roleValues(*model, "main_artist").at(2).toString(), QStringLiteral("juxtopposed"));
        QCOMPARE(roleValues(*model, "album_title").at(2).toString(), QStringLiteral("Moons"));
        QCOMPARE(roleValues(*model, "track_number").at(2).toUInt(), 2u);
    }

    void testSongsPathFromTheTop()
    {
        m_env->mlFolders = { {1, 0, "Music"}, {2, 1, "My Songs"} };
        m_env->mlMedia = { {10, 2, "untangle", -1., 0, VLC_ML_MEDIA_TYPE_AUDIO} };

        auto model = makeSongsModel(2);

        QTRY_COMPARE(model->path().size(), 2);
        QCOMPARE(model->path().at(1).toMap().value("title").toString(), QStringLiteral("My Songs"));
    }

    void testSongsSortByArtist()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "main_artist", Qt::AscendingOrder);

        // folders first; then by artist, album, disc and track; no artist last
        QCOMPARE(titles(*model), QStringList({"Live", "Alpha", "Bravo", "Delta", "Charlie", "Echo"}));
    }

    void testSongsSortByArtistDescending()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "main_artist", Qt::DescendingOrder);

        QCOMPARE(titles(*model), QStringList({"Live", "Echo", "Charlie", "Delta", "Bravo", "Alpha"}));
    }

    void testSongsSortByAlbum()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "album_title", Qt::AscendingOrder);

        QCOMPARE(titles(*model), QStringList({"Live", "Delta", "Alpha", "Bravo", "Charlie", "Echo"}));
    }

    void testSongsSortByTrack()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "track_number", Qt::AscendingOrder);

        QCOMPARE(titles(*model), QStringList({"Live", "Bravo", "Alpha", "Delta", "Echo", "Charlie"}));
    }

    void testSongsSortByDisc()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "disc_number", Qt::AscendingOrder);

        QCOMPARE(titles(*model), QStringList({"Live", "Alpha", "Delta", "Echo", "Bravo", "Charlie"}));
    }

    void testSongsSortByTitleZToA()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "title", Qt::DescendingOrder);

        QCOMPARE(titles(*model), QStringList({"Live", "Echo", "Delta", "Charlie", "Bravo", "Alpha"}));
    }

    void testSongsNewestFirst()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "insertion", Qt::DescendingOrder);

        QCOMPARE(titles(*model), QStringList({"Live", "Echo", "Delta", "Charlie", "Bravo", "Alpha"}));
    }

    void testSongsSearchMatchesArtistAndAlbum()
    {
        setUpSongs();

        auto model = makeSongsModel(1, "title", Qt::AscendingOrder, "dawn");

        // "Dawn" is Delta's album; the folder doesn't match
        QCOMPARE(titles(*model), QStringList({"Delta"}));
    }

    void testSongsPlayTakesTheRestOfTheView()
    {
        setUpSongs();
        PlaylistController controller(m_env->intf->p_playlist);
        m_queueMedialib = std::make_unique<MediaLib>(m_env->intf, &controller);
        auto model = makeTreeModel<MLMusicFolderTreeModel>(m_queueMedialib.get(), 1,
                                                           "main_artist", Qt::AscendingOrder);
        QCOMPARE(titles(*model), QStringList({"Live", "Alpha", "Bravo", "Delta", "Charlie", "Echo"}));

        // the tracks after Bravo play in the order of the view
        model->addAndPlay({model->index(2)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:21", "ml:23", "ml:22", "ml:24"}));
        QCOMPARE(currentIndex(), 0);
    }

private:
    // an album folder, with a subfolder and tracks added in the order of
    // their ids
    void setUpSongs()
    {
        const auto audio = VLC_ML_MEDIA_TYPE_AUDIO;
        m_env->mlFolders = { {1, 0, "Juxtopposed"}, {2, 1, "Live"} };
        m_env->mlMedia = {
            //id, folder, title, progress, plays, type, artist, album, track, disc
            {20, 1, "Alpha", -1., 0, audio, "Ana", "Day", 2, 1},
            {21, 1, "Bravo", -1., 0, audio, "Ana", "Day", 1, 2},
            {22, 1, "Charlie", -1., 0, audio, "Bob", "Day", 4, 2},
            {23, 1, "Delta", -1., 0, audio, "Bob", "Dawn", 3, 1},
            {24, 1, "Echo", -1., 0, audio, "", "", 3, 1},
            {30, 2, "Encore", -1., 0, audio, "Ana", "Live", 1, 1},
        };
    }

    std::unique_ptr<MLMusicFolderTreeModel> makeSongsModel(std::optional<int64_t> parent,
                                                           const QString &criteria = {},
                                                           Qt::SortOrder order = Qt::AscendingOrder,
                                                           const QString &pattern = {})
    {
        return makeTreeModel<MLMusicFolderTreeModel>(m_medialib.get(), parent, criteria, order, pattern);
    }

    // a show with a subfolder and episodes added in the order of their ids
    void setUpEpisodes()
    {
        m_env->mlFolders = { {1, 0, "The Wand"}, {2, 1, "Extras"} };
        m_env->mlMedia = {
            {20, 1, "E1"},
            {21, 1, "E2", -1., 1},  // watched
            {22, 1, "E3", -1., 2},  // watched twice
            {23, 1, "E4"},
            {24, 1, "E5", 0.4, 0},  // in progress
            {25, 1, "E10"},
            {26, 2, "Interview"},
        };
    }

    std::unique_ptr<MLFolderTreeModel> makeTreeModel(std::optional<int64_t> parent,
                                                     const QString &criteria = {},
                                                     Qt::SortOrder order = Qt::AscendingOrder,
                                                     const QString &pattern = {})
    {
        return makeTreeModel<MLFolderTreeModel>(m_medialib.get(), parent, criteria, order, pattern);
    }

    template<typename Model>
    std::unique_ptr<Model> makeTreeModel(MediaLib *medialib,
                                         std::optional<int64_t> parent,
                                         const QString &criteria = {},
                                         Qt::SortOrder order = Qt::AscendingOrder,
                                         const QString &pattern = {})
    {
        auto model = std::make_unique<Model>();
        model->classBegin();
        if (parent)
            model->setParentId(MLItemId(*parent, VLC_ML_PARENT_FOLDER));
        model->setSortCriteria(criteria);
        model->setSortOrder(order);
        model->setSearchPattern(pattern);
        model->setMl(medialib);
        model->componentComplete();
        [&] { QVERIFY(QTest::qWaitFor([&]() { return !model->loading(); })); }();
        return model;
    }

    QVariantList roleValues(const QAbstractItemModel &model, const QByteArray &roleName) const
    {
        const int role = model.roleNames().key(roleName, -1);
        Q_ASSERT(role != -1);
        QVariantList values;
        for (int row = 0; row < model.rowCount(); ++row)
            values.push_back(model.data(model.index(row, 0), role));
        return values;
    }

    QStringList titles(const QAbstractItemModel &model) const
    {
        QStringList result;
        for (const QVariant &title : roleValues(model, "title"))
            result.push_back(title.toString());
        return result;
    }

    /* A model of `count` media (ids 1 to count) whose medialib plays into
     * controller */
    std::unique_ptr<MLTestModel> makeModelWithQueue(PlaylistController &controller, int count)
    {
        m_queueMedialib = std::make_unique<MediaLib>(m_env->intf, &controller);
        auto model = std::make_unique<MLTestModel>();
        model->classBegin();
        model->setMl(m_queueMedialib.get());
        if (count > 0)
            model->appendRange(1, count);
        model->componentComplete();
        if (count > 0)
        {
            [&] { QVERIFY(QTest::qWaitFor([&]() { return !model->loading()
                                                && model->getCount() == unsigned(count); })); }();
        }
        return model;
    }

    QStringList queueNames() const
    {
        vlc_playlist_t *playlist = m_env->intf->p_playlist;
        QStringList names;
        vlc::playlist::vlc_playlist_locker locker(playlist);
        for (size_t i = 0; i < vlc_playlist_Count(playlist); ++i)
            names.push_back(qfu(vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist, i))->psz_name));
        return names;
    }

    int currentIndex() const
    {
        vlc_playlist_t *playlist = m_env->intf->p_playlist;
        vlc::playlist::vlc_playlist_locker locker(playlist);
        return static_cast<int>(vlc_playlist_GetCurrentIndex(playlist));
    }

    /* Leave an empty Queue and an idle player to the next test, even if this
     * one failed: a player still started, or still stopping the previous
     * media, would move the current item of the next test's Queue. */
    void resetQueue()
    {
        vlc_playlist_t *playlist = m_env->intf->p_playlist;
        vlc_player_t *player = vlc_playlist_GetPlayer(playlist);
        {
            vlc::playlist::vlc_playlist_locker locker(playlist);
            vlc_playlist_Stop(playlist);
            vlc_playlist_Clear(playlist);
        }
        QTRY_VERIFY([&] {
            vlc::playlist::vlc_playlist_locker locker(playlist);
            return vlc_player_GetCurrentMedia(player) == nullptr
                && vlc_player_GetState(player) == VLC_PLAYER_STATE_STOPPED;
        }());
    }

    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<MediaLib> m_medialib;
    std::unique_ptr<MLTestModel> m_model;
    std::unique_ptr<MediaLib> m_queueMedialib;
};


QTEST_GUILESS_MAIN(TestVLCMLModel)
#include "test_ml_model.moc"
