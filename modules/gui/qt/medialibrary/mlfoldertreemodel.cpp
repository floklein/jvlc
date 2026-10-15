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

#include "mlfoldertreemodel.hpp"

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "mlcustomcover.hpp"
#include "mlfolder.hpp"
#include "mlhelper.hpp"
#include "mlvideo.hpp"
#include "util/vlctick.hpp"

#include <QCollator>

#include <algorithm>

namespace {

// NOTE: We multiply by 3 to cover most dpi settings.
const int FOLDER_COVER_WIDTH  = 260 * 3; // 16 / 10 ratio
const int FOLDER_COVER_HEIGHT = 162 * 3;

// guards the walk up the tree against a corrupted library
const int MAX_FOLDER_DEPTH = 64;

QString folderMrl(QString mrl)
{
    if (!mrl.isEmpty() && !mrl.endsWith(QLatin1Char('/')))
        mrl += QLatin1Char('/');
    return mrl;
}

// The MRLs of the folders that hold media of the type themselves, any type
// for VLC_ML_MEDIA_TYPE_UNKNOWN
QStringList mediaFolderMrls(vlc_medialibrary_t *ml, vlc_ml_media_type_t type)
{
    ml_unique_ptr<vlc_ml_folder_list_t> list(type == VLC_ML_MEDIA_TYPE_UNKNOWN
                                             ? vlc_ml_list_folders(ml, nullptr)
                                             : vlc_ml_list_folders_by_type(ml, nullptr, type));
    QStringList mrls;
    if (list == nullptr)
        return mrls;

    for (const vlc_ml_folder_t &folder : ml_range_iterate<vlc_ml_folder_t>(list))
    {
        if (folder.psz_mrl)
            mrls.push_back(folderMrl(qfu(folder.psz_mrl)));
    }
    return mrls;
}

// whether the folder, or a folder below it, is one of mrls
bool holdsAny(const QStringList &mrls, const QString &mrl)
{
    if (mrl.isEmpty())
        return false;
    return std::any_of(mrls.cbegin(), mrls.cend(), [&mrl](const QString &held) {
        return held.startsWith(mrl);
    });
}

// Unwatched First: in progress, then unwatched, then watched
int unwatchedFirstRank(const MLVideo &video)
{
    if (video.progress() > 0)
        return 0;
    if (video.playCount() == 0)
        return 1;
    return 2;
}

// Watched First: watched, then in progress, then unwatched
int watchedFirstRank(const MLVideo &video)
{
    if (video.playCount() > 0 && video.progress() <= 0)
        return 0;
    if (video.progress() > 0)
        return 1;
    return 2;
}

} // namespace

// Folder tree

bool mlfoldertree::isFolder(const MLItem *item)
{
    return item->getId().type == VLC_ML_PARENT_FOLDER;
}

bool mlfoldertree::changesFolders(int eventType)
{
    switch (eventType)
    {
    case VLC_ML_EVENT_FOLDER_ADDED:
    case VLC_ML_EVENT_FOLDER_UPDATED:
    case VLC_ML_EVENT_FOLDER_DELETED:
    case VLC_ML_EVENT_ENTRY_POINT_ADDED:
    case VLC_ML_EVENT_ENTRY_POINT_REMOVED:
    case VLC_ML_EVENT_ENTRY_POINT_BANNED:
    case VLC_ML_EVENT_ENTRY_POINT_UNBANNED:
        return true;
    default:
        return false;
    }
}

