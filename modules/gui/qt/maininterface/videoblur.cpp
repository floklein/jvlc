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
#include "videoblur.hpp"

#include <QQuickWindow>

#include <algorithm>
#include <cmath>
#include <vector>

void VideoBlurProvider::updateRegion(const void* key, const Region& region)
{
    emit regionChanged(reinterpret_cast<quintptr>(key), region);
}

void VideoBlurProvider::removeRegion(const void* key)
{
    emit regionRemoved(reinterpret_cast<quintptr>(key));
}

VideoBlurLayout videoBlurLayout(const QList<VideoBlurProvider::Region>& regions)
{
    // the blurred parts: a rounded region without its corner squares
    QList<QRectF> blurred;
    VideoBlurLayout layout;
    for (const VideoBlurProvider::Region& region : regions)
    {
        if (region.opacity < 0.5 || region.rect.isEmpty())
            continue;
        layout.bounds = layout.bounds.united(region.rect);
        const qreal radius = std::min({ region.radius, region.rect.width() / 2.,
                                        region.rect.height() / 2. });
        if (radius <= 0.)
        {
            blurred.append(region.rect);
            continue;
        }
        const QRectF& rect = region.rect;
        blurred.append(rect.adjusted(radius, 0., -radius, 0.));
        blurred.append(rect.adjusted(0., radius, 0., -radius));

        // the corners in strips of 2 pixels at most, each blurred only where
        // the whole strip is inside the rounded corner
        const int strips = std::min(8, static_cast<int>(std::ceil(radius / 2.)));
        const qreal height = radius / strips;
        for (int i = 1; i < strips; ++i)
        {
            // from the center of the corner's circle to the strip's outer edge
            const qreal dy = radius - i * height;
            const qreal inset = radius - std::sqrt(radius * radius - dy * dy);
            const qreal top = rect.top() + i * height;
            const qreal bottom = rect.bottom() - (i + 1) * height;
            blurred.append(QRectF(QPointF(rect.left() + inset, top),
                                  QPointF(rect.left() + radius, top + height)));
            blurred.append(QRectF(QPointF(rect.right() - radius, top),
                                  QPointF(rect.right() - inset, top + height)));
            blurred.append(QRectF(QPointF(rect.left() + inset, bottom),
                                  QPointF(rect.left() + radius, bottom + height)));
            blurred.append(QRectF(QPointF(rect.right() - radius, bottom),
                                  QPointF(rect.right() - inset, bottom + height)));
        }
    }
    if (layout.bounds.isEmpty())
        return {};

    // a grid on every edge; its cells outside the blurred parts are gaps
    std::vector<qreal> xs{ layout.bounds.left(), layout.bounds.right() };
    std::vector<qreal> ys{ layout.bounds.top(), layout.bounds.bottom() };
    for (const QRectF& rect : blurred)
    {
        xs.insert(xs.end(), { rect.left(), rect.right() });
        ys.insert(ys.end(), { rect.top(), rect.bottom() });
    }
    for (std::vector<qreal>* edges : { &xs, &ys })
    {
        std::sort(edges->begin(), edges->end());
        edges->erase(std::unique(edges->begin(), edges->end()), edges->end());
    }

    const auto isBlurred = [&blurred](const QPointF& point) {
        return std::any_of(blurred.cbegin(), blurred.cend(),
                           [&point](const QRectF& rect) { return rect.contains(point); });
    };

    for (size_t row = 0; row + 1 < ys.size(); ++row)
    {
        const qreal top = ys[row], bottom = ys[row + 1];
        // the runs of gap cells of this row
        QList<QRectF> runs;
        for (size_t column = 0; column + 1 < xs.size(); ++column)
        {
            const qreal left = xs[column], right = xs[column + 1];
            if (isBlurred({ (left + right) / 2., (top + bottom) / 2. }))
                continue;
            if (!runs.isEmpty() && runs.last().right() == left)
                runs.last().setRight(right);
            else
                runs.append(QRectF(QPointF(left, top), QPointF(right, bottom)));
        }
        // a run just below a gap of the same width extends it
        for (const QRectF& run : runs)
        {
            const auto above = std::find_if(layout.gaps.begin(), layout.gaps.end(),
                                            [&run](const QRectF& gap) {
                return gap.bottom() == run.top() && gap.left() == run.left()
                    && gap.right() == run.right();
            });
            if (above != layout.gaps.end())
                above->setBottom(run.bottom());
            else
                layout.gaps.append(run);
        }
    }
    return layout;
}

VideoBlurGate::VideoBlurGate(VideoBlurProvider* provider, QObject* parent)
    : QObject(parent)
    , m_provider(provider)
{
}

void VideoBlurGate::setEnabled(bool enabled)
{
    set(m_enabled, enabled);
}

void VideoBlurGate::setTransparencyEffects(bool enabled)
{
    set(m_transparencyEffects, enabled);
}

