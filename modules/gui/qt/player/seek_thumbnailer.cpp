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
#include "seek_thumbnailer.hpp"

#include <vlc_common.h>
#include <vlc_input_item.h>
#include <vlc_picture.h>
#include <vlc_preparser.h>
#include <vlc_block.h>

#include "qt.hpp"

#include <QCryptographicHash>
#include <QMutexLocker>
#include <QQmlEngine>

#include <cmath>

namespace {

// at least one picture per second, at most this many over the seek bar
constexpr int MAX_STEPS = 400;

// a remote media may never deliver a picture
constexpr vlc_tick_t THUMBNAIL_TIMEOUT = VLC_TICK_FROM_SEC(10);

QImage toImage(vlc_object_t* obj, picture_t* picture, const QSize& size)
{
    block_t* block = nullptr;
    video_format_t fmt;
    if (picture_Export(obj, &block, &fmt, picture, VLC_CODEC_RGBA,
                       size.width(), size.height(), true) != VLC_SUCCESS)
        return {};

    QImage image;
    const qsizetype stride = qsizetype(fmt.i_width) * 4;
    if (fmt.i_visible_width > 0 && fmt.i_visible_height > 0
        && size_t(stride) * fmt.i_visible_height <= block->i_buffer)
    {
        image = QImage(block->p_buffer, fmt.i_visible_width, fmt.i_visible_height,
                       stride, QImage::Format_RGBA8888).copy();
    }
    block_Release(block);
    return image;
}

class PreparserRequest : public SeekThumbnailSource::Request
{
public:
    PreparserRequest(vlc_object_t* obj, vlc_preparser_t* preparser,
                     const QSize& size, SeekThumbnailSource::Callback done)
        : m_obj(obj)
        , m_preparser(preparser)
        , m_size(size)
        , m_done(std::move(done))
    {}

    ~PreparserRequest() override
    {
        if (m_req)
            vlc_preparser_req_Release(m_req);
    }

    void cancel() override
    {
        vlc_preparser_Cancel(m_preparser, m_req);
    }

    static void onEnded(vlc_preparser_req*, int status, picture_t* picture, void* data)
    {
        auto request = static_cast<PreparserRequest*>(data);

        QImage image;
        if (status == VLC_SUCCESS && picture)
            image = toImage(request->m_obj, picture, request->m_size);

        // the request may be released by now, keep it until we are done
        std::shared_ptr<PreparserRequest> self = std::move(request->m_self);
        request->m_done(image);
    }

    vlc_object_t* m_obj;
    vlc_preparser_t* m_preparser;
    vlc_preparser_req* m_req = nullptr;
    QSize m_size;
    SeekThumbnailSource::Callback m_done;
    // alive until the preparser is done with it
    std::shared_ptr<PreparserRequest> m_self;
};

} // namespace

// PreparserSeekThumbnailSource

PreparserSeekThumbnailSource::PreparserSeekThumbnailSource(vlc_object_t* parent)
    : m_parent(parent)
{
}

PreparserSeekThumbnailSource::~PreparserSeekThumbnailSource()
{
    // cancels what remains: each request still gets its callback
    if (m_preparser)
        vlc_preparser_Delete(m_preparser);
}

std::shared_ptr<SeekThumbnailSource::Request>
PreparserSeekThumbnailSource::start(const QString& mrl, vlc_tick_t time,
                                    const QSize& size, Callback done)
{
    vlc_preparser_t* preparser;
    {
        QMutexLocker lock(&m_lock);
        if (!m_preparser)
        {
            struct vlc_preparser_cfg cfg{};
            cfg.types = VLC_PREPARSER_TYPE_THUMBNAIL;
            cfg.max_thumbnailer_threads = 1;
            cfg.timeout = THUMBNAIL_TIMEOUT;
            cfg.external_process = false;
            m_preparser = vlc_preparser_New(m_parent, &cfg);
            if (!m_preparser)
                return nullptr;
        }
        preparser = m_preparser;
    }

    input_item_t* item = input_item_New(qtu(mrl), nullptr);
    if (!item)
        return nullptr;
    /* the times are media times, from its beginning: the options given to
     * every input on the command line (--start-time, --start-paused...) must
     * not apply to it */
    static const char* const options[] = {
        ":start-time=0", ":stop-time=0", ":run-time=0", ":no-start-paused",
    };
    input_item_AddOptions(item, ARRAY_SIZE(options), options, VLC_INPUT_OPTION_TRUSTED);

    struct vlc_thumbnailer_arg arg{};
    arg.seek.type = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_TIME;
    arg.seek.time = time;
    arg.seek.speed = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_FAST;
    arg.hw_dec = false;

    static const struct vlc_thumbnailer_cbs cbs = {
        &PreparserRequest::onEnded,
    };

    auto request = std::make_shared<PreparserRequest>(m_parent, preparser, size, std::move(done));
    request->m_req = vlc_preparser_req_NewThumbnail(preparser, item, &arg, &cbs, request.get());
    input_item_Release(item);
    if (!request->m_req)
        return nullptr;

    request->m_self = request;
    if (vlc_preparser_Submit(preparser, request->m_req) != VLC_SUCCESS)
    {
        request->m_self.reset();
        return nullptr;
    }
    return request;
}

