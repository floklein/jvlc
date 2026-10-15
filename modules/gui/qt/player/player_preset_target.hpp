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
#ifndef QVLC_PLAYER_PRESET_TARGET_HPP
#define QVLC_PLAYER_PRESET_TARGET_HPP

#include "preset_controller.hpp"

class PlayerController;
class TrackListModel;

/**
 * What a Preset holds, on the player and the tabs of the Adjustments sidebar:
 * - "audio-language", "sub-language": the preferred languages;
 * - "audio-delay", "sub-delay": the delays of the file, in ms;
 * - "sub-…": the look of the subtitles (Subtitles tab);
 * - "stereo-mode", "equalizer" ("" when off, else the preamp then the 10
 *   bands), "equalizer-2pass", the filters ("compressor"…) and their
 *   parameters by option name (Audio tab);
 * - "video-flip", "video-rotation" and "video-<value>" (Video tab);
 * - "rate": the speed.
 *
 * A new file is opened with the preferred languages the player had: when a
 * Show brings other ones, the matching tracks are picked once listed.
 */
class PlayerPresetTarget : public PresetTarget
{
    Q_OBJECT

public:
    explicit PlayerPresetTarget(PlayerController* player, QObject* parent = nullptr);

    QVariantMap values() const override;
    QVariantMap profileValues() const override;
    void apply(const QVariantMap& values) override;
    void setProfileWrites(bool enabled) override;

    /// "jpn", "Japanese" or "ja" → "ja"
    static QString languageCode(const QString& language);

private:
    void reportChanges(const QStringList& prefixes);
    void pickLanguages();

    PlayerController* m_player;
    bool m_profileWrites = true;
    bool m_applying = false;
    bool m_ratePending = false;

    // the languages to pick once the tracks are listed
    QString m_pickAudio;
    QString m_pickSub;
};

#endif // QVLC_PLAYER_PRESET_TARGET_HPP
