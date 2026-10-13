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

#include "../style/vlcicons.hpp"

#include <QGuiApplication>
#include <QTest>
#include <QFontDatabase>
#include <QRawFont>
#include <QMetaProperty>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QtPlugin>
#include <qconfig.h>

#ifdef QT_STATIC
// static Qt (Windows contribs): the test renders glyphs offscreen
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
#endif

/*
 * The icon font is generated offline (pixmaps/makeIconFont.py) and checked
 * in with its C++ index. These tests check that what is checked in holds
 * together: each key of the index draws a glyph of the font that VLCIcons
 * registers, and the layers of the stacked (two-tone) icons pair up.
 */
class TestVLCIcons : public QObject
{
    Q_OBJECT

private:
    static constexpr int pixelSize = 96;

    QRawFont m_font;
    QMap<QString, QChar> m_icons;

    QImage renderGlyph(QChar c) const
    {
        QImage image(pixelSize, pixelSize, QImage::Format_Alpha8);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        const QList<quint32> glyphs = m_font.glyphIndexesForString(QString(c));
        QPainterPath path = m_font.pathForGlyph(glyphs.first());
        // glyphs sit on the baseline and fill the ascent
        path.translate(0, m_font.ascent());
        painter.fillPath(path, Qt::black);
        return image;
    }

private slots:
    void initTestCase()
    {
        Q_INIT_RESOURCE( test_vlcicons );

        // registers the font from the resources, as the interface does
        VLCIcons icons;

        QVERIFY(QFontDatabase::families().contains(VLCIcons::fontFamily));

        QFont font(VLCIcons::fontFamily);
        font.setPixelSize(pixelSize);
        font.setHintingPreference(QFont::PreferNoHinting);
        m_font = QRawFont::fromFont(font);
        QVERIFY(m_font.isValid());
        QCOMPARE(m_font.familyName(), VLCIcons::fontFamily);

        const QMetaObject& meta = VLCIcons::staticMetaObject;
        for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i)
        {
            const QMetaProperty property = meta.property(i);
            if (property.metaType() != QMetaType::fromType<QChar>())
                continue;
            m_icons.insert(QString::fromLatin1(property.name()), property.read(&icons).value<QChar>());
        }
        QVERIFY(!m_icons.isEmpty());
    }

    void testEveryIconDrawsAGlyph()
    {
        for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it)
        {
            QVERIFY2(m_font.supportsCharacter(it.value()), qPrintable(it.key()));
            const QList<quint32> glyphs = m_font.glyphIndexesForString(QString(it.value()));
            QCOMPARE(glyphs.size(), 1);
            QVERIFY2(!m_font.pathForGlyph(glyphs.first()).isEmpty(), qPrintable(it.key()));
        }
    }

    void testIconsHaveTheirOwnGlyph()
    {
        QSet<QChar> seen;
        for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it)
            seen.insert(it.value());
        QCOMPARE(seen.size(), m_icons.size());
    }

    void testIconsShareTheEmSquare()
    {
        // every glyph is laid out in the same square, so that the layers of
        // a stacked icon drawn at the same pixel size overlay exactly
        QCOMPARE(m_font.ascent() + m_font.descent(), qreal(pixelSize));
        const QRectF square(0, -m_font.ascent(), pixelSize, pixelSize);
        for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it)
        {
            const QList<quint32> glyphs = m_font.glyphIndexesForString(QString(it.value()));
            QCOMPARE(m_font.advancesForGlyphIndexes(glyphs).first().x(), qreal(pixelSize));

            // the layers come from the 24x24 frames of the Figma icon set,
            // where the "active" dot badges overhang the frame by 2 px
            if (!it.key().endsWith(QLatin1String("_base")) && !it.key().endsWith(QLatin1String("_accent")))
                continue;
            const qreal overhang = 2.5 * pixelSize / 24;
            const QRectF bounds = m_font.pathForGlyph(glyphs.first()).boundingRect();
            QVERIFY2(square.adjusted(-overhang, -overhang, overhang, overhang).contains(bounds),
                     qPrintable(it.key()));
        }
    }

    void testRedesignIcons_data()
    {
        QTest::addColumn<QString>("key");
        // a few icons of each kind of the Figma set: monochrome, states,
        // directions, values, two-tone layers and accent-only two-tone
        for (const char* key : { "settings", "settings_active", "folder_selected",
                                 "arrow_left", "volume_none", "eq_rock_base", "eq_rock_accent",
                                 "folder_tt_base", "folder_tt_accent", "playback_tt_accent",
                                 "queue", "queue_tt_base", "queue_tt_accent", "record_active" })
            QTest::newRow(key) << QString::fromLatin1(key);
    }

    void testRedesignIcons()
    {
        QFETCH(QString, key);
        QVERIFY2(m_icons.contains(key), qPrintable(key));
    }

    void testStackedLayersComeInPairs()
    {
        int pairs = 0;
        for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it)
        {
            if (!it.key().endsWith(QLatin1String("_base")))
                continue;
            const QString accent = it.key().chopped(5) + QLatin1String("_accent");
            QVERIFY2(m_icons.contains(accent), qPrintable(accent));
            ++pairs;
        }
        QVERIFY(pairs > 0);
    }

    void testStackedLayersDoNotOverlap()
    {
        // the base layer is knocked out where the accent layer is drawn,
        // otherwise the colors blend at the edges of the accent
        for (auto it = m_icons.cbegin(); it != m_icons.cend(); ++it)
        {
            if (!it.key().endsWith(QLatin1String("_base")))
                continue;
            const QString accentKey = it.key().chopped(5) + QLatin1String("_accent");
            const QImage base = renderGlyph(it.value());
            const QImage accent = renderGlyph(m_icons.value(accentKey));

            int overlap = 0;
            for (int y = 0; y < pixelSize; ++y)
            {
                const uchar* b = base.constScanLine(y);
                const uchar* a = accent.constScanLine(y);
                for (int x = 0; x < pixelSize; ++x)
                    if (b[x] > 192 && a[x] > 192)
                        ++overlap;
            }
            QVERIFY2(overlap == 0, qPrintable(QStringLiteral("%1 overlaps %2 on %3 px")
                                              .arg(it.key(), accentKey).arg(overlap)));
        }
    }
};

int main(int argc, char** argv)
{
    // render without a display, the CI has none
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestVLCIcons test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "test_vlcicons.moc"
