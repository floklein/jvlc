# Backdrop blur over video

Research for [#22](https://github.com/floklein/jvlc/issues/22), on the [redesign map](https://github.com/floklein/jvlc/issues/1).

**Question.** Can a backdrop blur of the playing video, behind the control bars, sidebars and menus, run cheaply in the Qt/QML scene on Windows and Linux? What does the existing blur support cover? Which compositors can sample the video? What is the fallback when they can't?

All paths are relative to `modules/gui/qt/` unless they start with `modules/`, `include/` or `docs/`. Line numbers refer to branch `feat/redesign` at `fea44a0814`.

## Answer

**No compositor puts video pixels in the Qt scene, so no QML effect can blur the video today.** Every compositor shows the video in a native layer underneath the Qt UI: a DirectComposition visual, a child or sibling window, a Wayland subsurface, or an XRender picture. The QML `VideoSurface` item only reports where the video should go; it paints nothing. QML blur (`DualKawaseBlur`, `MultiEffect`, `layer.effect`) can only sample items in the same scene, so over the video it sees a transparent hole.

The existing blur covers two cases, and neither is "video behind the UI":

1. **In-scene blur of QML content**: `DualKawaseBlur`, `FrostedGlassEffect` and `PartialEffect`. It is cheap and proven: the mini player's frosted glass over the library, the modal dialog backdrop, and the blurred cover art in the audio player.
2. **Window-level acrylic** (`AcrylicController`, `AcrylicBackground`, the DComp acrylic surface, KWin/DWM blur-behind). It blurs the **desktop behind the window**, not the window's own video. In the DComp tree the acrylic visual is even placed *below* the video visual.

**Fallback (works on every compositor, already used today):** a translucent tinted rectangle over the video (`bg.primary` at 0.6–0.7 opacity), plus the black gradient "fade rectangles" when the bars float over the video. For the redesign, Figma's `bg blur` (radius 50) on surfaces over video should become **tint only**. Real blur stays on surfaces that sit over QML content: library, sidebars over the library, menus over the UI, and dialogs.

Getting a real blur over the video would take engine or compositor work. Options are listed below; none is cheap.

## Per-compositor table

The compositors are selected in this order (`maininterface/compositor.cpp:56-76`): `dcomp` → `platform` → `win7` (Windows); `wayland` → `x11` (Linux); `dummy` everywhere. The `qt-compositor` option overrides the order (`qt.cpp:391`).

| Compositor | When it's used | Where the video lives | Can QML sample the video? | Existing blur | Fallback over video |
| --- | --- | --- | --- | --- | --- |
| **dcomp** (default on Windows 8+ with D3D11/D3D12 RHI) | `compositor_dcomp.cpp:109-181` | A DirectComposition visual **behind** Qt's UI visual (`compositor_dcomp.cpp:68`), filled by the D3D11 vout's swap chain (`modules/video_output/win32/dxgi_swapchain.cpp:349`) | **No** | In-scene Kawase works (RHI). Acrylic surface = DWM desktop thumbnail + DComp Gaussian blur (σ 40) + saturation, added **below all siblings**, i.e. below the video (`compositor_dcomp.cpp:482`, `compositor_dcomp_acrylicsurface.cpp:254-277`) | Tint. In theory a native DComp blur of a second video visual (see option B) |
| **platform** (Windows, only in non-RHI software/OpenVG mode; also macOS) | `compositor_platform.cpp:41-72` | A child `QWindow` HWND under a layered, transparent Quick child window (`compositor_platform.cpp:80-116, 141-145`) | **No** | `DualKawaseBlur.available` is false without RHI (`widgets/qml/DualKawaseBlur.qml:38`) | Tint |
| **win7** (Windows fallback) | `compositor_win7.cpp:53-62` | A separate tool window kept z-ordered right behind the transparent UI window (`compositor_win7.cpp:72-92, 238-255`) | **No** | No acrylic flag (`compositor_win7.cpp:106`). In-scene Kawase only if RHI | Tint |
| **wayland** | `compositor_wayland.cpp:60-91` | A `wl_subsurface` placed **below** the UI surface, desynchronized (`maininterface/compositor_wayland_module.c:308-310`). The system compositor composites it, not Qt | **No** | In-scene Kawase works. Window blur-behind via KWindowEffects, KDE only (`maininterface/kwindowsystem_module.cpp:32-62`), which blurs what is behind the window (unverified whether KWin includes the window's own subsurfaces; most likely not) | Tint |
| **x11** | `compositor_x11.cpp:91-187` (needs OpenGL RHI or software Qt Quick) | An X window whose pixmap VLC composites itself with XRender: video `PICT_OP_SRC`, then the offscreen-rendered UI `PICT_OP_OVER` (`maininterface/compositor_x11_renderwindow.cpp:104-127`) | **No** (the UI is rendered offscreen through `QQuickRenderControl` and never sees the video) | In-scene Kawase with OpenGL RHI. KWin blur-behind when available (`compositor_x11.cpp:217, 237`) | Tint. In theory XRender downscale/upscale in `RenderTask` (see option C) |
| **dummy** | Last resort | No embedded video (separate vout window) | N/A | N/A | N/A |

Why the UI can't see the video:

- `VideoSurface` inherits `ViewBlockingRectangle` (`maininterface/videosurface.hpp:88`). In the player it is created with `renderingEnabled: false` (`player/qml/Player.qml:259-268`), so it paints nothing. It only forwards its scene position, size and scale to the compositor (`maininterface/videosurface.cpp:236-293`).
- `ViewBlockingRectangle` draws with blending disabled (`widgets/native/viewblockingrectangle.cpp:215`). It needs a depth buffer, which is why blur-behind and PiP are switched off without one (`maininterface/compositor.cpp:294-309`). It "punches through" the Qt scene to the native layer beneath.
- The vout only ever receives a native handle: `VLC_WINDOW_TYPE_DCOMP`, `HWND`, `WAYLAND` or `XID` (`compositor_dcomp.cpp:458`, `compositor_platform.cpp:142`, `compositor_wayland_module.c:274`, `compositor_x11.cpp:278`). No path hands the UI a frame texture.

## What the existing blur support is

### In-scene: `DualKawaseBlur` family

- `widgets/qml/DualKawaseBlur.qml` implements dual Kawase blur (Bjorge, SIGGRAPH 2015) with `shaders/DualKawaseBlur_{downsample,upsample,upsample_postprocess}.frag`. Its comment explains why it was preferred over Qt's `MultiEffect`: it supports the half-pixel trick, atlas textures and sub-textures (`:24-31`).
  - Requires RHI (`available`, `:38`).
  - The source must already be a texture provider. Unlike `MultiEffect`, the effect never creates a layer on its own (`:89-100`).
  - **Modes** (`:42-52`): TwoPass keeps 1 intermediate buffer, FourPass (the default) 3, SixPass 5. Each buffer is half the size of the previous one (`:284-286, 333-335`). With `live: false`, intermediate buffers are released after a chained update (`:564-595`), so SixPass on a static source costs no extra memory.
  - Postprocess adds tint, noise, exclusion and background color (`:54-63`).
- `widgets/qml/FrostedGlassEffect.qml:25-34`: FourPass, radius 3, tint 0.7, noise 0.02. This is VLC's "frosted glass" preset.
- `widgets/qml/PartialEffect.qml` applies an effect to a sub-rectangle of a layered item without rendering the source twice. It uses `QSGTextureView` sub-texturing (`:137-180`).
- `fast_gaussian_blur_template.h` (`util/effects_image_provider.cpp:28, 102`) is a CPU blur for generated images, not for live content.

Where it runs today:

| Use | File | Notes |
| --- | --- | --- |
| Frosted mini player / update pane over the library | `maininterface/qml/MainDisplay.qml:321-441` | `layer.enabled` on the library container when `MainCtx.backdropBlurRequested()` and RHI and the mini player is visible. `PartialEffect` and `FrostedGlassEffect` blur only the strip under the mini player, with a 16 px edge extension and a viewport that limits VRAM |
| Modal dialog backdrop | `dialogs/dialogs/qml/ModalDialog.qml:55-84` | SixPass, live `ShaderEffectSource` of the whole root window. Over video it blurs the UI only (the video area is a hole) |
| Audio player background (blurred cover) | `player/qml/Player.qml:374-456` | SixPass on a static image. `live` is toggled by a timer so intermediate layers are released |
| Artist banner, album section | `medialibrary/qml/ArtistTopBanner.qml:97-150`, `medialibrary/qml/MusicAlbumSectionDelegate.qml:126-145` | TwoPass on covers |

All in-scene uses are gated by `qt-backdrop-blur` (default on, `qt.cpp:290-292, 474`), exposed as `MainCtx.backdropBlurRequested()` (`maininterface/mainctx.hpp:285`).

### Window-level acrylic

- `style/AcrylicController.qml:25-44`: a singleton whose `uiTransluency` animates to 1 when `MainCtx.hasAcrylicSurface` is true and the window is active. It writes `MainCtx.acrylicActive`.
- `widgets/qml/AcrylicBackground.qml:28-41`: a `ViewBlockingRectangle` filled with `tintColor` at alpha 0.7, blended toward an opaque alternative color as transluency drops. Used by the side navigation, top bars, play queue pane, player top bar and minimal view (`SideNavigationPane.qml:91`, `MenuTopbar.qml:57`, `LocalTopbar.qml:69`, `PlaylistPane.qml:110`, `Player.qml:1009`, `MinimalView.qml:87`).
- Backends:
  - **DComp acrylic surface** (Windows): DWM shared thumbnail of the desktop and other windows (undocumented `dwmapi` ordinals 147/163/164), blurred by a DComp Gaussian effect with σ 40 and saturation 2 (`maininterface/compositor_dcomp_acrylicsurface.cpp:223-350`). It follows the Windows "transparency effects" setting and high contrast (`:47-63`). It is added below all siblings (`compositor_dcomp.cpp:476-487`; Microsoft documents that `AddVisual(v, TRUE, NULL)` renders `v` "below all of its siblings").
  - **Win32 window effects** (Windows 11 22H2+): `DWMSBT_TRANSIENTWINDOW` system backdrop (`maininterface/win32windoweffects_module.cpp:69-99`).
  - **KWindowSystem** (Linux, KWin): `KWindowEffects::enableBlurBehind` (`maininterface/kwindowsystem_module.cpp:42-62`), with the region adjusted for CSD shadows (`compositor.cpp:342-374`).

All three blur what is **behind the window**. None of them can see the video, which is the window's own content, and in DComp it is stacked above the acrylic.

### Current treatment over video

- Control bar: plain `Rectangle`, `bg.primary`. Opacity is 0.6 when floating over video, `pinOpacity` in fullscreen, 0.7 with acrylic (`player/qml/Player.qml:1336-1354`).
- Top bar: `AcrylicBackground` at 0.6 over video (`Player.qml:1009-1029`).
- Fade rectangles: black gradients up to 0.8, blending turned off through `SGManipulator` (`Player.qml:305-345`).

## Cost if the video were in the scene

The cost of the blur itself is small. The data path is the expensive part.

- Qt: a layer "will allocate memory in the GPU equal to `width x height x 4`" and "can not be batched". "In most cases, using a ShaderEffectSource will decrease performance, and in all cases, it will increase video memory usage." `MultiEffect`: "Blur and shadow are the heaviest effects", and its documentation advises against blurring animating sources ([Item layers](https://doc.qt.io/qt-6/qml-qtquick-item.html), [ShaderEffectSource](https://doc.qt.io/qt-6/qml-qtquick-shadereffectsource.html), [MultiEffect](https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html)).
- Video is a source that animates by definition: 24–60 updates per second, so the blur must be `live`.
- *Estimate (not measured; no build environment yet):* `DualKawaseBlur` FourPass on a 1920×100 bar strip, sub-textured from the frame the way `PartialEffect` does it, touches about 0.1 MP per pass. That is negligible on any GPU that runs the D3D11 or GL RHI. The real costs are getting the frame into the Qt scene and losing the direct video path, plus HDR, see option A.

## Options for a real blur over video

| Option | What | Platforms | Effort | Risks |
| --- | --- | --- | --- | --- |
| **A. Video in the scene** | A new compositor/vout path where the vout renders into a texture that a `VideoSurface` (made a texture provider) draws in the Qt scene. The core already has the mechanism: libvlc's `libvlc_video_set_output_callbacks` with D3D11/OpenGL engines (`include/vlc/libvlc_media_player.h:1046-1109`). Then `PartialEffect` + `FrostedGlassEffect` work over video unchanged | All RHI compositors | L–XL | Loses the DComp/subsurface direct path: an extra copy per frame, possibly a frame of latency, HDR/10-bit passthrough (the DComp swap chain is 10-bit, `dxgi_swapchain.cpp:340-345`), more power. Also a large change to `compositor_*` |
| **B. DComp native blur** | A second DComp visual between video and UI, with the video swap chain as content plus a `CreateGaussianBlurEffect` and a clip to the bar rectangles | Windows / dcomp only | M | The swap chain is set by the vout (`dxgi_swapchain.cpp:349`), and the Qt module never sees it. Microsoft's `SetContent` documentation does not say whether one swap chain may back two visuals: **unverified**. DComp effects blur the visual's own subtree, not the background ([SetEffect](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-seteffect)). Bar rectangles must be mirrored from QML to native code |
| **C. X11 compositing blur** | In `RenderTask`, before compositing the UI, re-composite the video picture under each bar region through a downscale/upscale XRender transform (or a convolution filter) | Linux / x11 only | M | XRender convolution is slow. Bilinear down/up-scaling gives a cheap but blocky blur. Not available on Wayland, which is where Linux desktops are heading |
| **D. Wayland** | Nothing client-side: the system compositor composites the subsurface. `org_kde_kwin_blur` and similar protocols blur behind the window | Wayland | – | Only option A helps |

## Recommendation

- **Over video: tint, no blur.** Keep the existing approach (translucent `bg.primary` plus gradients) and design for it. In Figma, surfaces that float over the video (top/bottom bars, the compact library sidebar, the adjustments sidebar, menus opened over video) need a no-blur variant: a solid or translucent fill.
- **Over UI content: keep the real blur.** `FrostedGlassEffect` + `PartialEffect` already deliver Figma's `bg blur` look where the backdrop is QML: library, home, sidebars over the library, the mini player, dialogs. Its strength should become a design token (the theme ticket asked for this).
- **Rule "blur over video" out of scope**, or park it behind option A. Per the map's rule, an engine-dependent feature leaves scope once research shows it infeasible. Here it is infeasible at reasonable cost in the current architecture, and option B is Windows-only and unverified.

## Open questions

- Should the redesign accept a different look over video (tint) and over the library (frosted)? Or should it use tint everywhere for consistency? This is a design decision for #8 (Design system) or #9 (Player layout).
- Is option A (video rendered inside the Qt scene) worth a spike for reasons beyond blur, such as rounded video corners, video under translucent sidebars, or seek-preview compositing? It is a major architecture change.
- If option B matters: can one `IDXGISwapChain1` be the content of two DComp visuals? This needs a 30-minute native test on Windows.
- Does KWin's blur-behind include the window's own subsurfaces? This only matters if someone wants a KDE-specific hack. Unverified.
