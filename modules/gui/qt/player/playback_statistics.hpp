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
#ifndef QVLC_PLAYBACK_STATISTICS_HPP
#define QVLC_PLAYBACK_STATISTICS_HPP

#include <QObject>

struct input_stats_t;

/**
 * The live statistics of the current media, for the statistics overlay of
 * the player ("Show Statistics" in the Playback tab)
 */
class PlaybackStatistics : public QObject
{
    Q_OBJECT

    // kilobits per second
    Q_PROPERTY(double inputBitrate READ inputBitrate NOTIFY changed FINAL)
    Q_PROPERTY(double contentBitrate READ contentBitrate NOTIFY changed FINAL)
    Q_PROPERTY(qint64 displayedFrames READ displayedFrames NOTIFY changed FINAL)
    Q_PROPERTY(qint64 droppedFrames READ droppedFrames NOTIFY changed FINAL)
    Q_PROPERTY(qint64 lateFrames READ lateFrames NOTIFY changed FINAL)
    Q_PROPERTY(qint64 lostAudioBuffers READ lostAudioBuffers NOTIFY changed FINAL)

public:
    using QObject::QObject;

    double inputBitrate() const { return m_inputBitrate; }
    double contentBitrate() const { return m_contentBitrate; }
    qint64 displayedFrames() const { return m_displayedFrames; }
    qint64 droppedFrames() const { return m_droppedFrames; }
    qint64 lateFrames() const { return m_lateFrames; }
    qint64 lostAudioBuffers() const { return m_lostAudioBuffers; }

    void update( const input_stats_t& stats );
    void reset();

signals:
    void changed();

private:
    double m_inputBitrate = 0.;
    double m_contentBitrate = 0.;
    qint64 m_displayedFrames = 0;
    qint64 m_droppedFrames = 0;
    qint64 m_lateFrames = 0;
    qint64 m_lostAudioBuffers = 0;
};

#endif // QVLC_PLAYBACK_STATISTICS_HPP
