# Figma coverage of the audit

Research for [#2](https://github.com/floklein/jvlc/issues/2), map [#1](https://github.com/floklein/jvlc/issues/1).

**Question.** Which features and surfaces in `AUDIT.md` (the video transcript) are fully drawn, partially drawn, or missing in the Figma file? For each one, list the Figma node ids and what stays ambiguous.

## Method and sources

The only primary source is the Figma file `QRb0F0V7fSTZCIoOyeV0Km` (single page `0:1`), read on 2026-10-12 through the Figma MCP:

- `use_figma` read-only scripts listed every top-level node on the page, the variants of every component set, and every visible text layer. Hidden layers were reported separately.
- `get_screenshot` rendered each frame and component cited below. Statements about what a frame *shows* come from those renders.
- Statements about what the redesign *intends* come from the transcript in `AUDIT.md`.

The page has **105 top-level nodes**. `AUDIT.md`'s node inventory lists all of them except three small components (`177:98` Icon Wrapper, `848:1712` play/pause button, `2245:9805` "100 %" audio value label) and two canvas captions ("Components", "Designs"). **No screen is hidden off-inventory.** Anything missing below is missing from the file, not just from the inventory.

Several surfaces sit inside component sets, not top-level frames: the right-hand player sidebars are variants of `491:1780` Controls, and the compact library views are variants of `848:1996` sidebar. Those variant ids are the useful ones:

| Component set | Variant → node id |
| --- | --- |
| `491:1780` Controls (control bar + right sidebar overlay) | default `491:1779`, hover/alt `2151:9605`, waveform seek bar `2139:8124`, music `4778:26181`, queue `491:1781`, bookmarks `6320:13763`, chapters `6321:14024`, subtitle tracks popover `491:2216`, audio tracks popover `496:2495`, video tracks popover `498:2674`, sidebar Playback tab `2238:9432`, sidebar Video tab `498:2937`, sidebar Audio tab `536:1059`, sidebar Subtitles tab `763:1958`, subtitle timeline seek bar `3744:12361` |
| `848:1996` sidebar (compact library) | default/folder `848:1995`, home `1106:5633`, videos `848:1997` / `4778:25847` / `4778:25957`, music `858:5001` / `4767:23606` / `4778:24990`, network `858:5294`, playlists `858:5374` |
| `970:3091` Miniplayer | collapsed `970:3090` `975:4333` `1919:9515`, with queue `970:3092` `975:4377` |

Legend: **Full** means the surface and its main states are drawn. **Partial** means the surface exists but key states, sub-screens, or controls the transcript names are absent. **Missing** means nothing is drawn beyond, at most, a menu label or settings toggle.

## Coverage table

### Features the ticket flags as suspected missing

| Audit feature | Coverage | Figma node ids | What is drawn / what stays ambiguous |
| --- | --- | --- | --- |
| Home page | **Full** | `6261:15078` (full window), `1106:5633` (compact sidebar) | Drawn, contrary to the suspicion. Sections: Continue Watching, Listen Again, Frequently Visited Folders, Frequently Visited Playlists, Frequently Listened Artists, Latest Streams. The transcript's "latest files" became *frequently visited* and *continue watching*. Ambiguous: section ordering and limits, the "›" see-all destinations, the empty/first-run state, and whether Home is the landing view on launch. `AUDIT.md` files `6261:15078` under "Player"; it is the Home screen. |
| Recent | **Missing** (nav label only) | rail/nav entry in `970:3308` etc.; menu "Open Recent ›" in `6062:13354` | "Recent" sits in the nav rail and the main menu, but no Recent view and no Open Recent submenu exist. The sidebar component set has no `recent` variant. Settings → Privacy `3062:11911` has "Save recently played items" and "Clear history now". |
| Pinned items | **Missing** | "+ Add item" nav entry in `970:3308`, `6261:15078`, rail "+" in `187:328` | The "+ Add item" entry presumably adds pins. No pinned entry, pin flow, or unpin action is drawn, and no context menu has "Pin". Only "Favorite" appears (`2404:13340`, `2459:9389`), and its relation to pinning is undefined. |
| Convert | **Missing** (menu label only) | "Convert Ctrl+R" in `6062:13354`; "Convert" in file context menu `2404:13340` | No Convert dialog. |
| Stream to another device | **Missing** (labels + settings only) | "Stream" in `2404:13340`; Settings → Recording `6571:16429` (Casting, Streaming) | No Stream/Cast dialog, and the main menu `6062:13354` has no Stream entry, though the transcript lists "stream to another device" among the menu's options. Settings cover defaults: cast discovery, cast quality, play locally while streaming, encoding preset. |
| Capture device | **Missing** (menu label only) | "Open Capture Device Ctrl+C" in `6062:13354` | No dialog. The Ctrl+C shortcut clashes with the platform copy shortcut. |
| Clipboard | **Missing** | none | Not in any menu, dialog, or setting. The transcript names it among the menu options to keep. |
| Bookmarks & chapters panel | **Partial** (panel full; shortcut missing) | `6320:13763` bookmarks, `6321:14024` chapters (tabs beside Queue `491:1781`); customize items "Bookmark", "Bookmarks", "Chapters" in `2107:5604` | Right sidebar with tabs Queue / Bookmarks / Chapters. Bookmarks list name + timestamp with "−" / "+". Chapters list number, title, and time. Not drawn: the quick-bookmark shortcut (transcript: Ctrl+B), rename/edit of a bookmark, bookmark or chapter marks on the seek bar, and an empty state. |
| Subtitle timeline | **Partial** | `3744:12361` (seek bar mode); toggle "Show Subtitle Timeline on Seekbar" in `2238:9432` | The seek bar turns into a cue-block timeline with `‹ − … + ›` controls and `00:01:50*` / `00:02:40*` times. The interaction is not spelled out: whether "−/+" shift the delay or zoom, what "‹ ›" and the asterisks mean, and whether cues are dragged directly. No full-player frame shows it in context. |
| Sleep timer | **Partial** | "Sleep Timer: Off ▾" in `2238:9432` | One dropdown row. No option list (the sibling Audio Visualizer dropdown has one, `2372:8639`), no countdown or active-timer indicator, and no expiry behavior. |
| Full-window library with mini player | **Full** (drag behavior undrawn) | `970:3308` (list, mini bottom-right), `970:3792` (list, mini bottom-left), `970:2703` (grid + mini), `970:4028` (grid, no mini); Miniplayer `970:3091` with queue variants `970:3092`, `975:4377` | Breadcrumb address bar, back/forward, sort, view toggle, search, "+" and "…" menus. The two corner positions imply a movable mini player, but dragging and snapping are not drawn. Not drawn: how you enter this mode (probably the "↗" pop-out icon in the compact sidebar header, `848:1995`) and how you return to the video. |
| Music player view | **Partial** | `4767:23726` (sidebar closed), `4778:25123` (sidebar open), control bar `4778:26181` | Large cover art on a blurred backdrop, title and artist, waveform seek bar, and ±skip buttons. Songs / Albums / Artists / Genres exist only as category entries (`858:5001`) plus one Songs folder list (`4778:24990`). No Albums, Artists, or Genres views and no full-window music library are drawn. |

### Player

| Audit feature | Coverage | Figma node ids | Notes / ambiguity |
| --- | --- | --- | --- |
| Control bar: play left with prev/next, thick seek bar, cone volume, right-side track/settings/queue icons | **Full** | `491:1779`, `2151:9605`, `244:184` (volume 0–200 %), `848:1712`, frames `187:328`, `247:445` | |
| Top bar: menu "…", library-sidebar toggle, current-folder button, title, window controls | **Full** | every player frame, e.g. `247:445`, `187:328` | Fullscreen sits at the top right next to the window buttons. The transcript leaves top versus bottom open, and alternate layout `2160:8301` puts it at the bottom. |
| Auto-hide top/bottom bars on hover, keep title in fullscreen | **Partial** | `3328:11554` (UI hidden) | Only the hidden end state is drawn. Hover zones, timings, and a fullscreen frame are not. Settings → Interface `3062:11379` has "Auto-hide the controls" and "Hide after". |
| Hide-UI option (right-click + shortcut) | **Full** | `6065:13616` ("Hide UI Ctrl+\\"), `3328:11554` | |
| Seek thumbnail | **Full** | `2151:9673`, `1904:7238`, `1905:7312`; setting "Show thumbnail while scrubbing" in `3072:13408` | |
| Waveform seek bar | **Partial** | `2139:8124`, `4778:26181` | Drawn as a seek-bar variant, but the "show waveform on the seek bar" toggle the transcript puts in the Playback tab is absent. The Audio Visualizer dropdown (`2372:8639`) has a "Waveform" entry, which may or may not be the same thing. |
| Subtitle track popover | **Full** | `491:2216` | Embedded and external tracks, with "Show Preferences". |
| Audio track + output device popover | **Full** | `496:2495` | Tracks plus Speakers/Headphones. |
| Video tracks as a third tab (no bar icon) | **Full** | `498:2674` | |
| Right sidebar: Playback / Video / Audio / Subtitles tabs | **Full** | `2238:9432`, `498:2937`, `536:1059`, `763:1958`; sections `536:1513`, `6114:13610`, `536:1703`, `612:1223`, `536:1855`, `543:990`, `614:1430` | |
| Subtitle timing sliders, font, size relative/precise, outline, position | **Full** | `763:1958` | Delay, speed, and duration factor. Font, colors, offset, "Size Values: Relative / Pixels", and outline. |
| Per-show presets (save/select) | **Partial** | `2372:8834` (preset dropdown with "New Preset"); "Default Preset" footer in each tab | Selection is drawn. The save/name flow and the rules for which settings a preset captures are not. |
| Default subtitle/audio track saved in preset | **Full** | `763:1958` (Subtitle Track/Language), `536:1059` (Audio Track/Language) | Whether these are per-preset or global is behavior, not visuals. |
| Audio filters with friendly presets + preview icons | **Partial** | `2372:8487` (EQ presets with curve icons), `6114:13610`, `3178:16769` | Static preview glyphs only. Live preview (map: *Not yet specified*) is not drawn. |
| Video filters with previews; flip/rotate/zoom/HDR/resolution/aspect | **Partial** | `498:2937`, `536:1855`, `543:990`, `614:1430`, `2372:8733` | The defaults block is full. Video filter presets with preview thumbnails are not drawn: only an "Essential" collapsible and "Expand Settings". |
| Playback tab: speed, reverse, sleep timer, subtitle timeline, waveform, metadata | **Partial** | `2238:9432` | Has speed slider, Reverse toggle, Sleep Timer, subtitle-timeline toggle, Show Statistics, and Audio Visualizer. "Show metadata" and the waveform toggle are absent. Reverse playback has no libVLC support today, so its feasibility needs research. |
| Continue-watching prompt near the center controls | **Full (hidden layer)** | `1919:9580` inside `247:445`; copies in `3328:11554`, `2425:13624`, `4767:23726`, all customize frames | A 552×52 bar centered above the control bar: "You already watched till 00:04:80. [Continue Watching] ✕". The layer is hidden, so screenshots don't show it; toggle visibility to review. ("00:04:80" is a typo.) |
| Queue (drag files in, add icons) | **Full** (visuals) | `491:1781`, `970:3092` | Drag-and-drop is behavior. |
| Record button | **Full** | `6065:13616`, customize palette `2107:5604` | |
| Right-click menu cleaned up | **Partial** | `6065:13616` | Top level only. The Playback, Video, Audio, Subtitle, More Settings, Playlist, and Open Media submenus are not drawn. |
| OSD showing the pressed shortcut | **Missing** (setting only) | "Show feedback for shortcuts" in `3180:17989` | No OSD toast is drawn. |
| Picture-in-picture | **Missing** (button only) | "Picture in Picture" in customize palette `2107:5604` | No PiP window. The map lists this under *Not yet specified*. |
| Light theme | **Partial** | `2425:13624`; onboarding theme picker `1902:3670`…`1902:6465`; Settings → General theme switch `999:5047` | Only one light player frame. Library, Home, settings, dialogs, menus, and sidebars have no light versions. |

### Library

| Audit feature | Coverage | Figma node ids | Notes / ambiguity |
| --- | --- | --- | --- |
| Compact library in the player (sidebar) | **Full** | `187:328`, `2425:13624`, `4778:25123`, `848:1996` variants | |
| Categories: Videos, Music, Network, Playlists | **Full** | `848:1997`, `858:5001`, `858:5294`, `858:5374` | |
| Back/forward + clickable address line | **Full** | `970:3308`, `848:1995` | |
| Sort menu incl. watched/unwatched first | **Full** | `856:2215` | |
| View modes: grid / list / detail | **Partial** | `858:3290` | Grid and List only. The transcript's "detail" view is absent. |
| "…" menu (add file/folder here) | **Partial** | `2107:5903` (placeholder "Option" ×6), `2139:8935` (folder: Add File, Add Folder, Edit Subtitle/Audio Tracks, Resync, Info) | The generic menu is unfilled, so which "…" belongs to which view is unclear. |
| Watched / unwatched state | **Full** | `2404:9167`, progress underline in `970:2703` | |
| File / playlist context menus | **Full** | `2404:13340`, `2459:9389` | "Add to Playlist" has no picker drawn. "Go to Source" and "Resync" are undefined. |
| Network streams (defaults + add) | **Full** | `858:5294`, `6216:13614` | |
| Import remote stream list as an automatic playlist | **Partial** | `6216:13614` (multi-URL field + optional Playlist Name) | The dialog accepts one or more links and a playlist name. The resulting auto-playlist and its refresh behavior are not drawn. |
| New playlist (name + select files) | **Full** | `6259:14954`, "Create playlist" in `858:5374` | Adds a thumbnail picker. |
| Add to playlist from elsewhere | **Partial** | "Add to Playlist" in `2404:13340` | No target picker. |

### Dialogs, menus, settings

| Audit feature | Coverage | Figma node ids | Notes / ambiguity |
| --- | --- | --- | --- |
| Main menu (replaces top menu) | **Full** | `6062:13354` | Open File(s), Open Folder, Network Stream, Disc, Capture Device, Convert, Open Recent, Profile, Keyboard Shortcuts, Customize Player, Settings, About. Shortcut clashes: Ctrl+F for Open Folder against find, Ctrl+C for Capture against copy. |
| Separate open dialogs for each source | **Partial** | Network: `6216:13614` | Only Open Network Stream is drawn. Disc, Capture Device, Convert, Stream, and Clipboard dialogs are missing. Open File(s) and Folder are presumably native pickers. |
| Add subtitles: to file / folder / playlist, from computer / OpenSubtitles | **Partial** | `1911:7545`, `3336:11929` (folder, from computer, auto-matched), `1919:8707` (file, from computer, multi-select), `1919:9278` (file, from OpenSubtitles + language); `1959:5325`, `1963:5465`, `1915:8397`, `1919:9121` | Folder and file are drawn. "To This Playlist" and "From Link…" exist only as dropdown items. The manual re-match of a wrong pair is not shown. `1911:7545` and `3336:11929` render identically. |
| Keyboard shortcuts dialog | **Full** | `2437:14832`; "Edit Keyboard Shortcuts" in `6536:14482` | |
| Settings: categories, basic → advanced, info icons, search | **Full** | 17 frames listed in `AUDIT.md`; components `3021:9835`, `804:1208`, `3176:16556` | General, Interface, Privacy, Controls, Playback, Video, Audio, Subtitles, Library, Network, Performance, Recording, Extensions. Advanced is drawn expanded for General, Interface, Playback, Video, Audio, Subtitles, and Performance only. |
| Accent color, light/dark theme | **Full** | `999:5047`, onboarding `1902:4020`…`1902:6465` | |
| Subtitle file-name matching in words | **Full** | `6456:13892`, `3180:17989` | |
| Extensions vs plugins split | **Partial** | `6575:17137` (Add-ons list, Find more, Add, Remove); "Rescan plugins" in `6571:15958` | The transcript puts plugins under Extensions → Advanced, but that section is never drawn expanded. |
| Customize player (drag palette) | **Full** | `2107:5604`, `2160:6137`, `2160:6564`, `2160:7005`, `2160:7437`, `2160:7869`, `6587:19923` | The drag gesture itself is behavior. |
| Alternate layout | **Full** | `2160:8301`, `2160:8732` | |
| Layouts per profile | **Partial** | `6648:20666` (Profile submenu: Movies, Shows, Music, Work, + New) | Switching is drawn. The "New" profile flow and what a profile stores are not. |
| Onboarding: theme + shortcut intro | **Full** | `1899:3055` (welcome + media folder), `1902:3670`, `1902:4020`…`1902:6465` (theme × accent), `1902:6770` (shortcuts) | Shortcut conflicts: onboarding shows Record = Shift+R and Prev/Next = `<` `>`, while the right-click menu shows R and P/N. |
| Custom title bar / window decorations | **Partial** | every frame's top bar | Drawn throughout, but Settings → Interface `3062:11379` still has "Show window decorations", so native versus custom is undecided. |

## Summary

- **Fully drawn:** player shell and control bar, track popovers, the four settings tabs in the player sidebar, compact library in all categories, Home, full-window library with mini player, sorting, playlists, network stream dialog, settings in all 13 categories, keyboard shortcuts, customization, onboarding.
- **Partially drawn:** music library beyond Songs, subtitle timeline semantics, sleep timer, waveform and metadata toggles, presets save flow, video filter previews, right-click submenus, add-subtitles to playlist/from link, profiles creation, light theme, title-bar decisions, extensions/plugins advanced view, detail view mode, add-to-playlist picker, automatic stream playlist.
- **Missing:** Recent view and Open Recent submenu, pinned items, Convert / Stream / Capture / Disc / Clipboard dialogs, quick-bookmark shortcut, OSD shortcut toast, PiP window.

### Surprises versus the ticket's suspicion list

- Home page, the bookmarks & chapters panel, and the full-window library with mini player **are drawn**.
- The continue-watching prompt exists but is a **hidden layer** (`1919:9580`), easy to miss in screenshots.
- Clipboard and Stream-to-device have **no menu entry at all** in the redesigned menu, not just a missing dialog.

## Open questions this surfaced

1. Can libVLC play in reverse? The Playback tab has a "Reverse" toggle.
2. Keyboard shortcuts disagree across frames: Record R vs Shift+R; Prev/Next P/N vs `<`/`>`. Ctrl+C and Ctrl+F clash with copy and find.
3. Does the subtitle timeline shift the delay, zoom, or both? Are the cues draggable?
4. Pinned items vs "Favorite": the same concept or two?
5. Native or custom window decorations? A custom bar is drawn everywhere, but the native toggle remains in settings.
