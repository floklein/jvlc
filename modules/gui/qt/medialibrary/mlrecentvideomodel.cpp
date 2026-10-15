/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#include "mlrecentvideomodel.hpp"

#include "medialib.hpp"

// Ctor / dtor

MLRecentVideoModel::MLRecentVideoModel(QObject * parent) : MLVideoModel(parent)
{
    connect(this, &MLBaseModel::mlChanged, this, &MLRecentVideoModel::onMlChanged);
}

void MLRecentVideoModel::onMlChanged()
{
    disconnect(m_hideListConnection);
    if (m_mediaLib)
        m_hideListConnection = connect(m_mediaLib->recentHideList(), &MLRecentHideList::changed,
                                       this, &MLRecentVideoModel::resetRequested);
}

// Protected MLBaseModel implementation

std::unique_ptr<MLListCacheLoader>
MLRecentVideoModel::createMLLoader() const
/* override */
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLRecentVideoModel::Loader>(*this));
}

// Private MLVideoModel reimplementation

void MLRecentVideoModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_HISTORY_CHANGED:
            emit resetRequested();
            return;
        default:
            break;
    }

    MLVideoModel::onVlcMlEvent(event);
}

// Loader

MLRecentVideoModel::Loader::Loader(const MLRecentVideoModel& model)
    : MLOp(model)
{
    if (model.m_mediaLib)
        m_hideList = model.m_mediaLib->recentHideList()->source();
}

ml_unique_ptr<vlc_ml_media_list_t>
MLRecentVideoModel::Loader::listAll(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    // the media library can't skip what isn't in progress: page here
    vlc_ml_query_params_t params = queryParams ? *queryParams : vlc_ml_query_params_create();
    params.i_offset = 0;
    params.i_nbResults = 0;
    return ml_unique_ptr<vlc_ml_media_list_t>{ vlc_ml_list_video_history(ml, &params) };
}

std::vector<const vlc_ml_media_t*>
MLRecentVideoModel::Loader::inProgress(const vlc_ml_media_list_t* list) const
{
    const MLRecentHideList::Filter hidden = m_hideList ? m_hideList->get()
                                                       : MLRecentHideList::Filter{};
    std::vector<const vlc_ml_media_t*> media;
    for (size_t i = 0; i < list->i_nb_items; ++i)
    {
        const vlc_ml_media_t& item = list->p_items[i];
        // a video watched to its end has no progress
        if (item.f_progress <= 0.f || item.f_progress >= 1.f)
            continue;
        if (hidden.isHidden(item.i_id, item.i_last_played_date))
            continue;
        media.push_back(&item);
    }
    return media;
}

size_t MLRecentVideoModel::Loader::count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    ml_unique_ptr<vlc_ml_media_list_t> media_list = listAll(ml, queryParams);
    if (media_list == nullptr)
        return 0;
    return inProgress(media_list.get()).size();
}

std::vector<std::unique_ptr<MLItem>>
MLRecentVideoModel::Loader::load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    ml_unique_ptr<vlc_ml_media_list_t> media_list = listAll(ml, queryParams);
    if (media_list == nullptr)
        return {};

    const std::vector<const vlc_ml_media_t*> media = inProgress(media_list.get());
    const size_t offset = queryParams ? queryParams->i_offset : 0;
    const size_t limit = queryParams ? queryParams->i_nbResults : 0;

    std::vector<std::unique_ptr<MLItem>> res;
    for (size_t i = offset; i < media.size() && (limit == 0 || res.size() < limit); ++i)
        res.emplace_back(std::make_unique<MLVideo>(media[i]));
    return res;
}

std::unique_ptr<MLItem>
MLRecentVideoModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLVideo>(media.get());
}
