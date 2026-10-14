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

#include <vlc_common.h>

#include <QTest>
#include <QSignalSpy>
#include <QImage>

#include <memory>
#include <thread>
#include <vector>

#include "../player/seek_thumbnailer.hpp"

namespace {

// A thumbnail generator driven by the test: it records what the seek bar
// asks for, and the test decides when each picture is ready.
class FakeSource : public SeekThumbnailSource
{
public:
    struct Job : public SeekThumbnailSource::Request
    {
        QString mrl;
        vlc_tick_t time;
        QSize size;
        Callback done;
        bool cancelled = false;
        bool ended = false;

        void cancel() override
        {
            if (ended)
                return;
            cancelled = true;
            finish(QImage());
        }

        void finish(const QImage& image)
        {
            ended = true;
            done(image);
        }
    };

    std::vector<std::shared_ptr<Job>> jobs;

    std::shared_ptr<Request> start(const QString& mrl, vlc_tick_t time,
                                   const QSize& size, Callback done) override
    {
        auto job = std::make_shared<Job>();
        job->mrl = mrl;
        job->time = time;
        job->size = size;
        job->done = std::move(done);
        jobs.push_back(job);
        return job;
    }
};

QImage picture(const QSize& size, Qt::GlobalColor color)
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

} // namespace

class TestSeekThumbnailer : public QObject
{
    Q_OBJECT

private:
    std::shared_ptr<SeekThumbnailCache> m_cache;
    std::shared_ptr<FakeSource> m_source;
    std::unique_ptr<SeekThumbnailProvider> m_provider;

    const QSize m_size { 176, 99 };
    const QUrl m_mrl { QStringLiteral("file:///media/The%20Wand/S01E04.mkv") };

    std::unique_ptr<SeekThumbnailer> makeThumbnailer(vlc_tick_t length = VLC_TICK_FROM_SEC(200))
    {
        auto thumbnailer = std::make_unique<SeekThumbnailer>(m_cache, m_source);
        thumbnailer->setMrl(m_mrl);
        thumbnailer->setLength(VLCDuration(length));
        thumbnailer->setSize(m_size);
        return thumbnailer;
    }

    // the picture the seek bar shows, as QML would load it
    QImage shown(const SeekThumbnailer& thumbnailer)
    {
        const QUrl source = thumbnailer.source();
        if (source.isEmpty())
            return {};
        if (source.scheme() != QLatin1String("image")
            || source.host() != QLatin1String(SeekThumbnailProvider::providerId))
            return {};
        QSize size;
        return m_provider->requestImage(source.path().mid(1), &size, m_size);
    }

private slots:
    void init()
    {
        m_cache = std::make_shared<SeekThumbnailCache>();
        m_source = std::make_shared<FakeSource>();
        m_provider = std::make_unique<SeekThumbnailProvider>(m_cache, m_source);
    }

    void cleanup()
    {
        m_provider.reset();
        m_source.reset();
        m_cache.reset();
    }

    void hoverGeneratesThePictureAtThatTime()
    {
        auto thumbnailer = makeThumbnailer();

        thumbnailer->request(0.25);

        QCOMPARE(m_source->jobs.size(), size_t(1));
        QCOMPARE(m_source->jobs[0]->mrl, QStringLiteral("file:///media/The%20Wand/S01E04.mkv"));
        QCOMPARE(m_source->jobs[0]->time, VLC_TICK_FROM_SEC(50));
        QCOMPARE(m_source->jobs[0]->size, m_size);
        QVERIFY(thumbnailer->source().isEmpty());

        QSignalSpy sourceChanged(thumbnailer.get(), &SeekThumbnailer::sourceChanged);
        m_source->jobs[0]->finish(picture(m_size, Qt::red));

        QTRY_COMPARE(sourceChanged.count(), 1);
        QCOMPARE(shown(*thumbnailer), picture(m_size, Qt::red));
    }

