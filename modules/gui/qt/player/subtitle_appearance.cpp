/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "subtitle_appearance.hpp"

#include <QLocale>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace {

// "sub-text-scale" of each relative size: VLC 3's "freetype-rel-fontsize"
// (the video height divided by 20, 18, 16, 12 or 6) relative to Normal
constexpr std::array<int, 5> RELATIVE_SCALES = { 80, 89, 100, 133, 267 };

// "freetype-outline-thickness" of each step
constexpr std::array<int, 4> OUTLINE_THICKNESSES = { 0, 2, 4, 6 };

SubtitleAppearance::RelativeSize nearestRelativeSize(int scale)
{
    size_t nearest = 0;
    for (size_t i = 1; i < RELATIVE_SCALES.size(); ++i)
        if (std::abs(RELATIVE_SCALES[i] - scale) < std::abs(RELATIVE_SCALES[nearest] - scale))
            nearest = i;
    return static_cast<SubtitleAppearance::RelativeSize>(nearest);
}

SubtitleAppearance::OutlineThickness nearestOutlineThickness(int thickness)
{
    size_t nearest = 0;
    for (size_t i = 1; i < OUTLINE_THICKNESSES.size(); ++i)
        if (std::abs(OUTLINE_THICKNESSES[i] - thickness) < std::abs(OUTLINE_THICKNESSES[nearest] - thickness))
            nearest = i;
    return static_cast<SubtitleAppearance::OutlineThickness>(nearest);
}

QColor rgbColor(const QVariant& value, const QColor& fallback)
{
    if (!value.isValid())
        return fallback;
    return QColor::fromRgb(static_cast<QRgb>(value.toLongLong() & 0xFFFFFF));
}

int rgbValue(const QColor& color)
{
    return static_cast<int>(color.rgb() & 0xFFFFFF);
}

}

SubtitleAppearance::SubtitleAppearance(std::unique_ptr<Backend> backend, QObject* parent)
    : QObject(parent)
    , m_backend(std::move(backend))
{
    read();
}

SubtitleAppearance::~SubtitleAppearance() = default;

void SubtitleAppearance::read()
{
    const auto value = [this](const char* option) { return m_backend->value(option); };

    // the first language of the preference list
    m_defaultLanguage = value("sub-language").toString().section(QLatin1Char(','), 0, 0).trimmed();

    const QVariant keep = value("freetype-keep-styling");
    m_keepFileStyling = !keep.isValid() || keep.toBool();
    m_font = value("freetype-font").toString();

    const QVariant scale = value("sub-text-scale");
    m_relativeSize = scale.isValid() ? nearestRelativeSize(scale.toInt()) : Normal;
    const int pixels = value("freetype-pixel-size").toInt();
    m_sizeMode = pixels > 0 ? Pixels : Relative;
    if (pixels > 0)
        m_pixelSize = pixels;

    m_textColor = rgbColor(value("freetype-color"), Qt::white);
    m_outlineColor = rgbColor(value("freetype-outline-color"), Qt::black);
    const QVariant thickness = value("freetype-outline-thickness");
    m_outlineThickness = thickness.isValid() ? nearestOutlineThickness(thickness.toInt())
                                             : NormalOutline;
    m_backgroundOpacity = qRound(value("freetype-background-opacity").toInt() * 100. / 255.);
    m_verticalOffset = value("sub-margin").toInt();
}

void SubtitleAppearance::reload()
{
    m_backend->reloadProfile();

    const QString language = m_defaultLanguage;
    const bool keep = m_keepFileStyling;
    const QString font = m_font;
    const SizeMode mode = m_sizeMode;
    const RelativeSize relative = m_relativeSize;
    const int pixels = m_pixelSize;
    const QColor text = m_textColor;
    const QColor outline = m_outlineColor;
    const OutlineThickness thickness = m_outlineThickness;
    const int background = m_backgroundOpacity;
    const int offset = m_verticalOffset;

    read();

    if (language != m_defaultLanguage)
        emit defaultLanguageChanged();
    if (keep != m_keepFileStyling)
        emit keepFileStylingChanged();
    if (font != m_font)
        emit fontChanged();
    if (mode != m_sizeMode)
        emit sizeModeChanged();
    if (relative != m_relativeSize)
        emit relativeSizeChanged();
    if (pixels != m_pixelSize)
        emit pixelSizeChanged();
    if (text != m_textColor)
        emit textColorChanged();
    if (outline != m_outlineColor)
        emit outlineColorChanged();
    if (thickness != m_outlineThickness)
        emit outlineThicknessChanged();
    if (background != m_backgroundOpacity)
        emit backgroundOpacityChanged();
    if (offset != m_verticalOffset)
        emit verticalOffsetChanged();

    m_backend->redraw();
}

void SubtitleAppearance::applyLook(const char* option, const QVariant& value)
{
    m_backend->setValue(option, value);
    m_backend->redraw();
}

