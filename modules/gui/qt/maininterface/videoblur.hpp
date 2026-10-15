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
        qreal opacity = 1.; // the panel's effective opacity, in ]0, 1]
        qreal deviation = 0.; // standard deviation of the blur, device pixels

        bool operator==(const Region& other) const
        {
            return rect == other.rect
                && qFuzzyCompare(radius + 1., other.radius + 1.)
                && qFuzzyCompare(opacity + 1., other.opacity + 1.)
                && qFuzzyCompare(deviation + 1., other.deviation + 1.);
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
 * Whether the Video blur shows. It follows the user's setting ("Blur the video
 * behind panels"), and turns off by itself while Windows transparency effects
 * are off or battery saver is on.
 *
 * The compositor publishes `provider()` as `MainCtx.videoBlurProvider`. It is
 * null while the gate is closed, so the panels keep the plain Tint and their
 * `VideoBlur` items withdraw their regions.
 */
class VideoBlurGate : public QObject
{
    Q_OBJECT

public:
    explicit VideoBlurGate(VideoBlurProvider* provider, QObject* parent = nullptr);

    void setEnabled(bool enabled);
    void setTransparencyEffects(bool enabled);
    void setBatterySaver(bool enabled);

    // the provider while the Video blur shows, else null
    VideoBlurProvider* provider() const;

signals:
    void providerChanged(VideoBlurProvider* provider);

private:
    void set(bool& condition, bool value);

    VideoBlurProvider* const m_provider;
    bool m_enabled = true;
    bool m_transparencyEffects = true;
    bool m_batterySaver = false;
};

/**
 * A rectangle of the scene where the Video blur shows. It paints nothing:
 * place it under the Tint of a panel that sits over the video.
 *
 * The region is shown while the item is visible and not fully transparent, and
 * the blur fades with the item. The region follows the item's geometry and its
 * ancestors' position, opacity and transforms (a sliding bar).
 */
class VideoBlur : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(VideoBlurProvider* provider READ provider WRITE setProvider NOTIFY providerChanged FINAL)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged FINAL)
    // standard deviation of the Gaussian blur, in pixels of the scene
    Q_PROPERTY(qreal deviation READ deviation WRITE setDeviation NOTIFY deviationChanged FINAL)

public:
    explicit VideoBlur(QQuickItem* parent = nullptr);
    ~VideoBlur() override;

    VideoBlurProvider* provider() const { return m_provider; }
    void setProvider(VideoBlurProvider* provider);

    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);

    qreal deviation() const { return m_deviation; }
    void setDeviation(qreal deviation);

signals:
    void providerChanged();
    void radiusChanged();
    void deviationChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void trackAncestors();
    void trackWindow(QQuickWindow* window);
    void report();
    void withdraw();

    QPointer<VideoBlurProvider> m_provider;
    QList<QMetaObject::Connection> m_ancestorConnections;
    QMetaObject::Connection m_windowConnection;
    qreal m_radius = 0.;
    // Figma's "background blur 50" is close to a standard deviation of 25
    qreal m_deviation = 25.;
    bool m_reported = false;
    VideoBlurProvider::Region m_lastRegion;
};

#endif // VLC_QT_VIDEOBLUR_HPP
