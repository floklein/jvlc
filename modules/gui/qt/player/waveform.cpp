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
#include "waveform.hpp"

#include <vlc_aout.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_demux.h>
#include <vlc_es_out.h>
#include <vlc_stream.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

/*
 * Background decode
 *
 * The media is demuxed and its first audio track decoded on the calling
 * thread, as fast as possible, without any clock or audio output. Every
 * 20 ms of decoded audio gives one window, the RMS of its frames (each frame
 * taking its loudest channel). The windows are then reduced to buckets.
 */

namespace {

constexpr unsigned WINDOWS_PER_SECOND = 50;

struct PeakAccumulator
{
    std::vector<float> windows;
    double sumSquares = 0.;
    unsigned frames = 0;

    void push(float frame, unsigned rate)
    {
        const unsigned windowFrames = std::max(1u, rate / WINDOWS_PER_SECOND);
        sumSquares += double(frame) * frame;
        if (++frames >= windowFrames)
            flush();
    }

    void flush()
    {
        if (frames == 0)
            return;
        windows.push_back(float(std::sqrt(sumSquares / frames)));
        sumSquares = 0.;
        frames = 0;
    }
};

bool readSample(vlc_fourcc_t format, const uint8_t* p, float* sample)
{
    switch (format)
    {
    case VLC_CODEC_U8:
        *sample = (int(*p) - 128) / 128.f;
        return true;
    case VLC_CODEC_S16N:
    {
        int16_t v;
        memcpy(&v, p, sizeof(v));
        *sample = v / 32768.f;
        return true;
    }
    case VLC_CODEC_S32N:
    {
        int32_t v;
        memcpy(&v, p, sizeof(v));
        *sample = float(v / 2147483648.);
        return true;
    }
    case VLC_CODEC_FL32:
    {
        float v;
        memcpy(&v, p, sizeof(v));
        *sample = v;
        return true;
    }
    case VLC_CODEC_FL64:
    {
        double v;
        memcpy(&v, p, sizeof(v));
        *sample = float(v);
        return true;
    }
    default:
        return false;
    }
}

struct DecoderOwner
{
    decoder_t dec;
    es_format_t fmt_in;
    PeakAccumulator* peaks;
};

DecoderOwner* ownerOf(decoder_t* dec)
{
    return container_of(dec, DecoderOwner, dec);
}

int UpdateAudioFormat(decoder_t* dec)
{
    dec->fmt_out.audio.i_format = dec->fmt_out.i_codec;
    aout_FormatPrepare(&dec->fmt_out.audio);
    return dec->fmt_out.audio.i_bytes_per_frame > 0 ? VLC_SUCCESS : VLC_EGENERIC;
}

void QueueAudio(decoder_t* dec, block_t* block)
{
    PeakAccumulator* peaks = ownerOf(dec)->peaks;
    const audio_format_t& fmt = dec->fmt_out.audio;

    const unsigned channels = fmt.i_channels;
    const unsigned bytes = fmt.i_bitspersample / 8;
    if (peaks && channels > 0 && bytes > 0 && fmt.i_rate > 0)
    {
        const size_t frameSize = size_t(channels) * bytes;
        const size_t frames = block->i_buffer / frameSize;
        const uint8_t* p = block->p_buffer;
        for (size_t i = 0; i < frames; ++i)
        {
            float loudest = 0.f;
            for (unsigned c = 0; c < channels; ++c, p += bytes)
            {
                float sample;
                if (!readSample(fmt.i_format, p, &sample))
                {
                    block_Release(block);
                    return;
                }
                loudest = std::max(loudest, std::fabs(sample));
            }
            peaks->push(loudest, fmt.i_rate);
        }
    }
    block_Release(block);
}

const struct decoder_owner_callbacks audioCallbacks = []() {
    struct decoder_owner_callbacks cbs {};
    cbs.audio.format_update = UpdateAudioFormat;
    cbs.audio.queue = QueueAudio;
    return cbs;
}();

decoder_t* CreateDecoderObject(vlc_object_t* parent, PeakAccumulator* peaks)
{
    auto owner = static_cast<DecoderOwner*>(vlc_object_create(parent, sizeof(DecoderOwner)));
    if (unlikely(owner == nullptr))
        return nullptr;
    owner->peaks = peaks;
    return &owner->dec;
}

bool LoadDecoder(decoder_t* dec, bool packetizer, const es_format_t* fmt)
{
    DecoderOwner* owner = ownerOf(dec);
    decoder_Init(dec, &owner->fmt_in, fmt);
    dec->cbs = &audioCallbacks;
    decoder_LoadModule(dec, packetizer, false);
    if (dec->p_module == nullptr)
    {
        decoder_Clean(dec);
        es_format_Clean(&owner->fmt_in);
        return false;
    }
    return true;
}

void UnloadDecoder(decoder_t* dec)
{
    decoder_Clean(dec);
    es_format_Clean(&ownerOf(dec)->fmt_in);
}

void DestroyDecoderObject(decoder_t* dec, bool loaded)
{
    if (loaded)
        UnloadDecoder(dec);
    vlc_object_delete(dec);
}

/// a packetizer feeding an audio decoder, as the input does
struct AudioChain
{
    decoder_t* packetizer = nullptr;
    decoder_t* decoder = nullptr;

