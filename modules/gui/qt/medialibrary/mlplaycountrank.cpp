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

#include "mlplaycountrank.hpp"

#include <algorithm>

#include <vlc_media_library.h>

#include "mlhelper.hpp"

MLPlayCountRank::MLPlayCountRank(const MLBaseModel& model, std::shared_ptr<MLOp> entities,
                                 Counter counter)
    : MLOp(model)
    , m_entities(std::move(entities))
    , m_counter(std::move(counter))
{
}

std::shared_ptr<MLListCacheLoader::MLOp>
MLPlayCountRank::rankIfSortedByPlayCount(const MLBaseModel& model, std::shared_ptr<MLOp> op)
{
    if (model.getMLSortingCriteria() != VLC_ML_SORTING_PLAYCOUNT)
        return op;
    return std::make_shared<MLPlayCountRank>(model, std::move(op));
}

MLPlayCountRank::PlayCount
MLPlayCountRank::mediaPlayCount(vlc_medialibrary_t* ml, const MLItemId& entity)
{
    PlayCount playCount;

    ml_unique_ptr<vlc_ml_media_list_t> media_list {
        vlc_ml_list_media_of(ml, nullptr, entity.type, entity.id)
    };
    if (media_list == nullptr)
        return playCount;

    for (const vlc_ml_media_t& media : ml_range_iterate<vlc_ml_media_t>(media_list))
    {
        playCount.count += media.i_playcount;
        playCount.lastPlayed = std::max(playCount.lastPlayed, media.i_last_played_date);
    }
    return playCount;
}

std::vector<std::unique_ptr<MLItem>>
MLPlayCountRank::rank(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    // list every entity matching the search, in the media library's own order
    vlc_ml_query_params_t params = getQueryParams();
    params.psz_pattern = queryParams ? queryParams->psz_pattern : nullptr;
    params.i_sort = VLC_ML_SORTING_DEFAULT;
    params.b_desc = false;

    struct Ranked
    {
        std::unique_ptr<MLItem> item;
        PlayCount playCount;
    };

    std::vector<Ranked> ranked;
    for (std::unique_ptr<MLItem>& item : m_entities->load(ml, &params))
    {
        const PlayCount playCount = m_counter(ml, item->getId());
        if (playCount.count == 0)
            continue;
        ranked.push_back({ std::move(item), playCount });
    }

    const bool desc = m_sortDesc;
    std::stable_sort(ranked.begin(), ranked.end(), [desc](const Ranked& a, const Ranked& b) {
        if (a.playCount.count != b.playCount.count)
            return desc ? a.playCount.count > b.playCount.count
                        : a.playCount.count < b.playCount.count;
        return a.playCount.lastPlayed > b.playCount.lastPlayed;
    });

    std::vector<std::unique_ptr<MLItem>> items;
    items.reserve(ranked.size());
    for (Ranked& entry : ranked)
        items.push_back(std::move(entry.item));
    return items;
}

size_t MLPlayCountRank::count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    return rank(ml, queryParams).size();
}

std::vector<std::unique_ptr<MLItem>>
MLPlayCountRank::load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    std::vector<std::unique_ptr<MLItem>> items = rank(ml, queryParams);

    const size_t first = std::min<size_t>(queryParams ? queryParams->i_offset : 0, items.size());
    size_t last = items.size();
    if (queryParams && queryParams->i_nbResults > 0)
        last = std::min<size_t>(first + queryParams->i_nbResults, items.size());

    return { std::make_move_iterator(items.begin() + first),
             std::make_move_iterator(items.begin() + last) };
}

std::unique_ptr<MLItem>
MLPlayCountRank::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    return m_entities->loadItemById(ml, itemId);
}
