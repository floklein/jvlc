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
#ifndef VLC_QT_PODCAST_FEEDS_HPP
#define VLC_QT_PODCAST_FEEDS_HPP

#include <QObject>
#include <QStringList>

typedef struct vlc_object_t vlc_object_t;

/**
 * The feeds the Podcasts service lists ("podcast-urls"). Added and removed
 * feeds show up at once when the service is running.
 */
class PodcastFeeds : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList urls READ urls NOTIFY urlsChanged FINAL)

public:
    explicit PodcastFeeds(vlc_object_t *obj, QObject *parent = nullptr);

    QStringList urls() const;

    /* @return false if url is not a link or is already a feed */
    Q_INVOKABLE bool add(const QString &url);
    Q_INVOKABLE void remove(const QString &url);
    Q_INVOKABLE bool contains(const QString &url) const;

signals:
    void urlsChanged();

private:
    void setUrls(const QStringList &urls);
    void request(const QString &request);

    vlc_object_t *const m_obj;
};

#endif // VLC_QT_PODCAST_FEEDS_HPP