    bool open(vlc_object_t* parent, const es_format_t* fmt, PeakAccumulator* peaks)
    {
        packetizer = CreateDecoderObject(parent, nullptr);
        decoder = CreateDecoderObject(parent, peaks);
        if (!packetizer || !decoder)
            return false;
        if (!LoadDecoder(packetizer, true, fmt))
        {
            DestroyDecoderObject(packetizer, false);
            packetizer = nullptr;
            return false;
        }
        if (!LoadDecoder(decoder, false, &packetizer->fmt_out))
        {
            DestroyDecoderObject(decoder, false);
            decoder = nullptr;
            return false;
        }
        return true;
    }

    void close()
    {
        if (decoder)
        {
            // drain
            if (decoder->p_module)
                decoder->pf_decode(decoder, nullptr);
            DestroyDecoderObject(decoder, decoder->p_module != nullptr);
        }
        if (packetizer)
            DestroyDecoderObject(packetizer, packetizer->p_module != nullptr);
        packetizer = decoder = nullptr;
    }

    void decode(block_t* block)
    {
        if (!decoder || !decoder->p_module)
        {
            block_Release(block);
            return;
        }

        block_t* packetized;
        while ((packetized = packetizer->pf_packetize(packetizer, &block)) != nullptr)
        {
            if (!es_format_IsSimilar(decoder->fmt_in, &packetizer->fmt_out))
            {
                // drain, then reload the decoder for the new format
                decoder->pf_decode(decoder, nullptr);
                UnloadDecoder(decoder);
                if (!LoadDecoder(decoder, false, &packetizer->fmt_out))
                {
                    block_ChainRelease(packetized);
                    if (block)
                        block_Release(block);
                    return;
                }
            }

            while (packetized != nullptr)
            {
                block_t* next = packetized->p_next;
                packetized->p_next = nullptr;
                if (decoder->pf_decode(decoder, packetized) == VLCDEC_ECRITICAL)
                {
                    block_ChainRelease(next);
                    if (block)
                        block_Release(block);
                    return;
                }
                packetized = next;
            }
        }
    }
};

struct DecodeState
{
    vlc_object_t* parent = nullptr;
    PeakAccumulator peaks;
    AudioChain audio;
    es_out_id_t* audioId = nullptr;
    std::vector<es_out_id_t*> ids;
    bool failed = false;
};

struct WaveformEsOut
{
    es_out_t out;
    DecodeState* state;
};

} // anonymous namespace

struct es_out_id_t
{
    es_format_t fmt;
};