// SeekThumbnailCache

SeekThumbnailCache::SeekThumbnailCache(qsizetype maxBytes)
    : m_images(maxBytes)
{
}

void SeekThumbnailCache::insert(const QString& id, const QImage& image)
{
    QMutexLocker lock(&m_lock);
    m_images.insert(id, new QImage(image), std::max<qsizetype>(image.sizeInBytes(), 1));
}

QImage SeekThumbnailCache::find(const QString& id) const
{
    QMutexLocker lock(&m_lock);
    const QImage* image = m_images.object(id);
    return image ? *image : QImage();
}

bool SeekThumbnailCache::contains(const QString& id) const
{
    QMutexLocker lock(&m_lock);
    return m_images.contains(id);
}

void SeekThumbnailCache::clear()
{
    QMutexLocker lock(&m_lock);
    m_images.clear();
}

// SeekThumbnailProvider

SeekThumbnailProvider::SeekThumbnailProvider(std::shared_ptr<SeekThumbnailCache> cache,
                                             std::shared_ptr<SeekThumbnailSource> source)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_cache(std::move(cache))
    , m_source(std::move(source))
{
}

QImage SeekThumbnailProvider::requestImage(const QString& id, QSize* size, const QSize&)
{
    QImage image = m_cache->find(id);
    if (size)
        *size = image.size();
    return image;
}

// SeekThumbnailer

// lets a picture generated on another thread reach its seek bar, if still there
struct SeekThumbnailer::Guard
{
    QMutex lock;
    SeekThumbnailer* owner = nullptr;
};

SeekThumbnailer::SeekThumbnailer(QObject* parent)
    : SeekThumbnailer(nullptr, nullptr, parent)
{
}

SeekThumbnailer::SeekThumbnailer(std::shared_ptr<SeekThumbnailCache> cache,
                                 std::shared_ptr<SeekThumbnailSource> source,
                                 QObject* parent)
    : QObject(parent)
    , m_cache(std::move(cache))
    , m_thumbnails(std::move(source))
    , m_guard(std::make_shared<Guard>())
{
    m_guard->owner = this;
}

SeekThumbnailer::~SeekThumbnailer()
{
    {
        QMutexLocker lock(&m_guard->lock);
        m_guard->owner = nullptr;
    }
    // without holding the guard: the callback may run right away
    if (m_running)
        m_running->cancel();
}

void SeekThumbnailer::componentComplete()
{
    if (m_cache && m_thumbnails)
        return;
    QQmlEngine* engine = qmlEngine(this);
    if (!engine)
        return;
    auto provider = dynamic_cast<SeekThumbnailProvider*>(
        engine->imageProvider(QLatin1String(SeekThumbnailProvider::providerId)));
    if (!provider)
        return;
    m_cache = provider->cache();
    m_thumbnails = provider->source();
}

void SeekThumbnailer::setMrl(const QUrl& mrl)
{
    if (mrl == m_mrl)
        return;
    m_mrl = mrl;
    m_mrlKey = mrl.isEmpty()
        ? QString()
        : QString::fromLatin1(QCryptographicHash::hash(mrl.toEncoded(),
                                                       QCryptographicHash::Md5).toHex());
    reset();
    emit mrlChanged();
}

void SeekThumbnailer::setLength(const VLCDuration& length)
{
    if (length == m_length && length.valid() == m_length.valid())
        return;
    m_length = length;
    emit lengthChanged();
}

