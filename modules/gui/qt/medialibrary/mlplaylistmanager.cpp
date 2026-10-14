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

#include "mlplaylistmanager.hpp"

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_media_library.h>
#include <vlc_url.h>

#include "qt.hpp"
#include "medialib.hpp"
#include "mlhelper.hpp"
#include "playlist/playlist_controller.hpp"
#include "util/shared_input_item.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QMenu>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QTimer>
#include <QXmlStreamWriter>

#include <algorithm>
#include <vector>

namespace {

const char RECENT_ORDER_KEY[] = "Playlists/RecentlyChanged";
/* ids kept in the recent order: more than shown, so that deletions do not
 * leave the submenu short */
const int RECENT_ORDER_MAX = 50;
const int THUMBNAIL_MAX_SIZE = 1024;

struct ExportedItem
{
    QByteArray uri;
    QString name;
    vlc_tick_t duration;
};

/* the MRLs of the media a Playlist holds, as the Queue would get them */
std::vector<ExportedItem> listPlaylistItems(vlc_medialibrary_t *ml, int64_t playlistId)
{
    std::vector<ExportedItem> items;
    vlc_ml_query_params_t query = vlc_ml_query_params_create();
    ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_media_of(ml, &query, VLC_ML_PARENT_PLAYLIST, playlistId));
    if (!list)
        return items;

    for (const vlc_ml_media_t &media : ml_range_iterate<vlc_ml_media_t>(list))
    {
        SharedInputItem item(vlc_ml_get_input_item(ml, media.i_id), false);
        if (!item)
            continue;
        char *uri = input_item_GetURI(item.get());
        char *name = input_item_GetName(item.get());
        if (uri)
            items.push_back({ QByteArray(uri), qfu(name), input_item_GetDuration(item.get()) });
        free(uri);
        free(name);
    }
    return items;
}

bool writeM3U8(QIODevice &file, const QString &path, const std::vector<ExportedItem> &items)
{
    /* As the core M3U export does: a relative URL for items in the folder
     * of the playlist file or below it, the full URL otherwise */
    QByteArray base;
    char *dirUri = vlc_path2uri(qtu(QFileInfo(path).absolutePath()), "file");
    if (dirUri)
    {
        base = QByteArray(dirUri) + '/';
        free(dirUri);
    }

    QByteArray out("#EXTM3U\n");
    for (const ExportedItem &item : items)
    {
        if (!item.name.isEmpty() && item.name.toUtf8() != item.uri)
        {
            const qint64 seconds = item.duration > 0 ? SEC_FROM_VLC_TICK(item.duration) : -1;
            out += "#EXTINF:" + QByteArray::number(seconds) + ',' + item.name.toUtf8() + '\n';
        }
        if (!base.isEmpty() && item.uri.startsWith(base))
            out += item.uri.mid(base.size());
        else
            out += item.uri;
        out += '\n';
    }
    return file.write(out) == out.size();
}

bool writeXSPF(QIODevice &file, const QString &title, const std::vector<ExportedItem> &items)
{
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("playlist"));
    xml.writeDefaultNamespace(QStringLiteral("http://xspf.org/ns/0/"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));
    xml.writeTextElement(QStringLiteral("title"), title);
    xml.writeStartElement(QStringLiteral("trackList"));
    for (const ExportedItem &item : items)
    {
        xml.writeStartElement(QStringLiteral("track"));
        xml.writeTextElement(QStringLiteral("location"), QString::fromUtf8(item.uri));
        if (!item.name.isEmpty())
            xml.writeTextElement(QStringLiteral("title"), item.name);
        if (item.duration > 0)
            xml.writeTextElement(QStringLiteral("duration"),
                                 QString::number(MS_FROM_VLC_TICK(item.duration)));
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return !xml.hasError();
}

/* Copies an image as a Playlist thumbnail, as a PNG no bigger than needed */
bool saveThumbnail(const QUrl &source, const QString &target)
{
    if (!source.isLocalFile())
        return false;
    QImage image(source.toLocalFile());
    if (image.isNull())
        return false;
    if (image.width() > THUMBNAIL_MAX_SIZE || image.height() > THUMBNAIL_MAX_SIZE)
        image = image.scaled(THUMBNAIL_MAX_SIZE, THUMBNAIL_MAX_SIZE,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!QDir().mkpath(QFileInfo(target).absolutePath()))
        return false;
    return image.save(target, "PNG");
}

} // anonymous namespace

