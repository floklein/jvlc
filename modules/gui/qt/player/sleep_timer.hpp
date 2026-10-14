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
#ifndef QVLC_SLEEP_TIMER_HPP
#define QVLC_SLEEP_TIMER_HPP

#include <QObject>
#include <QTimer>

#include <functional>

/**
 * The sleep timer of the Playback tab: pauses the playback after a duration,
 * at the end of the current item or at the end of the Queue.
 *
 * It knows nothing of the player: the player feeds it the time left in the
 * current item and in the rest of the Queue, and whether it is paused. A
 * duration ends with pauseRequested(). The end of an item is left to the
 * player, which pauses on the last frame while pauseAtEndOfItem is set; the
 * timer turns off when the player pauses there.
 */
class SleepTimer : public QObject
{
    Q_OBJECT

public:
    enum Mode
    {
        Off,
        Duration,
        EndOfItem,
        EndOfQueue
    };
    Q_ENUM(Mode)

    Q_PROPERTY(Mode mode READ mode NOTIFY modeChanged FINAL)
    // the chosen duration, in Duration mode
    Q_PROPERTY(int minutes READ minutes NOTIFY modeChanged FINAL)
    // milliseconds before the pause, -1 when unknown or off
    Q_PROPERTY(qint64 timeLeft READ timeLeft NOTIFY timeLeftChanged FINAL)
    // the player must pause when the current item ends
    Q_PROPERTY(bool pauseAtEndOfItem READ pauseAtEndOfItem NOTIFY pauseAtEndOfItemChanged FINAL)

    /// a monotonic clock in milliseconds
    using Clock = std::function<qint64()>;

    explicit SleepTimer( QObject* parent = nullptr, Clock clock = {} );

    Mode mode() const { return m_mode; }
    int minutes() const { return m_minutes; }
    qint64 timeLeft() const;
    bool pauseAtEndOfItem() const;

    Q_INVOKABLE void start( int minutes );
    Q_INVOKABLE void startEndOfItem();
    Q_INVOKABLE void startEndOfQueue();
    Q_INVOKABLE void cancel();

    /* from the player */
    /// real time left in the current item, -1 when unknown (live streams)
    void setItemTimeLeft( qint64 ms );
    /// real time of the items after the current one, -1 when unknown
    void setQueueTimeLeft( qint64 ms );
    /// the current item is the last of the Queue
    void setLastItem( bool last );
    void setPaused( bool paused );

    /// looks at the clock: called every second while a duration runs
    void tick();

signals:
    void modeChanged();
    void timeLeftChanged();
    void pauseAtEndOfItemChanged();
    /// the duration is over: pause now
    void pauseRequested();

private:
    void setMode( Mode mode, int minutes = 0 );
    void notifyChanges();

    Clock m_clock;
    QTimer m_ticker;

    Mode m_mode = Off;
    int m_minutes = 0;
    qint64 m_deadline = 0;

    qint64 m_itemTimeLeft = -1;
    qint64 m_queueTimeLeft = -1;
    bool m_lastItem = false;

    /* last notified values */
    qint64 m_notifiedTimeLeft = -1;
    bool m_notifiedPauseAtEnd = false;
};

#endif // QVLC_SLEEP_TIMER_HPP
