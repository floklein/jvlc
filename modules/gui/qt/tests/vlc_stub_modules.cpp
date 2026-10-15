/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Define builtin modules for mocked parts */
#define MODULE_NAME module_faker
#undef VLC_DYNAMIC_PLUGIN

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>
#include <vlc_probe.h>
#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_media_library.h>
#include <vlc_meta.h>
#include <vlc_fingerprinter.h>
#include <vlc_aout.h>
#include <vlc_demux.h>
#include <vlc_es_out.h>

#include "../../../../lib/libvlc_internal.h"

#include "qt.hpp"
#include <QUrl>

#include <algorithm>

#include "vlc_stub_modules.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>

#include <QStringList>

static VLCTestingEnv* testenv = nullptr;

///RenderDiscovery module

static int OpenRD( vlc_object_t* p_this )
{
    testenv->renderDiscovery = (vlc_renderer_discovery_t *)p_this;
    return VLC_SUCCESS;
}

static void CloseRD( vlc_object_t* )
{
    testenv->renderDiscovery = nullptr;
}

static int vlc_rd_probe_open(vlc_object_t *obj) {
    auto probe = (struct vlc_probe_t *)obj;

    if (testenv->renderDiscoveryProbeEnabled)
        vlc_rd_probe_add(probe, "rd", "a fake renderer for testing purpose");
    //only probe ourself
    return VLC_PROBE_STOP;
}

///Interface module

namespace vlc {
class Compositor {};
}

static int OpenIntf(vlc_object_t* p_this)
{
    auto intfThread = reinterpret_cast<intf_thread_t*>(p_this);
    libvlc_int_t* libvlc = vlc_object_instance( p_this );

    /* Ensure initialization of objects in qt_intf_t. */
    qt_intf_t* qt_intf = vlc_object_create<qt_intf_t>( libvlc );
    if (!qt_intf)
        return VLC_ENOMEM;

    qt_intf->obj.logger = vlc_LogHeaderCreate(libvlc->obj.logger, "qt");
    if (!qt_intf->obj.logger)
    {
        vlc_object_delete(qt_intf);
        return VLC_EGENERIC;
    }

    qt_intf->p_playlist = vlc_intf_GetMainPlaylist(intfThread);
    qt_intf->p_player = vlc_playlist_GetPlayer(qt_intf->p_playlist);
    qt_intf->intf = intfThread;

    intfThread->p_sys = reinterpret_cast<intf_sys_t*>(qt_intf);

    testenv->intf = qt_intf;

    return VLC_SUCCESS;
}

static void CloseIntf( vlc_object_t *p_this )
{
    intf_thread_t* intfThread = (intf_thread_t*)(p_this);
    auto qt_intf = reinterpret_cast<qt_intf_t*>(intfThread->p_sys);
    if (!qt_intf)
        return;
    vlc_LogDestroy(qt_intf->obj.logger);
    vlc_object_delete(qt_intf);

    testenv->intf = nullptr;
}

//Medialib module

