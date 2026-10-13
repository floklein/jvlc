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
#include "../medialibrary/medialib.hpp"
#include "../playlist/playlist_controller.hpp"
#include "vlc_stub_modules.hpp"

#include <memory>
#include <vector>

#include <QCoreApplication>
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
        QTRY_COMPARE_WITH_TIMEOUT(m_model->getCount(), 1000u, 100);
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

    // The Play rule from the library

    void testPlayMediaPlaysRestOfView()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 10);

        model->addAndPlay({model->index(6)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:7", "ml:8", "ml:9", "ml:10"}));
        QCOMPARE(currentIndex(), 0);
        clearQueue();
    }

    void testPlaySelectionPlaysOnlySelection()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 10);

        model->addAndPlay({model->index(1), model->index(4)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:2", "ml:5"}));
        clearQueue();
    }

    void testPlayReplacesQueueWithUndo()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 3);
        controller.append(QVariantList{QStringLiteral("vlc://pause:600")});

        model->addAndPlay({model->index(1)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:2", "ml:3"}));
        QVERIFY(controller.canUndoReplace());
        clearQueue();
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
        clearQueue();
    }

    void testAddToQueueFromLibrary()
    {
        PlaylistController controller(m_env->intf->p_playlist);
        auto model = makeModelWithQueue(controller, 0);
        controller.append(QVariantList{QStringLiteral("vlc://pause:600")});

        m_queueMedialib->addToPlaylist(QVariantList{QVariant::fromValue(MLItemId{3, VLC_ML_PARENT_UNKNOWN})});

        QTRY_COMPARE(queueNames().size(), 2);
        QCOMPARE(queueNames().at(1), QStringLiteral("ml:3"));
        clearQueue();
    }

private:
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

    void clearQueue()
    {
        vlc_playlist_t *playlist = m_env->intf->p_playlist;
        vlc::playlist::vlc_playlist_locker locker(playlist);
        vlc_playlist_Clear(playlist);
    }

    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<MediaLib> m_medialib;
    std::unique_ptr<MLTestModel> m_model;
    std::unique_ptr<MediaLib> m_queueMedialib;
};


QTEST_GUILESS_MAIN(TestVLCMLModel)
#include "test_ml_model.moc"
