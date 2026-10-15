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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "player_controller.hpp"
#include "player_controller_p.hpp"
#include "util/shared_input_item.hpp"
#include "player/waveform.hpp"
#include "player/subtitle_timeline.hpp"
#include "player/sleep_timer.hpp"
#include "player/playback_statistics.hpp"
#include "player/subtitle_appearance.hpp"
#include "player/video_adjustments.hpp"
#include "player/player_video_outputs.hpp"
#include "player/audio_adjustments.hpp"

#include <vlc_actions.h>           /* ACTION_ID */
#include <vlc_configuration.h>
#include <vlc_url.h>            /* vlc_uri_decode */
#include <vlc_strings.h>        /* vlc_strfplayer */
#include <vlc_aout.h>           /* audio_output_t */
#include <vlc_es.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_vout.h>
#include <vlc_preparser.h>
#include <vlc_media_library.h>
#include <vlc_playlist.h>

#include <algorithm>

#include <QFile>
#include <QDir>
#include <QSignalMapper>
#include <QThreadPool>
#include <QCryptographicHash>
#ifndef QT_HAS_LIBATOMIC
#include <QReadLocker>
#include <QWriteLocker>
#endif

#include <cassert>

#define POSITION_MIN_UPDATE_INTERVAL VLC_TICK_FROM_MS(15)

//PlayerController private implementation

using SharedEsId = ::vlc::vlc_shared_data_ptr<vlc_es_id_t,
                                              &vlc_es_id_Hold,
                                              &vlc_es_id_Release>;

using SharedTitleList = ::vlc::vlc_shared_data_ptr<vlc_player_title_list,
                                                   &vlc_player_title_list_Hold,
                                                   &vlc_player_title_list_Release>;

PlayerControllerPrivate::~PlayerControllerPrivate()
{
    assert(m_smpteTimerRequestCount == 0);

    vlc_player_locker locker{m_player}; //this also locks the player
    vlc_player_vout_RemoveListener( m_player, m_player_vout_listener );
    vlc_player_aout_RemoveListener( m_player, m_player_aout_listener );
    vlc_player_RemoveListener( m_player, m_player_listener );
    vlc_player_RemoveTimer( m_player, m_player_timer );
    if (m_preparser != nullptr)
        vlc_preparser_Delete(m_preparser);
}

bool PlayerControllerPrivate::isCurrentItemSynced()
{
    /* The media can change before the UI gets updated and thus a player
     * request can be submitted on the wrong media or worse, no media at
     * all. Here d->m_currentItem is read and modified under the player
     * lock. */
    return m_currentItem.get() == vlc_player_GetCurrentMedia( m_player );
}


void PlayerControllerPrivate::UpdateName(input_item_t* media)
{
    Q_Q(PlayerController);
    /* Update text, name and nowplaying */
    QString name;
    if (! media)
        return;

    /* Try to get the nowplaying */
    char *format = var_InheritString( p_intf, "input-title-format" );
    char *formatted = NULL;
    if (format != NULL)
    {
        vlc_player_Lock( m_player );
        formatted = vlc_strfplayer( m_player, media, format );
        vlc_player_Unlock( m_player );
        free( format );
        if( formatted != NULL )
        {
            name = qfu(formatted);
            free( formatted );
        }
    }

    /* If we have Nothing */
    if( name.simplified().isEmpty() )
    {
        char *uri = input_item_GetURI( media );
        char *file = uri ? strrchr( uri, '/' ) : NULL;
        if( file != NULL )
        {
            vlc_uri_decode( ++file );
            name = qfu(file);
        }
        else
            name = qfu(uri);
        free( uri );
    }

    name = name.trimmed();

    if( m_name != name )
    {
        emit q->nameChanged( name );
        m_name = std::move(name);
    }
}


void PlayerControllerPrivate::UpdateArt(input_item_t *p_item)
{
    Q_Q(PlayerController);
    if (! p_item)
        return;

    QString url = PlayerController::decodeArtURL( p_item );

    /* the art hasn't changed, no need to update */
    if(m_artUrl == url)
        return;

    /* Update Art meta */
    m_artUrl = std::move(url);
    emit q->artChanged( m_artUrl );
}

void PlayerControllerPrivate::UpdateStats( const input_stats_t& stats )
{
    Q_Q(PlayerController);
    emit q->statisticsUpdated( stats );
}

void PlayerControllerPrivate::UpdateProgram(enum vlc_player_list_action action, const struct vlc_player_program* prgm)
{
    Q_Q(PlayerController);
    m_programList.updatePrograms( action, prgm );

    bool hasPrograms = (m_programList.getCount() > 1);

    if (m_hasPrograms != hasPrograms)
    {
        m_hasPrograms = hasPrograms;

        emit q->hasProgramsChanged(hasPrograms);
    }

    emit q->isEncryptedChanged( prgm->scrambled );
}

void PlayerControllerPrivate::UpdateTrackSelection(vlc_es_id_t *trackid, bool selected)
{
    if (trackid == NULL)
        return;
    es_format_category_e cat = vlc_es_id_GetCat(trackid);
    TrackListModel* tracklist;
    switch (cat) {
    case VIDEO_ES: tracklist = &m_videoTracks; break;
    case AUDIO_ES: tracklist = &m_audioTracks; break;
    case SPU_ES: tracklist = &m_subtitleTracks; break;
    default: return;
    }
    tracklist->updateTrackSelection(trackid, selected);
}

void PlayerControllerPrivate::UpdateMeta( input_item_t *p_item )
{
    Q_Q(PlayerController);

    {
        vlc_mutex_locker locker(&p_item->lock);

        if (p_item->p_meta)
        {
            m_title   = vlc_meta_Get(p_item->p_meta, vlc_meta_Title);
            m_artist  = vlc_meta_Get(p_item->p_meta, vlc_meta_Artist);
            m_album   = vlc_meta_Get(p_item->p_meta, vlc_meta_Album);
            m_artwork = vlc_meta_Get(p_item->p_meta, vlc_meta_ArtworkURL);

            /* Capture the raw sylt-data buffer; the entry list is built on
             * first read so we don't pay for it when lyrics are never shown.
             * Later meta refreshes may not carry sylt-data again, so keep the
             * cached buffer until the current media changes. */
            const char *psz_sylt_data = vlc_meta_GetExtra( p_item->p_meta, "sylt-data" );
            if (!psz_sylt_data)
                psz_sylt_data = vlc_meta_GetExtra( p_item->p_meta, "uslt-data" );

            if (psz_sylt_data != NULL)
            {
                QByteArray newRaw( psz_sylt_data );
                const auto hash = qHash(newRaw);
                if (!m_syltRaw || hash != m_syltRaw->second)
                {
                    m_syltRaw = qMakePair(std::move( newRaw ), hash);
                    m_syltLyrics.reset();
                }
            }
        }
    }

    if (m_syltRaw) {
        emit q->hasLyricsChanged(true);
        emit q->syltLyricsChanged();
    }

    emit q->currentMetaChanged( p_item  );
}

QList<TimedText>
PlayerControllerPrivate::syltLyrics() const
{
    if (!m_syltLyrics.has_value())
    {
        QList<TimedText> lyrics;

        if (m_syltRaw)
        {
            assert(!m_syltRaw->first.isEmpty());
            const QString sylt = QString::fromUtf8(m_syltRaw->first);
            m_syltRaw->first.clear();

            const auto entries = QStringView( sylt ).split( QChar( 0x1E ), Qt::SkipEmptyParts );

            for (const QStringView &entry : entries)
            {
                const qsizetype sep = entry.indexOf( QChar( 0x1F ) );
                if (sep <= 0)
                    continue;

                const vlc_tick_t time =
                    VLC_TICK_FROM_MS( entry.first( sep ).toLongLong() );
                lyrics.push_back( TimedText( entry.sliced( sep + 1 ).toString(), VLCTime( time ) ) );
            }
            m_syltLyrics = std::move( lyrics );
        }
    }
    return *m_syltLyrics;
}

void PlayerControllerPrivate::UpdateInfo( input_item_t *p_item )
{
    Q_Q(PlayerController);
    emit q->infoChanged( p_item );
}

void PlayerControllerPrivate::UpdateVouts(vout_thread_t **vouts, size_t i_vouts, enum vlc_player_vout_action action)
{
    Q_Q(PlayerController);
    bool hadVideo = m_hasVideo;

    if (action == VLC_PLAYER_VOUT_STARTED)
        ++m_vout_ref;
    else if (action == VLC_PLAYER_VOUT_STOPPED)
        --m_vout_ref;
    assert(m_vout_ref >= 0);
    m_hasVideo = (m_vout_ref > 0);

    vout_thread_t* main_vout = nullptr;
    if (m_hasVideo)
        main_vout = vouts[0];

    m_videoStereoMode.resetObject( main_vout );
    m_zoom.resetObject( main_vout );
    m_aspectRatio.resetObject( main_vout );
    m_crop.resetObject(  main_vout );
    m_fit.resetObject(  main_vout );
    m_deinterlace.resetObject( main_vout );
    m_deinterlaceMode.resetObject( main_vout );
    m_autoscale.resetObject( main_vout );

    emit q->voutListChanged(vouts, i_vouts);
    if( hadVideo != m_hasVideo )
        emit q->hasVideoOutputChanged(m_hasVideo);
}

void PlayerControllerPrivate::UpdateSpuOrder(vlc_es_id_t *es_id, enum vlc_vout_order order)
{
    Q_Q(PlayerController);
    const bool hadSecondary = m_secondarySpuEsId.get() != NULL;

    switch (order)
    {
        case VLC_VOUT_ORDER_NONE:
            if (es_id == m_secondarySpuEsId.get())
                m_secondarySpuEsId.reset(NULL, false);
            break;
        case VLC_VOUT_ORDER_SECONDARY:
            m_secondarySpuEsId.reset(es_id, true);
            if (m_secondarySubtitleDelay.valid())
            {
                vlc_player_locker lock{ m_player };
                vlc_player_SetEsIdDelay(m_player, es_id,
                                        m_secondarySubtitleDelay.toVLCTick(),
                                        VLC_PLAYER_WHENCE_ABSOLUTE);
            }
            break;
        default:
            break;
    }

    if (hadSecondary != (m_secondarySpuEsId.get() != NULL))
        emit q->hasSecondarySubtitleChanged();
}

/* The tracks of an external file have string ids starting with the MD5 of
 * its MRL (see InputSourceNew), embedded tracks with their category */
static bool isEmbeddedTrackId(const char *str_id)
{
    if (str_id == NULL)
        return true;
    const char *end = strchr(str_id, '/');
    if (end == NULL)
        return true;
    const QLatin1StringView source(str_id, end - str_id);
    for (const char *cat : { "video", "audio", "spu", "data", "unknown" })
        if (source == QLatin1StringView(cat))
            return true;
    return false;
}

/* The file of an external track, empty until the input adds it to the item */
static QString slaveMrlOfTrack(input_item_t *item, const char *str_id)
{
    if (item == NULL || isEmbeddedTrackId(str_id))
        return {};
    const char *end = strchr(str_id, '/');
    const QLatin1StringView source(str_id, end - str_id);

    vlc_mutex_locker lock{ &item->lock };
    for (int i = 0; i < item->i_slaves; ++i)
    {
        const char *uri = item->pp_slaves[i]->psz_uri;
        const QByteArray hash = QCryptographicHash::hash(QByteArray(uri),
                                                         QCryptographicHash::Md5).toHex();
        if (source == QLatin1StringView(hash))
            return qfu(uri);
    }
    return {};
}

