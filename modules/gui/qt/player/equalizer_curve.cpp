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
#include "equalizer_curve.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

EqualizerCurve::EqualizerCurve(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);

    connect(this, &QQuickItem::widthChanged, this, [this]() { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this]() { update(); });
}

void EqualizerCurve::paint(QPainter* painter)
{
    const qreal w = width();
    const qreal h = height();
    if (w <= 0. || h <= 0. || m_range <= 0.)
        return;

    const qreal middle = h / 2.;
    const qreal lineWidth = std::max<qreal>(1., h / 16.);

    painter->setRenderHint(QPainter::Antialiasing);

    if (m_bands.size() >= 2 && m_color.isValid())
    {
        // one point per band, then a smooth curve through them
        QList<QPointF> points;
        for (qsizetype i = 0; i < m_bands.size(); ++i)
        {
            const double gain = std::clamp(m_bands[i], -m_range, m_range);
            points.append(QPointF(w * i / (m_bands.size() - 1),
                                  middle - gain / m_range * middle));
        }

        QPainterPath path(QPointF(0., middle));
        path.lineTo(points.first());
        for (qsizetype i = 1; i < points.size(); ++i)
        {
            const QPointF& a = points[i - 1];
            const QPointF& b = points[i];
            const qreal dx = (b.x() - a.x()) / 2.;
            path.cubicTo(QPointF(a.x() + dx, a.y()), QPointF(b.x() - dx, b.y()), b);
        }
        path.lineTo(QPointF(w, middle));
        path.closeSubpath();

        painter->setPen(Qt::NoPen);
        painter->setBrush(m_color);
        painter->drawPath(path);
    }

    if (m_lineColor.isValid())
    {
        painter->setPen(QPen(m_lineColor, lineWidth, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(lineWidth / 2., middle), QPointF(w - lineWidth / 2., middle));
    }
}

void EqualizerCurve::setBands(const QList<double>& bands)
{
    if (m_bands == bands)
        return;
    m_bands = bands;
    emit bandsChanged();
    update();
}

void EqualizerCurve::setRange(double range)
{
    if (qFuzzyCompare(m_range, range))
        return;
    m_range = range;
    emit rangeChanged();
    update();
}

void EqualizerCurve::setColor(const QColor& color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    update();
}

void EqualizerCurve::setLineColor(const QColor& color)
{
    if (m_lineColor == color)
        return;
    m_lineColor = color;
    emit lineColorChanged();
    update();
}
