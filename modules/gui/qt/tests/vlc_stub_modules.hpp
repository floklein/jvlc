/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#ifndef QT_VLC_STUB_MODULES
#define QT_VLC_STUB_MODULES

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_services_discovery.h>
#include <vlc_renderer_discovery.h>
#include <vlc_interface.h>
#include <vlc_media_library.h>
#include <vlc_cxx_helpers.hpp>

#include <string>
#include <vector>

#include <QString>

#include "qt.hpp"

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
#define QCOMPARE_LT(a, b) QVERIFY((a) < (b))
#define QCOMPARE_GT(a, b) QVERIFY((a) > (b))
#define QCOMPARE_LE(a, b) QVERIFY((a) <= (b))
#define QCOMPARE_GE(a, b) QVERIFY((a) >= (b))
#endif

/**
 * This class allows to instanciate a libvlc instance with stubbed modules
 * the modules instance can be reteived and manipulated using their pointer
 * This allows testing models depending on vlc in a controlled environment
 * validity of the pointers depends of the module usage
 */
struct VLCTestingEnv
{
    VLCTestingEnv();
    ~VLCTestingEnv();

    bool init();

    //should be valid after init
    libvlc_instance_t* libvlc = nullptr;

    //pointer to the qt interface pointer
    //no qt related objects are defined in it (no MainCtx, no Compositor ...)
    //should be valid after init
    qt_intf_t* intf = nullptr;

    //render discovery instance (valid once created)
    vlc_renderer_discovery_t* renderDiscovery = nullptr;

    //if set to false no render discovery will be found
    bool renderDiscoveryProbeEnabled = true;

    /*
     * The stub media library keeps Playlists in memory. Media ids are free
     * numbers: an item named "ml:<id>" stands for media <id>. External media
     * (created from an MRL) get ids from 1000.
     */
    struct Playlist
    {
        int64_t id;
        std::string name;
        std::vector<int64_t> media;
    };

    // forget every Playlist and external media
    void clearMediaLibrary();
    // the Playlists, in creation order
    std::vector<Playlist> playlists() const;
    // create a Playlist directly in the store (sends the ADDED event)
    int64_t addPlaylist(const std::string& name, const std::vector<int64_t>& media);
    // the external subtitle files saved for the media of this MRL
    std::vector<std::string> subtitleMatches(const std::string& mediaMrl) const;

    /*
     * The play history of the stub media library, newest first: media <id>
     * last played at <lastPlayed> (seconds since the epoch)
     */
    struct HistoryEntry
    {
        int64_t id;
        int64_t lastPlayed;
    };

    // replace the history (sends HISTORY_CHANGED)
    void setHistory(const std::vector<HistoryEntry>& history);
    // how many times the media library history was cleared
    int historyClears() const;

    //meta writer: what the last input_item_WriteMeta() received
    int metaWriterCalls = 0;
    QString metaWriterFile;
    QString metaWriterTitle;
    int metaWriterResult = VLC_SUCCESS;

    //fingerprinter instance (valid once created) and its pending request,
    //fill its results then trigger "results-available" on the fingerprinter
    vlc_object_t* fingerprinter = nullptr;
    struct fingerprint_request_t* fingerprintRequest = nullptr;

    //audio output (valid once created); it starts with the devices
    //"stub-speakers" (Speakers) and "stub-headphones" (Headphones),
    //aoutDevice is the last device selected
    //(the "stubtracks://N" access demux plays N embedded subtitle tracks)
    audio_output_t* aout = nullptr;
    QString aoutDevice;

    //folder tree served by the medialibrary module: folders the user added
    //have no parent, their MRL is "file:///library/<name>/"; ids give the
    //insertion order
    struct FakeFolder {
        int64_t id;
        int64_t parentId; //0 for a folder the user added
        QString name;
    };
    struct FakeMedia {
        int64_t id;
        int64_t folderId;
        QString title;
        double progress = -1.;
        uint32_t playCount = 0;
        vlc_ml_media_type_t type = VLC_ML_MEDIA_TYPE_VIDEO;
        //album track of an audio media; artists and albums get their ids
        //from the order in which their names first appear in mlMedia
        QString artist = {};
        QString album = {};
        int trackNumber = 0;
        int discNumber = 0;
    };
    std::vector<FakeFolder> mlFolders;
    std::vector<FakeMedia> mlMedia;
};

#endif /* QT_VLC_STUB_MODULES */
