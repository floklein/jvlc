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

/* What the Queue panel shows and does: played, current and upcoming items,
 * what is left, Sort, Repeat and the keyboard moves. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_stub_modules.hpp"

#include "../playlist/playlist_controller.hpp"
#include "../playlist/playlist_model.hpp"

#include <memory>

#include <QStringList>
#include <QTest>

using vlc::playlist::Media;
using vlc::playlist::PlaylistController;
using vlc::playlist::PlaylistListModel;
using vlc::playlist::vlc_playlist_locker;

/* An item that plays for a long time, so the Queue does not advance by
 * itself while a test runs */
static const char LONG_MRL[] = "vlc://pause:600";

/* An item named @a name that lasts @a seconds */
static Media media(const char *name, int seconds = 60)
{
    input_item_t *item = input_item_NewExt(LONG_MRL, name,
                                           VLC_TICK_FROM_SEC(seconds),
                                           ITEM_TYPE_FILE, ITEM_NET_UNKNOWN);
    if (!item)
        throw std::bad_alloc();
    Media m(item);
    input_item_Release(item);
    return m;
}

static QVector<Media> medias(const QStringList &names)
{
    QVector<Media> list;
    for (const QString &name : names)
        list.push_back(media(qtu(name)));
    return list;
}

class TestQueuePanel : public QObject
{
    Q_OBJECT

private:
    vlc_playlist_t *playlist() const
    {
        return m_env->intf->p_playlist;
    }

    /* The Queue as the panel shows it: the titles of its rows */
    QStringList rows() const
    {
        QStringList titles;
        for (int i = 0; i < m_model->rowCount(); ++i)
            titles.push_back(m_model->data(m_model->index(i), PlaylistListModel::TitleRole).toString());
        return titles;
    }

    QList<bool> playedRows() const
    {
        QList<bool> played;
        for (int i = 0; i < m_model->rowCount(); ++i)
            played.push_back(m_model->data(m_model->index(i), PlaylistListModel::IsPlayedRole).toBool());
        return played;
    }

    /* Fill the Queue and wait for the panel to show it */
    void fillMedia(const QVector<Media> &items, int current = -1)
    {
        m_controller->append(items, false);
        if (current >= 0)
            m_controller->goTo(current);
        QTRY_COMPARE(m_model->rowCount(), items.size());
        QTRY_COMPARE(m_model->getCurrentIndex(), current);
    }

    void fill(const QStringList &names, int current = -1)
    {
        fillMedia(medias(names), current);
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
        m_model = std::make_unique<PlaylistListModel>(playlist());
    }

    void cleanup()
    {
        {
            vlc_playlist_locker locker(playlist());
            vlc_playlist_Clear(playlist());
            vlc_playlist_SetPlaybackRepeat(playlist(), VLC_PLAYLIST_PLAYBACK_REPEAT_NONE);
            vlc_playlist_SetPlaybackOrder(playlist(), VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL);
        }
        m_model.reset();
        m_controller.reset();
    }

    // Played, current and upcoming items

    void testItemsBeforeTheCurrentOneArePlayed()
    {
        fill({"a", "b", "c", "d"}, 2);

        QCOMPARE(playedRows(), QList<bool>({true, true, false, false}));
        QVERIFY(m_model->data(m_model->index(2), PlaylistListModel::IsCurrentRole).toBool());
    }

    void testNothingIsPlayedWithoutCurrentItem()
    {
        fill({"a", "b"});

        QCOMPARE(playedRows(), QList<bool>({false, false}));
    }

    void testPlayedItemsFollowTheCurrentItem()
    {
        fill({"a", "b", "c", "d"}, 2);

        m_controller->goTo(3);

        QTRY_COMPARE(playedRows(), QList<bool>({true, true, true, false}));
        m_controller->goTo(0);
        QTRY_COMPARE(playedRows(), QList<bool>({false, false, false, false}));
    }

    // What is left

    void testWhatIsLeftCountsTheCurrentItemAndTheNextOnes()
    {
        fillMedia({media("a", 10), media("b", 20), media("c", 30), media("d", 40)}, 1);

        QCOMPARE(m_model->getRemainingCount(), 3);
        QCOMPARE(m_model->getRemainingDuration().toVLCTick(), VLC_TICK_FROM_SEC(90));
    }

    void testWhatIsLeftIsTheWholeQueueWithoutCurrentItem()
    {
        fillMedia({media("a", 10), media("b", 20)});

        QCOMPARE(m_model->getRemainingCount(), 2);
        QCOMPARE(m_model->getRemainingDuration().toVLCTick(), VLC_TICK_FROM_SEC(30));
    }

    void testTimeLeftExcludesWhatWasPlayedOfTheCurrentItem()
    {
        fillMedia({media("a", 10), media("b", 20), media("c", 30)}, 1);

        m_model->setCurrentTime(VLCTime(VLC_TICK_FROM_SEC(5)));

        QCOMPARE(m_model->getRemainingDuration().toVLCTick(), VLC_TICK_FROM_SEC(45));
    }

