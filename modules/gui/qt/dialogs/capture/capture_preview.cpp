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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "capture_preview.hpp"

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_input_item.h>
#include <vlc_picture.h>
#include <vlc_preparser.h>

namespace {

/* width of the preview frames */
constexpr int PREVIEW_WIDTH = 480;
/* a source that gives no frame in this time has no preview */
constexpr vlc_tick_t PREVIEW_TIMEOUT = VLC_TICK_FROM_SEC(8);
/* wait for the user to stop typing before opening the source */
constexpr int SOURCE_DELAY_MS = 300;

struct FrameRequest
{
    CapturePreview* preview;
    vlc_object_t* obj;
    unsigned generation;
};

} // namespace

/* called from the preparser thread, exactly once per submitted request */
void CapturePreview::onThumbnailEnded(vlc_preparser_req*, int status, picture_t* picture, void* data)
{
    auto request = static_cast<FrameRequest*>(data);

    QByteArray png;
    if (status == VLC_SUCCESS && picture)
    {
        block_t* block = nullptr;
        if (picture_Export(request->obj, &block, nullptr, picture, VLC_CODEC_PNG,
                           PREVIEW_WIDTH, 0, false) == VLC_SUCCESS)
        {
            png = QByteArray(reinterpret_cast<const char*>(block->p_buffer),
                             static_cast<qsizetype>(block->i_buffer));
            block_Release(block);
        }
    }

    /* the preview outlives the request: its destructor deletes the
     * preparser, which waits for this callback */
    CapturePreview* preview = request->preview;
    const unsigned generation = request->generation;
    delete request;
    QMetaObject::invokeMethod(preview, [preview, generation, png]() {
        preview->onFrame(generation, png);
    }, Qt::QueuedConnection);
}

CapturePreview::CapturePreview(qt_intf_t* intf, QObject* parent)
    : QObject(parent)
    , m_intf(intf)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &CapturePreview::request);
}

CapturePreview::~CapturePreview()
{
    if (m_preparser)
    {
        if (m_req)
        {
            vlc_preparser_Cancel(m_preparser, m_req);
            vlc_preparser_req_Release(m_req);
            m_req = nullptr;
        }
        vlc_preparser_Delete(m_preparser);
    }
}

void CapturePreview::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();

    if (m_active)
    {
        if (!m_mrl.isEmpty())
        {
            setStatus(Loading);
            m_timer.start(0);
        }
    }
    else
    {
        cancel();
        if (m_status == Loading)
            setStatus(m_image.isEmpty() ? Idle : Ready);
    }
}

void CapturePreview::setRefreshInterval(int interval)
{
    if (m_refreshInterval == interval)
        return;
    m_refreshInterval = interval;
    emit refreshIntervalChanged();
}

void CapturePreview::setSource(const QString& mrl, const QStringList& options)
{
    if (m_mrl == mrl && m_options == options)
        return;
    m_mrl = mrl;
    m_options = options;

    /* the frames of the previous source are stale */
    cancel();
    if (!m_image.isEmpty())
    {
        m_image.clear();
        emit imageChanged();
    }

    if (!m_active || m_mrl.isEmpty())
    {
        setStatus(Idle);
        return;
    }
    setStatus(Loading);
    m_timer.start(SOURCE_DELAY_MS);
}

void CapturePreview::request()
{
    if (!m_active || m_mrl.isEmpty() || m_req)
        return;

    if (!m_preparser)
    {
        struct vlc_preparser_cfg cfg{};
        cfg.types = VLC_PREPARSER_TYPE_THUMBNAIL;
        cfg.max_thumbnailer_threads = 1;
        cfg.timeout = PREVIEW_TIMEOUT;
        cfg.external_process = false;
        m_preparser = vlc_preparser_New(VLC_OBJECT(m_intf), &cfg);
        if (!m_preparser)
        {
            setStatus(Unavailable);
            return;
        }
    }

    input_item_t* item = input_item_New(qtu(m_mrl), nullptr);
    if (!item)
    {
        setStatus(Unavailable);
        return;
    }
    for (const QString& option : m_options)
        input_item_AddOption(item, qtu(option), VLC_INPUT_OPTION_TRUSTED);

    struct vlc_thumbnailer_arg arg{};
    arg.seek.type = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_NONE;
    arg.hw_dec = false;

    static const struct vlc_thumbnailer_cbs cbs = {
        &CapturePreview::onThumbnailEnded,
    };

    auto data = new FrameRequest{ this, VLC_OBJECT(m_intf), ++m_generation };
    m_req = vlc_preparser_req_NewThumbnail(m_preparser, item, &arg, &cbs, data);
    input_item_Release(item);
    if (!m_req)
    {
        delete data;
        setStatus(Unavailable);
        return;
    }
    if (vlc_preparser_Submit(m_preparser, m_req) != VLC_SUCCESS)
    {
        delete data;
        vlc_preparser_req_Release(m_req);
        m_req = nullptr;
        setStatus(Unavailable);
    }
}

void CapturePreview::cancel()
{
    m_timer.stop();
    if (!m_req)
        return;

    /* its frame, if any, is ignored: the generation moves on */
    ++m_generation;
    vlc_preparser_Cancel(m_preparser, m_req);
    vlc_preparser_req_Release(m_req);
    m_req = nullptr;
}

void CapturePreview::onFrame(unsigned generation, const QByteArray& png)
{
    if (generation != m_generation)
        return;

    if (m_req)
    {
        vlc_preparser_req_Release(m_req);
        m_req = nullptr;
    }

    if (png.isEmpty())
    {
        if (!m_image.isEmpty())
        {
            m_image.clear();
            emit imageChanged();
        }
        setStatus(Unavailable);
    }
    else
    {
        m_image = QUrl(QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64()));
        emit imageChanged();
        setStatus(Ready);
    }

    if (m_active && m_refreshInterval > 0)
        m_timer.start(m_refreshInterval);
}

void CapturePreview::setStatus(Status status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}
