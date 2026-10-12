# Folder browsing and "shows" in the media library

Research for [#6](https://github.com/floklein/jvlc/issues/6), part of map [#1](https://github.com/floklein/jvlc/issues/1).

**Question.** What do VLC's media library and the Qt ML models support for folder-based browsing: listing indexed folders and their media, moving up and down folders with history, grouping a folder as a show or series, sorting by watched state, and adding a folder to the library from the UI? What would be new?

**Sources.** This fork at `feat/redesign` (paths below are repo-relative). Upstream libmedialibrary at tag `0.13.2`, the version VLC pins in `contrib/src/medialibrary/rules.mak:1` (minimum `0.13.0` in `configure.ac:5097`), cited as `ml@0.13.2:<path>:<line>`. I also compared the public headers against upstream `master` (commit `176ffe39`, 2026-10-09, after tag `0.14.1`): `IFolder.h` and `IShow.h` are unchanged, and nothing touching folders, shows or watched state was added.

## Short answer

The engine already has almost everything a folder-first library needs: indexed roots, a folder tree, per-folder media, per-media progress and play count, and add/remove/ban folder. The Qt UI uses little of it. The library shows one flat list of folders that contain video, one level deep. There is no folder tree, no Shows view and no watched-state sort, and you can add a folder only from the Browse view header, Preferences or the first-run wizard.

VLC's notion of a "show" is not a folder. libmedialibrary creates a Show only when a file name matches `SxxEyy` or `NxNN`, and names it after the text before that match. The redesign's "show" (the folder name shown above the player) has to be modelled as an ML **folder**, not an ML **Show**.

| Capability | Status | Where it lives / what is missing |
| --- | --- | --- |
| List indexed roots (entry points) | **Supported** (engine + Qt model) | `vlc_ml_list_entry_points`; `MLFoldersModel`, used only by QWidget preferences and the first-run wizard |
| List all folders that contain media (flat) | **Supported** (engine + Qt + QML) | `VLC_ML_LIST_FOLDERS[_BY_TYPE]`; `MLVideoFoldersModel` behind the "Group by folder" option in Videos |
| Media of one folder (non-recursive) | **Supported** (engine + Qt + QML) | `VLC_ML_LIST_FOLDER_MEDIA` / `VLC_ML_LIST_VIDEO_OF` + `VLC_ML_PARENT_FOLDER`; `VideoGroupDisplay` |
| Subfolders of a folder (tree, down) | **Engine only, UI new** | `VLC_ML_LIST_SUBFOLDERS` exists, but no inline helper and no Qt model; VLC ignores sort and pagination for it |
| Parent of a folder (up) | **New (engine gap)** | `IFolder` has no `parent()`; up must be derived from the MRL string, or the API extended |
| Find the folder of the playing media / a folder by MRL | **New (VLC API gap)** | Upstream has `folder(mrl)`; VLC exposes only `VLC_ML_GET_FOLDER` by id, and `vlc_ml_media_t` has no folder id |
| Back navigation history | **Supported (UI)** | `NavigationHistory` (`History` singleton): push / previous only |
| Forward navigation | **New (UI)** | `NavigationHistory` has no forward stack |
| Address bar / breadcrumb | **UI-only, Browse view** | `NetworkAddressbar` over `NetworkMediaModel.path`; nothing like it for ML folders |
| Filesystem browsing (not indexed) | **Supported** | `NetworkMediaModel` / `BrowseTreeDisplay`; also shows ML progress for files the ML knows |
| Show / series entity | **Engine supported, UI new** | `VLC_ML_LIST_SHOWS`, `VLC_ML_LIST_SHOW_EPISODES`; no Qt model or view (macOS UI has one). Detection is filename-based only |
| "Folder as a show" | **New (concept)** | Not an ML concept. Model it as an ML folder plus UI (and storage for any per-show data, see #4/#15) |
| Name-based groups (closest current "series") | **Supported (engine + Qt + QML)** | Media groups, "Group by name" in Videos; expose `i_nb_seen`. Read-only in VLC's API |
| Per-media watched state (progress, play count, last played) | **Supported** | `f_progress`, `i_playcount`, `i_last_played_date` on `vlc_ml_media_t`; written by the core player |
| Mark as seen / unseen | **Supported (engine + QML)** | `vlc_ml_media_set_played`; `MLContextMenu` "Mark as seen/unseen" |
| Sort by play count | **Supported in model, not exposed in Videos UI** | `MLVideoModel` maps `"playcount"`; the Videos sort menu offers only Alphabetic and Duration |
| Sort "unwatched first / watched first" | **UI-only approximation, or new** | Play count ascending/descending only splits "never finished" from "finished"; in-progress items sort with unwatched. A true 3-state order needs a new sort criterion or a client-side sort |
| Sort by last played | **New in VLC API (engine has it)** | Upstream `SortingCriteria::LastPlaybackDate` is not in `vlc_ml_sorting_criteria_t` |
| Sort folders by media count | **New in VLC API (engine has it)** | Upstream `NbVideo/NbAudio/NbMedia` are not in VLC's enum |
| In-progress ("continue watching") list | **New in VLC API (engine has it)** | Upstream `inProgressMedia()`; VLC exposes only the history lists |
| Folder watched count ("3 of 10 seen") | **New (engine gap)** | Groups have `nbSeen`; folders don't |
| Add folder to library from the UI | **Supported, but scattered** | Browse header toggle; QWidget Preferences; first-run wizard. No QML "Add folder…" picker in the library |
| Remove / ban / reload folder | **Supported (engine + Qt model)** | `MLFoldersModel` / `MLBannedFoldersModel`; only the QWidget preferences UI uses them |
| Indexing progress | **Supported (QML)** | `ScanProgressBar.qml` |

## 1. Listing indexed folders and their media

### Engine (VLC C API)

- `vlc_ml_folder_t` carries `i_id`, `psz_name`, `psz_mrl`, `i_nb_media`, `i_nb_video`, `i_nb_audio`, `i_duration`, `b_is_favorite`, `b_present` and `b_banned` (`include/vlc_media_library.h:383-395`).
- List queries (`include/vlc_media_library.h:506-511, 550-554, 571`):
  - `VLC_ML_LIST_ENTRY_POINTS` returns the roots the user added (or the banned ones). The module maps it to `roots()` / `bannedRoots()` (`modules/misc/medialibrary/medialibrary.cpp:1318-1335`).
  - `VLC_ML_LIST_FOLDERS` and `VLC_ML_LIST_FOLDERS_BY_TYPE` return a **flattened** list of every folder that directly contains media of the type (`medialibrary.cpp:2422-2460` calls `folders()`/`searchFolders()`). Upstream filters this on `nb_video > 0` or `nb_audio > 0` (`ml@0.13.2:src/Folder.cpp:808-831`).
  - `VLC_ML_LIST_SUBFOLDERS` and `VLC_ML_COUNT_SUBFOLDERS` (`medialibrary.cpp:1576-1597`).
  - `VLC_ML_LIST_FOLDER_MEDIA`, and the generic `VLC_ML_LIST_{MEDIA,VIDEO,AUDIO}_OF` with `VLC_ML_PARENT_FOLDER` (`medialibrary.cpp:2462-2496`, mapping at `2048-2090`).
  - `VLC_ML_LIST_FAVORITE_FOLDERS` (`medialibrary.cpp:1546-1574`), with `vlc_ml_folder_set_favorite` (`vlc_media_library.h:2055`).
- **Folder media is not recursive.** A media belongs to the folder that holds its main file (`ml@0.13.2:include/medialibrary/IFolder.h:65-90`).
- **Subfolders include empty folders.** `IFolder::subfolders()` returns every child folder, even one with no media, "for optimization purposes" (`IFolder.h:105-121`). A tree view therefore has to hide empty branches itself, or accept them.
- **VLC's subfolder listing drops the query parameters.** The module calls `parent->subfolders()` with no `params` and returns `query->all()`, so sort, pagination and favorites-only are ignored (`medialibrary.cpp:1581-1583`), although upstream accepts them (`ml@0.13.2:src/Folder.cpp:1125-1132`). There is also no `vlc_ml_list_subfolders()` inline helper in the header (only the enum at `vlc_media_library.h:551`). The only caller today is the UPnP server (`modules/control/upnp_server/ml.hpp:202`).
- **Folder sorting** upstream supports only Alpha/Default and `NbVideo`/`NbAudio`/`NbMedia` (`ml@0.13.2:src/Folder.cpp:742-771`). `vlc_ml_sorting_criteria_t` lacks the `Nb*` values (`vlc_media_library.h:424-443` vs `ml@0.13.2:include/medialibrary/IMediaLibrary.h:72-102`), so from VLC only alphabetical folder order is reachable.

### Qt models and QML

- `MLVideoFoldersModel` lists `vlc_ml_list_folders_by_type(..., VLC_ML_MEDIA_TYPE_VIDEO)` (`modules/gui/qt/medialibrary/mlvideofoldersmodel.cpp:148-171`). There is no audio equivalent.
- It appears in the UI only as the "Group by folder" mode of Videos: `MainCtx.grouping` picks between plain videos, `MLVideoGroupsModel` and `MLVideoFoldersModel` (`modules/gui/qt/medialibrary/qml/VideoAllSubDisplay.qml:45-54, 229-275`; `maininterface/mainctx.hpp:199-201`).
- Activating a folder pushes `["…","group"]` with `parentId` set to the folder's `MLItemId` (`VideoAllDisplay.qml:60-62`). `VideoGroupDisplay` then lists that folder's videos with `MLVideoModel`, which calls `vlc_ml_list_video_of(parent.type, id)` (`mlvideomodel.cpp:172-190`). **The library is one level deep: folders → videos, with no subfolders.**
- Bugs to note:
  - `MLVideoFoldersModel` offers a `"duration"` sort (`mlvideofoldersmodel.cpp:101-107`). Upstream folders can't sort by duration, so it silently falls back to alphabetical (`ml@0.13.2:src/Folder.cpp:761-766`).
  - `MLFolder` hard-codes `m_duration(0)` with a FIXME (`mlfolder.cpp:34`), though `vlc_ml_folder_t::i_duration` now exists.
- `MLFoldersModel` and `MLBannedFoldersModel` list roots and banned roots (`mlfoldersmodel.hpp:108-132`, `mlfoldersmodel.cpp:163`). Both are registered for QML as `MLFolderModel` (`maininterface/mainui.cpp:380, 489`), but no QML file uses them. Only the QWidget `MLFoldersEditor` in Preferences and the first-run wizard do (`dialogs/preferences/simple_preferences.cpp:1033-1040`, `dialogs/firstrun/firstrunwizard.cpp:105-110`).

## 2. Navigating up and down folders, with history

- **Down:** supported by the engine (`VLC_ML_LIST_SUBFOLDERS`), not by any Qt model.
- **Up:** `IFolder` has no parent accessor (`ml@0.13.2:include/medialibrary/IFolder.h:35-143`). The parent id is stored internally (`src/Folder.h:195`), but nothing public exposes it. Options are to strip the last path segment of `psz_mrl` and look it up by MRL (which also needs a new VLC API, see below), or to add `parent()` upstream.
- **Folder by MRL:** upstream has `IMediaLibrary::folder(const std::string& mrl)` (`IMediaLibrary.h:936`), and the VLC module calls it internally for set-public/favorite (`medialibrary.cpp:571, 794`). There is no `VLC_ML_GET_FOLDER_BY_MRL`. `vlc_ml_media_t` has no folder id either (`vlc_media_library.h:209-247`). Upstream keeps `Media::folderId()` as test-only (`ml@0.13.2:src/Media.h:277`). So "open the library at the playing file's folder", which is the redesign's show-name button, needs a small new VLC API.
- **History:** `NavigationHistory` (QML singleton `History`, `maininterface/mainui.cpp:239`) has `push`, `update` and `previous`, plus `previousEmpty` (`modules/gui/qt/util/navigation_history.hpp:13-88`). It has **no forward stack**, so the audit's back **and forward** buttons mean extending this class. The back button lives in `maininterface/qml/LocalTopbar.qml:136-137`.
- **Address bar:** `NetworkAddressbar.qml` renders a clickable breadcrumb with overflow menu from a `path` list (`network/qml/NetworkAddressbar.qml:33-66, 184-186`), fed by `NetworkMediaModel.path` (`network/networkmediamodel.hpp:130`). It exists only for Browse (filesystem/network), not for ML folders. It could be reused, but an ML folder path would have to be built (MRL segments mapped to folder ids).
- **Browse (filesystem) already does up/down navigation.** `BrowseDisplay` pushes `browse` with a `tree` per directory (`network/qml/BrowseDisplay.qml:62-75`). Its sorts are name, URL, file size and modified date (`BrowseTreeDisplay.qml:72-77`), and the sort function also handles duration (`networkmediamodel.cpp:280-345`). Each file row carries `MLMedia` looked up by MRL, so **progress is already available in Browse** (`networkmediamodel.cpp:45-55, 576-584, 613-614`).

## 3. Shows and series

- **ML Show entity:** `vlc_ml_show_t` (name, summary, artwork, tvdb id, release year, episode and season counts) and `vlc_ml_show_episode_t` (season/episode number) (`vlc_media_library.h:121-139`). Queries: `VLC_ML_LIST_SHOWS`, `VLC_ML_LIST_SHOW_EPISODES` and `VLC_ML_PARENT_SHOW` (`vlc_media_library.h:494, 534-535, 592`; module `medialibrary.cpp:1191-1243`).
- **How shows are detected:** only from the **file name**. `MetadataAnalyzer::parseVideoFile` sanitizes the file name and runs `utils::title::analyze`. On a match it finds or creates a Show and adds the episode (`ml@0.13.2:src/metadata_services/MetadataParser.cpp:622-674`). The patterns are `(S|B)<season>(E|x| )<episode>` and `<n>x<m>`. The show name is the text **before** the match and the episode title the text after (`ml@0.13.2:src/utils/TitleAnalyzer.cpp:211-247`). The folder name is never used. VLC forwards `vlc_meta_ShowName` and `vlc_meta_Episode` to the parser (`modules/misc/medialibrary/MetadataExtractor.cpp:119-120`), but this version doesn't use them to build shows.
- **No API to create or edit a show, or move media into one.** `createShow` and `addEpisode` are internal (`ml@0.13.2:src/MediaLibrary.h:143`, `src/Show.h:69`) and absent from `IShow` and `IMediaLibrary`.
- **Sorting:** episodes by season then episode (default) or alpha (`ml@0.13.2:src/Show.cpp:150-175`); shows by title or release date (`src/Show.cpp:401-420`).
- **Qt has no show model or view.** No Qt file calls `vlc_ml_list_shows`. The macOS UI does (`modules/gui/macosx/library/VLCLibraryModel.m:542, 1243`), as does the Lua lib (`modules/lua/libs/medialibrary.c:456`). `VLC_ML_PARENT_SHOW` is only registered as a QML enum value (`mlqmltypes.hpp:36`).
- **Header bug:** `vlc_ml_count_show_episodes` passes `VLC_ML_COUNT_GENRE_ALBUMS` instead of `VLC_ML_COUNT_SHOW_EPISODES` (`include/vlc_media_library.h:1772-1779`). It needs a fix before any show view uses it.
- **Media groups** are the closest thing to a "series" in today's Qt UI. Upstream groups videos automatically by a common title prefix of at least 6 characters (`ml@0.13.2:src/MediaGroup.h:36`), and `vlc_ml_group_t` exposes `i_nb_seen` / `i_nb_present_seen` (`vlc_media_library.h:249-271`). VLC's API is read-only for groups: no create, rename or add, although upstream `IMediaGroup` has them (`ml@0.13.2:include/medialibrary/IMediaGroup.h:133-195`).
- **Conclusion for the redesign:** in the audit, the "show" is the folder the episodes live in. That maps cleanly to an ML folder (id + MRL + name + counts). The ML Show entity would miss shows whose files lack `SxxEyy` and split shows whose file-name prefixes differ. Treat "show = ML folder" as the working model and leave the Show entity unused (or optional metadata).

## 4. Watched state and sorting by it

- **Stored per media:** `f_progress`, `i_playcount` and `i_last_played_date` (`vlc_media_library.h:222-224`). The core player writes progress at the end of playback when `save-recentplay` is on (`src/player/medialib.c:205-206`). It creates an "external" ML media for files that aren't indexed (`src/player/medialib.c:184-190`), so watched state exists even outside indexed folders.
- **Thresholds:** upstream turns a saved position into Begin, End or AsIs. Its margin is 5% for media under 1 h, falling to 1% for media over 4 h (`ml@0.13.2:src/Media.cpp:331-354`). End bumps `play_count` and resets the position. Begin resets the position (`src/Media.cpp:358-424`). So "watched" means `play_count > 0`, and "in progress" means `last_position >= 0` with `play_count` unchanged.
- **Explicit toggle:** `vlc_ml_media_set_played(true)` calls `markAsPlayed()`, which bumps the play count. `false` calls `removeFromHistory()`, which clears position, play count and last-played (`medialibrary.cpp:1953-1962`; `ml@0.13.2:src/Media.cpp:462, 493`). Qt exposes this as `MLVideoModel::setItemPlayed` (`mlvideomodel.hpp:53`, `mlvideomodel.cpp:36-48`) and the "Mark as seen/unseen" context-menu items (`qml/MLContextMenu.qml:91-97`). The Qt `isNew` flag is `playCount == 0 && progress <= 0` (`mlvideo.cpp:81`). The QML roles `progress` and `playcount` exist on media models (`mlmediamodel.cpp:131-132`).
- **Sorting:**
  - `VLC_ML_SORTING_PLAYCOUNT` maps to upstream `PlayCount`, which sorts **descending by default** (`ml@0.13.2:src/Media.cpp:1438-1440`). `MLVideoModel` accepts `"playcount"` and `"insertion"` (`mlvideomodel.cpp:100-107`), but the Videos sort menu lists only Alphabetic and Duration (`qml/VideoAll.qml:88-90`).
  - **"Watched first" / "unwatched first" can be approximated** by play count descending/ascending, with no engine work. The approximation puts in-progress items among the unwatched and orders watched items by how often they were watched, not by name. A clean order (unwatched → in progress → watched, then by name or episode) needs either a **new upstream sort criterion** or a **client-side sort** inside a single folder's media list. The client-side sort is feasible because folder media lists are small.
  - Upstream `LastPlaybackDate` (`src/Media.cpp:1461`) and `inProgressMedia()` (`IMediaLibrary.h:599-606`, `src/Media.cpp:1540-1556`) exist but are **not plumbed through VLC's API** (`vlc_ml_sorting_criteria_t` ends at `TRACKNUMBER`). Adding them is a small change in `include/vlc_media_library.h` plus `MediaLibrary::sortingCriteria` (`medialibrary.cpp:1728-1757`).
  - Folder-level watched counts ("3/10 watched") don't exist: `IFolder` has no `nbSeen` (`IFolder.h:130-139`), unlike groups.

## 5. Adding a folder to the library from the UI

- **Engine:** `vlc_ml_add_folder`, `vlc_ml_remove_folder`, `vlc_ml_ban_folder`/`unban`, `vlc_ml_reload_folder` and `vlc_ml_is_indexed` (`vlc_media_library.h:599-615, 1041-1080`), mapped to `discover`/`removeRoot`/`banFolder`/`isIndexed` (`medialibrary.cpp:521-595`). They run asynchronously. Completion comes back as `VLC_ML_EVENT_ENTRY_POINT_ADDED/REMOVED/BANNED/UNBANNED` and progress as discovery and parsing events (`vlc_media_library.h:745-815`). Folder MRLs need a trailing `/` (`networkmediamodel.cpp:660-661`).
- **Where the UI can do it today:**
  1. **Browse header** "Add to medialibrary" / "Remove from medialibrary" toggle for the current directory (`network/qml/BrowseTreeHeader.qml:95-111` → `NetworkMediaModel::setIndexed`, `networkmediamodel.cpp:684-712`). It shows only for `file`, `smb` and `ftp` directories (`networkmediamodel.cpp:271-275`). Per-row `indexed` state is also exposed (`networkmediamodel.cpp:234-257, 626-680`).
  2. **Preferences → Media Library** (QWidget): add/remove/ban list with a `QFileDialog` (`dialogs/preferences/simple_preferences.cpp:1028-1040, 1657-1667`).
  3. **First-run wizard** (QWidget) (`dialogs/firstrun/firstrunwizard.cpp:105-110, 183-192`).
- **New for the redesign:** an "Add folder…" item in the library's overflow menu that opens a folder picker and calls `vlc_ml_add_folder`. All the pieces exist: `MLFoldersModel.add(url)` (`mlfoldersmodel.cpp:194-202`) is already QML-registered. Only the QML entry point and a folder picker (Qt Quick Dialogs or the existing `DialogsProvider`) are new. Indexing feedback can reuse `ScanProgressBar.qml`.
- **Caveat (likely, not fully verified):** `isIndexed` returns true for **any** MRL under an indexed root (`IMediaLibrary.h:893-899`), so the Browse toggle reads "Remove from medialibrary" inside a root's subfolder. Clicking it calls `removeRoot` on a non-root folder. Upstream deletes that folder's rows (`ml@0.13.2:src/discoverer/DiscovererWorker.cpp:548-561`), but the next reload of the parent root will probably rediscover it. **Ban** is the durable way to exclude a subfolder. The redesign's folder context menu should offer "Exclude" (ban) for non-root folders and "Remove from library" only for roots.

## What would be new (summary for planning)

Work is split by layer. "Small" means a few lines in an existing switch or enum.

1. **VLC C API (small):**
   - add `vlc_ml_list_subfolders()`/`count` helpers and honour `params` in the module (`medialibrary.cpp:1576-1597`);
   - add `VLC_ML_GET_FOLDER_BY_MRL`, or a media→folder lookup;
   - add `LAST_PLAYBACK_DATE`, `NB_VIDEO`/`NB_AUDIO`/`NB_MEDIA` sort criteria;
   - expose `inProgressMedia` as `VLC_ML_LIST_IN_PROGRESS`;
   - fix `vlc_ml_count_show_episodes`.
2. **libmedialibrary (fork or upstream patch; optional):** `IFolder::parent()`, folder seen counts, and a watched-state sort criterion. Each can be avoided: derive the parent from the MRL, count client-side, and sort client-side.
3. **Qt models:**
   - a hierarchical ML folder model (roots → subfolders → media, empty folders filtered);
   - fix `MLFolder` duration;
   - expose play-count and insertion sorts in the menus;
   - add a client-side "unwatched / in progress / watched" sort for folder media;
   - an optional `MLShowsModel` if the Show entity is ever surfaced.
4. **QML / UI:**
   - a folder browser with breadcrumb (reuse `NetworkAddressbar`) and back/forward;
   - forward support in `NavigationHistory`;
   - "Add folder…" in the library menu;
   - folder context menu (exclude/remove/favorite/reload);
   - a show-name header that opens the library at the playing file's folder.

## Open points surfaced

- **Library = ML folders or Browse?** Should the redesign's folder view be built on **ML folders**, which cover indexed content only and carry rich metadata, or on **Browse (filesystem)**, which already supports up/down, address bar and progress, but only for what's on disk? Browse already shows ML progress per file and an "Add to library" toggle, so a hybrid is plausible. This is a #13 (Library structure) decision.
- **What a "show" is.** The ML Show entity doesn't fit the audit's "show = folder". #15 (Presets and shows) should define a show as a folder, keyed by ML folder id or MRL, and #4 should check whether per-folder data can be stored (the ML has no per-folder key/value store; upstream `IMetadata` is per media).
- **Forward navigation** is a small but real change to `NavigationHistory` and affects every page, not only folders.
