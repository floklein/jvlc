/*****************************************************************************
 * media_properties_model.cpp : Properties & Metadata of one media
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "media_properties_model.hpp"
#include "dialogs/fingerprint/chromaprint.hpp"

#include <vlc_url.h>
#include <vlc_meta.h>
#include <vlc_input_item.h>
#include <vlc_fingerprinter.h>
#include <vlc_list.hpp>

#include <cmath>

namespace {

/* One minute of input bitrate, one sample per second */
constexpr int BITRATE_HISTORY_SIZE = 60;

QString takeString( char *psz )
{
    QString value = qfu( psz );
    free( psz );
    return value;
}

QVariantMap row( const QString& name, const QString& value )
{
    return QVariantMap{ { "name", name }, { "value", value } };
}

QVariantMap group( const QString& name, const QVariantList& rows )
{
    return QVariantMap{ { "name", name }, { "rows", rows } };
}

QString kibibytes( int64_t bytes )
{
    return qtr( "%1 KiB" ).arg( bytes / 1024 );
}

QString kilobits( double kbps )
{
    return qtr( "%1 kb/s" ).arg( std::lround( kbps ) );
}

}

MediaPropertiesModel::MediaPropertiesModel( qt_intf_t *_p_intf, bool live, QObject *parent )
    : QObject( parent )
    , p_intf( _p_intf )
    , m_live( live )
    , m_identifyRequest( nullptr, &fingerprint_request_Delete )
{
}

MediaPropertiesModel::~MediaPropertiesModel()
{
    /* Stop the fingerprinter before releasing its last request */
    m_chromaprint.reset();
}

void MediaPropertiesModel::setMedia( const SharedInputItem& item )
{
    m_item = item;
    m_location.clear();

    if ( m_item )
    {
        const QString uri = takeString( input_item_GetURI( m_item.get() ) );
        char *path = vlc_uri2path( qtu( uri ) );
        m_location = path ? qfu( path ) : uri;
        free( path );
    }

    resetIdentify();
    setDirty( false );
    loadTags();

    m_statistics.clear();
    m_bitrateHistory.clear();

    emit mediaChanged();
    emit metaChanged();
    emit statisticsChanged();
    refreshCodecInfo();
}

static bool isLocalFile( input_item_t *item )
{
    bool net;
    if ( input_item_GetType( item, &net ) != ITEM_TYPE_FILE )
        return false;

    char *uri = input_item_GetURI( item );
    char *path = uri ? vlc_uri2path( uri ) : nullptr;
    const bool local = ( path != nullptr );
    free( uri );
    free( path );
    return local;
}

bool MediaPropertiesModel::canSave() const
{
    return m_item && isLocalFile( m_item.get() );
}

bool MediaPropertiesModel::canIdentify() const
{
    if ( !m_item || !isLocalFile( m_item.get() ) )
        return false;

    /* Identify songs, not movies */
    {
        input_item_t *item = m_item.get();
        vlc_mutex_locker lock( &item->lock );
        const struct input_item_es *es;
        vlc_vector_foreach_ref( es, &item->es_vec )
            if ( es->es.i_cat == VIDEO_ES )
                return false;
    }

    return Chromaprint::isSupported();
}

void MediaPropertiesModel::loadTags()
{
    m_tags = Tags{};
    m_fallbackTitle.clear();

    input_item_t *item = m_item.get();
    if ( !item )
        return;

    m_tags.title = takeString( input_item_GetTitle( item ) );
    if ( m_tags.title.isEmpty() )
    {
        /* Show the name, but do not write it as a tag */
        m_fallbackTitle = takeString( input_item_GetName( item ) );
        m_tags.title = m_fallbackTitle;
    }
    m_tags.artist = takeString( input_item_GetArtist( item ) );
    m_tags.album = takeString( input_item_GetAlbum( item ) );
    m_tags.genre = takeString( input_item_GetGenre( item ) );
    m_tags.date = takeString( input_item_GetDate( item ) );
    m_tags.trackNumber = takeString( input_item_GetTrackNum( item ) );
    m_tags.trackTotal = takeString( input_item_GetTrackTotal( item ) );
    m_tags.description = takeString( input_item_GetDescription( item ) );

    const QString art = takeString( input_item_GetArtURL( item ) );
    if ( !art.isEmpty() )
        m_tags.artwork = QUrl( art );
}

void MediaPropertiesModel::setTag( QString Tags::*tag, const QString& value )
{
    if ( !m_item || m_tags.*tag == value )
        return;
    m_tags.*tag = value;
    setDirty( true );
    emit metaChanged();
}

