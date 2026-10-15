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

#include "player_preset_target.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_iso_lang.h>
#include <vlc_player.h>

#include "qt.hpp"
#include "player_controller.hpp"
#include "input_models.hpp"
#include "audio_adjustments.hpp"
#include "subtitle_appearance.hpp"
#include "video_adjustments.hpp"

#include <QColor>
#include <QLocale>

#include <cmath>
#include <utility>

namespace {

const char* const AUDIO_FILTERS[] = { "compressor", "spatializer", "stereo_widen" };

const QLatin1String SUB_PREFIX("sub-");
const QLatin1String VIDEO_PREFIX("video-");

// the equalizer of a Preset: "" when off, else the preamp then the bands
QString formatEqualizer(const AudioAdjustments& audio)
{
    if (!audio.isEqualizerEnabled())
        return {};
    QStringList values{ QLocale::c().toString(audio.preamp(), 'f', 1) };
    for (double band : audio.bands())
        values.append(QLocale::c().toString(band, 'f', 1));
    return values.join(' ');
}

QVariantMap audioValues(const AudioAdjustments& audio)
{
    QVariantMap values{
        { "audio-language", audio.language() },
        { "stereo-mode", audio.stereoMode() },
        { "equalizer", formatEqualizer(audio) },
        { "equalizer-2pass", audio.twoPass() },
    };
    const QStringList enabled = audio.enabledFilters();
    for (const char* filter : AUDIO_FILTERS)
        values.insert(filter, enabled.contains(filter));
    const QVariantMap parameters = audio.values();
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
        values.insert(it.key(), std::round(it.value().toDouble() * 1000.) / 1000.);
    return values;
}

QVariantMap subtitleValues(const SubtitleAppearance& sub)
{
    return {
        { "sub-language", sub.defaultLanguage() },
        { "sub-keep-styling", sub.keepFileStyling() },
        { "sub-font", sub.font() },
        { "sub-size-mode", int(sub.sizeMode()) },
        { "sub-relative-size", int(sub.relativeSize()) },
        { "sub-pixel-size", sub.pixelSize() },
        { "sub-text-color", sub.textColor().name() },
        { "sub-outline-color", sub.outlineColor().name() },
        { "sub-outline-thickness", int(sub.outlineThickness()) },
        { "sub-background-opacity", sub.backgroundOpacity() },
        { "sub-vertical-offset", sub.verticalOffset() },
    };
}

QVariantMap videoValues(const VideoAdjustments& video)
{
    QVariantMap values{
        { "video-flip", int(video.flip()) },
        { "video-rotation", video.rotation() },
    };
    const QVariantMap filters = video.values();
    for (auto it = filters.cbegin(); it != filters.cend(); ++it)
        values.insert(VIDEO_PREFIX + it.key(), it.value());
    return values;
}

// the subtitle options of the Profile, read from the configuration
class ProfileSubtitleOptions : public SubtitleAppearance::Backend
{
public:
    QVariant value(const char* option) const override
    {
        switch (config_GetType(option))
        {
        case VLC_VAR_BOOL:
            return config_GetInt(option) != 0;
        case VLC_VAR_INTEGER:
            return QVariant::fromValue<qlonglong>(config_GetInt(option));
        case VLC_VAR_STRING:
        {
            char* psz = config_GetPsz(option);
            const QString string = qfu(psz);
            free(psz);
            return string;
        }
        default:
            return {};
        }
    }
    void setValue(const char*, const QVariant&) override {}
    void apply(const char*, const QVariant&) override {}
    void redraw() override {}
    void reloadProfile() override {}
};

// no output: the defaults of the Video tab
class NoVideoOutputs : public VideoOutputTarget
{
public:
    QString filterChain() const override { return {}; }
    void setFilterChain(const QString&) override {}
    void setOption(const char*, const QVariant&) override {}
    bool hasPreference(const char*) const override { return false; }
    QVariant preference(const char*) const override { return {}; }
    void setPreference(const char*, const QVariant&) override {}
};

} // namespace

