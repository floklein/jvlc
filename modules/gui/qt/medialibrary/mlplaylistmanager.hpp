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

#ifndef MLPLAYLISTMANAGER_HPP
#define MLPLAYLISTMANAGER_HPP

#include <QObject>
#include <QList>
#include <QUrl>
#include <QVariantList>

#include "mlqmltypes.hpp"

class MediaLib;
class QMenu;
class QSettings;
struct vlc_medialibrary_t;
struct vlc_ml_event_callback_t;
struct vlc_ml_event_t;

namespace vlc {
namespace playlist {
class PlaylistController;
}
}

/**
 * The Playlists of the media library, as the user handles them: New Playlist,
 * Add to Playlist (with the most recently changed Playlists), Export, and
 * the optional thumbnail chosen at creation.
 *
 * Playing a Playlist goes through MediaLib (addAndPlay, shuffleAndPlay,
 * addToPlaylist), which copies its items into the Queue.
 */
class MLPlaylistManager : public QObject
{
    Q_OBJECT

    /* [{id: MLItemId, name: string}], most recently changed first */
    Q_PROPERTY(QVariantList recentPlaylists READ recentPlaylists NOTIFY recentPlaylistsChanged FINAL)

public:
    static constexpr int RECENT_PLAYLIST_COUNT = 10;

    /* settings may be null: the recent order is then kept for this session only */
    MLPlaylistManager(MediaLib *medialib, vlc::playlist::PlaylistController *controller,
                      QSettings *settings, QObject *parent = nullptr);
    ~MLPlaylistManager();

    /*
     * items are MLItemIds, URLs, MRLs or input items, in any mix. An empty
     * name is refused. thumbnail is an image file, copied as the Playlist
     * thumbnail.
     */
    Q_INVOKABLE void create(const QString &name, const QVariantList &items = {},
                            const QUrl &thumbnail = {});
    Q_INVOKABLE void append(const MLItemId &playlistId, const QVariantList &items);

    /* writes an M3U8 file, or an XSPF one when the file name ends in .xspf */
    Q_INVOKABLE void exportPlaylist(const MLItemId &playlistId, const QUrl &file);
    /* asks for the file with the system file picker, then exports */
    Q_INVOKABLE void exportPlaylistAs(const MLItemId &playlistId, const QString &name);

    /* open the New Playlist dialog, pre-filled with items */
    Q_INVOKABLE void requestNewPlaylist(const QVariantList &items = {});
    /* open the Playlist picker ("More…"), to add items */
    Q_INVOKABLE void requestPicker(const QVariantList &items);
    /* open the New Playlist dialog with the items of the Queue */
    Q_INVOKABLE void saveQueueAsPlaylist();

    /* system file pickers for the New Playlist dialog */
    Q_INVOKABLE QList<QUrl> pickFiles();
    Q_INVOKABLE QUrl pickThumbnail();

    /* adds the "Add to Playlist" submenu for items to menu */
    void addToPlaylistMenu(QMenu *menu, const QVariantList &items);

    QVariantList recentPlaylists() const { return m_recentPlaylists; }

    QString thumbnailDir() const { return m_thumbnailDir; }
    void setThumbnailDir(const QString &dir);
    /* where the thumbnail of a Playlist is, if it has one */
    QString thumbnailPath(int64_t playlistId) const;
    static QString thumbnailPath(const QString &dir, int64_t playlistId);

signals:
    void created(const MLItemId &playlistId, const QString &name);
    void appended(const MLItemId &playlistId, const QString &name);
    void exported(bool success, const QString &path);

    void newPlaylistRequested(const QVariantList &items);
    void pickerRequested(const QVariantList &items);

    void recentPlaylistsChanged();
    void thumbnailsChanged();

private:
    static void onVlcMlEvent(void *data, const vlc_ml_event_t *event);
    void playlistChanged(int64_t playlistId);
    void playlistDeleted(int64_t playlistId);
    void refreshRecentPlaylists();
    void saveRecentOrder();

    MediaLib *m_mediaLib;
    vlc::playlist::PlaylistController *m_controller;
    QSettings *m_settings;
    vlc_ml_event_callback_t *m_eventCallback = nullptr;

    QString m_thumbnailDir;
    /* Playlist ids, most recently changed first */
    QList<qint64> m_recentOrder;
    QVariantList m_recentPlaylists;
};

/* Appends items (MLItemIds, URLs, MRLs or input items) to a Playlist; ML thread only */
void appendItemsToMLPlaylist(vlc_medialibrary_t *ml, int64_t playlistId, const QVariantList &items);

#endif // MLPLAYLISTMANAGER_HPP