void MediaPropertiesModel::setTitle( const QString& value ) { setTag( &Tags::title, value ); }
void MediaPropertiesModel::setArtist( const QString& value ) { setTag( &Tags::artist, value ); }
void MediaPropertiesModel::setAlbum( const QString& value ) { setTag( &Tags::album, value ); }
void MediaPropertiesModel::setGenre( const QString& value ) { setTag( &Tags::genre, value ); }
void MediaPropertiesModel::setDate( const QString& value ) { setTag( &Tags::date, value ); }
void MediaPropertiesModel::setTrackNumber( const QString& value ) { setTag( &Tags::trackNumber, value ); }
void MediaPropertiesModel::setTrackTotal( const QString& value ) { setTag( &Tags::trackTotal, value ); }
void MediaPropertiesModel::setDescription( const QString& value ) { setTag( &Tags::description, value ); }

void MediaPropertiesModel::setArtwork( const QUrl& value )
{
    if ( !m_item || m_tags.artwork == value )
        return;
    m_tags.artwork = value;
    setDirty( true );
    emit metaChanged();
}

void MediaPropertiesModel::setDirty( bool dirty )
{
    if ( m_dirty == dirty )
        return;
    m_dirty = dirty;
    emit dirtyChanged();
}

bool MediaPropertiesModel::save()
{
    if ( !canSave() )
    {
        emit saveFailed();
        return false;
    }

    input_item_t *item = m_item.get();

    if ( m_fallbackTitle.isEmpty() || m_tags.title != m_fallbackTitle )
        input_item_SetTitle( item, qtu( m_tags.title ) );
    input_item_SetArtist( item, qtu( m_tags.artist ) );
    input_item_SetAlbum( item, qtu( m_tags.album ) );
    input_item_SetGenre( item, qtu( m_tags.genre ) );
    input_item_SetDate( item, qtu( m_tags.date ) );
    input_item_SetTrackNum( item, qtu( m_tags.trackNumber ) );
    input_item_SetTrackTotal( item, qtu( m_tags.trackTotal ) );
    input_item_SetDescription( item, qtu( m_tags.description ) );
    if ( !m_tags.artwork.isEmpty() )
        input_item_SetArtURL( item, qtu( m_tags.artwork.toString( QUrl::FullyEncoded ) ) );

    if ( input_item_WriteMeta( VLC_OBJECT( p_intf ), item ) != VLC_SUCCESS )
    {
        emit saveFailed();
        return false;
    }

    setDirty( false );
    emit saved();
    return true;
}

void MediaPropertiesModel::revert()
{
    setDirty( false );
    loadTags();
    emit metaChanged();
}

void MediaPropertiesModel::refreshMeta()
{
    if ( m_dirty || !m_item )
        return;
    loadTags();
    emit metaChanged();
}

void MediaPropertiesModel::refreshCodecInfo()
{
    m_codecInfo.clear();

    if ( input_item_t *item = m_item.get() )
    {
        vlc_mutex_locker lock( &item->lock );

        for ( auto& cat : vlc::from( item->categories, &info_category_t::node ) )
        {
            if ( info_category_IsHidden( &cat ) )
                continue;

            QVariantList infos;
            for ( auto& info : vlc::from( cat.infos, &info_t::node ) )
                infos.append( row( qfu( info.psz_name ), qfu( info.psz_value ) ) );

            m_codecInfo.append( QVariantMap{ { "name", qfu( cat.psz_name ) },
                                             { "infos", infos } } );
        }
    }

    emit codecInfoChanged();
}

