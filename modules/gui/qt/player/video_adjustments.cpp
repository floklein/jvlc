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

#include "video_adjustments.hpp"

#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

/* the filters of the tab, in their order in the chain */
const char* const OWNED_FILTERS[] = {
    "transform",
    "rotate",
    "adjust",
    "sharpen",
    "gradfun",
    "grain",
    "extract",
    "colorthres",
    "invert",
    "posterize",
    "gradient",
    "sepia",
};

/* two borders kept equal while their link is on */
struct CropLink
{
    const char* link;
    const char* first;
    const char* second;
};

const CropLink CROP_LINKS[] = {
    { "cropLinkTopBottom", "cropTop", "cropBottom" },
    { "cropLinkLeftRight", "cropLeft", "cropRight" },
};

/* the preferences of the tab */
const char HDR_MODE[] = "d3d11-hdr-mode";
const char MAX_HEIGHT[] = "adaptive-maxheight";

/* gradfun-radius below this is refused, 0 means off */
constexpr int BANDING_RADIUS_MIN = 4;

/* a flip then quarter turns clockwise, as one transform type (nullptr: none),
 * by flip and quarter turns */
const char* const TRANSFORMS[3][4] = {
    { nullptr, "90", "180", "270" },
    { "hflip", "antitranspose", "vflip", "transpose" },
    { "vflip", "transpose", "hflip", "antitranspose" },
};

/* "wave:logo{file=a:b}:mirror" -> its entries, ':' inside braces or quotes
 * do not split */
QStringList splitChain( const QString& chain )
{
    QStringList entries;
    QString current;
    int depth = 0;
    bool quoted = false;
    for ( const QChar c : chain )
    {
        if ( c == '"' )
            quoted = !quoted;
        else if ( !quoted && c == '{' )
            ++depth;
        else if ( !quoted && c == '}' && depth > 0 )
            --depth;
        else if ( !quoted && depth == 0 && c == ':' )
        {
            if ( !current.isEmpty() )
                entries.append( current );
            current.clear();
            continue;
        }
        current.append( c );
    }
    if ( !current.isEmpty() )
        entries.append( current );
    return entries;
}

QString entryName( const QString& entry )
{
    const qsizetype brace = entry.indexOf( '{' );
    return ( brace < 0 ? entry : entry.left( brace ) ).trimmed();
}

bool isOwned( const QString& name )
{
    for ( const char* owned : OWNED_FILTERS )
        if ( name == QLatin1String( owned ) )
            return true;
    return false;
}

} // namespace

const std::vector<VideoAdjustments::Parameter>& VideoAdjustments::parameters()
{
    /* the defaults leave the filters off */
    static const std::vector<Parameter> list = {
        /* Essential */
        { "hue", Essential, "adjust", "hue", 0., -180., 180., 1., true },
        { "brightness", Essential, "adjust", "brightness", 1., 0., 2., 0.01, true },
        { "contrast", Essential, "adjust", "contrast", 1., 0., 2., 0.01, true },
        { "saturation", Essential, "adjust", "saturation", 1., 0., 3., 0.01, true },
        { "gamma", Essential, "adjust", "gamma", 1., 0.01, 10., 0.01, true },
        { "sharpenSigma", Essential, "sharpen", "sharpen-sigma", 0., 0., 2., 0.01, true },
        { "bandingRadius", Essential, "gradfun", "gradfun-radius", 0, 0., 32., 1., true },
        { "grainVariance", Essential, "grain", "grain-variance", 0., 0., 10., 0.1, true },
        /* Crop: the borders of the video output, in pixels */
        { "cropTop", Crop, nullptr, nullptr, 0, 0., 10000., 1., false },
        { "cropBottom", Crop, nullptr, nullptr, 0, 0., 10000., 1., false },
        { "cropLeft", Crop, nullptr, nullptr, 0, 0., 10000., 1., false },
        { "cropRight", Crop, nullptr, nullptr, 0, 0., 10000., 1., false },
        { "cropLinkTopBottom", Crop, nullptr, nullptr, false, 0., 1., 1., false },
        { "cropLinkLeftRight", Crop, nullptr, nullptr, false, 0., 1., 1., false },
        /* Colors */
        { "extract", Colors, "extract", nullptr, false, 0., 1., 1., true },
        { "extractColor", Colors, "extract", "extract-component", 0xFF0000, 0., 0xFFFFFF, 1., false },
        { "colorThreshold", Colors, "colorthres", nullptr, false, 0., 1., 1., true },
        { "colorThresholdColor", Colors, "colorthres", "colorthres-color", 0xFF0000, 0., 0xFFFFFF, 1., false },
        { "colorThresholdSaturation", Colors, "colorthres", "colorthres-saturationthres", 20, 0., 255., 1., false },
        { "colorThresholdSimilarity", Colors, "colorthres", "colorthres-similaritythres", 15, 0., 255., 1., false },
        { "negate", Colors, "invert", nullptr, false, 0., 1., 1., true },
        { "posterize", Colors, "posterize", nullptr, false, 0., 1., 1., true },
        { "gradient", Colors, "gradient", nullptr, false, 0., 1., 1., true },
        { "gradientMode", Colors, "gradient", "gradient-mode", QStringLiteral( "gradient" ), 0., 0., 0., false },
        { "gradientColor", Colors, "gradient", "gradient-color", false, 0., 1., 1., false },
        { "gradientCartoon", Colors, "gradient", "gradient-cartoon", true, 0., 1., 1., false },
        { "sepiaIntensity", Colors, "sepia", "sepia-intensity", 0, 0., 255., 1., true },
    };
    return list;
}