namespace {

struct StubMediaLibrary
{
    mutable std::mutex lock;
    vlc_medialibrary_module_t* module = nullptr;
    std::vector<VLCTestingEnv::Playlist> playlists;
    int64_t nextPlaylistId = 1;
    // external media: id -> mrl
    std::map<int64_t, std::string> external;
    int64_t nextExternalId = 1000;
    // external subtitle files of a media (Subtitle matches): id -> mrls
    std::map<int64_t, std::vector<std::string>> subtitles;
    std::vector<VLCTestingEnv::HistoryEntry> history;
    int historyClears = 0;
};

StubMediaLibrary stubML;

VLCTestingEnv::Playlist* findPlaylist(int64_t id)
{
    for (auto& playlist : stubML.playlists)
        if (playlist.id == id)
            return &playlist;
    return nullptr;
}

void fillPlaylist(vlc_ml_playlist_t* out, const VLCTestingEnv::Playlist& playlist)
{
    memset(out, 0, sizeof(*out));
    out->i_id = playlist.id;
    out->psz_name = strdup(playlist.name.c_str());
    out->i_nb_media = playlist.media.size();
    out->i_nb_unknown = playlist.media.size();
    out->b_is_favorite = playlist.favorite;
    /* creation order */
    out->i_creation_date = playlist.id;
}

void fillMedia(vlc_ml_media_t* out, int64_t id)
{
    memset(out, 0, sizeof(*out));
    out->i_id = id;
    out->i_type = VLC_ML_MEDIA_TYPE_UNKNOWN;
    char name[32];
    snprintf(name, sizeof(name), "ml:%" PRId64, id);
    out->psz_title = strdup(name);
    /* the medialibrary always gives the lists, even empty */
    out->p_files = static_cast<vlc_ml_file_list_t*>(calloc(1, sizeof(vlc_ml_file_list_t)));
    out->p_tracks = static_cast<vlc_ml_media_track_list_t*>(calloc(1, sizeof(vlc_ml_media_track_list_t)));
}

/* the main file and the external subtitle files of a media */
void fillFiles(vlc_ml_media_t* out, int64_t id)
{
    auto subtitles = stubML.subtitles.find(id);
    const size_t count = subtitles == stubML.subtitles.end() ? 0 : subtitles->second.size();
    auto it = stubML.external.find(id);
    const bool hasMain = it != stubML.external.end();

    free(out->p_files);
    out->p_files = static_cast<vlc_ml_file_list_t*>(
        calloc(1, sizeof(vlc_ml_file_list_t) + (count + 1) * sizeof(vlc_ml_file_t)));
    size_t n = 0;
    if (hasMain)
    {
        vlc_ml_file_t& file = out->p_files->p_items[n++];
        file.psz_mrl = strdup(it->second.c_str());
        file.i_type = VLC_ML_FILE_TYPE_MAIN;
        file.b_present = true;
    }
    for (size_t i = 0; i < count; ++i)
    {
        vlc_ml_file_t& file = out->p_files->p_items[n++];
        file.psz_mrl = strdup(subtitles->second[i].c_str());
        file.i_type = VLC_ML_FILE_TYPE_SUBTITLE;
        file.b_external = true;
        file.b_present = true;
    }
    out->p_files->i_nb_items = n;
}

void sendEvent(int type, int64_t id, const VLCTestingEnv::Playlist* created = nullptr)
{
    vlc_medialibrary_module_t* module = stubML.module;
    if (!module || !module->cbs)
        return;

    vlc_ml_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.i_type = type;
    vlc_ml_playlist_t playlist;
    if (type == VLC_ML_EVENT_PLAYLIST_ADDED)
    {
        fillPlaylist(&playlist, *created);
        ev.creation.p_playlist = &playlist;
    }
    else if (type == VLC_ML_EVENT_HISTORY_CHANGED)
        ev.history_changed.history_type = VLC_ML_HISTORY_TYPE_GLOBAL;
    else if (type == VLC_ML_EVENT_PLAYLIST_DELETED)
        ev.deletion.i_entity_id = id;
    else
        ev.modification.i_entity_id = id;

    module->cbs->pf_send_event(module, &ev);

    if (type == VLC_ML_EVENT_PLAYLIST_ADDED)
        free(playlist.psz_name);
}

void sendFavoriteEvent(int64_t id, vlc_ml_parent_type type, bool favorite)
{
    vlc_medialibrary_module_t* module = stubML.module;
    if (!module || !module->cbs)
        return;

    vlc_ml_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.i_type = VLC_ML_EVENT_FAVORITES_CHANGED;
    ev.favorites_changed.i_entity_id = id;
    ev.favorites_changed.i_entity_type = type;
    ev.favorites_changed.b_favorite = favorite;
    module->cbs->pf_send_event(module, &ev);
}

vlc_ml_media_list_t* newMediaList(const std::vector<int64_t>& ids, const vlc_ml_query_params_t* params)
{
    size_t offset = params ? params->i_offset : 0;
    size_t count = (params && params->i_nbResults) ? params->i_nbResults : ids.size();
    if (offset > ids.size())
        offset = ids.size();
    count = std::min(count, ids.size() - offset);

    auto list = static_cast<vlc_ml_media_list_t*>(
        calloc(1, sizeof(vlc_ml_media_list_t) + count * sizeof(vlc_ml_media_t)));
    list->i_nb_items = count;
    for (size_t i = 0; i < count; ++i)
        fillMedia(&list->p_items[i], ids[offset + i]);
    return list;
}

std::vector<VLCTestingEnv::Playlist> sortedPlaylists(const vlc_ml_query_params_t* params)
{
    std::vector<VLCTestingEnv::Playlist> result = stubML.playlists;
    if (params && params->b_favorite_only)
        result.erase(std::remove_if(result.begin(), result.end(),
                                    [](const VLCTestingEnv::Playlist& p) { return !p.favorite; }),
                     result.end());
    const int sort = params ? params->i_sort : VLC_ML_SORTING_DEFAULT;
    const bool desc = params && params->b_desc;
    std::stable_sort(result.begin(), result.end(),
                     [sort](const VLCTestingEnv::Playlist& a, const VLCTestingEnv::Playlist& b) {
        if (sort == VLC_ML_SORTING_INSERTIONDATE)
            return a.id < b.id;
        return a.name < b.name;
    });
    if (desc)
        std::reverse(result.begin(), result.end());
    return result;
}

} // anonymous namespace

using FakeFolder = VLCTestingEnv::FakeFolder;
using FakeMedia = VLCTestingEnv::FakeMedia;

static const FakeFolder* fakeFolder(int64_t id)
{
    for (const FakeFolder& folder : testenv->mlFolders)
        if (folder.id == id)
            return &folder;
    return nullptr;
}

static QString fakeFolderMrl(const FakeFolder& folder)
{
    const FakeFolder* parent = fakeFolder(folder.parentId);
    const QString parentMrl = parent ? fakeFolderMrl(*parent) : QStringLiteral("file:///library/");
    // percent-encoded, like the MRLs of the medialibrary
    return parentMrl + QString::fromUtf8(QUrl::toPercentEncoding(folder.name)) + QLatin1Char('/');
}

static bool fakeFolderHolds(const FakeFolder& folder, int type)
{
    for (const FakeMedia& media : testenv->mlMedia)
        if (media.folderId == folder.id && (type == VLC_ML_MEDIA_TYPE_UNKNOWN || media.type == type))
            return true;
    return false;
}

static void fillFolder(vlc_ml_folder_t* out, const FakeFolder& folder)
{
    out->i_id = folder.id;
    out->psz_name = strdup(qtu(folder.name));
    out->psz_mrl = strdup(qtu(fakeFolderMrl(folder)));
    for (const FakeMedia& media : testenv->mlMedia)
    {
        if (media.folderId != folder.id)
            continue;
        out->i_nb_media++;
        if (media.type == VLC_ML_MEDIA_TYPE_VIDEO)
            out->i_nb_video++;
        else if (media.type == VLC_ML_MEDIA_TYPE_AUDIO)
            out->i_nb_audio++;
    }
    out->b_present = true;
}

