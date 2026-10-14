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
#ifndef VLC_QT_CAPTURE_PREVIEW_HPP
#define VLC_QT_CAPTURE_PREVIEW_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "qt.hpp"

struct vlc_preparser_t;
struct vlc_preparser_req;

/**
 * A live thumbnail of a capture source.
 *
 * While active, it grabs one frame of the source through the thumbnailer,
 * then grabs again refreshInterval ms after each frame. A new source cancels
 * the grab in progress. Frames come as data: URLs of PNG images.
 */
class CapturePreview : public QObject
{
    Q_OBJECT

public:
    enum Status
    {
        Idle,
        Loading,
        Ready,
        Unavailable,
    };
    Q_ENUM(Status)

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(int refreshInterval READ refreshInterval WRITE setRefreshInterval NOTIFY refreshIntervalChanged FINAL)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(QUrl image READ image NOTIFY imageChanged FINAL)

public:
    explicit CapturePreview(qt_intf_t* intf, QObject* parent = nullptr);
    ~CapturePreview() override;

    bool active() const { return m_active; }
    void setActive(bool active);
    /* ms between two frames, 0 for a single frame */
    int refreshInterval() const { return m_refreshInterval; }
    void setRefreshInterval(int interval);
    Status status() const { return m_status; }
    QUrl image() const { return m_image; }

    void setSource(const QString& mrl, const QStringList& options);

signals:
    void activeChanged();
    void refreshIntervalChanged();
    void statusChanged();
    void imageChanged();

private:
    static void onThumbnailEnded(vlc_preparser_req* req, int status,
                                 struct picture_t* picture, void* data);

    void request();
    void cancel();
    void onFrame(unsigned generation, const QByteArray& png);
    void setStatus(Status status);

    qt_intf_t* m_intf = nullptr;
    vlc_preparser_t* m_preparser = nullptr;
    vlc_preparser_req* m_req = nullptr;
    unsigned m_generation = 0;

    QString m_mrl;
    QStringList m_options;
    bool m_active = false;
    int m_refreshInterval = 0;
    Status m_status = Idle;
    QUrl m_image;
    QTimer m_timer;
};

#endif // VLC_QT_CAPTURE_PREVIEW_HPP
