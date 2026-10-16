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

/* The models behind the Track menu: the track lists with their embedded and
 * external tracks and the second subtitle, and the audio output devices. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_stub_modules.hpp"

#include <vlc_aout.h>

#include "../player/player_controller.hpp"
#include "../player/input_models.hpp"
#include "../util/audio_device_model.hpp"

#include <memory>

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

/* Feeds a TrackListModel from the player, on the Qt thread, like
 * PlayerController does */
class TrackFeeder
{
public:
    TrackFeeder(vlc_player_t *player, es_format_category_e cat, TrackListModel *model)
        : m_player(player), m_cat(cat), m_model(model)
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
    es_format_category_e m_cat;
    TrackListModel *m_model;
    vlc_player_listener_id *m_listener = nullptr;
};

class TestTrackModels : public QObject
{
    Q_OBJECT

private:
    vlc_player_t *player() const
    {
        return m_env->intf->p_player;
    }

    bool checked(int row) const
    {
        return m_subtitles->data(m_subtitles->index(row), Qt::CheckStateRole).toBool();
    }

    bool external(int row) const
    {
        return m_subtitles->data(m_subtitles->index(row), TrackListModel::ExternalRole).toBool();
    }

    bool secondary(int row) const
    {
        return m_subtitles->data(m_subtitles->index(row), TrackListModel::SecondaryRole).toBool();
    }

    QString name(int row) const
    {
        return m_subtitles->data(m_subtitles->index(row), Qt::DisplayRole).toString();
    }

    QString selectedNames() const
    {
        QStringList names;
        for (int i = 0; i < m_subtitles->rowCount(); ++i)
            if (checked(i))
                names.push_back(name(i));
        return names.join(',');
    }

    int rowOf(const QString &trackName) const
    {
        for (int i = 0; i < m_subtitles->rowCount(); ++i)
            if (name(i) == trackName)
                return i;
        return -1;
    }

    QString deviceNames() const
    {
        QStringList names;
        for (int i = 0; i < m_devices->rowCount(); ++i)
            names.push_back(m_devices->data(m_devices->index(i), Qt::DisplayRole).toString());
        return names.join(',');
    }

    QString checkedDevice() const
    {
        for (int i = 0; i < m_devices->rowCount(); ++i)
            if (m_devices->data(m_devices->index(i), Qt::CheckStateRole).toBool())
                return m_devices->data(m_devices->index(i), Qt::DisplayRole).toString();
        return {};
    }

private slots:
    void initTestCase()
    {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());

        m_subtitles = std::make_unique<TrackListModel>(player());
        m_feeder = std::make_unique<TrackFeeder>(player(), SPU_ES, m_subtitles.get());

        /* a media with two embedded subtitle tracks */
        input_item_t *media = input_item_New("stubtracks://2", "stub tracks");
        QVERIFY(media);
        {
            vlc_player_locker locker{ player() };
            QCOMPARE(vlc_player_SetCurrentMedia(player(), media), VLC_SUCCESS);
            QCOMPARE(vlc_player_Start(player()), VLC_SUCCESS);
        }
        input_item_Release(media);

        QTRY_COMPARE(m_subtitles->getCount(), 2);

        /* and an external subtitle file */
        QVERIFY(m_dir.isValid());
        const QString path = m_dir.filePath("external.srt");
        QFile srt(path);
        QVERIFY(srt.open(QIODevice::WriteOnly));
        srt.write("1\n00:00:00,000 --> 00:10:00,000\nexternal line\n\n");
        srt.close();
        const QByteArray uri = QUrl::fromLocalFile(path).toEncoded();
        {
            vlc_player_locker locker{ player() };
            QCOMPARE(vlc_player_AddAssociatedMedia(player(), SPU_ES, uri.constData(),
                                                   true, false, false), VLC_SUCCESS);
        }

