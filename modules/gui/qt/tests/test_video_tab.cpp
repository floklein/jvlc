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

#include <QTest>
#include <QSignalSpy>

#include "../player/video_adjustments.hpp"

// The video outputs as the Video tab sees them: a filter chain, the options
// set on the outputs, and the preferences
class FakeVideoOutputs : public VideoOutputTarget
{
public:
    QString chain;
    QVariantMap options;
    QVariantMap preferences;
    int chainChanges = 0;

    QString filterChain() const override { return chain; }
    void setFilterChain( const QString& value ) override
    {
        chain = value;
        ++chainChanges;
    }
    void setOption( const char* name, const QVariant& value ) override
    {
        options.insert( QString::fromUtf8( name ), value );
    }
    bool hasPreference( const char* name ) const override
    {
        return preferences.contains( QString::fromUtf8( name ) );
    }
    QVariant preference( const char* name ) const override
    {
        return preferences.value( QString::fromUtf8( name ) );
    }
    void setPreference( const char* name, const QVariant& value ) override
    {
        preferences.insert( QString::fromUtf8( name ), value );
    }
};

// The Video tab of the Adjustments sidebar (spec #30, ticket #52): flip and
// rotation, the Essential, Crop and Colors filter groups, HDR and resolution
class TestVideoTab : public QObject
{
    Q_OBJECT

    FakeVideoOutputs* m_outputs = nullptr;
    std::unique_ptr<VideoAdjustments> m_video;

private slots:
    void init()
    {
        auto outputs = std::make_unique<FakeVideoOutputs>();
        m_outputs = outputs.get();
        m_outputs->preferences.insert( "d3d11-hdr-mode", "auto" );
        m_outputs->preferences.insert( "adaptive-maxheight", 0 );
        m_video = std::make_unique<VideoAdjustments>( std::move( outputs ) );
    }

    void cleanup()
    {
        m_video.reset();
        m_outputs = nullptr;
    }

    void flipAddsATransformAndKeepsOtherFilters()
    {
        m_outputs->chain = "wave:mirror";

        m_video->setFlip( VideoAdjustments::FlipHorizontal );

        QCOMPARE( m_video->flip(), VideoAdjustments::FlipHorizontal );
        QCOMPARE( m_outputs->chain, QStringLiteral( "wave:mirror:transform{type=hflip}" ) );

        m_video->setFlip( VideoAdjustments::NoFlip );
        QCOMPARE( m_outputs->chain, QStringLiteral( "wave:mirror" ) );
    }

    void quarterTurnsRotateTheTransform_data()
    {
        QTest::addColumn<int>( "flip" );
        QTest::addColumn<double>( "rotation" );
        QTest::addColumn<QString>( "chain" );

        QTest::newRow( "90" ) << int( VideoAdjustments::NoFlip ) << 90. << "transform{type=90}";
        QTest::newRow( "180" ) << int( VideoAdjustments::NoFlip ) << 180. << "transform{type=180}";
        QTest::newRow( "270" ) << int( VideoAdjustments::NoFlip ) << 270. << "transform{type=270}";
        QTest::newRow( "-90 is 270" ) << int( VideoAdjustments::NoFlip ) << -90. << "transform{type=270}";
        QTest::newRow( "360 is none" ) << int( VideoAdjustments::NoFlip ) << 360. << "";
        // flipped, then turned clockwise
        QTest::newRow( "hflip 90" ) << int( VideoAdjustments::FlipHorizontal ) << 90. << "transform{type=antitranspose}";
        QTest::newRow( "hflip 180" ) << int( VideoAdjustments::FlipHorizontal ) << 180. << "transform{type=vflip}";
        QTest::newRow( "hflip 270" ) << int( VideoAdjustments::FlipHorizontal ) << 270. << "transform{type=transpose}";
        QTest::newRow( "vflip 90" ) << int( VideoAdjustments::FlipVertical ) << 90. << "transform{type=transpose}";
        QTest::newRow( "vflip 180" ) << int( VideoAdjustments::FlipVertical ) << 180. << "transform{type=hflip}";
    }

