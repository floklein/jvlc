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
#ifndef SEEK_THUMBNAILER_HPP
#define SEEK_THUMBNAILER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <QObject>
#include <QImage>
#include <QUrl>
#include <QSize>
#include <QCache>
#include <QMutex>
#include <QQmlParserStatus>
#include <QQuickImageProvider>

#include <functional>
#include <memory>

#include "util/vlctick.hpp"

typedef struct vlc_preparser_t vlc_preparser_t;

/**
 * Generates one picture of a media at a given time.
 */
class SeekThumbnailSource
{
public:
    /// called exactly once per started request, on any thread, with a null
    /// image on failure or cancellation
    using Callback = std::function<void (const QImage&)>;

    class Request
    {
    public:
        virtual ~Request() = default;
        /// stop generating; the callback still runs (with a null image)
        virtual void cancel() = 0;
    };

    virtual ~SeekThumbnailSource() = default;

    /// start generating the picture of `mrl` at `time`, scaled and cropped to
    /// `size`; returns nullptr (and never calls `done`) if it can't start
    virtual std::shared_ptr<Request> start(const QString& mrl, vlc_tick_t time,
                                           const QSize& size, Callback done) = 0;
};

/**
 * SeekThumbnailSource on the preparser thumbnailer (fast seek, in process)
 */
class PreparserSeekThumbnailSource : public SeekThumbnailSource
{
public:
    explicit PreparserSeekThumbnailSource(vlc_object_t* parent);
    ~PreparserSeekThumbnailSource() override;

    std::shared_ptr<Request> start(const QString& mrl, vlc_tick_t time,
                                   const QSize& size, Callback done) override;

private:
    vlc_object_t* m_parent;
    QMutex m_lock;
    vlc_preparser_t* m_preparser = nullptr;
};

/**
 * The seek thumbnails generated so far, shared by every seek bar and the
 * image provider. Thread safe.
 */
class SeekThumbnailCache
{
public:
    explicit SeekThumbnailCache(qsizetype maxBytes = 64 * 1024 * 1024);

    void insert(const QString& id, const QImage& image);
    QImage find(const QString& id) const;
    bool contains(const QString& id) const;
    void clear();

private:
    mutable QMutex m_lock;
    QCache<QString, QImage> m_images;
};

/**
 * image://seekthumb/<id>: serves the seek thumbnails from the cache, so that
 * a picture shows as soon as SeekThumbnailer::source points at it
 */
class SeekThumbnailProvider : public QQuickImageProvider
{
public:
    static constexpr const char* providerId = "seekthumb";

    SeekThumbnailProvider(std::shared_ptr<SeekThumbnailCache> cache,
                          std::shared_ptr<SeekThumbnailSource> source);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    std::shared_ptr<SeekThumbnailCache> cache() const { return m_cache; }
    std::shared_ptr<SeekThumbnailSource> source() const { return m_source; }

private:
    std::shared_ptr<SeekThumbnailCache> m_cache;
    std::shared_ptr<SeekThumbnailSource> m_source;
};

/**
 * The thumbnail of one seek bar: request() the hovered position, show
 * `source`.
 *
 * Positions are rounded to steps of at least one second, at most 400 steps
 * over the media. A cached step shows at once. Otherwise one picture is
 * generated at a time, and the moves made meanwhile collapse into the latest
 * position, generated next, while the last picture stays shown.
 */
class SeekThumbnailer : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QUrl mrl READ mrl WRITE setMrl NOTIFY mrlChanged FINAL)
    Q_PROPERTY(VLCDuration length READ length WRITE setLength NOTIFY lengthChanged FINAL)
    // the size of the picture, in pixels
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged FINAL)
    // the picture to show, empty until one is ready
    Q_PROPERTY(QUrl source READ source NOTIFY sourceChanged FINAL)
    // test hook: the position the seek bar previews as if hovered, from the
    // VLC_QT_TEST_SEEK_PREVIEW environment variable (0 to 1), -1 when unset
    Q_PROPERTY(double testPosition READ testPosition CONSTANT FINAL)

public:
    explicit SeekThumbnailer(QObject* parent = nullptr);
    SeekThumbnailer(std::shared_ptr<SeekThumbnailCache> cache,
                    std::shared_ptr<SeekThumbnailSource> source,
                    QObject* parent = nullptr);
    ~SeekThumbnailer() override;

    QUrl mrl() const { return m_mrl; }
    void setMrl(const QUrl& mrl);
    VLCDuration length() const { return m_length; }
    void setLength(const VLCDuration& length);
    QSize size() const { return m_size; }
    void setSize(const QSize& size);
    QUrl source() const { return m_source; }
    static double testPosition();

    /// show the picture at `position` (0 to 1) of the media
    Q_INVOKABLE void request(double position);
    /// forget the requested position (the pointer left the seek bar)
    Q_INVOKABLE void stop();

    void classBegin() override {}
    void componentComplete() override;

signals:
    void mrlChanged();
    void lengthChanged();
    void sizeChanged();
    void sourceChanged();

private:
    struct Guard;

    bool ready() const;
    vlc_tick_t stepTime(double position) const;
    QString pictureId(vlc_tick_t time) const;
    void show(const QString& id);
    void generate(vlc_tick_t time);
    void onGenerated(unsigned generation, vlc_tick_t time, const QImage& image);
    void reset();

    std::shared_ptr<SeekThumbnailCache> m_cache;
    std::shared_ptr<SeekThumbnailSource> m_thumbnails;
    std::shared_ptr<Guard> m_guard;

    QUrl m_mrl;
    QString m_mrlKey;
    VLCDuration m_length;
    QSize m_size;
    QUrl m_source;

    // bumped when the media or size changes: older pictures are not shown
    unsigned m_generation = 0;
    std::shared_ptr<SeekThumbnailSource::Request> m_running;
    vlc_tick_t m_runningTime = VLC_TICK_INVALID;
    vlc_tick_t m_wantedTime = VLC_TICK_INVALID;
};

#endif // SEEK_THUMBNAILER_HPP
