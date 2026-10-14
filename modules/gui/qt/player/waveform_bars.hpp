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
#ifndef QVLC_WAVEFORM_BARS_HPP
#define QVLC_WAVEFORM_BARS_HPP

#include <QQuickPaintedItem>
#include <QColor>
#include <QVector>

/**
 * Draws loudness buckets as the waveform seek bar's bars (Figma 4778:26191):
 * thin rounded bars centered vertically, one per `barWidth + barSpacing`.
 */
class WaveformBars : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(QVector<float> peaks READ peaks WRITE setPeaks NOTIFY peaksChanged FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(qreal barWidth READ barWidth WRITE setBarWidth NOTIFY barWidthChanged FINAL)
    Q_PROPERTY(qreal barSpacing READ barSpacing WRITE setBarSpacing NOTIFY barSpacingChanged FINAL)
    // the height of a silent bar and of the loudest one
    Q_PROPERTY(qreal minimumBarHeight READ minimumBarHeight WRITE setMinimumBarHeight NOTIFY minimumBarHeightChanged FINAL)
    Q_PROPERTY(qreal maximumBarHeight READ maximumBarHeight WRITE setMaximumBarHeight NOTIFY maximumBarHeightChanged FINAL)

public:
    explicit WaveformBars(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    QVector<float> peaks() const { return m_peaks; }
    QColor color() const { return m_color; }
    qreal barWidth() const { return m_barWidth; }
    qreal barSpacing() const { return m_barSpacing; }
    qreal minimumBarHeight() const { return m_minimumBarHeight; }
    qreal maximumBarHeight() const { return m_maximumBarHeight; }

    void setPeaks(const QVector<float>& peaks);
    void setColor(const QColor& color);
    void setBarWidth(qreal width);
    void setBarSpacing(qreal spacing);
    void setMinimumBarHeight(qreal height);
    void setMaximumBarHeight(qreal height);

signals:
    void peaksChanged();
    void colorChanged();
    void barWidthChanged();
    void barSpacingChanged();
    void minimumBarHeightChanged();
    void maximumBarHeightChanged();

private:
    QVector<float> m_peaks;
    QColor m_color;
    qreal m_barWidth = 2.;
    qreal m_barSpacing = 3.;
    qreal m_minimumBarHeight = 2.;
    qreal m_maximumBarHeight = 14.;
};

#endif // QVLC_WAVEFORM_BARS_HPP