/* The subtitle timeline shows the main subtitle: the selected track that is
 * not the second subtitle */
void PlayerControllerPrivate::UpdateSubtitleTimelineTrack()
{
    Q_Q(PlayerController);
    SubtitleTimeline::TrackKind kind = SubtitleTimeline::NoTrack;
    QString mrl;
    bool fileUnknown = false;
    {
        vlc_player_locker lock{ m_player };
        const size_t count = vlc_player_GetSubtitleTrackCount(m_player);
        for (size_t i = 0; i < count; ++i)
        {
            const struct vlc_player_track *track = vlc_player_GetSubtitleTrackAt(m_player, i);
            if (!track->selected || track->es_id == m_secondarySpuEsId.get())
                continue;

            const char *str_id = vlc_es_id_GetStrId(track->es_id);
            if (isEmbeddedTrackId(str_id))
            {
                kind = SubtitleTimeline::EmbeddedTrack;
                break;
            }
            mrl = slaveMrlOfTrack(vlc_player_GetCurrentMedia(m_player), str_id);
            if (mrl.isEmpty())
                fileUnknown = true;
            else if (SubtitleTimeline::isTextCodec(track->fmt.i_codec))
                kind = SubtitleTimeline::ExternalTextTrack;
            else
                kind = SubtitleTimeline::ExternalPictureTrack;
            break;
        }
    }
    m_subtitleTimeline->setTrack(kind, mrl);

    /* the input adds the file to the item just after its tracks: look again */
    if (fileUnknown && m_subtitleTimelineRetries < 10)
    {
        ++m_subtitleTimelineRetries;
        QTimer::singleShot(200, q, [this]() { UpdateSubtitleTimelineTrack(); });
    }
    else
        m_subtitleTimelineRetries = 0;
}

