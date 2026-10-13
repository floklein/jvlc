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
#include <QMutex>
#include <QPointF>

#include <map>

#include <wrl.h>

#include "qt.hpp"
#include "maininterface/videoblur.hpp"

class IDCompositionDevice;
class IDCompositionDevice3;
class IDCompositionVisual;
class IDCompositionVisual2;
class IDCompositionRectangleClip;
class IDCompositionGaussianBlurEffect;

namespace vlc
{

/**
 * Draws the Video blur with DirectComposition.
 *
 * The video output sets its swap chain as the content of the video visual.
 * A swap chain may be the content of several visuals, so each region reported
 * by `VideoBlur` items gets a visual showing the same swap chain, blurred by
 * a Gaussian blur effect and clipped by its parent visual:
 *
 *   rootVisual()                 between the video visual and Qt's UI visual
 *   └── clip visual (per region)  rectangle clip = the region
 *       └── blur visual           content = swap chain, offset = video position,
 *                                 effect = Gaussian blur
 *
 * The clip is on the parent and the effect on the child, so the blur samples
 * the video beyond the region's edges instead of transparent pixels.
 *
 * The swap chain never reaches the Qt module: `wrapVideoVisual()` returns the
 * visual to hand to the video output, which forwards every call to the real
 * video visual and also hands the content set by `SetContent()` to the blur.
 */
class CompositorDCompositionVideoBlur : public QObject
{
    Q_OBJECT

public:
    // Throws std::exception when DirectComposition lacks the blur effect (Windows 8).
    CompositorDCompositionVideoBlur(qt_intf_t *intf, IDCompositionDevice *device, QObject *parent = nullptr);
    ~CompositorDCompositionVideoBlur() override;

    VideoBlurProvider *provider() { return &m_provider; }

    IDCompositionVisual *rootVisual() const;

    Microsoft::WRL::ComPtr<IDCompositionVisual> wrapVideoVisual(IDCompositionVisual *videoVisual);

    // Not committed: the caller commits the device along with the video visual.
    void setVideoPosition(const QPointF &position);

    // Drops the reference to the video swap chain, when the video output goes away.
    void clearVideoContent();

    void setVideoContent(IUnknown *content);

private:
    struct Region
    {
        Microsoft::WRL::ComPtr<IDCompositionVisual2> clipVisual;
        Microsoft::WRL::ComPtr<IDCompositionRectangleClip> clip;
        Microsoft::WRL::ComPtr<IDCompositionVisual2> blurVisual;
        Microsoft::WRL::ComPtr<IDCompositionGaussianBlurEffect> blur;
    };

    void onRegionChanged(quintptr key, const VideoBlurProvider::Region &region);
    void onRegionRemoved(quintptr key);

    qt_intf_t *m_intf = nullptr;
    VideoBlurProvider m_provider;

    Microsoft::WRL::ComPtr<IDCompositionDevice3> m_device;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> m_rootVisual;

    // Accessed from the GUI thread (regions), the render thread (position)
    // and the video output thread (content):
    QMutex m_lock;
    std::map<quintptr, Region> m_regions;
    Microsoft::WRL::ComPtr<IUnknown> m_content;
    QPointF m_videoPosition;
    float m_standardDeviation;
};

}

#endif