PlayerPresetTarget::PlayerPresetTarget(PlayerController* player, QObject* parent)
    : PresetTarget(parent)
    , m_player(player)
{
    AudioAdjustments* audio = m_player->getAudioAdjustments();
    SubtitleAppearance* sub = m_player->getSubtitleAppearance();
    VideoAdjustments* video = m_player->getVideoAdjustments();

    connect(audio, &AudioAdjustments::adjusted, this, [this]() {
        reportChanges({ QStringLiteral("audio-"), QStringLiteral("stereo-mode"),
                        QStringLiteral("equalizer"), QStringLiteral("compressor"),
                        QStringLiteral("spatializer"), QStringLiteral("stereowiden"),
                        QStringLiteral("stereo_widen"), QStringLiteral("pitch-") });
    });
    connect(sub, &SubtitleAppearance::adjusted, this, [this]() {
        reportChanges({ SUB_PREFIX });
    });
    connect(video, &VideoAdjustments::adjusted, this, [this]() {
        reportChanges({ VIDEO_PREFIX });
    });

    // the speed changes later than asked; the delays at once
    connect(m_player, &PlayerController::adjustedByUser, this, [this](const QString& key) {
        if (key == QLatin1String("rate"))
            m_ratePending = true;
        else
            reportChanges({ key });
    });
    connect(m_player, &PlayerController::rateChanged, this, [this]() {
        if (!std::exchange(m_ratePending, false))
            return;
        reportChanges({ QStringLiteral("rate") });
    });

    // a picked track gives the preferred language of its category
    connect(m_player->getAudioTracks(), &TrackListModel::picked, this, [this](const QString& language) {
        const QString code = languageCode(language);
        if (!m_applying && !code.isEmpty())
            emit changedByUser({ { QStringLiteral("audio-language"), code } });
    });
    connect(m_player->getSubtitleTracks(), &TrackListModel::picked, this, [this](const QString& language) {
        const QString code = languageCode(language);
        if (!m_applying && !code.isEmpty())
            emit changedByUser({ { QStringLiteral("sub-language"), code } });
    });
    connect(m_player->getSubtitleTracks(), &TrackListModel::turnedOff, this, [this]() {
        if (!m_applying)
            emit changedByUser({ { QStringLiteral("sub-language"), QStringLiteral("none") } });
    });

    // a new file: what the previous one waited for is over; the languages
    // to pick come with apply()
    connect(m_player, &PlayerController::inputChanged, this, [this]() {
        m_pickAudio.clear();
        m_pickSub.clear();
        m_ratePending = false;
    });
    connect(m_player->getAudioTracks(), &TrackListModel::countChanged, this, &PlayerPresetTarget::pickLanguages);
    connect(m_player->getSubtitleTracks(), &TrackListModel::countChanged, this, &PlayerPresetTarget::pickLanguages);
}

QString PlayerPresetTarget::languageCode(const QString& language)
{
    const QString trimmed = language.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("none"))
        return trimmed;
    const iso639_lang_t* lang = vlc_find_iso639(qtu(trimmed), true);
    if (!lang)
        return trimmed.toLower();
    if (lang->psz_iso639_1[0])
        return QString::fromLatin1(lang->psz_iso639_1);
    return QString::fromLatin1(lang->psz_iso639_2T);
}

QVariantMap PlayerPresetTarget::values() const
{
    QVariantMap values = audioValues(*m_player->getAudioAdjustments());
    values.insert(subtitleValues(*m_player->getSubtitleAppearance()));
    values.insert(videoValues(*m_player->getVideoAdjustments()));

    vlc_player_t* player = m_player->getPlayer();
    vlc_player_locker lock{ player };
    values.insert("audio-delay", int(MS_FROM_VLC_TICK(vlc_player_GetAudioDelay(player))));
    values.insert("sub-delay", int(MS_FROM_VLC_TICK(vlc_player_GetSubtitleDelay(player))));
    values.insert("rate", std::round(vlc_player_GetRate(player) * 100.) / 100.);
    return values;
}