const VideoAdjustments::Parameter* VideoAdjustments::parameter( const QString& key )
{
    for ( const Parameter& param : parameters() )
        if ( key == QLatin1String( param.key ) )
            return &param;
    return nullptr;
}

VideoAdjustments::VideoAdjustments( std::unique_ptr<VideoOutputTarget> target,
                                    QObject* parent )
    : QObject( parent )
    , m_target( std::move( target ) )
{
    for ( const Parameter& param : parameters() )
        m_values.insert( QLatin1String( param.key ), param.defaultValue );
}

VideoAdjustments::~VideoAdjustments() = default;

void VideoAdjustments::setFlip( Flip flip )
{
    if ( flip == m_flip )
        return;
    m_flip = flip;
    m_touched = true;
    applyRotateOption();
    applyChain();
    emit flipChanged();
}

void VideoAdjustments::setRotation( double degrees )
{
    degrees = std::fmod( degrees, 360. );
    if ( degrees < 0 )
        degrees += 360.;
    if ( qFuzzyCompare( degrees + 1., m_rotation + 1. ) )
        return;
    m_rotation = degrees;
    m_touched = true;
    applyRotateOption();
    applyChain();
    emit rotationChanged();
}

bool VideoAdjustments::isQuarterTurn() const
{
    return std::fmod( m_rotation, 90. ) == 0.;
}

void VideoAdjustments::applyRotateOption()
{
    if ( isQuarterTurn() )
        return;
    /* the filter turns counterclockwise; a flip applies after it, at display,
     * which reverses the turn */
    const double angle = m_flip == NoFlip ? std::fmod( 360. - m_rotation, 360. ) : m_rotation;
    m_target->setOption( "rotate-angle", angle );
}

void VideoAdjustments::setValue( const QString& key, const QVariant& value )
{
    const Parameter* param = parameter( key );
    if ( !param )
        return;

    QVariant typed;
    switch ( param->defaultValue.typeId() )
    {
    case QMetaType::Double:
        typed = std::clamp( value.toDouble(), param->from, param->to );
        break;
    case QMetaType::Int:
    {
        int number = std::clamp( static_cast<int>( std::lround( value.toDouble() ) ),
                                 static_cast<int>( param->from ), static_cast<int>( param->to ) );
        if ( qstrcmp( param->filter, "gradfun" ) == 0 && number > 0 )
            number = std::max( number, BANDING_RADIUS_MIN );
        typed = number;
        break;
    }
    case QMetaType::Bool:
        typed = value.toBool();
        break;
    default:
        typed = value.toString();
        break;
    }

    if ( m_values.value( key ) == typed )
        return;
    m_values.insert( key, typed );
    m_touched = true;

    if ( param->group == Crop )
    {
        /* a linked border follows its pair */
        for ( const auto& link : CROP_LINKS )
        {
            if ( !m_values.value( QLatin1String( link.link ) ).toBool() )
                continue;
            if ( key == QLatin1String( link.first ) || key == QLatin1String( link.link ) )
                m_values.insert( QLatin1String( link.second ), m_values.value( QLatin1String( link.first ) ) );
            else if ( key == QLatin1String( link.second ) )
                m_values.insert( QLatin1String( link.first ), typed );
        }
        m_target->setOption( "crop", cropBorders() );
    }
    else if ( param->option )
        m_target->setOption( param->option, typed );

    applyChain();
    emit valuesChanged();
}

