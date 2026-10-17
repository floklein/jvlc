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

#include "mlmusicfoldertreemodel.hpp"

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "mlaudio.hpp"
#include "mlfolder.hpp"
#include "mlfoldertreemodel.hpp"
#include "mlhelper.hpp"
#include "util/vlctick.hpp"

#include <QCollator>

#include <algorithm>

namespace {

// Compares two tracks on one criterion, 0 when they are equal on it
using TrackCompare = int (*)(const QCollator &, const MLAudio &, const MLAudio &);

// the tracks without the name come last
int compareNames(const QCollator &collator, const QString &a, const QString &b)
{
    if (a.isEmpty() != b.isEmpty())
        return a.isEmpty() ? 1 : -1;
    return collator.compare(a, b);
}

int compareNumbers(unsigned a, unsigned b)
{
    return (a < b) ? -1 : (a > b) ? 1 : 0;
}

int byTitle(const QCollator &collator, const MLAudio &a, const MLAudio &b)
{
    return mlfoldertree::naturalCompare(collator, a.title(), b.title());
}

int byArtist(const QCollator &collator, const MLAudio &a, const MLAudio &b)
{
    return compareNames(collator, a.getArtist(), b.getArtist());
}

int byAlbum(const QCollator &collator, const MLAudio &a, const MLAudio &b)
{
    return compareNames(collator, a.getAlbumTitle(), b.getAlbumTitle());
}

int byTrack(const QCollator &, const MLAudio &a, const MLAudio &b)
{
    return compareNumbers(a.getTrackNumber(), b.getTrackNumber());
}

int byDisc(const QCollator &, const MLAudio &a, const MLAudio &b)
{
    return compareNumbers(a.getDiscNumber(), b.getDiscNumber());
}

// Unplayed First: never played, then played
int byUnplayed(const QCollator &, const MLAudio &a, const MLAudio &b)
{
    return compareNumbers(a.playCount() > 0, b.playCount() > 0);
}

// Played First: the most played first
int byPlayed(const QCollator &, const MLAudio &a, const MLAudio &b)
{
    return compareNumbers(b.playCount(), a.playCount());
}

// the criteria a sort compares, in order; empty for the library's order
std::vector<TrackCompare> trackCriteria(const QString &criteria)
{
    if (criteria == QLatin1String("insertion"))
        return {};
    if (criteria == QLatin1String("unplayed"))
        return { byUnplayed, byTitle };
    if (criteria == QLatin1String("played"))
        return { byPlayed, byTitle };
    if (criteria == QLatin1String("main_artist"))
        return { byArtist, byAlbum, byDisc, byTrack, byTitle };
    if (criteria == QLatin1String("album_title"))
        return { byAlbum, byDisc, byTrack, byTitle };
    if (criteria == QLatin1String("track_number"))
        return { byTrack, byTitle };
    if (criteria == QLatin1String("disc_number"))
        return { byDisc, byTrack, byTitle };
    return { byTitle };
}

} // namespace

MLMusicFolderTreeModel::MLMusicFolderTreeModel(QObject *parent)
    : MLAudioModel(parent)
{
    connect(this, &MLBaseModel::parentIdChanged, this, &MLMusicFolderTreeModel::updatePath);
    connect(this, &MLBaseModel::mlChanged, this, &MLMusicFolderTreeModel::updatePath);
}

QHash<int, QByteArray> MLMusicFolderTreeModel::roleNames() const
{
    QHash<int, QByteArray> hash = MLAudioModel::roleNames();

    hash.insert(FOLDER_IS_FOLDER, QByteArrayLiteral("isFolder"));

    return hash;
}

QVariant MLMusicFolderTreeModel::itemRoleData(const MLItem *item, int role) const
{
    if (item == nullptr)
        return {};

    if (!mlfoldertree::isFolder(item))
    {
        if (role == FOLDER_IS_FOLDER)
            return QVariant::fromValue(false);
        return MLAudioModel::itemRoleData(item, role);
    }

    const auto folder = static_cast<const MLFolder *>(item);

    switch (role)
    {
    case Qt::DisplayRole:
    case MEDIA_TITLE:
        return QVariant::fromValue(folder->getTitle());
    case MEDIA_ID:
        return QVariant::fromValue(folder->getId());
    case MEDIA_TITLE_FIRST_SYMBOL:
        return QVariant::fromValue(getFirstSymbol(folder->getTitle()));
    case MEDIA_SMALL_COVER:
    case AUDIO_COVER:
        // a folder shows the folder tile, never a mosaic of its media
        // (Figma 6284:13980): the views draw it
        return QVariant::fromValue(QString());
    case MEDIA_DURATION:
        // the duration counts the folder's own media only, hide it without
        if (folder->getDuration().asMilliseconds() <= 0)
            return {};
        return QVariant::fromValue(folder->getDuration());
    case MEDIA_MRL:
        return QVariant::fromValue(folder->getMRL());
    case MEDIA_IS_VIDEO:
        return QVariant::fromValue(false);
    case FOLDER_IS_FOLDER:
        return QVariant::fromValue(true);
    default:
        // NOTE: The media roles (isNew, isLocal...) stay undefined, so that
        // the media actions of the context menu don't apply to folders.
        return {};
    }
}