// the names of the artists (or albums) of the media, in order of appearance:
// the id of a name is its position + 1
static QStringList fakeNames(QString FakeMedia::*field)
{
    QStringList names;
    for (const FakeMedia& media : testenv->mlMedia)
    {
        const QString& name = media.*field;
        if (!name.isEmpty() && !names.contains(name))
            names.push_back(name);
    }
    return names;
}

static int64_t fakeNameId(QString FakeMedia::*field, const QString& name)
{
    if (name.isEmpty())
        return 0;
    return fakeNames(field).indexOf(name) + 1;
}

static QString fakeName(QString FakeMedia::*field, int64_t id)
{
    const QStringList names = fakeNames(field);
    if (id <= 0 || id > names.size())
        return {};
    return names.at(id - 1);
}

static void fillFakeMedia(vlc_ml_media_t* out, const FakeMedia& media)
{
    out->i_id = media.id;
    out->i_type = media.type;
    if (media.type == VLC_ML_MEDIA_TYPE_AUDIO)
    {
        out->i_subtype = VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK;
        out->album_track.i_artist_id = fakeNameId(&FakeMedia::artist, media.artist);
        out->album_track.i_album_id = fakeNameId(&FakeMedia::album, media.album);
        out->album_track.i_track_nb = media.trackNumber;
        out->album_track.i_disc_nb = media.discNumber;
    }
    out->i_duration = 60000;
    out->i_playcount = media.playCount;
    out->f_progress = media.progress;
    out->psz_title = strdup(qtu(media.title));
    out->psz_filename = strdup(qtu(media.title + QStringLiteral(".mkv")));

    out->p_files = static_cast<vlc_ml_file_list_t*>(
        calloc(1, sizeof(vlc_ml_file_list_t) + sizeof(vlc_ml_file_t)));
    out->p_files->i_nb_items = 1;
    vlc_ml_file_t& file = out->p_files->p_items[0];
    const FakeFolder* folder = fakeFolder(media.folderId);
    file.psz_mrl = strdup(qtu((folder ? fakeFolderMrl(*folder) : QString())
                              + QString::fromUtf8(QUrl::toPercentEncoding(media.title + QStringLiteral(".mkv")))));
    file.i_type = VLC_ML_FILE_TYPE_MAIN;
    file.b_present = true;

    out->p_tracks = static_cast<vlc_ml_media_track_list_t*>(
        calloc(1, sizeof(vlc_ml_media_track_list_t)));
}

static int outputFolders(bool list, const std::vector<const FakeFolder*>& folders, va_list args)
{
    if (!list)
    {
        *va_arg(args, size_t*) = folders.size();
        return VLC_SUCCESS;
    }
    auto res = static_cast<vlc_ml_folder_list_t*>(
        calloc(1, sizeof(vlc_ml_folder_list_t) + folders.size() * sizeof(vlc_ml_folder_t)));
    res->i_nb_items = folders.size();
    for (size_t i = 0; i < folders.size(); ++i)
        fillFolder(&res->p_items[i], *folders[i]);
    *va_arg(args, vlc_ml_folder_list_t**) = res;
    return VLC_SUCCESS;
}

static int outputMedia(bool list, const std::vector<const FakeMedia*>& media, va_list args)
{
    if (!list)
    {
        *va_arg(args, size_t*) = media.size();
        return VLC_SUCCESS;
    }
    auto res = static_cast<vlc_ml_media_list_t*>(
        calloc(1, sizeof(vlc_ml_media_list_t) + media.size() * sizeof(vlc_ml_media_t)));
    res->i_nb_items = media.size();
    for (size_t i = 0; i < media.size(); ++i)
        fillFakeMedia(&res->p_items[i], *media[i]);
    *va_arg(args, vlc_ml_media_list_t**) = res;
    return VLC_SUCCESS;
}