int PlayerControllerPrivate::interpolateTime(vlc_tick_t system_now)
{
    vlc_tick_t new_time;
    if (vlc_player_timer_point_Interpolate(&m_player_time, system_now,
                                           &new_time, &m_position) == VLC_SUCCESS)
    {
        m_time = new_time;
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

extern "C" {

//player callbacks

static  void on_player_current_media_changed(vlc_player_t *, input_item_t *new_media, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_current_media_changed");

    if (!new_media)
    {
        that->callAsync([that] () {
            {
                vlc_player_locker lock{ that->m_player };
                that->m_currentItem.reset(nullptr);
            }

            that->m_syltRaw.reset();
            that->m_syltLyrics.reset();
            that->m_currentLyricIndex = -1;
            emit that->q_func()->hasLyricsChanged(false);
            emit that->q_func()->syltLyricsChanged();
            emit that->q_func()->currentLyricIndexChanged(-1);
            emit that->q_func()->currentLyricChanged();
            emit that->q_func()->inputChanged(false);
        });
        return;
    }

    SharedInputItem newMediaPtr = SharedInputItem( new_media );
    that->callAsync([that,newMediaPtr] () {
        PlayerController* q = that->q_func();

        that->m_syltRaw.reset();
        that->m_syltLyrics.reset();
        that->m_currentLyricIndex = -1;
        emit q->hasLyricsChanged(false);
        emit q->syltLyricsChanged();
        emit q->currentLyricIndexChanged(-1);
        emit q->currentLyricChanged();

        that->UpdateArt( newMediaPtr.get() );
        that->UpdateMeta( newMediaPtr.get() );
        that->UpdateInfo( newMediaPtr.get() );
        that->UpdateName( newMediaPtr.get() );
        that->m_url = vlc::wrap_cptr( input_item_GetURI( newMediaPtr.get() ) ).get();

        {
            vlc_player_locker lock{ that->m_player };
            that->m_currentItem = std::move(newMediaPtr);
        }

        that->m_canRestorePlayback = false;
        emit q->playbackRestoreQueried();

        emit q->inputChanged(true);
    });
}

static void on_player_state_changed(vlc_player_t *, enum vlc_player_state state, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_state_changed");

    that->callAsync([that,state] () {
        PlayerController* q = that->q_func();
        that->m_playing_status = static_cast<PlayerController::PlayingState>(state);
        switch ( state ) {
        case VLC_PLAYER_STATE_STARTED:
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_STARTED");
            break;
        case VLC_PLAYER_STATE_PLAYING:
        {
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_PLAYING");
            SharedAOut aout = q->getAout();
            that->m_audioStereoMode.resetObject( aout.get() );
            that->m_audioMixMode.resetObject( aout.get() );
            that->m_audioVisualization.resetObject( aout.get() );
            break;
        }
        case VLC_PLAYER_STATE_PAUSED:
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_PAUSED");
            break;
        case VLC_PLAYER_STATE_STOPPING:
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_STOPPING");
            break;
        case VLC_PLAYER_STATE_STOPPED:
        {
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_STOPPED");

            that->m_audioStereoMode.resetObject((audio_output_t*)nullptr);
            that->m_audioMixMode.resetObject((audio_output_t*)nullptr);
            that->m_audioVisualization.resetObject((audio_output_t*)nullptr);

            /* reset the state on stop */
            that->m_position = 0;
            that->m_time = 0;
            that->m_length = 0;
            emit q->positionUpdated( -1.0, VLCTime() , VLCDuration() );
            that->m_rate = 1.0f;
            emit q->rateChanged( 1.0f );
            that->m_name = "";
            emit q->nameChanged( "" );
            that->m_hasChapters = false;
            emit q->hasChaptersChanged( false );
            that->m_hasTitles = false;
            emit q->hasTitlesChanged( false );
            that->m_hasMenu= false;
            emit q->hasMenuChanged( false );
            that->m_isMenu= false;
            emit q->isMenuChanged( false );
            that->m_isInteractive= false;
            emit q->isInteractiveChanged( false );
            that->m_canRestorePlayback = false;
            emit q->playbackRestoreQueried();

            that->m_teletextAvailable = false;
            emit q->teletextAvailableChanged( false );
            that->m_ABLoopState = PlayerController::ABLOOP_STATE_NONE;
            that->m_ABLoopA = VLCTime();
            that->m_ABLoopB = VLCTime();
            emit q->ABLoopStateChanged(PlayerController::ABLOOP_STATE_NONE);
            emit q->ABLoopAChanged(that->m_ABLoopA);
            emit q->ABLoopBChanged(that->m_ABLoopB);
            that->m_hasVideo = false;
            emit q->hasVideoOutputChanged( false );

            emit q->voutListChanged( NULL, 0 );

            /* Reset all InfoPanels but stats */
            that->m_artUrl = "";
            emit q->artChanged( NULL );
            emit q->artChanged( "" );
            emit q->infoChanged( NULL );
            emit q->currentMetaChanged( (input_item_t *)NULL );

            if (that->m_syltRaw || that->m_currentLyricIndex != -1)
            {
                that->m_syltRaw.reset();
                that->m_syltLyrics.reset();
                that->m_currentLyricIndex = -1;
                emit q->hasLyricsChanged( false );
                emit q->syltLyricsChanged();
                emit q->currentLyricIndexChanged( -1 );
                emit q->currentLyricChanged();
            }

            that->m_hasPrograms =false;
            emit q->hasProgramsChanged( false );

            that->m_encrypted =false;
            emit q->isEncryptedChanged( false );
            that->m_recording =false;
            emit q->recordingChanged( false );

            that->m_buffering = 0.f;
            emit q->bufferingChanged( 0.0 );

            break;
        }
        }
        emit q->playingStateChanged( that->m_playing_status );
    });
}

void on_player_error_changed(vlc_player_t *, enum vlc_player_error , void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_error_changed");
}

static void on_player_buffering(vlc_player_t *, float new_buffering, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_buffering");
    that->callAsync([that,new_buffering](){
        that->m_buffering = new_buffering;
        emit that->q_func()->bufferingChanged( new_buffering );
    });
}

static void on_player_capabilities_changed(vlc_player_t *, int old_caps, int new_caps, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_capabilities_changed");
    that->callAsync([that, old_caps, new_caps]() {
        PlayerController* q = that->q_func();
        that->m_capabilities = new_caps;

        bool oldSeekable = old_caps & VLC_PLAYER_CAP_SEEK;
        bool newSeekable = new_caps & VLC_PLAYER_CAP_SEEK;
        if (newSeekable != oldSeekable)
            emit q->seekableChanged( newSeekable );

        bool oldRewindable = old_caps & VLC_PLAYER_CAP_REWIND;
        bool newRewindable = new_caps & VLC_PLAYER_CAP_REWIND;
        if (newRewindable != oldRewindable)
            emit q->rewindableChanged( newRewindable );

        bool oldPauseable = old_caps & VLC_PLAYER_CAP_PAUSE;
        bool newPauseable = new_caps & VLC_PLAYER_CAP_PAUSE;
        if (newPauseable != oldPauseable)
            emit q->pausableChanged( newPauseable );

        bool oldChangeRate = old_caps & VLC_PLAYER_CAP_CHANGE_RATE;
        bool newChangeRate = new_caps & VLC_PLAYER_CAP_CHANGE_RATE;
        if (newChangeRate != oldChangeRate)
            emit q->rateChangableChanged( newChangeRate );
    });

    //FIXME other events?
}

static void on_player_track_list_changed(vlc_player_t *, enum vlc_player_list_action action, const struct vlc_player_track *track, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    struct vlc_player_track* dup = vlc_player_track_Dup(track);
    if (!dup)
        return;
    std::shared_ptr<struct vlc_player_track> trackPtr(dup, vlc_player_track_Delete);

    msg_Dbg( that->p_intf, "on_player_track_list_changed");
    that->callAsync([that,action,trackPtr] () {
        switch (trackPtr.get()->fmt.i_cat) {
        case VIDEO_ES:
            msg_Dbg( that->p_intf, "on_player_track_list_changed (video)");
            that->m_videoTracks.updateTracks( action, trackPtr.get() );
            break;
        case AUDIO_ES:
            msg_Dbg( that->p_intf, "on_player_track_list_changed (audio)");
            that->m_audioTracks.updateTracks( action, trackPtr.get() );
            break;
        case SPU_ES:
            msg_Dbg( that->p_intf, "on_player_track_list_changed (spu)");
            that->m_subtitleTracks.updateTracks( action, trackPtr.get() );
            break;
        default:
            //we don't handle other kind of tracks
            msg_Dbg( that->p_intf, "on_player_track_list_changed (other)");
            break;
        }
    });
}

static void on_player_track_selection_changed(vlc_player_t *, vlc_es_id_t * unselected, vlc_es_id_t *selected, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_track_selection_changed");

    SharedEsId unselectedPtr = SharedEsId(unselected);
    SharedEsId selectedPtr = SharedEsId(selected);

    that->callAsync([that,unselectedPtr,selectedPtr] () {
        if (unselectedPtr)
            that->UpdateTrackSelection( unselectedPtr.get(), false );
        if (selectedPtr)
            that->UpdateTrackSelection( selectedPtr.get(), true );
    });
}


static void on_player_program_list_changed(vlc_player_t *, enum vlc_player_list_action action, const struct vlc_player_program *prgm, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_program_list_changed");
    struct vlc_player_program* dup = vlc_player_program_Dup(prgm);
    if (!dup)
        return;
    std::shared_ptr<struct vlc_player_program> prgmPtr(dup, vlc_player_program_Delete);

    that->callAsync([that,action,prgmPtr] (){
        that->UpdateProgram(action, prgmPtr.get());
    });
}

static void on_player_program_selection_changed(vlc_player_t *, int unselected, int selected, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_program_selection_changed");

    that->callAsync([that,unselected,selected] (){
        that->m_programList.updateProgramSelection(unselected, false);
        that->m_programList.updateProgramSelection(selected, true);
    });
}

static void on_player_titles_changed(vlc_player_t *, struct vlc_player_title_list *titles, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_title_array_changed");

    SharedTitleList sharedTitleList = SharedTitleList(titles);

    that->callAsync([that,sharedTitleList] (){
        struct vlc_player_title_list *titles = sharedTitleList.get();
        that->m_chapterList.resetTitle(nullptr);
        that->m_titleList.resetTitles(titles);

        bool hasMenu = false;
        bool hasTitles = false;
        if (titles)
        {
            size_t nbTitles = vlc_player_title_list_GetCount(titles);
            for( size_t i = 0; i < nbTitles; i++)
            {
                const vlc_player_title* title = vlc_player_title_list_GetAt(titles, i);
                if( (title ->flags & INPUT_TITLE_MENU) != 0 )
                {
                    hasMenu = true;
                    break;
                }
            }
            hasTitles = nbTitles != 0;
        }
        if (hasTitles != that->m_hasTitles)
        {
            that->m_hasTitles = hasTitles;
            emit that->q_func()->hasTitlesChanged(hasTitles);
        }
        if (!hasTitles && that->m_hasChapters)
        {
            that->m_hasChapters = false;
            emit that->q_func()->hasChaptersChanged(false);
        }
        if (hasMenu != that->m_hasMenu)
        {
            that->m_hasMenu = hasMenu;
            emit that->q_func()->hasMenuChanged(hasMenu);
        }
    });
}

static void on_player_title_selection_changed(vlc_player_t *,
                                              const struct vlc_player_title *new_title, size_t new_idx, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_title_selection_changed");

    bool hasChapter = (new_title != nullptr && new_title->chapter_count != 0);
    that->callAsync([that,new_title,new_idx,hasChapter] (){
        PlayerController* q = that->q_func();
        that->m_chapterList.resetTitle(new_title);
        that->m_titleList.setCurrent(new_idx);
        that->m_hasChapters  = hasChapter;
        that->m_isInteractive = new_title && (new_title->flags & VLC_PLAYER_TITLE_INTERACTIVE);
        that->m_isMenu = new_title && (new_title->flags & VLC_PLAYER_TITLE_MENU);
        emit q->isMenuChanged( that->m_isMenu );
        emit q->isInteractiveChanged( that->m_isInteractive );
        emit q->hasChaptersChanged( hasChapter );
    });
}

static void on_player_chapter_selection_changed(vlc_player_t *,
                                                const struct vlc_player_title *, size_t,
                                                const struct vlc_player_chapter *, size_t chapter_idx,
                                                void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_chapter_selection_changed");
    that->callAsync([that,chapter_idx] (){
        that->m_chapterList.setCurrent(chapter_idx);
    });
}


static void on_player_teletext_menu_changed(vlc_player_t *, bool has_teletext_menu, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_menu_changed, %s", has_teletext_menu ? "available" : "unavailable" );
    that->callAsync([that,has_teletext_menu] () {
        that->m_teletextAvailable = has_teletext_menu;
        emit that->q_func()->teletextAvailableChanged(has_teletext_menu);
    });
}

static void on_player_teletext_enabled_changed(vlc_player_t *, bool enabled, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_enabled_changed %s", enabled ? "enabled" : "disabled");
    that->callAsync([that,enabled] () {
        that->m_teletextEnabled = enabled;
        emit that->q_func()->teletextEnabledChanged(enabled);
    });
}

static void on_player_teletext_page_changed(vlc_player_t *, unsigned new_page, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_page_changed %u", new_page);
    that->callAsync([that,new_page] () {
        that->m_teletextPage = new_page;
        emit that->q_func()->teletextPageChanged(new_page);
    });
}

static void on_player_teletext_transparency_changed(vlc_player_t *, bool enabled, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_transparency_changed %s", enabled ? "enabled" : "disabled");
    that->callAsync([that,enabled] () {
        that->m_teletextTransparent = enabled;
        emit that->q_func()->teletextTransparencyChanged(enabled);
    });
}

static void on_player_category_delay_changed(vlc_player_t *,
                               enum es_format_category_e cat, vlc_tick_t new_delay,
                               void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_category_delay_changed: %d", cat );
    that->callAsync([that,cat,new_delay] (){
        switch (cat)
        {
            case AUDIO_ES:
                that->m_audioDelay = new_delay;
                emit that->q_func()->audioDelayChanged( new_delay );
                break;
            case SPU_ES:
                that->m_subtitleDelay = new_delay;
                emit that->q_func()->subtitleDelayChanged( new_delay );
                break;
            default:
                break;
        }
    });
}

static void on_player_track_delay_changed(vlc_player_t *,
                                vlc_es_id_t *es_id, vlc_tick_t new_delay,
                                void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    SharedEsId sharedEsId = SharedEsId(es_id);

    that->callAsync([that,sharedEsId,new_delay] (){
        if (that->m_secondarySpuEsId == sharedEsId)
        {
            that->m_secondarySubtitleDelay = new_delay;
            emit that->q_func()->secondarySubtitleDelayChanged( new_delay );
        }
    });
}

static void on_player_associated_subs_fps_changed(vlc_player_t *, float subs_fps, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_associated_subs_fps_changed");
    that->callAsync([that,subs_fps] (){
        that->m_subtitleFPS = subs_fps;
        emit that->q_func()->subtitleFPSChanged( subs_fps );
    });
}

void on_player_renderer_changed(vlc_player_t *, vlc_renderer_item_t *new_item, void *data)
{
    VLC_UNUSED(new_item);
    VLC_UNUSED(data);
}


static void on_player_record_changed(vlc_player_t *, bool recording, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_record_changed");
    that->callAsync([that,recording] (){
        that->m_recording = recording;
        emit that->q_func()->recordingChanged( recording );
    });
}

static void on_player_signal_changed(vlc_player_t *, float quality, float strength, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    VLC_UNUSED(quality);
    VLC_UNUSED(strength);
    msg_Dbg( that->p_intf, "on_player_signal_changed");
}

static void on_player_stats_changed(vlc_player_t *, const struct input_stats_t *stats, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    struct input_stats_t stats_tmp = *stats;
    that->callAsync([that,stats_tmp] () {
        that->m_stats = stats_tmp;
        emit that->q_func()->statisticsUpdated( that->m_stats );
    });
}

static void on_player_atobloop_changed(vlc_player_t *, enum vlc_player_abloop state, vlc_tick_t time, double, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_atobloop_changed");
    that->callAsync([that,state,time] (){
        PlayerController* q = that->q_func();
        switch (state) {
        case VLC_PLAYER_ABLOOP_NONE:
            that->m_ABLoopA = VLCTime();
            that->m_ABLoopB = VLCTime();
            emit q->ABLoopAChanged(that->m_ABLoopA);
            emit q->ABLoopBChanged(that->m_ABLoopB);
            break;
        case VLC_PLAYER_ABLOOP_A:
            that->m_ABLoopA = time;
            emit q->ABLoopAChanged(that->m_ABLoopA);
            break;
        case VLC_PLAYER_ABLOOP_B:
            that->m_ABLoopB = time;
            emit q->ABLoopBChanged(that->m_ABLoopB);
            break;
        }
        that->m_ABLoopState = static_cast<PlayerController::ABLoopState>(state);
        emit q->ABLoopStateChanged(that->m_ABLoopState);
    });
}

static void on_player_media_meta_changed(vlc_player_t *, input_item_t *media, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_item_meta_changed");

    SharedInputItem mediaPtr(media);
    //call on object thread
    that->callAsync([that,mediaPtr] () {
        that->UpdateName(mediaPtr.get());
        that->UpdateArt(mediaPtr.get());
        that->UpdateMeta(mediaPtr.get());
        that->UpdateInfo(mediaPtr.get());
    });
}

static void on_player_media_epg_changed(vlc_player_t *, input_item_t *, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_item_epg_changed");
    that->callAsync([that] () {
        emit that->q_func()->epgChanged();
    });
}

static void on_player_subitems_changed(vlc_player_t *, input_item_t *, const input_item_node_t *, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_subitems_changed");
}


static void on_player_vout_changed(vlc_player_t *player, enum vlc_player_vout_action action,
    vout_thread_t *, enum vlc_vout_order order, vlc_es_id_t *es_id, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_vout_list_changed");

    switch (vlc_es_id_GetCat(es_id))
    {
        case VIDEO_ES:
        {
            //player is locked within callbacks*
            size_t i_vout = 0;
            vout_thread_t **vouts = vlc_player_vout_HoldAll(player, &i_vout);

            std::shared_ptr<vout_thread_t*> voutsPtr( vouts, [i_vout]( vout_thread_t**vouts ) {
                for (size_t i = 0; i < i_vout; i++)
                    vout_Release( vouts[i] );
                free(vouts);
            });

            //call on object thread
            that->callAsync([that,voutsPtr,i_vout,action] () {
                that->UpdateVouts(voutsPtr.get(), i_vout, action);
            });
            break;
        }
        case SPU_ES:
        {
            SharedEsId sharedEsId = SharedEsId(es_id);
            that->callAsync([that,sharedEsId,order] () {
                that->UpdateSpuOrder(sharedEsId.get(), order);
            });
            break;
        }
        default:
            break;
    }
}

//player vout callbacks

static void on_player_vout_fullscreen_changed(vout_thread_t* vout, bool is_fullscreen, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_vout_fullscreen_changed %s", is_fullscreen ? "fullscreen" : "windowed");

    SharedVOutThread vOutThread(vout);
    that->callAsync([that,vOutThread,is_fullscreen] () {
        PlayerController* q = that->q_func();
        const PlayerController::VOutThreadList voutList = q->getVouts();
        if (vOutThread == nullptr //property sets for all vout
            || (voutList.size() == 1 && vOutThread.get() == voutList[0].get()) ) //on the only vout
        {
            that->m_fullscreen = is_fullscreen;
            emit q->fullscreenChanged(is_fullscreen);
        }
    });
}

static void on_player_vout_wallpaper_mode_changed(vout_thread_t* vout, bool enabled, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_vout_wallpaper_mode_changed %s", enabled ? "enabled" : "disabled");

    SharedVOutThread vOutThread(vout);
    that->callAsync([that,vOutThread, enabled] () {
        PlayerController* q = that->q_func();
        const PlayerController::VOutThreadList voutList = q->getVouts();
        if (vOutThread == nullptr  //property sets for all vout
            || (voutList.size() == 1 && vOutThread.get() == voutList[0].get()) ) //on the only vout
        {
            that->m_wallpaperMode = enabled;
            emit q->wallpaperModeChanged(enabled);
        }
    });
}

//player aout callbacks

static void on_player_aout_volume_changed(audio_output_t *, float volume, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_aout_volume_changed");
    that->callAsync([that,volume](){
        that->m_volume = volume;
        emit that->q_func()->volumeChanged( volume );
    });
}

static void on_player_aout_mute_changed(audio_output_t *, bool muted, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_aout_mute_changed");
    that->callAsync([that,muted](){
        that->m_muted = muted;
        emit that->q_func()->soundMuteChanged(muted);
    });
}

static void on_player_corks_changed(vlc_player_t *, unsigned, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_corks_changed");
}

static void on_player_playback_restore_queried(vlc_player_t *player, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_playback_restore_queried");

    // the saved position, for the continue-watching pill
    float position = 0.f;
    vlc_medialibrary_t* ml = vlc_ml_instance_get( that->p_intf );
    input_item_t* item = vlc_player_GetCurrentMedia( player );
    if ( ml && item && item->psz_uri )
    {
        vlc_ml_media_t* media = vlc_ml_get_media_by_mrl( ml, item->psz_uri );
        if ( media )
        {
            position = media->f_progress;
            vlc_ml_release( media );
        }
    }

    that->callAsync([that, position](){
        that->m_canRestorePlayback = true;
        that->m_restorePlaybackPosition = position;
        emit that->q_func()->playbackRestoreQueried();
    });
}

static void on_player_timer_update(const struct vlc_player_timer_point *point,
                                   void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    if (that->seeking)
        return;

    that->callAsync([that,point_copy = *point](){
        PlayerController* q = that->q_func();

        that->m_player_time = point_copy;
        bool lengthOrRateChanged = false;

        if (that->m_length.toVLCTick() != that->m_player_time.length)
        {
            that->m_length = that->m_player_time.length > 0 ? VLCDuration(that->m_player_time.length) : VLCDuration();
            emit q->lengthChanged(that->m_length);

            lengthOrRateChanged = true;
        }
        if (that->m_rate != that->m_player_time.rate)
        {
            that->m_rate = that->m_player_time.rate;
            emit q->rateChanged(that->m_rate);

            lengthOrRateChanged = true;
        }

        vlc_tick_t system_now = vlc_tick_now();
        if (that->interpolateTime(system_now) == VLC_SUCCESS)
        {
            if (lengthOrRateChanged || !that->m_position_timer.isActive())
            {
                q->updatePosition();

                if (that->m_player_time.system_date != VLC_TICK_MAX)
                {
                    // Setup the position update interval, depending on media
                    // length and rate.  XXX: VLC_TICK_FROM_MS(1) is an educated
                    // guess, it should be also calculated according to the slider
                    // size.

                    vlc_tick_t interval =
                        that->m_length.toVLCTick() / that->m_player_time.rate / VLC_TICK_FROM_MS(1);
                    if (interval < POSITION_MIN_UPDATE_INTERVAL)
                        interval = POSITION_MIN_UPDATE_INTERVAL;

                    that->m_position_timer.start(MS_FROM_VLC_TICK(interval));
                }
            }
            q->updateTime(system_now, lengthOrRateChanged);
        }

    });
}

static void on_player_timer_paused(vlc_tick_t system_date, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    that->callAsync([that,system_date](){
        PlayerController* q = that->q_func();

        if (system_date != VLC_TICK_INVALID
         && that->interpolateTime(system_date) == VLC_SUCCESS)
        {
            // The discontinuity event got a valid system date, update the time
            // properties.
            q->updatePosition();
            q->updateTime(system_date, false);
        }

        // And stop the timers.
        that->m_position_timer.stop();
        that->m_time_timer.stop();
    });
}

static void on_player_timer_seek(const struct vlc_player_timer_point *point,
                                 void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    if (point != NULL)
    {
        that->seeking = true;
        that->callAsync([that,point_copy = *point](){
            PlayerController* q = that->q_func();

            that->m_player_time = point_copy;
            if (that->m_player_time.position > 0)
                that->m_position = that->m_player_time.position;
            if (that->m_player_time.ts != VLC_TICK_INVALID)
                that->m_time = that->m_player_time.ts - VLC_TICK_0;

            q->updatePosition();
            q->updateTime(VLC_TICK_INVALID, false);

            that->m_position_timer.stop();
            that->m_time_timer.stop();
        });
    }
    else
        that->seeking = false;
}
static void on_player_timer_smpte_update(const struct vlc_player_timer_smpte_timecode *tc,
                                         void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate *>(data);
#ifndef QT_HAS_LIBATOMIC
    QWriteLocker lock(&that->m_highResolutionTimeLock);
#endif
    that->m_highResolutionTime = *tc;
}


static void on_preparse_ended_callback(vlc_preparser_req *req,
                                       int, void *userdata)
{
    input_item_t *p_item = vlc_preparser_req_GetItem(req);
    PlayerControllerPrivate *me = reinterpret_cast<PlayerControllerPrivate *>(userdata);
    me->onArtFetchEnded(p_item, input_item_IsArtFetched(p_item));
}


} //extern "C"

static const struct vlc_player_cbs player_cbs = [] {
    struct vlc_player_cbs cbs {};
    cbs.on_current_media_changed = on_player_current_media_changed;
    cbs.on_state_changed = on_player_state_changed;
    cbs.on_error_changed = on_player_error_changed;
    cbs.on_buffering_changed = on_player_buffering;
    cbs.on_capabilities_changed = on_player_capabilities_changed;
    cbs.on_track_list_changed = on_player_track_list_changed;
    cbs.on_track_selection_changed = on_player_track_selection_changed;
    cbs.on_track_delay_changed = on_player_track_delay_changed;
    cbs.on_program_list_changed = on_player_program_list_changed;
    cbs.on_program_selection_changed = on_player_program_selection_changed;
    cbs.on_titles_changed = on_player_titles_changed;
    cbs.on_title_selection_changed = on_player_title_selection_changed;
    cbs.on_chapter_selection_changed = on_player_chapter_selection_changed;
    cbs.on_teletext_menu_changed = on_player_teletext_menu_changed;
    cbs.on_teletext_enabled_changed = on_player_teletext_enabled_changed;
    cbs.on_teletext_page_changed = on_player_teletext_page_changed;
    cbs.on_teletext_transparency_changed = on_player_teletext_transparency_changed;
    cbs.on_category_delay_changed = on_player_category_delay_changed;
    cbs.on_associated_subs_fps_changed = on_player_associated_subs_fps_changed;
    cbs.on_renderer_changed = on_player_renderer_changed;
    cbs.on_recording_changed = on_player_record_changed;
    cbs.on_signal_changed = on_player_signal_changed;
    cbs.on_statistics_changed = on_player_stats_changed;
    cbs.on_atobloop_changed = on_player_atobloop_changed;
    cbs.on_media_meta_changed = on_player_media_meta_changed;
    cbs.on_media_epg_changed = on_player_media_epg_changed;
    cbs.on_media_subitems_changed = on_player_subitems_changed;
    cbs.on_vout_changed = on_player_vout_changed;
    cbs.on_cork_changed = on_player_corks_changed;
    cbs.on_playback_restore_queried = on_player_playback_restore_queried;
    return cbs;
}();

static const vlc_player_vout_cbs player_vout_cbs = []{
    struct vlc_player_vout_cbs cbs{};
    cbs.on_fullscreen_changed = on_player_vout_fullscreen_changed;
    cbs.on_wallpaper_mode_changed = on_player_vout_wallpaper_mode_changed;
    return cbs;
}();

static const vlc_player_aout_cbs player_aout_cbs = []{
    struct vlc_player_aout_cbs cbs{};
    cbs.on_volume_changed = on_player_aout_volume_changed;
    cbs.on_mute_changed = on_player_aout_mute_changed;
    return cbs;
}();

static const vlc_player_timer_cbs player_timer_cbs = []{
    struct vlc_player_timer_cbs cbs {};
    cbs.on_update = on_player_timer_update;
    cbs.on_paused = on_player_timer_paused;
    cbs.on_seek = on_player_timer_seek;
    return cbs;
}();

static const struct vlc_player_timer_smpte_cbs player_timer_smpte_cbs = []{
    struct vlc_player_timer_smpte_cbs cbs {};
    cbs.on_update = on_player_timer_smpte_update;
    return cbs;
}();

// art fetcher callbacks

static const struct vlc_preparser_cbs art_fetcher_cbs = []{
    struct vlc_preparser_cbs cbs{};
    cbs.on_ended = on_preparse_ended_callback;
    return cbs;
}();


PlayerControllerPrivate::PlayerControllerPrivate(PlayerController *playercontroller, qt_intf_t *p_intf)
    : q_ptr(playercontroller)
    , p_intf(p_intf)
    , m_player(p_intf->p_player)
    , m_videoTracks(m_player)
    , m_audioTracks(m_player)
    , m_subtitleTracks(m_player)
    , m_titleList(m_player)
    , m_chapterList(m_player)
    , m_programList(m_player)
    , m_videoStereoMode((vout_thread_t*)nullptr, "video-stereo-mode")
    , m_zoom((vout_thread_t*)nullptr, "zoom")
    , m_aspectRatio((vout_thread_t*)nullptr, "aspect-ratio")
    , m_crop((vout_thread_t*)nullptr, "crop")
    , m_fit((vout_thread_t*)nullptr, "fit")
    , m_deinterlace((vout_thread_t*)nullptr, "deinterlace")
    , m_deinterlaceMode((vout_thread_t*)nullptr, "deinterlace-mode")
    , m_autoscale((vout_thread_t*)nullptr, "autoscale")
    , m_audioStereoMode((audio_output_t*)nullptr, "stereo-mode")
    , m_audioMixMode((audio_output_t*)nullptr, "mix-mode")
    , m_audioDeviceList(m_player)
    , m_audioVisualization((audio_output_t*)nullptr, "visual")
    , m_rendererManager(p_intf, m_player)
{
    {
        vlc_player_locker locker{m_player};
        m_player_listener = vlc_player_AddListener( m_player, &player_cbs, this );
        m_player_aout_listener = vlc_player_aout_AddListener( m_player, &player_aout_cbs, this );
        m_player_vout_listener = vlc_player_vout_AddListener( m_player, &player_vout_cbs, this );
        m_player_timer = vlc_player_AddTimer( m_player, VLC_TICK_FROM_MS(500), &player_timer_cbs, this );
    }

    QObject::connect( &m_autoscale, &QVLCBool::valueChanged, q_ptr, &PlayerController::autoscaleChanged );
    QObject::connect( &m_audioVisualization, &VLCVarChoiceModel::hasCurrentChanged, q_ptr, &PlayerController::hasAudioVisualizationChanged );

    m_time_timer.setSingleShot( true );
    m_time_timer.setTimerType( Qt::PreciseTimer );

    // Initialise fullscreen to match the player state
    m_fullscreen = vlc_player_vout_IsFullscreen( m_player );

    m_volume = vlc_player_aout_GetVolume( m_player );
}

void PlayerControllerPrivate::addSMPTETimer()
{
    assert( !m_player_timer_smpte );

    vlc_player_locker lock{ m_player };
    m_player_timer_smpte = vlc_player_AddSmpteTimer( m_player, &player_timer_smpte_cbs, this );
    assert( m_player_timer_smpte );
}

void PlayerControllerPrivate::removeSMPTETimer()
{
    assert( m_player_timer_smpte );

    vlc_player_locker lock{ m_player };
    vlc_player_RemoveTimer( m_player, m_player_timer_smpte );
    m_player_timer_smpte = nullptr;
}

namespace {

/* The subtitle options of the Subtitles tab: saved in the configuration (the
 * Profile) and applied to the player, whose variables the text renderer
 * inherits, or to the video outputs for the vertical offset */
class PlayerSubtitleOptions : public SubtitleAppearance::Backend
{
public:
    PlayerSubtitleOptions( qt_intf_t *intf, vlc_player_t *player )
        : m_intf( intf ), m_player( player )
    {
        m_saveTimer.setSingleShot( true );
        m_saveTimer.setInterval( 1000 );
        QObject::connect( &m_saveTimer, &QTimer::timeout, [this]() {
            config_SaveConfigFile( m_intf );
        } );
    }

    ~PlayerSubtitleOptions() override
    {
        if ( m_saveTimer.isActive() )
            config_SaveConfigFile( m_intf );
    }

    QVariant value( const char *option ) const override
    {
        const int type = config_GetType( option );

        /* the player's value may have changed since (a hotkey…) */
        if ( !strcmp( option, "sub-text-scale" ) )
        {
            vlc_player_locker lock{ m_player };
            return vlc_player_GetSubtitleTextScale( m_player );
        }
        if ( !strcmp( option, "sub-language" ) )
        {
            vlc_player_locker lock{ m_player };
            char *psz = vlc_player_GetCategoryLanguage( m_player, SPU_ES );
            QString language = qfu( psz );
            free( psz );
            return language;
        }
        vlc_value_t val;
        if ( vlc_player_GetSubtitleTextOption( m_player, option, &val ) == VLC_SUCCESS )
        {
            switch ( type )
            {
            case VLC_VAR_BOOL:
                return val.b_bool;
            case VLC_VAR_INTEGER:
                return QVariant::fromValue<qlonglong>( val.i_int );
            case VLC_VAR_STRING:
            {
                QString string = qfu( val.psz_string );
                free( val.psz_string );
                return string;
            }
            default:
                return {};
            }
        }

        switch ( type )
        {
        case VLC_VAR_BOOL:
            return config_GetInt( option ) != 0;
        case VLC_VAR_INTEGER:
            return QVariant::fromValue<qlonglong>( config_GetInt( option ) );
        case VLC_VAR_STRING:
        {
            char *psz = config_GetPsz( option );
            QString string = qfu( psz );
            free( psz );
            return string;
        }
        default:
            return {};
        }
    }

    void setValue( const char *option, const QVariant &value ) override
    {
        switch ( config_GetType( option ) )
        {
        case VLC_VAR_BOOL:
            config_PutInt( option, value.toBool() );
            break;
        case VLC_VAR_INTEGER:
            config_PutInt( option, value.toLongLong() );
            break;
        case VLC_VAR_STRING:
            config_PutPsz( option, value.toString().toUtf8().constData() );
            break;
        default:
            return;
        }
        m_saveTimer.start();
        apply( option, value );
    }

    void apply( const char *option, const QVariant &value ) override
    {
        const QByteArray utf8 = value.toString().toUtf8();
        vlc_value_t val{};
        switch ( config_GetType( option ) )
        {
        case VLC_VAR_BOOL:
            val.b_bool = value.toBool();
            break;
        case VLC_VAR_INTEGER:
            val.i_int = value.toLongLong();
            break;
        case VLC_VAR_STRING:
            val.psz_string = const_cast<char *>( utf8.constData() );
            break;
        default:
            return;
        }

        if ( !strcmp( option, "sub-text-scale" ) )
        {
            vlc_player_locker lock{ m_player };
            vlc_player_SetSubtitleTextScale( m_player, std::clamp<int64_t>( val.i_int, 10, 500 ) );
        }
        else if ( !strcmp( option, "sub-language" ) )
        {
            /* picks the subtitle track of the next media */
            vlc_player_locker lock{ m_player };
            vlc_player_SelectCategoryLanguage( m_player, SPU_ES, utf8.constData() );
        }
        else if ( !strncmp( option, "freetype-", strlen( "freetype-" ) ) )
            vlc_player_SetSubtitleTextOption( m_player, option, val );
        else if ( !strcmp( option, "sub-margin" ) )
        {
            size_t count;
            vout_thread_t **vouts = vlc_player_vout_HoldAll( m_player, &count );
            for ( size_t i = 0; vouts && i < count; ++i )
            {
                var_SetInteger( vouts[i], option, value.toLongLong() );
                vout_Release( vouts[i] );
            }
            free( vouts );
        }
    }

    void redraw() override
    {
        vlc_player_vout_RedrawSubtitles( m_player );
    }

    void reloadProfile() override
    {
        /* the player keeps its own copy of the options: give it the
         * configuration values again */
        static const char *const options[] = {
            "sub-language", "sub-text-scale", "sub-margin",
            "freetype-font", "freetype-opacity", "freetype-color",
            "freetype-background-opacity", "freetype-background-color",
            "freetype-outline-opacity", "freetype-outline-color",
            "freetype-outline-thickness", "freetype-shadow-opacity",
            "freetype-shadow-color", "freetype-pixel-size",
            "freetype-keep-styling",
        };
        for ( const char *option : options )
        {
            QVariant value;
            switch ( config_GetType( option ) )
            {
            case VLC_VAR_BOOL:
                value = config_GetInt( option ) != 0;
                break;
            case VLC_VAR_INTEGER:
                value = QVariant::fromValue<qlonglong>( config_GetInt( option ) );
                break;
            case VLC_VAR_STRING:
            {
                char *psz = config_GetPsz( option );
                value = qfu( psz );
                free( psz );
                break;
            }
            default:
                continue;
            }
            setValue( option, value );
        }
    }

private:
    qt_intf_t *m_intf;
    vlc_player_t *m_player;
    QTimer m_saveTimer;
};

}

PlayerController::PlayerController( qt_intf_t *_p_intf )
    : QObject(NULL)
    , d_ptr( new PlayerControllerPrivate(this, _p_intf) )
{
    d_ptr->m_subtitleAppearance = new SubtitleAppearance(
        std::make_unique<PlayerSubtitleOptions>( _p_intf, d_ptr->m_player ), this );

    connect( &d_ptr->m_position_timer, &QTimer::timeout, this, &PlayerController::updatePositionFromTimer );
    connect( &d_ptr->m_time_timer, &QTimer::timeout, this, &PlayerController::updateTimeFromTimer );

    // The waveform seek bar: peaks are cached in the user cache folder
    QString waveformCacheDir;
    if ( char *psz_cachedir = config_GetUserDir( VLC_CACHE_DIR ) )
    {
        waveformCacheDir = QDir( QString::fromUtf8( psz_cachedir ) ).filePath( QStringLiteral( "waveforms" ) );
        free( psz_cachedir );
    }
    d_ptr->m_waveform = new SeekWaveform( VLC_OBJECT(_p_intf), waveformCacheDir, this );
    // a video starts with no video track yet: wait for its tracks before decoding
    d_ptr->m_waveform->setStartDelay( 1000 );

    const auto updateWaveformMedia = [this]() {
        Q_D(PlayerController);
        d->m_waveform->setMedia( d->m_url, d->m_videoTracks.getCount() > 0 );
    };
    connect( this, &PlayerController::inputChanged, this, updateWaveformMedia );
    connect( &d_ptr->m_videoTracks, &TrackListModel::countChanged, this, updateWaveformMedia );

    // The subtitle timeline follows the shown subtitle
    d_ptr->m_subtitleTimeline = new SubtitleTimeline( VLC_OBJECT(_p_intf), this );
    const auto updateSubtitleTimeline = [this]() {
        Q_D(PlayerController);
        d->UpdateSubtitleTimelineTrack();
    };
    connect( &d_ptr->m_subtitleTracks, &TrackListModel::dataChanged, this, updateSubtitleTimeline );
    connect( &d_ptr->m_subtitleTracks, &TrackListModel::countChanged, this, updateSubtitleTimeline );
    connect( this, &PlayerController::hasSecondarySubtitleChanged, this, updateSubtitleTimeline );
    connect( this, &PlayerController::inputChanged, this, updateSubtitleTimeline );

    // The Video tab: its adjustments stay on the next video outputs
    d_ptr->m_videoAdjustments = new VideoAdjustments( std::make_unique<PlayerVideoOutputs>( this ), this );
    connect( this, &PlayerController::voutListChanged,
             d_ptr->m_videoAdjustments, &VideoAdjustments::outputsChanged );
    /* test hook for screenshots without input: "hue=120;negate=1" is set on
     * the first video output */
    if ( qEnvironmentVariableIsSet( "VLC_QT_TEST_VIDEO_ADJUSTMENTS" ) )
    {
        connect( this, &PlayerController::hasVideoOutputChanged, this, [this]( bool has ) {
            static bool applied = false;
            if ( !has || std::exchange( applied, true ) )
                return;
            const QString values = qEnvironmentVariable( "VLC_QT_TEST_VIDEO_ADJUSTMENTS" );
            for ( const QString& pair : values.split( ';', Qt::SkipEmptyParts ) )
            {
                const QString key = pair.section( '=', 0, 0 );
                const QString value = pair.section( '=', 1 );
                bool number;
                const double asNumber = value.toDouble( &number );
                d_ptr->m_videoAdjustments->setValue( key, number ? QVariant( asNumber ) : QVariant( value ) );
            }
        } );
    }

    // The statistics overlay
    d_ptr->m_statistics = new PlaybackStatistics( this );
    connect( this, &PlayerController::statisticsUpdated,
             d_ptr->m_statistics, &PlaybackStatistics::update );
    connect( this, &PlayerController::inputChanged,
             d_ptr->m_statistics, &PlaybackStatistics::reset );

    // The Audio tab
    d_ptr->m_audioAdjustments = new AudioAdjustments( d_ptr->m_player, this );

    // The sleep timer: it pauses after a duration; at the end of an item,
    // the player pauses by itself on the last frame
    d_ptr->m_sleepTimer = new SleepTimer( this );
    connect( d_ptr->m_sleepTimer, &SleepTimer::pauseRequested, this, [this]() {
        Q_D(PlayerController);
        vlc_player_locker lock{ d->m_player };
        vlc_player_Pause( d->m_player );
    } );
    connect( d_ptr->m_sleepTimer, &SleepTimer::pauseAtEndOfItemChanged, this, [this]() {
        Q_D(PlayerController);
        vlc_player_locker lock{ d->m_player };
        vlc_player_SetPlayAndPause( d->m_player, d->m_sleepTimer->pauseAtEndOfItem() );
    } );
    const auto updateSleepTimerItem = [this]() {
        Q_D(PlayerController);
        qint64 left = -1;
        if ( d->m_remainingTime.valid() && d->m_rate > 0.f )
            left = d->m_remainingTime.asMilliseconds() / d->m_rate;
        d->m_sleepTimer->setItemTimeLeft( left );
    };
    connect( this, &PlayerController::remainingTimeChanged, this, updateSleepTimerItem );
    connect( this, &PlayerController::rateChanged, this, updateSleepTimerItem );
    connect( this, &PlayerController::rateChanged, this, &PlayerController::updateSleepTimerQueue );
    connect( this, &PlayerController::playingStateChanged, this, [this]( PlayingState state ) {
        Q_D(PlayerController);
        d->m_sleepTimer->setPaused( state == PLAYING_STATE_PAUSED );
    } );
}

PlayerController::~PlayerController()
{
    // waits for a running decode, which uses the interface
    delete d_ptr->m_waveform;
    delete d_ptr->m_subtitleTimeline;
}

// PLAYBACK

vlc_player_t * PlayerController::getPlayer() const
{
    Q_D(const PlayerController);

    return d->m_player;
}

input_item_t *PlayerController::getInput() const
{
    Q_D(const PlayerController);
    vlc_player_locker locker{ d->m_player };
    return vlc_player_GetCurrentMedia( d->m_player );
}

bool PlayerController::isStarted() const
{
    Q_D(const PlayerController);
    vlc_player_locker locker{ d->m_player };
    return vlc_player_IsStarted( d->m_player );
}

bool PlayerController::hasInput() const
{
    return getInput();
}

void PlayerController::reverse()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "reverse");
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
    {
        float f_rate_ = vlc_player_GetRate( d->m_player );
        vlc_player_ChangeRate( d->m_player, -f_rate_ );
    }
}