vlc_ml_sorting_criteria_t MLMusicFolderTreeModel::nameToCriteria(QByteArray name) const
{
    return QHash<QByteArray, vlc_ml_sorting_criteria_t> {
        {QByteArrayLiteral("title"), VLC_ML_SORTING_ALPHA},
        {QByteArrayLiteral("main_artist"), VLC_ML_SORTING_ARTIST},
        {QByteArrayLiteral("album_title"), VLC_ML_SORTING_ALBUM},
        {QByteArrayLiteral("track_number"), VLC_ML_SORTING_TRACKNUMBER},
        {QByteArrayLiteral("disc_number"), VLC_ML_SORTING_TRACKNUMBER},
        {QByteArrayLiteral("insertion"), VLC_ML_SORTING_INSERTIONDATE},
    }.value(name, VLC_ML_SORTING_DEFAULT);
}

std::unique_ptr<MLListCacheLoader> MLMusicFolderTreeModel::createMLLoader() const
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLMusicFolderTreeModel::Loader>(*this));
}

void MLMusicFolderTreeModel::onVlcMlEvent(const MLEvent &event)
{
    if (mlfoldertree::changesFolders(event.i_type) || event.i_type == VLC_ML_EVENT_MEDIA_ADDED)
    {
        emit resetRequested();
        return;
    }

    MLAudioModel::onVlcMlEvent(event);
}

void MLMusicFolderTreeModel::updatePath()
{
    if (m_mediaLib && m_pathTask != 0)
        m_mediaLib->cancelMLTask(this, m_pathTask);
    m_pathTask = 0;

    if (!m_mediaLib || m_parent.id <= 0 || m_parent.type != VLC_ML_PARENT_FOLDER)
    {
        if (!m_path.isEmpty())
        {
            m_path.clear();
            emit pathChanged();
        }
        return;
    }

    struct Ctx {
        QVariantList path;
    };

    m_pathTask = m_mediaLib->runOnMLThread<Ctx>(this,
    // ML thread
    [parent = m_parent](vlc_medialibrary_t *ml, Ctx &ctx)
    {
        ctx.path = mlfoldertree::path(ml, parent);
    },
    // UI thread
    [this](quint64, Ctx &ctx)
    {
        m_pathTask = 0;
        m_path = ctx.path;
        emit pathChanged();
    });
}

// Loader

MLMusicFolderTreeModel::Loader::Loader(const MLMusicFolderTreeModel &model)
    : MLOp(model)
    , m_criteria(model.getSortCriteria())
{
}

size_t MLMusicFolderTreeModel::Loader::count(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *) const
{
    return rows(ml).size();
}

std::vector<std::unique_ptr<MLItem>>
MLMusicFolderTreeModel::Loader::load(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *queryParams) const
{
    // the levels of a folder tree are small, so each load lists the whole
    // level and returns the requested page of it
    return mlfoldertree::page(rows(ml), queryParams);
}

std::unique_ptr<MLItem>
MLMusicFolderTreeModel::Loader::loadItemById(vlc_medialibrary_t *ml, MLItemId itemId) const
{
    if (itemId.type == VLC_ML_PARENT_FOLDER)
    {
        ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder(ml, itemId.id));
        if (!folder)
            return nullptr;
        return std::make_unique<MLFolder>(folder.get());
    }

    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media || media->i_type != VLC_ML_MEDIA_TYPE_AUDIO)
        return nullptr;
    return std::make_unique<MLAudio>(ml, media.get());
}

std::vector<std::unique_ptr<MLItem>>
MLMusicFolderTreeModel::Loader::rows(vlc_medialibrary_t *ml) const
{
    const QString pattern = QString::fromUtf8(m_searchPattern);
    const auto matches = [&pattern](const MLAudio &track) {
        return pattern.isEmpty()
               || track.title().contains(pattern, Qt::CaseInsensitive)
               || track.getArtist().contains(pattern, Qt::CaseInsensitive)
               || track.getAlbumTitle().contains(pattern, Qt::CaseInsensitive);
    };

    const bool desc = m_sortDesc && !m_criteria.isEmpty();
    const bool titleDesc = desc && (m_criteria == QLatin1String("title"));

    // folders, by title

    std::vector<std::unique_ptr<MLFolder>> folders =
        mlfoldertree::folders(ml, m_parent, VLC_ML_MEDIA_TYPE_AUDIO, pattern, titleDesc);

    // tracks

    std::vector<std::unique_ptr<MLAudio>> tracks;
    if (m_parent.id > 0)
    {
        vlc_ml_query_params_t params = vlc_ml_query_params_create();
        params.i_sort = VLC_ML_SORTING_INSERTIONDATE;
        params.b_desc = (m_criteria == QLatin1String("insertion")) && m_sortDesc;

        ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_audio_of(ml, &params,
                                                                      VLC_ML_PARENT_FOLDER,
                                                                      m_parent.id));
        if (list)
        {
            for (const vlc_ml_media_t &media : ml_range_iterate<vlc_ml_media_t>(list))
            {
                auto track = std::make_unique<MLAudio>(ml, &media);
                if (matches(*track))
                    tracks.push_back(std::move(track));
            }
        }
    }

    const std::vector<TrackCompare> criteria = trackCriteria(m_criteria);
    if (!criteria.empty())
    {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);

        std::stable_sort(tracks.begin(), tracks.end(),
                         [&collator, &criteria, desc](const std::unique_ptr<MLAudio> &a, const std::unique_ptr<MLAudio> &b) {
            for (const TrackCompare compare : criteria)
            {
                const int result = desc ? compare(collator, *b, *a) : compare(collator, *a, *b);
                if (result != 0)
                    return result < 0;
            }
            return false;
        });
    }

    std::vector<std::unique_ptr<MLItem>> result;
    result.reserve(folders.size() + tracks.size());
    for (auto &folder : folders)
        result.push_back(std::move(folder));
    for (auto &track : tracks)
        result.push_back(std::move(track));
    return result;
}
