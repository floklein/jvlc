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
#ifndef QVLC_AUDIO_ADJUSTMENTS_HPP
#define QVLC_AUDIO_ADJUSTMENTS_HPP

#include <QObject>
#include <QList>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

typedef struct vlc_player_t vlc_player_t;

/**
 * The Audio tab of the Adjustments sidebar: the default audio language, the
 * stereo mode and the audio filters (equalizer, compressor, spatializer,
 * stereo widener and pitch).
 *
 * Every change applies at once to the audio output of the player. Filters,
 * their values and the stereo mode persist in the configuration (they stay
 * until changed); the default language is a preference.
 *
 * Equalizer presets can be previewed: previewEqualizerPreset() applies one to the
 * output only, endEqualizerPreview() puts back what was there, applyEqualizerPreset() keeps it.
 */
class AudioAdjustments : public QObject
{
    Q_OBJECT

    // the equalizer presets, by display name
    Q_PROPERTY(QStringList equalizerPresets READ equalizerPresets CONSTANT FINAL)
    // the frequencies of the 10 bands, "60 Hz"…
    Q_PROPERTY(QStringList bandNames READ bandNames CONSTANT FINAL)

    Q_PROPERTY(bool equalizerEnabled READ isEqualizerEnabled NOTIFY equalizerChanged FINAL)
    // index in equalizerPresets of the preset matching the bands, -1 for custom bands
    Q_PROPERTY(int equalizerPreset READ equalizerPreset NOTIFY equalizerChanged FINAL)
    Q_PROPERTY(double preamp READ preamp NOTIFY equalizerChanged FINAL)
    Q_PROPERTY(QList<double> bands READ bands NOTIFY equalizerChanged FINAL)
    Q_PROPERTY(bool twoPass READ twoPass NOTIFY equalizerChanged FINAL)
    // a preset is shown on the output, not saved
    Q_PROPERTY(bool previewing READ isPreviewing NOTIFY equalizerChanged FINAL)

    // the enabled filters among compressor, spatializer, stereo_widen and
    // scaletempo_pitch
    Q_PROPERTY(QStringList enabledFilters READ enabledFilters NOTIFY filtersChanged FINAL)
    // the value of each filter parameter, by option name
    Q_PROPERTY(QVariantMap values READ values NOTIFY filtersChanged FINAL)

    // the groups that differ from their defaults: "equalizer", "compressor",
    // "spatializer", "stereo_widen", "pitch", and "stereo" once a stereo mode
    // was chosen
    Q_PROPERTY(QStringList modifiedGroups READ modifiedGroups NOTIFY modifiedGroupsChanged FINAL)

    // the stereo modes: { value, name }, "Automatic" (the output's choice) first
    Q_PROPERTY(QVariantList stereoModes READ stereoModes CONSTANT FINAL)
    // the chosen (or tried) stereo mode, AOUT_VAR_CHAN_UNSET for automatic
    Q_PROPERTY(int stereoMode READ stereoMode NOTIFY stereoModeChanged FINAL)

    // the default audio language (ISO 639 code), empty for the file's choice
    Q_PROPERTY(QString language READ language NOTIFY languageChanged FINAL)
    // the choices of the default language: { code, name }, "Auto" first
    Q_PROPERTY(QVariantList languages READ languages NOTIFY languageChanged FINAL)

public:
    static constexpr int BAND_COUNT = 10;
    // the stereo mode "Headphones": the binaural mix of the output
    // ("mix-mode"), with the stereo mode automatic; the value the deprecated
    // AOUT_VAR_CHAN_HEADPHONES had
    static constexpr int HEADPHONES = 6;

    explicit AudioAdjustments(vlc_player_t* player, QObject* parent = nullptr);

    QStringList equalizerPresets() const;
    QStringList bandNames() const;

    bool isEqualizerEnabled() const;
    int equalizerPreset() const;
    double preamp() const;
    QList<double> bands() const;
    bool twoPass() const;
    bool isPreviewing() const;

    QStringList enabledFilters() const;
    QVariantMap values() const;
    QStringList modifiedGroups() const;

    QVariantList stereoModes() const;
    int stereoMode() const;

    QString language() const;
    QVariantList languages() const;

    /// whether changes are kept in the configuration; while a Preset is
    /// active they are only applied to the output (#56)
    void setProfileWrites(bool enabled) { m_profileWrites = enabled; }

    /// shows an equalizer state as a whole, as a Preset holds it
    void setEqualizer(bool enabled, double preamp, const QList<double>& bands);

    /// the band values of a preset, for its curve icon
    Q_INVOKABLE QList<double> equalizerPresetBands(int index) const;

    /**
     * the parameters of a filter (compressor, spatializer, stereo_widen,
     * scaletempo_pitch), as { name, text, unit, from, to, stepSize, scale,
     * defaultValue }; the value is shown multiplied by scale
     */
    Q_INVOKABLE QVariantList parameters(const QString& filter) const;

public slots:
    /// turns the equalizer on with a preset, or off with -1
    void applyEqualizerPreset(int index);
    /// shows a preset (or none with -1) until endEqualizerPreview() or applyEqualizerPreset()
    void previewEqualizerPreset(int index);
    void endEqualizerPreview();

    void setPreamp(double value);
    void setBand(int band, double value);
    void setTwoPass(bool enabled);

    void setFilterEnabled(const QString& filter, bool enabled);
    /// sets a filter parameter; the pitch filter turns on away from 0
    void setValue(const QString& name, double value);

    void setLanguage(const QString& code);

    /// sets the stereo mode of the output and keeps it as the default one
    void setStereoMode(int mode);
    /// tries a stereo mode until endStereoPreview() or setStereoMode()
    void previewStereoMode(int mode);
    void endStereoPreview();

    /// puts a group back to its defaults: a name of modifiedGroups, or
    /// "delay" for the audio delay of the current media
    void reset(const QString& group);
    /// every group, the stereo mode and the audio delay, not the language
    void resetAll();

signals:
    void equalizerChanged();
    void filtersChanged();
    void modifiedGroupsChanged();
    void languageChanged();
    void stereoModeChanged();
    /// the viewer changed something (not a preview)
    void adjusted();

private:
    struct EqualizerState
    {
        bool enabled = false;
        float preamp = 0.f;
        QList<double> bands;
    };

    void loadEqualizer();
    void showEqualizer(const EqualizerState& state);
    void saveEqualizer(const QString& presetName);
    void enableFilter(const char* name, bool enabled);
    void setParameter(const QString& name, double value);
    void emitModifiedGroups();
    void showStereoMode(int mode);

    vlc_player_t* m_player;
    bool m_profileWrites = true;
    QString m_language;

    EqualizerState m_equalizer;
    bool m_twoPass = false;
    std::optional<EqualizerState> m_beforePreview;

    int m_stereoMode = 0;
    std::optional<int> m_stereoBeforePreview;

    QStringList m_enabledFilters;
    QVariantMap m_values;
    QStringList m_modifiedGroups;
};

#endif // QVLC_AUDIO_ADJUSTMENTS_HPP