namespace {

DecodeState* stateOf(es_out_t* out)
{
    return container_of(out, WaveformEsOut, out)->state;
}

void DeleteId(DecodeState* state, es_out_id_t* id)
{
    if (id == state->audioId)
    {
        state->audio.close();
        state->audioId = nullptr;
    }
    es_format_Clean(&id->fmt);
    delete id;
}

es_out_id_t* EsOutAdd(es_out_t* out, input_source_t*, const es_format_t* fmt)
{
    DecodeState* state = stateOf(out);

    auto id = new (std::nothrow) es_out_id_t;
    if (unlikely(id == nullptr))
        return nullptr;
    es_format_Copy(&id->fmt, fmt);
    state->ids.push_back(id);

    // only the first audio track is drawn
    if (fmt->i_cat == AUDIO_ES && state->audioId == nullptr && !state->failed)
    {
        if (state->audio.open(state->parent, &id->fmt, &state->peaks))
            state->audioId = id;
        else
        {
            state->audio.close();
            state->failed = true;
        }
    }
    return id;
}

int EsOutSend(es_out_t* out, es_out_id_t* id, block_t* block)
{
    DecodeState* state = stateOf(out);
    if (id == state->audioId)
        state->audio.decode(block);
    else
        block_Release(block);
    return VLC_SUCCESS;
}

void EsOutDel(es_out_t* out, es_out_id_t* id)
{
    DecodeState* state = stateOf(out);
    state->ids.erase(std::remove(state->ids.begin(), state->ids.end(), id), state->ids.end());
    DeleteId(state, id);
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
    // owned by DecodeWindows()
}

const struct es_out_callbacks esOutCallbacks = {
    EsOutAdd,
    EsOutSend,
    EsOutDel,
    EsOutControl,
    EsOutDestroy,
};

/// the RMS windows of the first audio track, empty on failure or cancel
std::vector<float> DecodeWindows(vlc_object_t* parent, const QString& mrl,
                                 const std::atomic<bool>* cancelled)
{
    const QByteArray url = mrl.toUtf8();

    DecodeState state;
    state.parent = parent;

    WaveformEsOut esOut;
    esOut.out.cbs = &esOutCallbacks;
    esOut.state = &state;

    stream_t* stream = vlc_stream_NewURL(parent, url.constData());
    if (stream == nullptr)
        return {};

    demux_t* demux = demux_New(parent, "any", url.constData(), stream, &esOut.out);
    if (demux == nullptr)
    {
        vlc_stream_Delete(stream);
        return {};
    }

    int status;
    while ((status = demux_Demux(demux)) == VLC_DEMUXER_SUCCESS)
    {
        if (cancelled && cancelled->load())
            break;
    }

    // deleting the tracks drains the decoder
    demux_Delete(demux);
    for (es_out_id_t* id : state.ids)
        DeleteId(&state, id);
    state.ids.clear();

    const bool decoded = !state.failed && status != VLC_DEMUXER_EGENERIC
                      && !(cancelled && cancelled->load());
    if (!decoded)
        return {};

    state.peaks.flush();
    return std::move(state.peaks.windows);
}

QByteArray CacheMagic()
{
    return QByteArrayLiteral("VLCWAVE1");
}

} // anonymous namespace

/*
 * WaveformCache
 */

WaveformCache::WaveformCache(const QString& dir)
    : m_dir(dir)
{
}

QString WaveformCache::pathFor(const QString& mrl) const
{
    if (m_dir.isEmpty())
        return {};

    const QUrl url(mrl);
    if (!url.isLocalFile())
        return {};

    const QFileInfo info(url.toLocalFile());
    if (!info.exists())
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(mrl.toUtf8());
    hash.addData(QByteArray::number(info.size()));
    hash.addData(QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
    return QDir(m_dir).filePath(QString::fromLatin1(hash.result().toHex()) + ".peaks");
}

QVector<float> WaveformCache::load(const QString& mrl) const
{
    const QString path = pathFor(mrl);
    if (path.isEmpty())
        return {};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray data = file.readAll();
    const QByteArray magic = CacheMagic();
    const qsizetype headerSize = magic.size() + 4;
    if (data.size() < headerSize || !data.startsWith(magic))
        return {};

    const quint32 count = qFromLittleEndian<quint32>(data.constData() + magic.size());
    if (count == 0 || data.size() != headerSize + qsizetype(count))
        return {};

    QVector<float> peaks(count);
    const auto values = reinterpret_cast<const uchar*>(data.constData() + headerSize);
    for (quint32 i = 0; i < count; ++i)
        peaks[i] = values[i] / 255.f;
    return peaks;
}

void WaveformCache::store(const QString& mrl, const QVector<float>& peaks) const
{
    const QString path = pathFor(mrl);
    if (path.isEmpty() || peaks.isEmpty())
        return;

    if (!QDir().mkpath(m_dir))
        return;

    QByteArray data = CacheMagic();
    char count[4];
    qToLittleEndian<quint32>(quint32(peaks.size()), count);
    data.append(count, sizeof(count));
    for (float peak : peaks)
        data.append(char(uchar(std::lround(std::clamp(peak, 0.f, 1.f) * 255))));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(data);
    file.commit();
}

/*
 * SeekWaveform
 */

struct SeekWaveform::Job
{
    QString mrl;
    std::atomic<bool> cancelled { false };
    QVector<float> peaks;
};

SeekWaveform::SeekWaveform(vlc_object_t* parent, const QString& cacheDir, QObject* qparent)
    : QObject(qparent)
    , m_parent(parent)
    , m_cache(cacheDir)
{
    // one decode at a time, out of the way of playback
    m_pool.setMaxThreadCount(1);
    m_pool.setThreadPriority(QThread::LowPriority);

    m_startTimer.setSingleShot(true);
    m_startTimer.setInterval(0);
    connect(&m_startTimer, &QTimer::timeout, this, &SeekWaveform::start);
}

SeekWaveform::~SeekWaveform()
{
    cancel();
    // the decode uses m_parent and posts to this object
    m_pool.waitForDone();
}

bool SeekWaveform::isAvailableFor(const QUrl& mrl, bool hasVideo)
{
    return !hasVideo && mrl.isLocalFile();
}

QVector<float> SeekWaveform::resample(const QVector<float>& peaks, int count)
{
    if (count <= 0 || peaks.isEmpty())
        return {};

    const qint64 size = peaks.size();
    QVector<float> bars(count);
    for (qint64 i = 0; i < count; ++i)
    {
        const qint64 begin = i * size / count;
        const qint64 end = std::max(begin + 1, (i + 1) * size / count);
        bars[i] = *std::max_element(peaks.cbegin() + begin, peaks.cbegin() + end);
    }
    return bars;
}

QVector<float> SeekWaveform::decode(vlc_object_t* parent, const QString& mrl, int buckets,
                                    const std::atomic<bool>* cancelled)
{
    if (buckets <= 0 || (cancelled && cancelled->load()))
        return {};

    vlc_object_t* obj = static_cast<vlc_object_t*>(vlc_object_create(parent, sizeof(*obj)));
    if (unlikely(obj == nullptr))
        return {};

    std::vector<float> windows = DecodeWindows(obj, mrl, cancelled);
    vlc_object_delete(obj);

    if (windows.empty())
        return {};

    QVector<float> peaks = resample(QVector<float>(windows.cbegin(), windows.cend()), buckets);

    const float loudest = *std::max_element(peaks.cbegin(), peaks.cend());
    if (loudest > 0.f)
    {
        for (float& peak : peaks)
            peak /= loudest;
    }
    return peaks;
}

void SeekWaveform::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    update();
}

