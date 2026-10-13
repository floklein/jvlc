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
#ifndef VLC_QT_VIDEOBLUR_HPP
#define VLC_QT_VIDEOBLUR_HPP

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>

/**
 * Video blur: a blurred copy of the playing video shown beneath the Tint of
 * the panels that sit over the video.
 *
 * The blur itself is drawn natively by the compositor, under the Qt scene.
 * QML only says where: each `VideoBlur` item reports its scene rectangle to
 * the `VideoBlurProvider`, which the compositor listens to. A compositor that
 * can not draw the Video blur publishes no provider (`MainCtx.videoBlurProvider`
 * is null), and the panels keep the plain Tint.
 */
class VideoBlurProvider : public QObject
{
    Q_OBJECT

public:
    struct Region
    {
        QRectF rect; // scene coordinates, device pixels
        qreal radius = 0.; // corner radius, device pixels

        bool operator==(const Region& other) const
        {
            return rect == other.rect && qFuzzyCompare(radius + 1., other.radius + 1.);
        }
    };

    using QObject::QObject;

    void updateRegion(const void* key, const Region& region);
    void removeRegion(const void* key);

signals:
    // `key` identifies the region, it is never dereferenced.
    void regionChanged(quintptr key, const VideoBlurProvider::Region& region);
    void regionRemoved(quintptr key);
};

/**
 * A rectangle of the scene where the Video blur shows. It paints nothing:
 * place it under the Tint of a panel that sits over the video.
 *
 * The region is shown while the item is visible and not fully transparent.
 */
class VideoBlur : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(VideoBlurProvider* provider READ provider WRITE setProvider NOTIFY providerChanged FINAL)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged FINAL)

public:
    explicit VideoBlur(QQuickItem* parent = nullptr);
    ~VideoBlur() override;

    VideoBlurProvider* provider() const { return m_provider; }
    void setProvider(VideoBlurProvider* provider);

    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);

signals:
    void providerChanged();
    void radiusChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void trackAncestors();
    void report();
    void withdraw();

    QPointer<VideoBlurProvider> m_provider;
    QList<QMetaObject::Connection> m_ancestorConnections;
    qreal m_radius = 0.;
    bool m_reported = false;
    VideoBlurProvider::Region m_lastRegion;
};

#endif // VLC_QT_VIDEOBLUR_HPP