        QTRY_COMPARE(m_subtitles->getCount(), 3);
        m_externalName = name(2);
    }

    void testEmbeddedAndExternalTracks()
    {
        QCOMPARE(external(0), false);
        QCOMPARE(external(1), false);
        QCOMPARE(external(2), true);
        QCOMPARE(m_subtitles->roleNames().value(TrackListModel::ExternalRole),
                 QByteArray("external"));
    }

    void testSelectIsExclusive()
    {
        m_subtitles->select(0);
        QTRY_COMPARE(selectedNames(), name(0));
        QVERIFY(m_subtitles->hasSelection());

        m_subtitles->select(1);
        QTRY_COMPARE(selectedNames(), name(1));
    }

    void testNoneUnselectsEverything()
    {
        m_subtitles->select(0);
        QTRY_COMPARE(selectedNames(), name(0));

        QSignalSpy spy(m_subtitles.get(), &TrackListModel::hasSelectionChanged);
        m_subtitles->unselectAll();
        QTRY_COMPARE(selectedNames(), QString());
        QVERIFY(!m_subtitles->hasSelection());
        QVERIFY(spy.count() > 0);
    }

    void testShowAsSecondSubtitle()
    {
        m_subtitles->select(0);
        QTRY_COMPARE(selectedNames(), name(0));

        m_subtitles->selectSecondary(1);
        QTRY_COMPARE(selectedNames(), name(0) + ',' + name(1));
        QVERIFY(!secondary(0));
        QVERIFY(secondary(1));

        /* picking another track keeps the second subtitle */
        m_subtitles->select(2);
        QTRY_COMPARE(selectedNames(), name(1) + ',' + name(2));
        QVERIFY(secondary(1));
        QVERIFY(!secondary(2));

        /* a new second subtitle replaces the previous one */
        m_subtitles->selectSecondary(0);
        QTRY_COMPARE(selectedNames(), name(0) + ',' + name(2));
        QVERIFY(secondary(0));
        QVERIFY(!secondary(1));

        /* hiding the second subtitle keeps the first one */
        m_subtitles->unselectSecondary();
        QTRY_COMPARE(selectedNames(), name(2));
        QVERIFY(!secondary(0));
    }

    void testSecondSubtitleNeedsAFirstOne()
    {
        m_subtitles->unselectAll();
        QTRY_COMPARE(selectedNames(), QString());

        /* with nothing shown, the track simply becomes the subtitle */
        m_subtitles->selectSecondary(1);
        QTRY_COMPARE(selectedNames(), name(1));
        QVERIFY(!secondary(1));
    }

    void testRemoveExternalTrack()
    {
        /* embedded tracks can't be removed */
        QVERIFY(!m_subtitles->remove(0));
        QCOMPARE(m_subtitles->getCount(), 3);

        const int row = rowOf(m_externalName);
        QVERIFY(row >= 0);
        m_subtitles->select(row);
        QTRY_VERIFY(checked(row));

        QSignalSpy spy(m_subtitles.get(), &TrackListModel::externalTrackRemoved);
        QVERIFY(m_subtitles->remove(row));
        QCOMPARE(m_subtitles->getCount(), 2);
        QCOMPARE(rowOf(m_externalName), -1);
        QCOMPARE(spy.count(), 1);
        QTRY_VERIFY(!m_subtitles->hasSelection());

        /* and it doesn't come back with a later update */
        m_subtitles->select(0);
        QTRY_COMPARE(selectedNames(), name(0));
        QCOMPARE(m_subtitles->getCount(), 2);
    }

    void testAudioDevicesRefreshAndSwitch()
    {
        m_devices = std::make_unique<AudioDeviceModel>(player());
        QCOMPARE(deviceNames(), QStringLiteral("Speakers,Headphones"));

        /* a device plugged in later shows up on refresh */
        QVERIFY(m_env->aout);
        aout_HotplugReport(m_env->aout, "stub-usb", "USB headset");
        QCOMPARE(deviceNames(), QStringLiteral("Speakers,Headphones"));
        QSignalSpy reset(m_devices.get(), &QAbstractItemModel::modelReset);
        m_devices->refresh();
        QCOMPARE(reset.count(), 1);
        QCOMPARE(deviceNames(), QStringLiteral("Speakers,Headphones,USB headset"));

        /* picking a device switches the output live */
        QVERIFY(m_devices->setData(m_devices->index(1), true, Qt::CheckStateRole));
        QTRY_COMPARE(checkedDevice(), QStringLiteral("Headphones"));
        QCOMPARE(m_env->aoutDevice, QStringLiteral("stub-headphones"));

        QVERIFY(m_devices->setData(m_devices->index(2), true, Qt::CheckStateRole));
        QTRY_COMPARE(checkedDevice(), QStringLiteral("USB headset"));

        /* the current device stays checked across a refresh */
        m_devices->refresh();
        QCOMPARE(checkedDevice(), QStringLiteral("USB headset"));
    }

    /* Styled ASS subtitles draw themselves: the Subtitles tab disables its
     * Appearance while one is shown (ticket #53) */
    void testStyledSubtitles()
    {
        const int count = m_subtitles->getCount();
        const QString path = m_dir.filePath("styled.ass");
        QFile ass(path);
        QVERIFY(ass.open(QIODevice::WriteOnly));
        ass.write("[Script Info]\nScriptType: v4.00+\n\n"
                  "[V4+ Styles]\n"
                  "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
                  "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
                  "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
                  "Alignment, MarginL, MarginR, MarginV, Encoding\n"
                  "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,"
                  "&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n\n"
                  "[Events]\n"
                  "Format: Layer, Start, End, Style, Name, MarginL, MarginR, "
                  "MarginV, Effect, Text\n"
                  "Dialogue: 0,0:00:00.00,0:10:00.00,Default,,0,0,0,,styled line\n");
        ass.close();
        const QByteArray uri = QUrl::fromLocalFile(path).toEncoded();
        {
            vlc_player_locker locker{ player() };
            QCOMPARE(vlc_player_AddAssociatedMedia(player(), SPU_ES, uri.constData(),
                                                   false, false, false), VLC_SUCCESS);
        }
        QTRY_COMPARE(m_subtitles->getCount(), count + 1);
        const int styledRow = count;

        QCOMPARE(m_subtitles->roleNames().value(TrackListModel::StyledRole),
                 QByteArray("styled"));
        QVERIFY(m_subtitles->data(m_subtitles->index(styledRow), TrackListModel::StyledRole).toBool());
        QVERIFY(!m_subtitles->data(m_subtitles->index(0), TrackListModel::StyledRole).toBool());

        m_subtitles->select(0);
        QTRY_COMPARE(selectedNames(), name(0));
        QVERIFY(!m_subtitles->styledSelected());

        QSignalSpy spy(m_subtitles.get(), &TrackListModel::styledSelectedChanged);
        m_subtitles->select(styledRow);
        QTRY_VERIFY(m_subtitles->styledSelected());
        QVERIFY(spy.count() > 0);

        m_subtitles->unselectAll();
        QTRY_VERIFY(!m_subtitles->styledSelected());
    }

    /* The seek bar counts from the start time (":start-time"): the chapter
     * marks are placed within the part played, and chapters outside it get
     * none */
    void testChapterPositionsInClip()
    {
        static const vlc_player_chapter chapters[] = {
            { "One", VLC_TICK_FROM_SEC(0) },
            { "Two", VLC_TICK_FROM_SEC(20) },
            { "Three", VLC_TICK_FROM_SEC(50) },
            { "Four", VLC_TICK_FROM_SEC(80) },
        };
        static const vlc_player_title title = {
            "title", VLC_TICK_FROM_SEC(100), 0, 4, chapters
        };

        ChapterListModel model(player());
        model.resetTitle(&title);

        const auto start = [&model](int row) {
            return model.data(model.index(row), ChapterListModel::StartPositionRole).toFloat();
        };
        const auto end = [&model](int row) {
            return model.data(model.index(row), ChapterListModel::EndPositionRole).toFloat();
        };

        /* the whole media */
        QCOMPARE(start(2), 0.5f);
        QCOMPARE(end(2), 0.8f);
        QCOMPARE(model.getNameAtPosition(0.6f), QStringLiteral("Three"));
        QCOMPARE(model.getClosestChapterFromPos(0.5f, 0.01f), 2);

        /* played from 30 s to 70 s */
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setClip(VLC_TICK_FROM_SEC(30), VLC_TICK_FROM_SEC(40));
        QVERIFY(changed.count() > 0);

        /* before the part played: no mark */
        QCOMPARE(start(0), 0.f);
        QCOMPARE(end(0), 0.f);
        /* the chapter going on at the start time begins with the bar */
        QCOMPARE(start(1), 0.f);
        QCOMPARE(end(1), 0.5f);
        /* the one going on at the end goes to the end of the bar */
        QCOMPARE(start(2), 0.5f);
        QCOMPARE(end(2), 1.f);
        /* after the part played: no mark */
        QCOMPARE(start(3), 1.f);
        QCOMPARE(end(3), 1.f);

        QCOMPARE(model.getNameAtPosition(0.f), QStringLiteral("Two"));
        QCOMPARE(model.getNameAtPosition(0.6f), QStringLiteral("Three"));
        QCOMPARE(model.getClosestChapterFromPos(0.5f, 0.01f), 2);
        /* a press at the start of the bar must not seek back before it, nor
         * one at its end past it */
        QCOMPARE(model.getClosestChapterFromPos(0.f, 0.05f), -1);
        QCOMPARE(model.getClosestChapterFromPos(1.f, 0.3f), -1);

        /* played whole again */
        model.setClip(0, VLC_TICK_INVALID);
        QCOMPARE(start(2), 0.5f);
        QCOMPARE(end(2), 0.8f);
    }

    /* The Queue panel's Chapters list counts from the start time too, as the
     * seek bar does: the chapters it has no room for are out of the clip */
    void testChapterTimesInClip()
    {
        static const vlc_player_chapter chapters[] = {
            { "One", VLC_TICK_FROM_SEC(0) },
            { "Two", VLC_TICK_FROM_SEC(20) },
            { "Three", VLC_TICK_FROM_SEC(50) },
            { "Four", VLC_TICK_FROM_SEC(80) },
        };
        static const vlc_player_title title = {
            "title", VLC_TICK_FROM_SEC(100), 0, 4, chapters
        };

        ChapterListModel model(player());
        model.resetTitle(&title);

        const auto time = [&model](int row) {
            const QVariant v = model.data(model.index(row), ChapterListModel::TimeRole);
            return v.isValid() ? v.value<VLCDuration>().toVLCTick() : VLC_TICK_INVALID;
        };
        const auto inClip = [&model](int row) {
            return model.data(model.index(row), ChapterListModel::InClipRole).toBool();
        };
        QVERIFY(model.roleNames().values().contains("inClip"));

        /* the whole media */
        QCOMPARE(time(2), VLC_TICK_FROM_SEC(50));
        for (int row = 0; row < 4; ++row)
            QVERIFY(inClip(row));

        /* played from 30 s to 70 s */
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setClip(VLC_TICK_FROM_SEC(30), VLC_TICK_FROM_SEC(40));
        QVERIFY(changed.count() > 0);
        QVERIFY(changed.last().at(2).value<QList<int>>().contains(ChapterListModel::TimeRole));

        /* over before the start time: no time */
        QVERIFY(!inClip(0));
        QCOMPARE(time(0), VLC_TICK_INVALID);
        /* going on at the start time: it begins with the clip */
        QVERIFY(inClip(1));
        QCOMPARE(time(1), VLC_TICK_FROM_SEC(0));
        QVERIFY(inClip(2));
        QCOMPARE(time(2), VLC_TICK_FROM_SEC(20));
        /* after the part played: no time */
        QVERIFY(!inClip(3));
        QCOMPARE(time(3), VLC_TICK_INVALID);

        /* played whole again */
        model.setClip(0, VLC_TICK_INVALID);
        QVERIFY(inClip(0));
        QCOMPARE(time(3), VLC_TICK_FROM_SEC(80));
    }

    void cleanupTestCase()
    {
        {
            vlc_player_locker locker{ player() };
            vlc_player_Stop(player());
        }
        m_devices.reset();
        m_feeder.reset();
        m_subtitles.reset();
        m_env.reset();
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<TrackListModel> m_subtitles;
    std::unique_ptr<TrackFeeder> m_feeder;
    std::unique_ptr<AudioDeviceModel> m_devices;
    QTemporaryDir m_dir;
    QString m_externalName;
};

QTEST_GUILESS_MAIN(TestTrackModels)
#include "test_track_models.moc"
