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
#ifndef VLC_QT_NETWORK_PLAYLIST_MODEL_HPP
#define VLC_QT_NETWORK_PLAYLIST_MODEL_HPP

#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

struct NetworkPlaylistItem
{
    QString name;
    QString mrl;
    QString artwork;
};

/**
 * Reads the streams listed at a playlist URL (.m3u…).
 */
class NetworkPlaylistReader : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const QVector<NetworkPlaylistItem> &)>;

    using QObject::QObject;

    /**
     * Read the streams of a playlist URL. done is called once, on the thread
     * of the reader, with no item if the URL could not be read. It is not
     * called if receiver is destroyed first.
     */
    virtual void read(const QString &url, QObject *receiver, Callback done) = 0;
};

/**
 * The streams of a Network Playlist: its URLs, each playlist URL (.m3u…)
 * replaced by the streams it lists, read again on every reload.
 */
class NetworkPlaylistModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QStringList urls READ urls WRITE setUrls NOTIFY urlsChanged FINAL)
    Q_PROPERTY(NetworkPlaylistReader *reader READ reader WRITE setReader NOTIFY readerChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Roles {
        NAME = Qt::UserRole + 1,
        MRL,
        ARTWORK,
    };
    Q_ENUM(Roles)

    using QAbstractListModel::QAbstractListModel;

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QStringList urls() const;
    void setUrls(const QStringList &urls);

    NetworkPlaylistReader *reader() const;
    void setReader(NetworkPlaylistReader *reader);

    bool loading() const;
    int count() const;

    /* Read the playlist URLs again */
    Q_INVOKABLE void reload();

    /* The MRLs of the rows from index to the end */
    Q_INVOKABLE QStringList mrlsFrom(int index) const;

signals:
    void urlsChanged();
    void readerChanged();
    void loadingChanged();
    void countChanged();

private:
    void setLoading(bool loading);
    void publishIfComplete();

    QStringList m_urls;
    QPointer<NetworkPlaylistReader> m_reader;
    QVector<NetworkPlaylistItem> m_items;
    /* the items of each URL, until all are read */
    QVector<std::optional<QVector<NetworkPlaylistItem>>> m_pending;
    /* tells the answers of a previous reload apart */
    QObject *m_generation = nullptr;
    /* the rows show the last reload */
    bool m_published = false;
    bool m_loading = false;
};

#endif // VLC_QT_NETWORK_PLAYLIST_MODEL_HPP
