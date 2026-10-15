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

#include "audio_adjustments.hpp"

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_configuration.h>
#include <vlc_iso_lang.h>
#include <vlc_player.h>

#include <QCoreApplication>
#include <QLocale>

#include <algorithm>
#include <cmath>

#include "../../../audio_filter/equalizer_presets.h"

namespace {

// a filter parameter of the Audio tab, as in the old effects dialog
struct Parameter
{
    const char* filter;
    const char* name;
    const char* text;
    const char* unit;
    float from;
    float to;
    float defaultValue;
    float step;
    // shown multiplied by this
    float scale;
};

const Parameter PARAMETERS[] = {
    { "compressor", "compressor-rms-peak", QT_TRANSLATE_NOOP("AudioAdjustments", "RMS / Peak"), "", 0.f, 1.f, 0.2f, 0.01f, 1.f },
    { "compressor", "compressor-attack", QT_TRANSLATE_NOOP("AudioAdjustments", "Attack"), "ms", 1.5f, 400.f, 25.f, 0.5f, 1.f },
    { "compressor", "compressor-release", QT_TRANSLATE_NOOP("AudioAdjustments", "Release"), "ms", 2.f, 800.f, 100.f, 1.f, 1.f },
    { "compressor", "compressor-threshold", QT_TRANSLATE_NOOP("AudioAdjustments", "Threshold"), "dB", -30.f, 0.f, -11.f, 0.1f, 1.f },
    { "compressor", "compressor-ratio", QT_TRANSLATE_NOOP("AudioAdjustments", "Ratio"), ": 1", 1.f, 20.f, 4.f, 0.1f, 1.f },
    { "compressor", "compressor-knee", QT_TRANSLATE_NOOP("AudioAdjustments", "Knee Radius"), "dB", 1.f, 10.f, 5.f, 0.1f, 1.f },
    { "compressor", "compressor-makeup-gain", QT_TRANSLATE_NOOP("AudioAdjustments", "Makeup Gain"), "dB", 0.f, 24.f, 7.f, 0.1f, 1.f },
    { "spatializer", "spatializer-roomsize", QT_TRANSLATE_NOOP("AudioAdjustments", "Size"), "", 0.f, 1.1f, 0.85f, 0.01f, 10.f },
    { "spatializer", "spatializer-width", QT_TRANSLATE_NOOP("AudioAdjustments", "Width"), "", 0.f, 1.f, 1.f, 0.01f, 10.f },
    { "spatializer", "spatializer-wet", QT_TRANSLATE_NOOP("AudioAdjustments", "Wet"), "", 0.f, 1.f, 0.4f, 0.01f, 10.f },
    { "spatializer", "spatializer-dry", QT_TRANSLATE_NOOP("AudioAdjustments", "Dry"), "", 0.f, 1.f, 0.5f, 0.01f, 10.f },
    { "spatializer", "spatializer-damp", QT_TRANSLATE_NOOP("AudioAdjustments", "Damp"), "", 0.f, 1.f, 0.5f, 0.01f, 10.f },
    { "stereo_widen", "stereowiden-delay", QT_TRANSLATE_NOOP("AudioAdjustments", "Delay Time"), "ms", 1.f, 100.f, 20.f, 1.f, 1.f },
    { "stereo_widen", "stereowiden-feedback", QT_TRANSLATE_NOOP("AudioAdjustments", "Feedback Gain"), "%", 0.f, 0.9f, 0.3f, 0.01f, 100.f },
    { "stereo_widen", "stereowiden-crossfeed", QT_TRANSLATE_NOOP("AudioAdjustments", "Crossfeed"), "%", 0.f, 0.8f, 0.3f, 0.01f, 100.f },
    { "stereo_widen", "stereowiden-dry-mix", QT_TRANSLATE_NOOP("AudioAdjustments", "Dry Mix"), "%", 0.f, 1.f, 0.8f, 0.01f, 100.f },
    { "scaletempo_pitch", "pitch-shift", QT_TRANSLATE_NOOP("AudioAdjustments", "Adjust Pitch"), "semitones", -12.f, 12.f, 0.f, 0.5f, 1.f },
};

const char PITCH_FILTER[] = "scaletempo_pitch";
const char PITCH[] = "pitch-shift";
const char* const FILTERS[] = { "compressor", "spatializer", "stereo_widen", PITCH_FILTER };

// the default audio languages offered, by ISO 639-1 code
const char* const LANGUAGES[] = {
    "ar", "bg", "ca", "cs", "da", "de", "el", "en", "es", "et", "eu", "fa",
    "fi", "fr", "he", "hi", "hr", "hu", "id", "is", "it", "ja", "ko", "lt",
    "lv", "ms", "nl", "no", "pl", "pt", "ro", "ru", "sk", "sl", "sr", "sv",
    "ta", "th", "tr", "uk", "vi", "zh",
};

const float DEFAULT_PREAMP = 12.f;
const double GAIN_TOLERANCE = 0.05;

QString filterGroup(const QString& filter)
{
    return filter == QLatin1String(PITCH_FILTER) ? QStringLiteral("pitch") : filter;
}

const Parameter* findParameter(const QString& name)
{
    for (const Parameter& parameter : PARAMETERS)
        if (name == QLatin1String(parameter.name))
            return &parameter;
    return nullptr;
}

QStringList splitFilters(const QString& chain)
{
    return chain.split(':', Qt::SkipEmptyParts);
}

QString configString(const char* name)
{
    if (!config_FindConfig(name))
        return {};
    char* value = config_GetPsz(name);
    const QString result = QString::fromUtf8(value);
    free(value);
    return result;
}

QString formatBands(const QList<double>& bands)
{
    QStringList values;
    for (double band : bands)
        values.append(QLocale::c().toString(band, 'f', 1));
    return values.join(' ');
}

QList<double> parseBands(const QString& text)
{
    QList<double> bands;
    for (const QString& value : text.split(' ', Qt::SkipEmptyParts))
    {
        bool ok = false;
        const double band = QLocale::c().toDouble(value, &ok);
        if (!ok)
            break;
        bands.append(band);
    }
    return bands;
}

QList<double> presetBands(int index)
{
    QList<double> bands;
    for (int i = 0; i < EQZ_BANDS_MAX; ++i)
        bands.append(std::round(eqz_preset_10b[index].f_amp[i] * 10.) / 10.);
    return bands;
}

bool sameGains(const QList<double>& a, const QList<double>& b)
{
    if (a.size() != b.size())
        return false;
    for (qsizetype i = 0; i < a.size(); ++i)
        if (std::abs(a[i] - b[i]) > GAIN_TOLERANCE)
            return false;
    return true;
}

// the audio output of the player, held for a scope
class Aout
{
public:
    explicit Aout(vlc_player_t* player)
        : m_aout(player ? vlc_player_aout_Hold(player) : nullptr)
    {}
    ~Aout()
    {
        if (m_aout)
            aout_Release(m_aout);
    }
    Aout(const Aout&) = delete;
    Aout& operator=(const Aout&) = delete;