static void* MLGet( vlc_medialibrary_module_t*, int query, va_list args )
{
    switch (query)
    {
    case VLC_ML_GET_INPUT_ITEM:
    {
        // a media that plays long enough for tests, named after its id
        int64_t id = va_arg(args, int64_t);
        char name[32];
        snprintf(name, sizeof(name), "ml:%" PRId64, id);
        std::string mrl = "vlc://pause:600";
        {
            std::lock_guard<std::mutex> guard(stubML.lock);
            auto it = stubML.external.find(id);
            if (it != stubML.external.end())
                mrl = it->second;
        }
        return input_item_New(mrl.c_str(), name);
    }
    case VLC_ML_GET_PLAYLIST:
    {
        int64_t id = va_arg(args, int64_t);
        std::lock_guard<std::mutex> guard(stubML.lock);
        const VLCTestingEnv::Playlist* playlist = findPlaylist(id);
        if (!playlist)
            return nullptr;
        auto out = static_cast<vlc_ml_playlist_t*>(malloc(sizeof(vlc_ml_playlist_t)));
        fillPlaylist(out, *playlist);
        return out;
    }
    case VLC_ML_GET_MEDIA_BY_MRL:
    {
        const char* mrl = va_arg(args, const char*);
        std::lock_guard<std::mutex> guard(stubML.lock);
        for (const auto& it : stubML.external)
        {
            if (it.second == mrl)
            {
                auto out = static_cast<vlc_ml_media_t*>(malloc(sizeof(vlc_ml_media_t)));
                fillMedia(out, it.first);
                fillFiles(out, it.first);
                return out;
            }
        }
        return nullptr;
    }
    case VLC_ML_GET_MEDIA:
    {
        int64_t id = va_arg(args, int64_t);
        for (const FakeMedia& media : testenv->mlMedia)
        {
            if (media.id != id)
                continue;
            auto res = static_cast<vlc_ml_media_t*>(calloc(1, sizeof(vlc_ml_media_t)));
            fillFakeMedia(res, media);
            return res;
        }
        return nullptr;
    }
    case VLC_ML_GET_FOLDER:
    case VLC_ML_GET_FOLDER_PARENT:
    {
        const FakeFolder* folder = fakeFolder(va_arg(args, int64_t));
        if (folder && query == VLC_ML_GET_FOLDER_PARENT)
            folder = fakeFolder(folder->parentId);
        if (!folder)
            return nullptr;
        auto res = static_cast<vlc_ml_folder_t*>(calloc(1, sizeof(vlc_ml_folder_t)));
        fillFolder(res, *folder);
        return res;
    }
    case VLC_ML_GET_FOLDER_BY_MRL:
    {
        // the medialibrary module adds the trailing slash itself
        QString mrl = qfu(va_arg(args, const char*));
        if (!mrl.endsWith(QLatin1Char('/')))
            mrl += QLatin1Char('/');
        for (const FakeFolder& folder : testenv->mlFolders)
        {
            if (fakeFolderMrl(folder) != mrl)
                continue;
            auto res = static_cast<vlc_ml_folder_t*>(calloc(1, sizeof(vlc_ml_folder_t)));
            fillFolder(res, folder);
            return res;
        }
        return nullptr;
    }
    case VLC_ML_GET_ARTIST:
    {
        const int64_t id = va_arg(args, int64_t);
        const QString name = fakeName(&FakeMedia::artist, id);
        if (name.isEmpty())
            return nullptr;
        auto res = static_cast<vlc_ml_artist_t*>(calloc(1, sizeof(vlc_ml_artist_t)));
        res->i_id = id;
        res->psz_name = strdup(qtu(name));
        return res;
    }
    case VLC_ML_GET_ALBUM:
    {
        const int64_t id = va_arg(args, int64_t);
        const QString title = fakeName(&FakeMedia::album, id);
        if (title.isEmpty())
            return nullptr;
        auto res = static_cast<vlc_ml_album_t*>(calloc(1, sizeof(vlc_ml_album_t)));
        res->i_id = id;
        res->psz_title = strdup(qtu(title));
        return res;
    }
    default:
        return nullptr;
    }
}