QString VideoAdjustments::cropBorders() const
{
    const auto border = [this]( const char* key ) {
        return m_values.value( QLatin1String( key ) ).toInt();
    };
    const int left = border( "cropLeft" ), top = border( "cropTop" );
    const int right = border( "cropRight" ), bottom = border( "cropBottom" );
    if ( left == 0 && top == 0 && right == 0 && bottom == 0 )
        return {};
    /* the "crop" variable of the video output, left+top+right+bottom */
    return QStringLiteral( "%1+%2+%3+%4" ).arg( left ).arg( top ).arg( right ).arg( bottom );
}
QVariantMap VideoAdjustments::range( const QString& key ) const
{
    const Parameter* param = parameter( key );
    if ( !param )
        return {};
    return {
        { QStringLiteral( "from" ), param->from },
        { QStringLiteral( "to" ), param->to },
        { QStringLiteral( "step" ), param->step },
    };
}

bool VideoAdjustments::isFilterOn( const char* filter ) const
{
    for ( const Parameter& param : parameters() )
        if ( param.switches && qstrcmp( param.filter, filter ) == 0
          && m_values.value( QLatin1String( param.key ) ) != param.defaultValue )
            return true;
    return false;
}

bool VideoAdjustments::isModified( Group group ) const
{
    for ( const Parameter& param : parameters() )
        if ( param.group == group
          && m_values.value( QLatin1String( param.key ) ) != param.defaultValue )
            return true;
    return false;
}

bool VideoAdjustments::essentialModified() const
{
    return isModified( Essential );
}

bool VideoAdjustments::cropModified() const
{
    return isModified( Crop );
}

bool VideoAdjustments::colorsModified() const
{
    return isModified( Colors );
}

void VideoAdjustments::resetValues( Group group )
{
    for ( const Parameter& param : parameters() )
        if ( param.group == group )
            m_values.insert( QLatin1String( param.key ), param.defaultValue );
}

void VideoAdjustments::applyOptions( Group group )
{
    if ( group == Crop )
    {
        m_target->setOption( "crop", cropBorders() );
        return;
    }
    for ( const Parameter& param : parameters() )
        if ( param.group == group && param.option )
            m_target->setOption( param.option, m_values.value( QLatin1String( param.key ) ) );
}

void VideoAdjustments::reset( Group group )
{
    if ( !isModified( group ) )
        return;
    resetValues( group );
    applyOptions( group );
    applyChain();
    emit valuesChanged();
}

void VideoAdjustments::resetAll()
{
    setFlip( NoFlip );
    setRotation( 0. );
    for ( Group group : { Essential, Crop, Colors } )
        reset( group );
}

void VideoAdjustments::clearCropBorders()
{
    if ( !isModified( Crop ) )
        return;
    resetValues( Crop );
    emit valuesChanged();
}

bool VideoAdjustments::hdrAvailable() const
{
    return m_target->hasPreference( HDR_MODE );
}

QString VideoAdjustments::hdrMode() const
{
    return m_target->preference( HDR_MODE ).toString();
}

void VideoAdjustments::setHdrMode( const QString& mode )
{
    if ( !hdrAvailable() || mode == hdrMode() )
        return;
    m_target->setPreference( HDR_MODE, mode );
    emit hdrModeChanged();
}

bool VideoAdjustments::resolutionAvailable() const
{
    return m_target->hasPreference( MAX_HEIGHT );
}

int VideoAdjustments::maxResolution() const
{
    return m_target->preference( MAX_HEIGHT ).toInt();
}

void VideoAdjustments::setMaxResolution( int height )
{
    if ( !resolutionAvailable() || height == maxResolution() )
        return;
    m_target->setPreference( MAX_HEIGHT, height );
    emit maxResolutionChanged();
}

void VideoAdjustments::outputsChanged(){
    /* never touched: the outputs keep their own filters */
    if ( !m_touched )
        return;
    applyRotateOption();
    applyOptions( Essential );
    applyOptions( Colors );
    if ( !cropBorders().isEmpty() )
        applyOptions( Crop );
    applyChain();
}

QStringList VideoAdjustments::ownedEntries() const
{
    QStringList entries;
    const int turns = isQuarterTurn() ? static_cast<int>( m_rotation / 90. ) % 4 : 0;
    if ( const char* type = TRANSFORMS[m_flip][turns] )
        entries.append( QStringLiteral( "transform{type=%1}" ).arg( QLatin1String( type ) ) );
    if ( !isQuarterTurn() )
        entries.append( QStringLiteral( "rotate" ) );
    /* the filters of the groups */
    for ( const char* filter : OWNED_FILTERS )
        if ( isFilterOn( filter ) )
            entries.append( QLatin1String( filter ) );
    return entries;
}

void VideoAdjustments::applyChain()
{
    const QString current = m_target->filterChain();

    QStringList entries;
    for ( const QString& entry : splitChain( current ) )
        if ( !isOwned( entryName( entry ) ) )
            entries.append( entry );
    entries.append( ownedEntries() );

    const QString chain = entries.join( ':' );
    if ( chain != current )
        m_target->setFilterChain( chain );
}
