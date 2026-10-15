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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// Win 8.1 for IDCompositionDevice3/IDCompositionVisual2
# if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0603) // _WIN32_WINNT_WINBLUE
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0603
# endif

#include "compositor_dcomp_videoblur.hpp"

#include <QGuiApplication>
#include <QMutexLocker>
#include <QWindow>

#include <atomic>
#include <stdexcept>

#include <dcomp.h>

#include "compositor_dcomp_error.hpp"
#include "compositor_dcomp_acrylicsurface.hpp"
#include "mainctx.hpp"

using Microsoft::WRL::ComPtr;

namespace vlc
{

namespace
{

/**
 * The visual handed to the video output instead of the real video visual.
 *
 * The video output only creates a swap chain and calls `SetContent()` with it
 * (modules/video_output/win32/dxgi_swapchain.cpp). Every call is forwarded to
 * the real video visual; the content is also handed to the Video blur.
 * This visual is never part of the visual tree.
 */
class VideoVisualProxy final : public IDCompositionVisual
{
public:
    using ContentSink = CompositorDCompositionVideoBlur::ContentSink;

    VideoVisualProxy(IDCompositionVisual *visual, std::shared_ptr<ContentSink> sink)
        : m_visual(visual)
        , m_sink(std::move(sink))
    {
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
    {
        if (!object)
            return E_POINTER;
        // mingw-w64 declares no __uuidof(IDCompositionVisual):
        static constexpr GUID IID_VISUAL = { 0x4d93059d, 0x097b, 0x4651, { 0x9a, 0x60, 0xf0, 0xf2, 0x51, 0x16, 0xe2, 0xf3 } };
        if (riid == __uuidof(IUnknown) || riid == IID_VISUAL)
        {
            *object = static_cast<IDCompositionVisual *>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++m_refCount;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG refCount = --m_refCount;
        if (refCount == 0)
            delete this;
        return refCount;
    }

    // IDCompositionVisual
    HRESULT STDMETHODCALLTYPE SetOffsetX(IDCompositionAnimation *animation) override
    {
        return m_visual->SetOffsetX(animation);
    }

    HRESULT STDMETHODCALLTYPE SetOffsetX(float offsetX) override
    {
        return m_visual->SetOffsetX(offsetX);
    }

    HRESULT STDMETHODCALLTYPE SetOffsetY(IDCompositionAnimation *animation) override
    {
        return m_visual->SetOffsetY(animation);
    }

    HRESULT STDMETHODCALLTYPE SetOffsetY(float offsetY) override
    {
        return m_visual->SetOffsetY(offsetY);
    }

    HRESULT STDMETHODCALLTYPE SetTransform(IDCompositionTransform *transform) override
    {
        return m_visual->SetTransform(transform);
    }

    HRESULT STDMETHODCALLTYPE SetTransform(const D2D_MATRIX_3X2_F &matrix) override
    {
        return m_visual->SetTransform(matrix);
    }

    HRESULT STDMETHODCALLTYPE SetTransformParent(IDCompositionVisual *visual) override
    {
        return m_visual->SetTransformParent(visual);
    }

    HRESULT STDMETHODCALLTYPE SetEffect(IDCompositionEffect *effect) override
    {
        return m_visual->SetEffect(effect);
    }

    HRESULT STDMETHODCALLTYPE SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE mode) override
    {
        return m_visual->SetBitmapInterpolationMode(mode);
    }

    HRESULT STDMETHODCALLTYPE SetBorderMode(DCOMPOSITION_BORDER_MODE mode) override
    {
        return m_visual->SetBorderMode(mode);
    }

    HRESULT STDMETHODCALLTYPE SetClip(IDCompositionClip *clip) override
    {
        return m_visual->SetClip(clip);
    }

    HRESULT STDMETHODCALLTYPE SetClip(const D2D_RECT_F &rect) override
    {
        return m_visual->SetClip(rect);
    }

    HRESULT STDMETHODCALLTYPE SetContent(IUnknown *content) override
    {
        const HRESULT hr = m_visual->SetContent(content);
        // the blur may be gone already (the window surface was destroyed
        // while the video output lives)
        QMutexLocker lock(&m_sink->lock);
        if (SUCCEEDED(hr) && m_sink->blur)
            m_sink->blur->setVideoContent(content);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE AddVisual(IDCompositionVisual *visual, BOOL insertAbove, IDCompositionVisual *referenceVisual) override
    {
        return m_visual->AddVisual(visual, insertAbove, referenceVisual);
    }

    HRESULT STDMETHODCALLTYPE RemoveVisual(IDCompositionVisual *visual) override
    {
        return m_visual->RemoveVisual(visual);
    }

    HRESULT STDMETHODCALLTYPE RemoveAllVisuals() override
    {
        return m_visual->RemoveAllVisuals();
    }

    HRESULT STDMETHODCALLTYPE SetCompositeMode(DCOMPOSITION_COMPOSITE_MODE mode) override
    {
        return m_visual->SetCompositeMode(mode);
    }

private:
    ~VideoVisualProxy() = default;

    std::atomic<ULONG> m_refCount {1};
    ComPtr<IDCompositionVisual> m_visual;
    const std::shared_ptr<ContentSink> m_sink;
};

}

/**
 * IDCompositionLinearTransferEffect (Windows 8.1), which mingw-w64's dcomp.h
 * does not declare: `CreateLinearTransferEffect()` returns a `void *`.
 *
 * Each channel becomes `slope * channel + intercept`. MSVC lays out the
 * overloads of a method in reverse order, so for GCC the animation overload
 * comes first, as in mingw-w64's other effects.
 */
struct DCompLinearTransferEffect : public IDCompositionFilterEffect
{
#define LINEAR_TRANSFER_CHANNEL(Channel) \
    STDMETHOD(Set##Channel##YIntercept)(IDCompositionAnimation *animation) PURE; \
    STDMETHOD(Set##Channel##YIntercept)(float yIntercept) PURE; \
    STDMETHOD(Set##Channel##Slope)(IDCompositionAnimation *animation) PURE; \
    STDMETHOD(Set##Channel##Slope)(float slope) PURE; \
    STDMETHOD(Set##Channel##Disable)(BOOL disable) PURE;

    LINEAR_TRANSFER_CHANNEL(Red)
    LINEAR_TRANSFER_CHANNEL(Green)
    LINEAR_TRANSFER_CHANNEL(Blue)
    LINEAR_TRANSFER_CHANNEL(Alpha)
#undef LINEAR_TRANSFER_CHANNEL

    STDMETHOD(SetClampOutput)(BOOL clampOutput) PURE;
};

namespace
{

// GUID_POWER_SAVING_STATUS: battery saver, DWORD 0 or 1
constexpr GUID POWER_SAVING_STATUS = { 0xe00958c0, 0xc213, 0x4ace, { 0xac, 0x77, 0xfe, 0xcc, 0xed, 0x2e, 0xee, 0xa5 } };
// GUID_ENERGY_SAVER_STATUS (Windows 11 24H2): DWORD, 0 when off
constexpr GUID ENERGY_SAVER_STATUS = { 0x550e8400, 0xe29b, 0x41d4, { 0xa7, 0x16, 0x44, 0x66, 0x55, 0x44, 0x00, 0x00 } };

}

CompositorDCompositionVideoBlur::CompositorDCompositionVideoBlur(qt_intf_t *intf, IDCompositionDevice *device,
                                                                 MainCtx *mainCtx, QWindow *window, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
    , m_mainCtx(mainCtx)
{
    assert(device);
    assert(mainCtx);
    assert(window);

    // CreateGaussianBlurEffect() needs IDCompositionDevice3 (Windows 8.1+):
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&m_device))))
        throw std::runtime_error("DirectComposition device is not IDCompositionDevice3, no video blur");

    if (FAILED(m_device->CreateVisual(&m_rootVisual)))
        throw std::runtime_error("failed to create the video blur root visual");

    try
    {
        HR(m_device->CreateVisual(&m_blurClipVisual), "create video blur clip visual");
        HR(m_device->CreateRectangleClip(&m_blurClip), "create video blur clip");
        HR(m_device->CreateVisual(&m_blurVisual), "create video blur visual");
        HR(m_device->CreateGaussianBlurEffect(&m_blur), "create video blur effect");
        HR(m_device->CreateLinearTransferEffect(reinterpret_cast<void **>(m_clamp.ReleaseAndGetAddressOf())),
           "create video blur clamp");

        // Hard borders: the blur does not fade to transparent at the edges
        // of the video.
        HR(m_blur->SetBorderMode(D2D1_BORDER_MODE_HARD), "set video blur border mode");
        // The video, blurred, then clamped (makeThemeTokens() computes the
        // Tint over the blur from the clamped range). Opaque, so the raw
        // video never shows through where the blur is transparent.
        HR(m_clamp->SetInput(0, m_blur.Get(), 0), "set video blur clamp input");
        HR(m_clamp->SetClampOutput(TRUE), "set video blur clamp output");
        HR(m_clamp->SetAlphaSlope(0.f), "set video blur clamp alpha");
        HR(m_clamp->SetAlphaYIntercept(1.f), "set video blur clamp alpha");
        HR(m_blurVisual->SetEffect(m_clamp.Get()), "set video blur effect");
        HR(m_blurClipVisual->SetClip(m_blurClip.Get()), "set video blur clip");
        HR(m_blurClipVisual->AddVisual(m_blurVisual.Get(), FALSE, nullptr), "add video blur visual");
    }
    catch (const DXError &err)
    {
        throw std::runtime_error(err.what());
    }

    m_sink = std::make_shared<ContentSink>();
    m_sink->blur = this;

    connect(&m_provider, &VideoBlurProvider::regionChanged,
            this, &CompositorDCompositionVideoBlur::onRegionChanged);
    connect(&m_provider, &VideoBlurProvider::regionRemoved,
            this, &CompositorDCompositionVideoBlur::onRegionRemoved);

    // While the gate is closed, MainCtx.videoBlurProvider is null: the panels
    // keep the plain Tint and their VideoBlur items withdraw their regions.
    m_gate.setEnabled(m_mainCtx->videoBlurOption());
    m_gate.setTransparencyEffects(isTransparencyEffectsEnabled());
    updateBatterySaver();
    connect(m_mainCtx, &MainCtx::videoBlurOptionChanged, this, [this]() {
        m_gate.setEnabled(m_mainCtx->videoBlurOption());
    });
    connect(&m_gate, &VideoBlurGate::providerChanged, m_mainCtx, &MainCtx::setVideoBlurProvider);
    m_mainCtx->setVideoBlurProvider(m_gate.provider());

    m_hwnd = reinterpret_cast<HWND>(window->winId());
    qApp->installNativeEventFilter(this);
    // Each registration also sends the current state at once:
    m_powerSavingNotification = RegisterPowerSettingNotification(m_hwnd, &POWER_SAVING_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE);
    m_energySaverNotification = RegisterPowerSettingNotification(m_hwnd, &ENERGY_SAVER_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE);
}

CompositorDCompositionVideoBlur::~CompositorDCompositionVideoBlur()
{
    // the visuals handed to the video output stop calling it (waiting for a
    // call in progress); always the sink lock, then m_lock
    {
        QMutexLocker lock(&m_sink->lock);
        m_sink->blur = nullptr;
    }

    qApp->removeNativeEventFilter(this);
    if (m_powerSavingNotification)
        UnregisterPowerSettingNotification(m_powerSavingNotification);
    if (m_energySaverNotification)
        UnregisterPowerSettingNotification(m_energySaverNotification);

    QMutexLocker lock(&m_lock);
    if (m_rootVisual)
        m_rootVisual->RemoveAllVisuals();
    m_regions.clear();
    m_gaps.clear();
}

bool CompositorDCompositionVideoBlur::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
    const MSG *const msg = static_cast<MSG *>(message);
    if (msg->hwnd != m_hwnd)
        return false;

    switch (msg->message)
    {
    case WM_SETTINGCHANGE:
        // Windows "Transparency effects" (and high contrast)
        if (msg->lParam && !lstrcmpW(reinterpret_cast<LPCWSTR>(msg->lParam), L"ImmersiveColorSet"))
            m_gate.setTransparencyEffects(isTransparencyEffectsEnabled());
        break;
    case WM_SYSCOLORCHANGE:
        m_gate.setTransparencyEffects(isTransparencyEffectsEnabled());
        break;
    case WM_POWERBROADCAST:
        if (msg->wParam == PBT_POWERSETTINGCHANGE && msg->lParam)
        {
            const auto setting = reinterpret_cast<const POWERBROADCAST_SETTING *>(msg->lParam);
            if (setting->DataLength < sizeof(DWORD))
                break;
            const DWORD value = *reinterpret_cast<const DWORD *>(setting->Data);
            if (setting->PowerSetting == POWER_SAVING_STATUS)
                m_powerSaving = value != 0;
            else if (setting->PowerSetting == ENERGY_SAVER_STATUS)
                m_energySaver = value != 0;
            else
                break;
            m_gate.setBatterySaver(m_powerSaving || m_energySaver);
        }
        else if (msg->wParam == PBT_APMPOWERSTATUSCHANGE)
            updateBatterySaver();
        break;
    }

    return false;
}

void CompositorDCompositionVideoBlur::updateBatterySaver()
{
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status))
        m_powerSaving = status.SystemStatusFlag == 1;
    m_gate.setBatterySaver(m_powerSaving || m_energySaver);
}

IDCompositionVisual *CompositorDCompositionVideoBlur::rootVisual() const
{
    return m_rootVisual.Get();
}

ComPtr<IDCompositionVisual> CompositorDCompositionVideoBlur::wrapVideoVisual(IDCompositionVisual *videoVisual)
{
    assert(videoVisual);
    ComPtr<IDCompositionVisual> proxy;
    proxy.Attach(new VideoVisualProxy(videoVisual, m_sink));
    return proxy;
}

void CompositorDCompositionVideoBlur::setVideoPosition(const QPointF &position)
{
    QMutexLocker lock(&m_lock);
    m_videoPosition = position;
    m_blurVisual->SetOffsetX(position.x());
    m_blurVisual->SetOffsetY(position.y());
    for (Gap &gap : m_gaps)
    {
        gap.videoVisual->SetOffsetX(position.x());
        gap.videoVisual->SetOffsetY(position.y());
    }
}

void CompositorDCompositionVideoBlur::setVideoContent(IUnknown *content)
{
    QMutexLocker lock(&m_lock);
    m_content = content;
    m_blurVisual->SetContent(content);
    for (Gap &gap : m_gaps)
        gap.videoVisual->SetContent(content);
    // The video output commits the device after `SetContent()`.
}

void CompositorDCompositionVideoBlur::clearVideoContent()
{
    setVideoContent(nullptr);
    m_device->Commit();
}

void CompositorDCompositionVideoBlur::onRegionChanged(quintptr key, const VideoBlurProvider::Region &value)
{
    QMutexLocker lock(&m_lock);
    m_regions[key] = value;
    relayoutLocked();
}

void CompositorDCompositionVideoBlur::onRegionRemoved(quintptr key)
{
    QMutexLocker lock(&m_lock);
    if (m_regions.erase(key) == 0)
        return;
    relayoutLocked();
}

void CompositorDCompositionVideoBlur::relayoutLocked()
{
    QList<VideoBlurProvider::Region> regions;
    for (const auto &[key, region] : m_regions)
        regions.append(region);
    const VideoBlurLayout layout = videoBlurLayout(regions);

    try
    {
        if (layout.bounds.isEmpty())
        {
            // Detached, the blur costs nothing to compose:
            if (m_blurShown)
                HR(m_rootVisual->RemoveAllVisuals(), "remove the video blur");
            m_blurShown = false;
            m_gaps.clear();
            m_device->Commit();
            return;
        }

        // The regions share the blur: the strength follows the scale and the
        // clamped range the Theme, the same for every panel
        const VideoBlurProvider::Region &first = regions.first();
        HR(m_blur->SetStandardDeviation(static_cast<float>(first.deviation)), "set video blur deviation");
        // each channel from [0, 1] into [darkest, brightest]
        const float slope = static_cast<float>(first.brightest - first.darkest);
        const float intercept = static_cast<float>(first.darkest);
        m_clamp->SetRedSlope(slope);
        m_clamp->SetRedYIntercept(intercept);
        m_clamp->SetGreenSlope(slope);
        m_clamp->SetGreenYIntercept(intercept);
        m_clamp->SetBlueSlope(slope);
        m_clamp->SetBlueYIntercept(intercept);

        m_blurClip->SetLeft(layout.bounds.left());
        m_blurClip->SetTop(layout.bounds.top());
        m_blurClip->SetRight(layout.bounds.right());
        m_blurClip->SetBottom(layout.bounds.bottom());
        if (!m_blurShown)
        {
            HR(m_blurVisual->SetOffsetX(m_videoPosition.x()), "set video blur offset");
            HR(m_blurVisual->SetOffsetY(m_videoPosition.y()), "set video blur offset");
            HR(m_blurVisual->SetContent(m_content.Get()), "set video blur content");
            // at the bottom (TRUE without a reference)
            HR(m_rootVisual->AddVisual(m_blurClipVisual.Get(), TRUE, nullptr), "add the video blur");
            m_blurShown = true;
        }

        // a sharp copy of the video over each gap, above the blur
        while (m_gaps.size() > static_cast<size_t>(layout.gaps.size()))
        {
            m_rootVisual->RemoveVisual(m_gaps.back().clipVisual.Get());
            m_gaps.pop_back();
        }
        while (m_gaps.size() < static_cast<size_t>(layout.gaps.size()))
        {
            Gap gap;
            HR(m_device->CreateVisual(&gap.clipVisual), "create video blur gap");
            HR(m_device->CreateVisual(&gap.videoVisual), "create video blur gap video");
            HR(gap.videoVisual->SetOffsetX(m_videoPosition.x()), "set video blur gap offset");
            HR(gap.videoVisual->SetOffsetY(m_videoPosition.y()), "set video blur gap offset");
            HR(gap.videoVisual->SetContent(m_content.Get()), "set video blur gap content");
            HR(gap.clipVisual->AddVisual(gap.videoVisual.Get(), FALSE, nullptr), "add video blur gap video");
            // on top (FALSE without a reference)
            HR(m_rootVisual->AddVisual(gap.clipVisual.Get(), FALSE, nullptr), "add video blur gap");
            m_gaps.push_back(std::move(gap));
        }
        for (size_t i = 0; i < m_gaps.size(); ++i)
        {
            const QRectF &rect = layout.gaps[i];
            HR(m_gaps[i].clipVisual->SetClip(D2D_RECT_F{ static_cast<float>(rect.left()),
                                                          static_cast<float>(rect.top()),
                                                          static_cast<float>(rect.right()),
                                                          static_cast<float>(rect.bottom()) }),
               "set video blur gap clip");
        }
    }
    catch (const DXError &err)
    {
        msg_Err(m_intf, "failed to lay out the video blur: '%s' code: 0x%lX", err.what(), err.code());
    }

    m_device->Commit();
}

}
