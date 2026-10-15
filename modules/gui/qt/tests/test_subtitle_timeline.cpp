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
#include <QFile>
#include <QUrl>

#include "../player/subtitle_timeline.hpp"

// The subtitle timeline of the seek bar (spec #30, ticket #54): the lines of
// the external text subtitle shown as blocks the viewer shifts to sync
class TestSubtitleTimeline : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    libvlc_instance_t* m_vlc = nullptr;

    vlc_object_t* vlcObject() const
    {
        return VLC_OBJECT(m_vlc->p_libvlc_int);
    }

    QString writeFile(const QString& name, const QByteArray& content)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(content);
        return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    }

    QString writeSrt(const QString& name)
    {
        return writeFile(name,
            "1\n"
            "00:00:01,000 --> 00:00:02,500\n"
            "Hello\n"
            "\n"
            "2\n"
            "00:00:04,000 --> 00:00:06,000\n"
            "World\n"
            "\n");
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

    // The lines of a subtitle file, in media time
    void testParseReadsEveryLine()
    {
        const QVector<SubtitleCue> cues = SubtitleTimeline::parse(vlcObject(), writeSrt("lines.srt"));

        QCOMPARE(cues.size(), 2);
        QCOMPARE(cues[0].start, 1000);
        QCOMPARE(cues[0].end, 2500);
        QCOMPARE(cues[1].start, 4000);
        QCOMPARE(cues[1].end, 6000);
    }

    void testParseReadsWebVtt()
    {
        const QString mrl = writeFile("lines.vtt",
            "WEBVTT\n"
            "\n"
            "00:00:03.000 --> 00:00:04.200\n"
            "Hello\n"
            "\n"
            "00:01:00.000 --> 00:01:02.000\n"
            "World\n"
            "\n");

        const QVector<SubtitleCue> cues = SubtitleTimeline::parse(vlcObject(), mrl);

        QCOMPARE(cues.size(), 2);
        QCOMPARE(cues[0].start, 3000);
        QCOMPARE(cues[0].end, 4200);
        QCOMPARE(cues[1].start, 60000);
        QCOMPARE(cues[1].end, 62000);
    }

    void testParseFailsOnMissingFile()
    {
        const QString mrl = QUrl::fromLocalFile(m_dir.filePath("missing.srt")).toString(QUrl::FullyEncoded);
        QVERIFY(SubtitleTimeline::parse(vlcObject(), mrl).isEmpty());
    }

    // With the timeline on, choosing an external text subtitle reads its
    // lines in the background, then the seek bar shows them
    void testExternalTextSubtitleIsShown()
    {
        SubtitleTimeline timeline(vlcObject());
        QSignalSpy shownSpy(&timeline, &SubtitleTimeline::shownChanged);

        timeline.setEnabled(true);
        timeline.setTrack(SubtitleTimeline::ExternalTextTrack, writeSrt("shown.srt"));

        QVERIFY(timeline.unsupportedReason().isEmpty());
        QTRY_VERIFY(timeline.shown());
        QVERIFY(!timeline.loading());
        QCOMPARE(timeline.cues().size(), 2);
        QCOMPARE(shownSpy.count(), 1);

        // no subtitle any more: nothing to show, the toggle stays usable
        timeline.setTrack(SubtitleTimeline::NoTrack);
        QVERIFY(!timeline.shown());
        QVERIFY(timeline.cues().isEmpty());
        QVERIFY(timeline.unsupportedReason().isEmpty());
    }

    // Turned off, the lines are not read
    void testHiddenTimelineReadsNothing()
    {
        SubtitleTimeline timeline(vlcObject());
        timeline.setEnabled(false);
        timeline.setTrack(SubtitleTimeline::ExternalTextTrack, writeSrt("hidden.srt"));

        QVERIFY(!timeline.loading());
        QVERIFY(!timeline.shown());
        QVERIFY(timeline.cues().isEmpty());

        timeline.setEnabled(true);
        QTRY_VERIFY(timeline.shown());
    }

    // Embedded tracks would need a scan of the whole media: the toggle is
    // disabled with an explanation, and so are picture subtitles
    void testEmbeddedTrackIsNotSupported()
    {
        SubtitleTimeline timeline(vlcObject());
        timeline.setEnabled(true);

        timeline.setTrack(SubtitleTimeline::EmbeddedTrack);
        QVERIFY(!timeline.unsupportedReason().isEmpty());
        QVERIFY(!timeline.loading());
        QVERIFY(!timeline.shown());

        timeline.setTrack(SubtitleTimeline::ExternalPictureTrack, writeSrt("picture.srt"));
        QVERIFY(!timeline.unsupportedReason().isEmpty());
        QVERIFY(!timeline.loading());
        QVERIFY(!timeline.shown());
    }

    // − + zoom: the strip shows one minute at first, from 10 s to 30 min
    void testZoomSteps()
    {
        SubtitleTimeline timeline(vlcObject());
        QSignalSpy spanSpy(&timeline, &SubtitleTimeline::spanChanged);

        QCOMPARE(timeline.span(), 60000);

        timeline.zoomIn();
        QCOMPARE(timeline.span(), 30000);
        timeline.zoomIn();
        timeline.zoomIn();
        QCOMPARE(timeline.span(), 10000);
        QVERIFY(!timeline.canZoomIn());
        timeline.zoomIn();
        QCOMPARE(timeline.span(), 10000);
        QCOMPARE(spanSpy.count(), 3);

        for (int i = 0; i < 10; ++i)
            timeline.zoomOut();
        QCOMPARE(timeline.span(), 30 * 60000);
        QVERIFY(!timeline.canZoomOut());
        QVERIFY(timeline.canZoomIn());
    }

    // The strip turns a page when playback leaves it, and its last page ends
    // with the media
    void testWindowFollowsPlayback()
    {
        SubtitleTimeline timeline(vlcObject());
        const qint64 length = 590000;

        QCOMPARE(timeline.windowStart(0, length), 0);
        QCOMPARE(timeline.windowStart(59999, length), 0);
        QCOMPARE(timeline.windowStart(60000, length), 60000);
        QCOMPARE(timeline.windowStart(125000, length), 120000);
        QCOMPARE(timeline.windowStart(580000, length), 530000);

        // a media shorter than the strip fits in it
        QCOMPARE(timeline.windowStart(20000, 30000), 0);
        // a live stream has no end
        QCOMPARE(timeline.windowStart(1250000, -1), 1200000);
    }

    // Dragging the strip shifts the whole track by the time dragged over, to
    // the nearest 10 ms
    void testDragShiftsTheDelay()
    {
        SubtitleTimeline timeline(vlcObject());
        // one minute over 1000 px: 60 ms per pixel

        QCOMPARE(timeline.draggedDelay(0, 100., 1000.), 6000);
        QCOMPARE(timeline.draggedDelay(500, -10., 1000.), -100);
        QCOMPARE(timeline.draggedDelay(0, 0.1, 1000.), 10);
        QCOMPARE(timeline.draggedDelay(250, 0., 1000.), 250);
        QCOMPARE(timeline.draggedDelay(250, 50., 0.), 250);

        timeline.zoomIn(); // 30 ms per pixel
        QCOMPARE(timeline.draggedDelay(0, 100., 1000.), 3000);
    }

    // The blocks of the lines in the strip, moved by the subtitle delay
    void testBlocksFollowTheDelay()
    {
        const QVector<SubtitleCue> cues {
            { 1000, 2500 }, { 4000, 6000 }, { 7000, 7000 }, { 12000, 13000 }
        };

        // 10 s over 1000 px
        QVector<SubtitleBlock> blocks = SubtitleTimeline::blocks(cues, 0, 10000, 0, 1000.);
        QCOMPARE(blocks.size(), 3);
        QCOMPARE(blocks[0].x, 100.);
        QCOMPARE(blocks[0].width, 150.);
        QCOMPARE(blocks[1].x, 400.);
        QCOMPARE(blocks[1].width, 200.);
        // a line without length still shows
        QCOMPARE(blocks[2].x, 700.);
        QCOMPARE(blocks[2].width, SubtitleTimeline::MINIMUM_BLOCK_WIDTH);

        // 2.5 s later: the first line leaves, the last one comes in
        blocks = SubtitleTimeline::blocks(cues, 10000, 20000, -2500, 1000.);
        QCOMPARE(blocks.size(), 1);
        QCOMPARE(blocks[0].x, -50.);
        QCOMPARE(blocks[0].width, 100.);

        blocks = SubtitleTimeline::blocks(cues, 0, 10000, 500, 1000.);
        QCOMPARE(blocks.size(), 3);
        QCOMPARE(blocks[0].x, 150.);
        QCOMPARE(blocks[1].x, 450.);
    }
};

QTEST_GUILESS_MAIN(TestSubtitleTimeline)
#include "test_subtitle_timeline.moc"