QVariantMap PlayerPresetTarget::profileValues() const
{
    const AudioAdjustments audio(nullptr);
    const SubtitleAppearance sub(std::make_unique<ProfileSubtitleOptions>());
    const VideoAdjustments video(std::make_unique<NoVideoOutputs>());

    QVariantMap values = audioValues(audio);
    values.insert(subtitleValues(sub));
    values.insert(videoValues(video));
    values.insert("audio-delay", 0);
    values.insert("sub-delay", 0);
    values.insert("rate", 1.);
    return values;
}

QVariantMap PlayerPresetTarget::fileValues() const
{
    vlc_player_t* player = m_player->getPlayer();
    vlc_player_locker lock{ player };
    vlc_player_media_memory memory;
    if (vlc_player_GetMediaMemory(player, &memory) != VLC_SUCCESS)
        return {};

    QVariantMap values;
    if (memory.has_delays[AUDIO_ES])
        values.insert("audio-delay", int(MS_FROM_VLC_TICK(memory.delays[AUDIO_ES])));
    if (memory.has_delays[SPU_ES])
        values.insert("sub-delay", int(MS_FROM_VLC_TICK(memory.delays[SPU_ES])));
    return values;
}

bool PlayerPresetTarget::fileHasTracks(int category) const
{
    vlc_player_t* player = m_player->getPlayer();
    vlc_player_locker lock{ player };
    vlc_player_media_memory memory;
    return vlc_player_GetMediaMemory(player, &memory) == VLC_SUCCESS
        && memory.tracks[category];
}

void PlayerPresetTarget::setProfileWrites(bool enabled)
{
    m_profileWrites = enabled;
    m_player->getAudioAdjustments()->setProfileWrites(enabled);
    m_player->getSubtitleAppearance()->setProfileWrites(enabled);
}