static int MLList( vlc_medialibrary_module_t*, int query,
                const vlc_ml_query_params_t* params, va_list args )
{
    std::lock_guard<std::mutex> guard(stubML.lock);
    std::vector<const FakeFolder*> folders;
    std::vector<const FakeMedia*> media;

    switch (query)
    {
    case VLC_ML_LIST_PLAYLISTS:
    case VLC_ML_COUNT_PLAYLISTS:
    {
        (void) va_arg(args, int); /* playlist type: the stub has no media types */
        std::vector<VLCTestingEnv::Playlist> playlists = sortedPlaylists(params);
        if (query == VLC_ML_COUNT_PLAYLISTS)
        {
            *va_arg(args, size_t*) = playlists.size();
            return VLC_SUCCESS;
        }
        size_t offset = params ? params->i_offset : 0;
        size_t count = (params && params->i_nbResults) ? params->i_nbResults : playlists.size();
        offset = std::min(offset, playlists.size());
        count = std::min(count, playlists.size() - offset);
        auto list = static_cast<vlc_ml_playlist_list_t*>(
            calloc(1, sizeof(vlc_ml_playlist_list_t) + count * sizeof(vlc_ml_playlist_t)));
        list->i_nb_items = count;
        for (size_t i = 0; i < count; ++i)
            fillPlaylist(&list->p_items[i], playlists[offset + i]);
        *va_arg(args, vlc_ml_playlist_list_t**) = list;
        return VLC_SUCCESS;
    }
    case VLC_ML_LIST_MEDIA_OF:
    case VLC_ML_COUNT_MEDIA_OF:
    case VLC_ML_LIST_PLAYLIST_MEDIA:
    case VLC_ML_COUNT_PLAYLIST_MEDIA:
    {
        if (query == VLC_ML_LIST_MEDIA_OF || query == VLC_ML_COUNT_MEDIA_OF)
        {
            if (va_arg(args, int) != VLC_ML_PARENT_PLAYLIST)
                return VLC_EGENERIC;
        }
        const VLCTestingEnv::Playlist* playlist = findPlaylist(va_arg(args, int64_t));
        if (!playlist)
            return VLC_EGENERIC;
        if (query == VLC_ML_COUNT_MEDIA_OF || query == VLC_ML_COUNT_PLAYLIST_MEDIA)
            *va_arg(args, size_t*) = playlist->media.size();
        else
            *va_arg(args, vlc_ml_media_list_t**) = newMediaList(playlist->media, params);
        return VLC_SUCCESS;
    }
    case VLC_ML_LIST_HISTORY:
    case VLC_ML_COUNT_HISTORY:
    {
        (void) va_arg(args, int); /* history type: the stub has one history */
        if (query == VLC_ML_COUNT_HISTORY)
        {
            *va_arg(args, size_t*) = stubML.history.size();
            return VLC_SUCCESS;
        }
        std::vector<int64_t> ids;
        for (const auto& entry : stubML.history)
            ids.push_back(entry.id);
        vlc_ml_media_list_t* list = newMediaList(ids, params);
        const size_t offset = params ? std::min<size_t>(params->i_offset, ids.size()) : 0;
        for (size_t i = 0; i < list->i_nb_items; ++i)
            list->p_items[i].i_last_played_date = stubML.history[offset + i].lastPlayed;
        *va_arg(args, vlc_ml_media_list_t**) = list;
        return VLC_SUCCESS;
    }
    case VLC_ML_LIST_ENTRY_POINTS:
    case VLC_ML_COUNT_ENTRY_POINTS:
    {
        const bool banned = va_arg(args, int);
        for (const FakeFolder& folder : testenv->mlFolders)
            if (!banned && folder.parentId == 0)
                folders.push_back(&folder);
        return outputFolders(query == VLC_ML_LIST_ENTRY_POINTS, folders, args);
    }
    case VLC_ML_LIST_SUBFOLDERS:
    case VLC_ML_COUNT_SUBFOLDERS:
    {
        const int64_t parentId = va_arg(args, int64_t);
        for (const FakeFolder& folder : testenv->mlFolders)
            if (folder.parentId == parentId)
                folders.push_back(&folder);
        return outputFolders(query == VLC_ML_LIST_SUBFOLDERS, folders, args);
    }
    case VLC_ML_LIST_FOLDERS:
    case VLC_ML_COUNT_FOLDERS:
    case VLC_ML_LIST_FOLDERS_BY_TYPE:
    case VLC_ML_COUNT_FOLDERS_BY_TYPE:
    {
        // the folders holding media themselves
        int type = VLC_ML_MEDIA_TYPE_UNKNOWN;
        if (query == VLC_ML_LIST_FOLDERS_BY_TYPE || query == VLC_ML_COUNT_FOLDERS_BY_TYPE)
            type = va_arg(args, int);
        for (const FakeFolder& folder : testenv->mlFolders)
            if (fakeFolderHolds(folder, type))
                folders.push_back(&folder);
        return outputFolders(query == VLC_ML_LIST_FOLDERS || query == VLC_ML_LIST_FOLDERS_BY_TYPE,
                             folders, args);
    }
    case VLC_ML_LIST_VIDEO_OF:
    case VLC_ML_COUNT_VIDEO_OF:
    case VLC_ML_LIST_AUDIO_OF:
    case VLC_ML_COUNT_AUDIO_OF:
    {
        const int parentType = va_arg(args, int);
        const int64_t parentId = va_arg(args, int64_t);
        if (parentType != VLC_ML_PARENT_FOLDER)
            return VLC_EGENERIC;
        const bool video = (query == VLC_ML_LIST_VIDEO_OF || query == VLC_ML_COUNT_VIDEO_OF);
        const vlc_ml_media_type_t type = video ? VLC_ML_MEDIA_TYPE_VIDEO : VLC_ML_MEDIA_TYPE_AUDIO;
        for (const FakeMedia& item : testenv->mlMedia)
            if (item.folderId == parentId && item.type == type)
                media.push_back(&item);
        // the insertion order is the order of the ids
        if (params && params->i_sort == VLC_ML_SORTING_INSERTIONDATE)
        {
            std::sort(media.begin(), media.end(), [params](const FakeMedia* a, const FakeMedia* b) {
                return params->b_desc ? a->id > b->id : a->id < b->id;
            });
        }
        return outputMedia(query == VLC_ML_LIST_VIDEO_OF || query == VLC_ML_LIST_AUDIO_OF, media, args);
    }
    default:
        return VLC_EGENERIC;
    }
}

