# Playback engine feasibility of audit features

Research for [#3](https://github.com/floklein/jvlc/issues/3), part of the redesign map [#1](https://github.com/floklein/jvlc/issues/1).

**Question.** For each engine-dependent playback feature in [`AUDIT.md`](../../AUDIT.md), what do VLC core, libvlc, and the Qt player controller already support, and what would be new engine work?

**Sources.** This source tree at `feat/redesign` (`ccca5c8c92`, VLC 4.x master). Line numbers refer to that commit. External sources are linked inline.

**Terms.** *Engine* means VLC core (`src/`, `include/`), libvlc (`lib/`), and modules other than the Qt interface. The Qt interface (`modules/gui/qt`) is itself a VLC plugin, so it can call any `VLC_API` function directly. "Qt glue" means new C++ in `modules/gui/qt` that calls existing engine APIs; it counts as UI work, not engine work.

## Answer

| Feature | Classification | Size of non-UI work |
| --- | --- | --- |
| Default subtitle/audio track by language | **Supported** (global). Per-show presets are UI work | None for global defaults. Per-show storage is UI/data work (S–M) |
| Switching audio output device from the player | **Supported**; UI-only | None. Optional: refresh list on hotplug (XS, Qt glue) |
| Sleep timer | **UI-only** | None. Timer lives in Qt (XS–S) |
| Thumbnails while seeking | **UI-only** on an existing engine API (preparser thumbnailer) | Qt glue: request scheduling, cache, image provider (S–M) |
| Reverse playback | **Silent frame-by-frame reverse: UI-only** (S). **True reverse playback with audio at speed: new engine work (XL)** | XL for true reverse; recommend ruling it out |
| Subtitle timeline (cue times for the current track) | **Needs new work**: no API exposes cue lists | M for external text subtitles (Qt-side parse or new player API); L+ for embedded tracks |
| Waveform on the seek bar | **Needs new engine work**: no whole-file audio analysis exists | M–L (background decode + peak cache), local files only |
| Fetching subtitles from OpenSubtitles | **Needs new work** (network client + UI); loading the downloaded file is supported | M, plus an OpenSubtitles API key and quota constraints |

Sizes are rough: XS < 1 day, S ≈ days, M ≈ 1–2 weeks, L ≈ 3–6 weeks, XL ≈ months, for one developer familiar with the code.

## 1. Reverse playback

### What exists

- **Negative rates are accepted only by a few live/network access modules.** `INPUT_CONTROL_SET_RATE` takes the sign of the requested rate, but refuses a negative one whenever the input rescales timestamps itself (`b_rescale_ts`): "cannot set negative rate" (`src/input/input.c:2177-2208`). `b_rescale_ts` is true for every demuxer that controls its own pace, which covers every local file (`src/input/input.c:2994-3009`). Only demuxers that answer `DEMUX_CAN_CONTROL_RATE` (`include/vlc_demux.h:261-267`) without pace control can go negative, e.g. RTSP servers that support `Scale` (`modules/access/live555.cpp:1700-1721`: "Scale < 0 value indicates rewind").
- **The Qt UI already has a "Trickplay Reverse" button** that negates the rate (`modules/gui/qt/player/player_controller.cpp:1293-1303`, `modules/gui/qt/player/qml/controlbarcontrols/ReverseButton.qml:37`, `modules/gui/qt/player/qml/PlayerControlbarControls.qml:48`). On local files the input refuses it (see above), so the button does nothing useful.
- **Previous-frame stepping landed recently.** `vlc_player_PreviousVideoFrame()` (`include/vlc_player.h:490-501`) pauses and displays the previous video frame. Commit `cd270d030c` ("input: add previous-frame support", Nov 2025) implements it by seeking back to `current_frame - n` and decoding forward, and states it "is able to play backward 'in real-time' (when pressing the previous-frame key) most of 1080p videos". Status arrives through `on_prev_frame_status` (`include/vlc_player.h:3357-3375`), which returns `-EBUSY`, `-ENOTSUP` or `-ERANGE` on failure; the decoder gives up after `DEC_PF_SEEK_STEPS_MAX` (200) seek attempts (`src/input/decoder_prevframe.h:34`, `src/input/decoder.c:305-326`). The request requires the decoder to be paused (`src/input/decoder.c:3033-3065`). Qt wraps it as `PlayerController::framePrev()` (`modules/gui/qt/player/player_controller.cpp:1444-1451`) and libvlc as `libvlc_media_player_previous_frame()` (`lib/media_player.c:1982-1990`). The control-bar editor already offers a "Previous Frame" button (`modules/gui/qt/player/qml/controlbarcontrols/FramePrevButton.qml:33`; its enum id is swapped with "Next Frame" at `modules/gui/qt/player/qml/PlayerControlbarControls.qml:46-47`, which looks harmless but confusing). The Qt controller does not listen to `on_prev_frame_status` today.

### What it would take

- **Silent reverse ("jog back") — UI-only, S.** Hold a reverse control; Qt glue chains `vlc_player_PreviousVideoFrame()` calls, issuing the next one when `on_prev_frame_status` reports success. Video only, no audio, frame rate bounded by GOP length and decode speed, local seekable media only (`-ENOTSUP` otherwise).
- **True reverse playback (audio + video at 1×/2× backwards) — new engine work, XL.** It would need GOP-wise backward decoding with frame buffering, a clock that runs backwards, reversed audio output, and subtitle handling in reverse. None of this exists (the only negative-rate path is server-side RTSP `Scale`).

**Recommendation.** Rule true reverse playback out of scope. Either drop the feature or redefine it as "hold to step backwards" on top of previous-frame.

## 2. Waveform on the seek bar

### What exists

- **Nothing computes a whole-file waveform.** No player, preparser, or media-library API returns audio peaks. The preparser offers only parse, meta/art fetch, and thumbnail types (`include/vlc_preparser.h:65-69`). The `modules/visualization/` plugins (`visual`, `glspectrum`, `goom`, `projectm`, `vsxu`) render the audio *currently playing*; they cannot see ahead.
- **The building blocks are exported** to plugins: `demux_New()` (`include/vlc_demux.h:297`), `decoder_Init()` / `decoder_Clean()` and module loading (`src/libvlccore.sym:95,97,285`). The preparser's thumbnailer shows the pattern of a background `input_Create()` on a worker thread (`src/preparser/internal.c:519-553`).

### What it would take — new engine work, M–L

A background job that opens the media, decodes the full audio track as fast as possible, reduces it to N peak buckets, and caches the result per media. Natural homes: a new preparser request type next to `VLC_PREPARSER_TYPE_THUMBNAIL` (engine change), or a standalone worker inside the Qt plugin built from `demux_New` + an audio decoder. Then a QML seek-bar renderer (UI).

Constraints:

- Cost is a full audio decode. Fine for music files (seconds); a two-hour movie takes much longer and much I/O. Network streams and live inputs are out.
- The audit places the waveform in the music player ("you have the waveform seek bar if you want"). Restricting it to local audio files keeps the cost bounded.

## 3. Thumbnails while seeking

### What exists — engine API is ready

- **The preparser thumbnailer** takes a seek target by time or position, with `VLC_THUMBNAILER_SEEK_FAST` (keyframe) or `SEEK_PRECISE` (`include/vlc_preparser.h:195-227`), and an optional hardware decoder. `vlc_preparser_req_NewThumbnail()` returns a `picture_t` through `on_ended` (`include/vlc_preparser.h:125-150`, `:393-398`). The preparser can run several thumbnailer threads, a timeout, and an external process (`include/vlc_preparser.h:283-312`). libvlc exposes the same through `libvlc_parser_task_new_thumbnail()` (`include/vlc/libvlc_parser.h:51-119`).
- **Each request opens a fresh input** (`input_Create` with `INPUT_TYPE_THUMBNAILING`, then seek, `src/preparser/internal.c:519-553`). Latency per thumbnail is therefore an open + seek + decode of one frame.
- **Qt already creates a preparser** (`modules/gui/qt/maininterface/mainctx.cpp:279-287`), but configured for `VLC_PREPARSER_TYPE_PARSE` only; it would need the thumbnail type or a second instance.
- **The seek bar already has a hover tooltip** showing time and chapter name (`modules/gui/qt/player/qml/SliderBar.qml:74-96`); a thumbnail would attach there.

### What it would take — UI-only, S–M

Qt glue to request thumbnails on hover (debounced, `SEEK_FAST`), cancel stale requests, cache results per media/time bucket, and hand `picture_t` to QML through an image provider. Optionally pre-generate a coarse strip in the background for instant scrubbing. Local files work well; network streams will be slow and should fall back to time-only tooltips.

## 4. Sleep timer

### What exists

- No sleep timer anywhere in core, libvlc, Lua, or Qt (no matches for "sleep timer" in `src/`, `lib/`, `include/`, `share/lua`, `modules/gui/qt`).
- Everything a timer needs exists: `vlc_player_Pause()` / `vlc_player_Stop()` (`include/vlc_player.h:459,467`), volume control for a fade-out (`PlayerController::volume`, `modules/gui/qt/player/player_controller.hpp:214`), and an "after current media" mode: `vlc_playlist_SetMediaStoppedAction()` with `STOP` or `EXIT` (`include/vlc_playlist.h:176-183,456`), already exposed to QML as `PlaylistController.mediaStopAction` (`modules/gui/qt/playlist/playlist_controller.hpp:84-107`).

### What it would take — UI-only, XS–S

A Qt-side timer (C++ or QML `Timer`) with presets (15/30/60 min, end of current media), an optional volume fade, and an indicator. The timer cannot survive an app restart and needs no engine support.

## 5. Subtitle timeline (cue times for the current track)

### What exists

- **No API exposes cue lists.** The player only sees subtitle blocks when the decoder receives them at playback time. There is no demux control or player function to list a track's cues (`modules/demux/subtitle.c:777-853` handles only the generic controls).
- **Text subtitle demuxers load every cue into memory at open.** `subtitle.c` keeps an array of `{i_start, i_stop, psz_text}` (`modules/demux/subtitle.c:120-126`, filled at `:661-684`); `webvtt.c` keeps a `cues` array (`modules/demux/webvtt.c:62-65`). Embedded tracks (MKV/MP4 text, PGS/VobSub bitmaps) are interleaved with the video and only known after reading the whole file.
- **Delay control is complete.** `vlc_player_SetSubtitleDelay()` / `GetSubtitleDelay()` (`include/vlc_player.h:2081-2090`) and per-track `vlc_player_SetEsIdDelay()` (`:2034`); Qt exposes `subtitleDelay` / `subtitleDelayMS` (`modules/gui/qt/player/player_controller.hpp:167,170`). The existing "sub sync" hotkeys mark audio and subtitle moments and apply the difference (`modules/control/hotkeys.c:515-537`), which is the same workflow the audit describes, minus the visual timeline.
- **Source of external subtitles is reachable.** External subtitle files are attached as input item slaves (`include/vlc_input_item.h:123-124`) via `vlc_player_AddAssociatedMedia()` (`include/vlc_player.h:934-945`).

### What it would take — needs new work, M (external) / L+ (embedded)

Two routes for external text subtitles (the audit's case: subtitles downloaded and added to a folder):

1. **Qt-side parse, no core change.** In a worker, open the subtitle slave's URL with `demux_New()` using the `subtitle`/`webvtt` module and a capturing `es_out_t` (`include/vlc_es_out.h:131-136`), read to EOF, and keep each block's start time and duration. Shift by the current subtitle delay for display. M.
2. **New player API** (e.g. "get cues of track X") implemented by the text subtitle demuxers that already hold the array. Cleaner, but touches demux controls, the input thread, and the player. M–L.

Embedded tracks need a full-file scan for cues, like the waveform job. Defer them; show the timeline only when the selected subtitle track comes from an external text file.

## 6. Switching audio output device from the player

### What exists — supported

- Core: `aout_DevicesList()`, `aout_DeviceGet()`, `aout_DeviceSet()` (`include/vlc_aout.h:506-529`); device change event `on_device_changed` (`include/vlc_player.h:2417`). libvlc: `libvlc_audio_output_device_enum()` / `_set()` (`include/vlc/libvlc_media_player.h:3150-3195`).
- Qt: `AudioDeviceModel` lists devices and switches on selection (`modules/gui/qt/util/audio_device_model.cpp:41-53,119`), exposed to QML as `Player.audioDevices` (`modules/gui/qt/player/player_controller.hpp:216`). Today only the QWidgets menu uses it (`modules/gui/qt/menus/menus.cpp:418`); no QML view does.

### What it would take — UI-only, XS–S

Add a device list to the audio popup in QML. One gap: the model reads the device list once at construction (`audio_device_model.cpp:51-53`) and listens only to `on_device_changed`, so hot-plugged headphones don't appear. Re-reading `aout_DevicesList()` when the popup opens fixes it in Qt glue. Output modules do report hotplug (`aout_HotplugReport`, `include/vlc_aout.h:135,347`), but the player's aout listener has no list-changed callback; adding one would be a small engine change, and is optional.

## 7. Default subtitle/audio track by language

### What exists — supported

- Options `audio-language` and `sub-language` (`src/libvlc-module.c:1867-1872`) take a comma-separated priority list of language codes, plus `any` and `none` (`src/input/es_out.c:4521-4544`). The ES output selects tracks by that list (`src/input/es_out.c:2993-3010`, wired at `:4448-4451`).
- Player API: `vlc_player_SelectCategoryLanguage()` / `GetCategoryLanguage()` and the `SelectAudioLanguage` / `SelectSubtitleLanguage` helpers (`include/vlc_player.h:1808-1855`), implemented as player variables (`src/player/player.c:746-777,2158-2159`). Note the warning: the language applies to *future* media only (`include/vlc_player.h:1810`).
- The current Qt preferences already set both options (`modules/gui/qt/dialogs/preferences/simple_preferences.cpp:530,950`), as global config, not per show.
- External subtitle files get a language from the filename pattern `name.LANG.ext` (`modules/demux/subtitle.c:709-719,2516-2518`), so `Show.S01E02.en.srt` matches `sub-language=en`.
- **Per-file memory already exists**: with the media library, the player saves and restores the selected audio/subtitle track IDs and delays per media (`src/player/medialib.c:85-95,282-283`; `include/vlc_media_library.h:690-694`).

### What it would take — UI-only (S); per-show presets are a separate design question

- A global "default audio/subtitle language" setting in the new sidebar or settings: UI-only, wiring the existing options.
- The audit's per-show default ("saved in your preset") needs a place to store a preset per folder/show and to apply it when an item starts (e.g. `vlc_player_SelectCategoryLanguage()` before playback, or per-item input options such as `:sub-language=`). No engine change, but a storage design (S–M).
- Limitation: matching uses the track's language tag. Untagged tracks (common in fan-made MKVs, or `.srt` files without a language suffix) cannot be matched by language; falling back to "same track index/name as last episode" would be new UI logic.

## 8. Fetching subtitles from OpenSubtitles

### What exists

- **Loading a downloaded subtitle is supported.** `vlc_player_AddAssociatedMedia()` (`include/vlc_player.h:934-947`, `src/player/player.c:1156-1201`); Qt wraps it as `PlayerController::associateSubtitleFile()` (`modules/gui/qt/player/player_controller.cpp:2224-2227`), already used by QML drag-and-drop (`modules/gui/qt/maininterface/qml/MainInterface.qml:292-297`).
- **The bundled VLSub extension is effectively broken.** `share/lua/extensions/VLSub.lua` (v0.11.1) talks to the legacy XML-RPC API (`https://api.opensubtitles.org/xml-rpc`, `:1044`; `LogIn` / `SearchSubtitles` at `:1144-1212`), and despite the https URL sends plain HTTP on port 80 through `vlc.net.connect_tcp` (`:1095`, `:1725`, `:1735`). OpenSubtitles posted a final XML-RPC shutdown notice on 2026-01-29 for VIP and non-VIP users ([forum topic 5045](https://forum.opensubtitles.com/t/5045)). Useful pieces: the OpenSubtitles "moviehash" implementation (`:1328-1401`), and loading via `vlc.player.add_subtitle` → `modules/lua/libs/input.c:642-666` → `vlc_player_AddAssociatedMedia`.
- **The current UI routes "Search online" to VLSub's QWidgets dialog.** The QML subtitle menu entry (`modules/gui/qt/menus/qml_menu_wrapper.cpp:755-758`, `modules/gui/qt/player/qml/TracksListPage.qml:269-271`) calls `Player.openVLsub()` (`modules/gui/qt/player/player_controller.cpp:1926-1935`), which opens the Lua extension. Extension dialogs are QWidgets-only (`modules/gui/qt/dialogs/extensions/extensions.cpp:177-296`), conflicting with the map's "everything the redesign touches ends up in QML".
- **No HTTP client in the Qt module.** It does not link Qt Network (`modules/gui/qt/meson.build:17-19`); there is no `QNetworkAccessManager` and no generic HTTP POST/JSON helper in core.
- **Upstream has not ported VLSub.** VideoLAN issue [#27168](https://code.videolan.org/videolan/vlc/-/issues/27168) (migrate VLSub to the REST API) is open with no merge request. VDD 2024 notes suggest a VideoLAN-run proxy, because a consumer API key cannot be shipped inside every VLC. OpenSubtitles' own REST port, [opensubtitles/vlsub-opensubtitles-com](https://github.com/opensubtitles/vlsub-opensubtitles-com) (GPL-3.0), targets the VLC 3 Lua API and needs an external `curl`; it will not run on VLC 4 unported.

### OpenSubtitles REST API constraints

From the [REST API docs](https://opensubtitles.stoplight.io/docs/opensubtitles-api) and the [consumer page](https://www.opensubtitles.com/en/consumers):

- One consumer **Api-Key per application is mandatory**; apps must not make users bring their own key. A `User-Agent` header is required.
- Login returns a JWT valid 24 h. Search is unlimited; downloads are limited to 5/IP/day anonymous, 20/day for signed-up users, 1000/day for VIP. The `/download` docs list both Api-Key and Authorization as required, which conflicts with the anonymous quota; assume login is needed.
- Rate limit 5 requests/s/IP. Download links expire after 3 h and return UTF-8 SRT. The free tier injects ads into subtitles; paid consumer tiers run $20–$400/month.

### What it would take — needs new work, M (plus a non-technical blocker)

1. An HTTP client: link Qt Network in the Qt module (small build change) or write a C helper.
2. A consumer API key, or a VideoLAN/fork-run proxy holding the key. This is a policy and cost decision, not code.
3. Optional account login with secure credential storage (OS keychain), for the 20/day quota.
4. Client flow: moviehash (port from VLSub) → `GET /api/v1/subtitles` → `POST /api/v1/download` → fetch the link → save to a cache or next to the media → `associateSubtitleFile()`.
5. A QML search/results panel replacing `openVLsub()`, plus the audit's "add to file / folder / playlist" matching dialog (`AUDIT.md`, subtitle matching frames), which is UI and data work on top.

No core engine change is needed; the work is a network client, credentials, and UI.

## Open questions raised

- **Presets.** Default tracks, subtitle style, and delays "saved per show" need a preset model and storage location (media library app-specific state, per-folder file, or Qt settings). This is a domain decision, not engine work.
- **Reverse.** Is "hold to step backwards" (silent, video-only) an acceptable reinterpretation of the audit's "reverse feature"?
- **Waveform scope.** Music-only, local-files-only? Where does the peak cache live?
- **Subtitle timeline scope.** External text subtitles only for v1?
- **OpenSubtitles.** Who owns the consumer API key (register one for the fork, or a proxy)? Accept the ad-injected free tier and the 5–20 downloads/day quota? Require an OpenSubtitles login? Drop the bundled XML-RPC VLSub entirely?
- **Extensions area.** Lua extension dialogs are QWidgets-only (`modules/gui/qt/dialogs/extensions/extensions.cpp:177-296`). If extensions stay, either their dialogs get a QML renderer or they are the one surface left in QWidgets; this feeds the map's "Extensions / add-ons area" item.
