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
#include "subtitle_timeline_blocks.hpp"

#include <QPainter>

#include <cmath>

SubtitleTimelineBlocks::SubtitleTimelineBlocks(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);

    connect(this, &QQuickItem::widthChanged, this, [this]() { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this]() { update(); });
}

void SubtitleTimelineBlocks::paint(QPainter* painter)
{
    if (!m_timeline || !m_color.isValid())
        return;

    const QVector<SubtitleBlock> blocks = SubtitleTimeline::blocks(
        m_timeline->cues(), std::llround(m_from), std::llround(m_to), std::llround(m_delay), width());

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_color);

    for (const SubtitleBlock& block : blocks)
    {
        const qreal radius = std::min(m_radius, block.width / 2.);
        painter->drawRoundedRect(QRectF(block.x, 0., block.width, height()), radius, radius);
    }
}

void SubtitleTimelineBlocks::setTimeline(SubtitleTimeline* timeline)
{
    if (m_timeline == timeline)
        return;
    disconnect(m_cuesConnection);
    m_timeline = timeline;
    if (m_timeline)
        m_cuesConnection = connect(m_timeline, &SubtitleTimeline::cuesChanged, this, [this]() { update(); });
    emit timelineChanged();
    update();
}

void SubtitleTimelineBlocks::setFrom(double from)
{
    if (m_from == from)
        return;
    m_from = from;
    emit fromChanged();
    update();
}

void SubtitleTimelineBlocks::setTo(double to)
{
    if (m_to == to)
        return;
    m_to = to;
    emit toChanged();
    update();
}

void SubtitleTimelineBlocks::setDelay(double delay)
{
    if (m_delay == delay)
        return;
    m_delay = delay;
    emit delayChanged();
    update();
}

void SubtitleTimelineBlocks::setColor(const QColor& color)
{
    if (m_color == color)
        return;
    m_color = color;
    emit colorChanged();
    update();
}

void SubtitleTimelineBlocks::setRadius(qreal radius)
{
    if (qFuzzyCompare(m_radius, radius))
        return;
    m_radius = radius;
    emit radiusChanged();
    update();
}
