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

#include "playback_statistics.hpp"

#include <vlc_common.h>
#include <vlc_input_item.h>

void PlaybackStatistics::update( const input_stats_t& stats )
{
    /* bitrates are in bytes per microsecond */
    m_inputBitrate = stats.f_input_bitrate * 8000.;
    m_contentBitrate = stats.f_demux_bitrate * 8000.;
    m_displayedFrames = stats.i_displayed_pictures;
    m_droppedFrames = stats.i_lost_pictures;
    m_lateFrames = stats.i_late_pictures;
    m_lostAudioBuffers = stats.i_lost_abuffers;
    emit changed();
}

void PlaybackStatistics::reset()
{
    m_inputBitrate = 0.;
    m_contentBitrate = 0.;
    m_displayedFrames = 0;
    m_droppedFrames = 0;
    m_lateFrames = 0;
    m_lostAudioBuffers = 0;
    emit changed();
}