void appendItemsToMLPlaylist(vlc_medialibrary_t *ml, int64_t playlistId, const QVariantList &items)
{
    std::vector<int64_t> mediaIds;
    for (const QVariant &item : items)
    {
        if (item.canConvert<MLItemId>())
        {
            const MLItemId itemId = item.value<MLItemId>();
            if (itemId.id == 0)
                continue;
            // NOTE: When we have a parent it's a collection of media(s).
            if (itemId.type == VLC_ML_PARENT_UNKNOWN)
            {
                mediaIds.push_back(itemId.id);
                continue;
            }
            vlc_ml_query_params_t query = vlc_ml_query_params_create();
            ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_media_of(ml, &query, itemId.type, itemId.id));
            if (!list)
                continue;
            for (const vlc_ml_media_t &media : ml_range_iterate<vlc_ml_media_t>(list))
                mediaIds.push_back(media.i_id);
            continue;
        }

        for (const vlc::playlist::Media &media : vlc::playlist::toMediaList({ item }))
        {
            const char * const uri = media.raw()->psz_uri;
            vlc_ml_media_t *mlMedia = vlc_ml_get_media_by_mrl(ml, uri);
            if (!mlMedia)
                mlMedia = vlc_ml_new_external_media(ml, uri);
            if (!mlMedia)
                continue;
            mediaIds.push_back(mlMedia->i_id);
            vlc_ml_media_release(mlMedia);
        }
    }

    if (!mediaIds.empty())
        vlc_ml_playlist_append(ml, playlistId, mediaIds.data(), mediaIds.size());
}

MLPlaylistManager::MLPlaylistManager(MediaLib *medialib, vlc::playlist::PlaylistController *controller,
                                     QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_mediaLib(medialib)
    , m_controller(controller)
    , m_settings(settings)
{
    assert(m_mediaLib);

    const QString userData = QVLCUserDir(VLC_USERDATA_DIR);
    if (!userData.isEmpty())
        m_thumbnailDir = userData + QStringLiteral("/playlist-thumbnails");

    if (m_settings)
    {
        const QStringList ids = m_settings->value(RECENT_ORDER_KEY).toStringList();
        for (const QString &id : ids)
        {
            bool ok = false;
            const qint64 value = id.toLongLong(&ok);
            if (ok && value > 0 && !m_recentOrder.contains(value))
                m_recentOrder.push_back(value);
        }
    }

    m_eventCallback = m_mediaLib->registerEventListener(&MLPlaylistManager::onVlcMlEvent, this);
    refreshRecentPlaylists();
}

MLPlaylistManager::~MLPlaylistManager()
{
    if (m_eventCallback)
        m_mediaLib->unregisterEventListener(m_eventCallback);
}

void MLPlaylistManager::onVlcMlEvent(void *data, const vlc_ml_event_t *event)
{
    int64_t id;
    switch (event->i_type)
    {
    case VLC_ML_EVENT_PLAYLIST_ADDED:
        id = event->creation.p_playlist->i_id;
        break;
    case VLC_ML_EVENT_PLAYLIST_UPDATED:
        id = event->modification.i_entity_id;
        break;
    case VLC_ML_EVENT_PLAYLIST_DELETED:
        id = event->deletion.i_entity_id;
        break;
    default:
        return;
    }

    auto self = static_cast<MLPlaylistManager *>(data);
    const bool deleted = event->i_type == VLC_ML_EVENT_PLAYLIST_DELETED;
    QMetaObject::invokeMethod(self, [self, id, deleted]() {
        if (deleted)
            self->playlistDeleted(id);
        else
            self->playlistChanged(id);
    });
}

void MLPlaylistManager::playlistChanged(int64_t playlistId)
{
    m_recentOrder.removeAll(playlistId);
    m_recentOrder.push_front(playlistId);
    while (m_recentOrder.size() > RECENT_ORDER_MAX)
        m_recentOrder.pop_back();
    saveRecentOrder();
    refreshRecentPlaylists();
}

void MLPlaylistManager::playlistDeleted(int64_t playlistId)
{
    if (m_recentOrder.removeAll(playlistId) > 0)
        saveRecentOrder();
    refreshRecentPlaylists();

    if (!m_thumbnailDir.isEmpty() && QFile::remove(thumbnailPath(playlistId)))
        emit thumbnailsChanged();
}

void MLPlaylistManager::saveRecentOrder()
{
    if (!m_settings)
        return;
    QStringList ids;
    for (qint64 id : std::as_const(m_recentOrder))
        ids.push_back(QString::number(id));
    m_settings->setValue(RECENT_ORDER_KEY, ids);
}

void MLPlaylistManager::refreshRecentPlaylists()
{
    struct Entry
    {
        int64_t id;
        QString name;
        unsigned int creationDate;
    };
    struct Ctx
    {
        std::vector<Entry> playlists;
    };

    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [](vlc_medialibrary_t *ml, Ctx &ctx) {
        ml_unique_ptr<vlc_ml_playlist_list_t> list(vlc_ml_list_playlists(ml, nullptr, VLC_ML_PLAYLIST_TYPE_ALL));
        if (!list)
            return;
        for (const vlc_ml_playlist_t &playlist : ml_range_iterate<vlc_ml_playlist_t>(list))
            ctx.playlists.push_back({ playlist.i_id, qfu(playlist.psz_name), playlist.i_creation_date });
    },
    //UI thread
    [this](quint64, Ctx &ctx) {
        // the recently changed ones, then the newest
        auto rank = [this](const Entry &entry) {
            const qsizetype index = m_recentOrder.indexOf(entry.id);
            return index < 0 ? m_recentOrder.size() : index;
        };
        std::stable_sort(ctx.playlists.begin(), ctx.playlists.end(),
                         [&rank](const Entry &a, const Entry &b) {
            const qsizetype rankA = rank(a);
            const qsizetype rankB = rank(b);
            if (rankA != rankB)
                return rankA < rankB;
            if (a.creationDate != b.creationDate)
                return a.creationDate > b.creationDate;
            return a.id > b.id;
        });

        QVariantList recent;
        for (const Entry &entry : ctx.playlists)
        {
            if (recent.size() == RECENT_PLAYLIST_COUNT)
                break;
            recent.push_back(QVariantMap {
                { QStringLiteral("id"), QVariant::fromValue(MLItemId(entry.id, VLC_ML_PARENT_PLAYLIST)) },
                { QStringLiteral("name"), entry.name },
            });
        }

        if (recent == m_recentPlaylists)
            return;
        m_recentPlaylists = recent;
        emit recentPlaylistsChanged();
    }, "MLPlaylistManager");
}