std::vector<std::unique_ptr<MLFolder>>
mlfoldertree::folders(vlc_medialibrary_t *ml, MLItemId parent, vlc_ml_media_type_t type,
                      const QString &pattern, bool titleDesc)
{
    const bool atTop = (parent.id <= 0);

    const QStringList typeMrls = mediaFolderMrls(ml, type);
    QStringList anyMediaMrls;

    ml_unique_ptr<vlc_ml_folder_list_t> folderList;
    if (atTop)
    {
        folderList.reset(vlc_ml_list_entry_points(ml, nullptr));
        anyMediaMrls = mediaFolderMrls(ml, VLC_ML_MEDIA_TYPE_UNKNOWN);
    }
    else
    {
        vlc_ml_query_params_t params = vlc_ml_query_params_create();
        params.i_sort = VLC_ML_SORTING_ALPHA;
        folderList.reset(vlc_ml_list_subfolders(ml, &params, parent.id));
    }

    std::vector<std::unique_ptr<MLFolder>> result;
    if (!folderList)
        return result;

    for (const vlc_ml_folder_t &data : ml_range_iterate<vlc_ml_folder_t>(folderList))
    {
        if (data.b_banned)
            continue;

        auto folder = std::make_unique<MLFolder>(&data);
        const QString mrl = folderMrl(folder->getMRL());

        // an added folder without media is shown, it may be scanned yet
        const bool shown = holdsAny(typeMrls, mrl)
                           || (atTop && !holdsAny(anyMediaMrls, mrl));

        if (shown && (pattern.isEmpty() || folder->getTitle().contains(pattern, Qt::CaseInsensitive)))
            result.push_back(std::move(folder));
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(result.begin(), result.end(),
                     [&collator, titleDesc](const std::unique_ptr<MLFolder> &a, const std::unique_ptr<MLFolder> &b) {
        return titleDesc ? naturalCompare(collator, b->getTitle(), a->getTitle()) < 0
                         : naturalCompare(collator, a->getTitle(), b->getTitle()) < 0;
    });

    return result;
}

QVariantList mlfoldertree::path(vlc_medialibrary_t *ml, MLItemId parent)
{
    QVariantList result;
    if (parent.id <= 0 || parent.type != VLC_ML_PARENT_FOLDER)
        return result;

    ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder(ml, parent.id));
    for (int depth = 0; folder != nullptr && depth < MAX_FOLDER_DEPTH; ++depth)
    {
        result.prepend(QVariantMap {
            {QStringLiteral("id"), QVariant::fromValue(MLItemId(folder->i_id, VLC_ML_PARENT_FOLDER))},
            {QStringLiteral("title"), qfu(folder->psz_name)},
        });
        folder.reset(vlc_ml_get_folder_parent(ml, folder->i_id));
    }
    return result;
}

// QCollator's numeric mode can't be relied on: it is ignored for the C locale
// and by some backends.
int mlfoldertree::naturalCompare(const QCollator &collator, const QString &a, const QString &b)
{
    const auto isDigit = [](QChar c) { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); };

    qsizetype i = 0;
    qsizetype j = 0;
    while (i < a.size() && j < b.size())
    {
        const bool digitA = isDigit(a[i]);
        const bool digitB = isDigit(b[j]);

        qsizetype endA = i;
        while (endA < a.size() && isDigit(a[endA]) == digitA)
            ++endA;
        qsizetype endB = j;
        while (endB < b.size() && isDigit(b[endB]) == digitB)
            ++endB;

        int result;
        if (digitA && digitB)
        {
            // compare the values: skip the leading zeros, then the longer
            // number is the larger one
            qsizetype startA = i;
            while (startA < endA - 1 && a[startA] == QLatin1Char('0'))
                ++startA;
            qsizetype startB = j;
            while (startB < endB - 1 && b[startB] == QLatin1Char('0'))
                ++startB;
            const QStringView numberA = QStringView(a).mid(startA, endA - startA);
            const QStringView numberB = QStringView(b).mid(startB, endB - startB);
            if (numberA.size() != numberB.size())
                result = numberA.size() < numberB.size() ? -1 : 1;
            else
                result = numberA.compare(numberB);
        }
        else
            result = collator.compare(QStringView(a).mid(i, endA - i), QStringView(b).mid(j, endB - j));

        if (result != 0)
            return result;

        i = endA;
        j = endB;
    }

    if (i < a.size())
        return 1;
    if (j < b.size())
        return -1;
    return 0;
}

