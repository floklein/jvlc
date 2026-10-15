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
#ifndef QVLC_EQUALIZER_CURVE_HPP
#define QVLC_EQUALIZER_CURVE_HPP

#include <QQuickPaintedItem>
#include <QColor>
#include <QList>

/**
 * The curve icon of an equalizer preset (Figma 2372:8487): the area between
 * the 0 dB line and a smooth curve through the band gains, over the line.
 */
class EqualizerCurve : public QQuickPaintedItem
{
    Q_OBJECT

    // the gains of the bands in dB, low frequencies first
    Q_PROPERTY(QList<double> bands READ bands WRITE setBands NOTIFY bandsChanged FINAL)
    // the gain drawn at the top and bottom edges
    Q_PROPERTY(double range READ range WRITE setRange NOTIFY rangeChanged FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged FINAL)

public:
    explicit EqualizerCurve(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    QList<double> bands() const { return m_bands; }
    double range() const { return m_range; }
    QColor color() const { return m_color; }
    QColor lineColor() const { return m_lineColor; }

    void setBands(const QList<double>& bands);
    void setRange(double range);
    void setColor(const QColor& color);
    void setLineColor(const QColor& color);

signals:
    void bandsChanged();
    void rangeChanged();
    void colorChanged();
    void lineColorChanged();

private:
    QList<double> m_bands;
    double m_range = 12.;
    QColor m_color;
    QColor m_lineColor;
};

#endif // QVLC_EQUALIZER_CURVE_HPP
