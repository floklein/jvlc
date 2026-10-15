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

#include "mlfolderactions.hpp"

#include <vlc_common.h>
#include <vlc_media_library.h>

#include "medialib.hpp"
#include "mlfoldertreemodel.hpp"
#include "mlhelper.hpp"

#include <QCollator>
#include <QDir>
#include <QJSEngine>
#include <QUrl>

#include <algorithm>

namespace {

// guards the walk down the tree against a corrupted library
const int MAX_FOLDER_DEPTH = 64;

struct MediaInfo
{
    int64_t id;
    QString title;
    bool watched;
    bool inProgress;
    int64_t duration;
    int64_t size;
};

ml_unique_ptr<vlc_ml_media_list_t> listMedia(vlc_medialibrary_t *ml, int64_t folder, int type)
{
    vlc_ml_query_params_t params = vlc_ml_query_params_create();
    params.i_sort = VLC_ML_SORTING_ALPHA;
    switch (type)
    {
    case VLC_ML_MEDIA_TYPE_VIDEO:
        return ml_unique_ptr<vlc_ml_media_list_t>(
            vlc_ml_list_video_of(ml, &params, VLC_ML_PARENT_FOLDER, folder));
    case VLC_ML_MEDIA_TYPE_AUDIO:
        return ml_unique_ptr<vlc_ml_media_list_t>(
            vlc_ml_list_audio_of(ml, &params, VLC_ML_PARENT_FOLDER, folder));
    default:
        return ml_unique_ptr<vlc_ml_media_list_t>(
            vlc_ml_list_media_of(ml, &params, VLC_ML_PARENT_FOLDER, folder));
    }
}

// Appends the media of the folder and its subfolders to media, in the order
// of the folder tree; returns how many were found. Counts the subfolders
// holding some in folders.
int walk(vlc_medialibrary_t *ml, const QCollator &collator, int64_t folder, int type,
         int depth, std::vector<MediaInfo> &media, int &folders)
{
    if (depth > MAX_FOLDER_DEPTH)
        return 0;

    int found = 0;

    // the subfolders, by title in natural order
    struct Subfolder { int64_t id; QString title; };
    std::vector<Subfolder> subfolders;
    {
        vlc_ml_query_params_t params = vlc_ml_query_params_create();
        params.i_sort = VLC_ML_SORTING_ALPHA;
        ml_unique_ptr<vlc_ml_folder_list_t> list(vlc_ml_list_subfolders(ml, &params, folder));
        if (list)
        {
            for (const vlc_ml_folder_t &data : ml_range_iterate<vlc_ml_folder_t>(list))
            {
                if (!data.b_banned)
                    subfolders.push_back({ data.i_id, qfu(data.psz_name) });
            }
        }
    }
    std::stable_sort(subfolders.begin(), subfolders.end(),
                     [&collator](const Subfolder &a, const Subfolder &b) {
        return mlfoldertree::naturalCompare(collator, a.title, b.title) < 0;
    });

    for (const Subfolder &subfolder : subfolders)
    {
        const int count = walk(ml, collator, subfolder.id, type, depth + 1, media, folders);
        if (count > 0)
            ++folders;
        found += count;
    }

    // then the folder's own media, by title in natural order
    std::vector<MediaInfo> own;
    ml_unique_ptr<vlc_ml_media_list_t> list = listMedia(ml, folder, type);
    if (list)
    {
        for (const vlc_ml_media_t &data : ml_range_iterate<vlc_ml_media_t>(list))
        {
            int64_t size = 0;
            if (data.p_files)
            {
                for (const vlc_ml_file_t &file : ml_range_iterate<vlc_ml_file_t>(data.p_files))
                {
                    if (file.i_type == VLC_ML_FILE_TYPE_MAIN)
                        size += file.i_size;
                }
            }
            own.push_back({ data.i_id, qfu(data.psz_title),
                            data.i_playcount > 0 && data.f_progress <= 0.f,
                            data.f_progress > 0.f,
                            data.i_duration, size });
        }
    }
    std::stable_sort(own.begin(), own.end(), [&collator](const MediaInfo &a, const MediaInfo &b) {
        return mlfoldertree::naturalCompare(collator, a.title, b.title) < 0;
    });

    found += static_cast<int>(own.size());
    media.insert(media.end(), own.begin(), own.end());
    return found;
}

std::vector<MediaInfo> folderMedia(vlc_medialibrary_t *ml, int64_t folder, int type, int *folders = nullptr)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    std::vector<MediaInfo> media;
    int count = 0;
    walk(ml, collator, folder, type, 0, media, count);
    if (folders)
        *folders = count;
    return media;
}

QVariantMap describeFolder(vlc_medialibrary_t *ml, int64_t id, int type)
{
    ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder(ml, id));
    if (!folder)
        return {};
    ml_unique_ptr<vlc_ml_folder_t> parent(vlc_ml_get_folder_parent(ml, id));

    int folders = 0;
    const std::vector<MediaInfo> media = folderMedia(ml, id, type, &folders);

    int watched = 0;
    qint64 duration = 0;
    qint64 size = 0;
    for (const MediaInfo &info : media)
    {
        if (info.watched)
            ++watched;
        duration += std::max<int64_t>(info.duration, 0);
        size += std::max<int64_t>(info.size, 0);
    }

    const QString mrl = qfu(folder->psz_mrl);
    const QUrl url(mrl);
    QString path;
    if (url.isLocalFile())
    {
        path = QDir::toNativeSeparators(QDir::cleanPath(url.toLocalFile()));
        // a drive keeps its separator ("C:\")
        if (path.endsWith(QDir::separator()) && !path.endsWith(QLatin1String(":") + QDir::separator()))
            path.chop(1);
    }

    return QVariantMap {
        { QStringLiteral("id"), QVariant::fromValue(MLItemId(id, VLC_ML_PARENT_FOLDER)) },
        { QStringLiteral("type"), type },
        { QStringLiteral("name"), qfu(folder->psz_name) },
        { QStringLiteral("mrl"), mrl },
        { QStringLiteral("path"), path },
        { QStringLiteral("added"), parent == nullptr },
        { QStringLiteral("media"), static_cast<int>(media.size()) },
        { QStringLiteral("folders"), folders },
        { QStringLiteral("watched"), watched },
        { QStringLiteral("unwatched"), static_cast<int>(media.size()) - watched },
        { QStringLiteral("duration"), duration },
        { QStringLiteral("size"), size },
    };
}

} // namespace

