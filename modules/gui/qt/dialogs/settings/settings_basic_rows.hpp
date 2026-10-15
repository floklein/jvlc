/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_SETTINGS_BASIC_ROWS_HPP
#define VLC_QT_SETTINGS_BASIC_ROWS_HPP

#include <functional>

class SettingsModel;

/**
 * The basic rows of the Settings pages that hold VLC options and the
 * homes of the Profiles, Presets, library folders, service sources and
 * add-ons: Privacy, Controls (mouse and remote), Playback, Video, Audio,
 * Subtitles, Library, Network, Performance, Recording and Extensions, and
 * the Profiles of General.
 *
 * Each VLC option is in one row only. The lists (Profiles, Presets...) are
 * Panel rows, which the window draws with their own models.
 */
namespace SettingsBasicRows
{

// what the rows need from the running interface
struct Hooks
{
    /* the options VLC reads when it sets up its Queue (repeat, start
     * paused...) changed: apply them to the running Queue */
    std::function<void()> applyQueueOptions;
    // the media library runs: library folders, Clear Recent
    std::function<bool()> hasMediaLibrary;
    // hides everything played so far from Recent
    std::function<void()> clearRecent;
    // the subtitle look changed: show it on the playing file
    std::function<void()> applySubtitleLook;
};

void add(SettingsModel& model, const Hooks& hooks = {});

}

#endif // VLC_QT_SETTINGS_BASIC_ROWS_HPP
