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

#include "stream_output.hpp"

#include "qt.hpp"
#include "dialogs/convert/conversion_profile_model.hpp"
#include "util/soutchain.hpp"


namespace {

using vlc_playlist_locker = vlc_locker<vlc_playlist_t, vlc_playlist_Lock, vlc_playlist_Unlock>;

/* Muxers a client can read from the start of the stream, without seeking */
bool isStreamableMux(const QString &mux)
{
    static const char *const muxers[] = {
        "ts", "ps", "ogg", "webm", "mkv", "asf", "mpjpeg", "raw",
    };
    for (const char *streamable : muxers)
        if (mux == QLatin1String(streamable))
            return true;
    return false;
}

SoutModule output(const StreamOutput::Settings &settings, const QString &mux)
{
    const QString address = settings.address.trimmed();
    const QString port = QString::number(settings.port);

    switch (settings.protocol)
    {
    case StreamOutput::HTTP:
    {
        SoutModule http("http");
        http.option("mux", isStreamableMux(mux) ? mux : QStringLiteral("ts"), false);
        http.option("dst", address + ":" + port + "/", false);
        return http;
    }
    case StreamOutput::RTSP:
    {
        SoutModule rtp("rtp");
        rtp.option("sdp", "rtsp://" + address + ":" + port + "/", false);
        return rtp;
    }
    case StreamOutput::RTP:
    {
        SoutModule rtp("rtp");
        rtp.option("dst", address, false);
        rtp.option("port", port, false);
        rtp.option("mux", QStringLiteral("ts"), false);
        return rtp;
    }
    case StreamOutput::SRT:
    default:
    {
        SoutModule srt("srt");
        srt.option("mux", QStringLiteral("ts"), false);
        srt.option("dst", address + ":" + port, false);
        return srt;
    }
    }
}

} // namespace

StreamOutput::StreamOutput(vlc_playlist_t *playlist, QObject *parent)
    : QObject(parent)
    , m_playlist(playlist)
    , m_player(vlc_playlist_GetPlayer(playlist))
{
    static const struct vlc_player_cbs playerCbs = []{
        struct vlc_player_cbs cbs = {};
        cbs.on_state_changed = [](vlc_player_t *, vlc_player_state state, void *data) {
            auto that = static_cast<StreamOutput *>(data);
            /* the player is locked: start() can't change the generation */
            const unsigned generation = that->m_generation;
            QMetaObject::invokeMethod(that, [that, state, generation]() {
                that->onPlayerStateChanged(state, generation);
            }, Qt::QueuedConnection);
        };
        cbs.on_renderer_changed = [](vlc_player_t *, vlc_renderer_item_t *item, void *data) {
            auto that = static_cast<StreamOutput *>(data);
            const bool casting = item != nullptr;
            QMetaObject::invokeMethod(that, [that, casting]() {
                that->onRendererChanged(casting);
            }, Qt::QueuedConnection);
        };
        return cbs;
    }();

    static const struct vlc_playlist_callbacks playlistCbs = []{
        struct vlc_playlist_callbacks cbs = {};
        /* called with the playlist locked; m_stoppedAction is only used with
         * the playlist locked */
        cbs.on_media_stopped_action_changed = [](vlc_playlist_t *,
                                                 vlc_playlist_media_stopped_action action,
                                                 void *data) {
            static_cast<StreamOutput *>(data)->m_stoppedAction = action;
        };
        return cbs;
    }();

    vlc_playlist_locker locker(m_playlist);
    m_playerListener = vlc_player_AddListener(m_player, &playerCbs, this);
    m_playlistListener = vlc_playlist_AddListener(m_playlist, &playlistCbs, this, true);
}

StreamOutput::~StreamOutput()
{
    if (m_streaming)
        end();

    vlc_playlist_locker locker(m_playlist);
    if (m_playerListener)
        vlc_player_RemoveListener(m_player, m_playerListener);
    if (m_playlistListener)
        vlc_playlist_RemoveListener(m_playlist, m_playlistListener);
}

int StreamOutput::defaultPort(Protocol protocol)
{
    switch (protocol)
    {
    case HTTP: return 8080;
    case RTSP: return 8554;
    case RTP: return 5004;
    case SRT: return 7001;
    }
    return 0;
}

bool StreamOutput::addressRequired(Protocol protocol)
{
    return protocol == RTP || protocol == SRT;
}

bool StreamOutput::isValid(Protocol protocol, const QString &address, int port)
{
    if (port < 1 || port > 65535)
        return false;
    if (addressRequired(protocol) && address.trimmed().isEmpty())
        return false;
    return true;
}

QString StreamOutput::soutChain(const Settings &settings)
{
    if (!isValid(settings.protocol, settings.address, settings.port))
        return {};

    QString mux;
    SoutChain chain;
    if (!settings.profile.isEmpty())
        chain = ConversionProfileModel::transcodeChain(settings.profile, &mux);
    chain.header("#");

    if (settings.alsoPlayHere)
    {
        SoutModule duplicate("duplicate");
        duplicate.option("dst", QStringLiteral("display"), false);
        duplicate.option("dst", output(settings, mux));
        chain.module(duplicate);
    }
    else
        chain.module(output(settings, mux));

    return chain.to_string();
}

