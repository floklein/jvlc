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
#ifndef SUBTITLE_APPEARANCE_HPP
#define SUBTITLE_APPEARANCE_HPP

#include <QObject>
#include <QColor>
#include <QString>
#include <QVariant>

#include <memory>

/**
 * The Defaults and Appearance of the Subtitles tab of the Adjustments
 * sidebar: the default subtitle language and the look of text subtitles.
 *
 * Each value is an option of the Profile. A change is written at once and,
 * for the look, the line on screen is drawn again with it. The options are
 * reached through a Backend, so that this class needs no VLC instance.
 */
class SubtitleAppearance : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString defaultLanguage READ defaultLanguage WRITE setDefaultLanguage NOTIFY defaultLanguageChanged FINAL)
    // the subtitle track the next media shows, from 1; 0 lets it choose ("sub-track")
    Q_PROPERTY(int defaultTrack READ defaultTrack WRITE setDefaultTrack NOTIFY defaultTrackChanged FINAL)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT FINAL)

    Q_PROPERTY(bool keepFileStyling READ keepFileStyling WRITE setKeepFileStyling NOTIFY keepFileStylingChanged FINAL)
    Q_PROPERTY(QString font READ font WRITE setFont NOTIFY fontChanged FINAL)
    Q_PROPERTY(SizeMode sizeMode READ sizeMode WRITE setSizeMode NOTIFY sizeModeChanged FINAL)
    Q_PROPERTY(RelativeSize relativeSize READ relativeSize WRITE setRelativeSize NOTIFY relativeSizeChanged FINAL)
    Q_PROPERTY(int pixelSize READ pixelSize WRITE setPixelSize NOTIFY pixelSizeChanged FINAL)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged FINAL)
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY outlineColorChanged FINAL)
    Q_PROPERTY(OutlineThickness outlineThickness READ outlineThickness WRITE setOutlineThickness NOTIFY outlineThicknessChanged FINAL)
    Q_PROPERTY(int backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity NOTIFY backgroundOpacityChanged FINAL)
    Q_PROPERTY(int verticalOffset READ verticalOffset WRITE setVerticalOffset NOTIFY verticalOffsetChanged FINAL)

public:
    enum SizeMode { Relative, Pixels };
    Q_ENUM(SizeMode)

    // the relative sizes of VLC 3 ("Smaller" to "Larger")
    enum RelativeSize { Smaller, Small, Normal, Large, Larger };
    Q_ENUM(RelativeSize)

    // the steps of "freetype-outline-thickness"
    enum OutlineThickness { NoOutline, ThinOutline, NormalOutline, ThickOutline };
    Q_ENUM(OutlineThickness)

    // the pixel size used before any was chosen
    static constexpr int DEFAULT_PIXEL_SIZE = 40;

    /**
     * Where the options live: the core configuration and the player
     */
    class Backend
    {
    public:
        virtual ~Backend() = default;
        // the current value of an option, invalid if there is no such option
        virtual QVariant value(const char* option) const = 0;
        // apply and save an option
        virtual void setValue(const char* option, const QVariant& value) = 0;
        // apply an option to the player only, the Profile keeps its value
        virtual void apply(const char* option, const QVariant& value) = 0;
        // draw the subtitles on screen again with the current options
        virtual void redraw() = 0;
        // the Profile's options changed: apply them again
        virtual void reloadProfile() = 0;
    };

    explicit SubtitleAppearance(std::unique_ptr<Backend> backend, QObject* parent = nullptr);
    ~SubtitleAppearance() override;

    // another Profile is active: apply and show its options
    void reload();

    // whether changes are saved in the Profile; while a Preset is active
    // they are only shown (#56)
    void setProfileWrites(bool enabled) { m_profileWrites = enabled; }

    // "" lets the media choose, "none" shows no subtitle, else a language code
    QString defaultLanguage() const { return m_defaultLanguage; }
    void setDefaultLanguage(const QString& language);
    // the choices of the default language: { code, name }, Automatic and
    // None first, then the languages by name
    QVariantList languages() const;

    // the number of the subtitle track the next media shows, from 1 (Figma
    // "Subtitle Track", 845:1598); 0 lets the media choose
    int defaultTrack() const { return m_defaultTrack; }
    void setDefaultTrack(int track);

    bool keepFileStyling() const { return m_keepFileStyling; }
    void setKeepFileStyling(bool keep);

    // a font family, empty for the default one
    QString font() const { return m_font; }
    void setFont(const QString& font);

    SizeMode sizeMode() const { return m_sizeMode; }
    void setSizeMode(SizeMode mode);

    // picking a relative size leaves the pixels mode
    RelativeSize relativeSize() const { return m_relativeSize; }
    void setRelativeSize(RelativeSize size);

    // kept while the size is relative, for the next switch to pixels
    int pixelSize() const { return m_pixelSize; }
    void setPixelSize(int pixels);

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor& color);

    QColor outlineColor() const { return m_outlineColor; }
    void setOutlineColor(const QColor& color);

    OutlineThickness outlineThickness() const { return m_outlineThickness; }
    void setOutlineThickness(OutlineThickness thickness);

    // 0 to 100 %
    int backgroundOpacity() const { return m_backgroundOpacity; }
    void setBackgroundOpacity(int percent);

    // pixels above the bottom of the video
    int verticalOffset() const { return m_verticalOffset; }
    void setVerticalOffset(int pixels);

signals:
    void defaultLanguageChanged();
    void defaultTrackChanged();
    void keepFileStylingChanged();
    void fontChanged();
    void sizeModeChanged();
    void relativeSizeChanged();
    void pixelSizeChanged();
    void textColorChanged();
    void outlineColorChanged();
    void outlineThicknessChanged();
    void backgroundOpacityChanged();
    void verticalOffsetChanged();
    // a value was changed through a setter, not by a reload
    void adjusted();

private:
    void read();
    // save and apply, or only apply without Profile writes
    void write(const char* option, const QVariant& value);
    // write an option of the look, then show it on the line on screen
    void applyLook(const char* option, const QVariant& value);

    std::unique_ptr<Backend> m_backend;
    bool m_profileWrites = true;

    QString m_defaultLanguage;
    int m_defaultTrack = 0;
    bool m_keepFileStyling = true;
    QString m_font;
    SizeMode m_sizeMode = Relative;
    RelativeSize m_relativeSize = Normal;
    int m_pixelSize = DEFAULT_PIXEL_SIZE;
    QColor m_textColor = Qt::white;
    QColor m_outlineColor = Qt::black;
    OutlineThickness m_outlineThickness = NormalOutline;
    int m_backgroundOpacity = 0;
    int m_verticalOffset = 0;
};

#endif // SUBTITLE_APPEARANCE_HPP
