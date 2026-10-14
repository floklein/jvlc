/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "systempalette.hpp"
#include "themetokens.hpp"
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <maininterface/mainctx.hpp>
#include <vlc_modules.h>
#include <vlc_picture.h>

namespace {

QColor setColorAlpha(const QColor& c1, float alpha)
{
    QColor c(c1);
    c.setAlphaF(alpha);
    return c;
}

static void PaletteChangedCallback(vlc_qt_theme_provider_t*, void* data)
{
    auto priv = static_cast<ExternalPaletteImpl*>(data);
    emit priv->paletteChanged();
}

static void MetricsChangedCallback(vlc_qt_theme_provider_t*, vlc_qt_theme_image_type type, void* data)
{
    auto priv = static_cast<ExternalPaletteImpl*>(data);
    assert(priv);
    priv->updateMetrics(type);
}


static void ReleaseVLCPictureCb(void* data)
{
    auto pic = static_cast<picture_t*>(data);
    if (pic)
        picture_Release(pic);
}


static void setColorRBGAInt(
    vlc_qt_theme_provider_t* obj,
    vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
    vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
    int r, int g, int b, int a)
{
    auto palette = static_cast<SystemPalette*>(obj->setColorData);
    QColor color(r,g,b,a);
    palette->setColor(
        static_cast<ColorContext::ColorSet>(set), static_cast<ColorContext::ColorSection>(section),
        static_cast<ColorContext::ColorName>(name), static_cast<ColorContext::ColorState>(state),
        color);
}

static void setColorRBGAFloat(
    vlc_qt_theme_provider_t* obj,
    vlc_qt_theme_color_set set, vlc_qt_theme_color_section section,
    vlc_qt_theme_color_name name, vlc_qt_theme_color_state state,
    double r, double g, double b, double a)
{
    auto palette = static_cast<SystemPalette*>(obj->setColorData);
    QColor color;
    color.setRgbF(r,g,b,a);
    palette->setColor(
        static_cast<ColorContext::ColorSet>(set), static_cast<ColorContext::ColorSection>(section),
        static_cast<ColorContext::ColorName>(name), static_cast<ColorContext::ColorState>(state),
        color);
}

}

//ExternalPaletteImpl

ExternalPaletteImpl::ExternalPaletteImpl(MainCtx* ctx, SystemPalette& palette, QObject* parent)
    : QObject(parent)
    , m_palette(palette)
    , m_ctx(ctx)
{
}

ExternalPaletteImpl::~ExternalPaletteImpl()
{
    if (m_provider)
    {
        if (m_provider->close)
            m_provider->close(m_provider);
        if (m_module)
            module_unneed(m_provider, m_module);
        vlc_object_delete(m_provider);
    }
}

bool ExternalPaletteImpl::init()
{
    m_provider = vlc_object_create<vlc_qt_theme_provider_t>(m_ctx->getIntf());
    if (!m_provider)
        return false;


    m_provider->paletteUpdated = PaletteChangedCallback;
    m_provider->paletteUpdatedData = this;

    m_provider->metricsUpdated = MetricsChangedCallback;
    m_provider->metricsUpdatedData = this;

    m_provider->setColorF = setColorRBGAFloat;
    m_provider->setColorInt = setColorRBGAInt;
    m_provider->setColorData = &m_palette;

    m_module = module_need(m_provider, "qt theme provider", nullptr, true);
    if (!m_module)
        return false;
    return true;
}

bool ExternalPaletteImpl::isThemeDark() const
{
    if (!m_provider->isThemeDark)
        return false;
    return m_provider->isThemeDark(m_provider);
}

bool ExternalPaletteImpl::hasCSDImages() const
{
    if (!m_provider->supportThemeImage)
        return false;
    return m_provider->supportThemeImage(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON);
}


