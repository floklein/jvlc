/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mlvideofoldersmodel.hpp"

// VLC includes
#include <vlc_media_library.h>

// Util includes
#include "util/covergenerator.hpp"
#include "util/vlctick.hpp"

// MediaLibrary includes
#include "mlfolder.hpp"
#include "mlhelper.hpp"
#include "mlplaycountrank.hpp"

// Ctor / dtor

/* explicit */ MLVideoFoldersModel::MLVideoFoldersModel(QObject * parent) : MLBaseModel(parent) {}

// MLBaseModel reimplementation

QHash<int, QByteArray> MLVideoFoldersModel::roleNames() const /* override */
{
    return {
        { FOLDER_ID, QByteArrayLiteral("id") },
        { FOLDER_TITLE, QByteArrayLiteral("title") },
        { FOLDER_TITLE_FIRST_SYMBOL, QByteArrayLiteral("title_first_symbol") },
        { FOLDER_THUMBNAIL, QByteArrayLiteral("thumbnail") },
        { FOLDER_DURATION, QByteArrayLiteral("duration") },
        { FOLDER_COUNT, QByteArrayLiteral("count") },
        { FOLDER_NB_VIDEOS, QByteArrayLiteral("nb_videos") },
        { FOLDER_NB_AUDIO, QByteArrayLiteral("nb_audio") },
        { FOLDER_IS_FOLDER, QByteArrayLiteral("isFolder") },
    };
}

// Protected MLVideoModel implementation

QVariant MLVideoFoldersModel::itemRoleData(const MLItem * item, const int role) const /* override */
{
    if (item == nullptr)
        return QVariant();

    const MLFolder * folder = static_cast<const MLFolder *> (item);

    switch (role)
    {
        // NOTE: This is the condition for QWidget view(s).
        case Qt::DisplayRole:
            return QVariant::fromValue(folder->getTitle());
        // NOTE: These are the conditions for QML view(s).
        case FOLDER_ID:
            return QVariant::fromValue(folder->getId());
        case FOLDER_TITLE:
            return QVariant::fromValue(folder->getTitle());
        case FOLDER_TITLE_FIRST_SYMBOL:
            return QVariant::fromValue( getFirstSymbol(folder->getTitle()) );
        case FOLDER_THUMBNAIL:
            // a folder shows the folder tile, never a mosaic of its media
            // (Figma 6284:13980): the views draw it
            return QVariant::fromValue(QString());
        case FOLDER_IS_FOLDER:
            return QVariant::fromValue(true);
        case FOLDER_DURATION:
            // the library may not know it: no duration rather than "0 ms"
            if (folder->getDuration().asMilliseconds() <= 0)
                return {};
            return QVariant::fromValue(folder->getDuration());
        case FOLDER_COUNT:
            /* the media the model lists the folder for */
            if (m_anyMedia)
                return QVariant::fromValue(folder->getVideoCount() + folder->getAudioCount());
            return QVariant::fromValue(m_music ? folder->getAudioCount() : folder->getVideoCount());
        case FOLDER_NB_VIDEOS:
            return QVariant::fromValue(folder->getVideoCount());
        case FOLDER_NB_AUDIO:
            return QVariant::fromValue(folder->getAudioCount());
        default:
            return QVariant();
    }
}

vlc_ml_sorting_criteria_t MLVideoFoldersModel::nameToCriteria(QByteArray name) const /* override */
{
    return QHash<QByteArray, vlc_ml_sorting_criteria_t> {
        { QByteArrayLiteral("title"),    VLC_ML_SORTING_ALPHA    },
        { QByteArrayLiteral("duration"), VLC_ML_SORTING_DURATION },
        { QByteArrayLiteral("playcount"), VLC_ML_SORTING_PLAYCOUNT },
    }.value(name, VLC_ML_SORTING_DEFAULT);
}

void MLVideoFoldersModel::setMusic(bool music)
{
    if (m_music == music)
        return;
    m_music = music;
    emit musicChanged();
    emit resetRequested();
}

void MLVideoFoldersModel::setAnyMedia(bool anyMedia)
{
    if (m_anyMedia == anyMedia)
        return;
    m_anyMedia = anyMedia;
    emit anyMediaChanged();
    emit resetRequested();
}

std::unique_ptr<MLListCacheLoader>
MLVideoFoldersModel::createMLLoader() const /* override */
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib,
        MLPlayCountRank::rankIfSortedByPlayCount(*this, std::make_shared<MLVideoFoldersModel::Loader>(*this)));
}

// Protected MLBaseModel reimplementation

void MLVideoFoldersModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    int type = event.i_type;

    switch (type)
    {
        case VLC_ML_EVENT_FOLDER_ADDED:
        {
            emit resetRequested();
            return;
        }
        case VLC_ML_EVENT_FOLDER_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_FOLDER);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_FOLDER_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_FOLDER);
            deleteItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_HISTORY_CHANGED:
        {
            // playing a media changes the play counts of its folder
            if (getMLSortingCriteria() == VLC_ML_SORTING_PLAYCOUNT)
                emit resetRequested();
            return;
        }
        default:
            break;
    }

    MLBaseModel::onVlcMlEvent(event);
}

// Loader
size_t MLVideoFoldersModel::Loader::count(vlc_medialibrary_t * ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    return vlc_ml_count_folders_by_type(ml, queryParams, m_type);
}

std::vector<std::unique_ptr<MLItem>>
MLVideoFoldersModel::Loader::load(vlc_medialibrary_t * ml,
                                  const vlc_ml_query_params_t* queryParams) const /* override */
{
    ml_unique_ptr<vlc_ml_folder_list_t> list(vlc_ml_list_folders_by_type(ml, queryParams,
                                                                         m_type));

    if (list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> result;

    for (const vlc_ml_folder_t & folder : ml_range_iterate<vlc_ml_folder_t>(list))
    {
        result.emplace_back(std::make_unique<MLFolder>(&folder));
    }

    return result;
}

std::unique_ptr<MLItem>
MLVideoFoldersModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_FOLDER);
    ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder(ml, itemId.id));
    if (!folder)
        return nullptr;
    return std::make_unique<MLFolder>(folder.get());
}