void MLPlaylistManager::create(const QString &name, const QVariantList &items, const QUrl &thumbnail)
{
    const QString playlistName = name.trimmed();
    if (playlistName.isEmpty())
        return;

    struct Ctx
    {
        int64_t id = 0;
        bool thumbnailSaved = false;
    };

    const QString thumbnailDir = m_thumbnailDir;
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [playlistName, items, thumbnail, thumbnailDir](vlc_medialibrary_t *ml, Ctx &ctx) {
        ml_unique_ptr<vlc_ml_playlist_t> playlist(vlc_ml_playlist_create(ml, qtu(playlistName)));
        if (!playlist)
            return;
        ctx.id = playlist->i_id;

        if (!thumbnail.isEmpty() && !thumbnailDir.isEmpty())
            ctx.thumbnailSaved = saveThumbnail(thumbnail, thumbnailPath(thumbnailDir, ctx.id));

        appendItemsToMLPlaylist(ml, ctx.id, items);
    },
    //UI thread
    [this, playlistName](quint64, Ctx &ctx) {
        if (ctx.id == 0)
            return;
        if (ctx.thumbnailSaved)
            emit thumbnailsChanged();
        emit created(MLItemId(ctx.id, VLC_ML_PARENT_PLAYLIST), playlistName);
    });
}

void MLPlaylistManager::append(const MLItemId &playlistId, const QVariantList &items)
{
    if (playlistId.id == 0 || items.isEmpty())
        return;

    struct Ctx
    {
        QString name;
    };

    const int64_t id = playlistId.id;
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [id, items](vlc_medialibrary_t *ml, Ctx &ctx) {
        appendItemsToMLPlaylist(ml, id, items);
        ml_unique_ptr<vlc_ml_playlist_t> playlist(vlc_ml_get_playlist(ml, id));
        if (playlist)
            ctx.name = qfu(playlist->psz_name);
    },
    //UI thread
    [this, id](quint64, Ctx &ctx) {
        emit appended(MLItemId(id, VLC_ML_PARENT_PLAYLIST), ctx.name);
    });
}

void MLPlaylistManager::exportPlaylist(const MLItemId &playlistId, const QUrl &file)
{
    const QString path = file.toLocalFile();
    if (playlistId.id == 0 || path.isEmpty())
    {
        emit exported(false, path);
        return;
    }

    struct Ctx
    {
        bool success = false;
    };

    const int64_t id = playlistId.id;
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [id, path](vlc_medialibrary_t *ml, Ctx &ctx) {
        ml_unique_ptr<vlc_ml_playlist_t> playlist(vlc_ml_get_playlist(ml, id));
        if (!playlist)
            return;
        const std::vector<ExportedItem> items = listPlaylistItems(ml, id);

        QSaveFile out(path);
        if (!out.open(QIODevice::WriteOnly))
            return;
        bool written;
        if (path.endsWith(QLatin1String(".xspf"), Qt::CaseInsensitive))
            written = writeXSPF(out, qfu(playlist->psz_name), items);
        else
            written = writeM3U8(out, path, items);
        ctx.success = written && out.commit();
    },
    //UI thread
    [this, path](quint64, Ctx &ctx) {
        emit exported(ctx.success, path);
    });
}

