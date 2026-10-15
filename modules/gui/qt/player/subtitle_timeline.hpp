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
#ifndef QVLC_SUBTITLE_TIMELINE_HPP
#define QVLC_SUBTITLE_TIMELINE_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <QObject>
#include <QString>
#include <QVector>
#include <QThreadPool>

#include <atomic>
#include <memory>

/// one subtitle line, in milliseconds of media time, without the delay
struct SubtitleCue
{
    qint64 start = 0;
    qint64 end = 0;
};

/// where a line is drawn in the strip, in pixels
struct SubtitleBlock
{
    qreal x = 0.;
    qreal width = 0.;
};

/**
 * The subtitle timeline of the seek bar: the lines of the subtitle track
 * drawn as blocks, which the viewer shifts to sync the subtitles.
 *
 * Only external text subtitles are covered: their file is read in the
 * background while the timeline is on (Show subtitle timeline). Embedded
 * tracks would need a scan of the whole media.
 */
class SubtitleTimeline : public QObject
{
    Q_OBJECT

    // whether the user shows the timeline (Show subtitle timeline toggle)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    // why the current subtitle cannot be shown, empty when it can or when
    // there is no subtitle: the toggle is disabled with this explanation
    Q_PROPERTY(QString unsupportedReason READ unsupportedReason NOTIFY unsupportedReasonChanged FINAL)
    // under the toggle: the unsupported reason, or why nothing shows without
    // a subtitle; empty when the subtitle shows
    Q_PROPERTY(QString note READ note NOTIFY unsupportedReasonChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)
    // the seek bar shows the timeline: it is on and the lines are known
    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged FINAL)
    // the milliseconds of media the strip shows, set by zoomIn()/zoomOut()
    Q_PROPERTY(qint64 span READ span NOTIFY spanChanged FINAL)
    Q_PROPERTY(bool canZoomIn READ canZoomIn NOTIFY spanChanged FINAL)
    Q_PROPERTY(bool canZoomOut READ canZoomOut NOTIFY spanChanged FINAL)

public:
    static constexpr qreal MINIMUM_BLOCK_WIDTH = 2.;

    enum TrackKind
    {
        NoTrack,
        EmbeddedTrack,
        ExternalPictureTrack,
        ExternalTextTrack,
    };
    Q_ENUM(TrackKind)

    explicit SubtitleTimeline(vlc_object_t* parent, QObject* qparent = nullptr);
    ~SubtitleTimeline() override;

    /**
     * Read the lines of a subtitle file, sorted by start. This blocks until
     * the whole file is read: call it from a background thread. Returns
     * nothing on failure or cancel.
     */
    static QVector<SubtitleCue> parse(vlc_object_t* parent, const QString& mrl,
                                      const std::atomic<bool>* cancelled = nullptr);

    /// whether subtitles of this codec are text, which the timeline covers
    static bool isTextCodec(vlc_fourcc_t codec);

    /**
     * The blocks of the lines shown in a strip of `width` pixels covering
     * [from, to) milliseconds of media, the lines moved by `delay`. Blocks
     * may start before the strip or end after it.
     */
    static QVector<SubtitleBlock> blocks(const QVector<SubtitleCue>& cues,
                                         qint64 from, qint64 to, qint64 delay, qreal width);

    bool enabled() const { return m_enabled; }
    QString unsupportedReason() const;
    QString note() const;
    bool loading() const { return m_loading; }
    bool shown() const { return m_shown; }

    qint64 span() const;
    bool canZoomIn() const;
    bool canZoomOut() const;
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();

    /**
     * The start of the strip while playing at `time`: the strip shows one
     * span at a time and turns a page when playback leaves it. Its last page
     * ends with the media (`length`, or none when not positive).
     */
    Q_INVOKABLE qint64 windowStart(qint64 time, qint64 length) const;

    /**
     * The subtitle delay after dragging a strip of `width` pixels by `dx`
     * pixels from `delay` (milliseconds): the lines follow the pointer.
     */
    Q_INVOKABLE qint64 draggedDelay(qint64 delay, qreal dx, qreal width) const;

    /// the lines of the current subtitle, empty until read
    const QVector<SubtitleCue>& cues() const { return m_cues; }

    void setEnabled(bool enabled);
    /// the current subtitle track, and the file of an external one
    void setTrack(TrackKind kind, const QString& mrl = {});

signals:
    void enabledChanged();
    void unsupportedReasonChanged();
    void loadingChanged();
    void shownChanged();
    void cuesChanged();
    void spanChanged();

private:
    struct Job;

    void update();
    void cancel();
    void setLoading(bool loading);
    void setCues(const QVector<SubtitleCue>& cues);
    void updateShown();
    void onJobDone(const std::shared_ptr<Job>& job);

    vlc_object_t* m_parent;
    QThreadPool m_pool;

    bool m_enabled = false;
    TrackKind m_kind = NoTrack;
    QString m_mrl;
    bool m_loading = false;
    bool m_shown = false;
    // the lines of m_mrl, once read
    QVector<SubtitleCue> m_cues;
    bool m_read = false;
    // the zoom: an index in the spans of the strip
    int m_zoom;

    std::shared_ptr<Job> m_job;
};

#endif // QVLC_SUBTITLE_TIMELINE_HPP