    audio_output_t* get() const { return m_aout; }
    // the variable exists on the output (filters create theirs)
    bool has(const char* name) const { return m_aout && var_Type(m_aout, name) != 0; }

private:
    audio_output_t* m_aout;
};

} // namespace

AudioAdjustments::AudioAdjustments(vlc_player_t* player, QObject* parent)
    : QObject(parent)
    , m_player(player)
{
    m_enabledFilters = splitFilters(configString("audio-filter"));
    {
        Aout aout(m_player);
        if (aout.has("audio-filter"))
        {
            char* chain = var_GetString(aout.get(), "audio-filter");
            m_enabledFilters = splitFilters(QString::fromUtf8(chain));
            free(chain);
        }
    }

    for (const Parameter& parameter : PARAMETERS)
    {
        float value = parameter.defaultValue;
        if (config_FindConfig(parameter.name))
            value = config_GetFloat(parameter.name);
        m_values.insert(parameter.name, double(value));
    }

    loadEqualizer();
    if (config_FindConfig("equalizer-2pass"))
        m_twoPass = config_GetInt("equalizer-2pass") != 0;
    if (config_FindConfig("stereo-mode"))
        m_stereoMode = int(config_GetInt("stereo-mode"));

    m_modifiedGroups = modifiedGroups();
}