void SeekThumbnailer::setStartTime(const VLCDuration& startTime)
{
    if (startTime == m_startTime)
        return;
    m_startTime = startTime;
    emit startTimeChanged();
}

void SeekThumbnailer::setSize(const QSize& size)
{
    if (size == m_size)
        return;
    m_size = size;
    reset();
    emit sizeChanged();
}

double SeekThumbnailer::testPosition()
{
    bool ok = false;
    const double position = qEnvironmentVariable("VLC_QT_TEST_SEEK_PREVIEW").toDouble(&ok);
    return (ok && position >= 0.0 && position <= 1.0) ? position : -1.0;
}

bool SeekThumbnailer::ready() const
{
    return m_cache && m_thumbnails
        && !m_mrl.isEmpty()
        && m_length.valid() && m_length.toVLCTick() > 0
        && !m_size.isEmpty();
}

vlc_tick_t SeekThumbnailer::stepTime(double position) const
{
    const vlc_tick_t length = m_length.toVLCTick();
    const vlc_tick_t step = std::max(VLC_TICK_FROM_SEC(1), length / MAX_STEPS);
    position = qBound(0.0, position, 1.0);
    const vlc_tick_t time = std::llround(position * length / step) * step;
    // the thumbnailer opens the media from its beginning
    const vlc_tick_t start = m_startTime.valid() ? std::max<vlc_tick_t>(m_startTime.toVLCTick(), 0) : 0;
    return start + std::min(time, length);
}

QString SeekThumbnailer::pictureId(vlc_tick_t time) const
{
    return QStringLiteral("%1/%2/%3x%4")
        .arg(m_mrlKey)
        .arg(MS_FROM_VLC_TICK(time))
        .arg(m_size.width())
        .arg(m_size.height());
}

void SeekThumbnailer::show(const QString& id)
{
    const QUrl source(QStringLiteral("image://%1/%2")
                          .arg(QLatin1String(SeekThumbnailProvider::providerId), id));
    if (source == m_source)
        return;
    m_source = source;
    emit sourceChanged();
}

void SeekThumbnailer::request(double position)
{
    if (!ready())
        return;

    const vlc_tick_t time = stepTime(position);
    m_wantedTime = time;

    const QString id = pictureId(time);
    if (m_cache->contains(id))
    {
        show(id);
        return;
    }

    // one picture at a time: the latest position is generated next
    if (m_running)
        return;

    generate(time);
}

void SeekThumbnailer::stop()
{
    m_wantedTime = VLC_TICK_INVALID;
}

void SeekThumbnailer::generate(vlc_tick_t time)
{
    const unsigned generation = m_generation;
    std::shared_ptr<Guard> guard = m_guard;

    m_runningTime = time;
    m_running = m_thumbnails->start(m_mrl.toString(QUrl::FullyEncoded), time, m_size,
                                    [guard, generation, time](const QImage& image) {
        QMutexLocker lock(&guard->lock);
        SeekThumbnailer* owner = guard->owner;
        if (!owner)
            return;
        QMetaObject::invokeMethod(owner, [owner, generation, time, image]() {
            owner->onGenerated(generation, time, image);
        }, Qt::QueuedConnection);
    });
    if (!m_running)
        m_runningTime = VLC_TICK_INVALID;
}

void SeekThumbnailer::onGenerated(unsigned generation, vlc_tick_t time, const QImage& image)
{
    if (generation != m_generation || time != m_runningTime)
        return;

    m_running.reset();
    m_runningTime = VLC_TICK_INVALID;

    if (!image.isNull())
    {
        const QString id = pictureId(time);
        m_cache->insert(id, image);

        // unless the wanted picture is already shown, this one is the closest
        if (m_wantedTime == VLC_TICK_INVALID || !m_cache->contains(pictureId(m_wantedTime)))
            show(id);
        else
            show(pictureId(m_wantedTime));
    }

    if (m_wantedTime != VLC_TICK_INVALID && m_wantedTime != time
        && !m_cache->contains(pictureId(m_wantedTime)))
        generate(m_wantedTime);
}

void SeekThumbnailer::reset()
{
    ++m_generation;
    m_wantedTime = VLC_TICK_INVALID;
    m_runningTime = VLC_TICK_INVALID;

    std::shared_ptr<SeekThumbnailSource::Request> running = std::move(m_running);
    if (running)
        running->cancel();

    if (!m_source.isEmpty())
    {
        m_source.clear();
        emit sourceChanged();
    }
}
