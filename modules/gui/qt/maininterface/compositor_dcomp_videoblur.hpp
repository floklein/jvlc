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
#ifndef COMPOSITOR_DCOMP_VIDEOBLUR_HPP
#define COMPOSITOR_DCOMP_VIDEOBLUR_HPP

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QMutex>
#include <QPointF>

#include <map>
#include <memory>
#include <vector>

#include <windows.h>
#include <wrl.h>

#include "qt.hpp"
#include "maininterface/videoblur.hpp"

class IDCompositionDevice;
class IDCompositionDevice3;
class IDCompositionVisual;
class IDCompositionVisual2;
class IDCompositionRectangleClip;
class IDCompositionGaussianBlurEffect;
class IDCompositionEffectGroup;
class MainCtx;
class QWindow;

namespace vlc
{

struct DCompLinearTransferEffect;

/**
 * Draws the Video blur with DirectComposition.
 *
 * The video output sets its swap chain as the content of the video visual.
 * A swap chain may be the content of several visuals, but only one blurred
 * copy of it follows the video: with one blurred visual per region, the
 * others kept stale GPU memory (#81). So a single blurred copy fills the
 * bounds of the regions, and sharp copies of the video (no effect) cover the
 * parts of the bounds outside the regions (`videoBlurLayout()`):
 *
 *   rootVisual()                  between the video visual and Qt's UI visual
 *   ├── blur clip visual          rectangle clip = the bounds of the regions
 *   │   └── blur visual           content = swap chain, offset = video position,
 *   │                             effect = Gaussian blur, then a linear transfer
 *   └── gap visual (per gap)      rectangle clip = the gap
 *       └── sharp visual          content = swap chain, offset = video position
 *
 * The linear transfer clamps the blurred video to the range of the Theme
 * (`VideoBlurProvider::Region::darkest`, `brightest`), so the text stays
 * readable under the Tint over the blur.
 *
 * The clip is on the parent and the effect on the child, so the blur samples
 * the video beyond the bounds instead of transparent pixels. A panel's blur
 * shows from half its fade.
 *
 * The swap chain never reaches the Qt module: `wrapVideoVisual()` returns the
 * visual to hand to the video output, which forwards every call to the real
 * video visual and also hands the content set by `SetContent()` to the blur,
 * as long as the blur exists.
 *
 * The provider is published as `MainCtx.videoBlurProvider` while the Video
 * blur is on: the "qt-video-blur" setting, Windows transparency effects and
 * battery saver decide (`VideoBlurGate`).
 */
class CompositorDCompositionVideoBlur : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    // Throws std::exception when DirectComposition lacks the blur effect (Windows 8).
    CompositorDCompositionVideoBlur(qt_intf_t *intf, IDCompositionDevice *device,
                                    MainCtx *mainCtx, QWindow *window, QObject *parent = nullptr);
    ~CompositorDCompositionVideoBlur() override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

    IDCompositionVisual *rootVisual() const;

    Microsoft::WRL::ComPtr<IDCompositionVisual> wrapVideoVisual(IDCompositionVisual *videoVisual);

    // Not committed: the caller commits the device along with the video visual.
    void setVideoPosition(const QPointF &position);

    // Drops the reference to the video swap chain, when the video output goes away.
    void clearVideoContent();

    void setVideoContent(IUnknown *content);

    // shared by the blur and the visuals it wraps for the video output, which
    // may outlive it
    struct ContentSink
    {
        QMutex lock;
        CompositorDCompositionVideoBlur *blur = nullptr;
    };

private:
    // a sharp copy of the video over a gap
    struct Gap
    {
        Microsoft::WRL::ComPtr<IDCompositionVisual2> clipVisual;
        Microsoft::WRL::ComPtr<IDCompositionVisual2> videoVisual;
    };

    void onRegionChanged(quintptr key, const VideoBlurProvider::Region &region);
    void onRegionRemoved(quintptr key);
    // m_lock held: lays out the blur and the gaps for the regions, and commits
    void relayoutLocked();

    void updateBatterySaver();

    qt_intf_t *m_intf = nullptr;
    MainCtx *m_mainCtx = nullptr;
    HWND m_hwnd = nullptr;
    VideoBlurProvider m_provider;
    VideoBlurGate m_gate {&m_provider};

    // battery saver (Windows 10), energy saver (Windows 11 24H2)
    HPOWERNOTIFY m_powerSavingNotification = nullptr;
    HPOWERNOTIFY m_energySaverNotification = nullptr;
    bool m_powerSaving = false;
    bool m_energySaver = false;

    Microsoft::WRL::ComPtr<IDCompositionDevice3> m_device;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> m_rootVisual;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> m_blurClipVisual;
    Microsoft::WRL::ComPtr<IDCompositionRectangleClip> m_blurClip;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> m_blurVisual;
    Microsoft::WRL::ComPtr<IDCompositionGaussianBlurEffect> m_blur;
    Microsoft::WRL::ComPtr<DCompLinearTransferEffect> m_clamp;
    bool m_blurShown = false;

    std::shared_ptr<ContentSink> m_sink;

    // Accessed from the GUI thread (regions), the render thread (position)
    // and the video output thread (content):
    QMutex m_lock;
    std::map<quintptr, VideoBlurProvider::Region> m_regions;
    std::vector<Gap> m_gaps;
    Microsoft::WRL::ComPtr<IUnknown> m_content;
    QPointF m_videoPosition;
};

}

#endif
