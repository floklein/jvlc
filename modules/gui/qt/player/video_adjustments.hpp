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
#ifndef QVLC_VIDEO_ADJUSTMENTS_HPP
#define QVLC_VIDEO_ADJUSTMENTS_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <memory>
#include <vector>

/**
 * Where the Video tab applies its adjustments: the video outputs of the
 * player and the preferences
 */
class VideoOutputTarget
{
public:
    virtual ~VideoOutputTarget() = default;

    /// the "video-filter" chain of the video outputs
    virtual QString filterChain() const = 0;
    virtual void setFilterChain( const QString& chain ) = 0;
    /// a filter option ("hue") or a video output variable ("crop"); filters
    /// started later read it too
    virtual void setOption( const char* name, const QVariant& value ) = 0;

    /// a preference exists when its module is built
    virtual bool hasPreference( const char* name ) const = 0;
    virtual QVariant preference( const char* name ) const = 0;
    virtual void setPreference( const char* name, const QVariant& value ) = 0;
};

/**
 * The Video tab of the Adjustments sidebar: flip and rotation, the Essential,
 * Crop and Colors filter groups, HDR and the resolution of adaptive streams.
 *
 * Every change applies live. The filters it owns are added to or removed from
 * the outputs' filter chain, the others in the chain are kept. The values stay
 * until changed, also for the next video outputs (outputsChanged()).
 */
class VideoAdjustments : public QObject
{
    Q_OBJECT

public:
    enum Flip
    {
        NoFlip,
        FlipHorizontal,
        FlipVertical
    };
    Q_ENUM(Flip)

    enum Group
    {
        Essential,
        Crop,
        Colors
    };
    Q_ENUM(Group)

    /// a value of a filter group
    struct Parameter
    {
        const char* key;
        Group group;
        /// the filter it belongs to, nullptr for the crop borders
        const char* filter;
        /// its option, nullptr when it only switches its filter on
        const char* option;
        QVariant defaultValue;
        double from;
        double to;
        double step;
        /// the filter is on while this value differs from its default
        bool switches;
    };
    Q_PROPERTY(Flip flip READ flip WRITE setFlip NOTIFY flipChanged FINAL)
    // clockwise, in degrees, 0 to 360
    Q_PROPERTY(double rotation READ rotation WRITE setRotation NOTIFY rotationChanged FINAL)
    // the values of the filter groups by key ("hue", "cropTop", "negate"…)
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged FINAL)
    // a group differs from its defaults: it shows Reset
    Q_PROPERTY(bool essentialModified READ essentialModified NOTIFY valuesChanged FINAL)
    Q_PROPERTY(bool cropModified READ cropModified NOTIFY valuesChanged FINAL)
    Q_PROPERTY(bool colorsModified READ colorsModified NOTIFY valuesChanged FINAL)

    // the HDR output of Direct3D 11 (d3d11-hdr-mode: auto, never, always,
    // generate), read when a video output starts
    Q_PROPERTY(bool hdrAvailable READ hdrAvailable CONSTANT FINAL)
    Q_PROPERTY(QString hdrMode READ hdrMode WRITE setHdrMode NOTIFY hdrModeChanged FINAL)
    // the highest resolution of adaptive streams (adaptive-maxheight, 0 for
    // the best), read when a stream opens
    Q_PROPERTY(bool resolutionAvailable READ resolutionAvailable CONSTANT FINAL)
    Q_PROPERTY(int maxResolution READ maxResolution WRITE setMaxResolution NOTIFY maxResolutionChanged FINAL)

    explicit VideoAdjustments( std::unique_ptr<VideoOutputTarget> target,
                               QObject* parent = nullptr );
    ~VideoAdjustments() override;

    Flip flip() const { return m_flip; }
    void setFlip( Flip flip );

    double rotation() const { return m_rotation; }
    void setRotation( double degrees );

    QVariantMap values() const { return m_values; }
    /// sets a value of a filter group, kept in its range
    Q_INVOKABLE void setValue( const QString& key, const QVariant& value );
    /// the range of a value: from, to and step
    Q_INVOKABLE QVariantMap range( const QString& key ) const;

    bool essentialModified() const;
    bool cropModified() const;
    bool colorsModified() const;

    bool hdrAvailable() const;
    QString hdrMode() const;
    void setHdrMode( const QString& mode );
    bool resolutionAvailable() const;
    int maxResolution() const;
    void setMaxResolution( int height );

    /// puts the values of a group back to their defaults
    Q_INVOKABLE void reset( Group group );
    /// puts everything back: flip, rotation and the groups
    Q_INVOKABLE void resetAll();
    /// a crop ratio was picked: forget the borders without cropping
    Q_INVOKABLE void clearCropBorders();

    /// new video outputs started: apply the adjustments to them too
    void outputsChanged();

signals:
    void flipChanged();
    void rotationChanged();
    void valuesChanged();
    void hdrModeChanged();
    void maxResolutionChanged();

private:
    static const std::vector<Parameter>& parameters();
    static const Parameter* parameter( const QString& key );
    bool isFilterOn( const char* filter ) const;
    bool isModified( Group group ) const;
    /// the "crop" variable for the borders, empty for none
    QString cropBorders() const;
    void resetValues( Group group );
    /// sets the options of a group on the outputs
    void applyOptions( Group group );
    /// a multiple of 90 degrees: the transform filter alone turns it
    bool isQuarterTurn() const;
    void applyRotateOption();
    /// the chain entries of the tab, in order
    QStringList ownedEntries() const;
    /// sets the outputs' chain: the other filters, then ours
    void applyChain();

    std::unique_ptr<VideoOutputTarget> m_target;

    Flip m_flip = NoFlip;
    double m_rotation = 0.;
    QVariantMap m_values;
    /// something was changed: new outputs get the adjustments
    bool m_touched = false;
};

#endif // QVLC_VIDEO_ADJUSTMENTS_HPP