void SeekWaveform::setMedia(const QUrl& mrl, bool hasVideo)
{
    if (m_mrl == mrl && m_hasVideo == hasVideo)
        return;

    const bool sameMedia = (m_mrl == mrl);
    m_mrl = mrl;
    m_hasVideo = hasVideo;

    const bool available = isAvailableFor(m_mrl, m_hasVideo);
    if (!sameMedia || !available)
    {
        cancel();
        setPeaks({});
    }

    if (m_available != available)
    {
        m_available = available;
        emit availableChanged();
    }
    update();
}

void SeekWaveform::setStartDelay(int ms)
{
    m_startTimer.setInterval(ms);
}

void SeekWaveform::update()
{
    if (!m_enabled || !m_available)
    {
        cancel();
        setPeaks({});
        return;
    }

    if (!m_peaks.isEmpty() || m_job || m_startTimer.isActive())
        return;

    const QString mrl = m_mrl.toString(QUrl::FullyEncoded);
    QVector<float> cached = m_cache.load(mrl);
    if (!cached.isEmpty())
    {
        setPeaks(cached);
        return;
    }

    setLoading(true);
    m_startTimer.start();
}

void SeekWaveform::start()
{
    if (!m_enabled || !m_available || m_job)
        return;

    auto job = std::make_shared<Job>();
    job->mrl = m_mrl.toString(QUrl::FullyEncoded);
    m_job = job;
    setLoading(true);

    vlc_object_t* parent = m_parent;
    const WaveformCache cache = m_cache;
    m_pool.start([this, job, parent, cache]() {
        job->peaks = decode(parent, job->mrl, BUCKET_COUNT, &job->cancelled);
        if (!job->cancelled.load())
            cache.store(job->mrl, job->peaks);

        QMetaObject::invokeMethod(this, [this, job]() {
            onJobDone(job);
        }, Qt::QueuedConnection);
    });
}

void SeekWaveform::onJobDone(const std::shared_ptr<Job>& job)
{
    if (job != m_job)
        return;

    m_job.reset();
    setLoading(false);
    if (!job->cancelled.load())
        setPeaks(job->peaks);
}

void SeekWaveform::cancel()
{
    m_startTimer.stop();
    if (m_job)
    {
        m_job->cancelled = true;
        m_job.reset();
    }
    setLoading(false);
}

void SeekWaveform::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void SeekWaveform::setPeaks(const QVector<float>& peaks)
{
    if (m_peaks == peaks)
        return;
    m_peaks = peaks;
    emit peaksChanged();
}