void MLPlaylistManager::exportPlaylistAs(const MLItemId &playlistId, const QString &name)
{
    const QString m3u8 = qtr("M3U8 playlist (*.m3u8)");
    const QString xspf = qtr("XSPF playlist (*.xspf)");
    QString filter = m3u8;

    QString fileName = name.trimmed();
    fileName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    if (fileName.isEmpty())
        fileName = qtr("Playlist");
    const QString start = QDir(QVLCUserDir(VLC_DOCUMENTS_DIR)).filePath(fileName + QStringLiteral(".m3u8"));

    QString path = QFileDialog::getSaveFileName(nullptr, qtr("Export Playlist"), start,
                                                m3u8 + QStringLiteral(";;") + xspf, &filter);
    if (path.isEmpty())
        return;

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != QLatin1String("m3u8") && suffix != QLatin1String("xspf"))
        path += (filter == xspf) ? QStringLiteral(".xspf") : QStringLiteral(".m3u8");

    exportPlaylist(playlistId, QUrl::fromLocalFile(path));
}

void MLPlaylistManager::requestNewPlaylist(const QVariantList &items)
{
    emit newPlaylistRequested(items);
}

void MLPlaylistManager::requestPicker(const QVariantList &items)
{
    emit pickerRequested(items);
}

void MLPlaylistManager::saveQueueAsPlaylist()
{
    QVariantList items;
    if (m_controller)
    {
        vlc_playlist_t *playlist = m_controller->getPlaylist().m_playlist;
        vlc::playlist::vlc_playlist_locker locker(playlist);
        const size_t count = vlc_playlist_Count(playlist);
        for (size_t i = 0; i < count; ++i)
        {
            input_item_t *media = vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist, i));
            if (media && media->psz_uri)
                items.push_back(QUrl::fromEncoded(media->psz_uri));
        }
    }
    emit newPlaylistRequested(items);
}

QList<QUrl> MLPlaylistManager::pickFiles()
{
    QString filter = QStringLiteral("%1 (%2);;%3 (*)")
                         .arg(qtr("Media Files"), QString(EXTENSIONS_MEDIA).replace(QLatin1String(";*"), QLatin1String(" *")),
                              qtr("All Files"));
    return QFileDialog::getOpenFileUrls(nullptr, qtr("Select Files"), QUrl(), filter);
}

QUrl MLPlaylistManager::pickThumbnail()
{
    return QFileDialog::getOpenFileUrl(nullptr, qtr("Choose a Thumbnail"), QUrl(),
                                       qtr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
}

void MLPlaylistManager::addToPlaylistMenu(QMenu *menu, const QVariantList &items)
{
    QMenu *submenu = menu->addMenu(qtr("Add to Playlist"));

    for (const QVariant &entry : std::as_const(m_recentPlaylists))
    {
        const QVariantMap playlist = entry.toMap();
        const MLItemId id = playlist.value(QStringLiteral("id")).value<MLItemId>();
        QString name = playlist.value(QStringLiteral("name")).toString();
        name.replace('&', QLatin1String("&&"));
        submenu->addAction(name, this, [this, id, items]() {
            append(id, items);
        });
    }

    if (!m_recentPlaylists.isEmpty())
        submenu->addSeparator();

    submenu->addAction(qtr("More…"), this, [this, items]() {
        requestPicker(items);
    });
    submenu->addAction(qtr("+ New Playlist…"), this, [this, items]() {
        requestNewPlaylist(items);
    });
}

void MLPlaylistManager::setThumbnailDir(const QString &dir)
{
    if (m_thumbnailDir == dir)
        return;
    m_thumbnailDir = dir;
    emit thumbnailsChanged();
}

QString MLPlaylistManager::thumbnailPath(int64_t playlistId) const
{
    return thumbnailPath(m_thumbnailDir, playlistId);
}

QString MLPlaylistManager::thumbnailPath(const QString &dir, int64_t playlistId)
{
    return QDir(dir).filePath(QString::number(playlistId) + QStringLiteral(".png"));
}