void AudioAdjustments::loadEqualizer()
{
    m_equalizer.enabled = m_enabledFilters.contains("equalizer");
    m_equalizer.preamp = config_FindConfig("equalizer-preamp")
                       ? config_GetFloat("equalizer-preamp") : DEFAULT_PREAMP;
    m_equalizer.bands = parseBands(configString("equalizer-bands"));
    if (m_equalizer.bands.size() != BAND_COUNT)
    {
        // the bands of the preset, flat by default
        const QString name = configString("equalizer-preset");
        int preset = 0;
        for (int i = 0; i < NB_PRESETS; ++i)
            if (name == QLatin1String(preset_list[i]))
                preset = i;
        m_equalizer.bands = presetBands(preset);
    }
}

QStringList AudioAdjustments::equalizerPresets() const
{
    QStringList names;
    for (int i = 0; i < NB_PRESETS; ++i)
        names.append(QString::fromUtf8(vlc_gettext(preset_list_text[i])));
    return names;
}

QStringList AudioAdjustments::bandNames() const
{
    const bool vlcBands = !config_FindConfig("equalizer-vlcfreqs")
                          || config_GetInt("equalizer-vlcfreqs") != 0;
    const float* frequencies = vlcBands ? f_vlc_frequency_table_10b : f_iso_frequency_table_10b;
    QStringList names;
    for (int i = 0; i < EQZ_BANDS_MAX; ++i)
    {
        const float f = frequencies[i];
        if (f >= 1000.f)
            //: an equalizer band, "12 KHz"
            names.append(tr("%1 KHz").arg(QLocale().toString(f / 1000.f, 'g', 3)));
        else
            //: an equalizer band, "60 Hz"
            names.append(tr("%1 Hz").arg(QLocale().toString(std::round(f))));
    }
    return names;
}

bool AudioAdjustments::isEqualizerEnabled() const
{
    return m_equalizer.enabled;
}

int AudioAdjustments::equalizerPreset() const
{
    for (int i = 0; i < NB_PRESETS; ++i)
        if (std::abs(m_equalizer.preamp - eqz_preset_10b[i].f_preamp) <= GAIN_TOLERANCE
            && sameGains(m_equalizer.bands, presetBands(i)))
            return i;
    return -1;
}

double AudioAdjustments::preamp() const
{
    return m_equalizer.preamp;
}

QList<double> AudioAdjustments::bands() const
{
    return m_equalizer.bands;
}

bool AudioAdjustments::twoPass() const
{
    return m_twoPass;
}

bool AudioAdjustments::isPreviewing() const
{
    return m_beforePreview.has_value();
}

QStringList AudioAdjustments::enabledFilters() const
{
    QStringList filters;
    for (const char* filter : FILTERS)
        if (m_enabledFilters.contains(filter))
            filters.append(filter);
    return filters;
}

QVariantMap AudioAdjustments::values() const
{
    return m_values;
}

QStringList AudioAdjustments::modifiedGroups() const
{
    const EqualizerState& eq = m_beforePreview ? *m_beforePreview : m_equalizer;

    QStringList groups;
    if (eq.enabled || m_twoPass || std::abs(eq.preamp - DEFAULT_PREAMP) > GAIN_TOLERANCE
        || !sameGains(eq.bands, presetBands(0)))
        groups.append("equalizer");

    for (const char* filter : FILTERS)
    {
        bool modified = m_enabledFilters.contains(filter);
        for (const Parameter& parameter : PARAMETERS)
            if (!strcmp(parameter.filter, filter)
                && std::abs(m_values.value(parameter.name).toDouble() - parameter.defaultValue) > 1e-4)
                modified = true;
        if (modified)
            groups.append(filterGroup(filter));
    }

    if (config_FindConfig("stereo-mode") && config_GetInt("stereo-mode") != AOUT_VAR_CHAN_UNSET)
        groups.append("stereo");
    return groups;
}