void SubtitleAppearance::setDefaultLanguage(const QString& language)
{
    const QString code = language.trimmed();
    if (code == m_defaultLanguage)
        return;
    m_defaultLanguage = code;
    // it picks the subtitle track of the next media, not of this one
    m_backend->setValue("sub-language", code);
    emit defaultLanguageChanged();
}

QVariantList SubtitleAppearance::languages() const
{
    struct Language
    {
        QString code;
        QString name;
    };
    std::vector<Language> list;
    QSet<QString> codes;
    for (int i = QLocale::AnyLanguage + 1; i <= QLocale::LastLanguage; ++i)
    {
        const auto language = static_cast<QLocale::Language>(i);
        const QString code = QLocale::languageToCode(language, QLocale::ISO639Part1);
        if (code.isEmpty() || codes.contains(code))
            continue;
        const QString name = QLocale::languageToString(language);
        if (name.isEmpty())
            continue;
        codes.insert(code);
        list.push_back({ code, name });
    }
    std::sort(list.begin(), list.end(), [](const Language& a, const Language& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });

    const auto choice = [](const QString& code, const QString& name) {
        return QVariantMap{ { QStringLiteral("code"), code }, { QStringLiteral("name"), name } };
    };
    QVariantList languages;
    languages.reserve(static_cast<qsizetype>(list.size()) + 2);
    //: default subtitle language: the media picks its subtitles
    languages.append(choice(QString(), tr("Automatic")));
    //: default subtitle language: no subtitles
    languages.append(choice(QStringLiteral("none"), tr("None")));
    for (const Language& language : list)
        languages.append(choice(language.code, language.name));
    return languages;
}

void SubtitleAppearance::setKeepFileStyling(bool keep)
{
    if (keep == m_keepFileStyling)
        return;
    m_keepFileStyling = keep;
    applyLook("freetype-keep-styling", keep);
    emit keepFileStylingChanged();
}

void SubtitleAppearance::setFont(const QString& font)
{
    if (font == m_font)
        return;
    m_font = font;
    applyLook("freetype-font", font);
    emit fontChanged();
}

void SubtitleAppearance::setSizeMode(SizeMode mode)
{
    if (mode == m_sizeMode)
        return;
    m_sizeMode = mode;
    applyLook("freetype-pixel-size", mode == Pixels ? m_pixelSize : 0);
    emit sizeModeChanged();
}

void SubtitleAppearance::setRelativeSize(RelativeSize size)
{
    if (size < Smaller || size > Larger)
        return;
    if (size == m_relativeSize && m_sizeMode == Relative)
        return;
    const bool sizeChanged = size != m_relativeSize;
    m_relativeSize = size;
    m_backend->setValue("sub-text-scale", RELATIVE_SCALES[size]);
    if (m_sizeMode == Pixels)
    {
        m_sizeMode = Relative;
        m_backend->setValue("freetype-pixel-size", 0);
        emit sizeModeChanged();
    }
    m_backend->redraw();
    if (sizeChanged)
        emit relativeSizeChanged();
}

void SubtitleAppearance::setPixelSize(int pixels)
{
    pixels = std::clamp(pixels, 1, 4096);
    if (pixels == m_pixelSize)
        return;
    m_pixelSize = pixels;
    if (m_sizeMode == Pixels)
        applyLook("freetype-pixel-size", pixels);
    emit pixelSizeChanged();
}

void SubtitleAppearance::setTextColor(const QColor& color)
{
    if (!color.isValid() || rgbValue(color) == rgbValue(m_textColor))
        return;
    m_textColor = QColor::fromRgb(color.rgb());
    applyLook("freetype-color", rgbValue(color));
    emit textColorChanged();
}

void SubtitleAppearance::setOutlineColor(const QColor& color)
{
    if (!color.isValid() || rgbValue(color) == rgbValue(m_outlineColor))
        return;
    m_outlineColor = QColor::fromRgb(color.rgb());
    applyLook("freetype-outline-color", rgbValue(color));
    emit outlineColorChanged();
}

void SubtitleAppearance::setOutlineThickness(OutlineThickness thickness)
{
    if (thickness < NoOutline || thickness > ThickOutline || thickness == m_outlineThickness)
        return;
    m_outlineThickness = thickness;
    applyLook("freetype-outline-thickness", OUTLINE_THICKNESSES[thickness]);
    emit outlineThicknessChanged();
}

void SubtitleAppearance::setBackgroundOpacity(int percent)
{
    percent = std::clamp(percent, 0, 100);
    if (percent == m_backgroundOpacity)
        return;
    m_backgroundOpacity = percent;
    applyLook("freetype-background-opacity", qRound(percent * 255. / 100.));
    emit backgroundOpacityChanged();
}

void SubtitleAppearance::setVerticalOffset(int pixels)
{
    pixels = std::max(pixels, 0);
    if (pixels == m_verticalOffset)
        return;
    m_verticalOffset = pixels;
    // the video output moves the subtitles by itself
    m_backend->setValue("sub-margin", pixels);
    emit verticalOffsetChanged();
}