    void picturesArriveFromTheGeneratorThread()
    {
        auto thumbnailer = makeThumbnailer();
        thumbnailer->request(0.5);
        QCOMPARE(m_source->jobs.size(), size_t(1));

        std::thread generator([job = m_source->jobs[0], this] {
            job->finish(picture(m_size, Qt::green));
        });
        generator.join();

        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::green));
    }

    void nearbyPositionsShareOnePicture()
    {
        // 200 s media: one picture per second
        auto thumbnailer = makeThumbnailer(VLC_TICK_FROM_SEC(200));

        thumbnailer->request(0.5);       // 100 s
        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::red));

        thumbnailer->request(0.5024);    // 100.48 s
        QCOMPARE(m_source->jobs.size(), size_t(1));
        QCOMPARE(shown(*thumbnailer), picture(m_size, Qt::red));

        thumbnailer->request(0.5026);    // 100.52 s
        QCOMPARE(m_source->jobs.size(), size_t(2));
        QCOMPARE(m_source->jobs[1]->time, VLC_TICK_FROM_SEC(101));
    }

    void longMediaUseCoarserSteps()
    {
        // 2 h media: a picture every 18 s at most 400 pictures over the bar
        auto thumbnailer = makeThumbnailer(VLC_TICK_FROM_SEC(7200));

        thumbnailer->request(0.0013);    // 9.36 s
        QCOMPARE(m_source->jobs.size(), size_t(1));
        QCOMPARE(m_source->jobs[0]->time, VLC_TICK_FROM_SEC(18));
    }

    void aCachedPictureShowsAtOnce()
    {
        auto thumbnailer = makeThumbnailer();

        thumbnailer->request(0.25);
        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::red));

        thumbnailer->request(0.75);
        m_source->jobs[1]->finish(picture(m_size, Qt::blue));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::blue));

        // back to the first position: no new generation, no event loop
        thumbnailer->request(0.25);
        QCOMPARE(m_source->jobs.size(), size_t(2));
        QCOMPARE(shown(*thumbnailer), picture(m_size, Qt::red));
    }

    void anotherSeekBarReusesTheCache()
    {
        auto first = makeThumbnailer();
        first->request(0.25);
        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTRY_COMPARE(shown(*first), picture(m_size, Qt::red));

        auto second = makeThumbnailer();
        second->request(0.25);
        QCOMPARE(m_source->jobs.size(), size_t(1));
        QCOMPARE(shown(*second), picture(m_size, Qt::red));
    }

    void aBurstOfMovesOnlyGeneratesTheLatestPosition()
    {
        auto thumbnailer = makeThumbnailer();

        thumbnailer->request(0.10);
        QCOMPARE(m_source->jobs.size(), size_t(1));

        // the pointer keeps moving while the first picture is generated
        thumbnailer->request(0.20);
        thumbnailer->request(0.30);
        thumbnailer->request(0.40);
        QCOMPARE(m_source->jobs.size(), size_t(1));

        // the first picture shows while the latest position is generated
        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::red));
        QCOMPARE(m_source->jobs.size(), size_t(2));
        QCOMPARE(m_source->jobs[1]->time, VLC_TICK_FROM_SEC(80));

        m_source->jobs[1]->finish(picture(m_size, Qt::blue));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::blue));
        QCOMPARE(m_source->jobs.size(), size_t(2));
    }

    void comingBackToThePositionBeingGeneratedStartsNothingNew()
    {
        auto thumbnailer = makeThumbnailer();

        thumbnailer->request(0.10);
        thumbnailer->request(0.30);
        thumbnailer->request(0.10);

        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::red));
        QCOMPARE(m_source->jobs.size(), size_t(1));
    }

    void aFailedPictureKeepsThePreviousOne()
    {
        auto thumbnailer = makeThumbnailer();

        thumbnailer->request(0.10);
        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::red));

        thumbnailer->request(0.90);
        QSignalSpy sourceChanged(thumbnailer.get(), &SeekThumbnailer::sourceChanged);
        m_source->jobs[1]->finish(QImage());
        QTest::qWait(0);
        QCoreApplication::processEvents();
        QCOMPARE(sourceChanged.count(), 0);
        QCOMPARE(shown(*thumbnailer), picture(m_size, Qt::red));
    }

    void anotherMediaDropsTheOldPictures()
    {
        auto thumbnailer = makeThumbnailer();

        thumbnailer->request(0.10);
        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTRY_COMPARE(shown(*thumbnailer), picture(m_size, Qt::red));

        thumbnailer->request(0.30);
        QCOMPARE(m_source->jobs.size(), size_t(2));

        const QUrl next(QStringLiteral("file:///media/The%20Wand/S01E05.mkv"));
        thumbnailer->setMrl(next);

        // the picture of the previous media is no longer shown, and the one
        // being generated for it is cancelled
        QVERIFY(thumbnailer->source().isEmpty());
        QVERIFY(m_source->jobs[1]->cancelled);

        thumbnailer->request(0.10);
        QCOMPARE(m_source->jobs.size(), size_t(3));
        QCOMPARE(m_source->jobs[2]->mrl, QStringLiteral("file:///media/The%20Wand/S01E05.mkv"));
    }

    void stopKeepsGeneratingOnlyWhatIsUnderway()
    {
        auto thumbnailer = makeThumbnailer();

        thumbnailer->request(0.10);
        thumbnailer->request(0.30);
        // the pointer leaves the seek bar
        thumbnailer->stop();

        m_source->jobs[0]->finish(picture(m_size, Qt::red));
        QTest::qWait(0);
        QCoreApplication::processEvents();
        QCOMPARE(m_source->jobs.size(), size_t(1));
    }

    void noPictureWithoutMediaOrLength()
    {
        auto thumbnailer = makeThumbnailer(VLC_TICK_INVALID);
        thumbnailer->setLength(VLCDuration());
        thumbnailer->request(0.5);
        QCOMPARE(m_source->jobs.size(), size_t(0));

        thumbnailer->setLength(VLCDuration(VLC_TICK_FROM_SEC(10)));
        thumbnailer->setMrl(QUrl());
        thumbnailer->request(0.5);
        QCOMPARE(m_source->jobs.size(), size_t(0));
    }

    void destroyingTheSeekBarCancelsItsPicture()
    {
        auto thumbnailer = makeThumbnailer();
        thumbnailer->request(0.10);
        auto job = m_source->jobs[0];

        thumbnailer.reset();
        QVERIFY(job->cancelled);
        QCoreApplication::processEvents();
    }

    void aPictureArrivingAfterTheSeekBarIsGoneIsHarmless()
    {
        auto thumbnailer = makeThumbnailer();
        thumbnailer->request(0.10);
        auto job = m_source->jobs[0];
        // a generator that cannot cancel any more
        job->ended = true;

        thumbnailer.reset();

        job->done(picture(m_size, Qt::red));
        QCoreApplication::processEvents();
    }
};

QTEST_GUILESS_MAIN(TestSeekThumbnailer)
#include "test_seek_thumbnailer.moc"