std::vector<std::unique_ptr<MLItem>>
mlfoldertree::page(std::vector<std::unique_ptr<MLItem>> rows, const vlc_ml_query_params_t *queryParams)
{
    const size_t first = std::min<size_t>(queryParams->i_offset, rows.size());
    const size_t last = (queryParams->i_nbResults == 0)
                        ? rows.size()
                        : std::min<size_t>(first + queryParams->i_nbResults, rows.size());

    return { std::make_move_iterator(rows.begin() + first),
             std::make_move_iterator(rows.begin() + last) };
}

// MLFolderTreeModel

MLFolderTreeModel::MLFolderTreeModel(QObject *parent)
    : MLVideoModel(parent)
{
    connect(this, &MLBaseModel::parentIdChanged, this, &MLFolderTreeModel::updatePath);
    connect(this, &MLBaseModel::mlChanged, this, &MLFolderTreeModel::updatePath);
}

QHash<int, QByteArray> MLFolderTreeModel::roleNames() const
{
    QHash<int, QByteArray> hash = MLVideoModel::roleNames();

    hash.insert({
        {FOLDER_IS_FOLDER, QByteArrayLiteral("isFolder")},
        {FOLDER_NB_VIDEOS, QByteArrayLiteral("nb_videos")},
    });

    return hash;
}

QVariant MLFolderTreeModel::itemRoleData(const MLItem *item, int role) const
{
    if (item == nullptr)
        return {};

    if (!mlfoldertree::isFolder(item))
    {
        switch (role)
        {
        case FOLDER_IS_FOLDER:
            return QVariant::fromValue(false);
        case FOLDER_NB_VIDEOS:
            return {};
        default:
            return MLVideoModel::itemRoleData(item, role);
        }
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
    case VIDEO_THUMBNAIL:
        return MLCustomCover::url(folder->getId()
                                  , QSize(FOLDER_COVER_WIDTH, FOLDER_COVER_HEIGHT)
                                  , QStringLiteral(":/placeholder/noart_videoCover.svg"));
    case MEDIA_DURATION:
        // the duration counts the folder's own videos only, hide it without
        if (folder->getDuration().asMilliseconds() <= 0)
            return {};
        return QVariant::fromValue(folder->getDuration());
    case MEDIA_MRL:
        return QVariant::fromValue(folder->getMRL());
    case MEDIA_IS_VIDEO:
        return QVariant::fromValue(false);
    case FOLDER_IS_FOLDER:
        return QVariant::fromValue(true);
    case FOLDER_NB_VIDEOS:
        return QVariant::fromValue(folder->getVideoCount());
    default:
        // NOTE: The media roles (isNew, isLocal...) stay undefined, so that
        // the media actions of the context menu don't apply to folders.
        return {};
    }
}

vlc_ml_sorting_criteria_t MLFolderTreeModel::nameToCriteria(QByteArray name) const
{
    return QHash<QByteArray, vlc_ml_sorting_criteria_t> {
        {QByteArrayLiteral("title"), VLC_ML_SORTING_ALPHA},
        {QByteArrayLiteral("insertion"), VLC_ML_SORTING_INSERTIONDATE},
    }.value(name, VLC_ML_SORTING_DEFAULT);
}

std::unique_ptr<MLListCacheLoader> MLFolderTreeModel::createMLLoader() const
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLFolderTreeModel::Loader>(*this));
}

void MLFolderTreeModel::onVlcMlEvent(const MLEvent &event)
{
    if (mlfoldertree::changesFolders(event.i_type))
    {
        emit resetRequested();
        return;
    }

    MLVideoModel::onVlcMlEvent(event);
}

void MLFolderTreeModel::updatePath()
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

