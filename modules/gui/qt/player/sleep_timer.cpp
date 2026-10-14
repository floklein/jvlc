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
#include "sleep_timer.hpp"

#include <QElapsedTimer>

#include <algorithm>
#include <memory>

/* the player paused on the last frame of the item, not before */
static constexpr qint64 END_OF_ITEM_MARGIN = 1000;

static SleepTimer::Clock monotonicClock()
{
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    return [elapsed]() { return elapsed->elapsed(); };
}

SleepTimer::SleepTimer( QObject* parent, Clock clock )
    : QObject( parent )
    , m_clock( clock ? std::move( clock ) : monotonicClock() )
{
    /* a QTimer, not a QML one: it keeps running while the window is hidden */
    m_ticker.setInterval( 1000 );
    connect( &m_ticker, &QTimer::timeout, this, &SleepTimer::tick );
}

qint64 SleepTimer::timeLeft() const
{
    switch ( m_mode )
    {
    case Duration:
        return std::max<qint64>( 0, m_deadline - m_clock() );
    case EndOfItem:
        return m_itemTimeLeft;
    case EndOfQueue:
        if ( m_itemTimeLeft < 0 )
            return -1;
        if ( m_lastItem )
            return m_itemTimeLeft;
        if ( m_queueTimeLeft < 0 )
            return -1;
        return m_itemTimeLeft + m_queueTimeLeft;
    case Off:
    default:
        return -1;
    }
}

bool SleepTimer::pauseAtEndOfItem() const
{
    return m_mode == EndOfItem || ( m_mode == EndOfQueue && m_lastItem );
}

void SleepTimer::start( int minutes )
{
    if ( minutes <= 0 )
    {
        cancel();
        return;
    }
    m_deadline = m_clock() + minutes * qint64( 60 * 1000 );
    setMode( Duration, minutes );
}

void SleepTimer::startEndOfItem()
{
    setMode( EndOfItem );
}

void SleepTimer::startEndOfQueue()
{
    setMode( EndOfQueue );
}

void SleepTimer::cancel()
{
    setMode( Off );
}

void SleepTimer::setItemTimeLeft( qint64 ms )
{
    m_itemTimeLeft = ms;
    notifyChanges();
}

void SleepTimer::setQueueTimeLeft( qint64 ms )
{
    m_queueTimeLeft = ms;
    notifyChanges();
}

void SleepTimer::setLastItem( bool last )
{
    m_lastItem = last;
    notifyChanges();
}

void SleepTimer::setPaused( bool paused )
{
    /* the player paused itself at the end of the item: the timer is done */
    if ( paused && pauseAtEndOfItem()
         && m_itemTimeLeft >= 0 && m_itemTimeLeft <= END_OF_ITEM_MARGIN )
        setMode( Off );
}

void SleepTimer::tick()
{
    if ( m_mode == Duration && m_clock() >= m_deadline )
    {
        setMode( Off );
        emit pauseRequested();
        return;
    }
    notifyChanges();
}

void SleepTimer::setMode( Mode mode, int minutes )
{
    const bool changed = m_mode != mode || m_minutes != minutes;
    m_mode = mode;
    m_minutes = minutes;

    if ( m_mode == Duration )
        m_ticker.start();
    else
        m_ticker.stop();

    if ( changed )
        emit modeChanged();
    notifyChanges();
}

void SleepTimer::notifyChanges()
{
    const qint64 left = timeLeft();
    if ( left != m_notifiedTimeLeft )
    {
        m_notifiedTimeLeft = left;
        emit timeLeftChanged();
    }

    const bool pauseAtEnd = pauseAtEndOfItem();
    if ( pauseAtEnd != m_notifiedPauseAtEnd )
    {
        m_notifiedPauseAtEnd = pauseAtEnd;
        emit pauseAtEndOfItemChanged();
    }
}