static int MLControl( vlc_medialibrary_module_t*, int query, va_list args )
{
    std::unique_lock<std::mutex> guard(stubML.lock);
    switch (query)
    {
    case VLC_ML_PLAYLIST_CREATE:
    {
        const char* name = va_arg(args, const char*);
        auto out = va_arg(args, vlc_ml_playlist_t**);
        stubML.playlists.push_back({stubML.nextPlaylistId++, name, {}});
        const VLCTestingEnv::Playlist created = stubML.playlists.back();
        if (out)
        {
            *out = static_cast<vlc_ml_playlist_t*>(malloc(sizeof(vlc_ml_playlist_t)));
            fillPlaylist(*out, created);
        }
        guard.unlock();
        sendEvent(VLC_ML_EVENT_PLAYLIST_ADDED, created.id, &created);
        return VLC_SUCCESS;
    }
    case VLC_ML_PLAYLIST_DELETE:
    {
        int64_t id = va_arg(args, int64_t);
        auto it = std::find_if(stubML.playlists.begin(), stubML.playlists.end(),
                               [id](const VLCTestingEnv::Playlist& p) { return p.id == id; });
        if (it == stubML.playlists.end())
            return VLC_EGENERIC;
        stubML.playlists.erase(it);
        guard.unlock();
        sendEvent(VLC_ML_EVENT_PLAYLIST_DELETED, id);
        return VLC_SUCCESS;
    }
    case VLC_ML_PLAYLIST_APPEND:
    case VLC_ML_PLAYLIST_INSERT:
    {
        int64_t id = va_arg(args, int64_t);
        const int64_t* ids = va_arg(args, const int64_t*);
        size_t count = va_arg(args, size_t);
        VLCTestingEnv::Playlist* playlist = findPlaylist(id);
        if (!playlist)
            return VLC_EGENERIC;
        size_t position = playlist->media.size();
        if (query == VLC_ML_PLAYLIST_INSERT)
            position = std::min<size_t>(va_arg(args, uint32_t), position);
        playlist->media.insert(playlist->media.begin() + position, ids, ids + count);
        guard.unlock();
        sendEvent(VLC_ML_EVENT_PLAYLIST_UPDATED, id);
        return VLC_SUCCESS;
    }
    case VLC_ML_PLAYLIST_REMOVE:
    {
        int64_t id = va_arg(args, int64_t);
        uint32_t position = va_arg(args, uint32_t);
        uint32_t count = va_arg(args, uint32_t);
        VLCTestingEnv::Playlist* playlist = findPlaylist(id);
        if (!playlist || position + count > playlist->media.size())
            return VLC_EGENERIC;
        playlist->media.erase(playlist->media.begin() + position,
                              playlist->media.begin() + position + count);
        guard.unlock();
        sendEvent(VLC_ML_EVENT_PLAYLIST_UPDATED, id);
        return VLC_SUCCESS;
    }
    case VLC_ML_PLAYLIST_MOVE:
    {
        /* the semantics of medialibrary's IPlaylist::move() */
        int64_t id = va_arg(args, int64_t);
        uint32_t from = va_arg(args, uint32_t);
        uint32_t to = va_arg(args, uint32_t);
        uint32_t count = va_arg(args, uint32_t);
        VLCTestingEnv::Playlist* playlist = findPlaylist(id);
        if (!playlist || from + count > playlist->media.size())
            return VLC_EGENERIC;
        if (to >= from && to < from + count)
            return VLC_SUCCESS;
        std::vector<int64_t> block(playlist->media.begin() + from,
                                   playlist->media.begin() + from + count);
        playlist->media.erase(playlist->media.begin() + from,
                              playlist->media.begin() + from + count);
        size_t position = to > from ? to - count + 1 : to;
        position = std::min(position, playlist->media.size());
        playlist->media.insert(playlist->media.begin() + position, block.begin(), block.end());
        guard.unlock();
        sendEvent(VLC_ML_EVENT_PLAYLIST_UPDATED, id);
        return VLC_SUCCESS;
    }
    case VLC_ML_PLAYLIST_RENAME:
    {
        int64_t id = va_arg(args, int64_t);
        const char* name = va_arg(args, const char*);
        VLCTestingEnv::Playlist* playlist = findPlaylist(id);
        if (!playlist)
            return VLC_EGENERIC;
        playlist->name = name;
        guard.unlock();
        sendEvent(VLC_ML_EVENT_PLAYLIST_UPDATED, id);
        return VLC_SUCCESS;
    }
    case VLC_ML_PLAYLIST_SET_FAVORITE:
    {
        int64_t id = va_arg(args, int64_t);
        bool favorite = va_arg(args, int);
        VLCTestingEnv::Playlist* playlist = findPlaylist(id);
        if (!playlist)
            return VLC_EGENERIC;
        playlist->favorite = favorite;
        guard.unlock();
        sendFavoriteEvent(id, VLC_ML_PARENT_PLAYLIST, favorite);
        return VLC_SUCCESS;
    }
    case VLC_ML_NEW_EXTERNAL_MEDIA:
    {
        const char* mrl = va_arg(args, const char*);
        auto out = va_arg(args, vlc_ml_media_t**);
        const int64_t id = stubML.nextExternalId++;
        stubML.external[id] = mrl;
        *out = static_cast<vlc_ml_media_t*>(malloc(sizeof(vlc_ml_media_t)));
        fillMedia(*out, id);
        return VLC_SUCCESS;
    }
    case VLC_ML_MEDIA_ADD_EXTERNAL_MRL:
    {
        int64_t id = va_arg(args, int64_t);
        const char* mrl = va_arg(args, const char*);
        int type = va_arg(args, int);
        if (stubML.external.count(id) == 0 || type != VLC_ML_FILE_TYPE_SUBTITLE)
            return VLC_EGENERIC;
        std::vector<std::string>& files = stubML.subtitles[id];
        if (std::find(files.begin(), files.end(), mrl) == files.end())
            files.push_back(mrl);
        return VLC_SUCCESS;
    }
    case VLC_ML_MEDIA_REMOVE_EXTERNAL_MRL:
    {
        int64_t id = va_arg(args, int64_t);
        const char* mrl = va_arg(args, const char*);
        std::vector<std::string>& files = stubML.subtitles[id];
        auto it = std::find(files.begin(), files.end(), mrl);
        if (it == files.end())
            return VLC_EGENERIC;
        files.erase(it);
        return VLC_SUCCESS;
    }
    case VLC_ML_CLEAR_HISTORY:
    {
        stubML.history.clear();
        stubML.historyClears++;
        guard.unlock();
        sendEvent(VLC_ML_EVENT_HISTORY_CHANGED, 0);
        return VLC_SUCCESS;
    }
    default:
        return VLC_EGENERIC;
    }
}

static int MLOpen( vlc_object_t* obj )
{
    auto* p_ml = reinterpret_cast<vlc_medialibrary_module_t*>( obj );
    p_ml->pf_control = MLControl;
    p_ml->pf_get = MLGet;
    p_ml->pf_list = MLList;
    stubML.module = p_ml;
    return VLC_SUCCESS;
}

static void MLClose( vlc_object_t* obj )
{
    VLC_UNUSED(obj);
    stubML.module = nullptr;
}

//Meta writer module

static int WriteMeta( vlc_object_t* obj )
{
    auto p_export = reinterpret_cast<meta_export_t*>( obj );
    testenv->metaWriterCalls++;
    testenv->metaWriterFile = qfu( p_export->psz_file );
    char* title = input_item_GetTitle( p_export->p_item );
    testenv->metaWriterTitle = qfu( title );
    free( title );
    return testenv->metaWriterResult;
}