void PlayerController::setRate( float new_rate )
{
    Q_D(PlayerController);

    if (qFuzzyCompare(d->m_rate, new_rate))
        return;

    emit adjustedByUser( QStringLiteral( "rate" ) );
    msg_Dbg( d->p_intf, "setRate %f", new_rate);
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
    {
        vlc_player_ChangeRate( d->m_player, new_rate );

        // Until the timer notifies the change, we need to still change
        // the rate, because player timer update may take a long time.
        // For position, it is supposed to be interpolated but for rate
        // the new value can be taken as is:
        d->m_rate = new_rate;
        emit rateChanged(d->m_rate);
    }
}

void PlayerController::slower()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "slower");
    emit adjustedByUser( QStringLiteral( "rate" ) );
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
        vlc_player_DecrementRate( d->m_player );
}

void PlayerController::faster()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "faster");
    emit adjustedByUser( QStringLiteral( "rate" ) );
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
        vlc_player_IncrementRate( d->m_player );
}

void PlayerController::littlefaster()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "littlefaster");
    emit adjustedByUser( QStringLiteral( "rate" ) );
    var_SetInteger( vlc_object_instance(d->p_intf), "key-action", ACTIONID_RATE_FASTER_FINE );
}

void PlayerController::littleslower()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "littleslower");
    emit adjustedByUser( QStringLiteral( "rate" ) );
    var_SetInteger( vlc_object_instance(d->p_intf), "key-action", ACTIONID_RATE_SLOWER_FINE );
}

