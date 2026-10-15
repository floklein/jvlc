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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "settings_basic_rows.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_modules.h>

#include "settings_model.hpp"

#include <algorithm>
#include <cstring>

namespace {

using Row = SettingsModel::Row;
QVariantMap choice(const QVariant& value, const QString& label)
{
    return SettingsModel::choice(value, label);
}

Row makeRow(const char* id, const char* page, const QString& section, const QString& label,
            SettingsModel::Control control = SettingsModel::Toggle)
{
    Row row;
    row.id = QString::fromLatin1(id);
    row.page = QString::fromLatin1(page);
    row.section = section;
    row.label = label;
    row.control = control;
    return row;
}

bool optionExists(const char* name)
{
    return config_GetType(name) != 0;
}

bool configBool(const char* name)
{
    return optionExists(name) && config_GetInt(name) != 0;
}

bool defaultBool(const char* name)
{
    const module_config_t* item = config_FindConfig(name);
    return item && item->orig.i != 0;
}

int64_t configInt(const char* name)
{
    return optionExists(name) ? config_GetInt(name) : 0;
}

int64_t defaultInt(const char* name)
{
    const module_config_t* item = config_FindConfig(name);
    return item ? item->orig.i : 0;
}

void putInt(const char* name, int64_t value)
{
    if (optionExists(name))
        config_PutInt(name, value);
}

// the choices VLC declares for the option, read when the row shows
void declaredChoices(Row& row, const char* option)
{
    const QString name = QString::fromLatin1(option);
    row.choicesProvider = [name]() {
        QVariantList choices = SettingsModel::configChoices(name);
        /* a text option left empty lets VLC choose (x264 presets...): say
         * so rather than show a blank choice */
        const QByteArray utf8 = name.toUtf8();
        if (!choices.isEmpty() && config_GetType(utf8.constData()) == VLC_VAR_STRING)
        {
            const bool listed = std::any_of(choices.cbegin(), choices.cend(), [](const QVariant& entry) {
                return entry.toMap().value(QStringLiteral("value")).toString().isEmpty();
            });
            if (!listed)
                choices.prepend(choice(QString(), qtr("Default")));
        }
        return choices;
    };
}

// the row asks the opposite of the option ("Seek precisely" = not fast seek)
void invert(Row& row)
{
    row.read = [read = row.read]() {
        const QVariant value = read();
        return value.isValid() ? QVariant(!value.toBool()) : value;
    };
    row.readDefault = [readDefault = row.readDefault]() {
        const QVariant value = readDefault();
        return value.isValid() ? QVariant(!value.toBool()) : value;
    };
    row.write = [write = row.write](const QVariant& value) { return write(!value.toBool()); };
}

// the row also applies to the running Queue
void applyToQueue(Row& row, const SettingsBasicRows::Hooks& hooks)
{
    row.effect = SettingsModel::Immediate;
    if (!hooks.applyQueueOptions)
        return;
    row.write = [write = row.write, apply = hooks.applyQueueOptions](const QVariant& value) {
        if (!write(value))
            return false;
        apply();
        return true;
    };
}

/* the row changes the subtitle look, which the player redraws at once (as
 * the Subtitles tab of the sidebar does) */
void showOnPlayingFile(Row& row, const SettingsBasicRows::Hooks& hooks)
{
    if (!hooks.applySubtitleLook)
        return;
    row.effect = SettingsModel::Immediate;
    row.write = [write = row.write, apply = hooks.applySubtitleLook](const QVariant& value) {
        if (!write(value))
            return false;
        apply();
        return true;
    };
}

/* a toggle loading an interface module with VLC ("audioscrobbler" for
 * last.fm, "http" for the web remote) */
void bindInterface(Row& row, const char* intf, const char* module)
{
    const QByteArray name(intf);
    const QByteArray owner(module);
    row.isAvailable = [owner]() { return module_exists(owner.constData()); };
    row.read = [name]() { return QVariant(config_ExistIntf(name.constData())); };
    row.readDefault = []() { return QVariant(false); };
    row.write = [name](const QVariant& value) {
        if (value.toBool())
            config_AddIntf(name.constData());
        else
            config_RemoveIntf(name.constData());
        return true;
    };
    row.savesConfig = true;
    row.effect = SettingsModel::Restart;
}

Row panel(const char* id, const char* page, const QString& section, const QString& label,
          const char* list)
{
    Row row = makeRow(id, page, section, label, SettingsModel::Panel);
    row.panel = QString::fromLatin1(list);
    return row;
}

using SettingsBasicRows::Hooks;

void addGeneralRows(SettingsModel& model)
{
    Row row = panel("profiles", "general", qtr("Profiles"), qtr("Your profiles"), "profiles");
    row.help = qtr("Each Profile keeps its own settings, Theme, Accent color, scale and Layouts. "
                   "Switch Profile from the VLC menu. The Default Profile can't be deleted.");
    model.addRow(std::move(row));
}

void addPrivacyRows(SettingsModel& model, const Hooks& hooks)
{
    const QString history = qtr("History");

    {
        Row row = makeRow("save-recentplay", "privacy", history, qtr("Keep a list of recently played items"));
        row.help = qtr("Recent and Open Recent list what you played. When off, VLC doesn't remember it.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("save-recentplay") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("clear-recent", "privacy", history, qtr("Clear Recent"), SettingsModel::Action);
        row.help = qtr("Hides everything played so far from Recent. Your library and the playback "
                       "positions are kept.");
        row.buttonText = qtr("Clear…");
        row.confirmText = qtr("Clear Recent");
        const auto hasLibrary = hooks.hasMediaLibrary;
        row.isAvailable = [hasLibrary]() { return hasLibrary && hasLibrary(); };
        row.action = hooks.clearRecent;
        model.addRow(std::move(row));
    }

    const QString lastfm = QStringLiteral("Last.fm");

    {
        Row row = makeRow("lastfm", "privacy", lastfm, qtr("Submit played tracks to Last.fm"));
        row.help = qtr("Tells Last.fm what you listen to (scrobbling), with the account below.");
        bindInterface(row, "audioscrobbler", "audioscrobbler");
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("lastfm-username", "privacy", lastfm, qtr("Last.fm username"), SettingsModel::Text);
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("lastfm-username") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("lastfm-password", "privacy", lastfm, qtr("Last.fm password"), SettingsModel::Password);
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("lastfm-password") });
        model.addRow(std::move(row));
    }
}

