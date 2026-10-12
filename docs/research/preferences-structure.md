# Structure of VLC's preferences

Research for [#7](https://github.com/floklein/jvlc/issues/7) (map [#1](https://github.com/floklein/jvlc/issues/1)). It feeds the grilling ticket [#18](https://github.com/floklein/jvlc/issues/18) "Settings structure".

**Question.** How is VLC's preferences system structured? How many options exist? Where do duplicates and inconsistent wordings occur? How would the options spread across the 13 Figma settings categories?

**Sources.** The source tree of this fork (VLC 4.x master, `feat/redesign` @ `ccca5c8c92`) and the Figma file `QRb0F0V7fSTZCIoOyeV0Km`, with settings frames listed in [`AUDIT.md`](../../AUDIT.md). Option counts come from a scan of every `add_*` macro in `src/` and `modules/` (see [Method](#method-and-caveats)).

## Answer in brief

- **Two layers, three views.**
  - **Options** are declared in code. The core's options live in `src/libvlc-module.c`; each plugin declares its own. Each option is filed under one of 7 categories and 32 subcategories.
  - **The Qt dialog** shows those options three ways:
    - *Simple*: 7 hand-built panels covering 84 options.
    - *All*: a category → subcategory → plugin tree that shows every option.
    - *Expert*: one flat, searchable table.
  - Hotkeys have their own editor.
- **Option counts.**
  - 2,078 option declarations in total.
  - 84 are obsolete and 86 are private, so the GUI never shows them.
  - The *All* and *Expert* views show **1,647 options**, spread over about 268 plugins. On top of these, the hotkey editor shows **114 actions**, each with a local and a global binding (228 entries).
  - On Windows and Linux with the Qt interface, about **1,590** of the 1,647 apply. The rest are macOS, Android or OS/2 only, or belong to skins2 and ncurses.
- **The core is small; plugins are most of it.** The core and the Qt module together own only 240 options. The bulk comes from encoders (x264 alone has 76), capture devices, video filters and stream output.
- **Figma has about 217 rows.** Most of them map onto core or Qt options:
  - About 25 rows are new behavior, buttons, or settings stored outside vlcrc.
  - About 25 more map onto existing options with a different shape: a dropdown over a bool, or one row driving several options or a filter.
  - See [Figma rows](#5-figma-rows-mapped-to-vlc-options).
- **Draft distribution of the 1,647 options plus 114 hotkey actions:**
  - Recording 410, Video 350, Audio 169, Network 111, Subtitles 94, Playback 54, Performance 36, Interface 35, General 29, Controls 26 + 114 hotkeys, Library 19, Privacy 6, Extensions 4.
  - **247 options have no Figma home:** 184 for capture devices, 63 demuxer-specific.
  - 57 options are out of scope: they are off-platform or belong to the dropped interfaces.
- **Duplicates and wording problems are real and countable.**
  - The same option name is declared in 2–4 plugins (`contrast`, `brightness`, `hue`, `saturation` and `gamma` in 4 plugins each; `smb-user` in 4; `screen-*` in 3).
  - The same label is used for different options ("Password" ×11, "Frame rate" ×11, "Username" ×9).
  - 42 of 65 Simple-view labels word the option differently from its own label in *All*/*Expert*.
  - The Simple view binds one obsolete option, one option that doesn't exist and two pairs of swapped labels.

## 1. How the system is built

### 1.1 Options and categories (engine side)

- **Declaration.** Options are declared with macros in a module descriptor:
  - Value macros: `add_bool`, `add_integer`, `add_integer_with_range`, `add_float`, `add_float_with_range`, `add_string`.
  - Typed string macros: `add_password`, `add_loadfile`, `add_savefile`, `add_directory`, `add_font`, `add_module`, `add_module_list`, `add_key`, `add_rgb`.
  - All of them are defined in `include/vlc_plugin.h:445-497`.
  - `add_key` declares two options per action, `name` and `global-name` (`include/vlc_plugin.h:478-481`).
- **Placement.** `set_subcategory()` files the options that follow it under a subcategory. `set_section()` inserts a group heading (`include/vlc_plugin.h:438-443`).
- **Categories.** The 7 categories are Interface, Audio, Video, Input/Codecs, Stream output, Advanced and Play Queue (`include/vlc_plugin.h:150-166`).
  - `CAT_HIDDEN` and `SUBCAT_HIDDEN` hide an option from the GUI but keep it in `--help` (`include/vlc_plugin.h:152-155`).
- **Subcategories and their names.** The 32 subcategories are in `include/vlc_plugin.h:169-217`. Their names are in `src/config/cat.c:46-89`, built on titles from `include/vlc_config_cat.h:38-47`.
  - The names don't match how users talk: "Input / Codecs", "Play Queue" (the category that holds "one instance" and "media library"), "Subtitles / OSD" under Video, "Sout stream", "VOD".
- **Hidden options.**
  - `change_private()` / `change_volatile()` keep an option out of the preferences (`include/vlc_plugin.h:542-548`).
  - `module_config_get()` drops private (`internal`) and obsolete options before any GUI sees them (`src/modules/modules.c:344-346`).
- **One plugin, one place.** A plugin's config set belongs to the whole plugin, not to each submodule (`src/modules/modules.c:325-329`).
- **The core layout** (`src/libvlc-module.c:1606-2881`) is one long descriptor:
  - Audio: 1610.
  - Video: 1670. Snapshots and window properties are sections at 1722 and 1738.
  - Subpictures / OSD: 1800.
  - Input: 1848. Track settings, playback control, devices, network, metadata and advanced are sections at 1851–1996.
  - Stream output: 2065.
  - Advanced: 2104.
  - Play Queue: 2145.
  - Interface: 2239.
  - Hotkeys: 2273–2815.
  - Hidden/CLI-only: 2818.

### 1.2 The three Qt views (`modules/gui/qt/dialogs/preferences/`)

The `Simple / All / Expert` radio buttons are in `preferences.cpp:74-84`. The view that opens first is set by `qt-initial-prefs-view` (`preferences.cpp:169-174`).

**Simple** (`simple_preferences.cpp`, `sprefs_*.ui`)
- 7 panels: Interface, Audio, Video, Subtitles / OSD, Input / Codecs, Hotkeys and Media Library (`simple_preferences.hpp:52-60`, `simple_preferences.cpp:257-264`).
- Each panel is a hand-made form. It binds options by name through `configBool` and `configGeneric` (`simple_preferences.cpp:285-349`). If an option doesn't exist, the control is disabled (`:293-298`).
- **84 option bindings in all.**
  - Video: `:383-437`.
  - Audio: `:445-585`.
  - Input & codecs: `:603-685`.
  - Interface: `:697-919`.
  - Subtitles: `:933-978`.
  - Hotkeys: `:985-1013`.
  - Media library: `:1024-1090`.
- Some controls combine several options:
  - One "caching policy" combo writes `file-`, `network-`, `disc-` and `live-caching` together (`:676-685`, `:1221-1224`).
  - One volume slider writes whichever volume or gain option belongs to the active audio output: `mmdevice-volume`, `alsa-gain`, `jack-gain`, `waveout-volume`, … (`:151-190`, `:1271-1302`).
  - The audio-filter checkboxes edit the `audio-filter` string (`:585`, `:1266`).
- **Some Simple-view settings are not VLC options at all.** They live in three other stores:
  - Interface language: the Windows registry `HKCU\Software\VideoLAN\VLC\Lang`, Windows only (`:703-722`).
  - Qt style, color scheme and interface scale: Qt `QSettings` / `MainCtx` (`:196`, `:765`, `:832-852`).
  - Media library folders: the medialibrary database (`:1035-1041`).

**All** (`complete_preferences.cpp`)
- Builds a tree from the core's subcategory markers (`:68-98`). It then attaches **one node per plugin**, under the first subcategory that plugin uses (`:100-152`).
- A plugin whose options span several subcategories therefore appears in only one place.
- `CAT_HIDDEN` and plugins without options are skipped (`:140`).

**Expert** (`expert_model.cpp`, `expert_view.cpp`)
- A flat table showing "Module :: label", type and value.
- It skips unknown and hidden subcategories and all hotkeys (`expert_model.cpp:254-260`).
- It has a text filter and a "Modified only" checkbox (`preferences.cpp:201-203`).

**Hotkeys**: the editor is a custom panel in the Simple view. Hotkeys are left out of Expert on purpose (`expert_model.cpp:256-258`).

## 2. Counts

All figures are option declarations found in `src/` and `modules/`.

| Measure | Count |
| --- | ---: |
| All `add_*` declarations | 2,078 |
| … obsolete (`add_obsolete_*`) | 84 |
| … hotkey entries (114 actions × local/global) | 228 |
| … private/volatile (never in the GUI) | 86 |
| … `SUBCAT_HIDDEN`, CLI-only (e.g. `bookmark1-10`) | 11 |
| … counted without a subcategory (mostly header macros, see [caveats](#method-and-caveats)) | 22 |
| **Shown in *All* / *Expert*** | **1,647** |
| … owned by the core (`src/`) or Qt (`gui/qt/qt.cpp`) | 240 |
| … owned by about 268 plugins | 1,407 |
| … with long help text (usable for the Figma info icons) | ≥1,267 (77%) |
| Bound in the *Simple* view | 84 (5% of 1,647) |
| Off-platform (macOS, iOS, Android, OS/2, Raspberry Pi MMAL) | 48 |
| skins2 + ncurses (dropped interfaces) | 9 |

By type, ignoring hotkeys:

| Type | Count |
| --- | ---: |
| int | 686 |
| string | 388 |
| bool | 387 |
| float | 178 |
| file | 50 |
| module selector | 24 |
| password | 23 |
| module list | 13 |
| directory | 7 |
| color | 7 |
| font | 3 |

By engine subcategory, visible options only:

| Subcategory | Count |
| --- | ---: |
| Input / Access modules | 256 |
| Video codecs (mostly encoders) | 174 |
| Video filters | 146 |
| Sout stream | 117 |
| Video output | 108 |
| Demuxers | 97 |
| Subtitles / OSD | 95 |
| Interface / Main interfaces | 84 |
| Audio filters | 83 |
| Access output | 63 |
| Input general | 60 |
| Subtitle codecs | 43 |
| Muxers | 43 |
| Video general | 42 |
| Visualizations | 36 |
| Splitters | 30 |
| Audio output | 29 |
| Audio codecs | 27 |
| Play Queue general | 25 |
| Advanced misc | 21 |
| Audio general | 14 |
| Control interfaces | 12 |
| Sout general | 9 |
| Services discovery | 6 |
| Hotkeys (non-key options) | 6 |
| Stream filters | 5 |
| Resampler | 4 |
| Interface general | 4 |
| Advanced network | 3 |
| Packetizers | 2 |

The plugins with the most options:

| Plugin | Options |
| --- | ---: |
| `codec/x264.c` | 76 |
| `access/dtv/access.c` (DVB) | 49 |
| `video_output/libplacebo/display.c` | 46 |
| `access/v4l2` | 45 |
| `gui/qt/qt.cpp` | 37 |
| `codec/avcodec` | 37 |
| `stream_out/transcode` | 26 |
| `video_splitter/panoramix.c` | 25 |
| `mux/mpeg/ts.c` | 22 |

## 3. Duplicates and inconsistencies

### 3.1 The same option declared in several plugins

The same name is declared again, usually with the same label. The *All* tree then shows it under each plugin.

| Option(s) | Declared in |
| --- | --- |
| `contrast`, `brightness`, `hue`, `saturation`, `gamma` | `video_filter/adjust.c:72-84`, `video_filter/gladjust.c:395-399`, `hw/d3d11/d3d11_filters.c:568-580`, `hw/d3d9/d3d9_filters.c:481-493` |
| `smb-user`, `smb-pwd`, `smb-domain` | `access/samba.c:449-451`, `access/smb2.c:108-110`, `access/dsm/access.c:83-85`, `access/unc.c:416-418` |
| `screen-fps`, `-top`, `-left`, `-width`, `-height` | `access/screen/screen.c:103-109`, `screen/xcb.c:76-86`, `screen/wayland.c:494-504` |
| 16 RIST and 5 SRT parameters | Shared by input and output: `access/rist.c:329-363` vs `access_output/rist.c:290-326`, and `access/srt.c:550-561` vs `access_output/srt.c:462-478` |
| Resampling quality, three ways | `soxr-resampler-quality` (`resampler/soxr.c:70`), `speex-resampler-quality` (`resampler/speex.c:47`), `src-converter-type` (`resampler/src.c:61`) |
| Visualization size, five plugins | `effect-width/height`, `glspectrum-*`, `projectm-*`, `vsxu-*`, `goom-*`. Labelled "Video width"/"Video height", the same labels as the core `width`/`height` |
| Output volume, one per audio output | `mmdevice-volume`, `waveout-volume`, `alsa-gain`, `jack-gain`, … (the Simple view hides this behind one slider) |

Within a single plugin, `visual.c` declares "Show 80 bands instead of 20" and "Draw peaks in the analyzer" twice each (`visualization/visual/visual.c:136/153` and `:138/159`).

### 3.2 The same label on different options, or different labels for the same idea

- **"Password" ×11, "Username" ×9, "Frame rate" ×11.** Without the plugin name these labels are ambiguous. The Expert view adds the plugin as a "Module ::" prefix for this reason (`expert_model.cpp:78`).
- **Frame rate** is worded four ways: "Frame rate" (11), "Frames per Second" (6), "Video input frame rate", "Subtitle Frames per Second".
- **User name** is worded two ways: "Username" (9) vs "SOCKS user name".
- **Capitalisation and spelling vary:**
  - "Aspect ratio" (4) vs "Aspect Ratio" (2).
  - "Sample rate" vs "Samplerate".
  - "Maximum bitrate" vs "Maximum Bitrate".
  - "Dry mix" vs "Dry Mix".
  - "Audio filter" vs "Audio filters".
  - "Video output module" vs "… modules".
  - "SRT Stream ID" vs " SRT Stream ID" (with a leading space).
- **Ranges are baked into labels:** "Image contrast (0-2)", "Image hue (-180..180)", "Motion threshold (10-100)", "Verbosity (0,1,2)", "Windows opacity between 0.1 and 1".
- **Numbers stand in for words.** The audit's example, `sub-autodetect-fuzzy`, takes a bare integer 0–4. Its meaning is only in the long text (`src/libvlc-module.c:866-873`, `:1819`).
- **Category names differ between views:**
  - "Hotkeys" (Simple) vs "Hotkeys settings" (subcategory, `cat.c:54`) vs "Hot keys" (core hint, `libvlc-module.c:2274`).
  - "Subtitles / OSD" sits under Video in *All* but is a top-level panel in *Simple*.
- **Hotkey wording differs:** "Very short forward jump" (the action) vs "Very short jump length" (the setting), `libvlc-module.c:2612` vs `:2765`.

### 3.3 Simple view vs All/Expert

- **42 of 65 labels differ.** These are the 65 Simple-view controls whose label can be compared with the option's own label. Examples:

  | Option | Simple view | Option label |
  | --- | --- | --- |
  | `play-and-pause` | "Pause on the last frame of a video" | "Play and pause" |
  | `spu` | "Enable subtitles" | "Enable sub-pictures" |
  | `dec-dev` | "Hardware-accelerated decoding" | "Preferred decoder hardware device" |
  | `avi-index` | "Damaged or incomplete AVI file" | "Force index creation" |
  | `spdif` | "Use S/PDIF when available" | "Force S/PDIF support" (the opposite meaning) |
  | `aspect-ratio` | "Force aspect ratio" | "Source aspect ratio" |
  | `freetype-color` | "Font color" | "Text default color" |
  | `secondary-sub-margin` | "Offset" | "Force secondary subtitle position" |

- **Bugs in the Simple view's wiring:**
  - Font size is bound to `freetype-rel-fontsize` (`simple_preferences.cpp:953`). That option has been obsolete since 4.0 (`text_renderer/freetype/freetype.c:198-199`). The real setting is now the core `sub-text-scale` (`libvlc-module.c:1830`).
  - `directx-audio-device` (`simple_preferences.cpp:494`) is not declared anywhere.
  - `snapshot-format` is wired to the aspect-ratio label (`:437`).
  - The x264 tune/preset controls are bound to the label reading "x264 profile and level selection", and profile/level to "x264 preset and tuning selection" (`simple_preferences.cpp:661-664` vs `sprefs_input.ui:33,124`).
- **One setting in two panels.** `metadata-network-access` sits in the Simple *Interface* panel, while the engine files it under Play Queue.

## 4. Draft distribution over the Figma categories

**Rules.** Core and Qt options are assigned one by one. Plugin options are assigned by subcategory and plugin, for example:
- Encoders → Recording.
- Decoders → Performance.
- Network access plugins → Network.
- Disc access → Playback.
- Subtitle codecs and the freetype renderer → Subtitles.
- Overlay filters such as logo, marquee and mosaic → Video.
- Loggers → General.

The full rules are in the method note at the end.

| Figma category | Options | Core + Qt | Plugin | Figma rows (basic + advanced) |
| --- | ---: | ---: | ---: | ---: |
| General | 29 | 14 | 15 (loggers, ntservice) | 19 |
| Interface | 35 | 34 | 1 | 26 |
| Privacy | 6 | 3 | 3 (last.fm) | 9 |
| Controls | 26 + 114 hotkey actions | 12 | 14 (gestures, lirc, CLI) | 16 |
| Playback | 54 | 37 | 17 (discs, AVI/MKV) | 19 |
| Video | 350 | 33 | 317 (filters, outputs, splitters, overlays) | 27 |
| Audio | 169 | 21 | 148 (filters, outputs, visualizations) | 29 + EQ panel |
| Subtitles | 94 | 21 | 73 (freetype, subtitle codecs) | 31 |
| Library | 19 | 12 | 7 (services discovery) | 7 |
| Network | 111 | 15 | 96 (protocols, TLS, adaptive streaming) | 9 |
| Performance | 36 | 14 | 22 (decoders) | 13 |
| Recording | 410 | 23 | 387 (encoders, muxers, sout) | 10 |
| Extensions | 4 | 1 | 3 (Lua) | 2 (+ add-on list) |
| *No home: capture devices* | 184 | 0 | 184 (DVB, v4l2, DirectShow, screen, DeckLink, SDI, JACK/ALSA/OSS capture, VNC/RDP) | 0 |
| *No home: demuxer-specific* | 63 | 0 | 63 (TS, MKV, MP4, raw, image, MOD, GME, …) | 0 |
| *Off-platform / dropped UIs* | 57 | 0 | 57 | 0 |
| **Total** | **1,647 + 114** | 240 | 1,407 | **≈217** |

**What the table shows:**
- **Figma has about 13% as many rows as there are options.** Four categories hold 63% of the options (Recording, Video, Audio and Network, 1,040 of 1,647), yet Figma gives them 75 of its ≈217 rows.
- **Nearly all of the long tail is per-plugin.** Under the Figma model it can only live in each category's "Advanced Options" area. Without grouping by plugin it would be a list of hundreds of rows.
- **Privacy, Library and Extensions are almost empty engine-side.** Their Figma rows are mostly new features or settings stored outside vlcrc.

## 5. Figma rows mapped to VLC options

Notation:
- `opt` is an existing option with the same shape.
- **≈** means the option exists but has a different shape (for example a bool where Figma has a dropdown, or one row spanning several options).
- **NEW** means no option exists today.
- **ACTION** means a button, not a setting.
- **EXT** means the setting lives outside vlcrc (registry, `QSettings`, medialibrary DB).

**General** (`999:5047`, `3046:9282`)

| Figma row | VLC |
| --- | --- |
| Interface language | EXT (Windows registry only; Linux uses the environment) |
| Theme Dark/Light/System | EXT (Qt color scheme in `QSettings`) |
| Accent color | `qt-accent-color` (`qt.cpp:480`) |
| Show confirmation and error dialogs | `qt-error-dialogs` |
| Check for updates automatically | `qt-updates-notif` (+ `qt-updates-days`; Windows/macOS builds only) |
| Developer mode | NEW |
| Save a log / Log file / Log details | `file-logging` / `logfile` / `log-verbose` (`logger/file.c:239-243`) |
| Show minor warnings | NEW, or ≈ `qt-error-dialogs` (overlaps the row above) |
| Additional command line options | NEW |
| Log format | `logmode` |
| Collect playback statistics | `stats` |
| Open configurations folder | ACTION |
| Reset all settings | ACTION (the existing *Reset* button) |
| Developer: syslog ×4 | `syslog`, `syslog-debug`, `syslog-ident`, `syslog-facility` (Linux only) |

**Interface** (`3046:10057`, `3062:11379`)

| Figma row | VLC |
| --- | --- |
| Use a custom skin / Skin file | `intf=skins2` / `skins2-last`. **Conflicts with the map: skins2 is out of scope** |
| Play video inside main window | `embedded-video` |
| Resize window to fit video (dropdown) | ≈ `qt-video-autoresize` (bool) |
| Remember window size and position | EXT (`QSettings` geometry) |
| Keep window above other windows | `video-on-top` |
| Show window decorations | ≈ `video-deco` (video window) and `qt-titlebar` (main window, client-side decorations) |
| Show the file name in the title bar | `qt-name-in-title` |
| Open fullscreen on | `qt-fullscreen-screennumber` |
| Start in compact mode | `qt-minimal-view` |
| Auto-hide the controls (dropdown) | ≈ `qt-pin-controls` (bool) |
| Hide after | `mouse-hide-timeout` (core, ms) |
| Show controls in fullscreen | `qt-fs-controller` |
| Show a system tray icon / Start minimized | `qt-system-tray` / `qt-start-minimized` |
| Popup on track change | `qt-notification` |
| Popup duration | NEW (`notify-timeout` exists only for the libnotify plugin) |
| Show the playlist as a tree | `playlist-tree` |
| Window opacity / Fullscreen controls opacity | `qt-opacity` / `qt-fs-opacity` |
| Pause video when minimized | `qt-pause-minimized` |
| Bring VLC to the front | `qt-auto-raise` |
| Show cover art when playing audio | `qt-bgcone` |
| File browser inside open dialog | `qt-embedded-open` |
| Allow the app icon to change | `qt-icon-change` |
| Fullscreen controls sensitivity | `qt-fs-sensitivity` |

Qt options that have no Figma row:
- `qt-menubar`
- `qt-close-to-system-tray`
- `qt-backdrop-blur`
- `qt-pip-mode`
- `qt-safe-area`
- `qt-smooth-scrolling`
- `qt-use-scroll-handler`
- `qt-compositor`
- `qt-bgcone-expands`
- `qt-max-volume` (Figma moves it to Audio)
- `qt-verbose`
- `qt-qmljsdebugger`
- `qt-initial-prefs-view` (the redesign removes the views)

**Privacy** (`3062:11911`)

| Figma row | VLC |
| --- | --- |
| Save recently played items | `save-recentplay` |
| Remember the playback position | `restore-playback-pos`. **Also appears in Playback as "Continue where you left off"** |
| Never remember items matching | NEW (`qt-recentplay-filter` is obsolete, `qt.cpp:395`) |
| Clear history now | ACTION |
| Fetch titles and cover art from the internet | `metadata-network-access`. **Also appears in Library** |
| Submit played tracks to last.fm | ≈ add `audioscrobbler` to `control`/`extraintf` |
| Last.fm username / password / URL | `lastfm-username`, `lastfm-password`, `scrobbler-url` (Figma labels the password row "username") |

**Controls** (`6536:14482`)

| Figma row | VLC |
| --- | --- |
| Edit Keyboard Shortcuts / Reset | The hotkey editor (114 actions × local/global) |
| Very short / short / medium / long skip | `extrashort-`, `short-`, `medium-`, `long-jump-size` |
| Single click / double click on the video | NEW |
| Scroll wheel / Horizontal scroll | `hotkeys-y-wheel-mode` / `hotkeys-x-wheel-mode` |
| Reverse scroll direction | ≈ the "Position control reversed" value of the wheel modes (`libvlc-module.c:1324-1328`) |
| Web remote control / Port | ≈ `extraintf=http` + `http-password` / `http-port` (the core HTTP port, shared) |
| Remotes: scroll rows | Copy-paste in Figma |

No Figma row: `qt-disable-volume-keys`, `mouse-events`, `keyboard-events`, `mouse-pause-policy`, gestures, lirc, telnet and CLI (`rc-*`), `dbus`.

**Playback** (`3062:12552`, `3072:13408`)

| Figma row | VLC |
| --- | --- |
| Continue where you left off | `restore-playback-pos` (duplicate of Privacy) |
| Start playing as soon as a file opens | `playlist-autostart` / `start-paused` |
| Default playback speed | `rate` |
| Pause when another app uses audio | `playlist-cork` |
| Preferred video quality | `preferred-resolution` |
| Preferred menu language | `menu-language` |
| Default audio / subtitle track | `audio-track` / `sub-track` (VLC also has `*-track-id` and `*-language`) |
| Repeat / Shuffle | `loop` + `repeat` / `random` |
| Stop after each item | `play-and-stop` |
| When playback finishes | ≈ `play-and-exit` / `play-and-pause` |
| Seek precisely | ≈ `input-fast-seek` (inverted) |
| Show thumbnail while scrubbing | NEW |
| Default optical drive | `dvd` / `vcd` / `cd-audio` (one Simple-view control writes all three) |
| Repair damaged AVI files | `avi-index` |
| Load matching .mkv files | `mkv-preload-local-dir` |
| Time-shift folder / size | `input-timeshift-path` / `input-timeshift-granularity` |

**Video** (`3073:14187`, `3073:15375`)

| Figma row | VLC |
| --- | --- |
| Enable video / Start in fullscreen / Stop screensaver / Wallpaper | `video` / `fullscreen` / `disable-screensaver` / `video-wallpaper` |
| Hue, Brightness, Contrast, Saturation, Gamma, Reset | ≈ `adjust` filter options + `video-filter=adjust` (the same names exist in 4 plugins) |
| Deinterlacing / Method | `deinterlace` / `deinterlace-mode` |
| Aspect ratio / Crop / Scale to window / Zoom / Position | `aspect-ratio` / `crop` / `autoscale` / `zoom` / `align` |
| Snapshots: Save to, prefix, format, sequential, preview | `snapshot-path`, `snapshot-prefix`, `snapshot-format`, `snapshot-sequential`, `snapshot-preview` |
| Snapshot size (dropdown) | ≈ `snapshot-width` + `snapshot-height` |
| Custom aspect ratios / crop ratios / monitor PAR | `custom-aspect-ratios` / `custom-crop-ratios` / `monitor-par` |
| Time-shift buffer size | Copy-paste from Playback |

No Figma row:
- Core: `grayscale`, `video-stereo-mode`, `width`/`height`/`video-x`/`video-y`, `hdtv-fix`, `fit`, `deinterlace-filter`, `vout`, `video-filter`.
- Plugins: about 317 options (filters, libplacebo, splitters).

**Audio** (`3073:15879`, `3180:17398`)

| Figma row | VLC |
| --- | --- |
| Enable audio | `audio` |
| Output device | ≈ `aout` + the per-output `*-audio-device` |
| Speaker setup | ≈ `stereo-mode` / `mix-mode` |
| Maximum volume | `qt-max-volume` |
| Startup volume | ≈ `volume-save` + the per-output volume/gain |
| Volume step | `volume-step` |
| Normalize loudness / Target level | ≈ `audio-filter+=normvol` / `norm-max-level` |
| Night mode | NEW (could be a compressor preset) |
| Replay gain / preamp | `audio-replay-gain-mode` / `audio-replay-gain-preamp` |
| Equalizer / preset / bands / two-pass | ≈ `audio-filter+=equalizer`, `equalizer-preset`, `equalizer-bands`, `equalizer-preamp`, `equalizer-2pass` |
| Spatial audio / mode | ≈ the `spatializer` *or* `headphone` *or* `spatialaudio` filter (three plugins) |
| Stereo widening / Compressor / Chorus-flanger / Karaoke | ≈ enabling the `stereo_widen` / `compressor` / `chorus_flanger` / `karaoke` filter |
| Pitch shift | `pitch-shift` (scaletempo_pitch) |
| Preferred audio language / Audio delay | `audio-language` / `audio-desync` |
| Visualizer | `audio-visual` |
| Pass compressed audio | `spdif` |
| Detect Dolby Surround | `force-dolby-surround` |
| Resampling quality | ≈ three resampler-specific options (§3.1) |
| Media role | `role` |
| Visualization size | ≈ five plugin-specific width/height pairs (§3.1) |

**Subtitles** (`3180:17989`, `6536:13908`)

| Figma row | VLC |
| --- | --- |
| Enable subtitles / Preferred language / Text encoding | `spu` / `sub-language` / `subsdec-encoding` |
| Load subtitle files automatically | `sub-autodetect-file` |
| File name matching | `sub-autodetect-fuzzy` (the integer 0–4 becomes words, as the audit asks) |
| Font / Bold / Text color / Text opacity | `freetype-font` / `freetype-bold` / `freetype-color` / `freetype-opacity` |
| Size values Relative/Pixels + Font size | ≈ `sub-text-scale` only. **Pixels is NEW**, since `freetype-fontsize` is obsolete (`freetype.c:198`) |
| Outline thickness / color | `freetype-outline-thickness` / `freetype-outline-color` |
| Shadow / color / distance | ≈ `freetype-shadow-opacity`>0 / `freetype-shadow-color` / `freetype-shadow-distance` |
| Background / color / opacity | ≈ `freetype-background-opacity`>0 / `freetype-background-color` / `freetype-background-opacity` |
| Vertical position (dropdown) | ≈ `sub-margin` (pixels) |
| Subtitle delay | `sub-delay` |
| Show feedback for shortcuts | `osd` |
| Feedback duration | NEW |
| Show filename when a video starts / for | `video-title-show` / `video-title-timeout` |
| Load subtitles from these folders | `sub-autodetect-path` |
| Ignore styling in ASS/SSA | NEW (libass only has `ssa-fontsdir`) |
| Shadow angle / Text direction / Monospace font | `freetype-shadow-angle` / `freetype-text-direction` / `freetype-monofont` |
| Visualization size | Copy-paste from Audio |

**Library** (`6542:15251`)

| Figma row | VLC |
| --- | --- |
| Only allow one VLC window / from file manager / enqueue | `one-instance` / `one-instance-when-started-from-file` / `playlist-enqueue` |
| When opening a folder | `recursive` |
| Show hidden files | `show-hiddenfiles` |
| Fetch titles and artwork (dropdown) | ≈ `metadata-network-access` (+ `auto-preparse`). Duplicate of Privacy |
| Use the media library | `media-library` |

No Figma row: `ignore-filetypes`, `preparse-*`, `fetch-art-threads`, `services-discovery`, `podcast-urls`, `album-art-filename`, and the media library folders (EXT).

**Network** (`6543:15649`)

| Figma row | VLC |
| --- | --- |
| Buffering preset + Local files / Discs / Network / Live | `file-caching`, `disc-caching`, `network-caching`, `live-caching`. The preset already exists in the Simple view |
| HTTP proxy / password | `http-proxy` / `http-proxy-pwd` |
| Verify certificates against system store / Cipher preference | `gnutls-system-trust` / `gnutls-priorities` |

No Figma row: `mtu`, `ipv4-timeout`, the HTTP/RTSP server host and ports, TLS cert/key, SOCKS, `rtsp-tcp`, plus about 96 protocol options.

**Performance** (`6571:15958`)

| Figma row | VLC |
| --- | --- |
| Hardware-accelerated decoding | `dec-dev` (+ `hw-dec`) |
| Video post-processing | `postproc-q` |
| Skip the H.264 loop filter | `avcodec-skiploopfilter` |
| Decoding threads | `avcodec-threads` |
| Drop frames when late | `drop-late-frames` |
| Real-time priority / Process priority | `high-priority` (bool); the numeric priority is NEW |
| Rescan plugins | ACTION (`reset-plugins-cache` is CLI-only) |
| Encoder / Decoder | ≈ none / `codec` |
| Clock synchronisation / jitter / reference averaging | `clock-synchro` / `clock-jitter` / `cr-average` |

**Recording** (`6571:16429`)

| Figma row | VLC |
| --- | --- |
| Save recordings to | `input-record-path` |
| Record without re-encoding | `input-record-native` |
| Discover cast devices | ≈ renderer discovery (not an option) |
| Cast quality | `sout-chromecast-conversion-quality` (macro, `stream_out/renderer_common.hpp:56`) |
| Play locally while streaming | `sout-display` |
| Include audio / video / subtitles | `sout-audio` / `sout-video` / `sout-spu` |
| Encoding preset / Tuning | `sout-x264-preset` / `sout-x264-tune` |

**Extensions** (`6575:17137`)

| Figma row | VLC |
| --- | --- |
| Add-on list, Find more, Add, Remove | The addons manager (not options) |
| Load add-ons on startup | `qt-autoload-extensions` |

## 6. Options that don't fit

1. **Capture devices (184).** DVB/DTV (49), v4l2 (45), DirectShow (19), screen capture (21), DeckLink, SDI, JACK/ALSA/OSS/WASAPI capture, shm, VNC/RDP. Figma has no "Devices" or "Capture" category. These are settings for the Open Media → Capture flow; [#19](https://github.com/floklein/jvlc/issues/19) and [#20](https://github.com/floklein/jvlc/issues/20) may be a better home than Settings.
2. **Demuxer-specific options (63).** TS, MKV (other than preload), MP4, raw audio/video, image, MOD, GME, ES. These could go in Performance or Playback → Advanced, grouped by plugin.
3. **Per-media and command-line options shown as preferences.** `start-time`, `stop-time`, `run-time`, `input-repeat`, `input-list`, `input-slave`, `bookmarks`, `program(s)`, `open`, `meta-*` (8 stream metadata setters), `sub-file`, `video-title`, `video-x/y`. These make sense on the command line, not as persistent settings. They are candidates for hiding.
4. **Interface module selection.** `intf`, `extraintf`, `control`, plus the Lua HTTP, telnet and CLI interfaces and the Windows service `ntservice-*`. In the redesign these turn into features ("Web remote", "last.fm"), not module lists.
5. **Video splitters (30).** Wall, panoramix, clone. They are video output, but niche.
6. **Visualization tuning (36).** Spectrum, projectM and goom parameters.
7. **Hotkey actions (114).** These belong in the separate shortcuts dialog the audit asks for ([#21](https://github.com/floklein/jvlc/issues/21)), not as rows.
8. **Developer and debug options.** `qt-qmljsdebugger`, `qt-verbose`, `ml-verbose`, `tracer`, `mft-debug`, `avcodec-debug`, `color`. The Figma General "Developer Options" section could hold them.

## 7. Inconsistencies inside the Figma frames

Found while extracting the frames:
- "Show a popup when the track change**w**" (Interface).
- "Last.fm username" appears twice; the second should be the password (Privacy).
- "**i**nclude subtitles" is lowercase (Recording).
- "Time-shift buffer size" appears in Playback and again in Video Advanced.
- "Visualization size" appears in Audio and again in Subtitles Advanced.
- Remotes repeats the three Mouse rows (Controls).
- Playback "Continue where you left off" and Privacy "Remember the playback position" set the same option. Privacy and Library "Fetch titles…" also set one option.
- Global "Show minor warnings" overlaps "Show confirmation and error dialogs".
- The sidebar says "General", but the page header says "Global Settings".
- No settings frame designs profiles, and no frame designs the info-icon popover.

## 8. Open questions this raises

- **Where do capture-device settings go?** That is 184 options with no Figma home. The candidates are a "Devices" settings category, the Open → Capture dialog, or plugin-level Advanced.
- **What is the long tail's shape under "Advanced Options"?** Choices:
  - A per-category list grouped by plugin.
  - One global "All settings / plugins" page, like today's *All*/*Expert* views.
  - Search only.

  The audit says plugins should live in advanced settings. The redesign also drops the separate *All* tree.
- **Should Theme, language, window geometry and media library folders stay outside vlcrc?** This bears on profiles ([#11](https://github.com/floklein/jvlc/issues/11)) and persistence ([#4](https://github.com/floklein/jvlc/issues/4)). A profile that "changes all your settings" has to switch the registry, `QSettings` and vlcrc together.
- **Skins in Figma vs skins2 out of scope.** Figma Interface → Skin contradicts the map's out-of-scope list.
- **Composite rows.** Several Figma rows drive more than one option or a filter chain: Buffering, Startup volume, Equalizer, the Picture sliders, Output device, Resampling quality, Visualization size. Each needs a defined write-through, as the Simple view already does for caching and volume.
- **Where does the info-icon text come from?** At least 77% of the visible options have a long text, and 219 of the 240 core and Qt options do. Is that text reused, or rewritten in "human words"?

## Method and caveats

- **Scanner.** A Node script walked every `.c/.cpp/.m/.h` file in `src/` and `modules/` for `add_*` macro calls. It tracked `vlc_module_begin`, `set_subcategory`, `set_section` and `change_private`/`change_volatile`, and resolved label macros (`#define FOO_TEXT N_("…")`) within the same file.
- **Headers count once.** Options declared in header macros were counted once, at the header, even when several plugins expand them: `audio_output/volume.h`, `libplacebo/utils.h`, `opengl/gl_scale.h`, `stream_out/renderer_common.hpp`, `access/avio.h`. They make up most of the 22 options counted with no subcategory. Totals are therefore exact for declarations but may undercount instantiated options by about 1–2%.
- **Conditional compilation.** Plugins compiled several ways (x264 / x262 / x264-10bit) were counted once, and `#ifdef` alternatives were counted once each.
- **Labels.** About 190 labels could not be resolved statically because they use prefixes defined in other headers. They are still counted.
- **Category rules.** The Figma mapping assigns each core and Qt option by hand. Plugin options are assigned by subcategory and path:
  - `SOUT_*` and encoder files → Recording.
  - `AUDIO_*` → Audio.
  - `VIDEO_*` → Video, except freetype, subsdelay and sapi → Subtitles.
  - `INPUT_SCODEC` → Subtitles.
  - Network access, adaptive streaming, RTP and live555 → Network.
  - Disc access → Playback.
  - Decoders → Performance.
  - Loggers → General.
  - netsync and gnutls → Network.
  - `PLAYLIST_SD` → Library.
  - audioscrobbler → Privacy.
  - gestures, lirc and CLI → Controls.
- **Off-platform.** Paths for macOS, iOS, Android, OS/2 and MMAL.
- **Figma.** Rows were read from the node tree (`get_metadata`) and checked against screenshots of the 19 settings frames.