void PlayerController::normalRate()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "normalRate");
    emit adjustedByUser( QStringLiteral( "rate" ) );
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
        vlc_player_ChangeRate( d->m_player, 1.0f );
}


void PlayerController::setTime(VLCTime new_time)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_SetTime( d->m_player, new_time.toVLCTick() );
}

void PlayerController::setPosition(double position)
{
    Q_D(PlayerController);
    if (qFuzzyCompare(d->m_position, position))
        return;

    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_SetPosition( d->m_player, position );
}

void PlayerController::jumpFwd()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "jumpFwd");
    int i_interval = var_InheritInteger( d->p_intf, "short-jump-size" );
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_JumpTime( d->m_player, vlc_tick_from_sec( i_interval ) );
}

void PlayerController::jumpBwd()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "jumpBwd");
    int i_interval = var_InheritInteger( d->p_intf, "short-jump-size" );
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_JumpTime( d->m_player, vlc_tick_from_sec( -i_interval ) );
}

void PlayerController::jumpSeconds( int seconds )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_JumpTime( d->m_player, vlc_tick_from_sec( seconds ) );
}

void PlayerController::jumpToTime(int seconds)
{
    if (seconds < 0)
        return;
    setTime(VLCTime(vlc_tick_from_sec(seconds)));
}

