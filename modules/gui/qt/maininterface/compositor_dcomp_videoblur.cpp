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
    VideoVisualProxy(IDCompositionVisual *visual, CompositorDCompositionVideoBlur *blur)
        : m_visual(visual)
        , m_blur(blur)
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
        if (SUCCEEDED(hr))
            m_blur->setVideoContent(content);
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
    CompositorDCompositionVideoBlur *m_blur;
};

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
    qApp->removeNativeEventFilter(this);
    if (m_powerSavingNotification)
        UnregisterPowerSettingNotification(m_powerSavingNotification);
    if (m_energySaverNotification)
        UnregisterPowerSettingNotification(m_energySaverNotification);

    QMutexLocker lock(&m_lock);
    if (m_rootVisual)
        m_rootVisual->RemoveAllVisuals();
    m_regions.clear();
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
    proxy.Attach(new VideoVisualProxy(videoVisual, this));
    return proxy;
}

void CompositorDCompositionVideoBlur::setVideoPosition(const QPointF &position)
{
    QMutexLocker lock(&m_lock);
    m_videoPosition = position;
    for (auto &[key, region] : m_regions)
    {
        region.blurVisual->SetOffsetX(position.x());
        region.blurVisual->SetOffsetY(position.y());
    }
}

void CompositorDCompositionVideoBlur::setVideoContent(IUnknown *content)
{
    QMutexLocker lock(&m_lock);
    m_content = content;
    for (auto &[key, region] : m_regions)
        region.blurVisual->SetContent(content);
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

    auto it = m_regions.find(key);
    if (it == m_regions.end())
    {
        Region region;
        try
        {
            HR(m_device->CreateVisual(&region.clipVisual), "create video blur clip visual");
            HR(m_device->CreateRectangleClip(&region.clip), "create video blur clip");
            HR(m_device->CreateEffectGroup(&region.fade), "create video blur fade");
            HR(m_device->CreateVisual(&region.blurVisual), "create video blur visual");
            HR(m_device->CreateGaussianBlurEffect(&region.blur), "create video blur effect");

            // Hard borders: the blur does not fade to transparent at the
            // edges of the video.
            HR(region.blur->SetBorderMode(D2D1_BORDER_MODE_HARD), "set video blur border mode");
            HR(region.blurVisual->SetEffect(region.blur.Get()), "set video blur effect");
            HR(region.clipVisual->SetEffect(region.fade.Get()), "set video blur fade");
            HR(region.blurVisual->SetOffsetX(m_videoPosition.x()), "set video blur offset");
            HR(region.blurVisual->SetOffsetY(m_videoPosition.y()), "set video blur offset");
            HR(region.blurVisual->SetContent(m_content.Get()), "set video blur content");
            HR(region.clipVisual->SetClip(region.clip.Get()), "set video blur clip");
            HR(region.clipVisual->AddVisual(region.blurVisual.Get(), FALSE, nullptr), "add video blur visual");
            HR(m_rootVisual->AddVisual(region.clipVisual.Get(), TRUE, nullptr), "add video blur clip visual");
        }
        catch (const DXError &err)
        {
            msg_Err(m_intf, "failed to create a video blur region: '%s' code: 0x%lX", err.what(), err.code());
            return;
        }
        it = m_regions.emplace(key, std::move(region)).first;
    }

    // the blur fades with its panel, and its strength follows the scale
    it->second.fade->SetOpacity(static_cast<float>(value.opacity));
    it->second.blur->SetStandardDeviation(static_cast<float>(value.deviation));

    IDCompositionRectangleClip *const clip = it->second.clip.Get();
    const QRectF &rect = value.rect;
    clip->SetLeft(rect.left());
    clip->SetTop(rect.top());
    clip->SetRight(rect.right());
    clip->SetBottom(rect.bottom());

    const float radius = value.radius;
    clip->SetTopLeftRadiusX(radius);
    clip->SetTopLeftRadiusY(radius);
    clip->SetTopRightRadiusX(radius);
    clip->SetTopRightRadiusY(radius);
    clip->SetBottomLeftRadiusX(radius);
    clip->SetBottomLeftRadiusY(radius);
    clip->SetBottomRightRadiusX(radius);
    clip->SetBottomRightRadiusY(radius);

    m_device->Commit();
}

void CompositorDCompositionVideoBlur::onRegionRemoved(quintptr key)
{
    QMutexLocker lock(&m_lock);

    const auto it = m_regions.find(key);
    if (it == m_regions.end())
        return;

    // Detached, the region costs nothing to compose:
    m_rootVisual->RemoveVisual(it->second.clipVisual.Get());
    m_regions.erase(it);
    m_device->Commit();
}

}