void VideoBlurGate::setBatterySaver(bool enabled)
{
    set(m_batterySaver, enabled);
}

VideoBlurProvider* VideoBlurGate::provider() const
{
    const bool open = m_enabled && m_transparencyEffects && !m_batterySaver;
    return open ? m_provider : nullptr;
}

void VideoBlurGate::set(bool& condition, bool value)
{
    if (condition == value)
        return;

    VideoBlurProvider* const before = provider();
    condition = value;
    VideoBlurProvider* const after = provider();
    if (after != before)
        emit providerChanged(after);
}

VideoBlur::VideoBlur(QQuickItem* parent)
    : QQuickItem(parent)
{
    // itemChange() is not called for the parent set by the base constructor:
    trackAncestors();
    trackWindow(window());
}

VideoBlur::~VideoBlur()
{
    withdraw();
}

void VideoBlur::setProvider(VideoBlurProvider* provider)
{
    if (m_provider == provider)
        return;

    withdraw();
    m_provider = provider;
    emit providerChanged();
    report();
}

void VideoBlur::setRadius(qreal radius)
{
    if (qFuzzyCompare(m_radius, radius))
        return;

    m_radius = radius;
    emit radiusChanged();
    report();
}

void VideoBlur::setDeviation(qreal deviation)
{
    if (qFuzzyCompare(m_deviation, deviation))
        return;

    m_deviation = deviation;
    emit deviationChanged();
    report();
}

void VideoBlur::setDarkest(qreal darkest)
{
    if (qFuzzyCompare(m_darkest + 1., darkest + 1.))
        return;

    m_darkest = darkest;
    emit darkestChanged();
    report();
}

void VideoBlur::setBrightest(qreal brightest)
{
    if (qFuzzyCompare(m_brightest + 1., brightest + 1.))
        return;

    m_brightest = brightest;
    emit brightestChanged();
    report();
}

void VideoBlur::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickItem::itemChange(change, value);

    switch (change)
    {
    case ItemSceneChange:
        withdraw();
        trackWindow(value.window);
        [[fallthrough]];
    case ItemParentHasChanged:
        trackAncestors();
        report();
        break;
    case ItemVisibleHasChanged:
    case ItemOpacityHasChanged:
        report();
        break;
    default:
        break;
    }
}

void VideoBlur::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    report();
}

void VideoBlur::trackAncestors()
{
    for (const QMetaObject::Connection& connection : m_ancestorConnections)
        disconnect(connection);
    m_ancestorConnections.clear();

    // The scene rectangle and the effective opacity also change when an
    // ancestor moves or fades. Anchors and size changes of the item itself
    // go through geometryChange().
    for (QQuickItem* item = parentItem(); item; item = item->parentItem())
    {
        m_ancestorConnections
            << connect(item, &QQuickItem::xChanged, this, &VideoBlur::report)
            << connect(item, &QQuickItem::yChanged, this, &VideoBlur::report)
            << connect(item, &QQuickItem::opacityChanged, this, &VideoBlur::report)
            << connect(item, &QQuickItem::parentChanged, this, [this]() {
                   trackAncestors();
                   report();
               });
    }
}

void VideoBlur::trackWindow(QQuickWindow* window)
{
    disconnect(m_windowConnection);
    m_windowConnection = {};
    if (!window)
        return;

    // Transforms (the bars' reveal slide) change the scene rectangle without
    // a signal. Checking before each frame also keeps the region in step with
    // the frame Qt is about to draw. report() only commits a change.
    m_windowConnection = connect(window, &QQuickWindow::afterAnimating, this, &VideoBlur::report);
}

void VideoBlur::report()
{
    QQuickWindow* const window = this->window();
    if (!m_provider || !window)
    {
        withdraw();
        return;
    }

    // isVisible() is the effective visibility, opacity() is not:
    qreal opacity = 1.;
    for (const QQuickItem* item = this; item && opacity > 0.; item = item->parentItem())
        opacity *= item->opacity();

    if (!isVisible() || opacity <= 0. || width() <= 0. || height() <= 0.)
    {
        withdraw();
        return;
    }

    const qreal dpr = window->effectiveDevicePixelRatio();

    VideoBlurProvider::Region region;
    region.rect = mapRectToScene(QRectF(0., 0., width(), height()));
    region.rect = QRectF(region.rect.topLeft() * dpr, region.rect.size() * dpr);
    region.radius = m_radius * dpr;
    region.opacity = opacity;
    region.deviation = m_deviation * dpr;
    region.darkest = m_darkest;
    region.brightest = m_brightest;

    if (m_reported && region == m_lastRegion)
        return;

    m_lastRegion = region;
    m_reported = true;
    m_provider->updateRegion(this, region);
}

void VideoBlur::withdraw()
{
    if (!m_reported)
        return;

    m_reported = false;
    if (m_provider)
        m_provider->removeRegion(this);
}