QString AudioAdjustments::language() const
{
    // the first of the preferred languages
    return configString("audio-language").section(',', 0, 0).trimmed();
}

QVariantList AudioAdjustments::languages() const
{
    QVariantList languages;
    //: the default audio language: the one the file prefers
    languages.append(QVariantMap{ { "code", QString() }, { "name", tr("Auto") } });

    QStringList codes;
    for (const char* code : LANGUAGES)
        codes.append(code);
    const QString current = language();
    if (!current.isEmpty() && !codes.contains(current))
        codes.append(current);

    QVariantList named;
    for (const QString& code : codes)
    {
        const iso639_lang_t* lang = vlc_find_iso639(code.toUtf8().constData(), false);
        const QString name = lang ? QString::fromUtf8(vlc_gettext(lang->psz_eng_name)) : code;
        named.append(QVariantMap{ { "code", code }, { "name", name } });
    }
    std::sort(named.begin(), named.end(), [](const QVariant& a, const QVariant& b) {
        return QString::localeAwareCompare(a.toMap().value("name").toString(),
                                           b.toMap().value("name").toString()) < 0;
    });
    return languages + named;
}

QList<double> AudioAdjustments::equalizerPresetBands(int index) const
{
    if (index < 0 || index >= NB_PRESETS)
        return {};
    return presetBands(index);
}

QVariantList AudioAdjustments::parameters(const QString& filter) const
{
    QVariantList list;
    for (const Parameter& parameter : PARAMETERS)
    {
        if (filter != QLatin1String(parameter.filter))
            continue;
        list.append(QVariantMap{
            { "name", parameter.name },
            { "text", qApp->translate("AudioAdjustments", parameter.text) },
            { "unit", qApp->translate("AudioAdjustments", parameter.unit) },
            { "from", double(parameter.from) },
            { "to", double(parameter.to) },
            { "stepSize", double(parameter.step) },
            { "scale", double(parameter.scale) },
            { "defaultValue", double(parameter.defaultValue) },
        });
    }
    return list;
}

void AudioAdjustments::showEqualizer(const EqualizerState& state)
{
    Aout aout(m_player);
    if (aout.has("equalizer-preamp"))
        var_SetFloat(aout.get(), "equalizer-preamp", state.preamp);
    if (aout.has("equalizer-bands"))
        var_SetString(aout.get(), "equalizer-bands", (formatBands(state.bands)).toUtf8().constData());
    if (aout.get())
        aout_EnableFilter(aout.get(), "equalizer", state.enabled);
}

void AudioAdjustments::saveEqualizer(const QString& presetName)
{
    config_PutFloat("equalizer-preamp", m_equalizer.preamp);
    config_PutPsz("equalizer-bands", (formatBands(m_equalizer.bands)).toUtf8().constData());
    if (!presetName.isEmpty())
        config_PutPsz("equalizer-preset", presetName.toUtf8().constData());
    enableFilter("equalizer", m_equalizer.enabled);
}

void AudioAdjustments::enableFilter(const char* name, bool enabled)
{
    if (enabled && !m_enabledFilters.contains(name))
        m_enabledFilters.append(name);
    else if (!enabled)
        m_enabledFilters.removeAll(name);

    // the equalizer goes first, as the output does it
    QStringList chain = splitFilters(configString("audio-filter"));
    chain.removeAll(name);
    if (enabled)
    {
        if (!strcmp(name, "equalizer"))
            chain.prepend(name);
        else
            chain.append(name);
    }
    config_PutPsz("audio-filter", (chain.join(':')).toUtf8().constData());

    Aout aout(m_player);
    if (aout.get())
        aout_EnableFilter(aout.get(), name, enabled);
}

void AudioAdjustments::emitModifiedGroups()
{
    const QStringList groups = modifiedGroups();
    if (groups == m_modifiedGroups)
        return;
    m_modifiedGroups = groups;
    emit modifiedGroupsChanged();
}

