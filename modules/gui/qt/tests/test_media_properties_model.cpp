/*****************************************************************************
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
#include "vlc_stub_modules.hpp"

#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include <QUrl>

#include <vlc_input_item.h>
#include <vlc_meta.h>
#include <vlc_fingerprinter.h>

#include "dialogs/mediainfo/media_properties_model.hpp"

namespace {

SharedInputItem newItem( const QString& uri, const char* name = "item" )
{
    return SharedInputItem( input_item_New( qtu( uri ), name ), false );
}

QString localUri( const char* fileName )
{
    return QUrl::fromLocalFile( QDir::temp().filePath( fileName ) ).toString( QUrl::FullyEncoded );
}

void addEs( input_item_t* item, es_format_category_e cat )
{
    struct input_item_es es;
    es_format_Init( &es.es, cat, 0 );
    es.id = nullptr;
    es.id_stable = false;
    /* vlc_vector_push() does not build as C++ */
    vlc_mutex_locker lock( &item->lock );
    input_item_es_vector& vec = item->es_vec;
    vec.data = static_cast<struct input_item_es*>(
        realloc( vec.data, ( vec.size + 1 ) * sizeof( *vec.data ) ) );
    QVERIFY( vec.data );
    vec.data[vec.size++] = es;
    vec.cap = vec.size;
}

vlc_meta_t* newResult( const char* title, const char* artist, const char* mbid )
{
    vlc_meta_t* meta = vlc_meta_New();
    vlc_meta_Set( meta, vlc_meta_Title, title );
    vlc_meta_Set( meta, vlc_meta_Artist, artist );
    vlc_meta_SetExtra( meta, "musicbrainz-id", mbid );
    return meta;
}

QVariantMap findRow( const QVariantList& groups, const QString& group, const QString& row )
{
    for ( const QVariant& g : groups )
    {
        const QVariantMap groupMap = g.toMap();
        if ( groupMap.value( "name" ).toString() != group )
            continue;
        for ( const QVariant& r : groupMap.value( "rows" ).toList() )
        {
            const QVariantMap rowMap = r.toMap();
            if ( rowMap.value( "name" ).toString() == row )
                return rowMap;
        }
    }
    return {};
}

}

class TestMediaPropertiesModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY( m_env->init() );
    }

    void cleanupTestCase()
    {
        m_env.reset();
    }

    void init()
    {
        m_env->metaWriterCalls = 0;
        m_env->metaWriterFile.clear();
        m_env->metaWriterTitle.clear();
        m_env->metaWriterResult = VLC_SUCCESS;
    }

    void testNoMedia()
    {
        MediaPropertiesModel model( m_env->intf );
        QVERIFY( !model.hasMedia() );
        QVERIFY( !model.canSave() );
        QVERIFY( !model.canIdentify() );
        QVERIFY( !model.isDirty() );
        QVERIFY( model.codecInfo().isEmpty() );
        QCOMPARE( model.title(), QString() );
        QVERIFY( !model.save() );
    }

    void testLoadsTags()
    {
        SharedInputItem item = newItem( localUri( "song.mp3" ), "song.mp3" );
        input_item_SetTitle( item.get(), "A Title" );
        input_item_SetArtist( item.get(), "An Artist" );
        input_item_SetAlbum( item.get(), "An Album" );
        input_item_SetGenre( item.get(), "Jazz" );
        input_item_SetDate( item.get(), "1999" );
        input_item_SetTrackNum( item.get(), "3" );
        input_item_SetTrackTotal( item.get(), "12" );
        input_item_SetDescription( item.get(), "Some notes" );
        input_item_SetArtURL( item.get(), "file:///art/cover.jpg" );

        MediaPropertiesModel model( m_env->intf );
        QSignalSpy mediaSpy( &model, &MediaPropertiesModel::mediaChanged );
        model.setMedia( item );

        QCOMPARE( mediaSpy.count(), 1 );
        QVERIFY( model.hasMedia() );
        QCOMPARE( model.title(), QStringLiteral( "A Title" ) );
        QCOMPARE( model.artist(), QStringLiteral( "An Artist" ) );
        QCOMPARE( model.album(), QStringLiteral( "An Album" ) );
        QCOMPARE( model.genre(), QStringLiteral( "Jazz" ) );
        QCOMPARE( model.date(), QStringLiteral( "1999" ) );
        QCOMPARE( model.trackNumber(), QStringLiteral( "3" ) );
        QCOMPARE( model.trackTotal(), QStringLiteral( "12" ) );
        QCOMPARE( model.description(), QStringLiteral( "Some notes" ) );
        QCOMPARE( model.artwork(), QUrl( "file:///art/cover.jpg" ) );
        QCOMPARE( model.location(), QDir::toNativeSeparators( QDir::temp().filePath( "song.mp3" ) ) );
        QVERIFY( !model.isDirty() );
    }

    void testTitleFallsBackToName()
    {
        SharedInputItem item = newItem( localUri( "untitled.mp3" ), "untitled.mp3" );
        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );
        QCOMPARE( model.title(), QStringLiteral( "untitled.mp3" ) );
    }

    void testEditMarksDirtyAndRevert()
    {
        SharedInputItem item = newItem( localUri( "song.mp3" ) );
        input_item_SetTitle( item.get(), "Original" );

        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );

        QSignalSpy dirtySpy( &model, &MediaPropertiesModel::dirtyChanged );
        QSignalSpy metaSpy( &model, &MediaPropertiesModel::metaChanged );

        model.setTitle( "Original" );
        QVERIFY( !model.isDirty() );
        QCOMPARE( dirtySpy.count(), 0 );

        model.setTitle( "Edited" );
        QVERIFY( model.isDirty() );
        QCOMPARE( dirtySpy.count(), 1 );
        QVERIFY( metaSpy.count() >= 1 );
        QCOMPARE( model.title(), QStringLiteral( "Edited" ) );

        /* the item itself is untouched until Save */
        char* title = input_item_GetTitle( item.get() );
        QCOMPARE( QString::fromUtf8( title ), QStringLiteral( "Original" ) );
        free( title );

        model.revert();
        QVERIFY( !model.isDirty() );
        QCOMPARE( model.title(), QStringLiteral( "Original" ) );
    }

    void testRefreshKeepsEdits()
    {
        SharedInputItem item = newItem( localUri( "song.mp3" ) );
        input_item_SetTitle( item.get(), "Original" );
        input_item_SetArtist( item.get(), "Artist" );

        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );

        input_item_SetArtist( item.get(), "New Artist" );
        model.refreshMeta();
        QCOMPARE( model.artist(), QStringLiteral( "New Artist" ) );

        model.setTitle( "Edited" );
        input_item_SetArtist( item.get(), "Newer Artist" );
        model.refreshMeta();
        QCOMPARE( model.title(), QStringLiteral( "Edited" ) );
        QCOMPARE( model.artist(), QStringLiteral( "New Artist" ) );
        QVERIFY( model.isDirty() );
    }

    void testSaveWritesTags()
    {
        SharedInputItem item = newItem( localUri( "song.mp3" ) );
        input_item_SetTitle( item.get(), "Original" );

        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );
        QVERIFY( model.canSave() );

        QSignalSpy savedSpy( &model, &MediaPropertiesModel::saved );

        model.setTitle( "Saved Title" );
        model.setArtist( "Saved Artist" );
        model.setTrackNumber( "4" );
        model.setArtwork( QUrl( "file:///art/new.png" ) );
        QVERIFY( model.save() );

        QCOMPARE( m_env->metaWriterCalls, 1 );
        QCOMPARE( m_env->metaWriterTitle, QStringLiteral( "Saved Title" ) );
        QCOMPARE( QDir::fromNativeSeparators( m_env->metaWriterFile ),
                  QDir::fromNativeSeparators( QDir::temp().filePath( "song.mp3" ) ) );
        QCOMPARE( savedSpy.count(), 1 );
        QVERIFY( !model.isDirty() );

        char* artist = input_item_GetArtist( item.get() );
        QCOMPARE( QString::fromUtf8( artist ), QStringLiteral( "Saved Artist" ) );
        free( artist );
        char* art = input_item_GetArtURL( item.get() );
        QCOMPARE( QString::fromUtf8( art ), QStringLiteral( "file:///art/new.png" ) );
        free( art );
    }

    void testSaveFailureKeepsEdits()
    {
        SharedInputItem item = newItem( localUri( "song.mp3" ) );

        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );

        QSignalSpy failedSpy( &model, &MediaPropertiesModel::saveFailed );
        m_env->metaWriterResult = VLC_EGENERIC;
        model.setTitle( "Edited" );
        QVERIFY( !model.save() );
        QCOMPARE( failedSpy.count(), 1 );
        QVERIFY( model.isDirty() );
    }

    void testRemoteMediaCannotBeSaved()
    {
        SharedInputItem item = newItem( "https://example.org/stream.mp3" );

        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );
        QVERIFY( model.hasMedia() );
        QVERIFY( !model.canSave() );
        QCOMPARE( model.location(), QStringLiteral( "https://example.org/stream.mp3" ) );

        model.setTitle( "Edited" );
        QVERIFY( !model.save() );
        QCOMPARE( m_env->metaWriterCalls, 0 );
    }

    void testCodecInfo()
    {
        SharedInputItem item = newItem( localUri( "song.mp3" ) );
        input_item_AddInfo( item.get(), "Stream 0", "Codec", "%s", "MPEG Audio" );
        input_item_AddInfo( item.get(), "Stream 0", "Channels", "%s", "Stereo" );
        input_item_AddInfo( item.get(), ".hidden", "Secret", "%s", "x" );
        input_item_AddInfo( item.get(), "Stream 1", "Type", "%s", "Video" );

        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );

        const QVariantList categories = model.codecInfo();
        QCOMPARE( categories.size(), 2 );

        const QVariantMap first = categories.at( 0 ).toMap();
        QCOMPARE( first.value( "name" ).toString(), QStringLiteral( "Stream 0" ) );
        const QVariantList infos = first.value( "infos" ).toList();
        QCOMPARE( infos.size(), 2 );
        QCOMPARE( infos.at( 0 ).toMap().value( "name" ).toString(), QStringLiteral( "Codec" ) );
        QCOMPARE( infos.at( 0 ).toMap().value( "value" ).toString(), QStringLiteral( "MPEG Audio" ) );
        QCOMPARE( infos.at( 1 ).toMap().value( "name" ).toString(), QStringLiteral( "Channels" ) );

        QCOMPARE( categories.at( 1 ).toMap().value( "name" ).toString(), QStringLiteral( "Stream 1" ) );

        QSignalSpy codecSpy( &model, &MediaPropertiesModel::codecInfoChanged );
        input_item_AddInfo( item.get(), "Stream 2", "Type", "%s", "Subtitle" );
        model.refreshCodecInfo();
        QCOMPARE( codecSpy.count(), 1 );
        QCOMPARE( model.codecInfo().size(), 3 );
    }

    void testStatistics()
    {
        MediaPropertiesModel model( m_env->intf, true );
        QVERIFY( model.isLive() );
        model.setMedia( newItem( localUri( "movie.mkv" ) ) );

        QSignalSpy statsSpy( &model, &MediaPropertiesModel::statisticsChanged );

        input_stats_t stats = {};
        stats.i_read_bytes = 2048;
        stats.f_input_bitrate = 0.5f; /* bytes per µs: 4000 kb/s */
        stats.i_displayed_pictures = 42;
        stats.i_lost_abuffers = 3;
        model.updateStatistics( stats );

        QCOMPARE( statsSpy.count(), 1 );
        const QVariantList groups = model.statistics();
        QCOMPARE( groups.size(), 3 );
        QCOMPARE( findRow( groups, "Input", "Media data size" ).value( "value" ).toString(), QStringLiteral( "2 KiB" ) );
        QCOMPARE( findRow( groups, "Input", "Input bitrate" ).value( "value" ).toString(), QStringLiteral( "4000 kb/s" ) );
        QCOMPARE( findRow( groups, "Video", "Displayed frames" ).value( "value" ).toString(), QStringLiteral( "42" ) );
        QCOMPARE( findRow( groups, "Audio", "Lost buffers" ).value( "value" ).toString(), QStringLiteral( "3" ) );

        QCOMPARE( model.bitrateHistory().size(), 1 );
        QCOMPARE( model.bitrateHistory().at( 0 ).toDouble(), 4000. );

        /* the history keeps one sample per second, the latest one */
        stats.f_input_bitrate = 0.25f;
        model.updateStatistics( stats );
        QCOMPARE( statsSpy.count(), 2 );
        QCOMPARE( model.bitrateHistory().size(), 1 );
        QCOMPARE( model.bitrateHistory().at( 0 ).toDouble(), 2000. );
    }

    void testCanIdentify()
    {
        MediaPropertiesModel model( m_env->intf );

        model.setMedia( newItem( localUri( "song.mp3" ) ) );
        QVERIFY( model.canIdentify() );

        model.setMedia( newItem( "https://example.org/song.mp3" ) );
        QVERIFY( !model.canIdentify() );

        SharedInputItem video = newItem( localUri( "movie.mkv" ) );
        addEs( video.get(), AUDIO_ES );
        addEs( video.get(), VIDEO_ES );
        model.setMedia( video );
        QVERIFY( !model.canIdentify() );

        SharedInputItem audio = newItem( localUri( "song.flac" ) );
        addEs( audio.get(), AUDIO_ES );
        model.setMedia( audio );
        QVERIFY( model.canIdentify() );
    }

    void testIdentifyAndApply()
    {
        SharedInputItem item = newItem( localUri( "song.mp3" ) );
        input_item_SetTitle( item.get(), "track01" );

        MediaPropertiesModel model( m_env->intf );
        model.setMedia( item );

        QSignalSpy stateSpy( &model, &MediaPropertiesModel::identifyStateChanged );
        model.identify();
        QCOMPARE( model.identifyState(), MediaPropertiesModel::IdentifySearching );
        QVERIFY( m_env->fingerprinter );
        QVERIFY( m_env->fingerprintRequest );
        QCOMPARE( m_env->fingerprintRequest->p_item, item.get() );

        vlc_array_append( &m_env->fingerprintRequest->results.metas_array,
                          newResult( "First Song", "First Artist", "mbid-1" ) );
        vlc_array_append( &m_env->fingerprintRequest->results.metas_array,
                          newResult( "Second Song", "Second Artist", "mbid-2" ) );
        var_TriggerCallback( m_env->fingerprinter, "results-available" );

        QTRY_COMPARE( model.identifyState(), MediaPropertiesModel::IdentifyFound );
        const QVariantList results = model.identifyResults();
        QCOMPARE( results.size(), 2 );
        QCOMPARE( results.at( 1 ).toMap().value( "title" ).toString(), QStringLiteral( "Second Song" ) );
        QCOMPARE( results.at( 1 ).toMap().value( "artist" ).toString(), QStringLiteral( "Second Artist" ) );
        QCOMPARE( results.at( 1 ).toMap().value( "url" ).toUrl(),
                  QUrl( "https://mb.videolan.org/recording/mbid-2" ) );

        model.applyIdentity( 1 );
        QCOMPARE( model.title(), QStringLiteral( "Second Song" ) );
        QCOMPARE( model.artist(), QStringLiteral( "Second Artist" ) );
        QVERIFY( model.isDirty() );
        QCOMPARE( model.identifyState(), MediaPropertiesModel::IdentifyIdle );
        QVERIFY( model.identifyResults().isEmpty() );
        QVERIFY( stateSpy.count() >= 3 );
    }

    void testIdentifyNotFound()
    {
        MediaPropertiesModel model( m_env->intf );
        model.setMedia( newItem( localUri( "noise.wav" ) ) );

        model.identify();
        QCOMPARE( model.identifyState(), MediaPropertiesModel::IdentifySearching );
        QVERIFY( m_env->fingerprintRequest );
        var_TriggerCallback( m_env->fingerprinter, "results-available" );

        QTRY_COMPARE( model.identifyState(), MediaPropertiesModel::IdentifyNotFound );
        QVERIFY( model.identifyResults().isEmpty() );
    }

    void testChangingMediaResetsState()
    {
        MediaPropertiesModel model( m_env->intf );
        model.setMedia( newItem( localUri( "a.mp3" ) ) );
        model.setTitle( "Edited" );
        QVERIFY( model.isDirty() );

        SharedInputItem other = newItem( localUri( "b.mp3" ) );
        input_item_SetTitle( other.get(), "Other" );
        model.setMedia( other );
        QVERIFY( !model.isDirty() );
        QCOMPARE( model.title(), QStringLiteral( "Other" ) );
        QCOMPARE( model.identifyState(), MediaPropertiesModel::IdentifyIdle );
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
};

QTEST_GUILESS_MAIN(TestMediaPropertiesModel)
#include "test_media_properties_model.moc"