    void testWhatIsLeftFollowsTheQueue()
    {
        fillMedia({media("a", 10), media("b", 20)}, 0);

        m_controller->append(QVector<Media>{media("c", 30)}, false);
        QTRY_COMPARE(m_model->getRemainingCount(), 3);
        QCOMPARE(m_model->getRemainingDuration().toVLCTick(), VLC_TICK_FROM_SEC(60));

        m_controller->goTo(2);
        QTRY_COMPARE(m_model->getRemainingCount(), 1);
        QCOMPARE(m_model->getRemainingDuration().toVLCTick(), VLC_TICK_FROM_SEC(30));
    }

    // Sort, Repeat

    void testSortOffersNameDurationAndDateAdded()
    {
        QList<int> keys;
        for (const QVariant &entry : m_controller->getSortKeyTitleList())
            keys.push_back(entry.toMap().value("criteria").toInt());

        QCOMPARE(keys, QList<int>({PlaylistController::SORT_KEY_TITLE,
                                   PlaylistController::SORT_KEY_DURATION,
                                   PlaylistController::SORT_KEY_DATE_ADDED}));
    }

    void testSortByNameSortsOnce()
    {
        fill({"b", "c", "a"});

        m_controller->sort(PlaylistController::SORT_KEY_TITLE);

        QTRY_COMPARE(rows(), QStringList({"a", "b", "c"}));
        /* the Queue is not kept sorted */
        m_controller->append(medias({"0"}), false);
        QTRY_COMPARE(rows(), QStringList({"a", "b", "c", "0"}));
        QTRY_COMPARE(m_controller->getSortKey(), PlaylistController::SORT_KEY_NONE);
    }

    void testSortByDateAddedRestoresTheOrderItemsWereAdded()
    {
        fill({"b", "c", "a"});
        m_controller->sort(PlaylistController::SORT_KEY_TITLE);
        QTRY_COMPARE(rows(), QStringList({"a", "b", "c"}));

        m_controller->sort(PlaylistController::SORT_KEY_DATE_ADDED,
                           PlaylistController::SORT_ORDER_ASC);
        QTRY_COMPARE(rows(), QStringList({"b", "c", "a"}));

        m_controller->sort(PlaylistController::SORT_KEY_DATE_ADDED,
                           PlaylistController::SORT_ORDER_DESC);
        QTRY_COMPARE(rows(), QStringList({"a", "c", "b"}));
    }

    void testSortKeepsTheCurrentItem()
    {
        fill({"b", "c", "a"}, 1);

        m_controller->sort(PlaylistController::SORT_KEY_DATE_ADDED,
                           PlaylistController::SORT_ORDER_DESC);

        QTRY_COMPARE(rows(), QStringList({"a", "c", "b"}));
        QTRY_COMPARE(m_model->getCurrentIndex(), 1);
    }

    void testRepeatCyclesOffAllOne()
    {
        QCOMPARE(m_controller->getRepeatMode(), PlaylistController::PLAYBACK_REPEAT_NONE);

        m_controller->toggleRepeatMode();
        QTRY_COMPARE(m_controller->getRepeatMode(), PlaylistController::PLAYBACK_REPEAT_ALL);
        m_controller->toggleRepeatMode();
        QTRY_COMPARE(m_controller->getRepeatMode(), PlaylistController::PLAYBACK_REPEAT_CURRENT);
        m_controller->toggleRepeatMode();
        QTRY_COMPARE(m_controller->getRepeatMode(), PlaylistController::PLAYBACK_REPEAT_NONE);
    }

    // Keyboard moves (Alt+Up / Alt+Down)

    void testMoveUp()
    {
        fill({"a", "b", "c", "d"});

        m_model->shiftItems({2}, -1);

        QTRY_COMPARE(rows(), QStringList({"a", "c", "b", "d"}));
    }

    void testMoveDownKeepsTheSelectionTogether()
    {
        fill({"a", "b", "c", "d"});

        m_model->shiftItems({1, 2}, 1);

        QTRY_COMPARE(rows(), QStringList({"a", "d", "b", "c"}));
    }

    void testMoveStopsAtTheEnds()
    {
        fill({"a", "b", "c"});

        m_model->shiftItems({0}, -1);
        m_model->shiftItems({2}, 1);
        m_model->shiftItems({1}, 1);

        QTRY_COMPARE(rows(), QStringList({"a", "c", "b"}));
    }

    // Row menu: Play next

    void testPlayNextMovesItemsAfterTheCurrentOne()
    {
        fill({"a", "b", "c", "d", "e"}, 1);

        m_model->moveItemsAfterCurrent({3, 4});

        QTRY_COMPARE(rows(), QStringList({"a", "b", "d", "e", "c"}));
        QCOMPARE(m_model->getCurrentIndex(), 1);
    }

    void testPlayNextOnAPlayedItem()
    {
        fill({"a", "b", "c"}, 1);

        m_model->moveItemsAfterCurrent({0});

        QTRY_COMPARE(rows(), QStringList({"b", "a", "c"}));
        QTRY_COMPARE(m_model->getCurrentIndex(), 0);
    }

    void testPlayNextWithoutCurrentItemMovesFirst()
    {
        fill({"a", "b", "c"});

        m_model->moveItemsAfterCurrent({2});

        QTRY_COMPARE(rows(), QStringList({"c", "a", "b"}));
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<PlaylistController> m_controller;
    std::unique_ptr<PlaylistListModel> m_model;
};

QTEST_GUILESS_MAIN(TestQueuePanel)
#include "test_queue_panel.moc"
