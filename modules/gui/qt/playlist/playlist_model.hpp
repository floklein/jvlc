/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

#ifndef VLC_QT_PLAYLIST_NEW_MODEL_HPP_
#define VLC_QT_PLAYLIST_NEW_MODEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "playlist_common.hpp"
#include "playlist_item.hpp"
#include "util/vlctick.hpp"

#include <QAbstractListModel>

namespace vlc {
namespace playlist {

class PlaylistListModelPrivate;
class PlaylistListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Playlist playlist READ getPlaylist WRITE setPlaylist NOTIFY playlistChanged FINAL)
    Q_PROPERTY(int currentIndex READ getCurrentIndex NOTIFY currentIndexChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(VLCDuration duration READ getDuration NOTIFY countChanged FINAL)
    /* What is left: the current item and the ones after it */
    Q_PROPERTY(int remainingCount READ getRemainingCount NOTIFY remainingChanged FINAL)
    /* The time left: what remains of the current item, then the next ones */
    Q_PROPERTY(VLCDuration remainingDuration READ getRemainingDuration NOTIFY remainingChanged FINAL)
    /* The time reached in the current item */
    Q_PROPERTY(VLCTime currentTime READ getCurrentTime WRITE setCurrentTime NOTIFY remainingChanged FINAL)

public:
    enum Roles
    {
        TitleRole = Qt::DisplayRole,
        FilterRole = Qt::UserRole,
        DurationRole,
        IsCurrentRole,
        ArtistRole,
        AlbumRole,
        ArtworkRole,
        UrlRole,
        PreparsedRole,
        /* the item comes before the current one */
        IsPlayedRole
    };
    Q_ENUM(Roles);

    PlaylistListModel(QObject *parent = nullptr);
    PlaylistListModel(vlc_playlist_t *playlist, QObject *parent = nullptr);
    ~PlaylistListModel();

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    VLCDuration getDuration() const;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    /* provided for convenience */
    Q_INVOKABLE PlaylistItem itemAt(int index) const;

    Q_INVOKABLE virtual void removeItems(const QVector<int> &indexes);
    Q_INVOKABLE virtual void moveItemsPre(const QVector<int> &indexes, int preTarget);
    Q_INVOKABLE virtual void moveItemsPost(const QVector<int> &indexes, int postTarget);
    /* Move the items, kept together, @a offset rows up (< 0) or down (> 0),
     * stopping at the ends of the Queue */
    Q_INVOKABLE void shiftItems(const QVector<int> &sortedIndexes, int offset);
    /* Move the items right after the current one ("Play next") */
    Q_INVOKABLE void moveItemsAfterCurrent(const QVector<int> &sortedIndexes);

    int getCurrentIndex() const;
    int getRemainingCount() const;
    VLCDuration getRemainingDuration() const;
    VLCTime getCurrentTime() const;
    void setCurrentTime(VLCTime time);

    Q_INVOKABLE QVariantList getItemsForIndexes(const QVector<int> & indexes) const;

public slots:
    Playlist getPlaylist() const;
    void setPlaylist(const Playlist& playlist);
    void setPlaylist(vlc_playlist_t* playlist);

signals:
    void playlistChanged(const Playlist&);
    void currentIndexChanged( int );
    void countChanged(int);
    void remainingChanged();

private:
    Q_DECLARE_PRIVATE(PlaylistListModel)

    void moveItems(const QVector<int> &indexes, int target, bool isPreTarget);

    QScopedPointer<PlaylistListModelPrivate> d_ptr;

};


  } // namespace playlist
} // namespace vlc

#endif
