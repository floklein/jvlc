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
#ifndef VLC_QT_PREPARSER_PLAYLIST_READER_HPP
#define VLC_QT_PREPARSER_PLAYLIST_READER_HPP

#include "network_playlist_model.hpp"

typedef struct vlc_object_t vlc_object_t;
typedef struct vlc_preparser_t vlc_preparser_t;

/**
 * Reads playlist URLs with VLC's own playlist parsers, through a preparser
 * running in this process.
 */
class PreparserPlaylistReader : public NetworkPlaylistReader
{
    Q_OBJECT

public:
    explicit PreparserPlaylistReader(vlc_object_t *obj, QObject *parent = nullptr);
    ~PreparserPlaylistReader() override;

    void read(const QString &url, QObject *receiver, Callback done) override;

private:
    vlc_object_t *const m_obj;
    vlc_preparser_t *m_preparser = nullptr;
};

#endif // VLC_QT_PREPARSER_PLAYLIST_READER_HPP