void PlayerPresetTarget::apply(const QVariantMap& values)
{
    AudioAdjustments* audio = m_player->getAudioAdjustments();
    SubtitleAppearance* sub = m_player->getSubtitleAppearance();
    VideoAdjustments* video = m_player->getVideoAdjustments();
    vlc_player_t* player = m_player->getPlayer();
    const QVariantMap audioParameters = audio->values();

    // shown only: the Profile keeps its options
    audio->setProfileWrites(false);
    sub->setProfileWrites(false);
    m_applying = true;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        const QString& key = it.key();
        const QVariant& value = it.value();

        if (key == QLatin1String("audio-language"))
        {
            audio->setLanguage(value.toString());
            // this file too, once its tracks are known, unless it remembers
            // the viewer's own
            if (!fileHasTracks(AUDIO_ES))
                m_pickAudio = value.toString();
        }
        else if (key == QLatin1String("stereo-mode"))
            audio->setStereoMode(value.toInt());
        else if (key == QLatin1String("equalizer"))
        {
            const QStringList numbers = value.toString().split(' ', Qt::SkipEmptyParts);
            QList<double> bands;
            for (qsizetype i = 1; i < numbers.size(); ++i)
                bands.append(QLocale::c().toDouble(numbers[i]));
            if (numbers.size() == AudioAdjustments::BAND_COUNT + 1)
                audio->setEqualizer(true, QLocale::c().toDouble(numbers[0]), bands);
            else
                audio->setEqualizer(false, audio->preamp(), audio->bands());
        }
        else if (key == QLatin1String("equalizer-2pass"))
            audio->setTwoPass(value.toBool());
        else if (key == QLatin1String("compressor") || key == QLatin1String("spatializer")
                 || key == QLatin1String("stereo_widen"))
            audio->setFilterEnabled(key, value.toBool());
        else if (audioParameters.contains(key))
            audio->setValue(key, value.toDouble());
        else if (key == QLatin1String("sub-language"))
        {
            sub->setDefaultLanguage(value.toString());
            if (!fileHasTracks(SPU_ES))
                m_pickSub = value.toString();
        }
        else if (key == QLatin1String("sub-keep-styling"))
            sub->setKeepFileStyling(value.toBool());
        else if (key == QLatin1String("sub-font"))
            sub->setFont(value.toString());
        else if (key == QLatin1String("sub-size-mode"))
            sub->setSizeMode(SubtitleAppearance::SizeMode(value.toInt()));
        else if (key == QLatin1String("sub-relative-size"))
            sub->setRelativeSize(SubtitleAppearance::RelativeSize(value.toInt()));
        else if (key == QLatin1String("sub-pixel-size"))
            sub->setPixelSize(value.toInt());
        else if (key == QLatin1String("sub-text-color"))
            sub->setTextColor(QColor(value.toString()));
        else if (key == QLatin1String("sub-outline-color"))
            sub->setOutlineColor(QColor(value.toString()));
        else if (key == QLatin1String("sub-outline-thickness"))
            sub->setOutlineThickness(SubtitleAppearance::OutlineThickness(value.toInt()));
        else if (key == QLatin1String("sub-background-opacity"))
            sub->setBackgroundOpacity(value.toInt());
        else if (key == QLatin1String("sub-vertical-offset"))
            sub->setVerticalOffset(value.toInt());
        else if (key == QLatin1String("video-flip"))
            video->setFlip(VideoAdjustments::Flip(value.toInt()));
        else if (key == QLatin1String("video-rotation"))
            video->setRotation(value.toDouble());
        else if (key.startsWith(VIDEO_PREFIX))
            video->setValue(key.mid(VIDEO_PREFIX.size()), value);
        else if (key == QLatin1String("audio-delay") || key == QLatin1String("sub-delay")
                 || key == QLatin1String("rate"))
        {
            vlc_player_locker lock{ player };
            if (key == QLatin1String("rate"))
            {
                // kept for the next files too
                if (value.toDouble() > 0.)
                    vlc_player_ChangeRate(player, value.toFloat());
            }
            // not the file's own: only the viewer's later changes are
            else
                vlc_player_SetCategoryDefaultDelay(player,
                    key == QLatin1String("audio-delay") ? AUDIO_ES : SPU_ES,
                    VLC_TICK_FROM_MS(value.toLongLong()));
        }
    }
    m_applying = false;
    audio->setProfileWrites(m_profileWrites);
    sub->setProfileWrites(m_profileWrites);

    pickLanguages();
}

void PlayerPresetTarget::reportChanges(const QStringList& prefixes)
{
    if (m_applying)
        return;
    const QVariantMap changed = PresetController::keysMatching(values(), prefixes);
    if (!changed.isEmpty())
        emit changedByUser(changed);
}

void PlayerPresetTarget::pickLanguages()
{
    const auto pick = [this](TrackListModel* tracks, QString& wanted) {
        if (wanted.isEmpty() || tracks->getCount() == 0)
            return;
        if (wanted == QLatin1String("none"))
        {
            if (tracks->hasSelection())
                tracks->unselectAll();
            wanted.clear();
            return;
        }
        const QString code = languageCode(wanted);
        for (int row = 0; row < tracks->getCount(); ++row)
        {
            const QModelIndex index = tracks->index(row);
            if (languageCode(tracks->data(index, TrackListModel::LanguageRole).toString()) != code)
                continue;
            // a preference, not the file's own pick
            if (!tracks->data(index, Qt::CheckStateRole).toBool())
                tracks->selectAsDefault(row);
            wanted.clear();
            return;
        }
    };

    m_applying = true;
    pick(m_player->getAudioTracks(), m_pickAudio);
    pick(m_player->getSubtitleTracks(), m_pickSub);
    m_applying = false;
}
