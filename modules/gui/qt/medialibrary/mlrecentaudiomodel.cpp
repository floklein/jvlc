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

#include "mlrecentaudiomodel.hpp"

#include "medialib.hpp"
#include "mlaudio.hpp"

// Ctor / dtor

MLRecentAudioModel::MLRecentAudioModel(QObject * parent) : MLAudioModel(parent)
{
    connect(this, &MLBaseModel::mlChanged, this, &MLRecentAudioModel::onMlChanged);
}

void MLRecentAudioModel::onMlChanged()
{
    disconnect(m_hideListConnection);
    if (m_mediaLib)
        m_hideListConnection = connect(m_mediaLib->recentHideList(), &MLRecentHideList::changed,
                                       this, &MLRecentAudioModel::resetRequested);
}

// Protected MLBaseModel implementation

std::unique_ptr<MLListCacheLoader>
MLRecentAudioModel::createMLLoader() const
/* override */
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLRecentAudioModel::Loader>(*this));
}

void MLRecentAudioModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_HISTORY_CHANGED:
            emit resetRequested();
            return;
        // a new song isn't recent until it is played
        case VLC_ML_EVENT_MEDIA_ADDED:
            return;
        default:
            break;
    }

    MLMediaModel::onVlcMlEvent(event);
}

// Loader

MLRecentAudioModel::Loader::Loader(const MLRecentAudioModel& model)
    : MLOp(model)
{
    if (model.m_mediaLib)
        m_hideList = model.m_mediaLib->recentHideList()->source();
}

ml_unique_ptr<vlc_ml_media_list_t>
MLRecentAudioModel::Loader::listAll(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    // the media library can't skip the hidden songs: page here
    vlc_ml_query_params_t params = queryParams ? *queryParams : vlc_ml_query_params_create();
    params.i_offset = 0;
    params.i_nbResults = 0;
    // the history is sorted by last played date, oldest first unless
    // descending: the last played first
    if (params.i_sort == VLC_ML_SORTING_DEFAULT)
        params.b_desc = true;
    return ml_unique_ptr<vlc_ml_media_list_t>{ vlc_ml_list_audio_history(ml, &params) };
}

std::vector<const vlc_ml_media_t*>
MLRecentAudioModel::Loader::visible(const vlc_ml_media_list_t* list) const
{
    const MLRecentHideList::Filter hidden = m_hideList ? m_hideList->get()
                                                       : MLRecentHideList::Filter{};
    std::vector<const vlc_ml_media_t*> media;
    for (size_t i = 0; i < list->i_nb_items; ++i)
        if (!hidden.isHidden(list->p_items[i].i_id, list->p_items[i].i_last_played_date))
            media.push_back(&list->p_items[i]);
    return media;
}

size_t MLRecentAudioModel::Loader::count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    if (!m_hideList || m_hideList->get().isEmpty())
        return vlc_ml_count_audio_history(ml, queryParams);
    ml_unique_ptr<vlc_ml_media_list_t> media_list = listAll(ml, queryParams);
    return media_list ? visible(media_list.get()).size() : 0;
}

std::vector<std::unique_ptr<MLItem>>
MLRecentAudioModel::Loader::load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    ml_unique_ptr<vlc_ml_media_list_t> media_list = listAll(ml, queryParams);
    if (media_list == nullptr)
        return {};

    const std::vector<const vlc_ml_media_t*> media = visible(media_list.get());
    const size_t offset = queryParams ? queryParams->i_offset : 0;
    const size_t limit = queryParams ? queryParams->i_nbResults : 0;

    std::vector<std::unique_ptr<MLItem>> res;
    for (size_t i = offset; i < media.size() && (limit == 0 || res.size() < limit); ++i)
        res.emplace_back(std::make_unique<MLAudio>(ml, media[i]));
    return res;
}

std::unique_ptr<MLItem>
MLRecentAudioModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLAudio>(ml, media.get());
}
