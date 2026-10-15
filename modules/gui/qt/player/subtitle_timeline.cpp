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
#include "subtitle_timeline.hpp"

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_demux.h>
#include <vlc_es_out.h>
#include <vlc_stream.h>

#include "qt.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

/*
 * Reading the lines
 *
 * The subtitle file is demuxed on the calling thread with its own es_out,
 * which keeps the time and length of every subtitle block instead of
 * decoding it. Text subtitle demuxers hold every line in memory, so this is
 * quick.
 */

namespace {

struct ParseState
{
    std::vector<SubtitleCue> cues;
    std::vector<es_out_id_t*> ids;
    es_out_id_t* spuId = nullptr;
};

struct TimelineEsOut
{
    es_out_t out;
    ParseState* state;
};

// the tracks are only told apart, so es_out_id_t stays opaque here (the
// waveform defines its own)
struct TimelineTrack
{
    int unused;
};

void DeleteTrack(es_out_id_t* id)
{
    delete reinterpret_cast<TimelineTrack*>(id);
}

ParseState* stateOf(es_out_t* out)
{
    return container_of(out, TimelineEsOut, out)->state;
}

es_out_id_t* EsOutAdd(es_out_t* out, input_source_t*, const es_format_t* fmt)
{
    ParseState* state = stateOf(out);

    auto track = new (std::nothrow) TimelineTrack;
    if (unlikely(track == nullptr))
        return nullptr;
    auto id = reinterpret_cast<es_out_id_t*>(track);
    state->ids.push_back(id);

    // the first subtitle track of the file
    if (fmt->i_cat == SPU_ES && state->spuId == nullptr)
        state->spuId = id;
    return id;
}

int EsOutSend(es_out_t* out, es_out_id_t* id, block_t* block)
{
    ParseState* state = stateOf(out);
    if (id == state->spuId && block->i_pts != VLC_TICK_INVALID)
    {
        SubtitleCue cue;
        cue.start = MS_FROM_VLC_TICK(block->i_pts - VLC_TICK_0);
        cue.end = cue.start + MS_FROM_VLC_TICK(std::max<vlc_tick_t>(block->i_length, 0));
        state->cues.push_back(cue);
    }
    block_Release(block);
    return VLC_SUCCESS;
}

void EsOutDel(es_out_t* out, es_out_id_t* id)
{
    ParseState* state = stateOf(out);
    state->ids.erase(std::remove(state->ids.begin(), state->ids.end(), id), state->ids.end());
    if (id == state->spuId)
        state->spuId = nullptr;
    DeleteTrack(id);
}

int EsOutControl(es_out_t*, input_source_t*, int query, va_list args)
{
    switch (query)
    {
    case ES_OUT_GET_ES_STATE:
        (void) va_arg(args, es_out_id_t*);
        *va_arg(args, bool*) = true;
        return VLC_SUCCESS;
    case ES_OUT_IS_EMPTY:
        *va_arg(args, bool*) = true;
        return VLC_SUCCESS;
    case ES_OUT_SET_ES:
    case ES_OUT_RESTART_ES:
    case ES_OUT_SET_ES_DEFAULT:
    case ES_OUT_SET_ES_STATE:
    case ES_OUT_SET_ES_CAT_POLICY:
    case ES_OUT_SET_GROUP:
    case ES_OUT_SET_PCR:
    case ES_OUT_SET_GROUP_PCR:
    case ES_OUT_RESET_PCR:
    case ES_OUT_SET_ES_FMT:
    case ES_OUT_SET_NEXT_DISPLAY_TIME:
    case ES_OUT_SET_GROUP_META:
    case ES_OUT_SET_GROUP_EPG:
    case ES_OUT_DEL_GROUP:
    case ES_OUT_SET_ES_SCRAMBLED_STATE:
    case ES_OUT_DRAIN:
    case ES_OUT_SET_META:
        return VLC_SUCCESS;
    default:
        return VLC_EGENERIC;
    }
}

void EsOutDestroy(es_out_t*)
{
    // owned by ReadCues()
}

const struct es_out_callbacks esOutCallbacks = {
    EsOutAdd,
    EsOutSend,
    EsOutDel,
    EsOutControl,
    EsOutDestroy,
};

/// the lines of the first subtitle track of a file, empty on failure or cancel
std::vector<SubtitleCue> ReadCues(vlc_object_t* parent, const QString& mrl,
                                  const std::atomic<bool>* cancelled)
{
    const QByteArray url = mrl.toUtf8();

    ParseState state;

    TimelineEsOut esOut;
    esOut.out.cbs = &esOutCallbacks;
    esOut.state = &state;

    stream_t* stream = vlc_stream_NewURL(parent, url.constData());
    if (stream == nullptr)
        return {};

    // as the input opens subtitle files (input_SlaveSourceAdd)
    demux_t* demux = demux_New(parent, "subtitle", url.constData(), stream, &esOut.out);
    if (demux == nullptr)
    {
        if (vlc_stream_Seek(stream, 0) == VLC_SUCCESS)
            demux = demux_New(parent, "any", url.constData(), stream, &esOut.out);
        if (demux == nullptr)
        {
            vlc_stream_Delete(stream);
            return {};
        }
    }

    // as a slave: every line up to the end at once
    demux_Control(demux, DEMUX_SET_NEXT_DEMUX_TIME, VLC_TICK_MAX);

    int status;
    while ((status = demux_Demux(demux)) == VLC_DEMUXER_SUCCESS)
    {
        if (cancelled && cancelled->load())
            break;
    }

    demux_Delete(demux);
    for (es_out_id_t* id : state.ids)
        DeleteTrack(id);
    state.ids.clear();

    if (status == VLC_DEMUXER_EGENERIC || (cancelled && cancelled->load()))
        return {};

    std::stable_sort(state.cues.begin(), state.cues.end(),
                     [](const SubtitleCue& a, const SubtitleCue& b) { return a.start < b.start; });
    return std::move(state.cues);
}

// the spans of the strip, from the closest zoom, in milliseconds
constexpr qint64 SPANS[] = {
    10000, 20000, 30000, 60000, 2 * 60000, 5 * 60000, 10 * 60000, 30 * 60000
};
constexpr int SPAN_COUNT = int(std::size(SPANS));
// one minute
constexpr int DEFAULT_ZOOM = 3;

} // anonymous namespace

