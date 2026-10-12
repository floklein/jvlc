# Cheap blurred-video backdrop

Research for [#23](https://github.com/floklein/jvlc/issues/23), on the [redesign map](https://github.com/floklein/jvlc/issues/1). It builds on [Backdrop blur over video](https://github.com/floklein/jvlc/issues/22) ([findings](https://github.com/floklein/jvlc/blob/research/backdrop-blur/docs/research/backdrop-blur.md)), which found that every compositor shows the video in a native layer under the Qt UI, so QML cannot sample it.

**Question.** Is there a cheap way to put a blurred-video backdrop behind the panels that float over the video (top and bottom bars, compact library sidebar, adjustments sidebar, menus) without drawing the main video inside the Qt scene? For each candidate: cost, sync with the picture, platform coverage, code touched, and size.

Paths are relative to the repo root. Line numbers refer to branch `feat/redesign` at `c5f7bbfcbc`. Sizes and costs marked *estimate* are not measured: there is no build environment yet.

## Answer

**Yes on Windows, no elsewhere.** On the default Windows compositor (`dcomp`), DirectComposition can show the video's own swap chain a second time, blurred and clipped to the panels. The blur is exact and in sync, with no extra copy. On Linux (Wayland, X11) and the Windows fallbacks (`win7`, `platform`), every cheap route either can't see the video or lags it by a few hundred milliseconds.

- **Recommended: native DirectComposition blur on `dcomp`, with tint as the baseline everywhere else.** Size M (*estimate* ~350–450 lines; 3–5 days including a 1-day spike). It touches one core header field, ~10 lines in the D3D11 vout, the `dcomp` compositor, and a new QML item that reports panel rectangles.
- **Low-res copy and ambient color** both rely on periodic frame grabs (2–5 Hz). They give an ambient glow that lags the picture, not a backdrop blur. They are not worth it as a blur substitute. Ambient color could become a separate, optional "video-tinted panels" feature.
- **"Tint everywhere" (#8)**: reopen it only if the spike passes, and only as "tint baseline + native blur where the compositor supports it". This is the same split VLC already makes for acrylic (`MainCtx.hasAcrylicSurface`). Linux stays tint.

## Comparison

| Approach | Cost (*estimates*) | Sync with picture | Platforms | Code touched | Size |
| --- | --- | --- | --- | --- | --- |
| **A. DComp native blur** (second visual showing the vout's swap chain, Gaussian blur, clip per panel) | No copy or readback. DWM runs one Gaussian blur per composed frame over the panel areas | **Exact**: the same swap chain buffer in the same DWM frame. Clips follow the QML frame (same mechanism as `VideoSurface`) | Windows 8.1+ with the `dcomp` compositor (the default with D3D11/D3D12 RHI). Not `win7` or `platform` | `include/vlc_window.h`, `modules/video_output/win32/dxgi_swapchain.cpp`, `direct3d11.cpp`, `compositor_dcomp.*`, a new QML item | **M** |
| **B. X11 XRender pyramid** (in `RenderTask`, down/up-scale the video picture under each panel) | A few small XRender composites per refresh, server side (GPU with glamor, CPU with pixman) | Exact (same composite pass) | X11 sessions with the `x11` compositor only | `compositor_x11_renderwindow.cpp`, the same QML item | **M** |
| **C. Low-res copy via periodic grab** (snapshot-style grab, 64×36, Kawase-blurred in QML) | Per grab: full-frame GPU→CPU readback (1080p NV12 ≈ 3 MB, 4K P010 ≈ 25 MB) with a blocking `Map`, plus a CPU downscale. 9 KB texture | **Lags** 1 grab period + conversion (≈250–500 ms at 2–4 Hz). Needs a crossfade | All compositors, all OSes | Small core change (a grab that skips the snapshot SPU path), a Qt grabber thread, QML | **S–M** |
| **C′. Low-res copy via vout filter / splitter / thumbnailer** | Per-frame readback (filter), a second display window (clone), or a second demux+decode (thumbnailer) | Filter: exact but costly. Others: unusable | – | – | **Rejected** |
| **D. Ambient color** (same grab as C, reduced to 4–8 colors, gradient behind panels) | Same grab as C. Averaging is negligible | Lags like C. Less visible, because colors drift smoothly | All | Same as C, simpler QML | **S–M** |
| **E. Native blur on Wayland** (`ext-background-effect-v1`, KDE blur) | – | – | **Doesn't work**: compositors blur what is behind the *window*, not its own subsurfaces | – | **Rejected** |
| **F. In-vout region blur** (the D3D11 and OpenGL renderers draw blurred strips into the video swap chain under the panels) | One blur pass per video frame over the strips | Video exact. Panel geometry lags up to one video frame, or 80 ms while paused | All compositors with the d3d11 or OpenGL vout | Core window→display plumbing, plus 2–3 renderers (d3d11, OpenGL, libplacebo) | **L** |
| *Reference:* video in the Qt scene (#22, option A) | Extra copy per frame, loses the direct path and HDR passthrough | Exact | All RHI compositors | Compositors + vout | L–XL |

## A. DirectComposition native blur (recommended)

### How the video gets on screen today

- The `dcomp` compositor builds a root visual with Qt's UI visual as a child (`modules/gui/qt/maininterface/compositor_dcomp.cpp:238-244`). The DComp device is Qt's own RHI device (`:213-231`).
- It creates a video visual (`:449`) and inserts it *below* the UI visual (`AddVisual(m_videoVisual, FALSE, m_uiVisual)`, `:68`).
- It hands the vout the device and that visual through the window handle: `p_wnd->display.dcomp_device` and `p_wnd->handle.dcomp_visual` (`:458-461`; union fields in `include/vlc_window.h:394-419`).
- The D3D11 vout creates a composition swap chain and calls `visual->SetContent(swapchain)` and then `device->Commit()` (`modules/video_output/win32/dxgi_swapchain.cpp:339-351`, visual pointer from `:419`). The Qt module never sees the swap chain.
- Qt moves the video visual from the render thread through `VideoSurface::synchronize()` (`modules/gui/qt/maininterface/videosurface.cpp:236-293`), then calls `SetOffsetX/Y` (`compositor_dcomp.cpp:367-376`) and commits explicitly (`commitSurface`, `:525-529`).

### What DirectComposition allows

- **One swap chain can back several visuals.** Microsoft: "For all three kinds of bitmaps, an application can associate the same bitmap with multiple visuals, which means that a single memory allocation can be used to display the same content several times." A swap chain is one of those bitmaps ([Bitmap objects](https://learn.microsoft.com/en-us/windows/win32/directcomp/bitmap-surfaces); [SetContent](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-setcontent) lists `IDXGISwapChain1` as valid content). This answers #22's open question "Can one DXGI swap chain back two DComp visuals?": documented yes, still to be confirmed in the spike.
- **Effects apply to the visual's own subtree.** `SetEffect` "creates an implicit off-screen surface to which the subtree that is rooted at this visual is composed. The surface is used as one of the inputs to the specified effect" ([SetEffect](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-seteffect)). A visual whose content is the video swap chain therefore yields a blurred copy of the video. It does not blur "the background", which is why #22 saw no route through acrylic.
- `IDCompositionDevice3::CreateGaussianBlurEffect` needs `IDCompositionDevice3`, available on Windows 8.1+ ([IDCompositionDevice3](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nn-dcomp-idcompositiondevice3), [CreateGaussianBlurEffect](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice3-creategaussianblureffect)). The effect "must be created by the same IDCompositionDevice that created this visual" ([SetEffect](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-seteffect)).
- **The tree already does this.** The acrylic surface obtains `IDCompositionDevice3` from Qt's device by `QueryInterface` (`compositor_dcomp_acrylicsurface.cpp:111`). It builds a root visual with a rectangle clip, a saturation effect and a Gaussian blur (σ 40, hard borders) on non-surface content, a DWM thumbnail (`:255-277`, `:288-307`). Option A reuses that pattern with the video swap chain as content.

### Design

```
root
├── videoVisual            (content = swap chain)         ← vout, unchanged
├── backdropContainer      (inserted above videoVisual)
│   └── panelClip[i]       (offset + rectangle clip = panel i)
│       └── blurVisual[i]  (content = same swap chain, offset = video offset,
│                           effect = Gaussian blur [+ saturation])
└── uiVisual               (Qt, draws the tint on top)
```

- **Clip on the parent, effect on the child.** The parent's clip then cuts the child's already-blurred output. That way the blur samples real video beyond the panel edge instead of transparent pixels. Microsoft does not document the order of clip and effect *on the same visual*. The two-level tree avoids depending on it.
- **Getting the swap chain to the second visual: minimal vout change.** Turn the `dcomp_visual` union member into a small struct (`{ visual, backdrop_visual }`), or add a field outside the union. `dxgi_swapchain.cpp` then calls `SetContent` on both visuals (and clears both on cleanup). That is ~10 lines in `dxgi_swapchain.cpp` and `direct3d11.cpp:638`, plus the header. No new copy and no new thread.
- **Panel rectangles.** A new QML item (`VideoBackdrop`, say) placed under each panel reports its scene rectangle through the same render-thread `synchronize()` pattern as `VideoSurface`. Clips then move in the same DComp commit as the video offset, so sliding panels don't trail by a frame. QML still paints the panel's tint on top. The blur strength (σ) becomes a theme token.
- **Only while needed.** Attach the blur visuals only while a panel over the video is visible, and detach them when the bars hide. Fullscreen video with hidden bars then composes exactly as today. *Unverified:* whether an extra visual sampling the swap chain blocks DWM's overlay or independent-flip path. The spike should check fullscreen power use with bars hidden and shown.

### Cost and coverage

- **Cost:** no GPU→CPU copy, no extra decode, no extra texture in VLC. DWM blurs the clipped regions on each composed frame. *Estimate:* comparable to the existing acrylic, which blurs a full-desktop thumbnail at σ 40.
- **Sync:** exact. Both visuals sample the same presented buffer in the same DWM frame.
- **Coverage:** the `dcomp` compositor only, which is the Windows default with D3D11/D3D12 RHI (#22 table). `win7` and `platform` keep the tint. So does a vout that cannot take a DComp handle, because the backdrop visual then has no content.
- **HDR:** *unverified*. The swap chain may be 10-bit, HDR10 (`dxgi_swapchain.cpp:340-346`). DWM composes it either way, and the blur output only needs to look plausible under a tint.

### Spike (1 day, Windows)

1. `SetContent` with the same `IDXGISwapChain1` on two visuals of Qt's device: does the second visual update every `Present1`?
2. Is the two-level clip/effect tree correct at the panel edges, and what does a σ 25–50 blur over a 1920×120 strip cost in GPU time (PIX or GPUView)?
3. With bars hidden and backdrop visuals detached: is fullscreen playback power unchanged?

## B. X11: XRender pyramid in `RenderTask`

- VLC composites the X11 window itself. On each refresh it draws the video picture (`PICT_OP_SRC`), then the offscreen UI (`PICT_OP_OVER`) (`modules/gui/qt/maininterface/compositor_x11_renderwindow.cpp:78-130`).
- XRender supports projective transforms (`SetPictureTransform`) and filters. `nearest` and `bilinear` are required; `convolution`, `gaussian` and `binomial` are optional ([Render protocol](https://xorg.freedesktop.org/archive/current/doc/renderproto/renderproto.txt), section 11).
- Between the two composites, render the video region under each panel through 3–4 halving steps with `bilinear` into small pictures. Then scale back up into the panel rectangle. A single 16× bilinear step would alias and shimmer; the halving pyramid is a cheap box blur. The optional `gaussian` filter isn't guaranteed per screen.
- Cost: a handful of tiny composites per refresh. Sync is exact.
- **Not recommended.** It covers only X11 sessions with the `x11` compositor, the Linux path that is shrinking. Wayland has no equivalent (E), so Linux would still split in two.

## C. Low-res copy in Qt

### Route 1: periodic grab through the vout snapshot

- **The API exists.** `vout_GetSnapshot(vout, NULL, &picture, &fmt, NULL, timeout)` returns a reference to the next rendered picture (`include/vlc_vout.h:113-131`, `src/video_output/video_output.c:416-447`). The vout clones the displayed picture into the request (`src/video_output/snapshot.c:133-156`). Qt already reaches the vout from a thread pool for the snapshot button (`modules/gui/qt/player/player_controller.cpp:2046-2066`).
- **Paused video works**: an idle vout re-renders every 80 ms (`video_output.c:191, 1684-1703`).
- **Hardware frames are read back on the caller thread.** `image_Convert` builds a "video converter" with the picture's video context (`src/misc/image.c:503-548, 763-790`). For D3D11 that is `d3d11_surface`. It copies the whole frame to a staging texture, then calls a blocking `Map(D3D11_MAP_READ)` under the shared device lock (`modules/hw/d3d11/d3d11_surface.c:279-291`). The downscale to 64×36 runs on the CPU after that.
- **Side effect: subtitles may drop on grabbed frames.** When a snapshot is pending, the vout turns off display-side subtitle blending for that frame (`vd_does_blending = !do_snapshot && …`, `video_output.c:1276-1279`). It tries to blend into the picture instead (`:1335-1366`), which fails for opaque hardware pictures. Only the snapshot copy gets an RGBA conversion with subtitles (`ConvertRGBAAndBlend`, `:1164-1191`). The displayed frame then carries no subpicture (`:1399-1405`). Grabbing at 2–5 Hz could make subtitles flicker with hardware decoding. *Code reading, not tested.* The fix is a small core addition: a grab request that takes the `picture_Clone` without touching the SPU path.
- **Display in QML.** A 64×36 `QImage` becomes a texture through [`QQuickWindow::createTextureFromImage`](https://doc.qt.io/qt-6/qquickwindow.html#createTextureFromImage) or an image provider. The existing `DualKawaseBlur` and `PartialEffect` then blur and slice it per panel (#22). No native texture interop (`QNativeInterface`) is needed at this size.
- **Cost (*estimate*):** per grab, one full-frame readback and a CPU downscale (about 3 MB at 1080p NV12, about 25 MB at 4K P010), a few milliseconds of CPU, and a possible device-context stall from the synchronous `Map`. At 2–5 Hz that is affordable. At 24–60 Hz it isn't.
- **Sync:** the backdrop trails by one grab period plus conversion. It needs a crossfade, which adds more lag. Under a strong blur and a 0.6 tint this shows mainly at scene cuts: the panel changes color a few hundred milliseconds after the picture. It reads as an "ambient glow", not as frosted glass over the picture directly behind.

### Route 2: vout filter, clone splitter, thumbnailer (rejected)

- **Video filter** in the vout chain: on hardware-decoded video, a CPU filter makes the chain insert a converter in front of it (the `"%s,chain"` fallback, `src/misc/filter_chain.c:321-331`). That means a full-frame readback on *every* frame, and the display then gets CPU frames. Avoiding the readback needs one GPU filter per hardware API (D3D11, VAAPI, VDPAU, NVDEC, …). L, for the same lagging-free result that A already gives on Windows.
- **Clone splitter:** with a splitter, the core forces the vout's own window to `wdummy` (`src/video_output/video_output.c:2233-2246`). The splitter display refuses embedded windows (`modules/video_output/splitter.c:219-226`) and opens one new window per output (`:189-211, 285-298`). The clone `picture_Copy`s every frame on the CPU (`modules/video_splitter/clone.c:180-194`). It would un-embed the main video. Unusable.
- **Thumbnailer:** each request creates a separate input of type `INPUT_TYPE_THUMBNAILING`, with its own demux and decode (`src/preparser/internal.c:506-531`). That takes seconds to produce a frame and doesn't follow live streams or discs. Fine for seek previews (#3), useless for a live backdrop.

## D. Ambient color

- It uses the same grab as C, reduced to 4–8 averaged colors (edges or quadrants) and painted as a gradient (a `Rectangle` gradient or a 4-corner `ShaderEffect`) with a `ColorAnimation`. Cost: the grab only. Averaging a 16×9 image is negligible.
- **History:** VLC had an "Atmo" Ambilight filter that drove external LED hardware. It was removed in 2015 (`git show 6713041eeb`, "atmo: remove video filter"), so there is nothing in-tree to reuse.
- **Sync:** lags like C, but it is less noticeable because nothing in the image has to line up with the video edge.
- **Verdict:** this is not a blur, and it changes the design language (colored panels). It is a plausible *separate* feature: the panel tint takes the video's dominant color instead of a fixed `bg.primary`. It works the same on every platform and fits "tint everywhere". If wanted, it should be its own ticket, and it needs the SPU-safe grab from C.

## E. Wayland: compositor-side blur cannot see the video

- `ext-background-effect-v1` (staging) blurs "the background behind" a surface, over a region "clipped by the compositor to the surface size". It says nothing about subsurfaces (protocol XML, [mirror](https://github.com/wayland-mirror/wayland-protocols/blob/main/staging/ext-background-effect/ext-background-effect-v1.xml); [wayland.app](https://wayland.app/protocols/ext-background-effect-v1) lists KWin, Mutter and niri as implementations).
- KWin, the reference implementation, reads the blur region from the **window's main surface only** (`w->surface()->blurRegion()`). In `drawWindow` it blurs the framebuffer and only *then* draws the whole window, subsurfaces included ([`src/plugins/blur/blur.cpp`](https://github.com/KDE/kwin/blob/cd18e4a1a620f671c3af9dc69e1df888a056c8be/src/plugins/blur/blur.cpp), lines 297-300 and 508-513). VLC's video is a subsurface placed below the UI surface (`modules/gui/qt/maininterface/compositor_wayland_module.c:308-310`), so it is never in the blurred background. This answers #22's open question "Does KWin's blur-behind include the window's own subsurfaces?": no.
- The same holds for X11 KWin blur-behind (`_KDE_NET_WM_BLUR_BEHIND_REGION`, same code path): it blurs other windows, not VLC's own composited video.
- So on Wayland only client-side routes remain (C, D, F, or video in the scene).

## F. Blur strips drawn by the vout itself

The D3D11 and OpenGL renderers already hold the frame on the GPU and draw the whole surface, letterbox included. They could draw a blurred copy into rectangles sent down from the UI, perfectly synced to the video on every compositor. That needs:

- a new window→display message to carry the rectangles;
- a blur pass in each renderer: d3d11, OpenGL (`modules/video_output/opengl/`), and libplacebo;
- care with geometry lag: the vout only picks up new rectangles on its next frame, and it redisplays every 80 ms when paused (`video_output.c:191`), so panel animations over paused video would stutter unless the UI forces a redisplay.

Size L, and it ties the UI's look to engine internals. A covers Windows for a fraction of the work. On Linux, F is the only synced option short of putting the video in the scene. It would be worth reconsidering only if Linux blur over video became a hard requirement.

## Recommendation

1. **Run the 1-day DComp spike** (A, three checks above).
2. **If it passes, adopt A on Windows `dcomp`.** Panels over video get the native blur under their tint. Every other compositor keeps the tint. Size M.
3. **Reopen "tint everywhere" in [#8](https://github.com/floklein/jvlc/issues/8) only as a narrow change**, after the spike: "tint is the baseline; Windows dcomp adds a native video blur beneath it". The Figma no-blur variant stays the reference design, because Linux, `win7` and `platform` render it. This mirrors how acrylic already differs by platform. If the user prefers one look across platforms, keep tint everywhere: nothing cheap gives Linux a synced blur.
4. **Don't build** the low-res copy (C) as a blur substitute, nor B, E or F.
5. **Optional, separate:** ambient-colored tint (D) as its own feature ticket, if the design wants it. It needs an SPU-safe grab in the core first.

## Open questions

- **Spike:** does one swap chain on two DComp visuals update live? Does the clip-on-parent/effect-on-child tree give clean edges? What does the blur cost in GPU time, and does it affect fullscreen overlay or independent flip and power?
- **Design (#8):** is a platform-dependent look over the video acceptable (blur under tint on Windows, tint only on Linux)?
- **Engine bug candidate:** with hardware decoding, do subtitles drop on the frame where a snapshot is taken? It would affect the existing snapshot button too. 10-minute test: take snapshots during subtitled d3d11va playback.
- **Feature idea:** panels tinted by the video's dominant color (D)?
