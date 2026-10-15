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

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_configuration.h>
#include <vlc_input_item.h>
#include <vlc_player.h>
#include <vlc/libvlc.h>
#include "../../../../lib/libvlc_internal.h"

#include <QTest>
#include <QSignalSpy>
#include <QLocale>

#include <cmath>

#include "../player/audio_adjustments.hpp"

// The Audio tab of the Adjustments sidebar (spec #30, ticket #51): what its
// model does to the player's audio output and to the configuration
class TestAudioTab : public QObject
{
    Q_OBJECT

    libvlc_instance_t* m_vlc = nullptr;
    vlc_player_t* m_player = nullptr;
    std::unique_ptr<AudioAdjustments> m_audio;

    static constexpr int FLAT = 0;
    static constexpr int CLUB = 2;
    static constexpr int ROCK = 13;

    vlc_object_t* vlcObject() const
    {
        return VLC_OBJECT(m_vlc->p_libvlc_int);
    }

    // a string variable of the audio output
    QString aoutString(const char* name)
    {
        audio_output_t* aout = vlc_player_aout_Hold(m_player);
        if (!aout)
            return {};
        char* value = var_GetString(aout, name);
        aout_Release(aout);
        QString result = QString::fromUtf8(value);
        free(value);
        return result;
    }

    float aoutFloat(const char* name)
    {
        audio_output_t* aout = vlc_player_aout_Hold(m_player);
        if (!aout)
            return NAN;
        float value = var_GetFloat(aout, name);
        aout_Release(aout);
        return value;
    }

    int aoutInteger(const char* name)
    {
        audio_output_t* aout = vlc_player_aout_Hold(m_player);
        if (!aout)
            return -1;
        const int value = int(var_GetInteger(aout, name));
        aout_Release(aout);
        return value;
    }

    bool aoutHasFilter(const char* name)
    {
        return aoutString("audio-filter").split(':').contains(name);
    }

    QString configString(const char* name)
    {
        char* value = config_GetPsz(name);
        QString result = QString::fromUtf8(value);
        free(value);
        return result;
    }

