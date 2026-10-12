# Can the Qt theme system express the Figma design system?

Research for [#5](https://github.com/floklein/jvlc/issues/5) (map [#1](https://github.com/floklein/jvlc/issues/1)). It feeds the design-system grilling, [#8](https://github.com/floklein/jvlc/issues/8).

Sources:
- Figma file `QRb0F0V7fSTZCIoOyeV0Km`, read through the Figma MCP (read-only plugin scripts, `get_variable_defs`, screenshots).
- The Qt module at `modules/gui/qt/` on `feat/redesign`. Code paths below are relative to that directory.

## Answer

**Mostly yes.** Colors, light/dark switching, icon size and stroke weight, and the radius and spacing values all fit the existing mechanisms: `SystemPalette` → `ColorContext` for colors, and `VLCStyle.qml` for metrics. They need re-authoring, not new architecture. Five things need extension:

1. A bundled text font (Inter, plus DM Mono for key caps).
2. A derived accent ramp, so `qt-accent-color` drives the six accent tokens instead of two.
3. Radius tokens, and spacing and type-scale values that VLCStyle lacks.
4. Translucent blurred surfaces over video.
5. A two-tone icon component.

**Two-tone icons can stay on the icon-font pipeline.** Split each two-tone icon into two single-color glyphs and stack them in a small QML component that colors each layer from the `ColorContext`. A font glyph cannot carry two colors by itself. The existing SVG placeholder recolorer (`image://svgcolor`) is a working fallback, but it rasterizes per color and size. Either way, the Figma icons must first have their strokes outlined and overlaps flattened, because the font generator imports fills only.

## 1. The Figma design system

### Where tokens live

The file has **no variables** (one empty collection) and **no text styles**. Its tokens are **49 paint styles** named `Dark/...` and `Light/...`, plus **2 effect styles**. Theme is part of each style name, not a variable mode, so every themed node is bound to a Dark or a Light style directly. `get_variable_defs` reports these paint styles as if they were variables.

### Colors

| Token | Dark | Light | Typical use |
| --- | --- | --- | --- |
| BG/A1 | `#222222` | `#EDEDED` | selected setting category |
| BG/A2 | `#1F1F1F` | `#DDDDDD` | option hover/selected |
| BG/A3 | `#1B1B1B` | `#D5D5D5` | panels |
| BG/A4 | `#191919` | `#B3B3B3` | dropdown list, button variant, toggle track |
| BG/A5 | `#151515` | `#888888` | |
| BG/B1 | `#0E0E0E` | `#E7E7E7` | |
| BG/B2 | `#090909` | `#DEDEDE` | inputs, buttons, toggle base (most used BG) |
| BG/A3 Transparent | `#191919` @95% | `#D5D5D5` @75% | control bar, menus (with `bg blur`) |
| BG/B1 Transparent | `#0E0E0E` @95% | `#E7E7E7` @80% | sidebar over video |
| BG/B2 Transparent | `#000000` @80% | (none) | |
| BG/Accent Bright | `#371F06` | `#D8BA9A` | toggle-on track, accent tint |
| BG/Accent Dark | `#1A1611` | `#C6AB89` | |
| FG/1 | `#EAEAEA` | `#EAEAEA` (same as dark) | selected text |
| FG/2 | `#C6C6C6` | `#C6C6C6` (same as dark) | |
| FG/A1 | `#969696` | `#3C3C3C` | |
| FG/A2 | `#8F8F8F` | `#2E2E2E` | body text, default icon color |
| FG/A3 | `#6B6B6B` | `#6B6B6B` | secondary text, shortcuts |
| FG/A4 | `#404040` | `#3B3B3B` | most-used style overall (disabled/dim strokes) |
| FG/B1 | `#161616` | `#161616` | glyph on accent (play button) |
| Accent/Main | `#D37918` | `#E8851A` | play button, active icons |
| Accent/Bright | `#FC9729` | `#FFA13B` | |
| Accent/Dim | `#AB6314` | `#965916` | two-tone icon accent, toggle-on knob |
| Accent/Logo | `#D37918` | `#C2711A` | the cone |
| Divider/Bright | `#373737` | `#9B9B9B` | |
| Divider/Dim | `#222222` | `#ACACAC` | control-bar top border |

The volume cone (`244:184`) is a hard-coded linear gradient: `#95D318` 0 → `#B2A918` 0.58 → `#D37918` 0.79 → `#FF3737` 1. It is the same in every player frame, including the light one.

**The light theme is incomplete in Figma.**
- One frame (`2425:13624`) uses Light styles.
- Light FG/1, FG/2 and FG/B1 are byte-identical to Dark, which looks like they were never adjusted.
- In that frame, the transport icons (`previous`, `next`, `playback settings`) and the media thumbnails still use Dark styles.
- The icon set (`42:356`) is bound only to Dark styles. Instances are recolored to Light by overriding each one.

### Typography

Counted over the player, settings, dialog, onboarding, profile, menu, sidebar and component frames listed in `AUDIT.md`:

- **Inter** for all UI text: Regular 785 runs, Medium 31, Semi Bold 16, Light 2.
- **DM Mono** Regular, only for key caps in the keyboard-shortcuts dialog (`2437:14832`).
- Sizes: **14** (577 runs, body), 16 (101), 12 (94), 15 (39), 8 (17), 10 (4), 24 (1), 32 (1).
- Line height is auto, except 175% on some 14 and 16 px paragraphs.

### Corner radius

Measured on frames, rectangles and non-icon instances:
- **4** (456 nodes), **6** (193), **2** (146), **3** (46), **14** (20), **10** (7), 1, 16, 19, 20.
- Sheet tops use `14/14/0/0`.

Examples:
- Buttons, dropdown options: 4.
- Dropdowns, toggle base, options, control-bar play button: 6.
- Menu: 10.
- Toggle knob: 3.
- Settings category: 2.

### Spacing

- Auto-layout gaps: **10** (1878 frames, the dominant rhythm), **8** (156), **12** (119), **20** (83), **5** (76), **14** (63), **6** (19), **4** (11), 16, 17, 27.
- Common paddings: `4`, `10`, `2/14`, `2/8`, `20`, `8`, `11/20`, `4/20`.

### Sizes, strokes and effects

**Component sizes:**
- Control bar: 54 px tall.
- Play button: 54×28.
- Button: 32 tall.
- Dropdown, option and menu row: 48.
- Toggle: 68×32.
- Setting category: 40.

**Icon sizes and strokes:**
- Icons render at **24** px in most places, and at 28, 32 and 36 px in title bars and thumbnails.
- Icon strokes are **1.5 px**. Most "active" icons use 2 px.
- Borders are 1 px.

**Effects:**
- `bg blur`: background blur, radius 50.
- `bg blur + shadow`: the same blur plus a drop shadow (black @46%, y 8, blur 15).
- Both are used on the translucent surfaces: control bar, sidebar, menus.

### Icon set (`42:356`)

**Structure:**
- One component set with **208 variants** of **96 icon names**, every one a 24×24 frame.
- Variant properties:
  - `name`
  - `direction`: default, left, right, up, down
  - `state`: default, active, selected
  - `value`: default, high, medium, low, none (volume)
  - `EQ`: 17 equalizer presets
  - `two-tone`: no, yes

**Defects found while reading it:**
- Figma reports "Component set has existing errors", so variant properties cannot be read through the API.
- Three volume-level variants are named `close` (`182:187`, `182:193`, `182:198`).

**Color rules per variant:**

| Variant | Count | Colors |
| --- | --- | --- |
| monochrome, default | ~120 | `FG/A2` only (95), a few with an `Accent/Main` detail |
| **two-tone, default** | 46 | `FG/A2` + **`Accent/Dim`** (38). The transport-style icons (playback, stop, next, previous, record, favorite) are all `Accent/Dim`. `video` and `link` use `Accent/Main`. |
| two-tone, selected | 2 | `FG/A2` + `Accent/Dim` |
| active | 30 | `Accent/Main`, often a **different shape** (gear, folder and record filled in; stroke 2 instead of 1.5) |
| selected | 7 | `FG/A2` filled shape, details knocked out in `#303030` or `#D37918` |

All shapes are **stroked paths**: 1.5 px strokes and no outlines. Some cut-outs are painted in a background gray (`#303030`) rather than being real holes.

**Which set to use where.** The audit transcript gives the rule: two-tone in menus, where the icon isn't clickable, and monochrome everywhere else.

## 2. The Qt theme system as it is

- **Colors.** `ColorContext` resolves `palette × colorSet × section × name × state`.
  - **15 color sets:** View, Window, Item, Badge, ButtonStandard, ButtonAccent, ToolButton, TabButton, MenuBar, Tooltip, Slider, ComboBox, SpinBox, TextField, Switch (`style/colorcontext.hpp:73-90`).
  - **5 states:** Normal, Disabled, Pressed, Hovered, Focused (`:93-99`). Priority is Disabled > Pressed > Focused > Hovered > Normal (`style/colorcontext.cpp:365-382`).
  - **Names:** Fg/Bg use Primary, Secondary, Highlight, Link, Positive, Neutral, Negative. Decoration uses VisualFocus, Border, Accent, Shadow, Separator, Indicator (`style/colorcontext.hpp:107-123`).
  - **Inheritance:** contexts inherit palette, set and state from ancestors (`style/colorcontext.cpp:87-184`).
  - **Usage in QML:** `theme.fg.primary`, `theme.accent`, and so on (`widgets/qml/IconToolButton.qml:37`).
- **Palettes.** `SystemPalette` stores colors in a map keyed by those four axes.
  - Lookup falls back from the exact key to the Normal state, then to the View set, then to magenta (`style/systempalette.cpp:444-469`).
  - The light palette is `makeLightPalette`, about 106 `setColor` calls (`:472-684`). The dark palette is `makeDarkPalette`, about 107 calls (`:686-897`).
  - Both are built from base constants in `style/systempalette.hpp:114-136`: `orange500 #FF8800`, `orange800 #FF610A`, `darkGrey200..800`, `lightGrey100..700`.
- **Scheme.** `util/color_scheme_model.hpp:35-37` offers System, Day and Night. The choice is saved in QSettings `MainWindow/color-scheme-index` (`maininterface/mainctx.cpp:336`, `:497`). VLCStyle binds it at `style/VLCStyle.qml:38-48`.
- **Accent.** `qt-accent-color` (`qt.cpp:480`) accepts "default", "system" (Qt ≥ 6.6) or a color (`style/systempalette.cpp:305-336`).
  - It sets `orange500` to the chosen color and `orange800` to that color `.lighter(150)`.
  - Dark uses orange500 and light uses orange800, so in the light theme a custom accent appears 50% lighter than chosen.
  - ButtonAccent's pressed color stays hard-coded at `#e65609` / `#e67a30` (`:596`, `:808`). No hover shade exists.
- **Theme providers.** They can set any color, plus the window-button (CSD) images and window-frame margins only (`style/qtthemeprovider.hpp:129-164`). They cannot set fonts, radii or spacing.
  - With the System scheme, the GTK or system-palette provider replaces the VLC palette.
  - The Windows provider acts only in High Contrast.
- **Metrics.** `style/VLCStyle.qml` is plain QML. Every size is `MainCtx.dp(px, scale)`, with `scale = MainCtx.intfScaleFactor` (`:27`). Values below are before scaling.
  - Font sizes 6, 8, 10, 12, 14, 16, 20, 24 (`:29-36`); "normal" is 12.
  - Margins 2, 4, 8, 12, 16, 24, 32, 36 (`:53-60`). Border 1 (`:67`).
  - Icon sizes 8, 12, 24, 48, 64, 128; play button 28 (`:103-124`).
  - Radii exist only per component:
    - `listAlbumCover_radius` 3 (`:95`)
    - `button_radius` 4 (`:178`)
    - `navBoxButton_radius` 8 (`:189`)
    - `gridCover_radius` 4 (`:206`)
    - `mainView_topLeftRadius` 8 (`:215`)
    - ToolTipExt 4 and ComboBoxExt 2 are hard-coded in those widgets.
  - **No text font is loaded.** The UI uses the system font. Only the icon font is registered (`style/vlcicons.cpp:12-13`).
- **Icon font.** `pixmaps/makeIconFont.py` runs fontforge.
  - It calls `importOutlines` on each SVG listed in `pixmaps/VLCIcons.json` (96 glyphs, U+E000+i, `:165-173`). The output is single-color glyphs with no COLR or layers.
  - Its docstring warns against overlapping paths (`:10-15`).
  - `VLCIcons.ttf` and `style/vlcicons.{hpp,cpp}` are committed, not built. Only `Makefile.am:793` lists the TTF as a resource.
  - Existing source SVGs are filled outlines, for example `pixmaps/icons/add.svg`.
  - QML draws a glyph as a `Text` (`widgets/qml/IconLabel.qml`) colored with `Text.color`.
- **SVG recoloring.** `util/color_svg_image_provider.cpp:33-74` does case-insensitive text substitution in the SVG source: `#FF00FF` → color1, `#00FFFF` → color2, `#FF8800` → accent.
  - QML: `SVGColorImage.colorize(path).color1(c).accent(c).uri()`. Alpha is lost.
  - QWidgets: `util/colorizedsvgicon.cpp` swaps only color1.
- **Existing two-tone icons.** All are SVGs, rendered only through the recolorer or as fixed colors:
  - `pixmaps/menu/*.svg`: fixed `#747474` gray + `#FF950D` orange.
  - `pixmaps/sd/*.svg`: `#FF8800`, recolored to the accent; the gray stays fixed.
  - No QML component stacks two glyphs.

## 3. Mapping

| Figma | Qt mechanism | Verdict |
| --- | --- | --- |
| Dark/Light paint styles | `SystemPalette` base constants + `makeDarkPalette` / `makeLightPalette` | **Maps directly.** Replace `darkGrey*` / `lightGrey*` / `orange*` with the Figma ramps (BG A1–A5, B1–B2, FG 1/2/A1–A4/B1, Divider, Accent) and re-author the `setColor` tables for each color set. |
| Hover / selected / active fills (e.g. option hover = BG/A2, selected category = BG/A1 + FG/1, active icon = Accent/Main) | ColorContext states (Hovered, Pressed, Focused, Disabled) + names (Highlight, Accent) | **Maps.** "Selected" and "active" are not ColorContext states, but VLC already models them in QML: `checked ? theme.accent : theme.fg.primary`. A Disabled state is not drawn in Figma. |
| Transparent BG + alpha | `setColorAlpha` already used in the palettes | **Maps directly.** |
| Dark / light theme choice | ColorSchemeModel System/Day/Night | **Maps directly.** "System" must pick between the two Figma palettes, not hand the colors to the GTK or system-palette provider. See open questions. |
| Accent color setting (audit: user-chosen accent) | `qt-accent-color` → `orange500`/`orange800` | **Needs extension.** Figma has six accent tokens per theme: Main, Bright, Dim, Logo, BG/Accent Bright, BG/Accent Dark. Today one color yields two shades, and the light one is wrongly lightened. A per-theme derivation is needed: HSL offsets measured from the Figma pairs, applied to the user's accent. The hard-coded pressed colors must go. |
| Inter / DM Mono | none | **Gap.** Bundle the fonts (both SIL OFL) and register them with `QFontDatabase::addApplicationFont`, as `vlcicons.cpp` does for icons. Set them as the application or root font. |
| Type sizes 8/10/12/14/15/16/24/32, weights Regular/Medium/SemiBold | `fontSize_*` 6–24; weights set per widget (`Font.DemiBold`, …) | **Needs extension.** Figma body text is 14, which VLC calls `fontSize_large`, so the scale's names must change (body 12 → 14). 15 and 32 are missing; 15 is used on 39 runs and probably should snap to 14 or 16. A Medium weight is not used today. |
| Radius 2/3/4/6/10/14 | per-component `*_radius` properties | **Needs extension (trivial).** Add a `radius_*` scale to VLCStyle and point component radii at it. Theme providers cannot set radii, and don't need to. |
| Spacing 4/5/6/8/10/12/14/20 | `margin_*` 2/4/8/12/16/24/32/36 | **Needs extension.** 10 (the dominant gap), 20, 14, 5 and 6 are off-scale. Either add steps or snap the design to the existing scale; this is a #8 decision. |
| Icon size 24 (+28/32/36), stroke 1.5 | `icon_normal` 24, `icon_play` 28, `icon_audioPlayerButton` 32 | **Maps.** 36 (title bar) is new. |
| Border 1 px, Divider tokens | `VLCStyle.border` 1; Decoration Separator/Border | **Maps directly.** |
| `bg blur` (radius 50) + drop shadow | Decoration Shadow color; `AcrylicController` only drives window-level acrylic (`MainCtx.hasAcrylicSurface`) | **Gap.** Blurring the video behind the control bar, sidebar and menus needs an in-scene blur (e.g. QtQuick.Effects `MultiEffect` over a `ShaderEffectSource`). That is a rendering and performance question, not a palette one. |
| Volume cone gradient | none (component-local) | **Component-level.** Hard-code it in the volume widget, or derive it if it should follow the accent. |
| Monochrome icons + states | icon font + `IconLabel` colored by ColorContext | **Maps.** Each state with a different shape (active filled, selected filled) is a separate glyph key, for example `folder`, `folder_active`, `folder_selected`. |
| Two-tone icons | none | **Needs a new component.** See below. |

## 4. Two-tone icons

A TrueType glyph has a single color, and `makeIconFont.py` imports outlines only. A two-tone icon therefore cannot be one glyph. There are three options.

**A. Layered glyphs (recommended).**
- Export each two-tone icon as two SVGs: a base layer (`FG/A2`) and an accent layer (`Accent/Dim`).
- Add both to `VLCIcons.json` (e.g. `folder_tt_base`, `folder_tt_accent`).
- Draw them with a `TwoToneIcon.qml`: two `IconLabel`s stacked at the same size. Base is colored `theme.fg.*` and accent `theme.accent`, or a new dim-accent name.

Pros:
- Stays on the existing pipeline.
- Vector-sharp at every `intfScaleFactor`.
- Follows the light/dark switch and `qt-accent-color` live with no re-rasterization.
- Cheap: two `Text` items.

Cost:
- About 46 more glyphs.
- The base layer needs a knock-out wherever the accent layer overlaps it, or the edges will halo.
- The transport icons that are all-accent need only one glyph.

**B. SVG placeholders through `image://svgcolor`.**
- Author the base layer in `#FF00FF` and the accent layer in `#FF8800`.
- Render with `SVGColorImage.colorize(src).color1(fg).accent(accent).uri()`.

It already works for `pixmaps/sd/*.svg` in network views and needs no font work. But:
- Each icon is an async raster image, re-rendered for each color and size, and on every theme or accent change.
- Alpha is dropped.
- The QWidgets path only swaps color1.

It is acceptable for static, non-clickable menu icons, which is where two-tone is used. It is worse than A for consistency, because there would be two icon systems.

**C. Color font (COLR).** The colors would be baked into the font palette, which breaks theming and the custom accent. **Rejected.**

**Preparation needed for either A or B:**
- The Figma icons are 1.5 px **strokes**, and some cut-outs are painted `#303030` rather than being real holes.
- Before export, run "Outline stroke", flatten or union the shapes, and turn the painted cut-outs into real holes.
- Today's `pixmaps/icons/*.svg` are already fill-only paths.
- `makeIconFont.py` needs fontforge's Python bindings. The script is offline tooling; nothing in the build runs it.

## 5. Open questions

- **Accent reach.** Should a custom accent recolor the cone logo (`Accent/Logo` is a separate token) and the volume-cone gradient? How should the four accent shades be derived from one user color per theme?
- **System scheme and theme providers.** With the "System" scheme, the GTK and system-palette providers paint the whole UI from desktop colors, which would override the Figma palette. Keep them, drop them, or limit "System" to choosing light or dark?
- **Light theme completeness.** The light palette has copy-paste values (FG/1, FG/2, FG/B1) and only one light frame exists. This overlaps the map's "Light-theme parity" fog.
- **Scales.** Snap the design's 10/20/14/5/6 spacing and 15/32 type sizes to VLC's scales, or grow the scales? Rename VLC's type scale so body = 14?
- **Backdrop blur.** How does blurring the video behind the bars and sidebar perform? It needs measuring, likely in a prototype.
- **Icon export.** Who outlines the 208 variants and produces the 96+ glyph SVGs, and with what tooling (Figma export plus fontforge)?
- **Figma cleanup.** Fix the icon component set's variant errors and the volume variants misnamed `close`.