//Fingerprinter module

static int FingerprinterEnqueue( fingerprinter_thread_t*, fingerprint_request_t* request )
{
    if ( testenv->fingerprintRequest )
        fingerprint_request_Delete( testenv->fingerprintRequest );
    testenv->fingerprintRequest = request;
    return VLC_SUCCESS;
}

static fingerprint_request_t* FingerprinterGetResults( fingerprinter_thread_t* )
{
    fingerprint_request_t* request = testenv->fingerprintRequest;
    testenv->fingerprintRequest = nullptr;
    return request;
}

static void FingerprinterApply( fingerprint_request_t* request, size_t i_result )
{
    if ( i_result >= vlc_array_count( &request->results.metas_array ) )
        return;
    auto meta = static_cast<vlc_meta_t*>(
        vlc_array_item_at_index( &request->results.metas_array, i_result ) );
    vlc_mutex_locker lock( &request->p_item->lock );
    vlc_meta_Merge( request->p_item->p_meta, meta );
}

static int OpenFingerprinter( vlc_object_t* obj )
{
    auto fingerprinter = reinterpret_cast<fingerprinter_thread_t*>( obj );
    fingerprinter->pf_enqueue = FingerprinterEnqueue;
    fingerprinter->pf_getresults = FingerprinterGetResults;
    fingerprinter->pf_apply = FingerprinterApply;
    var_Create( fingerprinter, "results-available", VLC_VAR_BOOL );
    testenv->fingerprinter = obj;
    return VLC_SUCCESS;
}

static void CloseFingerprinter( vlc_object_t* )
{
    if ( testenv->fingerprintRequest )
        fingerprint_request_Delete( testenv->fingerprintRequest );
    testenv->fingerprintRequest = nullptr;
    testenv->fingerprinter = nullptr;
}

//Chromaprint stream output: only there to be found, never opened

static int OpenChromaprint( vlc_object_t* )
{
    return VLC_EGENERIC;
}

//Audio output: plays nothing, has devices

static int AoutStart( audio_output_t*, audio_sample_format_t* fmt )
{
    fmt->i_format = VLC_CODEC_FL32;
    fmt->channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
    return VLC_SUCCESS;
}

static void AoutPlay( audio_output_t*, block_t* block, vlc_tick_t )
{
    block_Release( block );
}

static void AoutFlush( audio_output_t* )
{
}

static int AoutDeviceSelect( audio_output_t* aout, const char* id )
{
    testenv->aoutDevice = qfu( id );
    aout_DeviceReport( aout, id );
    return VLC_SUCCESS;
}

static int OpenAout( vlc_object_t* obj )
{
    auto aout = reinterpret_cast<audio_output_t*>( obj );
    aout->start = AoutStart;
    aout->play = AoutPlay;
    aout->flush = AoutFlush;
    aout->stop = AoutFlush;
    aout->pause = nullptr;
    aout->volume_set = nullptr;
    aout->mute_set = nullptr;
    aout->device_select = AoutDeviceSelect;
    aout_HotplugReport( aout, "stub-speakers", "Speakers" );
    aout_HotplugReport( aout, "stub-headphones", "Headphones" );
    testenv->aout = aout;
    return VLC_SUCCESS;
}

static void CloseAout( vlc_object_t* )
{
    testenv->aout = nullptr;
}

//Access demux "stubtracks://N": N embedded subtitle tracks, never ends

static int StubTracksDemux( demux_t* )
{
    vlc_tick_sleep( VLC_TICK_FROM_MS( 10 ) );
    return VLC_DEMUXER_SUCCESS;
}

static int StubTracksControl( demux_t*, int query, va_list args )
{
    switch ( query )
    {
    case DEMUX_CAN_SEEK:
    case DEMUX_CAN_CONTROL_PACE:
        *va_arg( args, bool* ) = false;
        return VLC_SUCCESS;
    case DEMUX_CAN_PAUSE:
        *va_arg( args, bool* ) = true;
        return VLC_SUCCESS;
    case DEMUX_SET_PAUSE_STATE:
        return VLC_SUCCESS;
    case DEMUX_GET_PTS_DELAY:
    case DEMUX_GET_TIME:
        *va_arg( args, vlc_tick_t* ) = 0;
        return VLC_SUCCESS;
    case DEMUX_GET_POSITION:
        *va_arg( args, double* ) = 0.;
        return VLC_SUCCESS;
    case DEMUX_TEST_AND_CLEAR_FLAGS:
        *va_arg( args, unsigned* ) = 0;
        return VLC_SUCCESS;
    default:
        return VLC_EGENERIC;
    }
}

static int OpenStubTracks( vlc_object_t* obj )
{
    auto demux = reinterpret_cast<demux_t*>( obj );
    if ( demux->out == nullptr )
        return VLC_EGENERIC;

    const int count = atoi( demux->psz_location );
    for ( int i = 0; i < count; ++i )
    {
        es_format_t fmt;
        es_format_Init( &fmt, SPU_ES, VLC_CODEC_SUBT );
        fmt.i_id = i;
        char description[32];
        snprintf( description, sizeof(description), "Embedded %d", i + 1 );
        fmt.psz_description = description;
        es_out_id_t* es = es_out_Add( demux->out, &fmt );
        fmt.psz_description = nullptr;
        es_format_Clean( &fmt );
        if ( es == nullptr )
            return VLC_EGENERIC;
    }

    demux->pf_demux = StubTracksDemux;
    demux->pf_control = StubTracksControl;
    return VLC_SUCCESS;
}

