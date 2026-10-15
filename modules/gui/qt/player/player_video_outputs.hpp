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
#ifndef QVLC_PLAYER_VIDEO_OUTPUTS_HPP
#define QVLC_PLAYER_VIDEO_OUTPUTS_HPP

#include "player/video_adjustments.hpp"

class PlayerController;

/**
 * The video outputs of the player and the VLC preferences, for the Video tab
 *
 * Options are set as variables of each video output: a filter started later
 * inherits them, a running filter follows them.
 */
class PlayerVideoOutputs : public VideoOutputTarget
{
public:
    explicit PlayerVideoOutputs( PlayerController* player );

    QString filterChain() const override;
    void setFilterChain( const QString& chain ) override;
    void setOption( const char* name, const QVariant& value ) override;

    bool hasPreference( const char* name ) const override;
    QVariant preference( const char* name ) const override;
    void setPreference( const char* name, const QVariant& value ) override;

private:
    PlayerController* m_player;
};

#endif // QVLC_PLAYER_VIDEO_OUTPUTS_HPP