/*
 * SubtitleTimeline
 */

struct SubtitleTimeline::Job
{
    QString mrl;
    std::atomic<bool> cancelled { false };
    QVector<SubtitleCue> cues;
};

SubtitleTimeline::SubtitleTimeline(vlc_object_t* parent, QObject* qparent)
    : QObject(qparent)
    , m_parent(parent)
    , m_zoom(DEFAULT_ZOOM)
{
    // one file at a time, out of the way of playback
    m_pool.setMaxThreadCount(1);
    m_pool.setThreadPriority(QThread::LowPriority);
}

SubtitleTimeline::~SubtitleTimeline()
{
    cancel();
    // the reading uses m_parent and posts to this object
    m_pool.waitForDone();
}

bool SubtitleTimeline::isTextCodec(vlc_fourcc_t codec)
{
    switch (codec)
    {
    case VLC_CODEC_SUBT:
    case VLC_CODEC_SSA:
    case VLC_CODEC_ITU_T140:
    case VLC_CODEC_USF:
    case VLC_CODEC_TX3G:
    case VLC_CODEC_TTML:
    case VLC_CODEC_TTML_TS:
    case VLC_CODEC_WEBVTT:
        return true;
    default:
        return false;
    }
}

QVector<SubtitleBlock> SubtitleTimeline::blocks(const QVector<SubtitleCue>& cues,
                                                qint64 from, qint64 to, qint64 delay, qreal width)
{
    QVector<SubtitleBlock> blocks;
    if (to <= from || width <= 0.)
        return blocks;

    const qreal scale = width / qreal(to - from);
    for (const SubtitleCue& cue : cues)
    {
        const qint64 start = cue.start + delay;
        const qint64 end = std::max(cue.end, cue.start) + delay;
        // the lines are sorted by start
        if (start >= to)
            break;
        if (end < from || (end == from && end > start))
            continue;

        SubtitleBlock block;
        block.x = (start - from) * scale;
        block.width = std::max((end - start) * scale, MINIMUM_BLOCK_WIDTH);
        blocks.append(block);
    }
    return blocks;
}

qint64 SubtitleTimeline::span() const
{
    return SPANS[m_zoom];
}

bool SubtitleTimeline::canZoomIn() const
{
    return m_zoom > 0;
}

bool SubtitleTimeline::canZoomOut() const
{
    return m_zoom < SPAN_COUNT - 1;
}

