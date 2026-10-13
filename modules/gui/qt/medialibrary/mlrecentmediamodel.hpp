/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#ifndef ML_RECENT_MEDIA_MODEL_H
#define ML_RECENT_MEDIA_MODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mlmediamodel.hpp"

class MLRecentMediaModel : public MLMediaModel
{
    Q_OBJECT

    // Which media the model lists: every played media (default) or only the
    // played local files or network streams
    Q_PROPERTY(HistoryType historyType READ historyType WRITE setHistoryType NOTIFY historyTypeChanged FINAL)

public:
    enum Roles
    {
        RECENT_MEDIA_URL = MLMediaModel::MEDIA_ROLES_COUNT,
    };

    enum HistoryType
    {
        GLOBAL = VLC_ML_HISTORY_TYPE_GLOBAL,
        LOCAL = VLC_ML_HISTORY_TYPE_LOCAL,
        NETWORK = VLC_ML_HISTORY_TYPE_NETWORK,
    };
    Q_ENUM(HistoryType)

    explicit MLRecentMediaModel( QObject* parent = nullptr );
    virtual ~MLRecentMediaModel() = default;

    Q_INVOKABLE void clearHistory();

    HistoryType historyType() const { return m_historyType; }
    void setHistoryType( HistoryType type );

signals:
    void historyTypeChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

    QVariant itemRoleData(const MLItem *item, int role) const override;

    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

private:
    void onVlcMlEvent( const MLEvent &event ) override;

    struct Loader : public MLListCacheLoader::MLOp
    {
        Loader(const MLRecentMediaModel& model)
            : MLOp(model)
            , m_historyType(static_cast<vlc_ml_history_type_t>(model.m_historyType))
        {}
        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;

        vlc_ml_history_type_t m_historyType;
    };

    HistoryType m_historyType = GLOBAL;
};

#endif // ML_RECENT_MEDIA_MODEL_H