    void quarterTurnsRotateTheTransform()
    {
        QFETCH( int, flip );
        QFETCH( double, rotation );
        QFETCH( QString, chain );

        m_video->setFlip( static_cast<VideoAdjustments::Flip>( flip ) );
        m_video->setRotation( rotation );

        QCOMPARE( m_outputs->chain, chain );
    }

    void aFreeAngleUsesTheRotateFilter()
    {
        QSignalSpy changed( m_video.get(), &VideoAdjustments::rotationChanged );

        m_video->setRotation( 30 );

        QCOMPARE( m_video->rotation(), 30. );
        QCOMPARE( changed.count(), 1 );
        QCOMPARE( m_outputs->chain, QStringLiteral( "rotate" ) );
        // the filter turns counterclockwise
        QCOMPARE( m_outputs->options.value( "rotate-angle" ).toDouble(), 330. );
    }

    void aFlippedFreeAngleTurnsTheOtherWay()
    {
        m_video->setFlip( VideoAdjustments::FlipHorizontal );
        m_video->setRotation( 30 );

        // the flip applies last, at display
        QCOMPARE( m_outputs->chain, QStringLiteral( "transform{type=hflip}:rotate" ) );
        QCOMPARE( m_outputs->options.value( "rotate-angle" ).toDouble(), 30. );
    }

    void essentialValuesStartAtTheirDefaults()
    {
        const QVariantMap values = m_video->values();
        QCOMPARE( values.value( "hue" ).toDouble(), 0. );
        QCOMPARE( values.value( "brightness" ).toDouble(), 1. );
        QCOMPARE( values.value( "gamma" ).toDouble(), 1. );
        QCOMPARE( values.value( "sharpenSigma" ).toDouble(), 0. );
        QCOMPARE( m_video->essentialModified(), false );
        QCOMPARE( m_outputs->chain, QString() );
    }

    void movingASliderAppliesItsFilterLive()
    {
        QSignalSpy changed( m_video.get(), &VideoAdjustments::valuesChanged );

        m_video->setValue( "hue", 40 );

        QCOMPARE( m_video->values().value( "hue" ).toDouble(), 40. );
        QCOMPARE( changed.count(), 1 );
        QCOMPARE( m_video->essentialModified(), true );
        QCOMPARE( m_outputs->chain, QStringLiteral( "adjust" ) );
        // the filter reads a float
        QCOMPARE( m_outputs->options.value( "hue" ).typeId(), QMetaType::Double );
        QCOMPARE( m_outputs->options.value( "hue" ).toDouble(), 40. );

        // a running filter follows its option: the chain stays
        m_video->setValue( "hue", 50 );
        QCOMPARE( m_outputs->options.value( "hue" ).toDouble(), 50. );
        QCOMPARE( m_outputs->chainChanges, 1 );
    }

    void filtersFollowTheTabOrder()
    {
        m_video->setValue( "grainVariance", 2 );
        m_video->setValue( "contrast", 1.5 );
        m_video->setRotation( 90 );

        QCOMPARE( m_outputs->chain, QStringLiteral( "transform{type=90}:adjust:grain" ) );
    }

    void backToTheDefaultRemovesTheFilter()
    {
        m_video->setValue( "sharpenSigma", 0.5 );
        QCOMPARE( m_outputs->chain, QStringLiteral( "sharpen" ) );

        m_video->setValue( "sharpenSigma", 0 );
        QCOMPARE( m_outputs->chain, QString() );
        QCOMPARE( m_video->essentialModified(), false );
    }

    void valuesStayInTheirRange()
    {
        const QVariantMap range = m_video->range( "saturation" );
        QCOMPARE( range.value( "from" ).toDouble(), 0. );
        QCOMPARE( range.value( "to" ).toDouble(), 3. );

        m_video->setValue( "saturation", 7 );
        QCOMPARE( m_video->values().value( "saturation" ).toDouble(), 3. );

        // the banding filter needs a radius of 4 at least, 0 is off
        m_video->setValue( "bandingRadius", 2 );
        QCOMPARE( m_video->values().value( "bandingRadius" ).toInt(), 4 );
        QCOMPARE( m_outputs->options.value( "gradfun-radius" ).typeId(), QMetaType::Int );
        QCOMPARE( m_outputs->chain, QStringLiteral( "adjust:gradfun" ) );
    }