    // the 10 bands of the equalizer of the output
    QList<double> aoutBands()
    {
        QList<double> bands;
        for (const QString& band : aoutString("equalizer-bands").split(' ', Qt::SkipEmptyParts))
            bands.append(QLocale::c().toDouble(band));
        return bands;
    }

private slots:
    void initTestCase()
    {
        setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 0);
        setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 0);

        static const char* args[] = { "--ignore-config", "-v", "--aout=adummy" };
        m_vlc = libvlc_new(ARRAY_SIZE(args), args);
        QVERIFY(m_vlc);
    }

    void cleanupTestCase()
    {
        if (m_vlc)
            libvlc_release(m_vlc);
    }

    void init()
    {
        m_player = vlc_player_New(vlcObject(), VLC_PLAYER_LOCK_NORMAL);
        QVERIFY(m_player);
        m_audio = std::make_unique<AudioAdjustments>(m_player);
    }

    void cleanup()
    {
        if (m_audio)
            m_audio->resetAll();
        m_audio.reset();
        if (m_player)
            vlc_player_Delete(m_player);
        m_player = nullptr;
    }

    void theEqualizerIsOffAtFirst()
    {
        QCOMPARE(m_audio->isEqualizerEnabled(), false);
        QVERIFY(!aoutHasFilter("equalizer"));
        QCOMPARE(m_audio->equalizerPresets().first(), QStringLiteral("Flat"));
        QCOMPARE(m_audio->equalizerPresets().size(), 18);
    }

    void aPresetTurnsTheEqualizerOnWithItsBands()
    {
        QSignalSpy changed(m_audio.get(), &AudioAdjustments::equalizerChanged);

        m_audio->applyEqualizerPreset(CLUB);

        QVERIFY(changed.count() > 0);
        QCOMPARE(m_audio->isEqualizerEnabled(), true);
        QCOMPARE(m_audio->equalizerPreset(), CLUB);
        // VLC's "club" preset
        const QList<double> club = { 0, 0, 8, 5.6, 5.6, 5.6, 3.2, 0, 0, 0 };
        for (int i = 0; i < club.size(); ++i)
        {
            QVERIFY(qAbs(m_audio->bands()[i] - club[i]) < 0.01);
            QVERIFY(qAbs(aoutBands()[i] - club[i]) < 0.01);
        }
        QCOMPARE(m_audio->preamp(), 6.0);
        QCOMPARE(aoutFloat("equalizer-preamp"), 6.f);
        QVERIFY(aoutHasFilter("equalizer"));

        // it stays for the next files and sessions
        QVERIFY(configString("audio-filter").split(':').contains("equalizer"));
        QCOMPARE(configString("equalizer-preset"), QStringLiteral("club"));
    }

    void noneTurnsTheEqualizerOff()
    {
        m_audio->applyEqualizerPreset(ROCK);
        m_audio->applyEqualizerPreset(-1);

        QCOMPARE(m_audio->isEqualizerEnabled(), false);
        QVERIFY(!aoutHasFilter("equalizer"));
        QVERIFY(!configString("audio-filter").split(':').contains("equalizer"));
    }

    // hovering a preset tries it, moving away puts back what was there
    void aPreviewedPresetIsUndoneWhenThePreviewEnds()
    {
        m_audio->applyEqualizerPreset(CLUB);

        m_audio->previewEqualizerPreset(ROCK);
        QVERIFY(m_audio->isPreviewing());
        QCOMPARE(m_audio->equalizerPreset(), ROCK);
        QVERIFY(qAbs(aoutBands()[0] - 8.0) < 0.01); // rock starts at +8 dB
        // a preview is not kept
        QCOMPARE(configString("equalizer-preset"), QStringLiteral("club"));

        m_audio->previewEqualizerPreset(-1);
        QVERIFY(!aoutHasFilter("equalizer"));

        m_audio->endEqualizerPreview();
        QVERIFY(!m_audio->isPreviewing());
        QCOMPARE(m_audio->equalizerPreset(), CLUB);
        QVERIFY(aoutHasFilter("equalizer"));
        QVERIFY(qAbs(aoutBands()[2] - 8.0) < 0.01);
    }

    void aPreviewedPresetIsKeptWhenClicked()
    {
        m_audio->previewEqualizerPreset(ROCK);
        QVERIFY(aoutHasFilter("equalizer"));

        m_audio->applyEqualizerPreset(ROCK);
        m_audio->endEqualizerPreview();

        QVERIFY(!m_audio->isPreviewing());
        QCOMPARE(m_audio->equalizerPreset(), ROCK);
        QVERIFY(aoutHasFilter("equalizer"));
        QCOMPARE(configString("equalizer-preset"), QStringLiteral("rock"));
    }

    void movingABandMakesCustomBands()
    {
        m_audio->applyEqualizerPreset(FLAT);

        m_audio->setBand(3, -4.5);

        QCOMPARE(m_audio->equalizerPreset(), -1);
        QCOMPARE(m_audio->bands()[3], -4.5);
        QVERIFY(qAbs(aoutBands()[3] + 4.5) < 0.01);
        QVERIFY(configString("equalizer-bands").startsWith("0.0 0.0 0.0 -4.5"));

        m_audio->setPreamp(3);
        QCOMPARE(aoutFloat("equalizer-preamp"), 3.f);
        QCOMPARE(config_GetFloat("equalizer-preamp"), 3.f);
    }

    void theBandsHaveTheirFrequencies()
    {
        const QStringList names = m_audio->bandNames();
        QCOMPARE(names.size(), AudioAdjustments::BAND_COUNT);
        QCOMPARE(names.first(), QStringLiteral("60 Hz"));
        QCOMPARE(names.last(), QStringLiteral("16 KHz"));
    }

    void thePresetCurvesFollowTheirBands()
    {
        // VLC's "full bass": +9.6 dB at 170 Hz, -11.2 dB at 16 KHz
        const QList<double> fullBass = m_audio->equalizerPresetBands(4);
        QCOMPARE(fullBass.size(), AudioAdjustments::BAND_COUNT);
        QCOMPARE(fullBass[1], 9.6);
        QCOMPARE(fullBass.last(), -11.2);
        QVERIFY(m_audio->equalizerPresetBands(-1).isEmpty());
    }

    void aFilterTurnsOnAndOff()
    {
        QSignalSpy groups(m_audio.get(), &AudioAdjustments::modifiedGroupsChanged);

        m_audio->setFilterEnabled("compressor", true);

        QVERIFY(m_audio->enabledFilters().contains("compressor"));
        QVERIFY(aoutHasFilter("compressor"));
        QVERIFY(configString("audio-filter").split(':').contains("compressor"));
        QVERIFY(m_audio->modifiedGroups().contains("compressor"));
        QCOMPARE(groups.count(), 1);

        m_audio->setFilterEnabled("compressor", false);
        QVERIFY(!aoutHasFilter("compressor"));
        QVERIFY(!m_audio->modifiedGroups().contains("compressor"));
    }

    void aFilterParameterIsKept()
    {
        const QVariantList parameters = m_audio->parameters("spatializer");
        QCOMPARE(parameters.size(), 5);
        QCOMPARE(parameters.first().toMap().value("name").toString(), QStringLiteral("spatializer-roomsize"));

        m_audio->setValue("spatializer-wet", 0.7);

        QCOMPARE(m_audio->values().value("spatializer-wet").toDouble(), 0.7);
        QCOMPARE(config_GetFloat("spatializer-wet"), 0.7f);
        QVERIFY(m_audio->modifiedGroups().contains("spatializer"));
    }

    void aGroupResetsToItsDefaults()
    {
        m_audio->setFilterEnabled("stereo_widen", true);
        m_audio->setValue("stereowiden-delay", 50);

        m_audio->reset("stereo_widen");

        QVERIFY(!m_audio->enabledFilters().contains("stereo_widen"));
        QVERIFY(!aoutHasFilter("stereo_widen"));
        QCOMPARE(m_audio->values().value("stereowiden-delay").toDouble(), 20.0);
        QCOMPARE(config_GetFloat("stereowiden-delay"), 20.f);
        QVERIFY(m_audio->modifiedGroups().isEmpty());
    }

    // the pitch filter only runs while the pitch is shifted
    void thePitchTurnsItsFilterOn()
    {
        m_audio->setValue("pitch-shift", 2.5);

        QVERIFY(aoutHasFilter("scaletempo_pitch"));
        QCOMPARE(config_GetFloat("pitch-shift"), 2.5f);
        QVERIFY(m_audio->modifiedGroups().contains("pitch"));

        m_audio->setValue("pitch-shift", 0);
        QVERIFY(!aoutHasFilter("scaletempo_pitch"));
        QVERIFY(!m_audio->modifiedGroups().contains("pitch"));
    }

    // the stereo mode is a preference, tried on hover like the presets
    void aStereoModeIsChosenOrTried()
    {
        const QVariantList modes = m_audio->stereoModes();
        QCOMPARE(modes.first().toMap().value("value").toInt(), AOUT_VAR_CHAN_UNSET);
        bool mono = false;
        for (const QVariant& mode : modes)
            mono |= mode.toMap().value("value").toInt() == AOUT_VAR_CHAN_MONO
                    && mode.toMap().value("name") == QLatin1String("Mono");
        QVERIFY(mono);
        QCOMPARE(m_audio->stereoMode(), AOUT_VAR_CHAN_UNSET);

        m_audio->previewStereoMode(AOUT_VAR_CHAN_RSTEREO);
        QCOMPARE(aoutInteger("stereo-mode"), AOUT_VAR_CHAN_RSTEREO);
        QCOMPARE(config_GetInt("stereo-mode"), AOUT_VAR_CHAN_UNSET);
        m_audio->endStereoPreview();
        QCOMPARE(aoutInteger("stereo-mode"), AOUT_VAR_CHAN_UNSET);

        QSignalSpy changed(m_audio.get(), &AudioAdjustments::stereoModeChanged);
        m_audio->previewStereoMode(AOUT_VAR_CHAN_LEFT);
        m_audio->setStereoMode(AOUT_VAR_CHAN_MONO);
        m_audio->endStereoPreview();

        QVERIFY(changed.count() > 0);
        QCOMPARE(m_audio->stereoMode(), AOUT_VAR_CHAN_MONO);
        QCOMPARE(aoutInteger("stereo-mode"), AOUT_VAR_CHAN_MONO);
        QCOMPARE(config_GetInt("stereo-mode"), AOUT_VAR_CHAN_MONO);
        QVERIFY(m_audio->modifiedGroups().contains("stereo"));

        m_audio->reset("stereo");
        QCOMPARE(m_audio->stereoMode(), AOUT_VAR_CHAN_UNSET);
        QCOMPARE(aoutInteger("stereo-mode"), AOUT_VAR_CHAN_UNSET);
        QVERIFY(!m_audio->modifiedGroups().contains("stereo"));
    }

    void theDefaultLanguageIsAPreference()
    {
        const QVariantList languages = m_audio->languages();
        QCOMPARE(languages.first().toMap().value("code").toString(), QString());
        bool english = false;
        for (const QVariant& language : languages)
            english |= language.toMap().value("name") == QLatin1String("English")
                       && language.toMap().value("code") == QLatin1String("en");
        QVERIFY(english);

        QSignalSpy changed(m_audio.get(), &AudioAdjustments::languageChanged);
        m_audio->setLanguage("fr");

        QCOMPARE(changed.count(), 1);
        QCOMPARE(m_audio->language(), QStringLiteral("fr"));
        QCOMPARE(configString("audio-language"), QStringLiteral("fr"));
        vlc_player_Lock(m_player);
        char* lang = vlc_player_GetCategoryLanguage(m_player, AUDIO_ES);
        vlc_player_Unlock(m_player);
        QCOMPARE(QString::fromUtf8(lang), QStringLiteral("fr"));
        free(lang);

        // not an adjustment: Reset all keeps it
        m_audio->resetAll();
        QCOMPARE(m_audio->language(), QStringLiteral("fr"));
        m_audio->setLanguage({});
        QCOMPARE(configString("audio-language"), QString());
    }

    void resetAllPutsEveryGroupBack()
    {
        m_audio->applyEqualizerPreset(CLUB);
        m_audio->setTwoPass(true);
        m_audio->setFilterEnabled("compressor", true);
        m_audio->setValue("pitch-shift", -3);
        config_PutInt("stereo-mode", AOUT_VAR_CHAN_MONO);
        // a delay belongs to the current media
        input_item_t* media = input_item_New("mock://", "mock");
        QVERIFY(media);
        vlc_player_Lock(m_player);
        QCOMPARE(vlc_player_SetCurrentMedia(m_player, media), VLC_SUCCESS);
        vlc_player_SetAudioDelay(m_player, VLC_TICK_FROM_MS(500), VLC_PLAYER_WHENCE_ABSOLUTE);
        QCOMPARE(vlc_player_GetAudioDelay(m_player), VLC_TICK_FROM_MS(500));
        vlc_player_Unlock(m_player);
        input_item_Release(media);

        m_audio->resetAll();

        QVERIFY(m_audio->modifiedGroups().isEmpty());
        QCOMPARE(m_audio->isEqualizerEnabled(), false);
        QCOMPARE(m_audio->twoPass(), false);
        QVERIFY(m_audio->enabledFilters().isEmpty());
        QCOMPARE(config_GetInt("stereo-mode"), AOUT_VAR_CHAN_UNSET);
        QCOMPARE(splitChain(configString("audio-filter")), QStringList());
        vlc_player_Lock(m_player);
        const vlc_tick_t delay = vlc_player_GetAudioDelay(m_player);
        vlc_player_Unlock(m_player);
        QCOMPARE(delay, VLC_TICK_FROM_MS(0));
    }

    // a new model, for example after a restart, shows what was kept
    void theFiltersStayForTheNextSession()
    {
        m_audio->applyEqualizerPreset(ROCK);
        m_audio->setFilterEnabled("spatializer", true);

        AudioAdjustments next(m_player);

        QCOMPARE(next.isEqualizerEnabled(), true);
        QCOMPARE(next.equalizerPreset(), ROCK);
        QVERIFY(next.enabledFilters().contains("spatializer"));
    }

private:
    static QStringList splitChain(const QString& chain)
    {
        return chain.split(':', Qt::SkipEmptyParts);
    }
};

QTEST_GUILESS_MAIN(TestAudioTab)
#include "test_audio_tab.moc"