void PlayerController::jumpTime(VLCTime i_time)
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "jumpTime");
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_JumpTime( d->m_player, i_time.toVLCTick() );
}

void PlayerController::jumpToPos( double new_pos )
{
    Q_D(PlayerController);
    {
        vlc_player_locker lock{ d->m_player };
        if( !d->isCurrentItemSynced() )
            return;
        if( vlc_player_IsStarted( d->m_player ) )
            vlc_player_SetPosition( d->m_player, new_pos );
    }
    emit seekRequested( new_pos );
}

void PlayerController::scrubToPos( double new_pos )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_SetPositionFast( d->m_player, new_pos );
}

void PlayerController::frameNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_NextVideoFrame( d->m_player );
}

void PlayerController::framePrev()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_PreviousVideoFrame( d->m_player );
}

//TRACKS

void PlayerController::setAudioDelay(VLCDuration delay)
{
    Q_D(PlayerController);
    {
        vlc_player_locker lock{ d->m_player };
        if( !d->isCurrentItemSynced() )
            return;
        vlc_player_SetAudioDelay( d->m_player, delay.toVLCTick(), VLC_PLAYER_WHENCE_ABSOLUTE );
    }
    emit adjustedByUser( QStringLiteral( "audio-delay" ) );
}

/*Q_INVOKABLE*/void PlayerController::addAudioDelay(VLCDuration delay)
{
    Q_D(PlayerController);
    {
        vlc_player_locker lock{ d->m_player };
        vlc_player_SetAudioDelay( d->m_player, delay.toVLCTick(), VLC_PLAYER_WHENCE_RELATIVE );
    }
    emit audioDelayChanged(delay);
    emit adjustedByUser( QStringLiteral( "audio-delay" ) );
}

void PlayerController::setSubtitleDelay(VLCDuration delay)
{
    Q_D(PlayerController);
    {
        vlc_player_locker lock{ d->m_player };
        if(!d->isCurrentItemSynced() )
            return;
        vlc_player_SetSubtitleDelay( d->m_player, delay.toVLCTick(), VLC_PLAYER_WHENCE_ABSOLUTE );
    }
    emit adjustedByUser( QStringLiteral( "sub-delay" ) );
}

/*Q_INVOKABLE*/void PlayerController::addSubtitleDelay(VLCDuration delay)
{
    Q_D(PlayerController);
    {
        vlc_player_locker lock{ d->m_player };
        vlc_player_SetSubtitleDelay( d->m_player, delay.toVLCTick(), VLC_PLAYER_WHENCE_RELATIVE);
    }
    emit subtitleDelayChanged(delay);
    emit adjustedByUser( QStringLiteral( "sub-delay" ) );
}

void PlayerController::setSecondarySubtitleDelay(VLCDuration delay)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (d->m_secondarySpuEsId.get() != NULL)
        vlc_player_SetEsIdDelay(d->m_player, d->m_secondarySpuEsId.get(),
                                delay.toVLCTick(), VLC_PLAYER_WHENCE_ABSOLUTE);
}

bool PlayerController::hasSecondarySubtitle() const
{
    Q_D(const PlayerController);
    return d->m_secondarySpuEsId.get() != NULL;
}

/*Q_INVOKABLE*/void PlayerController::addSecondarySubtitleDelay(VLCDuration delay)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if (d->m_secondarySpuEsId.get() != NULL) {
        vlc_player_SetEsIdDelay(d->m_player, d->m_secondarySpuEsId.get(),
                                delay.toVLCTick(), VLC_PLAYER_WHENCE_RELATIVE);
        emit secondarySubtitleDelayChanged(delay);
    }
}

int PlayerController::getAudioDelayMS() const
{
    return getAudioDelay().toMilliseconds();
}

void PlayerController::setAudioDelayMS(int ms)
{
    setAudioDelay( VLCDuration::fromMS(ms) );
}

int PlayerController::getSubtitleDelayMS() const
{
    return getSubtitleDelay().toMilliseconds();
}

void PlayerController::setSubtitleDelayMS(int ms)
{
    setSubtitleDelay( VLCDuration::fromMS(ms) );
}

int PlayerController::getSecondarySubtitleDelayMS() const
{
    return getSecondarySubtitleDelay().toMilliseconds();
}

void PlayerController::setSecondarySubtitleDelayMS(int ms)
{
    setSecondarySubtitleDelay( VLCDuration::fromMS(ms) );
}

void PlayerController::setSubtitleFPS(float fps)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    vlc_player_SetAssociatedSubsFPS( d->m_player, fps );
}

double PlayerController::getSubtitleDurationFactor() const
{
    Q_D(const PlayerController);
    return d->m_subtitleDurationFactor;
}

void PlayerController::setSubtitleDurationFactor( double factor )
{
    Q_D(PlayerController);
    factor = std::clamp( factor, 0., 20. );
    if ( qFuzzyCompare( factor + 1., d->m_subtitleDurationFactor + 1. ) )
        return;
    d->m_subtitleDurationFactor = factor;

    /* the "subsdelay" sub filter multiplies the durations; its factor is
     * proxied by a variable of the video output */
    size_t count;
    vout_thread_t **vouts = vlc_player_vout_HoldAll( d->m_player, &count );
    for ( size_t i = 0; vouts && i < count; ++i )
    {
        vout_thread_t *vout = vouts[i];
        if ( var_Type( vout, "sub-filter" ) != 0 )
        {
            if ( var_Type( vout, "subsdelay-factor" ) == 0 )
                var_Create( vout, "subsdelay-factor",
                            VLC_VAR_FLOAT | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
            var_SetFloat( vout, "subsdelay-factor", factor );

            char *psz_chain = var_GetString( vout, "sub-filter" );
            QStringList chain = qfu( psz_chain ).split( ':', Qt::SkipEmptyParts );
            free( psz_chain );
            const bool present = chain.contains( QStringLiteral( "subsdelay" ) );
            if ( factor > 0 && !present )
                chain.append( QStringLiteral( "subsdelay" ) );
            else if ( factor <= 0 && present )
                chain.removeAll( QStringLiteral( "subsdelay" ) );
            if ( present != ( factor > 0 ) )
                var_SetString( vout, "sub-filter", qtu( chain.join( ':' ) ) );
        }
        vout_Release( vout );
    }
    free( vouts );

    emit subtitleDurationFactorChanged();
}

//TITLE/CHAPTER/MENU

void PlayerController::sectionPrev()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
    {
        if (vlc_player_GetSelectedChapter( d->m_player ) != NULL)
            vlc_player_SelectPrevChapter( d->m_player );
        else
            vlc_player_SelectPrevTitle( d->m_player );
    }
}

void PlayerController::sectionNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
    {
        if (vlc_player_GetSelectedChapter( d->m_player ) != NULL)
            vlc_player_SelectNextChapter( d->m_player );
        else
            vlc_player_SelectNextTitle( d->m_player );
    }
}

void PlayerController::sectionMenu()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_Navigate( d->m_player, VLC_PLAYER_NAV_MENU );
}

void PlayerController::navigateUp()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_Navigate( d->m_player, VLC_PLAYER_NAV_UP);
}

void PlayerController::navigateDown()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_Navigate( d->m_player, VLC_PLAYER_NAV_DOWN);
}

void PlayerController::navigateLeft()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_Navigate( d->m_player, VLC_PLAYER_NAV_LEFT);
}

void PlayerController::navigateRight()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_Navigate( d->m_player, VLC_PLAYER_NAV_RIGHT);
}

void PlayerController::navigateActivate()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_Navigate( d->m_player, VLC_PLAYER_NAV_ACTIVATE);
}

void PlayerController::chapterNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectNextChapter( d->m_player );
}

void PlayerController::chapterPrev()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectPrevChapter( d->m_player );
}

void PlayerController::titleNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectNextTitle( d->m_player );
}

void PlayerController::titlePrev()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectPrevTitle( d->m_player );
}

//PROGRAMS

void PlayerController::changeProgram( int program )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_SelectProgram( d->m_player, program );
}

//TELETEXT


void PlayerController::enableTeletext( bool enable )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SetTeletextEnabled( d->m_player, enable );
}

void PlayerController::setTeletextPage(int page)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (vlc_player_IsTeletextEnabled( d->m_player ))
        vlc_player_SelectTeletextPage( d->m_player, page );
}

void PlayerController::setTeletextTransparency( bool transparent )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !d->isCurrentItemSynced() )
        return;
    if (vlc_player_IsTeletextEnabled( d->m_player ))
        vlc_player_SetTeletextTransparency( d->m_player, transparent );
}