void addControlsRows(SettingsModel& model)
{
    /* Jumps */
    const QString jumps = qtr("Jumps");
    const struct
    {
        const char* option;
        const char* label;
    } jumpRows[] = {
        { "extrashort-jump-size", N_("Very short jump") },
        { "short-jump-size", N_("Short jump") },
        { "medium-jump-size", N_("Medium jump") },
        { "long-jump-size", N_("Long jump") },
    };
    for (const auto& jump : jumpRows)
    {
        Row row = makeRow(jump.option, "controls", jumps, qtr(jump.label), SettingsModel::Number);
        row.help = qtr("How far the jump keys and the skip buttons go, in seconds.");
        row.unit = qtr("s");
        row.minimum = 1.;
        row.maximum = 3600.;
        model.bindConfig(row, { QString::fromLatin1(jump.option) });
        model.addRow(std::move(row));
    }

    /* Mouse */
    const QString mouse = qtr("Mouse");

    {
        Row row = makeRow("wheel-vertical", "controls", mouse, qtr("Scroll wheel"), SettingsModel::Choice);
        row.help = qtr("What scrolling up and down over the video does.");
        declaredChoices(row, "hotkeys-y-wheel-mode");
        model.bindConfig(row, { QStringLiteral("hotkeys-y-wheel-mode") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("wheel-horizontal", "controls", mouse, qtr("Horizontal scroll"), SettingsModel::Choice);
        row.help = qtr("What scrolling left and right over the video does.");
        declaredChoices(row, "hotkeys-x-wheel-mode");
        model.bindConfig(row, { QStringLiteral("hotkeys-x-wheel-mode") });
        model.addRow(std::move(row));
    }

    /* Web remote control: the Lua HTTP interface */
    const QString remote = qtr("Web remote control");

    {
        Row row = makeRow("web-remote", "controls", remote, qtr("Control VLC from a web browser"));
        row.help = qtr("Starts VLC's web interface, for a phone or another computer on your network. "
                       "It needs a password.");
        bindInterface(row, "http", "lua");
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("web-remote-password", "controls", remote, qtr("Password"), SettingsModel::Password);
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("http-password") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("web-remote-port", "controls", remote, qtr("Port"), SettingsModel::Number);
        row.help = qtr("The web interface's address is http://this-computer:port.");
        row.effect = SettingsModel::Restart;
        row.minimum = 1.;
        row.maximum = 65535.;
        model.bindConfig(row, { QStringLiteral("http-port") });
        model.addRow(std::move(row));
    }
}

void addPlaybackRows(SettingsModel& model, const Hooks& hooks)
{
    /* Resume */
    const QString resume = qtr("Resume");

    {
        Row row = makeRow("restore-playback-states", "playback", resume, qtr("Restore playback states"));
        row.help = qtr("Each file remembers its tracks, subtitle and audio delays, and other "
                       "adjustments, and gets them back when it plays again.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("restore-playback-states") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("continue-playback", "playback", resume, qtr("Continue where you left off"),
                          SettingsModel::Choice);
        row.help = qtr("Whether a file starts again where you stopped it.");
        row.effect = SettingsModel::NextFile;
        row.choices = {
            choice(0, qtr("Never")),
            choice(1, qtr("Ask")),
            choice(2, qtr("Always")),
        };
        model.bindConfig(row, { QStringLiteral("restore-playback-pos") });
        model.addRow(std::move(row));
    }

    /* Defaults */
    const QString defaults = qtr("Defaults");

    {
        Row row = makeRow("start-playing", "playback", defaults, qtr("Start playing as soon as a file opens"));
        row.help = qtr("When off, files open paused on their first frame.");
        model.bindConfig(row, { QStringLiteral("start-paused") });
        invert(row);
        applyToQueue(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("rate", "playback", defaults, qtr("Default playback speed"), SettingsModel::Choice);
        row.help = qtr("The speed files start at.");
        row.effect = SettingsModel::NextFile;
        for (const double rate : { 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0 })
            row.choices.append(choice(rate, rate == 1.0 ? qtr("Normal") : qtr("%1×").arg(rate)));
        model.bindConfig(row, { QStringLiteral("rate") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("audio-language", "playback", defaults, qtr("Preferred audio language"),
                          SettingsModel::Text);
        row.help = qtr("Language codes in order of preference, separated by commas, such as “fr,en”.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("audio-language") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("menu-language", "playback", defaults, qtr("Preferred disc menu language"),
                          SettingsModel::Text);
        row.help = qtr("For DVD and Blu-ray menus, such as “en”.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("menu-language") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("preferred-resolution", "playback", defaults, qtr("Preferred video quality"),
                          SettingsModel::Choice);
        row.help = qtr("For streams offered in several qualities.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "preferred-resolution");
        model.bindConfig(row, { QStringLiteral("preferred-resolution") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("seek-precisely", "playback", defaults, qtr("Seek precisely"));
        row.help = qtr("Jumps land on the exact time. When off, they land on the nearest key frame, "
                       "which is faster.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("input-fast-seek") });
        invert(row);
        model.addRow(std::move(row));
    }

    /* The Queue */
    const QString queue = qtr("Queue");

    {
        Row row = makeRow("repeat", "playback", queue, qtr("Repeat"), SettingsModel::Segmented);
        row.help = qtr("How VLC starts, also set by the Repeat button.");
        row.choices = {
            choice(QStringLiteral("off"), qtr("Off")),
            choice(QStringLiteral("all"), qtr("All")),
            choice(QStringLiteral("one"), qtr("One")),
        };
        model.bindConfig(row, { QStringLiteral("loop"), QStringLiteral("repeat") });
        row.read = []() {
            return QVariant(configBool("repeat") ? QStringLiteral("one")
                            : configBool("loop") ? QStringLiteral("all") : QStringLiteral("off"));
        };
        row.readDefault = []() {
            return QVariant(defaultBool("repeat") ? QStringLiteral("one")
                            : defaultBool("loop") ? QStringLiteral("all") : QStringLiteral("off"));
        };
        row.write = [](const QVariant& value) {
            const QString mode = value.toString();
            putInt("loop", mode == QLatin1String("all"));
            putInt("repeat", mode == QLatin1String("one"));
            return true;
        };
        applyToQueue(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("random", "playback", queue, qtr("Shuffle"));
        row.help = qtr("How VLC starts, also set by the Shuffle button.");
        model.bindConfig(row, { QStringLiteral("random") });
        applyToQueue(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("after-each-file", "playback", queue, qtr("After each file"), SettingsModel::Choice);
        row.help = qtr("What happens when a file ends.");
        row.choices = {
            choice(QStringLiteral("next"), qtr("Play the next one")),
            choice(QStringLiteral("stop"), qtr("Stop")),
            choice(QStringLiteral("pause"), qtr("Pause on the last frame")),
        };
        model.bindConfig(row, { QStringLiteral("play-and-stop"), QStringLiteral("play-and-pause") });
        row.read = []() {
            return QVariant(configBool("play-and-stop") ? QStringLiteral("stop")
                            : configBool("play-and-pause") ? QStringLiteral("pause") : QStringLiteral("next"));
        };
        row.readDefault = []() {
            return QVariant(defaultBool("play-and-stop") ? QStringLiteral("stop")
                            : defaultBool("play-and-pause") ? QStringLiteral("pause") : QStringLiteral("next"));
        };
        row.write = [](const QVariant& value) {
            const QString mode = value.toString();
            putInt("play-and-stop", mode == QLatin1String("stop"));
            putInt("play-and-pause", mode == QLatin1String("pause"));
            return true;
        };
        applyToQueue(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("play-and-exit", "playback", queue, qtr("Quit VLC when the Queue ends"));
        model.bindConfig(row, { QStringLiteral("play-and-exit") });
        applyToQueue(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("playlist-cork", "playback", queue, qtr("Pause when another app uses audio"));
        row.help = qtr("Such as a call. Playback goes on when it ends.");
        model.bindConfig(row, { QStringLiteral("playlist-cork") });
        applyToQueue(row, hooks);
        model.addRow(std::move(row));
    }

    /* Presets */
    {
        Row row = panel("presets", "playback", qtr("Presets"), qtr("Your Presets"), "presets");
        row.help = qtr("A Preset keeps subtitle, audio and video adjustments for the folders "
                       "(Shows) it is attached to. Create one from the Adjustments sidebar.");
        row.allProfiles = true;
        model.addRow(std::move(row));
    }

    /* Discs and files */
    const QString discs = qtr("Discs and files");

    {
        Row row = makeRow("optical-drive", "playback", discs, qtr("Default optical drive"), SettingsModel::Text);
        row.help = qtr("The drive Open Disc uses first for DVDs, VCDs and audio CDs.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("dvd"), QStringLiteral("vcd"), QStringLiteral("cd-audio") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("avi-index", "playback", discs, qtr("Repair damaged AVI files"), SettingsModel::Choice);
        row.help = qtr("Rebuilds the index of a broken or incomplete AVI file, so that you can seek in it.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "avi-index");
        model.bindConfig(row, { QStringLiteral("avi-index") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("mkv-preload-local-dir", "playback", discs, qtr("Load matching .mkv files"));
        row.help = qtr("Plays the linked segments of a Matroska file found in its folder.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("mkv-preload-local-dir") });
        model.addRow(std::move(row));
    }
}

void addVideoRows(SettingsModel& model)
{
    const QString display = qtr("Display");

    {
        Row row = makeRow("video", "video", display, qtr("Enable video"));
        row.help = qtr("When off, VLC plays only the sound of videos.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("video") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("fullscreen", "video", display, qtr("Start videos in fullscreen"));
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("fullscreen") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("disable-screensaver", "video", display, qtr("Keep the screen on during videos"));
        row.help = qtr("Stops the screen saver and screen sleep while a video plays.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("disable-screensaver") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("video-wallpaper", "video", display, qtr("Play videos as the desktop wallpaper"));
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("video-wallpaper") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("autoscale", "video", display, qtr("Scale videos to the window"));
        row.help = qtr("When off, videos show at their own size.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("autoscale") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("deinterlace", "video", display, qtr("Deinterlacing"), SettingsModel::Choice);
        row.help = qtr("Removes the comb lines of interlaced videos, such as TV recordings.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "deinterlace");
        model.bindConfig(row, { QStringLiteral("deinterlace") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("deinterlace-mode", "video", display, qtr("Deinterlacing method"), SettingsModel::Choice);
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "deinterlace-mode");
        model.bindConfig(row, { QStringLiteral("deinterlace-mode") });
        model.addRow(std::move(row));
    }

    /* Snapshots: read when one is taken */
    const QString snapshots = qtr("Snapshots");

    {
        Row row = makeRow("snapshot-path", "video", snapshots, qtr("Save snapshots to"), SettingsModel::Directory);
        row.help = qtr("When empty, snapshots go to your Pictures folder.");
        model.bindConfig(row, { QStringLiteral("snapshot-path") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("snapshot-prefix", "video", snapshots, qtr("File name prefix"), SettingsModel::Text);
        model.bindConfig(row, { QStringLiteral("snapshot-prefix") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("snapshot-format", "video", snapshots, qtr("Format"), SettingsModel::Choice);
        declaredChoices(row, "snapshot-format");
        model.bindConfig(row, { QStringLiteral("snapshot-format") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("snapshot-sequential", "video", snapshots, qtr("Number snapshots in sequence"));
        row.help = qtr("Names them 00001, 00002… instead of with the time.");
        model.bindConfig(row, { QStringLiteral("snapshot-sequential") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("snapshot-preview", "video", snapshots, qtr("Show a preview of the snapshot"));
        model.bindConfig(row, { QStringLiteral("snapshot-preview") });
        model.addRow(std::move(row));
    }
}

void addAudioRows(SettingsModel& model)
{
    const QString output = qtr("Output");

    {
        Row row = makeRow("audio", "audio", output, qtr("Enable audio"));
        row.help = qtr("When off, VLC plays videos without sound.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("audio") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("aout", "audio", output, qtr("Output module"), SettingsModel::Choice);
        row.help = qtr("How VLC sends sound to your system. Its device is under Advanced Options.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "aout");
        model.bindConfig(row, { QStringLiteral("aout") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("stereo-mode", "audio", output, qtr("Stereo mode"), SettingsModel::Choice);
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "stereo-mode");
        model.bindConfig(row, { QStringLiteral("stereo-mode") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("mix-mode", "audio", output, qtr("Speaker setup"), SettingsModel::Choice);
        row.help = qtr("How many channels your speakers or headphones play.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "mix-mode");
        model.bindConfig(row, { QStringLiteral("mix-mode") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("spdif", "audio", output, qtr("Pass compressed audio to the receiver"));
        row.help = qtr("Sends Dolby and DTS sound as is over S/PDIF or HDMI, for a receiver to decode.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("spdif") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("force-dolby-surround", "audio", output, qtr("Detect Dolby Surround"),
                          SettingsModel::Choice);
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "force-dolby-surround");
        model.bindConfig(row, { QStringLiteral("force-dolby-surround") });
        model.addRow(std::move(row));
    }

    /* Volume */
    const QString volume = qtr("Volume");

    {
        Row row = makeRow("max-volume", "audio", volume, qtr("Maximum volume"), SettingsModel::Slider);
        row.help = qtr("Above 100%, sound is amplified and may distort.");
        row.effect = SettingsModel::Immediate;
        row.minimum = 100.;
        row.maximum = 300.;
        row.step = 25.;
        row.unit = QStringLiteral("%");
        model.bindConfig(row, { QStringLiteral("qt-max-volume") });
        model.addRow(std::move(row));
    }

    {
        // the volume goes from 0 to 256 for 100%
        Row row = makeRow("volume-step", "audio", volume, qtr("Volume step"), SettingsModel::Slider);
        row.help = qtr("How much the volume keys and the mouse wheel change the volume.");
        row.minimum = 2.56;
        row.maximum = 51.2;
        row.step = 2.56;
        row.scale = 100. / 256.;
        row.unit = QStringLiteral("%");
        model.bindConfig(row, { QStringLiteral("volume-step") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("volume-save", "audio", volume, qtr("Remember the volume"));
        row.help = qtr("VLC starts at the volume you left it.");
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("volume-save") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("normalize-volume", "audio", volume, qtr("Normalize loudness"));
        row.help = qtr("Evens out loud and quiet files.");
        model.bindFilter(row, QStringLiteral("audio-filter"), QStringLiteral("normvol"));
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("replay-gain", "audio", volume, qtr("Replay gain"), SettingsModel::Choice);
        row.help = qtr("Uses the loudness tags of music files to play them at the same level.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "audio-replay-gain-mode");
        model.bindConfig(row, { QStringLiteral("audio-replay-gain-mode") });
        model.addRow(std::move(row));
    }

    /* Visualizations */
    {
        Row row = makeRow("audio-visual", "audio", qtr("Visualizations"), qtr("Visualization"),
                          SettingsModel::Choice);
        row.help = qtr("What the player shows while music plays.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "audio-visual");
        model.bindConfig(row, { QStringLiteral("audio-visual") });
        model.addRow(std::move(row));
    }
}

void addSubtitlesRows(SettingsModel& model, const Hooks& hooks)
{
    /* Languages and files */
    const QString languages = qtr("Languages and files");

    {
        Row row = makeRow("spu", "subtitles", languages, qtr("Show subtitles"));
        row.help = qtr("When off, files start without subtitles. The Track menu still turns them on.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("spu") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("sub-language", "subtitles", languages, qtr("Preferred subtitle language"),
                          SettingsModel::Text);
        row.help = qtr("Language codes in order of preference, separated by commas, such as “fr,en”.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("sub-language") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("subtitle-autodetect", "subtitles", languages,
                          qtr("Load subtitle files automatically"), SettingsModel::Choice);
        row.help = qtr("Which subtitle files next to a video are loaded with it.");
        row.effect = SettingsModel::NextFile;
        row.choices = {
            choice(0, qtr("Off")),
            choice(1, qtr("Any subtitle file in the folder")),
            choice(2, qtr("Files containing the video's name")),
            choice(3, qtr("Video name plus extra characters")),
            choice(4, qtr("Exact video name")),
        };
        model.bindConfig(row, { QStringLiteral("sub-autodetect-fuzzy"), QStringLiteral("sub-autodetect-file") });
        row.read = []() {
            return QVariant(configBool("sub-autodetect-file") ? int(configInt("sub-autodetect-fuzzy")) : 0);
        };
        row.readDefault = []() {
            return QVariant(defaultBool("sub-autodetect-file") ? int(defaultInt("sub-autodetect-fuzzy")) : 0);
        };
        row.write = [](const QVariant& value) {
            const int level = value.toInt();
            putInt("sub-autodetect-file", level > 0);
            if (level > 0)
                putInt("sub-autodetect-fuzzy", level);
            return true;
        };
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("sub-autodetect-path", "subtitles", languages,
                          qtr("Also look for subtitles in these folders"), SettingsModel::Text);
        row.help = qtr("Folders next to the video, separated by commas, such as “./Subtitles”.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("sub-autodetect-path") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("subsdec-encoding", "subtitles", languages, qtr("Text encoding"), SettingsModel::Choice);
        row.help = qtr("For subtitle files whose letters show wrong.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "subsdec-encoding");
        model.bindConfig(row, { QStringLiteral("subsdec-encoding") });
        model.addRow(std::move(row));
    }

    /* Appearance, shared with the Subtitles tab of the sidebar */
    const QString appearance = qtr("Appearance");

    {
        Row row = makeRow("sub-text-scale", "subtitles", appearance, qtr("Text size"), SettingsModel::Slider);
        row.minimum = 25.;
        row.maximum = 300.;
        row.step = 5.;
        row.unit = QStringLiteral("%");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("sub-text-scale") });
        showOnPlayingFile(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("freetype-font", "subtitles", appearance, qtr("Font"), SettingsModel::Text);
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("freetype-font") });
        showOnPlayingFile(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("freetype-bold", "subtitles", appearance, qtr("Bold"));
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("freetype-bold") });
        model.addRow(std::move(row));
    }

    const struct
    {
        const char* option;
        const char* label;
        SettingsModel::Control control;
    } lookRows[] = {
        { "freetype-color", N_("Text color"), SettingsModel::Color },
        { "freetype-opacity", N_("Text opacity"), SettingsModel::Slider },
        { "freetype-outline-color", N_("Outline color"), SettingsModel::Color },
        { "freetype-outline-thickness", N_("Outline thickness"), SettingsModel::Number },
        { "freetype-background-color", N_("Background color"), SettingsModel::Color },
        { "freetype-background-opacity", N_("Background opacity"), SettingsModel::Slider },
        { "freetype-shadow-color", N_("Shadow color"), SettingsModel::Color },
        { "freetype-shadow-opacity", N_("Shadow opacity"), SettingsModel::Slider },
    };
    for (const auto& look : lookRows)
    {
        Row row = makeRow(look.option, "subtitles", appearance, qtr(look.label), look.control);
        row.effect = SettingsModel::NextFile;
        if (look.control == SettingsModel::Slider)
        {
            // opacities go from 0 to 255
            row.minimum = 0.;
            row.maximum = 255.;
            row.step = 255. / 20.;
            row.scale = 100. / 255.;
            row.unit = QStringLiteral("%");
        }
        else if (look.control == SettingsModel::Number)
        {
            row.minimum = 0.;
            row.maximum = 50.;
        }
        model.bindConfig(row, { QString::fromLatin1(look.option) });
        showOnPlayingFile(row, hooks);
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("sub-margin", "subtitles", appearance, qtr("Distance from the bottom"),
                          SettingsModel::Number);
        row.help = qtr("Raises the subtitles by this many pixels.");
        row.unit = qtr("px");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("sub-margin") });
        showOnPlayingFile(row, hooks);
        model.addRow(std::move(row));
    }

    /* On-screen messages, next to the Shortcut message row */
    const QString osd = qtr("On-screen messages");

    {
        Row row = makeRow("osd", "subtitles", osd, qtr("Show on-screen messages"));
        row.help = qtr("Volume, position and other feedback drawn over the video.");
        model.bindConfig(row, { QStringLiteral("osd") });
        model.addRow(std::move(row));
    }

    {
        // the Shortcut message
        Row row = makeRow("osd-shortcut-keys", "subtitles", osd, qtr("Show shortcut keys in on-screen messages"));
        row.help = qtr("The message shown after an action also names the key that ran it.");
        model.bindConfig(row, { QStringLiteral("qt-osd-shortcut-keys") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("video-title-show", "subtitles", osd, qtr("Show the file name when a video starts"));
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("video-title-show") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("video-title-timeout", "subtitles", osd, qtr("Show it for"), SettingsModel::Slider);
        row.minimum = 1000.;
        row.maximum = 20000.;
        row.step = 500.;
        row.scale = 0.001;
        row.unit = qtr("s");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("video-title-timeout") });
        model.addRow(std::move(row));
    }
}

void addLibraryRows(SettingsModel& model, const Hooks& hooks)
{
    {
        Row row = panel("library-folders", "library", qtr("Library"), qtr("Library folders"), "library-folders");
        row.help = qtr("The folders VLC watches for videos and music. Excluded folders inside them "
                       "are left out. Rescan looks for new and removed files.");
        row.allProfiles = true;
        const auto hasLibrary = hooks.hasMediaLibrary;
        row.isAvailable = [hasLibrary]() { return hasLibrary && hasLibrary(); };
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("fetch-metadata", "library", qtr("Library"), qtr("Fetch titles and artwork from the internet"));
        row.help = qtr("Looks up missing titles, covers and artist pictures online.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("metadata-network-access") });
        model.addRow(std::move(row));
    }

    /* Opening files */
    const QString opening = qtr("Opening files");

    {
        Row row = makeRow("play-adds-to-queue", "library", opening, qtr("Add to the Queue instead of replacing it"));
        row.help = qtr("Play adds the item at the end of the Queue and jumps to it.");
        model.bindConfig(row, { QStringLiteral("qt-play-adds-to-queue") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("one-instance", "library", opening, qtr("Use only one VLC window"));
        row.help = qtr("Files opened from elsewhere go to the VLC already open.");
        model.bindConfig(row, { QStringLiteral("one-instance") });
        row.effect = SettingsModel::Restart;
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("one-instance-from-file", "library", opening,
                          qtr("Use only one VLC window for files opened from the file manager"));
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("one-instance-when-started-from-file") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("playlist-enqueue", "library", opening,
                          qtr("Add files opened from the file manager to the Queue"));
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("playlist-enqueue") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("recursive", "library", opening, qtr("When opening a folder"), SettingsModel::Choice);
        row.help = qtr("Whether its subfolders are added too.");
        row.choices = {
            choice(QStringLiteral("none"), qtr("Add its files only")),
            choice(QStringLiteral("collapse"), qtr("Add its subfolders, collapsed")),
            choice(QStringLiteral("expand"), qtr("Add its subfolders, expanded")),
        };
        model.bindConfig(row, { QStringLiteral("recursive") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("show-hiddenfiles", "library", opening, qtr("Show hidden files"));
        model.bindConfig(row, { QStringLiteral("show-hiddenfiles") });
        model.addRow(std::move(row));
    }
}

/* VLC's caching presets: a value for local files, discs and live sources,
 * three times more for network streams */
const int CACHING_DEFAULT = -1;
const int CACHING_CUSTOM = 0;

int cachingPreset(int64_t file, int64_t network, int64_t disc, int64_t live)
{
    for (const int preset : { 100, 200, 300, 500, 1000 })
        if (file == preset && disc == preset && live == preset && network == 3 * preset)
            return preset;
    return CACHING_CUSTOM;
}

void addNetworkRows(SettingsModel& model)
{
    const QString buffering = qtr("Buffering");

    {
        Row row = makeRow("buffering", "network", buffering, qtr("Buffering"), SettingsModel::Choice);
        row.help = qtr("More buffering plays more smoothly on slow connections, but starts later.");
        row.effect = SettingsModel::NextFile;
        row.choices = {
            choice(CACHING_DEFAULT, qtr("VLC's values")),
            choice(CACHING_CUSTOM, qtr("Custom")),
            choice(100, qtr("Lowest latency")),
            choice(200, qtr("Low latency")),
            choice(300, qtr("Normal")),
            choice(500, qtr("High latency")),
            choice(1000, qtr("Higher latency")),
        };
        // the four caching rows keep their own options
        row.isAvailable = []() { return optionExists("file-caching"); };
        row.read = []() {
            const bool defaults = configInt("file-caching") == defaultInt("file-caching")
                && configInt("network-caching") == defaultInt("network-caching")
                && configInt("disc-caching") == defaultInt("disc-caching")
                && configInt("live-caching") == defaultInt("live-caching");
            if (defaults)
                return QVariant(CACHING_DEFAULT);
            return QVariant(cachingPreset(configInt("file-caching"), configInt("network-caching"),
                                          configInt("disc-caching"), configInt("live-caching")));
        };
        row.readDefault = []() { return QVariant(CACHING_DEFAULT); };
        row.write = [](const QVariant& value) {
            const int preset = value.toInt();
            if (preset == CACHING_CUSTOM)
                return true;
            for (const char* name : { "file-caching", "network-caching", "disc-caching", "live-caching" })
            {
                int64_t ms = preset;
                if (preset == CACHING_DEFAULT)
                    ms = defaultInt(name);
                else if (!strcmp(name, "network-caching"))
                    ms = 3 * preset;
                putInt(name, ms);
            }
            return true;
        };
        row.savesConfig = true;
        model.addRow(std::move(row));
    }

    const struct
    {
        const char* option;
        const char* label;
    } cachingRows[] = {
        { "file-caching", N_("Local files") },
        { "disc-caching", N_("Discs") },
        { "network-caching", N_("Network streams") },
        { "live-caching", N_("Cameras and live sources") },
    };
    for (const auto& caching : cachingRows)
    {
        Row row = makeRow(caching.option, "network", buffering, qtr(caching.label), SettingsModel::Number);
        row.unit = qtr("ms");
        row.minimum = 0.;
        row.maximum = 60000.;
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QString::fromLatin1(caching.option) });
        model.addRow(std::move(row));
    }

    const QString proxy = qtr("Proxy");

    {
        Row row = makeRow("http-proxy", "network", proxy, qtr("HTTP proxy"), SettingsModel::Text);
        row.help = qtr("Such as http://proxy.example.com:3128/. When empty, VLC connects directly.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("http-proxy") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("http-proxy-pwd", "network", proxy, qtr("Proxy password"), SettingsModel::Password);
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("http-proxy-pwd") });
        model.addRow(std::move(row));
    }

    const QString security = qtr("Security");

    {
        Row row = makeRow("gnutls-system-trust", "network", security,
                          qtr("Trust the certificates your system trusts"));
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("gnutls-system-trust") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("gnutls-priorities", "network", security, qtr("Encryption"), SettingsModel::Choice);
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "gnutls-priorities");
        model.bindConfig(row, { QStringLiteral("gnutls-priorities") });
        model.addRow(std::move(row));
    }
}

void addPerformanceRows(SettingsModel& model)
{
    const QString decoding = qtr("Decoding");

    {
        Row row = makeRow("dec-dev", "performance", decoding, qtr("Hardware-accelerated decoding"),
                          SettingsModel::Choice);
        row.help = qtr("Lets the graphics card decode videos, which saves power.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "dec-dev");
        model.bindConfig(row, { QStringLiteral("dec-dev") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("avcodec-threads", "performance", decoding, qtr("Decoding threads"),
                          SettingsModel::Number);
        row.help = qtr("0 lets VLC choose.");
        row.effect = SettingsModel::NextFile;
        row.minimum = 0.;
        row.maximum = 64.;
        model.bindConfig(row, { QStringLiteral("avcodec-threads") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("avcodec-skiploopfilter", "performance", decoding,
                          qtr("Skip the H.264 loop filter"), SettingsModel::Choice);
        row.help = qtr("Saves processing power on slow computers, at some cost in picture quality.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "avcodec-skiploopfilter");
        model.bindConfig(row, { QStringLiteral("avcodec-skiploopfilter") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("postproc-q", "performance", decoding, qtr("Video post-processing"),
                          SettingsModel::Slider);
        row.help = qtr("Hides compression artifacts. Higher levels need more processing power.");
        row.effect = SettingsModel::NextFile;
        row.minimum = 0.;
        row.maximum = 6.;
        model.bindConfig(row, { QStringLiteral("postproc-q") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("drop-late-frames", "performance", decoding, qtr("Drop frames when late"));
        row.help = qtr("Keeps the picture in time with the sound on slow computers.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("drop-late-frames") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("high-priority", "performance", decoding, qtr("Run VLC at a higher priority"));
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("high-priority") });
        model.addRow(std::move(row));
    }

    const QString timeshift = qtr("Time-shift");

    {
        Row row = makeRow("timeshift-path", "performance", timeshift, qtr("Time-shift folder"),
                          SettingsModel::Directory);
        row.help = qtr("Where VLC keeps paused live streams. When empty, the temporary folder.");
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("input-timeshift-path") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("timeshift-granularity", "performance", timeshift, qtr("Time-shift block size"),
                          SettingsModel::Number);
        row.help = qtr("In bytes. -1 lets VLC choose.");
        row.effect = SettingsModel::NextFile;
        row.minimum = -1.;
        row.maximum = 2147483647.;
        model.bindConfig(row, { QStringLiteral("input-timeshift-granularity") });
        model.addRow(std::move(row));
    }
}

void addRecordingRows(SettingsModel& model)
{
    const QString recording = qtr("Recording");

    {
        Row row = makeRow("input-record-path", "recording", recording, qtr("Save recordings to"),
                          SettingsModel::Directory);
        row.help = qtr("When empty, recordings go to your Videos or Music folder.");
        model.bindConfig(row, { QStringLiteral("input-record-path") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("input-record-native", "recording", recording, qtr("Record without re-encoding"));
        row.help = qtr("Saves the stream as it comes, when its format allows.");
        model.bindConfig(row, { QStringLiteral("input-record-native") });
        model.addRow(std::move(row));
    }

    const QString streaming = qtr("Streaming and casting");

    {
        Row row = makeRow("cast-quality", "recording", streaming, qtr("Cast quality"), SettingsModel::Choice);
        row.help = qtr("For files a cast device can't play as they are.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "sout-chromecast-conversion-quality");
        model.bindConfig(row, { QStringLiteral("sout-chromecast-conversion-quality") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("sout-display", "recording", streaming, qtr("Play locally while streaming"));
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QStringLiteral("sout-display") });
        model.addRow(std::move(row));
    }

    const struct
    {
        const char* option;
        const char* label;
    } streamRows[] = {
        { "sout-audio", N_("Include audio") },
        { "sout-video", N_("Include video") },
        { "sout-spu", N_("Include subtitles") },
    };
    for (const auto& stream : streamRows)
    {
        Row row = makeRow(stream.option, "recording", streaming, qtr(stream.label));
        row.effect = SettingsModel::NextFile;
        model.bindConfig(row, { QString::fromLatin1(stream.option) });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("x264-preset", "recording", streaming, qtr("Encoding preset"), SettingsModel::Choice);
        row.help = qtr("Faster presets use less processing power for bigger files.");
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "sout-x264-preset");
        model.bindConfig(row, { QStringLiteral("sout-x264-preset") });
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("x264-tune", "recording", streaming, qtr("Encoding tuning"), SettingsModel::Choice);
        row.effect = SettingsModel::NextFile;
        declaredChoices(row, "sout-x264-tune");
        model.bindConfig(row, { QStringLiteral("sout-x264-tune") });
        model.addRow(std::move(row));
    }
}

void addExtensionsRows(SettingsModel& model)
{
    {
        Row row = panel("addons", "extensions", qtr("Add-ons"), qtr("Installed add-ons"), "addons");
        row.help = qtr("Extensions, playlist readers and services added to VLC. "
                       "Find more lists the ones you can install.");
        model.addRow(std::move(row));
    }

    {
        Row row = makeRow("autoload-extensions", "extensions", qtr("Add-ons"), qtr("Load extensions when VLC starts"));
        row.effect = SettingsModel::Restart;
        model.bindConfig(row, { QStringLiteral("qt-autoload-extensions") });
        model.addRow(std::move(row));
    }

    {
        Row row = panel("service-sources", "extensions", qtr("Service sources"), qtr("Services"),
                        "service-sources");
        row.help = qtr("The services the Network section lists, such as podcasts, radio, "
                       "UPnP and SAP announcements.");
        model.addRow(std::move(row));
    }

    {
        Row row = panel("plugins", "extensions", qtr("Plugins"), qtr("Built-in plugins"), "plugins");
        row.help = qtr("Every plugin of this VLC, with what it does.");
        row.advanced = true;
        model.addRow(std::move(row));
    }
}

}

namespace SettingsBasicRows
{

void add(SettingsModel& model, const Hooks& hooks)
{
    addGeneralRows(model);
    addPrivacyRows(model, hooks);
    addControlsRows(model);
    addPlaybackRows(model, hooks);
    addVideoRows(model);
    addAudioRows(model);
    addSubtitlesRows(model, hooks);
    addLibraryRows(model, hooks);
    addNetworkRows(model);
    addPerformanceRows(model);
    addRecordingRows(model);
    addExtensionsRows(model);
}

}
