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

#ifndef ML_RECENT_AUDIO_MODEL_H
#define ML_RECENT_AUDIO_MODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mlaudiomodel.hpp"
#include "mlhelper.hpp"
#include "mlrecenthidelist.hpp"

// The songs played most recently ("Listen Again" on Home), except the ones
// Recent hides (Clear Recent, Remove from Recent)
class MLRecentAudioModel : public MLAudioModel
{
    Q_OBJECT

public:
    explicit MLRecentAudioModel(QObject * parent = nullptr);

    ~MLRecentAudioModel() override = default;

protected: // MLBaseModel implementation
    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

    void onVlcMlEvent(const MLEvent & event) override;

private:
    void onMlChanged();

    struct Loader : public MLListCacheLoader::MLOp
    {
        explicit Loader(const MLRecentAudioModel& model);

        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t*) const override;

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t*) const override;

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;

    private:
        // every song of the history, the hidden ones taken out
        std::vector<const vlc_ml_media_t*> visible(const vlc_ml_media_list_t* list) const;
        ml_unique_ptr<vlc_ml_media_list_t> listAll(vlc_medialibrary_t* ml,
                                                   const vlc_ml_query_params_t* queryParams) const;

        std::shared_ptr<const MLRecentHideList::Source> m_hideList;
    };

    QMetaObject::Connection m_hideListConnection;
};

#endif // ML_RECENT_AUDIO_MODEL_H