//VOUT PROPERTIES

PlayerController::VOutThreadList PlayerController::getVouts() const
{
    Q_D(const PlayerController);
    vout_thread_t **pp_vout;
    VOutThreadList VoutList;
    size_t i_vout;
    {
        vlc_player_locker lock{ d->m_player };
        if( !vlc_player_IsStarted( d->m_player ) )
            return VOutThreadList{};
        i_vout = 0;
        pp_vout = vlc_player_vout_HoldAll( d->m_player, &i_vout );
        if ( i_vout <= 0 )
            return VOutThreadList{};
    }
    VoutList.reserve( i_vout );
    for( size_t i = 0; i < i_vout; i++ )
    {
        assert( pp_vout[i] );
        //pass ownership
        VoutList.append(SharedVOutThread(pp_vout[i], false));
    }
    free( pp_vout );

    return VoutList;
}

SharedVOutThread PlayerController::getVout()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vout_thread_t* vout = vlc_player_vout_Hold( d->m_player );
    if( vout == NULL )
        return SharedVOutThread{};
    return SharedVOutThread{vout, false};
}

void PlayerController::setFullscreen( bool new_val )
{
    Q_D(PlayerController);
    msg_Dbg(d->p_intf, "setFullscreen %s", new_val? "fullscreen" : "windowed");
    vlc_player_locker lock{ d->m_player };
    vlc_player_vout_SetFullscreen( d->m_player, new_val );
}

void PlayerController::toggleFullscreen()
{
    Q_D(PlayerController);
    setFullscreen( ! d->m_fullscreen );
}

void PlayerController::setWallpaperMode( bool new_val )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_vout_SetWallpaperModeEnabled( d->m_player, new_val );
}

bool PlayerController::getAutoscale( ) const
{
    Q_D(const PlayerController);
    return d->m_autoscale.getValue();
}

void PlayerController::setAutoscale( bool new_val )
{
    Q_D(PlayerController);
    d->m_autoscale.setValue( new_val );
}

//AOUT PROPERTIES

SharedAOut PlayerController::getAout()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    return SharedAOut( vlc_player_aout_Hold( d->m_player ), false );
}

void PlayerController::setVolume(float volume)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_SetVolume( d->m_player, volume );
}

void PlayerController::setVolumeUp(int steps)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_IncrementVolume( d->m_player, steps, NULL );
}

void PlayerController::setVolumeDown(int steps)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_DecrementVolume( d->m_player, steps, NULL );
}

void PlayerController::setMuted(bool muted)
{
    Q_D(PlayerController);
    if( d->m_muted == muted )
        return;
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_Mute( d->m_player, muted );
}

void PlayerController::toggleMuted()
{
    Q_D(PlayerController);
    setMuted( !d->m_muted );
}

bool PlayerController::hasAudioVisualization() const
{
    Q_D(const PlayerController);
    return d->m_audioVisualization.hasCurrent();
}

void PlayerController::updatePosition()
{
    Q_D(PlayerController);

    // Update position properties
    emit positionChanged(d->m_position);
    emit positionUpdated(d->m_position, d->m_time, d->m_length);
}

void PlayerController::updatePositionFromTimer()
{
    Q_D(PlayerController);

    vlc_tick_t system_now = vlc_tick_now();
    if (d->interpolateTime(system_now) == VLC_SUCCESS)
        updatePosition();
}

void PlayerController::updateTime(vlc_tick_t system_now, bool forceUpdate)
{
    Q_D(PlayerController);

    // Update time properties
    emit timeChanged(d->m_time);
    if (d->m_time.valid() && d->m_length.valid() && d->m_time <= VLCTime(d->m_length))
        d->m_remainingTime = VLCTime(d->m_length) - d->m_time;
    else
        d->m_remainingTime = VLCDuration();
    emit remainingTimeChanged(d->m_remainingTime);

    if (d->m_syltRaw.has_value() && d->m_time.valid())
    {
        const auto& lyrics = d->syltLyrics();
        int currentIndex = -1;
        const vlc_tick_t currentTime = d->m_time.toVLCTick();

        for (int i = 0; i < lyrics.size(); ++i)
        {
            if ((lyrics[i].time().toVLCTick() - 1) > currentTime)
                break;
            currentIndex = i;
        }

        if (currentIndex != d->m_currentLyricIndex)
        {
            d->m_currentLyricIndex = currentIndex;
            emit currentLyricIndexChanged( currentIndex );
            emit currentLyricChanged();
        }
    }

    if (system_now != VLC_TICK_INVALID
     && d->m_player_time.system_date != VLC_TICK_MAX
     && (forceUpdate || !d->m_time_timer.isActive()))
    {
        // Tell the timer to wait until the next second is reached.
        vlc_tick_t next_update_date =
            vlc_player_timer_point_GetNextIntervalDate(&d->m_player_time, system_now,
                                                       d->m_time.toVLCTick(), VLC_TICK_FROM_SEC(1));

        vlc_tick_t next_update_interval = next_update_date - system_now;

        if (next_update_interval > 0)
        {
            // The timer can be triggered a little before. In that case, it's
            // likely that we didn't reach the next next second. It's better to
            // add a very small delay in order to be triggered after the next
            // seconds.
            static const unsigned imprecision_delay_ms = 30;

            d->m_time_timer.start(MS_FROM_VLC_TICK(next_update_interval)
                                  + imprecision_delay_ms);
        }
    }
}

void PlayerController::updateTimeFromTimer()
{
    Q_D(PlayerController);

    vlc_tick_t system_now = vlc_tick_now();
    if (d->interpolateTime(system_now) == VLC_SUCCESS)
        updateTime(system_now, false);
}

void PlayerController::restorePlaybackPos()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if (!d->isCurrentItemSynced())
        return;
    vlc_player_RestorePlaybackPos( d->m_player );
}

void PlayerController::acknowledgeRestoreCallback()
{
    Q_D(PlayerController);
    if (d->m_canRestorePlayback) {
        d->m_canRestorePlayback = false;
        emit playbackRestoreQueried();
    }
}

//MISC

void PlayerController::setABloopState(ABLoopState state)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_SetAtoBLoop( d->m_player, static_cast<vlc_player_abloop>(state));
}

void PlayerController::toggleABloopState()
{
    Q_D(PlayerController);
    switch (d->m_ABLoopState) {
    case ABLOOP_STATE_NONE:
        setABloopState(ABLOOP_STATE_A);
        break;
    case ABLOOP_STATE_A:
        setABloopState(ABLOOP_STATE_B);
        break;
    case ABLOOP_STATE_B:
        setABloopState(ABLOOP_STATE_NONE);
        break;
    }
}

void PlayerController::toggleRecord()
{
    Q_D(PlayerController);
    setRecording(!d->m_recording);
}

void PlayerController::toggleVisualization()
{
    Q_D(PlayerController);

    if ( d->m_audioVisualization.rowCount() < 1 )
        return;

    if ( !d->m_audioVisualization.hasCurrent()
         || d->m_audioVisualization.currentRow() == 0 /*0th row is "Disable"*/)
    {
        const int r = rand() % d->m_audioVisualization.rowCount();
        d->m_audioVisualization.setData( d->m_audioVisualization.index( r )
                                         , QVariant::fromValue<bool>( true )
                                         , Qt::CheckStateRole );
    }
    else
    {
        d->m_audioVisualization.setData( d->m_audioVisualization.index(0)
                                         , QVariant::fromValue<bool>( true )
                                         , Qt::CheckStateRole);
    }
}

void PlayerController::requestAddSMPTETimer()
{
    Q_D(PlayerController);

    if ( ++d->m_smpteTimerRequestCount == 1 )
    {
        d->addSMPTETimer();
    }
}

void PlayerController::requestRemoveSMPTETimer()
{
    Q_D(PlayerController);

    assert( d->m_smpteTimerRequestCount > 0 );
    if ( --d->m_smpteTimerRequestCount == 0 )
    {
        d->removeSMPTETimer();
    }
}

void PlayerController::setRecording( bool recording )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_SetRecordingEnabled( d->m_player, recording, NULL );
}

void PlayerController::snapshot()
{
    SharedVOutThread vout = getVout();
    if (vout)
    {
        /* Passing a lambda directly would require Qt 5.15:
         * <https://doc.qt.io/qt-5/qthreadpool.html#start-1>
         */
        struct SnapshotTask : public QRunnable
        {
            SharedVOutThread vout;
            SnapshotTask(SharedVOutThread vout) : vout(std::move(vout)) {}
            void run() override
            {
                var_TriggerCallback(vout.get(), "video-snapshot");
            }
        };

        QThreadPool::globalInstance()->start(new SnapshotTask(std::move(vout)));
    }
}

QString PlayerController::highResolutionTime() const
{
    Q_D(const PlayerController);

    // Sample:
    vlc_player_timer_smpte_timecode sample;
    {
#ifndef QT_HAS_LIBATOMIC
        QReadLocker lock(&d->m_highResolutionTimeLock);
#endif
        sample = d->m_highResolutionTime;
    }

    const vlc_player_timer_smpte_timecode& lastSample = d->m_highResolutionTimeSample.first;

    if (sample.frames == lastSample.frames &&
        sample.frame_resolution == lastSample.frame_resolution &&
        sample.seconds == lastSample.seconds &&
        sample.minutes == lastSample.minutes &&
        sample.hours == lastSample.hours &&
        sample.drop_frame == lastSample.drop_frame)
        return d->m_highResolutionTimeSample.second;

    d->m_highResolutionTimeSample.second = QString::asprintf("%02u:%02u:%02u%c%02u",
                                                             sample.hours,
                                                             sample.minutes,
                                                             sample.seconds,
                                                             sample.drop_frame ? '.' : ':',
                                                             sample.frames);
    d->m_highResolutionTimeSample.first = std::move(sample);

    return d->m_highResolutionTimeSample.second;
}

//OTHER


/* Playlist Control functions */

void PlayerController::requestArtUpdate( input_item_t *p_item )
{
    Q_D(PlayerController);

    if (d->m_preparser == nullptr)
    {
        vlc_tick_t default_timeout =
            VLC_TICK_FROM_MS(var_InheritInteger(d->p_intf, "preparse-timeout"));
        if (default_timeout < 0)
            default_timeout = 0;

        const struct vlc_preparser_cfg cfg = [default_timeout]{
            struct vlc_preparser_cfg cfg{};
            cfg.types = VLC_PREPARSER_TYPE_FETCHMETA_ALL;
            cfg.max_parser_threads = 1;
            cfg.timeout = default_timeout;
            cfg.external_process = false;
            return cfg;
        }();
        d->m_preparser = vlc_preparser_New(VLC_OBJECT(d->p_intf), &cfg);
        if (unlikely(d->m_preparser == nullptr))
            return;
    }

    int fetch_options = var_InheritBool( d->p_intf, "metadata-network-access" ) ?
            VLC_PREPARSER_TYPE_FETCHMETA_ALL :
            VLC_PREPARSER_TYPE_FETCHMETA_LOCAL;

    vlc_preparser_req *req =
        vlc_preparser_req_NewParse( d->m_preparser, p_item, fetch_options,
                                    &art_fetcher_cbs, d );
    if (unlikely(req == nullptr))
        return;

    /* This request is never cancelled from here, so submit it and drop the
       handle straight away: on success the running task owns it, on failure
       this release is the one that frees it. */
    vlc_preparser_Submit( d->m_preparser, req );
    vlc_preparser_req_Release( req );
}