void SubtitleTimeline::zoomIn()
{
    if (!canZoomIn())
        return;
    --m_zoom;
    emit spanChanged();
}

void SubtitleTimeline::zoomOut()
{
    if (!canZoomOut())
        return;
    ++m_zoom;
    emit spanChanged();
}

qint64 SubtitleTimeline::windowStart(qint64 time, qint64 length) const
{
    const qint64 span = this->span();
    if (length > 0 && length <= span)
        return 0;

    qint64 start = std::max<qint64>(time, 0) / span * span;
    if (length > 0)
        start = std::min(start, length - span);
    return start;
}

qint64 SubtitleTimeline::draggedDelay(qint64 delay, qreal dx, qreal width) const
{
    if (width <= 0.)
        return delay;
    // to the nearest 10 ms
    const qint64 shift = std::llround(dx * span() / width / 10.) * 10;
    return delay + shift;
}

QString SubtitleTimeline::unsupportedReason() const
{
    switch (m_kind)
    {
    case EmbeddedTrack:
        return qtr("Only subtitles from a separate file can be shown on the timeline.");
    case ExternalPictureTrack:
        return qtr("Only text subtitles can be shown on the timeline.");
    default:
        return {};
    }
}

QString SubtitleTimeline::note() const
{
    if (m_kind == NoTrack)
        return qtr("Shows the lines of a subtitle file once one is on.");
    return unsupportedReason();
}

void SubtitleTimeline::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    update();
}

void SubtitleTimeline::setTrack(TrackKind kind, const QString& mrl)
{
    const QString source = (kind == ExternalTextTrack) ? mrl : QString();
    if (m_kind == kind && m_mrl == source)
        return;

    const QString oldReason = note();
    m_kind = kind;
    if (m_mrl != source)
    {
        m_mrl = source;
        cancel();
        m_read = false;
        setCues({});
    }

    if (note() != oldReason)
        emit unsupportedReasonChanged();
    update();
}

void SubtitleTimeline::update()
{
    if (!m_enabled || m_kind != ExternalTextTrack)
    {
        cancel();
        updateShown();
        return;
    }

    if (m_read || m_job)
    {
        updateShown();
        return;
    }

    auto job = std::make_shared<Job>();
    job->mrl = m_mrl;
    m_job = job;
    setLoading(true);

    vlc_object_t* parent = m_parent;
    m_pool.start([this, job, parent]() {
        job->cues = parse(parent, job->mrl, &job->cancelled);

        QMetaObject::invokeMethod(this, [this, job]() {
            onJobDone(job);
        }, Qt::QueuedConnection);
    });
}

void SubtitleTimeline::onJobDone(const std::shared_ptr<Job>& job)
{
    if (job != m_job)
        return;

    m_job.reset();
    setLoading(false);
    if (job->cancelled.load())
        return;
    m_read = true;
    setCues(job->cues);
    updateShown();
}

void SubtitleTimeline::cancel()
{
    if (m_job)
    {
        m_job->cancelled = true;
        m_job.reset();
    }
    setLoading(false);
}

void SubtitleTimeline::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void SubtitleTimeline::setCues(const QVector<SubtitleCue>& cues)
{
    if (cues.isEmpty() && m_cues.isEmpty())
        return;
    m_cues = cues;
    emit cuesChanged();
}

void SubtitleTimeline::updateShown()
{
    const bool shown = m_enabled && m_kind == ExternalTextTrack && !m_cues.isEmpty();
    if (m_shown == shown)
        return;
    m_shown = shown;
    emit shownChanged();
}

QVector<SubtitleCue> SubtitleTimeline::parse(vlc_object_t* parent, const QString& mrl,
                                             const std::atomic<bool>* cancelled)
{
    if (cancelled && cancelled->load())
        return {};

    vlc_object_t* obj = static_cast<vlc_object_t*>(vlc_object_create(parent, sizeof(*obj)));
    if (unlikely(obj == nullptr))
        return {};
    // the subtitle demuxer reads it from the input (input/var.c); unset,
    // frame-based files use their default rate
    var_Create(obj, "sub-original-fps", VLC_VAR_FLOAT);

    std::vector<SubtitleCue> cues = ReadCues(obj, mrl, cancelled);
    vlc_object_delete(obj);

    return QVector<SubtitleCue>(cues.cbegin(), cues.cend());
}