    void colorFiltersSwitchOnWithTheirToggle()
    {
        m_video->setValue( "colorThresholdColor", 0x00FF00 );
        // an option alone does not start the filter
        QCOMPARE( m_outputs->chain, QString() );
        QCOMPARE( m_outputs->options.value( "colorthres-color" ).toInt(), 0x00FF00 );

        m_video->setValue( "colorThreshold", true );
        m_video->setValue( "negate", true );
        m_video->setValue( "gradientMode", "edge" );
        m_video->setValue( "gradient", true );
        m_video->setValue( "sepiaIntensity", 100 );

        QCOMPARE( m_outputs->chain, QStringLiteral( "colorthres:invert:gradient:sepia" ) );
        QCOMPARE( m_outputs->options.value( "gradient-mode" ).toString(), QStringLiteral( "edge" ) );
        QCOMPARE( m_outputs->options.value( "sepia-intensity" ).toInt(), 100 );
        QCOMPARE( m_video->colorsModified(), true );
        QCOMPARE( m_video->essentialModified(), false );
    }

    void cropBordersCropTheOutputs()
    {
        m_video->setValue( "cropTop", 10 );
        m_video->setValue( "cropLeft", 4 );

        // left+top+right+bottom
        QCOMPARE( m_outputs->options.value( "crop" ).toString(), QStringLiteral( "4+10+0+0" ) );
        QCOMPARE( m_outputs->chain, QString() );
        QCOMPARE( m_video->cropModified(), true );

        m_video->setValue( "cropTop", 0 );
        m_video->setValue( "cropLeft", 0 );
        QCOMPARE( m_outputs->options.value( "crop" ).toString(), QString() );
    }

    void linkedBordersStayEqual()
    {
        m_video->setValue( "cropTop", 12 );
        m_video->setValue( "cropLinkTopBottom", true );
        QCOMPARE( m_video->values().value( "cropBottom" ).toInt(), 12 );

        m_video->setValue( "cropBottom", 20 );
        QCOMPARE( m_video->values().value( "cropTop" ).toInt(), 20 );
        QCOMPARE( m_outputs->options.value( "crop" ).toString(), QStringLiteral( "0+20+0+20" ) );

        // the other pair is free
        m_video->setValue( "cropLeft", 3 );
        QCOMPARE( m_video->values().value( "cropRight" ).toInt(), 0 );
    }

    void aCropRatioReplacesTheBorders()
    {
        m_video->setValue( "cropTop", 12 );
        m_outputs->options.remove( "crop" );

        m_video->clearCropBorders();

        QCOMPARE( m_video->values().value( "cropTop" ).toInt(), 0 );
        QCOMPARE( m_video->cropModified(), false );
        // the ratio picked stays
        QVERIFY( !m_outputs->options.contains( "crop" ) );
    }

    void resetPutsAGroupBack()
    {
        m_video->setValue( "hue", 40 );
        m_video->setValue( "sharpenSigma", 1 );
        m_video->setValue( "negate", true );
        m_video->setValue( "cropTop", 8 );

        m_video->reset( VideoAdjustments::Essential );

        QCOMPARE( m_video->essentialModified(), false );
        QCOMPARE( m_video->values().value( "hue" ).toDouble(), 0. );
        QCOMPARE( m_outputs->options.value( "hue" ).toDouble(), 0. );
        QCOMPARE( m_outputs->chain, QStringLiteral( "invert" ) );
        QCOMPARE( m_video->colorsModified(), true );

        m_video->reset( VideoAdjustments::Crop );
        QCOMPARE( m_outputs->options.value( "crop" ).toString(), QString() );
        QCOMPARE( m_video->cropModified(), false );
    }

