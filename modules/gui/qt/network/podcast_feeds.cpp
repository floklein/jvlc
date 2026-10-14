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

#include "podcast_feeds.hpp"

#include "saved_streams_model.hpp"

#include "qt.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_media_source.h>
#include <vlc_variables.h>

namespace {

const char PODCAST_URLS[] = "podcast-urls";
const char PODCAST_REQUEST[] = "podcast-request";

} // namespace

PodcastFeeds::PodcastFeeds(vlc_object_t *obj, QObject *parent)
    : QObject(parent)
    , m_obj(obj)
{
}

QStringList PodcastFeeds::urls() const
{
    if (!config_FindConfig(PODCAST_URLS))
        return {};

    const QString urls = QString::fromUtf8(vlc::wrap_cptr(config_GetPsz(PODCAST_URLS)).get());
    return urls.split(u'|', Qt::SkipEmptyParts);
}

bool PodcastFeeds::add(const QString &url)
{
    const QStringList links = SavedStreamsModel::urlsFromText(url);
    if (links.size() != 1 || !config_FindConfig(PODCAST_URLS))
        return false;

    const QString feed = links.first();
    QStringList feeds = urls();
    if (feeds.contains(feed))
        return false;

    feeds.push_back(feed);
    setUrls(feeds);
    request(QStringLiteral("ADD:") + feed);
    return true;
}

void PodcastFeeds::remove(const QString &url)
{
    QStringList feeds = urls();
    if (!feeds.removeAll(url))
        return;

    setUrls(feeds);
    request(QStringLiteral("RM:") + url);
}

bool PodcastFeeds::contains(const QString &url) const
{
    return urls().contains(url);
}

void PodcastFeeds::setUrls(const QStringList &urls)
{
    config_PutPsz(PODCAST_URLS, qtu(urls.join(u'|')));
    emit urlsChanged();
}

void PodcastFeeds::request(const QString &request)
{
    /* the running Podcasts service listens to its parent, the media source
     * provider, whose first member is its object */
    vlc_media_source_provider_t *provider = vlc_media_source_provider_Get(vlc_object_instance(m_obj));
    if (!provider)
        return;
    vlc_object_t *providerObj = reinterpret_cast<vlc_object_t *>(provider);
    if (var_Type(providerObj, PODCAST_REQUEST) != 0)
        var_SetString(providerObj, PODCAST_REQUEST, qtu(request));
}