//module declaration

vlc_module_begin()
    set_callbacks(OpenIntf, CloseIntf)
    set_capability("interface", 0)
add_submodule()
    set_capability("renderer_discovery", 0)
    add_shortcut("rd")
    set_callbacks(OpenRD, CloseRD)
add_submodule()
    set_capability("renderer probe", 10000)
    set_callback(vlc_rd_probe_open)
add_submodule()
    set_capability("medialibrary", 10000)
    set_callbacks(MLOpen, MLClose)
add_submodule()
    set_capability("meta writer", 10000)
    set_callback(WriteMeta)
add_submodule()
    set_capability("fingerprinter", 10000)
    set_callbacks(OpenFingerprinter, CloseFingerprinter)
add_submodule()
    set_capability("sout output", 0)
    add_shortcut("chromaprint")
    set_callback(OpenChromaprint)
add_submodule()
    set_capability("audio output", 0)
    add_shortcut("aout_stub")
    set_callbacks(OpenAout, CloseAout)
add_submodule()
    set_capability("access", 0)
    add_shortcut("stubtracks")
    set_callback(OpenStubTracks)
vlc_module_end()

extern "C" {

const char vlc_module_name[] = MODULE_STRING;
VLC_EXPORT vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};
}

VLCTestingEnv::VLCTestingEnv()
{
    assert(testenv == nullptr);
    testenv = this;

}

VLCTestingEnv::~VLCTestingEnv()
{
    libvlc_release(libvlc);
    testenv = nullptr;
    clearMediaLibrary();
}

void VLCTestingEnv::clearMediaLibrary()
{
    std::lock_guard<std::mutex> guard(stubML.lock);
    stubML.playlists.clear();
    stubML.external.clear();
    stubML.subtitles.clear();
    stubML.nextExternalId = 1000;
    stubML.history.clear();
    stubML.historyClears = 0;
}

void VLCTestingEnv::setHistory(const std::vector<HistoryEntry>& history)
{
    {
        std::lock_guard<std::mutex> guard(stubML.lock);
        stubML.history = history;
    }
    sendEvent(VLC_ML_EVENT_HISTORY_CHANGED, 0);
}

int VLCTestingEnv::historyClears() const
{
    std::lock_guard<std::mutex> guard(stubML.lock);
    return stubML.historyClears;
}

std::vector<std::string> VLCTestingEnv::subtitleMatches(const std::string& mediaMrl) const
{
    std::lock_guard<std::mutex> guard(stubML.lock);
    for (const auto& it : stubML.external)
    {
        if (it.second != mediaMrl)
            continue;
        auto subtitles = stubML.subtitles.find(it.first);
        if (subtitles != stubML.subtitles.end())
            return subtitles->second;
    }
    return {};
}

std::vector<VLCTestingEnv::Playlist> VLCTestingEnv::playlists() const
{
    std::lock_guard<std::mutex> guard(stubML.lock);
    return stubML.playlists;
}

int64_t VLCTestingEnv::addPlaylist(const std::string& name, const std::vector<int64_t>& media)
{
    Playlist created;
    {
        std::lock_guard<std::mutex> guard(stubML.lock);
        stubML.playlists.push_back({stubML.nextPlaylistId++, name, media});
        created = stubML.playlists.back();
    }
    sendEvent(VLC_ML_EVENT_PLAYLIST_ADDED, created.id, &created);
    return created.id;
}

void VLCTestingEnv::setPlaylistFavorite(int64_t id, bool favorite)
{
    {
        std::lock_guard<std::mutex> guard(stubML.lock);
        VLCTestingEnv::Playlist* playlist = findPlaylist(id);
        if (!playlist)
            return;
        playlist->favorite = favorite;
    }
    sendFavoriteEvent(id, VLC_ML_PARENT_PLAYLIST, favorite);
}

bool VLCTestingEnv::playlistFavorite(int64_t id) const
{
    std::lock_guard<std::mutex> guard(stubML.lock);
    const VLCTestingEnv::Playlist* playlist = findPlaylist(id);
    return playlist && playlist->favorite;
}

static const char * test_defaults_args[] = {
    "-v", "--ignore-config", "--vout=vdummy", "--aout=aout_stub", "--text-renderer=tdummy", "--media-library"
};

static const int test_defaults_nargs = ARRAY_SIZE(test_defaults_args);

bool VLCTestingEnv::init()
{
    //see test/libvlc/test.h
    QByteArray alarm_timeout = qgetenv("VLC_TEST_TIMEOUT");
    bool has_timeout;
    auto vlc_timeout = alarm_timeout.toLongLong(&has_timeout);
    if (!has_timeout)
        qputenv("QTEST_FUNCTION_TIMEOUT", "5000");
    else
    {
        alarm_timeout.setNum(vlc_timeout * 1000);
        qputenv("QTEST_FUNCTION_TIMEOUT", alarm_timeout);
    }

    // keep paths set by the caller, e.g. when the tests run on another
    // machine than the build one (cross-built Windows tests)
    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 0);
    setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 0);

    libvlc = libvlc_new(test_defaults_nargs, test_defaults_args);
    if (!libvlc)
        return false;
    libvlc_InternalAddIntf(libvlc->p_libvlc_int, MODULE_STRING);
    libvlc_InternalPlay(libvlc->p_libvlc_int);

    if (!intf)
        return false;

    return true;
}