MLFolderActions::MLFolderActions(MediaLib *ml, QObject *parent)
    : QObject(parent)
    , m_mediaLib(ml)
{
}

void MLFolderActions::play(const MLItemId &folder, int type)
{
    toQueue(folder, type, Play);
}

void MLFolderActions::shuffle(const MLItemId &folder, int type)
{
    toQueue(folder, type, Shuffle);
}

void MLFolderActions::addToQueue(const MLItemId &folder, int type)
{
    toQueue(folder, type, Enqueue);
}

void MLFolderActions::playNext(const MLItemId &folder, int type)
{
    toQueue(folder, type, PlayNext);
}

void MLFolderActions::toQueue(const MLItemId &folder, int type, QueueAction action)
{
    if (!m_mediaLib || folder.type != VLC_ML_PARENT_FOLDER)
        return;

    struct Ctx {
        QVariantList ids;
    };

    m_mediaLib->runOnMLThread<Ctx>(this,
    // ML thread
    [id = folder.id, type](vlc_medialibrary_t *ml, Ctx &ctx)
    {
        for (const MediaInfo &info : folderMedia(ml, id, type))
            ctx.ids.push_back(QVariant::fromValue(MLItemId(info.id, VLC_ML_PARENT_UNKNOWN)));
    },
    // UI thread
    [this, action](quint64, Ctx &ctx)
    {
        if (ctx.ids.isEmpty())
            return;
        switch (action)
        {
        case Play:
            m_mediaLib->addAndPlay(ctx.ids);
            break;
        case Shuffle:
            m_mediaLib->shuffleAndPlay(ctx.ids);
            break;
        case Enqueue:
            /* MediaLib keeps the upstream name: it adds to the Queue */
            m_mediaLib->addToPlaylist(ctx.ids);
            break;
        case PlayNext:
            m_mediaLib->playNext(ctx.ids);
            break;
        }
    });
}

void MLFolderActions::setWatched(const MLItemId &folder, int type, bool watched)
{
    if (!m_mediaLib || folder.type != VLC_ML_PARENT_FOLDER)
        return;

    m_mediaLib->runOnMLThread(this,
    // ML thread
    [id = folder.id, type, watched](vlc_medialibrary_t *ml)
    {
        for (const MediaInfo &info : folderMedia(ml, id, type))
        {
            // a watched media isn't played once more, an unwatched one
            // isn't touched
            if (watched && info.inProgress)
                // played to its end: the progress goes too
                vlc_ml_media_update_progress(ml, info.id, 1.0);
            else if (watched && !info.watched)
                vlc_ml_media_set_played(ml, info.id, true);
            else if (!watched && (info.watched || info.inProgress))
                vlc_ml_media_set_played(ml, info.id, false);
        }
    });
}

void MLFolderActions::removeFromLibrary(const QString &mrl)
{
    if (!m_mediaLib || mrl.isEmpty())
        return;
    m_mediaLib->runOnMLThread(this, [mrl = mrl.toStdString()](vlc_medialibrary_t *ml)
    {
        vlc_ml_remove_folder(ml, mrl.c_str());
    });
}

void MLFolderActions::excludeFromLibrary(const QString &mrl)
{
    if (!m_mediaLib || mrl.isEmpty())
        return;
    m_mediaLib->runOnMLThread(this, [mrl = mrl.toStdString()](vlc_medialibrary_t *ml)
    {
        vlc_ml_ban_folder(ml, mrl.c_str());
    });
}

void MLFolderActions::rescan(const QString &mrl)
{
    if (!m_mediaLib)
        return;
    m_mediaLib->runOnMLThread(this, [mrl = mrl.toStdString()](vlc_medialibrary_t *ml)
    {
        vlc_ml_reload_folder(ml, mrl.empty() ? nullptr : mrl.c_str());
    });
}

void MLFolderActions::describe(const MLItemId &folder, int type,
                               std::function<void(const QVariantMap &)> callback)
{
    if (!m_mediaLib || folder.type != VLC_ML_PARENT_FOLDER)
    {
        callback({});
        return;
    }

    struct Ctx {
        QVariantMap description;
    };

    m_mediaLib->runOnMLThread<Ctx>(this,
    // ML thread
    [id = folder.id, type](vlc_medialibrary_t *ml, Ctx &ctx)
    {
        ctx.description = describeFolder(ml, id, type);
    },
    // UI thread
    [callback](quint64, Ctx &ctx)
    {
        callback(ctx.description);
    });
}

void MLFolderActions::describe(const MLItemId &folder, int type, QJSValue callback)
{
    if (!callback.isCallable())
        return;

    describe(folder, type, [this, callback](const QVariantMap &description) mutable {
        QJSEngine *engine = qjsEngine(this);
        if (!engine)
            return;
        callback.call({ engine->toScriptValue(description) });
    });
}

void MLFolderActions::showProperties(const MLItemId &folder, int type)
{
    describe(folder, type, [this](const QVariantMap &description) {
        if (!description.isEmpty())
            emit propertiesRequested(description);
    });
}