    void resetAllPutsEverythingBack()
    {
        m_outputs->chain = "wave";
        m_video->setFlip( VideoAdjustments::FlipVertical );
        m_video->setRotation( 45 );
        m_video->setValue( "contrast", 2 );
        m_video->setValue( "posterize", true );

        m_video->resetAll();

        QCOMPARE( m_video->flip(), VideoAdjustments::NoFlip );
        QCOMPARE( m_video->rotation(), 0. );
        QCOMPARE( m_video->colorsModified(), false );
        QCOMPARE( m_outputs->chain, QStringLiteral( "wave" ) );
    }

    void newOutputsGetTheAdjustments()
    {
        m_video->setRotation( 30 );
        m_video->setValue( "hue", 40 );
        m_video->setValue( "cropTop", 6 );

        // a new video output starts with the chain of the preferences
        m_outputs->chain = "wave";
        m_outputs->options.clear();
        m_video->outputsChanged();

        QCOMPARE( m_outputs->chain, QStringLiteral( "wave:rotate:adjust" ) );
        QCOMPARE( m_outputs->options.value( "hue" ).toDouble(), 40. );
        QCOMPARE( m_outputs->options.value( "rotate-angle" ).toDouble(), 330. );
        QCOMPARE( m_outputs->options.value( "crop" ).toString(), QStringLiteral( "0+6+0+0" ) );
    }

    void untouchedAdjustmentsLeaveNewOutputsAlone()
    {
        m_outputs->chain = "wave";
        m_video->outputsChanged();

        QCOMPARE( m_outputs->chainChanges, 0 );
        QVERIFY( m_outputs->options.isEmpty() );
    }

    void hdrAndResolutionArePreferences()
    {
        QCOMPARE( m_video->hdrAvailable(), true );
        QCOMPARE( m_video->hdrMode(), QStringLiteral( "auto" ) );
        QCOMPARE( m_video->resolutionAvailable(), true );
        QCOMPARE( m_video->maxResolution(), 0 );

        QSignalSpy hdr( m_video.get(), &VideoAdjustments::hdrModeChanged );
        QSignalSpy resolution( m_video.get(), &VideoAdjustments::maxResolutionChanged );
        m_video->setHdrMode( "never" );
        m_video->setMaxResolution( 720 );

        QCOMPARE( m_outputs->preferences.value( "d3d11-hdr-mode" ).toString(), QStringLiteral( "never" ) );
        QCOMPARE( m_outputs->preferences.value( "adaptive-maxheight" ).toInt(), 720 );
        QCOMPARE( m_video->hdrMode(), QStringLiteral( "never" ) );
        QCOMPARE( m_video->maxResolution(), 720 );
        QCOMPARE( hdr.count(), 1 );
        QCOMPARE( resolution.count(), 1 );
        // not a filter: Reset all leaves them
        m_video->resetAll();
        QCOMPARE( m_video->maxResolution(), 720 );
    }

    // what a Preset holds (#56): flip, rotation and the values, not the
    // preferences
    void adjustmentsAreSignalledForPresets()
    {
        QSignalSpy adjusted( m_video.get(), &VideoAdjustments::adjusted );
        m_video->setFlip( VideoAdjustments::FlipVertical );
        m_video->setRotation( 30. );
        m_video->setValue( "hue", 90 );
        QCOMPARE( adjusted.count(), 3 );

        m_video->setHdrMode( "never" );
        m_video->setMaxResolution( 720 );
        QCOMPARE( adjusted.count(), 3 );
    }

    void noHdrWithoutDirect3D11()
    {
        m_outputs->preferences.remove( "d3d11-hdr-mode" );
        QCOMPARE( m_video->hdrAvailable(), false );
    }

    void unknownValuesAreIgnored()
    {
        QSignalSpy changed( m_video.get(), &VideoAdjustments::valuesChanged );
        m_video->setValue( "nope", 3 );
        QCOMPARE( changed.count(), 0 );
        QVERIFY( m_outputs->options.isEmpty() );
    }
};

QTEST_GUILESS_MAIN(TestVideoTab)
#include "test_video_tab.moc"
