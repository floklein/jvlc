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

#include <vlc_common.h>
#include <vlc_input_item.h>

#include <QTest>
#include <QSignalSpy>

#include "../player/sleep_timer.hpp"
#include "../player/playback_statistics.hpp"

// The Playback tab of the Adjustments sidebar (spec #30, ticket #50): its
// sleep timer and the statistics overlay
class TestPlaybackTab : public QObject
{
    Q_OBJECT

    static constexpr qint64 MINUTE = 60 * 1000;

    qint64 m_now = 0;
    std::unique_ptr<SleepTimer> m_timer;

    // time passes, the timer looks at the clock
    void advance(qint64 ms)
    {
        m_now += ms;
        m_timer->tick();
    }

private slots:
    void init()
    {
        m_now = 1000;
        m_timer = std::make_unique<SleepTimer>( nullptr, [this]() { return m_now; } );
    }

    void cleanup()
    {
        m_timer.reset();
    }

    void offAtFirst()
    {
        QCOMPARE( m_timer->mode(), SleepTimer::Off );
        QCOMPARE( m_timer->pauseAtEndOfItem(), false );
    }

    void aDurationCountsDownThenPausesOnce()
    {
        QSignalSpy pause( m_timer.get(), &SleepTimer::pauseRequested );

        m_timer->start( 15 );
        QCOMPARE( m_timer->mode(), SleepTimer::Duration );
        QCOMPARE( m_timer->minutes(), 15 );
        QCOMPARE( m_timer->timeLeft(), 15 * MINUTE );

        advance( 5 * MINUTE );
        QCOMPARE( m_timer->timeLeft(), 10 * MINUTE );
        QCOMPARE( pause.count(), 0 );

        advance( 10 * MINUTE );
        QCOMPARE( pause.count(), 1 );
        QCOMPARE( m_timer->mode(), SleepTimer::Off );

        advance( 15 * MINUTE );
        QCOMPARE( pause.count(), 1 );
    }

    void offCancelsTheTimer()
    {
        QSignalSpy pause( m_timer.get(), &SleepTimer::pauseRequested );

        m_timer->start( 30 );
        m_timer->cancel();
        advance( 31 * MINUTE );

        QCOMPARE( m_timer->mode(), SleepTimer::Off );
        QCOMPARE( pause.count(), 0 );
    }

    void endOfItemLetsThePlayerPauseAtTheEnd()
    {
        QSignalSpy pause( m_timer.get(), &SleepTimer::pauseRequested );
        m_timer->setItemTimeLeft( 4 * MINUTE );

        m_timer->startEndOfItem();
        QCOMPARE( m_timer->mode(), SleepTimer::EndOfItem );
        QCOMPARE( m_timer->pauseAtEndOfItem(), true );
        QCOMPARE( m_timer->timeLeft(), 4 * MINUTE );

        // the player paused itself on the last frame
        m_timer->setItemTimeLeft( 0 );
        m_timer->setPaused( true );

        QCOMPARE( m_timer->mode(), SleepTimer::Off );
        QCOMPARE( m_timer->pauseAtEndOfItem(), false );
        QCOMPARE( pause.count(), 0 );
    }

    void aPauseBeforeTheEndKeepsTheTimer()
    {
        m_timer->setItemTimeLeft( 4 * MINUTE );
        m_timer->startEndOfItem();

        m_timer->setPaused( true );
        m_timer->setPaused( false );

        QCOMPARE( m_timer->mode(), SleepTimer::EndOfItem );
        QCOMPARE( m_timer->pauseAtEndOfItem(), true );
    }

    void endOfQueuePausesAtTheEndOfTheLastItemOnly()
    {
        m_timer->setLastItem( false );
        m_timer->setItemTimeLeft( 3 * MINUTE );
        m_timer->setQueueTimeLeft( 20 * MINUTE );

        m_timer->startEndOfQueue();
        QCOMPARE( m_timer->mode(), SleepTimer::EndOfQueue );
        QCOMPARE( m_timer->pauseAtEndOfItem(), false );
        QCOMPARE( m_timer->timeLeft(), 23 * MINUTE );

        // the item before the last one ended
        m_timer->setItemTimeLeft( 0 );
        m_timer->setPaused( true );
        QCOMPARE( m_timer->mode(), SleepTimer::EndOfQueue );

        m_timer->setPaused( false );
        m_timer->setLastItem( true );
        m_timer->setQueueTimeLeft( 0 );
        m_timer->setItemTimeLeft( 20 * MINUTE );
        QCOMPARE( m_timer->pauseAtEndOfItem(), true );
        QCOMPARE( m_timer->timeLeft(), 20 * MINUTE );

        m_timer->setItemTimeLeft( 0 );
        m_timer->setPaused( true );
        QCOMPARE( m_timer->mode(), SleepTimer::Off );
        QCOMPARE( m_timer->pauseAtEndOfItem(), false );
    }

    void timeLeftIsUnknownWithAStreamInTheQueue()
    {
        m_timer->setItemTimeLeft( 3 * MINUTE );
        m_timer->setQueueTimeLeft( -1 );

        m_timer->startEndOfQueue();

        QCOMPARE( m_timer->timeLeft(), -1 );
    }

    void aNewChoiceReplacesThePreviousOne()
    {
        QSignalSpy pause( m_timer.get(), &SleepTimer::pauseRequested );
        m_timer->setItemTimeLeft( 4 * MINUTE );
        m_timer->startEndOfItem();

        m_timer->start( 45 );

        QCOMPARE( m_timer->mode(), SleepTimer::Duration );
        QCOMPARE( m_timer->pauseAtEndOfItem(), false );

        advance( 45 * MINUTE );
        QCOMPARE( pause.count(), 1 );
    }

    void timeLeftIsNotifiedWhileCountingDown()
    {
        m_timer->start( 60 );
        QSignalSpy changed( m_timer.get(), &SleepTimer::timeLeftChanged );

        advance( 1000 );

        QCOMPARE( changed.count(), 1 );
        QCOMPARE( m_timer->timeLeft(), 60 * MINUTE - 1000 );
    }

    void statisticsShowBitrateAndDroppedFrames()
    {
        PlaybackStatistics statistics;
        QSignalSpy changed( &statistics, &PlaybackStatistics::changed );

        input_stats_t stats = {};
        /* bitrates are in bytes per microsecond: 0.125 B/us = 1000 kb/s */
        stats.f_input_bitrate = 0.125f;
        stats.f_demux_bitrate = 0.0625f;
        stats.i_displayed_pictures = 1200;
        stats.i_lost_pictures = 7;
        stats.i_late_pictures = 3;
        stats.i_lost_abuffers = 2;
        statistics.update( stats );

        QCOMPARE( changed.count(), 1 );
        QCOMPARE( qRound( statistics.inputBitrate() ), 1000 );
        QCOMPARE( qRound( statistics.contentBitrate() ), 500 );
        QCOMPARE( statistics.displayedFrames(), 1200 );
        QCOMPARE( statistics.droppedFrames(), 7 );
        QCOMPARE( statistics.lateFrames(), 3 );
        QCOMPARE( statistics.lostAudioBuffers(), 2 );

        statistics.reset();
        QCOMPARE( qRound( statistics.inputBitrate() ), 0 );
        QCOMPARE( statistics.droppedFrames(), 0 );
    }
};

QTEST_GUILESS_MAIN(TestPlaybackTab)
#include "test_playback_tab.moc"