void AudioAdjustments::applyEqualizerPreset(int index)
{
    m_beforePreview.reset();

    if (index >= 0 && index < NB_PRESETS)
    {
        m_equalizer.enabled = true;
        m_equalizer.preamp = eqz_preset_10b[index].f_preamp;
        m_equalizer.bands = presetBands(index);
    }
    else
        m_equalizer.enabled = false;

    showEqualizer(m_equalizer);
    saveEqualizer(index >= 0 && index < NB_PRESETS ? QString(preset_list[index]) : QString());
    emit equalizerChanged();
    emitModifiedGroups();
}

void AudioAdjustments::previewEqualizerPreset(int index)
{
    if (!m_beforePreview)
        m_beforePreview = m_equalizer;

    EqualizerState shown = *m_beforePreview;
    if (index >= 0 && index < NB_PRESETS)
    {
        shown.enabled = true;
        shown.preamp = eqz_preset_10b[index].f_preamp;
        shown.bands = presetBands(index);
    }
    else
        shown.enabled = false;

    m_equalizer = shown;
    showEqualizer(m_equalizer);
    emit equalizerChanged();
}

void AudioAdjustments::endEqualizerPreview()
{
    if (!m_beforePreview)
        return;
    m_equalizer = *m_beforePreview;
    m_beforePreview.reset();
    showEqualizer(m_equalizer);
    emit equalizerChanged();
}

void AudioAdjustments::setPreamp(double value)
{
    m_beforePreview.reset();
    m_equalizer.enabled = true;
    m_equalizer.preamp = std::clamp(value, -20., 20.);
    showEqualizer(m_equalizer);
    saveEqualizer({});
    emit equalizerChanged();
    emitModifiedGroups();
}

void AudioAdjustments::setBand(int band, double value)
{
    if (band < 0 || band >= m_equalizer.bands.size())
        return;
    m_beforePreview.reset();
    m_equalizer.enabled = true;
    m_equalizer.bands[band] = std::clamp(value, -20., 20.);
    showEqualizer(m_equalizer);
    saveEqualizer({});
    emit equalizerChanged();
    emitModifiedGroups();
}

void AudioAdjustments::setTwoPass(bool enabled)
{
    if (m_twoPass == enabled)
        return;
    m_twoPass = enabled;
    config_PutInt("equalizer-2pass", enabled);
    Aout aout(m_player);
    if (aout.has("equalizer-2pass"))
        var_SetBool(aout.get(), "equalizer-2pass", enabled);
    emit equalizerChanged();
    emitModifiedGroups();
}

void AudioAdjustments::setFilterEnabled(const QString& filter, bool enabled)
{
    bool known = false;
    for (const char* name : FILTERS)
        known |= filter == QLatin1String(name);
    if (!known || m_enabledFilters.contains(filter) == enabled)
        return;

    enableFilter(filter.toUtf8().constData(), enabled);
    emit filtersChanged();
    emitModifiedGroups();
}

void AudioAdjustments::setParameter(const QString& name, double value)
{
    const Parameter* parameter = findParameter(name);
    if (!parameter)
        return;
    value = std::clamp(value, double(parameter->from), double(parameter->to));
    m_values.insert(name, value);

    if (config_FindConfig(parameter->name))
        config_PutFloat(parameter->name, value);
    Aout aout(m_player);
    if (aout.has(parameter->name))
        var_SetFloat(aout.get(), parameter->name, value);
}

void AudioAdjustments::setValue(const QString& name, double value)
{
    if (!findParameter(name))
        return;
    setParameter(name, value);

    // the pitch filter only runs away from 0
    if (name == QLatin1String(PITCH))
    {
        const bool shifted = std::abs(m_values.value(PITCH).toDouble()) > 1e-4;
        if (shifted != m_enabledFilters.contains(PITCH_FILTER))
            enableFilter(PITCH_FILTER, shifted);
    }

    emit filtersChanged();
    emitModifiedGroups();
}