void PlayerControllerPrivate::onArtFetchEnded(input_item_t *p_item, bool)
{
    Q_Q(PlayerController);

    vlc_player_locker lock{ m_player };
    bool b_current_item = (p_item == vlc_player_GetCurrentMedia( m_player ));
    /* No input will signal the cover art to update,
         * let's do it ourself */
    if ( b_current_item )
        UpdateArt( p_item );
    else
        emit q->artChanged( p_item );
}

const QString PlayerController::decodeArtURL( input_item_t *p_item )
{
    assert( p_item );

    char *psz_art = input_item_GetArtURL( p_item );
    if( psz_art )
    {
        char *psz = vlc_uri2path( psz_art );
        free( psz_art );
        psz_art = psz;
    }

#if 0
    /* Taglib seems to define a attachment://, It won't work yet */
    url = url.replace( "attachment://", "" );
#endif

    QString path = qfu( psz_art ? psz_art : "" );
    free( psz_art );
    return path;
}

void PlayerController::setArt( input_item_t *p_item, QString fileUrl )
{
    Q_D(PlayerController);
    if( hasInput() )
    {
        char *psz_cachedir = config_GetUserDir( VLC_CACHE_DIR );
        QString old_url = decodeArtURL( p_item );
        old_url = QDir( old_url ).canonicalPath();

        if( psz_cachedir != nullptr && old_url.startsWith( QString::fromUtf8( psz_cachedir ) ) )
            QFile( old_url ).remove(); /* Purge cached artwork */

        free( psz_cachedir );

        input_item_SetArtURL( p_item , fileUrl.toUtf8().constData() );
        d->UpdateArt( p_item );
    }
}

void PlayerController::refreshMediaMeta( input_item_t *p_item )
{
    if (p_item == nullptr)
        return;

    Q_D(PlayerController);

    const bool isCurrentItem = [d, p_item]() {
        vlc_player_locker lock{ d->m_player };
        return p_item == vlc_player_GetCurrentMedia( d->m_player );
    }();

    if (!isCurrentItem)
        return;

    d->UpdateName( p_item );
    d->UpdateArt( p_item );
    d->UpdateMeta( p_item );
    d->UpdateInfo( p_item );
}

bool PlayerController::associateSubtitleFile(const QString &uri)
{
    return AddAssociatedMedia(SPU_ES, uri, true, true, true) == VLC_SUCCESS;
}

int PlayerController::AddAssociatedMedia(es_format_category_e cat, const QString &uri, bool select, bool notify, bool check_ext)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    return vlc_player_AddAssociatedMedia( d->m_player, cat, qtu(uri), select, notify, check_ext );
}

#define QABSTRACTLIST_GETTER( type, fun, var ) \
    type* PlayerController::fun() \
    { \
        Q_D(PlayerController); \
        return &d->var; \
    }


QABSTRACTLIST_GETTER( TrackListModel, getVideoTracks, m_videoTracks)
QABSTRACTLIST_GETTER( TrackListModel, getAudioTracks, m_audioTracks)
QABSTRACTLIST_GETTER( TrackListModel, getSubtitleTracks, m_subtitleTracks)
QABSTRACTLIST_GETTER( TitleListModel, getTitles, m_titleList)
QABSTRACTLIST_GETTER( ChapterListModel,getChapters, m_chapterList)
QABSTRACTLIST_GETTER( AudioDeviceModel, getAudioDevices, m_audioDeviceList)
QABSTRACTLIST_GETTER( ProgramListModel, getPrograms, m_programList)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getVideoStereoMode, m_videoStereoMode)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getZoom, m_zoom)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getAspectRatio, m_aspectRatio)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getCrop, m_crop)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getFit, m_fit)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getDeinterlace, m_deinterlace)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getDeinterlaceMode, m_deinterlaceMode)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getAudioStereoMode, m_audioStereoMode)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getAudioMixMode, m_audioMixMode)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getAudioVisualizations, m_audioVisualization)
QABSTRACTLIST_GETTER( RendererManager, getRendererManager, m_rendererManager)

SeekWaveform* PlayerController::getWaveform()
{
    Q_D(PlayerController);
    return d->m_waveform;
}

SubtitleTimeline* PlayerController::getSubtitleTimeline()
{
    Q_D(PlayerController);
    return d->m_subtitleTimeline;
}

VideoAdjustments* PlayerController::getVideoAdjustments()
{
    Q_D(PlayerController);
    return d->m_videoAdjustments;
}

SleepTimer* PlayerController::getSleepTimer()
{
    Q_D(PlayerController);
    return d->m_sleepTimer;
}

SubtitleAppearance* PlayerController::getSubtitleAppearance()
{
    Q_D(PlayerController);
    return d->m_subtitleAppearance;
}

PlaybackStatistics* PlayerController::getStatistics()
{
    Q_D(PlayerController);
    return d->m_statistics;
}

AudioAdjustments* PlayerController::getAudioAdjustments()
{
    Q_D(PlayerController);
    return d->m_audioAdjustments;
}

void PlayerController::updateSleepTimerQueue()
{
    Q_D(PlayerController);

    vlc_playlist_t *playlist = d->p_intf->p_playlist;
    if ( !playlist )
        return;

    bool last = false;
    qint64 queueLeft = 0;
    vlc_playlist_Lock( playlist );
    {
        const ssize_t current = vlc_playlist_GetCurrentIndex( playlist );
        const size_t count = vlc_playlist_Count( playlist );
        /* in Queue order; with "repeat all" the Queue never ends, the timer
         * still pauses at its last item */
        last = current >= 0 && static_cast<size_t>( current ) + 1 >= count;
        for ( size_t i = current + 1; current >= 0 && i < count; ++i )
        {
            input_item_t *media = vlc_playlist_item_GetMedia( vlc_playlist_Get( playlist, i ) );
            const vlc_tick_t duration = input_item_GetDuration( media );
            if ( duration == INPUT_DURATION_UNSET || duration == INPUT_DURATION_INDEFINITE )
            {
                queueLeft = -1;
                break;
            }
            queueLeft += MS_FROM_VLC_TICK( duration );
        }
    }
    vlc_playlist_Unlock( playlist );
    if ( queueLeft > 0 && d->m_rate > 0.f )
        queueLeft /= d->m_rate;

    d->m_sleepTimer->setLastItem( last );
    d->m_sleepTimer->setQueueTimeLeft( queueLeft );
}

#undef QABSTRACTLIST_GETTER

#define PRIMITIVETYPE_GETTER( type, fun, var ) \
    type PlayerController::fun() const \
    { \
        Q_D(const PlayerController); \
        return d->var; \
    }


PRIMITIVETYPE_GETTER(PlayerController::PlayingState, getPlayingState, m_playing_status)
PRIMITIVETYPE_GETTER(QString, getName, m_name)
PRIMITIVETYPE_GETTER(VLCTime, getTime, m_time)
PRIMITIVETYPE_GETTER(VLCDuration, getRemainingTime, m_remainingTime)
PRIMITIVETYPE_GETTER(double, getPosition, m_position)
PRIMITIVETYPE_GETTER(VLCDuration, getLength, m_length)
PRIMITIVETYPE_GETTER(VLCDuration, getAudioDelay, m_audioDelay)
PRIMITIVETYPE_GETTER(VLCDuration, getSubtitleDelay, m_subtitleDelay)
PRIMITIVETYPE_GETTER(VLCDuration, getSecondarySubtitleDelay, m_secondarySubtitleDelay)
PRIMITIVETYPE_GETTER(bool, isSeekable, m_capabilities & VLC_PLAYER_CAP_SEEK)
PRIMITIVETYPE_GETTER(bool, isRewindable, m_capabilities & VLC_PLAYER_CAP_REWIND)
PRIMITIVETYPE_GETTER(bool, isPausable, m_capabilities & VLC_PLAYER_CAP_PAUSE)
PRIMITIVETYPE_GETTER(bool, isRateChangable, m_capabilities & VLC_PLAYER_CAP_CHANGE_RATE)
PRIMITIVETYPE_GETTER(bool, canRestorePlayback, m_canRestorePlayback);
PRIMITIVETYPE_GETTER(float, restorePlaybackPosition, m_restorePlaybackPosition);
PRIMITIVETYPE_GETTER(float, getSubtitleFPS, m_subtitleFPS)
PRIMITIVETYPE_GETTER(bool, hasVideoOutput, m_hasVideo)
PRIMITIVETYPE_GETTER(float, getBuffering, m_buffering)
PRIMITIVETYPE_GETTER(float, getVolume, m_volume)
PRIMITIVETYPE_GETTER(bool, isMuted, m_muted)
PRIMITIVETYPE_GETTER(bool, isFullscreen, m_fullscreen)
PRIMITIVETYPE_GETTER(bool, getWallpaperMode, m_wallpaperMode)
PRIMITIVETYPE_GETTER(float, getRate, m_rate)
PRIMITIVETYPE_GETTER(bool, hasTitles, m_hasTitles)
PRIMITIVETYPE_GETTER(bool, hasChapters, m_hasChapters)
PRIMITIVETYPE_GETTER(bool, hasMenu, m_hasMenu)
PRIMITIVETYPE_GETTER(bool, isMenu, m_isMenu)
PRIMITIVETYPE_GETTER(bool, isInteractive, m_isInteractive)
PRIMITIVETYPE_GETTER(bool, hasPrograms, m_hasPrograms)
PRIMITIVETYPE_GETTER(bool, isEncrypted, m_encrypted)
PRIMITIVETYPE_GETTER(bool, isRecording, m_recording)
PRIMITIVETYPE_GETTER(PlayerController::ABLoopState, getABloopState, m_ABLoopState)
PRIMITIVETYPE_GETTER(VLCTime, getABLoopA, m_ABLoopA)
PRIMITIVETYPE_GETTER(VLCTime, getABLoopB, m_ABLoopB)
PRIMITIVETYPE_GETTER(bool, isTeletextEnabled, m_teletextEnabled)
PRIMITIVETYPE_GETTER(bool, isTeletextAvailable, m_teletextAvailable)
PRIMITIVETYPE_GETTER(int, getTeletextPage, m_teletextPage)
PRIMITIVETYPE_GETTER(bool, getTeletextTransparency, m_teletextTransparent)
PRIMITIVETYPE_GETTER(QString, getTitle, m_title)
PRIMITIVETYPE_GETTER(QString, getArtist, m_artist)
PRIMITIVETYPE_GETTER(QString, getAlbum, m_album)
PRIMITIVETYPE_GETTER(QUrl, getArtwork, m_artwork)
PRIMITIVETYPE_GETTER(QUrl, getUrl, m_url)

TimedText PlayerController::getCurrentLyric() const
{
    Q_D(const PlayerController);
    if (d->m_currentLyricIndex < 0)
        return {};
    const auto& lyrics = d->syltLyrics();
    if (d->m_currentLyricIndex >= lyrics.size())
        return {};
    return lyrics[d->m_currentLyricIndex];
}

bool PlayerController::hasLyrics() const
{
    Q_D(const PlayerController);
    return d->m_syltRaw.has_value();
}

QList<TimedText> PlayerController::getSyltLyrics() const
{
    Q_D(const PlayerController);
    return d->syltLyrics();
}

int PlayerController::getCurrentLyricIndex() const
{
    Q_D(const PlayerController);
    return d->m_currentLyricIndex;
}

#undef PRIMITIVETYPE_GETTER
