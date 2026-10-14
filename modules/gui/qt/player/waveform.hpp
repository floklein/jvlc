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
#ifndef QVLC_WAVEFORM_HPP
#define QVLC_WAVEFORM_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <memory>

/**
 * The peak cache of the waveform seek bar: one small file per media in the
 * user cache folder, keyed by the media's location, size and modification
 * time, so an edited file is decoded again.
 */
class WaveformCache
{
public:
    explicit WaveformCache(const QString& dir);

    /// the cached loudness buckets of a local media, empty when missing or stale
    QVector<float> load(const QString& mrl) const;
    void store(const QString& mrl, const QVector<float>& peaks) const;

private:
    QString pathFor(const QString& mrl) const;

    QString m_dir;
};

/**
 * The waveform of the music player's seek bar.
 *
 * It is only available for music played from a local file. While it is
 * enabled (the user shows it), the audio is decoded in the background as fast
 * as possible and reduced to loudness buckets in 0..1, read from and written
 * to the peak cache.
 */
class SeekWaveform : public QObject
{
    Q_OBJECT

    // whether the current media can show a waveform: local music only
    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
    // whether the user shows the waveform (Show waveform toggle)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)
    // loudness buckets in 0..1 over the whole media, empty when not shown
    Q_PROPERTY(QVector<float> peaks READ peaks NOTIFY peaksChanged FINAL)

public:
    static constexpr int BUCKET_COUNT = 1024;

    SeekWaveform(vlc_object_t* parent, const QString& cacheDir, QObject* qparent = nullptr);
    ~SeekWaveform() override;

    static bool isAvailableFor(const QUrl& mrl, bool hasVideo);

    /**
     * Decode the audio of a media into `buckets` loudness values, normalized
     * so that the loudest is 1. This blocks until the end of the media: call
     * it from a background thread. Returns nothing on failure or cancel.
     */
    static QVector<float> decode(vlc_object_t* parent, const QString& mrl, int buckets,
                                 const std::atomic<bool>* cancelled = nullptr);

    /// one value per drawn bar: the loudest bucket each bar covers
    static QVector<float> resample(const QVector<float>& peaks, int count);

    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    bool loading() const { return m_loading; }
    QVector<float> peaks() const { return m_peaks; }

    void setEnabled(bool enabled);
    void setMedia(const QUrl& mrl, bool hasVideo);

    /// wait this long before decoding, so short-lived states do not start a decode
    void setStartDelay(int ms);

signals:
    void availableChanged();
    void enabledChanged();
    void loadingChanged();
    void peaksChanged();

private:
    struct Job;

    void update();
    void start();
    void cancel();
    void setLoading(bool loading);
    void setPeaks(const QVector<float>& peaks);
    void onJobDone(const std::shared_ptr<Job>& job);

    vlc_object_t* m_parent;
    WaveformCache m_cache;
    QThreadPool m_pool;
    QTimer m_startTimer;

    QUrl m_mrl;
    bool m_hasVideo = false;
    bool m_available = false;
    bool m_enabled = false;
    bool m_loading = false;
    QVector<float> m_peaks;

    std::shared_ptr<Job> m_job;
};

#endif // QVLC_WAVEFORM_HPP
