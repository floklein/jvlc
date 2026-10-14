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

#include "network_playlist_model.hpp"

#include "saved_streams_model.hpp"

QHash<int, QByteArray> NetworkPlaylistModel::roleNames() const
{
    return {
        { NAME, "name" },
        { MRL, "mrl" },
        { ARTWORK, "artwork" },
    };
}

int NetworkPlaylistModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant NetworkPlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const NetworkPlaylistItem &item = m_items.at(index.row());
    switch (role)
    {
    case Qt::DisplayRole:
    case NAME:
        return item.name.isEmpty() ? item.mrl : item.name;
    case MRL:
        return item.mrl;
    case ARTWORK:
        return item.artwork;
    default:
        return {};
    }
}

QStringList NetworkPlaylistModel::urls() const
{
    return m_urls;
}

void NetworkPlaylistModel::setUrls(const QStringList &urls)
{
    if (m_urls == urls)
        return;

    m_urls = urls;
    emit urlsChanged();
    reload();
}

NetworkPlaylistReader *NetworkPlaylistModel::reader() const
{
    return m_reader;
}

void NetworkPlaylistModel::setReader(NetworkPlaylistReader *reader)
{
    if (m_reader == reader)
        return;

    m_reader = reader;
    emit readerChanged();
    if (!m_urls.isEmpty())
        reload();
}

bool NetworkPlaylistModel::loading() const
{
    return m_loading;
}

int NetworkPlaylistModel::count() const
{
    return m_items.size();
}

void NetworkPlaylistModel::reload()
{
    /* the answers to the previous reload go to a dead receiver */
    delete m_generation;
    m_generation = new QObject(this);

    m_published = false;
    m_pending.clear();
    m_pending.resize(m_urls.size());

    QVector<int> toRead;
    for (int i = 0; i < m_urls.size(); ++i)
    {
        const QString &url = m_urls.at(i);
        if (m_reader && SavedStreamsModel::isPlaylistUrl(url))
            toRead.push_back(i);
        else
            m_pending[i] = QVector<NetworkPlaylistItem> { { {}, url, {} } };
    }

    setLoading(!toRead.isEmpty());

    QObject *generation = m_generation;
    for (int i : toRead)
    {
        m_reader->read(m_urls.at(i), generation, [this, generation, i](const QVector<NetworkPlaylistItem> &items) {
            if (generation != m_generation)
                return;
            m_pending[i] = items;
            publishIfComplete();
        });
    }

    publishIfComplete();
}

QStringList NetworkPlaylistModel::mrlsFrom(int index) const
{
    QStringList mrls;
    for (int i = std::max(index, 0); i < m_items.size(); ++i)
        mrls.push_back(m_items.at(i).mrl);
    return mrls;
}

void NetworkPlaylistModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void NetworkPlaylistModel::publishIfComplete()
{
    if (m_published)
        return;

    for (const auto &items : m_pending)
    {
        if (!items)
            return;
    }

    QVector<NetworkPlaylistItem> result;
    for (const auto &items : m_pending)
        result += *items;
    m_pending.clear();
    m_published = true;

    const int previousCount = m_items.size();
    beginResetModel();
    m_items = std::move(result);
    endResetModel();
    if (previousCount != m_items.size())
        emit countChanged();

    setLoading(false);
}