bool StreamOutput::start(Source source, Protocol protocol, const QString &address,
                         int port, const QString &profile, bool alsoPlayHere)
{
    Settings settings;
    settings.protocol = protocol;
    settings.address = address.trimmed();
    settings.port = port;
    settings.profile = profile;
    settings.alsoPlayHere = alsoPlayHere;
    return start(source, settings);
}

bool StreamOutput::startMedia(const QString &mrl, Protocol protocol, const QString &address,
                              int port, const QString &profile, bool alsoPlayHere)
{
    Settings settings;
    settings.protocol = protocol;
    settings.address = address.trimmed();
    settings.port = port;
    settings.profile = profile;
    settings.alsoPlayHere = alsoPlayHere;
    return startMedia(mrl, settings);
}

bool StreamOutput::start(Source source, const Settings &settings)
{
    return startAt(source, {}, settings);
}

bool StreamOutput::startMedia(const QString &mrl, const Settings &settings)
{
    if (mrl.isEmpty())
        return false;
    return startAt(CurrentItem, mrl, settings);
}

/* puts the media after the current item of the Queue (first if there is
 * none); returns its index, or -1. The playlist is locked. */
static ssize_t insertAfterCurrent(vlc_playlist_t *playlist, const QString &mrl)
{
    input_item_t *media = input_item_New(qtu(mrl), nullptr);
    if (media == nullptr)
        return -1;
    const ssize_t index = vlc_playlist_GetCurrentIndex(playlist) + 1;
    const int ret = vlc_playlist_InsertOne(playlist, index, media);
    input_item_Release(media);
    return ret == VLC_SUCCESS ? index : -1;
}

bool StreamOutput::playMedia(const QString &mrl)
{
    if (mrl.isEmpty())
        return false;

    vlc_playlist_locker locker(m_playlist);
    const ssize_t index = insertAfterCurrent(m_playlist, mrl);
    return index >= 0 && vlc_playlist_PlayAt(m_playlist, index) == VLC_SUCCESS;
}

bool StreamOutput::startAt(Source source, const QString &mrl, const Settings &settings)
{
    const QString chain = soutChain(settings);
    if (chain.isEmpty())
        return false;

    {
        vlc_playlist_locker locker(m_playlist);

        ssize_t index;
        if (!mrl.isEmpty())
            index = insertAfterCurrent(m_playlist, mrl);
        else if (vlc_playlist_Count(m_playlist) == 0)
            return false;
        else
            index = source == Queue ? 0 : vlc_playlist_GetCurrentIndex(m_playlist);
        if (index < 0)
            return false;

        /* a device we cast to would take the output instead */
        if (vlc_player_GetRenderer(m_player))
            vlc_player_SetRenderer(m_player, nullptr);

        vlc_player_SetStreamOutput(m_player, qtu(chain));

        if (!m_restoreStoppedAction)
            m_savedStoppedAction = m_stoppedAction;
        if (source == CurrentItem)
        {
            /* stream this item only: the Queue stops at its end */
            vlc_playlist_SetMediaStoppedAction(m_playlist, VLC_PLAYLIST_MEDIA_STOPPED_STOP);
            m_restoreStoppedAction = true;
        }
        else if (m_restoreStoppedAction)
        {
            vlc_playlist_SetMediaStoppedAction(m_playlist, m_savedStoppedAction);
            m_restoreStoppedAction = false;
        }

        /* the stop of what played before is not the end of the stream */
        ++m_generation;
        if (vlc_playlist_PlayAt(m_playlist, index) != VLC_SUCCESS)
        {
            vlc_player_SetStreamOutput(m_player, nullptr);
            if (m_restoreStoppedAction)
            {
                vlc_playlist_SetMediaStoppedAction(m_playlist, m_savedStoppedAction);
                m_restoreStoppedAction = false;
            }
            return false;
        }
    }

    m_settings = settings;
    m_streaming = true;
    emit streamingChanged();
    return true;
}

void StreamOutput::stop()
{
    if (!m_streaming)
        return;
    end();
    vlc_playlist_locker locker(m_playlist);
    vlc_playlist_Stop(m_playlist);
}

void StreamOutput::end()
{
    {
        vlc_playlist_locker locker(m_playlist);
        vlc_player_SetStreamOutput(m_player, nullptr);
        if (m_restoreStoppedAction)
        {
            vlc_playlist_SetMediaStoppedAction(m_playlist, m_savedStoppedAction);
            m_restoreStoppedAction = false;
        }
    }
    m_streaming = false;
    emit streamingChanged();
}

void StreamOutput::onPlayerStateChanged(vlc_player_state state, unsigned generation)
{
    /* ignore the states reported before the stream started */
    if (!m_streaming || generation != m_generation)
        return;

    if (state == VLC_PLAYER_STATE_STOPPED)
        end();
}

void StreamOutput::onRendererChanged(bool casting)
{
    if (!casting || !m_streaming)
        return;

    {
        /* the event may predate the stream, which turned the device off */
        vlc_playlist_locker locker(m_playlist);
        if (!vlc_player_GetRenderer(m_player))
            return;
    }
    end();
}