void AudioAdjustments::setLanguage(const QString& code)
{
    if (code == language())
        return;
    config_PutPsz("audio-language", code.toUtf8().constData());
    if (m_player)
    {
        vlc_player_Lock(m_player);
        vlc_player_SelectCategoryLanguage(m_player, AUDIO_ES, code.isEmpty() ? nullptr : code.toUtf8().constData());
        vlc_player_Unlock(m_player);
    }
    emit languageChanged();
}

QVariantList AudioAdjustments::stereoModes() const
{
    const struct { int value; const char* name; } modes[] = {
        //: the stereo mode the audio output picks for the file
        { AOUT_VAR_CHAN_UNSET, QT_TR_NOOP("Automatic") },
        { AOUT_VAR_CHAN_STEREO, QT_TR_NOOP("Stereo") },
        { AOUT_VAR_CHAN_MONO, QT_TR_NOOP("Mono") },
        { AOUT_VAR_CHAN_RSTEREO, QT_TR_NOOP("Reverse stereo") },
        { AOUT_VAR_CHAN_LEFT, QT_TR_NOOP("Left") },
        { AOUT_VAR_CHAN_RIGHT, QT_TR_NOOP("Right") },
        { AOUT_VAR_CHAN_DOLBYS, QT_TR_NOOP("Dolby Surround") },
    };
    QVariantList list;
    for (const auto& mode : modes)
        list.append(QVariantMap{ { "value", mode.value }, { "name", tr(mode.name) } });
    return list;
}

int AudioAdjustments::stereoMode() const
{
    return m_stereoMode;
}

void AudioAdjustments::showStereoMode(int mode)
{
    // the output restarts with the new mode
    Aout aout(m_player);
    if (aout.has("stereo-mode") && var_GetInteger(aout.get(), "stereo-mode") != mode)
        var_SetInteger(aout.get(), "stereo-mode", mode);
    if (m_stereoMode != mode)
    {
        m_stereoMode = mode;
        emit stereoModeChanged();
    }
}

void AudioAdjustments::setStereoMode(int mode)
{
    m_stereoBeforePreview.reset();
    showStereoMode(mode);
    config_PutInt("stereo-mode", mode);
    emitModifiedGroups();
}

void AudioAdjustments::previewStereoMode(int mode)
{
    if (!m_stereoBeforePreview)
        m_stereoBeforePreview = m_stereoMode;
    showStereoMode(mode);
}

void AudioAdjustments::endStereoPreview()
{
    if (!m_stereoBeforePreview)
        return;
    const int mode = *m_stereoBeforePreview;
    m_stereoBeforePreview.reset();
    showStereoMode(mode);
}

void AudioAdjustments::reset(const QString& group)
{
    if (group == QLatin1String("equalizer"))
    {
        m_beforePreview.reset();
        m_equalizer.enabled = false;
        m_equalizer.preamp = DEFAULT_PREAMP;
        m_equalizer.bands = presetBands(0);
        showEqualizer(m_equalizer);
        saveEqualizer(preset_list[0]);
        if (m_twoPass)
            setTwoPass(false);
        emit equalizerChanged();
    }
    else if (group == QLatin1String("stereo"))
    {
        setStereoMode(AOUT_VAR_CHAN_UNSET);
    }
    else if (group == QLatin1String("delay"))
    {
        if (m_player)
        {
            vlc_player_Lock(m_player);
            vlc_player_SetAudioDelay(m_player, 0, VLC_PLAYER_WHENCE_ABSOLUTE);
            vlc_player_Unlock(m_player);
        }
    }
    else
    {
        const QString filter = group == QLatin1String("pitch") ? QString(PITCH_FILTER) : group;
        for (const Parameter& parameter : PARAMETERS)
            if (filter == QLatin1String(parameter.filter))
                setParameter(parameter.name, parameter.defaultValue);
        if (m_enabledFilters.contains(filter))
            enableFilter(filter.toUtf8().constData(), false);
        emit filtersChanged();
    }
    emitModifiedGroups();
}

void AudioAdjustments::resetAll()
{
    reset("equalizer");
    for (const char* filter : FILTERS)
        reset(filterGroup(filter));
    reset("stereo");
    reset("delay");
}
