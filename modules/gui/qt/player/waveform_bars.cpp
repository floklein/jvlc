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
#include "waveform_bars.hpp"

#include "waveform.hpp"

#include <QPainter>

#include <cmath>

WaveformBars::WaveformBars(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);

    connect(this, &QQuickItem::widthChanged, this, [this]() { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this]() { update(); });
}

void WaveformBars::paint(QPainter* painter)
{
    const qreal pitch = m_barWidth + m_barSpacing;
    if (m_peaks.isEmpty() || pitch <= 0. || !m_color.isValid())
        return;

    const int count = int(std::floor((width() + m_barSpacing) / pitch));
    const QVector<float> bars = SeekWaveform::resample(m_peaks, count);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_color);

    const qreal radius = m_barWidth / 2.;
    for (int i = 0; i < bars.size(); ++i)
    {
        const qreal h = std::max(m_minimumBarHeight,
                                 std::min(m_maximumBarHeight, qreal(bars[i]) * m_maximumBarHeight));
        painter->drawRoundedRect(QRectF(i * pitch, (height() - h) / 2., m_barWidth, h), radius, radius);
    }
}

void WaveformBars::setPeaks(const QVector<float>& peaks)
{
    if (m_peaks == peaks)
        return;
    m_peaks = peaks;
    emit peaksChanged();
    update();
}

void WaveformBars::setColor(const QColor& color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    update();
}

void WaveformBars::setBarWidth(qreal width)
{
    if (qFuzzyCompare(m_barWidth, width))
        return;
    m_barWidth = width;
    emit barWidthChanged();
    update();
}

void WaveformBars::setBarSpacing(qreal spacing)
{
    if (qFuzzyCompare(m_barSpacing, spacing))
        return;
    m_barSpacing = spacing;
    emit barSpacingChanged();
    update();
}

void WaveformBars::setMinimumBarHeight(qreal height)
{
    if (qFuzzyCompare(m_minimumBarHeight, height))
        return;
    m_minimumBarHeight = height;
    emit minimumBarHeightChanged();
    update();
}

void WaveformBars::setMaximumBarHeight(qreal height)
{
    if (qFuzzyCompare(m_maximumBarHeight, height))
        return;
    m_maximumBarHeight = height;
    emit maximumBarHeightChanged();
    update();
}
