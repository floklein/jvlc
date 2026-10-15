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

#include "preparser_playlist_reader.hpp"

#include <vlc_common.h>
#include <vlc_input_item.h>
#include <vlc_preparser.h>

#include "qt.hpp"

#include <QMetaObject>

namespace {

/* reading a remote list may take a while, but not forever */
constexpr vlc_tick_t READ_TIMEOUT = VLC_TICK_FROM_SEC(30);

struct ReadRequest
{
    PreparserPlaylistReader *reader;
    QPointer<QObject> receiver;
    NetworkPlaylistReader::Callback done;
    /* filled on the preparser thread */
    QVector<NetworkPlaylistItem> items;
};

QString takeString(char *str)
{
    const QString result = QString::fromUtf8(str);
    free(str);
    return result;
}

void onSubtreeAdded(vlc_preparser_req *, input_item_node_t *node, void *data)
{
    auto request = static_cast<ReadRequest *>(data);

    /* the streams of the list; lists inside it are listed as they are */
    for (int i = 0; i < node->i_children; ++i)
    {
        input_item_t *item = node->pp_children[i]->p_item;
        NetworkPlaylistItem entry;
        entry.mrl = takeString(input_item_GetURI(item));
        if (entry.mrl.isEmpty() || entry.mrl == QLatin1String(INPUT_ITEM_URI_NOP))
            continue;
        entry.name = takeString(input_item_GetTitleFbName(item));
        if (entry.name == entry.mrl)
            entry.name.clear();
        entry.artwork = takeString(input_item_GetArtURL(item));
        request->items.push_back(std::move(entry));
    }
    input_item_node_Delete(node);
}

void onEnded(vlc_preparser_req *req, int, void *data)
{
    auto request = static_cast<ReadRequest *>(data);

    /* the reader deletes the preparser, which ends every request, before it
     * is destroyed: it is alive here, and drops this call if it dies first */
    QMetaObject::invokeMethod(request->reader, [request]() {
        if (request->receiver)
            request->done(request->items);
        delete request;
    }, Qt::QueuedConnection);

    vlc_preparser_req_Release(req);
}

} // namespace

PreparserPlaylistReader::PreparserPlaylistReader(vlc_object_t *obj, QObject *parent)
    : NetworkPlaylistReader(parent)
    , m_obj(obj)
{
}

PreparserPlaylistReader::~PreparserPlaylistReader()
{
    if (m_preparser)
        vlc_preparser_Delete(m_preparser);
}

void PreparserPlaylistReader::read(const QString &url, QObject *receiver, Callback done)
{
    auto fail = [this, receiver = QPointer<QObject>(receiver), done]() {
        QMetaObject::invokeMethod(this, [receiver, done]() {
            if (receiver)
                done({});
        }, Qt::QueuedConnection);
    };

    if (!m_preparser)
    {
        struct vlc_preparser_cfg cfg {};
        cfg.types = VLC_PREPARSER_TYPE_PARSE;
        cfg.max_parser_threads = 1;
        cfg.timeout = READ_TIMEOUT;
        cfg.external_process = false;
        m_preparser = vlc_preparser_New(m_obj, &cfg);
        if (!m_preparser)
            return fail();
    }

    input_item_t *item = input_item_New(qtu(url), nullptr);
    if (!item)
        return fail();

    static const struct vlc_preparser_cbs cbs = {
        onEnded,
        onSubtreeAdded,
        nullptr,
    };

    auto request = new ReadRequest { this, receiver, done, {} };
    vlc_preparser_req *req = vlc_preparser_req_NewParse(m_preparser, item,
                                                        VLC_PREPARSER_TYPE_PARSE
                                                        | VLC_PREPARSER_OPTION_SUBITEMS,
                                                        &cbs, request);
    input_item_Release(item);
    if (!req)
    {
        delete request;
        return fail();
    }
    if (vlc_preparser_Submit(m_preparser, req) != VLC_SUCCESS)
    {
        vlc_preparser_req_Release(req);
        delete request;
        return fail();
    }
}