MLFolderTreeModel::Loader::Loader(const MLFolderTreeModel &model)
    : MLOp(model)
    , m_criteria(model.getSortCriteria())
{
}

size_t MLFolderTreeModel::Loader::count(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *) const
{
    return rows(ml).size();
}

std::vector<std::unique_ptr<MLItem>>
MLFolderTreeModel::Loader::load(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *queryParams) const
{
    // the levels of a folder tree are small, so each load lists the whole
    // level and returns the requested page of it
    return mlfoldertree::page(rows(ml), queryParams);
}

std::unique_ptr<MLItem>
MLFolderTreeModel::Loader::loadItemById(vlc_medialibrary_t *ml, MLItemId itemId) const
{
    if (itemId.type == VLC_ML_PARENT_FOLDER)
    {
        ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder(ml, itemId.id));
        if (!folder)
            return nullptr;
        return std::make_unique<MLFolder>(folder.get());
    }

    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLVideo>(media.get());
}

std::vector<std::unique_ptr<MLItem>>
MLFolderTreeModel::Loader::rows(vlc_medialibrary_t *ml) const
{
    const QString pattern = QString::fromUtf8(m_searchPattern);
    const auto matches = [&pattern](const QString &title) {
        return pattern.isEmpty() || title.contains(pattern, Qt::CaseInsensitive);
    };

    const bool byTitle = (m_criteria != QLatin1String("insertion")
                          && m_criteria != QLatin1String("unwatched"));
    const bool titleDesc = byTitle && m_sortDesc && !m_criteria.isEmpty();

    // folders, by title

    std::vector<std::unique_ptr<MLFolder>> folders =
        mlfoldertree::folders(ml, m_parent, VLC_ML_MEDIA_TYPE_VIDEO, pattern, titleDesc);

    // videos

    std::vector<std::unique_ptr<MLVideo>> videos;
    if (m_parent.id > 0)
    {
        vlc_ml_query_params_t params = vlc_ml_query_params_create();
        if (m_criteria == QLatin1String("insertion"))
        {
            params.i_sort = VLC_ML_SORTING_INSERTIONDATE;
            params.b_desc = m_sortDesc;
        }
        else
            params.i_sort = VLC_ML_SORTING_ALPHA;

        ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_video_of(ml, &params,
                                                                      VLC_ML_PARENT_FOLDER,
                                                                      m_parent.id));
        if (list)
        {
            for (const vlc_ml_media_t &media : ml_range_iterate<vlc_ml_media_t>(list))
            {
                auto video = std::make_unique<MLVideo>(&media);
                if (matches(video->title()))
                    videos.push_back(std::move(video));
            }
        }
    }

    // sort the videos, titles in natural order

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    if (m_criteria == QLatin1String("unwatched"))
    {
        const auto rank = m_sortDesc ? watchedFirstRank : unwatchedFirstRank;
        std::stable_sort(videos.begin(), videos.end(),
                         [&collator, rank](const std::unique_ptr<MLVideo> &a, const std::unique_ptr<MLVideo> &b) {
            const int rankA = rank(*a);
            const int rankB = rank(*b);
            if (rankA != rankB)
                return rankA < rankB;
            return mlfoldertree::naturalCompare(collator, a->title(), b->title()) < 0;
        });
    }
    else if (byTitle)
    {
        std::stable_sort(videos.begin(), videos.end(),
                         [&collator, titleDesc](const std::unique_ptr<MLVideo> &a, const std::unique_ptr<MLVideo> &b) {
            return titleDesc ? mlfoldertree::naturalCompare(collator, b->title(), a->title()) < 0
                             : mlfoldertree::naturalCompare(collator, a->title(), b->title()) < 0;
        });
    }

    std::vector<std::unique_ptr<MLItem>> result;
    result.reserve(folders.size() + videos.size());
    for (auto &folder : folders)
        result.push_back(std::move(folder));
    for (auto &video : videos)
        result.push_back(std::move(video));
    return result;
}
