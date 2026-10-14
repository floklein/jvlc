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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_common.h>
#include "../../../../lib/libvlc_internal.h"

#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QtEndian>

#include <algorithm>
#include <cmath>

#include "../player/waveform.hpp"

// The waveform seek bar of the music player: a background decode of local
// audio files reduced to loudness buckets, kept in a peak cache
class TestWaveform : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    libvlc_instance_t* m_vlc = nullptr;

    vlc_object_t* vlcObject() const
    {
        return VLC_OBJECT(m_vlc->p_libvlc_int);
    }

    // A mono 16-bit WAV file: one loud second, then one quiet second
    QString writeLoudThenQuietWav(const QString& name)
    {
        const int rate = 8000;
        const int samples = rate * 2;

        QByteArray data;
        data.resize(samples * 2);
        for (int i = 0; i < samples; ++i)
        {
            const double amplitude = (i < rate) ? 0.8 : 0.1;
            const double value = amplitude * std::sin(2 * M_PI * 440. * i / rate);
            qToLittleEndian<qint16>(qint16(value * 32767), data.data() + i * 2);
        }

        QByteArray header;
        auto put32 = [&header](quint32 v) { char b[4]; qToLittleEndian(v, b); header.append(b, 4); };
        auto put16 = [&header](quint16 v) { char b[2]; qToLittleEndian(v, b); header.append(b, 2); };
        header.append("RIFF");
        put32(36 + data.size());
        header.append("WAVEfmt ");
        put32(16);
        put16(1);           // PCM
        put16(1);           // mono
        put32(rate);
        put32(rate * 2);    // byte rate
        put16(2);           // block align
        put16(16);          // bits per sample
        header.append("data");
        put32(data.size());

        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(header);
        file.write(data);
        return path;
    }

    static QString mrlOf(const QString& path)
    {
        return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    }

    static float maxOf(const QVector<float>& peaks, int from, int to)
    {
        return *std::max_element(peaks.begin() + from, peaks.begin() + to);
    }

    static float minOf(const QVector<float>& peaks, int from, int to)
    {
        return *std::min_element(peaks.begin() + from, peaks.begin() + to);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());

        // keep paths set by the caller (cross-built Windows tests)
        setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 0);
        setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 0);

        static const char* args[] = { "--ignore-config", "-v" };
        m_vlc = libvlc_new(ARRAY_SIZE(args), args);
        QVERIFY(m_vlc);
    }

    void cleanupTestCase()
    {
        if (m_vlc)
            libvlc_release(m_vlc);
    }

    // The toggle and the bar only exist for music played from a local file
    void testAvailableForLocalMusicOnly()
    {
        const QUrl local = QUrl::fromLocalFile(m_dir.filePath("song.flac"));

        QVERIFY(SeekWaveform::isAvailableFor(local, false));
        QVERIFY(!SeekWaveform::isAvailableFor(local, true));
        QVERIFY(!SeekWaveform::isAvailableFor(QUrl("https://example.org/radio.mp3"), false));
        QVERIFY(!SeekWaveform::isAvailableFor(QUrl("cdda:///D:/"), false));
        QVERIFY(!SeekWaveform::isAvailableFor(QUrl(), false));
    }

    void testDecodeFollowsTheLoudness()
    {
        const QString path = writeLoudThenQuietWav("loudness.wav");
        QVERIFY(!path.isEmpty());

        const QVector<float> peaks = SeekWaveform::decode(vlcObject(), mrlOf(path), 100);

        QCOMPARE(peaks.size(), 100);
        // normalized: the loudest bucket is 1
        QVERIFY(qFuzzyCompare(maxOf(peaks, 0, 100), 1.f));
        // the loud second, then the quiet one at an eighth of it
        QVERIFY(minOf(peaks, 5, 45) > 0.9f);
        QVERIFY(maxOf(peaks, 55, 95) < 0.2f);
        QVERIFY(minOf(peaks, 55, 95) > 0.05f);
    }

    void testDecodeFailsOnMissingFile()
    {
        const QVector<float> peaks = SeekWaveform::decode(vlcObject(), mrlOf(m_dir.filePath("missing.wav")), 100);
        QVERIFY(peaks.isEmpty());
    }

    void testDecodeCanBeCancelled()
    {
        const QString path = writeLoudThenQuietWav("cancelled.wav");
        std::atomic<bool> cancelled { true };

        QVERIFY(SeekWaveform::decode(vlcObject(), mrlOf(path), 100, &cancelled).isEmpty());
    }

    // The seek bar draws one bar per few pixels: each bar shows the loudest
    // bucket it covers
    void testResampleKeepsTheLoudestBucket()
    {
        const QVector<float> peaks { 0.1f, 0.9f, 0.2f, 0.4f };

        QCOMPARE(SeekWaveform::resample(peaks, 2), (QVector<float>{ 0.9f, 0.4f }));
        QCOMPARE(SeekWaveform::resample(peaks, 8),
                 (QVector<float>{ 0.1f, 0.1f, 0.9f, 0.9f, 0.2f, 0.2f, 0.4f, 0.4f }));
        QCOMPARE(SeekWaveform::resample(peaks, 0), QVector<float>{});
        QCOMPARE(SeekWaveform::resample({}, 4), QVector<float>{});
    }

    void testCacheSurvivesRestart()
    {
        const QString path = writeLoudThenQuietWav("cached.wav");
        const QString cacheDir = m_dir.filePath("cache-restart");
        const QVector<float> peaks { 0.f, 0.25f, 0.5f, 1.f };

        {
            WaveformCache cache(cacheDir);
            QVERIFY(cache.load(mrlOf(path)).isEmpty());
            cache.store(mrlOf(path), peaks);
        }

        WaveformCache cache(cacheDir);
        const QVector<float> loaded = cache.load(mrlOf(path));
        QCOMPARE(loaded.size(), peaks.size());
        for (int i = 0; i < peaks.size(); ++i)
            QVERIFY(std::abs(loaded[i] - peaks[i]) < 0.01f);
    }

    void testCacheMissesWhenTheFileChanges()
    {
        const QString path = writeLoudThenQuietWav("changed.wav");
        const QString cacheDir = m_dir.filePath("cache-changed");

        WaveformCache cache(cacheDir);
        cache.store(mrlOf(path), { 0.5f, 1.f });

        QFile file(path);
        QVERIFY(file.open(QIODevice::Append));
        file.write(QByteArray(64, '\0'));
        file.close();

        QVERIFY(WaveformCache(cacheDir).load(mrlOf(path)).isEmpty());
    }

    // Playing local music with the waveform shown computes it in the
    // background and stores it in the peak cache
    void testComputesInTheBackgroundForLocalMusic()
    {
        const QString path = writeLoudThenQuietWav("played.wav");
        const QString cacheDir = m_dir.filePath("cache-played");

        SeekWaveform waveform(vlcObject(), cacheDir);
        QSignalSpy peaksSpy(&waveform, &SeekWaveform::peaksChanged);

        waveform.setEnabled(true);
        waveform.setMedia(QUrl(mrlOf(path)), false);
        QVERIFY(waveform.available());

        QTRY_VERIFY(!waveform.peaks().isEmpty());
        QVERIFY(!waveform.loading());
        QVERIFY(waveform.peaks().first() > 0.9f);
        QVERIFY(waveform.peaks().last() < 0.2f);
        QVERIFY(!QDir(cacheDir).entryList(QDir::Files).isEmpty());

        // another media clears it
        waveform.setMedia(QUrl("https://example.org/radio.mp3"), false);
        QVERIFY(!waveform.available());
        QVERIFY(waveform.peaks().isEmpty());
    }

    void testHiddenWaveformIsNotComputed()
    {
        const QString path = writeLoudThenQuietWav("hidden.wav");
        const QString cacheDir = m_dir.filePath("cache-hidden");

        SeekWaveform waveform(vlcObject(), cacheDir);
        waveform.setEnabled(false);
        waveform.setMedia(QUrl(mrlOf(path)), false);

        QVERIFY(waveform.available());
        QVERIFY(!waveform.loading());
        QVERIFY(waveform.peaks().isEmpty());

        // showing it starts the computation
        waveform.setEnabled(true);
        QTRY_VERIFY(!waveform.peaks().isEmpty());
    }

    void testVideoGetsNoWaveform()
    {
        const QString path = writeLoudThenQuietWav("video.wav");

        SeekWaveform waveform(vlcObject(), m_dir.filePath("cache-video"));
        waveform.setEnabled(true);
        waveform.setMedia(QUrl(mrlOf(path)), true);

        QVERIFY(!waveform.available());
        QVERIFY(!waveform.loading());
        QVERIFY(waveform.peaks().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestWaveform)
#include "test_waveform.moc"
