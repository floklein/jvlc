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
#ifndef VLC_QT_STREAM_OUTPUT_HPP
#define VLC_QT_STREAM_OUTPUT_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QString>

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_player.h>

/**
 * Streams the current item or the Queue to the network (the Stream tab of
 * Stream & Cast).
 *
 * Streaming takes over the main player: its "sout" variable is set, so every
 * media it opens goes to the stream output, and the media is restarted. The
 * stream ends with stop(), when playback stops, or when the user casts to a
 * device (Cast also uses the player's output).
 */
class StreamOutput : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool streaming READ isStreaming NOTIFY streamingChanged FINAL)
    /* the settings of the running stream */
    Q_PROPERTY(Protocol protocol READ protocol NOTIFY streamingChanged FINAL)
    Q_PROPERTY(QString address READ address NOTIFY streamingChanged FINAL)
    Q_PROPERTY(int port READ port NOTIFY streamingChanged FINAL)

public:
    enum Protocol {
        HTTP,
        RTSP,
        RTP, /* RTP over UDP, MPEG transport stream */
        SRT,
    };
    Q_ENUM(Protocol)

    enum Source {
        CurrentItem,
        Queue,
    };
    Q_ENUM(Source)

    struct Settings {
        Protocol protocol = HTTP;
        /* the address to listen on (HTTP, RTSP; empty for all of them), or
         * the receiver's address (RTP, SRT) */
        QString address;
        int port = 0;
        /* a Conversion profile value, empty to stream without transcoding */
        QString profile;
        bool alsoPlayHere = false;
    };

    explicit StreamOutput(vlc_playlist_t *playlist, QObject *parent = nullptr);
    ~StreamOutput() override;

    /** The stream output chain ("#..."), or an empty string if the settings
     *  are not valid */
    static QString soutChain(const Settings &settings);

    Q_INVOKABLE static int defaultPort(Protocol protocol);
    Q_INVOKABLE static bool addressRequired(Protocol protocol);
    Q_INVOKABLE static bool isValid(Protocol protocol, const QString &address, int port);

    bool start(Source source, const Settings &settings);
    Q_INVOKABLE bool start(Source source, Protocol protocol, const QString &address,
                           int port, const QString &profile, bool alsoPlayHere);
    /** Streams this media only (Stream & Cast opened on a file): it goes
     *  after the current item of the Queue */
    bool startMedia(const QString &mrl, const Settings &settings);
    Q_INVOKABLE bool startMedia(const QString &mrl, Protocol protocol, const QString &address,
                                int port, const QString &profile, bool alsoPlayHere);
    /** Plays this media after the current item of the Queue, without a
     *  stream (casting a file: it plays on the device) */
    Q_INVOKABLE bool playMedia(const QString &mrl);
    /** Stops the stream and the playback */
    Q_INVOKABLE void stop();

    bool isStreaming() const { return m_streaming; }
    Protocol protocol() const { return m_settings.protocol; }
    QString address() const { return m_settings.address; }
    int port() const { return m_settings.port; }

signals:
    void streamingChanged();

private:
    /* streams the source, or the media of this MRL if not empty */
    bool startAt(Source source, const QString &mrl, const Settings &settings);
    void onPlayerStateChanged(vlc_player_state state, unsigned generation);
    void onRendererChanged(bool casting);
    /* ends the stream, without touching the playback */
    void end();

    vlc_playlist_t *m_playlist;
    vlc_player_t *m_player;
    vlc_player_listener_id *m_playerListener = nullptr;
    vlc_playlist_listener_id *m_playlistListener = nullptr;

    bool m_streaming = false;
    /* counts the starts of a stream, so that the player states reported
     * before a start are ignored; changed with the player locked */
    unsigned m_generation = 0;
    Settings m_settings;

    vlc_playlist_media_stopped_action m_stoppedAction = VLC_PLAYLIST_MEDIA_STOPPED_CONTINUE;
    /* what the Queue did at the end of a media before the stream */
    vlc_playlist_media_stopped_action m_savedStoppedAction = VLC_PLAYLIST_MEDIA_STOPPED_CONTINUE;
    bool m_restoreStoppedAction = false;
};

#endif // VLC_QT_STREAM_OUTPUT_HPP
