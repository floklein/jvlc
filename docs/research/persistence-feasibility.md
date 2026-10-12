# Persistence feasibility: presets, profiles and library state

Research for [#4](https://github.com/floklein/jvlc/issues/4) (map [#1](https://github.com/floklein/jvlc/issues/1)).

**Question.** What can VLC persist today (media library, per-media options, QSettings, vlcrc) for six redesign features? Classify each as **supported**, **supported with UI work**, or **needs new storage**.

**Sources.** This tree at `feat/redesign` (`ccca5c8c92`); line numbers below refer to that commit. Upstream libmedialibrary facts come from `videolan/medialibrary` (see [Upstream libmedialibrary](#upstream-libmedialibrary)).

## Answer

| Feature | Classification | Where it would live |
| --- | --- | --- |
| Per-show presets (subtitle/audio/video settings) | **Needs new storage** | Named presets plus a folder-to-preset map in QSettings (or a JSON file); apply through player APIs on media change. ML per-media metadata covers only part of the values and has no folder/show scope. |
| Profiles bundling layouts and settings | **Needs new storage** | Control-bar layouts already persist (QSettings). vlcrc is one file per user with no runtime switch, so a profile needs a snapshot of options stored by the UI and re-applied with `config_Put*`. |
| Saved subtitle-to-episode matches across a folder | **Supported with UI work** (plus a small loading hook) | ML stores external subtitle files per media (`IFile::Type::Subtitles`). Nothing in VLC reads them back at playback; a Qt-side or module-side hook must attach them. |
| Watched / unwatched state | **Supported with UI work** | ML `playcount`, `progress`, `last_played_date`; mark played/unplayed already in the context menu. No exact "watched first" sort (`PLAYCOUNT` approximates it). Clearing history also resets watched state. |
| Pinned sidebar items | **Supported with UI work** for ML entities; **needs new storage** for anything else or for manual order | ML favorites on media, playlists, folders, albums, artists, genres. Not on media groups, shows or non-indexed locations, and favorites have no order. |
| Recent media | **Supported** (UI only) | ML history (global/local/network), already modelled by `MLRecentMediaModel` and the "Open Recent" menu. Caveat: "clear recents" also resets play counts. |

## The four stores

### 1. Media library (SQLite, libmedialibrary)

The desktop launchers always enable it: `bin/winvlc.c:138` and `bin/vlc.c:219` pass `--media-library`, and the core option itself defaults to false (`src/libvlc-module.c:2179`, used at `src/libvlc.c:217`). Every feature below that leans on the ML therefore breaks on builds or setups where the ML failed to start; the UI already branches on `MainCtx.mediaLibraryAvailable` (`modules/gui/qt/maininterface/qml/MainDisplay.qml:110`).

What the VLC wrapper (`include/vlc_media_library.h`) exposes:

- **Per-media fields**: `i_playcount`, `f_progress`, `i_last_played_date`, `b_is_favorite` (`include/vlc_media_library.h:222-230`).
- **Per-media playback states**, as free strings (`include/vlc_media_library.h:668-696`, comment at 668-676: "All values are stored and returned as strings"; `NULL` unsets). Keys: rating, speed, title, chapter, program, video/audio/subtitle track, aspect ratio, zoom, crop, deinterlace, video filter, gain, audio delay, subtitle delay, and one `VLC_ML_PLAYBACK_STATE_APP_SPECIFIC` slot. They map one-to-one onto `IMedia::MetadataType` (`modules/misc/medialibrary/medialibrary.cpp:1685-1722`) and are written with `IMedia::setMetadata` / `unsetMetadata` (`medialibrary.cpp:1832-1842`).
- **External files per media**: `VLC_ML_MEDIA_ADD_EXTERNAL_MRL` with `VLC_ML_FILE_TYPE_SUBTITLE` (`include/vlc_media_library.h:639`), implemented with `IMedia::addExternalMrl(mrl, IFile::Type::Subtitles)` (`medialibrary.cpp:1917-1951`). Files come back in `vlc_ml_media_t::p_files` with type and MRL (`include/vlc_media_library.h:153-162`).
- **Played state**: `VLC_ML_MEDIA_SET_PLAYED` true calls `markAsPlayed()`, false calls `removeFromHistory()` (`medialibrary.cpp:1953-1963`).
- **Favorites**: media (`:642`), folder by MRL, artist, album, genre, playlist (`:660-664`); favorite listings for each (`:557-572`); `b_favorite_only` filter on any query (`:457`); `VLC_ML_EVENT_FAVORITES_CHANGED` (`:843`).
- **History**: `VLC_ML_LIST_HISTORY` by `vlc_ml_history_type_t` global/local/network (`:89-94`, `:500-505`), `VLC_ML_CLEAR_HISTORY` (`:622`), `VLC_ML_EVENT_HISTORY_CHANGED` (`:833`).
- **Labels**: `VLC_ML_MEDIA_ADD_LABEL` / `REMOVE_LABEL` (`:643-644`), `VLC_ML_LIST_MEDIA_LABELS` (`:538`). The wrapper has no "list media by label" query.
- **Groups and shows** are read-only in the wrapper: `vlc_ml_group_t` carries counts including `i_nb_seen` (`:249-271`), `vlc_ml_show_t` carries name/summary/artwork (`:129-139`). Neither has a settable metadata field or a favorite flag.
- **Sorting**: `vlc_ml_sorting_criteria_t` (`:424-443`) has `PLAYCOUNT` but no last-played or played-state criterion.

How the player uses it (`src/player/medialib.c`):

- On stop, `vlc_player_UpdateMLStates` (`:171`) creates an external ML media for any unknown MRL (`:190`), so every played file gets a row, then saves progress when `save-recentplay` is on (`:205`) and the state bundle `vlc_ml_playback_states_all` (`:289`).
- That bundle (`include/vlc_media_library.h:698-710`) holds rate, zoom, title, video/audio/subtitle track ids, aspect ratio, crop, deinterlace and video filter. **It has no subtitle delay, audio delay or gain**, so the player never saves them even though the ML has keys for them.
- On start, `vlc_player_input_RestoreMlStates` (`:30`) restores **only for video media** (`:50`) and only applies states when `restore-playback-states` is on, which defaults to false (`src/libvlc-module.c:1932`).
- Track ids are per-file ES string ids (`:83-95`), not languages, so they do not carry across episodes.

### 2. Per-media input options (`input_item_t`)

`input_item_t` holds `ppsz_options` and `pp_slaves` (`include/vlc_input_item.h:101-125`), added with `input_item_AddOption` (`:254`) and `input_item_AddSlave` (`:281`; `SLAVE_PRIORITY_USER` at `:159`). These live in memory only. The one path to disk is playlist export: XSPF writes each option as `<vlc:option>` (`modules/misc/playlist/xspf.c:129-149`) and reads it back (`modules/demux/playlist/xspf.c:546`, `:615`). ML playlists store media ids, not options. Input options are therefore not a persistence layer for the redesign.

### 3. QSettings (`vlc-qt-interface`)

One file per user: INI on Windows, native format elsewhere (`modules/gui/qt/qt.cpp:973-979`). QML can read and write arbitrary keys through `MainCtx.settingValue` / `setSettingValue` (`modules/gui/qt/maininterface/mainctx.hpp:391-392`, `mainctx.cpp:1054-1062`); `util/qml/ModelSortSettingHandler.qml:40-55` already persists per-view sort this way. Existing groups include `MainWindow/*` panel and view state (`mainctx.cpp:318-340`) and the control-bar profiles.

**Control-bar profiles** (`modules/gui/qt/dialogs/toolbar/controlbar_profile_model.cpp`): group `ControlbarProfileModel`, array `Profiles` of `Name` + serialized `Model`, plus `SelectedProfile` (`:30-33`, `save()` at `:696-750`, `reload()` at `:752-847`). A profile here is only a control-bar layout: three button lists per bar identifier.

### 4. vlcrc (core configuration)

A single file, `<user config dir>/vlcrc`, unless `--config` points elsewhere (`src/config/file.c:55-69`, `CONFIG_FILE` at `src/config/configuration.h:83`). `--config` is volatile and read at startup only (`src/libvlc-module.c:2878`). Saving rewrites the whole file atomically (`src/config/file.c:337-360`); `config_LoadConfigFile` is internal (`src/config/configuration.h:62`), so a running VLC cannot load a second vlcrc. Changing settings at runtime means `config_Put*` per key plus `config_SaveConfigFile` (`include/vlc_configuration.h:231`).

## Per-feature detail

### Per-show presets of subtitle/audio/video settings — needs new storage

The audit wants a named preset holding subtitle delay, font/size/outline/position, default subtitle and audio track or language, audio delay, filters, video adjustments, selectable for a show.

- **No show-level slot.** Playback states are per media only. `vlc_ml_group_t` and `vlc_ml_show_t` cannot store anything (`include/vlc_media_library.h:129-139`, `:249-271`), and folders only carry favorite/banned/public flags (`:383-396`).
- **Partial value coverage.** Per-media keys exist for tracks, rate, zoom, crop, aspect ratio, deinterlace, video filter, gain, audio delay and subtitle delay (`:677-696`). There are none for subtitle font, size, outline, position, equalizer bands or video-adjust values. These are vlcrc options read when the filter is created, such as `freetype-font` and `freetype-outline-thickness` (`modules/text_renderer/freetype/freetype.c:141`, `:176`, `:720-728`), `sub-text-scale` (`src/libvlc-module.c:1830`) and `equalizer-preset` / `equalizer-bands` (`modules/audio_filter/equalizer.c:84-93`).
- **Track by language is global.** `sub-language` and `audio-language` are vlcrc strings (`src/libvlc-module.c:1867-1870`); at runtime `vlc_player_SelectCategoryLanguage` (`include/vlc_player.h:1818`) applies a language to the current media.
- **Apply path exists.** `vlc_player_SetSubtitleDelay` (`include/vlc_player.h:2090`), `vlc_player_SetAudioDelay` (`:2050`), `vlc_player_SetSubtitleTextScale` (`:1954`), `vlc_player_SelectCategoryLanguage` and the vout/aout variables cover most values.

**Recommendation.** Store presets as a QSettings group (for example `Presets/<id>/<key>`) plus a map from folder MRL (or ML group id) to preset id. On media change, look up the media's folder and apply the preset through player APIs. The ML `APP_SPECIFIC` slot holds one string per media (upstream `include/medialibrary/IMedia.h:66-102`). It could tag a single media with a preset id, but it cannot express "this whole folder" and is lost if the media row is re-created, so treat it as a fallback at most.

### Profiles bundling layouts and settings — needs new storage

- Layout half: control-bar profiles already persist and switch at runtime (`SelectedProfile`). The redesign's broader layout (panels, top bar) would need more keys in the same QSettings file.
- Settings half: vlcrc is a single global file with no profile concept and no runtime reload. A profile must snapshot the option values it owns (in QSettings or one file per profile), then on switch call `config_Put*` for each key and save. Options read only at startup (modules, output devices, decoders) take effect after a restart, so the profile UI has to say which ones do.

### Saved subtitle-to-episode matches across a folder — supported with UI work

- **Storage exists.** `vlc_ml_media_add_external_mrl(ml, mediaId, subMrl, VLC_ML_FILE_TYPE_SUBTITLE)` persists the link (`include/vlc_media_library.h:639`, `medialibrary.cpp:1917-1951`). Episodes not yet in the ML can get a row with `vlc_ml_new_external_media` (`:625`), the same call the player makes for unknown files (`src/player/medialib.c:190`).
- **Nothing reads it at playback.** `MediaToInputItem` builds the input item from the main file only and adds no slaves (`modules/misc/medialibrary/entities.cpp:545-600`). The only in-tree reader of ML subtitle files is the UPnP server (`modules/control/upnp_server/cds/Item.cpp:121-122`). The Qt "add subtitle" action is session-only (`modules/gui/qt/player/player_controller.cpp:2224-2234`, `vlc_player_AddAssociatedMedia`).
- **Loading hook needed.** Either add `input_item_AddSlave(..., SLAVE_PRIORITY_USER)` for each subtitle file in `MediaToInputItem`, which covers items launched from the library, or have the Qt player controller, on current-media change, look the MRL up in the ML and call `AddAssociatedMedia` for each subtitle file, which covers every launch path. Both are small and use existing APIs.
- **One link per subtitle file.** Upstream refuses an MRL that is already attached as an external file to another media (`src/File.cpp:495-501`). That is fine for one-to-one episode matching, but the UI must unlink before re-matching. Subtitles that libVLC detects during discovery are linked automatically (`src/metadata_services/LinkService.cpp:151-182`); those would be found again at playback anyway.
- **Folder-wide matching** is UI logic. The engine's auto-match is per file at open time, governed by `sub-autodetect-fuzzy` (`src/input/subtitles.c:220-322`, `src/libvlc-module.c:1819`), and its result is not stored.

### Watched / unwatched state — supported with UI work

- `playcount`, `progress` and `last_played_date` are on every ML media (`include/vlc_media_library.h:222-224`). The player writes progress on stop (`src/player/medialib.c:205`).
- Qt already derives "new" as `playCount == 0 && progress <= 0` (`modules/gui/qt/medialibrary/mlvideo.cpp:81`) and offers mark played/unplayed in `MLContextMenu.qml:180-184`, implemented by `MLVideoModel::setItemPlayed` → `vlc_ml_media_set_played` (`mlvideomodel.cpp:30-48`).
- Groups expose `i_nb_seen` for "3 of 10 watched" (`include/vlc_media_library.h:264-265`).
- "Mark unplayed" is a full reset: `removeFromHistory()` zeroes play count and progress and drops the item from history (upstream `src/Media.cpp:497-514`).
- **Gap:** there is no "watched first / unwatched first" sort. `VLC_ML_SORTING_PLAYCOUNT` (`:439`) comes close, but partially watched media (playcount 0, progress > 0) sort together with unwatched ones. An exact sort needs either a new criterion in libmedialibrary and the wrapper, or client-side sorting in the Qt model.
- **Coupling:** clearing recent history also wipes watched state (see [Recent media](#recent-media--supported)).

### Pinned sidebar items — supported with UI work (ML entities), needs new storage (the rest)

- ML favorites cover media (including streams), playlists, folders, albums, artists and genres (`include/vlc_media_library.h:557-572`, `:642`, `:660-664`), with change events (`:843`). The Home page already shows favorite media (`modules/gui/qt/medialibrary/qml/HomePage.qml:313-322`).
- **Limits:**
  - Folder favorites need the folder to be in the ML index, since `m_ml->folder(mrl)` returns null otherwise (`medialibrary.cpp:790-797`).
  - Media groups have a favorite flag upstream, but VLC's wrapper has no setter for it, so using it means a small wrapper addition. Shows have no favorite flag at all.
  - Favorites are a boolean with no user order.
  - Qt has no folder-favorite UI yet.
- The sidebar itself is a fixed QML array (`modules/gui/qt/maininterface/qml/MainDisplay.qml:107-129`).

**Recommendation.** If pins must be ordered, or must include network shares, non-indexed folders or media groups, store an ordered list of `{type, id-or-MRL}` in QSettings. ML favorites can back the "is pinned" flag for ML entities so other views stay consistent.

### Recent media — supported

ML history is recorded on playback (`src/player/medialib.c:205`, gated by `save-recentplay`, `src/libvlc-module.c:1926`), listed by type (`include/vlc_media_library.h:500-505`) and cleared by type (`:622`). Qt has `MLRecentMediaModel` on global history (`modules/gui/qt/medialibrary/mlrecentmediamodel.cpp:53-100`), `MLRecentVideoModel` on the Home page (`HomePage.qml:233`), and the "Open Recent" menu (`modules/gui/qt/menus/menus.cpp:201`). A recent-files sidebar tab is pure UI work. Recent *folders* or *playlists* are not tracked.

**Caveat:** clearing history resets play count and position for every media in scope (upstream `src/Media.cpp:3066-3112`). Qt's "Clear" button calls this for global history (`modules/gui/qt/medialibrary/mlrecentmediamodel.cpp:53-59`), so in the redesign "clear recents" would also mark everything unwatched. Avoiding that needs either per-item removal from recents (which still resets that item) or a separate recents list in QSettings.

## Upstream libmedialibrary

This tree pins **0.13.2** in contrib (`contrib/src/medialibrary/rules.mak:1`) and requires `>= 0.13.0` (`configure.ac:5097`, `modules/misc/meson.build:52`). That is DB model 38. Upstream facts below were read at `videolan/medialibrary` master `176ffe39` (model 40, 0.15.0-dev); everything cited also exists in 0.13.x unless noted. Paths are relative to `https://code.videolan.org/videolan/medialibrary/-/blob/176ffe39efa818cb08606cb299556b313b4fde19/`.

- **Metadata store.**
  - `IMedia::MetadataType` has 17 fixed values; `ApplicationSpecific = 250` is the only app-defined slot (`include/medialibrary/IMedia.h:66-102`).
  - The `Metadata` table is keyed `(id_media, entity_type, type)` with a `TEXT` value. The only entity type is `Media` (`src/Metadata.cpp:220-227`, `include/medialibrary/IMetadata.h:34-37`).
  - There is no arbitrary-key API. Casting other integers to `MetadataType` is unsafe because `init()` pre-sizes the vector to `NbMeta` for thread safety (`src/Metadata.cpp:93-97`).
  - Metadata is keyed by media id, not MRL, so it is lost if a media row is deleted and re-created.
- **Played state.**
  - `markAsPlayed()` sets `last_played_date = now` and `play_count + 1`, leaving the position alone (`src/Media.cpp:466-480`).
  - `removeFromHistory()` resets `play_count = 0`, `last_position = -1`, `last_time = -1` and `last_played_date = NULL` (`src/Media.cpp:497-514`).
  - `setLastPosition` counts a play once the position passes `1 - margin`, where the margin is 1-5% depending on duration (`src/Media.cpp:335-428`).
  - There is no played-state filter. `QueryParameters` only has sort, desc, includeMissing, publicOnly and favoriteOnly (`include/medialibrary/IMediaLibrary.h:109-128`).
- **Sorting.**
  - Upstream `SortingCriteria` includes `LastPlaybackDate` and `Rating`. `Rating` arrived in 0.14.0 (`include/medialibrary/IMediaLibrary.h:72-106`).
  - VLC's wrapper does not expose `LastPlaybackDate`, and upstream has no played-state criterion either.
- **History couples to watched state.**
  - `clearHistory(type)` resets play count and position for every media in scope, not just the recents list (`src/Media.cpp:3066-3112`).
  - History is defined as `last_played_date IS NOT NULL` (`src/Media.cpp:2862-2897`).
- **Favorites.**
  - Upstream supports favorites on media, folders, playlists, media groups, albums, artists and genres. Model 38 completed the set, which makes it available from 0.13.0 (`src/database/migrations/migration37-38.sql:9-11`).
  - VLC's wrapper has no setter for media groups.
  - Shows, episodes, labels and bookmarks have no favorite flag.
- **Folders, groups, shows.**
  - `IFolder` stores no app data, only public, favorite and banned flags (`src/Folder.cpp:211-224`).
  - `IMediaGroup`, `IShow` and `IShowEpisode` have no metadata setter (`include/medialibrary/IMediaGroup.h:39-198`, `IShow.h:38-49`).
- **Subtitles.**
  - `addExternalMrl(mrl, Subtitles)` inserts a `File` row with `is_external = 1` and needs no filesystem access (`src/Media.cpp:1252-1269`, `src/File.cpp:505-508`).
  - It refuses an MRL that is already attached as an external file elsewhere (`src/File.cpp:495-501`), so one subtitle file links to one media.
  - Discovery auto-links subtitles that libVLC reports as slaves of a media inside a discovered root (`src/filesystem/libvlc/Directory.cpp:140-151`, `src/metadata_services/LinkService.cpp:151-182`).
- **Labels.**
  - `createLabel`, `IMedia::addLabel` and `ILabel::media()` work as media-only tags (`include/medialibrary/IMediaLibrary.h:579-593`, `src/Label.cpp:68-75`).
  - There is no "list all labels" call, so the app must track label names itself.
- **No global key/value table.** The `Settings` table holds only internal values (`src/Settings.cpp:148-155`).

## Open questions surfaced

- **Preset scope key.** Should a preset bind to a folder MRL, an ML media group, an ML show, or a playlist? Each has different stability: folders move, groups are heuristic, shows need metadata.
- **Live apply of subtitle style.** `freetype-*` options are read when the text renderer is created (`freetype.c:720-728`). Changing font or outline mid-playback, as the sidebar implies, may need the renderer recreated. That is an engine-feasibility question, not a persistence one.
- **Restoring per-media states by default.** `restore-playback-states` is off by default and audio is never restored (`src/player/medialib.c:50`). Should the redesign turn it on, and how does it interact with presets (which wins)?
- **Delays and gain are not saved per media** even though ML keys exist. Should the redesign wire them into `vlc_player_UpdateMLStates`? That is a small core change.
- **Clear recents vs. watched.** Is it acceptable that clearing recents marks everything unwatched (ML behaviour), or do recents need their own store?
