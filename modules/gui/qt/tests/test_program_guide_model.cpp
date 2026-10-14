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
#include "vlc_stub_modules.hpp"

#include <QTest>
#include <QSignalSpy>
#include <QDateTime>

#include <vlc_input_item.h>
#include <vlc_epg.h>
#include <vlc_arrays.h>

#include "dialogs/epg/programguidemodel.hpp"

namespace {

// 2026-10-13 20:00:00 UTC
constexpr int64_t T0 = 1791921600;

struct Event
{
    uint16_t id;
    int64_t start;
    uint32_t duration;
    const char *name;
};

vlc_epg_t *makeTable(uint32_t id, uint16_t sourceId, const char *name,
                     std::initializer_list<Event> events)
{
    vlc_epg_t *epg = vlc_epg_New(id, sourceId);
    if (name)
        epg->psz_name = strdup(name);
    for (const Event &e : events)
    {
        vlc_epg_event_t *evt = vlc_epg_event_New(e.id, e.start, e.duration);
        evt->psz_name = strdup(e.name);
        vlc_epg_AddEvent(epg, evt);
    }
    return epg;
}

void addTable(input_item_t *item, vlc_epg_t *epg)
{
    vlc_mutex_lock(&item->lock);
    TAB_APPEND_CAST((vlc_epg_t **), item->i_epg, item->pp_epg, epg);
    vlc_mutex_unlock(&item->lock);
}

}

class TestProgramGuideModel : public QObject
{
    Q_OBJECT

private:
    QStringList column(int role) const
    {
        QStringList values;
        for (int i = 0; i < m_model->rowCount(); ++i)
            values << m_model->data(m_model->index(i), role).toString();
        return values;
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
        m_item = input_item_New("dvb-t://frequency=474000000", "TV");
        QVERIFY(m_item);
        m_model = std::make_unique<ProgramGuideModel>();
    }

    void cleanup()
    {
        m_model.reset();
        input_item_Release(m_item);
        m_item = nullptr;
    }

    void testEmptyWithoutGuide()
    {
        m_model->update(m_item);
        QCOMPARE(m_model->rowCount(), 0);

        m_model->update(nullptr);
        QCOMPARE(m_model->rowCount(), 0);
    }

    void testListsEventsByChannelThenTime()
    {
        addTable(m_item, makeTable(1, 10, "Alpha", {
            { 2, T0 + 3600, 1800, "Late news" },
            { 1, T0, 3600, "Movie" },
        }));
        addTable(m_item, makeTable(2, 20, "Beta", {
            { 1, T0 + 600, 600, "Cartoon" },
        }));

        m_model->update(m_item);

        QCOMPARE(column(ProgramGuideModel::NAME), QStringList({ "Movie", "Late news", "Cartoon" }));
        QCOMPARE(column(ProgramGuideModel::CHANNEL), QStringList({ "Alpha", "Alpha", "Beta" }));
        QCOMPARE(column(ProgramGuideModel::PROGRAM_ID), QStringList({ "10", "10", "20" }));

        const QModelIndex late = m_model->index(1);
        QCOMPARE(m_model->data(late, ProgramGuideModel::START).toDateTime().toSecsSinceEpoch(), T0 + 3600);
        QCOMPARE(m_model->data(late, ProgramGuideModel::END).toDateTime().toSecsSinceEpoch(), T0 + 5400);
    }

    void testMergesTablesOfOneChannel()
    {
        addTable(m_item, makeTable(1, 10, "Alpha", { { 1, T0, 600, "First" } }));
        addTable(m_item, makeTable(2, 10, nullptr, { { 2, T0 + 600, 600, "Second" } }));

        m_model->update(m_item);

        QCOMPARE(column(ProgramGuideModel::NAME), QStringList({ "First", "Second" }));
        QCOMPARE(column(ProgramGuideModel::CHANNEL), QStringList({ "Alpha", "Alpha" }));
    }

    void testMarksCurrentEvent()
    {
        vlc_epg_t *epg = makeTable(1, 10, "Alpha", {
            { 1, T0, 600, "Before" },
            { 2, T0 + 600, 600, "Now" },
        });
        vlc_epg_SetCurrent(epg, T0 + 600);
        addTable(m_item, epg);

        m_model->update(m_item);

        QCOMPARE(column(ProgramGuideModel::CURRENT), QStringList({ "false", "true" }));
    }

    void testDropsEndedEvents()
    {
        addTable(m_item, makeTable(1, 10, "Alpha", {
            { 1, T0, 600, "Ended" },
            { 2, T0 + 600, 600, "Running" },
        }));
        vlc_mutex_lock(&m_item->lock);
        m_item->i_epg_time = T0 + 900;
        vlc_mutex_unlock(&m_item->lock);

        m_model->update(m_item);

        QCOMPARE(column(ProgramGuideModel::NAME), QStringList({ "Running" }));
    }

    void testUnchangedGuideKeepsRows()
    {
        addTable(m_item, makeTable(1, 10, "Alpha", { { 1, T0, 600, "Movie" } }));
        m_model->update(m_item);

        QSignalSpy resetSpy(m_model.get(), &QAbstractItemModel::modelReset);
        m_model->update(m_item);
        QCOMPARE(resetSpy.count(), 0);
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<ProgramGuideModel> m_model;
    input_item_t *m_item = nullptr;
};

QTEST_GUILESS_MAIN(TestProgramGuideModel)
#include "test_program_guide_model.moc"