QImage ExternalPaletteImpl::getCSDImage(vlc_qt_theme_csd_button_type type, vlc_qt_theme_csd_button_state state, bool maximized, bool active, int bannerHeight)
{
    if (!m_provider->getThemeImage)
        return {};
    vlc_qt_theme_image_setting imageSettings;
    imageSettings.windowScaleFactor = m_ctx->intfMainWindow()->devicePixelRatio();
    imageSettings.userScaleFacor = m_ctx->getIntfUserScaleFactor();
    imageSettings.u.csdButton.buttonType = type;
    imageSettings.u.csdButton.state = state;
    imageSettings.u.csdButton.maximized = maximized;
    imageSettings.u.csdButton.active = active;
    imageSettings.u.csdButton.bannerHeight = bannerHeight;
    picture_t* pic = m_provider->getThemeImage(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON, &imageSettings);
    if (!pic)
        return {};

    QImage::Format format = QImage::Format_Invalid;
    switch (pic->format.i_chroma)
    {
    case VLC_CODEC_ARGB:
        format = QImage::Format_ARGB32_Premultiplied;
        break;
    case VLC_CODEC_RGBA:
        format = QImage::Format_RGBA8888_Premultiplied;
        break;
    case VLC_CODEC_RGB24:
        format = QImage::Format_RGB888;
        break;
    default:
        msg_Err(m_ctx->getIntf(), "unexpected image format from theme provider");
        break;
    }

    return QImage(pic->p[0].p_pixels,
            pic->format.i_visible_width, pic->format.i_visible_height, pic->p[0].i_pitch,
            format,
            ReleaseVLCPictureCb, pic
            );
}

CSDMetrics* ExternalPaletteImpl::getCSDMetrics() const
{
    return m_csdMetrics.get();
}

int ExternalPaletteImpl::update()
{
    if (m_provider->updatePalette)
        return m_provider->updatePalette(m_provider);
    return VLC_EGENERIC;
}

void ExternalPaletteImpl::updateMetrics(vlc_qt_theme_image_type type)
{
    if (m_provider->getThemeMetrics)
    {
        if (type == VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON)
        {
            m_csdMetrics.reset();
            [&](){
                if (!m_provider->getThemeMetrics)
                    return;
                vlc_qt_theme_metrics metrics;
                memset(&metrics, 0, sizeof(metrics));
                bool ret = m_provider->getThemeMetrics(m_provider, VLC_QT_THEME_IMAGE_TYPE_CSD_BUTTON, &metrics);
                if (!ret)
                    return;
                m_csdMetrics = std::make_unique<CSDMetrics>();
                m_csdMetrics->interNavButtonSpacing = metrics.u.csd.interNavButtonSpacing;

                m_csdMetrics->csdFrameMarginLeft = metrics.u.csd.csdFrameMarginLeft;
                m_csdMetrics->csdFrameMarginRight = metrics.u.csd.csdFrameMarginRight;
                m_csdMetrics->csdFrameMarginTop = metrics.u.csd.csdFrameMarginTop;
                m_csdMetrics->csdFrameMarginBottom = metrics.u.csd.csdFrameMarginBottom;
            }();
            emit CSDMetricsChanged();
        }
    }
}

SystemPalette::SystemPalette(QObject* parent)
    : QObject(parent)
{

}

void SystemPalette::loadAccentColor()
{
    assert(m_ctx);
    const QString accentColor = m_ctx->accentColor();

    if (accentColor == QLatin1String("default"))
    {
        // Use default
        m_accent = QColor();
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    else if (accentColor == QLatin1String("system"))
    {
        assert(qApp);
        m_accent = qApp->palette().color(QPalette::Normal, QPalette::Accent);
    }
#endif
    else
    {
        m_accent = QColor(accentColor);
    }
}

void SystemPalette::componentComplete()
{
    loadAccentColor();

    // the Accent color can change in Settings
    connect(m_ctx, &MainCtx::accentColorChanged, this, [this]() {
        loadAccentColor();
        updatePalette();
    });

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // follow the operating system's light/dark setting when no theme provider does
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this]() {
        if (m_source == ColorSchemeModel::System && !m_palettePriv)
            updatePalette();
    });
