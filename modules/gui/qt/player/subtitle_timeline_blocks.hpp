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
#ifndef QVLC_SUBTITLE_TIMELINE_BLOCKS_HPP
#define QVLC_SUBTITLE_TIMELINE_BLOCKS_HPP

#include <QQuickPaintedItem>
#include <QColor>
#include <QPointer>

#include "subtitle_timeline.hpp"

/**
 * Draws the lines of the subtitle timeline (Figma 3744:12361) as rounded
 * blocks over the item's height, for the [from, to) milliseconds of media
 * the strip shows, moved by the subtitle delay.
 */
class SubtitleTimelineBlocks : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(SubtitleTimeline* timeline READ timeline WRITE setTimeline NOTIFY timelineChanged FINAL)
    Q_PROPERTY(double from READ from WRITE setFrom NOTIFY fromChanged FINAL)
    Q_PROPERTY(double to READ to WRITE setTo NOTIFY toChanged FINAL)
    Q_PROPERTY(double delay READ delay WRITE setDelay NOTIFY delayChanged FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged FINAL)

public:
    explicit SubtitleTimelineBlocks(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    SubtitleTimeline* timeline() const { return m_timeline; }
    double from() const { return m_from; }
    double to() const { return m_to; }
    double delay() const { return m_delay; }
    QColor color() const { return m_color; }
    qreal radius() const { return m_radius; }

    void setTimeline(SubtitleTimeline* timeline);
    void setFrom(double from);
    void setTo(double to);
    void setDelay(double delay);
    void setColor(const QColor& color);
    void setRadius(qreal radius);

signals:
    void timelineChanged();
    void fromChanged();
    void toChanged();
    void delayChanged();
    void colorChanged();
    void radiusChanged();

private:
    QPointer<SubtitleTimeline> m_timeline;
    QMetaObject::Connection m_cuesConnection;
    double m_from = 0.;
    double m_to = 0.;
    double m_delay = 0.;
    QColor m_color;
    qreal m_radius = 2.;
};

#endif // QVLC_SUBTITLE_TIMELINE_BLOCKS_HPP
