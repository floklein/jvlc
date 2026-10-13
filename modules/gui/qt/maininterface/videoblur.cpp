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

void VideoBlurProvider::updateRegion(const void* key, const Region& region)
{
    emit regionChanged(reinterpret_cast<quintptr>(key), region);
}

void VideoBlurProvider::removeRegion(const void* key)
{
    emit regionRemoved(reinterpret_cast<quintptr>(key));
}

VideoBlur::VideoBlur(QQuickItem* parent)
    : QQuickItem(parent)
{
    // itemChange() is not called for the parent set by the base constructor:
    trackAncestors();
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

void VideoBlur::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickItem::itemChange(change, value);

    switch (change)
    {
    case ItemSceneChange:
        withdraw();
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