void MediaPropertiesModel::updateStatistics( const input_stats_t& stats )
{
    /* bitrates are in bytes per microsecond */
    const double inputBitrate = stats.f_input_bitrate * 8000.;
    const double demuxBitrate = stats.f_demux_bitrate * 8000.;

    m_statistics = {
        group( qtr( "Input" ), {
            row( qtr( "Media data size" ), kibibytes( stats.i_read_bytes ) ),
            row( qtr( "Input bitrate" ), kilobits( inputBitrate ) ),
            row( qtr( "Demuxed data size" ), kibibytes( stats.i_demux_read_bytes ) ),
            row( qtr( "Content bitrate" ), kilobits( demuxBitrate ) ),
            row( qtr( "Discarded (corrupted)" ), QString::number( stats.i_demux_corrupted ) ),
            row( qtr( "Dropped (discontinued)" ), QString::number( stats.i_demux_discontinuity ) ),
        } ),
        group( qtr( "Video" ), {
            row( qtr( "Decoded blocks" ), QString::number( stats.i_decoded_video ) ),
            row( qtr( "Displayed frames" ), QString::number( stats.i_displayed_pictures ) ),
            row( qtr( "Late frames" ), QString::number( stats.i_late_pictures ) ),
            row( qtr( "Lost frames" ), QString::number( stats.i_lost_pictures ) ),
        } ),
        group( qtr( "Audio" ), {
            row( qtr( "Decoded blocks" ), QString::number( stats.i_decoded_audio ) ),
            row( qtr( "Played buffers" ), QString::number( stats.i_played_abuffers ) ),
            row( qtr( "Lost buffers" ), QString::number( stats.i_lost_abuffers ) ),
        } ),
    };

    /* The input reports several times per second: keep the latest value of
     * each second */
    const vlc_tick_t now = vlc_tick_now();
    if ( !m_bitrateHistory.isEmpty() && now - m_lastHistoryTick < VLC_TICK_FROM_SEC( 1 ) )
        m_bitrateHistory.last() = inputBitrate;
    else
    {
        m_bitrateHistory.append( inputBitrate );
        m_lastHistoryTick = now;
    }
    while ( m_bitrateHistory.size() > BITRATE_HISTORY_SIZE )
        m_bitrateHistory.removeFirst();

    emit statisticsChanged();
}

void MediaPropertiesModel::setIdentifyState( IdentifyState state )
{
    if ( m_identifyState == state )
        return;
    m_identifyState = state;
    emit identifyStateChanged();
}

void MediaPropertiesModel::resetIdentify()
{
    m_identifyRequest.reset();
    m_identifyResults.clear();
    setIdentifyState( IdentifyIdle );
}

void MediaPropertiesModel::identify()
{
    if ( m_identifyState == IdentifySearching || !canIdentify() )
        return;

    resetIdentify();

    if ( !m_chromaprint )
    {
        m_chromaprint = std::make_unique<Chromaprint>( p_intf );
        connect( m_chromaprint.get(), &Chromaprint::finished,
                 this, &MediaPropertiesModel::onIdentifyResults );
    }

    setIdentifyState( IdentifySearching );
    if ( !m_chromaprint->enqueue( m_item.get() ) )
        setIdentifyState( IdentifyNotFound );
}

void MediaPropertiesModel::onIdentifyResults()
{
    fingerprint_request_t *request = m_chromaprint->fetchResults();
    if ( !request )
    {
        if ( m_identifyState == IdentifySearching )
            setIdentifyState( IdentifyNotFound );
        return;
    }

    std::unique_ptr<fingerprint_request_t, void (*)( fingerprint_request_t * )>
        owner( request, &fingerprint_request_Delete );

    /* The media changed while searching */
    if ( m_identifyState != IdentifySearching || request->p_item != m_item.get() )
        return;

    const size_t count = vlc_array_count( &request->results.metas_array );
    if ( count == 0 )
    {
        setIdentifyState( IdentifyNotFound );
        return;
    }

    m_identifyResults.clear();
    for ( size_t i = 0; i < count; ++i )
    {
        auto meta = static_cast<vlc_meta_t *>(
            vlc_array_item_at_index( &request->results.metas_array, i ) );
        const QString mbid = qfu( vlc_meta_GetExtra( meta, "musicbrainz-id" ) );
        QVariantMap result{
            { "title", qfu( vlc_meta_Get( meta, vlc_meta_Title ) ) },
            { "artist", qfu( vlc_meta_Get( meta, vlc_meta_Artist ) ) },
        };
        if ( !mbid.isEmpty() )
            result.insert( "url", QUrl( QStringLiteral( "https://mb.videolan.org/recording/%1" ).arg( mbid ) ) );
        m_identifyResults.append( result );
    }

    m_identifyRequest = std::move( owner );
    setIdentifyState( IdentifyFound );
}

void MediaPropertiesModel::applyIdentity( int index )
{
    if ( m_identifyState != IdentifyFound || !m_identifyRequest
         || index < 0 || index >= m_identifyResults.size() )
        return;

    m_chromaprint->apply( m_identifyRequest.get(), index );

    const QVariantMap result = m_identifyResults.at( index ).toMap();
    setTitle( result.value( "title" ).toString() );
    setArtist( result.value( "artist" ).toString() );

    resetIdentify();
}

void MediaPropertiesModel::discardIdentities()
{
    resetIdentify();
}
