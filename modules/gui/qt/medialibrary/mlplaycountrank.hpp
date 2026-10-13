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

#ifndef MLPLAYCOUNTRANK_HPP
#define MLPLAYCOUNTRANK_HPP

#include <ctime>
#include <functional>
#include <memory>

#include "mlbasemodel.hpp"

// Lists the entities of a model (folders, playlists, artists) by how often
// their media were played.
//
// The media library can't sort these entities by play count, so this operation
// wraps the model's own one: it loads every entity, adds up the play counts of
// each entity's media and drops the entities never played. A descending order
// lists the most played first; equal play counts list the most recently played
// first.
class MLPlayCountRank : public MLListCacheLoader::MLOp
{
public:
    struct PlayCount
    {
        uint64_t count = 0;
        time_t lastPlayed = 0;
    };

    using Counter = std::function<PlayCount (vlc_medialibrary_t* ml, const MLItemId& entity)>;

    MLPlayCountRank(const MLBaseModel& model, std::shared_ptr<MLOp> entities,
                    Counter counter = &MLPlayCountRank::mediaPlayCount);

    // Sums the play counts of the media of a folder, playlist or artist.
    static PlayCount mediaPlayCount(vlc_medialibrary_t* ml, const MLItemId& entity);

    // Ranks the entities of `op` when the model sorts by play count
    // (VLC_ML_SORTING_PLAYCOUNT), otherwise returns `op` unchanged.
    static std::shared_ptr<MLOp> rankIfSortedByPlayCount(const MLBaseModel& model,
                                                         std::shared_ptr<MLOp> op);

    size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
    std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
    std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;

private:
    std::vector<std::unique_ptr<MLItem>> rank(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const;

    std::shared_ptr<MLOp> m_entities;
    Counter m_counter;
};

#endif // MLPLAYCOUNTRANK_HPP