#endif

    updatePalette();
}

bool SystemPalette::isSystemThemeDark() const
{
    if (m_palettePriv)
        return m_palettePriv->isThemeDark();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qGuiApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

ColorSchemeModel::ColorScheme SystemPalette::source() const
{
    return m_source;
}

QImage SystemPalette::getCSDImage(vlc_qt_theme_csd_button_type type, vlc_qt_theme_csd_button_state state, bool maximized, bool active, int bannerHeight)
{
    if (!hasCSDImage())
        return QImage();
    assert(m_palettePriv);
    return m_palettePriv->getCSDImage(type, state, maximized, active, bannerHeight);

}

CSDMetrics* SystemPalette::getCSDMetrics() const
{
    if (!m_palettePriv)
        return nullptr;
    return m_palettePriv->getCSDMetrics();
}

bool SystemPalette::hasCSDImage() const
{
    if (!m_palettePriv)
        return false;
    return m_palettePriv->hasCSDImages();
}

void SystemPalette::setSource(ColorSchemeModel::ColorScheme source)
{
    if (m_source == source)
        return;
    m_source = source;

    updatePalette();
    emit sourceChanged();
}

void SystemPalette::setCtx(MainCtx* ctx)
{
    if (ctx == m_ctx)
        return;
    m_ctx = ctx;
    emit ctxChanged();
    updatePalette();
}

void SystemPalette::updatePalette()
{
    m_palettePriv.reset();

    // the theme provider (Windows) detects the system theme and High Contrast
    if (m_ctx)
    {
        auto palette = std::make_unique<ExternalPaletteImpl>(m_ctx, *this);
        if (palette->init())
            m_palettePriv = std::move(palette);
    }

    m_isDark = resolveDarkTheme(m_source, isSystemThemeDark());

    m_colorMap.clear();
    // High Contrast colors take precedence over any Theme
    if (m_palettePriv && m_palettePriv->update() == VLC_SUCCESS)
    {
        const QColor background = getColor(ColorContext::View, ColorContext::Bg,
                                            ColorContext::Primary, ColorContext::Normal);
        m_isDark = background.lightnessF() < 0.5;
    }
    else
    {
        // the provider may have set some colors before failing
        m_colorMap.clear();
        makeThemePalette(makeThemeTokens(m_isDark, m_accent));
    }

    if (m_palettePriv)
    {
        connect(
            m_palettePriv.get(), &ExternalPaletteImpl::paletteChanged,
            this, &SystemPalette::updatePalette);
        connect(
            m_palettePriv.get(), &ExternalPaletteImpl::CSDMetricsChanged,
            this, &SystemPalette::CSDMetricsChanged);
    }
    emit paletteChanged();

    bool hasCSDImage = m_palettePriv && m_palettePriv->hasCSDImages();
    if (m_hasCSDImage != hasCSDImage)
    {
        m_hasCSDImage = hasCSDImage;
        emit hasCSDImageChanged();
    }
}

static quint64 makeKey(ColorContext::ColorSet colorSet, ColorContext::ColorSection section,
                       ColorContext::ColorName name, ColorContext::ColorState state)
{
    static_assert(VQTC_STATE_COUNT < (1<<4), "");
    static_assert(VQTC_SECTION_COUNT < (1<<4), "");
    static_assert(VQTC_NAME_COUNT < (1<<8), "");
    static_assert(VQTC_SET_COUNT < (1<<16), "");
    return  (colorSet << 16)
        + (name << 8)
        + (section << 4)
        + state;
}

void SystemPalette::setColor(ColorContext::ColorSet colorSet,  ColorContext::ColorSection section,
                             ColorContext::ColorName name, ColorContext::ColorState state, QColor color)
{

    quint64 key = makeKey(colorSet, section, name, state);
    m_colorMap[key] = color;
}


QColor SystemPalette::getColor(ColorContext::ColorSet colorSet, ColorContext::ColorSection section,
                               ColorContext::ColorName name, ColorContext::ColorState state) const
{
    typedef ColorContext C;

    quint64 key = makeKey(colorSet, section, name, state);
    auto it = m_colorMap.find(key);
    if (it != m_colorMap.cend())
        return *it;
    //we don't have the role explicitly set, fallback to the normal state
    key = makeKey(colorSet, section, name, C::Normal);
    it = m_colorMap.find(key);
    if (it != m_colorMap.cend())
        return *it;
    //we don't have the role explicitly set, fallback to the colorSet View
    //TODO do we want finer hierarchy?
    if (colorSet != C::View)
    {
        return getColor(C::View, section, name, state);
    }
    else
    {
        //nothing matches, that's probably an issue, return an ugly color
        return Qt::magenta;
    }
}

void SystemPalette::makeThemePalette(const ThemeTokens& t)
{
    typedef ColorContext C;

    const QColor transparent = Qt::transparent;

    //base set
    {
        C::ColorSet CS = C::View;
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgB1);
        setColor(CS, C::Bg, C::Secondary, C::Normal, t.bgB2);
        setColor(CS, C::Bg, C::Highlight, C::Normal, t.bgA2);

        setColor(CS, C::Fg, C::Primary, C::Normal, t.fg2);
        setColor(CS, C::Fg, C::Primary, C::Disabled, t.fgA4);
        setColor(CS, C::Fg, C::Secondary, C::Normal, t.fgA1);
        setColor(CS, C::Fg, C::Secondary, C::Disabled, t.fgA4);
        setColor(CS, C::Fg, C::Highlight, C::Normal, t.fg1);
        setColor(CS, C::Fg, C::Link, C::Normal, t.accentMain);

        setColor(CS, C::Bg, C::Negative, C::Normal, t.negativeBg);
        setColor(CS, C::Fg, C::Negative, C::Normal, t.negative);
        setColor(CS, C::Bg, C::Neutral, C::Normal, t.neutralBg);
        setColor(CS, C::Fg, C::Neutral, C::Normal, t.neutral);
        setColor(CS, C::Bg, C::Positive, C::Normal, t.positiveBg);
        setColor(CS, C::Fg, C::Positive, C::Normal, t.positive);

        setColor(CS, C::Decoration, C::VisualFocus, C::Normal, setColorAlpha(t.accentMain, 0.0));
        setColor(CS, C::Decoration, C::VisualFocus, C::Focused, t.accentMain);

        setColor(CS, C::Decoration, C::Border, C::Normal, t.dividerBright);
        setColor(CS, C::Decoration, C::Border, C::Focused, t.fgA3);
        setColor(CS, C::Decoration, C::Border, C::Hovered, t.fgA3);
        setColor(CS, C::Decoration, C::Border, C::Disabled, transparent);

        setColor(CS, C::Decoration, C::Separator, C::Normal, t.dividerDim);
        setColor(CS, C::Decoration, C::Shadow, C::Normal, t.shadow);
        setColor(CS, C::Decoration, C::Accent, C::Normal, t.accentMain);
        setColor(CS, C::Decoration, C::Indicator, C::Normal, t.fgA3);
        setColor(CS, C::Decoration, C::Tint, C::Normal, t.tint);
    }

    //window banner & miniplayer
    {
        C::ColorSet CS = C::Window;
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgA3);
        setColor(CS, C::Bg, C::Secondary, C::Normal, t.bgA3);
        setColor(CS, C::Decoration, C::Border, C::Normal, t.dividerDim);
        // close window button
        setColor(CS, C::Bg, C::Negative, C::Normal, t.critical);
        setColor(CS, C::Fg, C::Negative, C::Normal, t.dark ? t.fg1 : t.bgB2);
    }

    //badges, drawn over thumbnails
    {
        C::ColorSet CS = C::Badge;
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgB2Transparent);
        setColor(CS, C::Fg, C::Primary, C::Normal, t.dark ? t.fg1 : t.bgB2);
    }

    //tab button
    {
        C::ColorSet CS = C::TabButton;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(t.bgA2, 0.0));
        setColor(CS, C::Bg, C::Primary, C::Focused, t.bgA2);
        setColor(CS, C::Bg, C::Primary, C::Hovered, t.bgA2);
        setColor(CS, C::Bg, C::Primary, C::Pressed, t.bgA1);

        setColor(CS, C::Fg, C::Primary, C::Normal, t.fgA1);
        setColor(CS, C::Fg, C::Primary, C::Focused, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Hovered, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Disabled, t.fgA4);
        setColor(CS, C::Fg, C::Secondary, C::Normal, t.fg1);
    }

    //tool button
    {
        C::ColorSet CS = C::ToolButton;
        setColor(CS, C::Bg, C::Primary, C::Normal, transparent);
        setColor(CS, C::Bg, C::Secondary, C::Normal, t.bgA3);

        setColor(CS, C::Fg, C::Primary, C::Normal, t.fgA2);
        setColor(CS, C::Fg, C::Primary, C::Focused, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Hovered, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Pressed, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Disabled, t.fgA4);
        setColor(CS, C::Fg, C::Secondary, C::Normal, t.fg1);

        setColor(CS, C::Decoration, C::Border, C::Normal, transparent);
        setColor(CS, C::Decoration, C::Border, C::Focused, transparent);
        setColor(CS, C::Decoration, C::Border, C::Hovered, transparent);
        setColor(CS, C::Decoration, C::Border, C::Disabled, transparent);
    }

    //menubar
    {
        C::ColorSet CS = C::MenuBar;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(t.bgA2, 0.0));
        setColor(CS, C::Bg, C::Primary, C::Focused, t.bgA2);
        setColor(CS, C::Bg, C::Primary, C::Hovered, t.bgA2);
        setColor(CS, C::Fg, C::Primary, C::Normal, t.fg2);
        setColor(CS, C::Fg, C::Primary, C::Disabled, t.fgA4);
    }

    //Item
    {
        // selected rows must stand out from the main surface in both themes
        const QColor selected = t.dark ? t.bgA1 : t.bgA4;

        C::ColorSet CS = C::Item;
        setColor(CS, C::Bg, C::Primary, C::Normal, setColorAlpha(t.bgA2, 0.0));
        setColor(CS, C::Bg, C::Primary, C::Focused, t.bgA2);
        setColor(CS, C::Bg, C::Primary, C::Hovered, t.bgA2);

        setColor(CS, C::Bg, C::Highlight, C::Normal, selected);
        setColor(CS, C::Fg, C::Highlight, C::Normal, t.fg1);

        setColor(CS, C::Fg, C::Primary, C::Normal, t.fg2);
        setColor(CS, C::Fg, C::Primary, C::Focused, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Hovered, t.fg1);
        setColor(CS, C::Fg, C::Secondary, C::Normal, t.fgA1);

        setColor(CS, C::Decoration, C::Indicator, C::Normal, t.fgA3);
    }

    //Accent Buttons
    {
        C::ColorSet CS = C::ButtonAccent;
        setColor(CS, C::Bg, C::Primary, C::Normal, t.accentMain);
        setColor(CS, C::Bg, C::Primary, C::Hovered, t.accentBright);
        setColor(CS, C::Bg, C::Primary, C::Focused, t.accentBright);
        setColor(CS, C::Bg, C::Primary, C::Pressed, t.accentDim);
        setColor(CS, C::Bg, C::Primary, C::Disabled, t.fgA4);

        setColor(CS, C::Fg, C::Primary, C::Normal, t.fgB1);
        setColor(CS, C::Fg, C::Primary, C::Disabled, t.fgA1);

        setColor(CS, C::Decoration, C::Border, C::Normal, transparent);
        setColor(CS, C::Decoration, C::Border, C::Focused, transparent);
        setColor(CS, C::Decoration, C::Border, C::Hovered, transparent);
        setColor(CS, C::Decoration, C::Border, C::Disabled, transparent);
    }

    //Standard Buttons
    {
        C::ColorSet CS = C::ButtonStandard;
        setColor(CS, C::Bg, C::Primary, C::Normal, transparent);
        setColor(CS, C::Bg, C::Primary, C::Focused, t.bgA2);
        setColor(CS, C::Bg, C::Primary, C::Hovered, t.bgA2);
        setColor(CS, C::Bg, C::Primary, C::Pressed, t.bgA1);

        setColor(CS, C::Fg, C::Primary, C::Normal, t.fg2);
        setColor(CS, C::Fg, C::Primary, C::Focused, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Hovered, t.fg1);
        setColor(CS, C::Fg, C::Primary, C::Disabled, t.fgA4);

        setColor(CS, C::Decoration, C::Border, C::Normal, transparent);
        setColor(CS, C::Decoration, C::Border, C::Focused, transparent);
        setColor(CS, C::Decoration, C::Border, C::Hovered, transparent);
        setColor(CS, C::Decoration, C::Border, C::Disabled, transparent);
    }

    //tooltip
    {
        C::ColorSet CS = C::Tooltip;
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgA3);
        setColor(CS, C::Fg, C::Primary, C::Normal, t.fg2);
        setColor(CS, C::Decoration, C::Border, C::Normal, t.dividerBright);
    }

    //slider
    {
        C::ColorSet CS = C::Slider;
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgB2);
        setColor(CS, C::Bg, C::Secondary, C::Normal, t.dividerBright);
        setColor(CS, C::Bg, C::Highlight, C::Normal, t.bgAccentBright);
        setColor(CS, C::Fg, C::Primary, C::Normal, t.accentMain);
        setColor(CS, C::Fg, C::Positive, C::Normal, t.positive);
        setColor(CS, C::Fg, C::Neutral, C::Normal, t.neutral);
        setColor(CS, C::Fg, C::Negative, C::Normal, t.negative);
        // outlines the track, which is as dark as the scrim over the video
        setColor(CS, C::Decoration, C::Border, C::Normal, t.dividerDim);
    }

    //Combo box
    {
        C::ColorSet CS = C::ComboBox;
        setColor(CS, C::Fg, C::Primary, C::Normal, t.fg2);
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgB2);
        setColor(CS, C::Bg, C::Secondary, C::Normal, t.bgA4);
    }

    //TextField & SpinBox
    for (C::ColorSet CS : { C::TextField, C::SpinBox })
    {
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgB2);

        setColor(CS, C::Decoration, C::Border, C::Normal, t.dividerBright);
        setColor(CS, C::Decoration, C::Border, C::Focused, t.accentMain);
        setColor(CS, C::Decoration, C::Border, C::Hovered, t.fgA3);
        setColor(CS, C::Decoration, C::Border, C::Disabled, transparent);

        // text selection
        setColor(CS, C::Bg, C::Highlight, C::Normal, t.bgAccentBright);
        setColor(CS, C::Fg, C::Highlight, C::Normal, t.fg1);
    }

    //Switch
    {
        C::ColorSet CS = C::Switch;
        // off
        setColor(CS, C::Bg, C::Primary, C::Normal, t.bgB2);
        setColor(CS, C::Bg, C::Highlight, C::Normal, t.bgA4);   // unselected segment, off side
        setColor(CS, C::Fg, C::Primary, C::Normal, t.fgA3);
        setColor(CS, C::Decoration, C::Border, C::Normal, t.dividerBright);
        setColor(CS, C::Decoration, C::Indicator, C::Normal, t.fgA4); // off knob
        // on
        setColor(CS, C::Bg, C::Secondary, C::Normal, t.bgAccentBright);
        setColor(CS, C::Fg, C::Secondary, C::Normal, t.accentDim);
    }
}

