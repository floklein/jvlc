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

#include "settings_registry.hpp"

#include <QObject>

#include "qt.hpp"

#include <vlc_configuration.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_config_cat.h>

namespace {

bool isOneOf(const QString& value, std::initializer_list<const char*> list)
{
    for (const char* entry : list)
        if (value == QLatin1String(entry))
            return true;
    return false;
}

bool startsWithOneOf(const QString& value, std::initializer_list<const char*> list)
{
    for (const char* entry : list)
        if (value.startsWith(QLatin1String(entry)))
            return true;
    return false;
}

/* Options that make sense on the command line or for one media, not as
 * preferences, and options for the start of the process. */
bool isCommandLineOnly(const QString& name)
{
    return isOneOf(name, {
            "program", "programs", "input-repeat", "start-time", "stop-time",
            "run-time", "input-list", "input-slave", "bookmarks", "open",
            "sub-file", "video-title", "video-x", "video-y", "width", "height",
            "daemon", "pidfile", "started-from-file",
        })
        || name.startsWith(QLatin1String("meta-"))
        || name.startsWith(QLatin1String("bookmark"));
}

/* The capture devices: their options live in Open Capture Device. And the
 * interfaces this redesign drops. */
bool isHiddenModule(const QString& module)
{
    return isOneOf(module, {
        "dshow", "v4l2", "dtv", "decklink", "screen", "screen_win", "xcb_screen",
        "wl_screen", "linsys_hdsdi", "linsys_sdi", "access_alsa", "access_jack",
        "access_oss", "pulsesrc", "access_wasapi", "avcapture", "shm", "rdp",
        "vnc", "imem", "access_imem",
        "skins2", "ncurses",
    });
}

// the core and Qt options are not grouped by plugin
bool isUngrouped(const QString& module)
{
    return module == QLatin1String("core") || module == QLatin1String("qt");
}

// options placed one by one, whatever their subcategory
QString pageOfName(const QString& name)
{
    if (isOneOf(name, { "verbose", "color", "advanced", "interact", "stats",
                        "stats-min-report-interval", "tracer" }))
        return QStringLiteral("general");

    if (isOneOf(name, { "save-recentplay", "keystore" })
        || startsWithOneOf(name, { "lastfm-", "scrobbler-" }))
        return QStringLiteral("privacy");

    if (isOneOf(name, { "dbus", "keyboard-events", "mouse-events", "mouse-pause-policy",
                        "control", "qt-disable-volume-keys" })
        || name.startsWith(QLatin1String("hotkeys-")))
        return QStringLiteral("controls");

    if (isOneOf(name, { "qt-max-volume" }))
        return QStringLiteral("audio");

    if (isOneOf(name, { "one-instance", "one-instance-when-started-from-file",
                        "playlist-enqueue", "media-library", "auto-preparse",
                        "preparse-timeout", "preparse-threads", "fetch-art-threads",
                        "metadata-network-access", "recursive", "ignore-filetypes",
                        "show-hiddenfiles", "extractor-flatten", "playlist-tree",
                        "services-discovery" }))
        return QStringLiteral("library");

    if (isOneOf(name, { "encoder", "vlm-conf", "sap-interval", "mux", "ttl", "miface",
                        "dscp", "packetizer", "vod-server" })
        || startsWithOneOf(name, { "sout", "input-record-" }))
        return QStringLiteral("recording");

    if (isOneOf(name, { "mtu", "ipv4-timeout", "http-host", "http-port", "https-port",
                        "rtsp-host", "rtsp-port", "http-cert", "http-key", "http-ca",
                        "http-crl", "http-proxy", "http-proxy-pwd" })
        || name.endsWith(QLatin1String("-caching")))
        return QStringLiteral("network");

    if (isOneOf(name, { "cr-average", "clock-synchro", "clock-jitter", "clock-master",
                        "clock-source", "drop-late-frames", "skip-frames", "quiet-synchro",
                        "low-delay", "codec", "hw-dec", "dec-dev", "demux", "demux-filter",
                        "stream-filter", "rt-priority", "rt-offset", "high-priority",
                        "plugins-cache", "plugins-scan" })
        || name.startsWith(QLatin1String("input-timeshift-")))
        return QStringLiteral("performance");

    if (isOneOf(name, { "qt-autoload-extensions" }))
        return QStringLiteral("extensions");

    return {};
}

// plugins placed by what they are rather than by their subcategory
QString pageOfModule(const QString& module)
{
    if (isOneOf(module, { "console_logger", "file_logger", "syslog", "sd_journal",
                          "json_tracer", "telegraf_rs", "ntservice" }))
        return QStringLiteral("general");
    if (isOneOf(module, { "audioscrobbler" }))
        return QStringLiteral("privacy");
    if (isOneOf(module, { "gestures", "hotkeys", "rc", "lirc", "dbus", "win_hotkeys",
                          "xcb_hotkeys", "globalhotkeys" }))
        return QStringLiteral("controls");
    if (isOneOf(module, { "lua" }))
        return QStringLiteral("extensions");
    if (isOneOf(module, { "dvdnav", "dvdread", "vcd", "cdda", "libbluray", "bluray",
                          "filesystem" }))
        return QStringLiteral("playback");
    if (isOneOf(module, { "gnutls", "netsync" }))
        return QStringLiteral("network");
    // overlays drawn on the video
    if (isOneOf(module, { "logo", "marq", "mosaic", "rss", "remoteosd", "dynamicoverlay",
                          "audiobargraph_v" }))
        return QStringLiteral("video");
    return {};
}

QString pageOfSubcategory(int subcategory)
{
    switch (subcategory)
    {
    case SUBCAT_INTERFACE_GENERAL:
    case SUBCAT_INTERFACE_MAIN:
        return QStringLiteral("interface");
    case SUBCAT_INTERFACE_CONTROL:
    case SUBCAT_INTERFACE_HOTKEYS:
        return QStringLiteral("controls");
    case SUBCAT_AUDIO_GENERAL:
    case SUBCAT_AUDIO_AOUT:
    case SUBCAT_AUDIO_AFILTER:
    case SUBCAT_AUDIO_VISUAL:
    case SUBCAT_AUDIO_RESAMPLER:
        return QStringLiteral("audio");
    case SUBCAT_VIDEO_GENERAL:
    case SUBCAT_VIDEO_VOUT:
    case SUBCAT_VIDEO_VFILTER:
    case SUBCAT_VIDEO_SPLITTER:
        return QStringLiteral("video");
    case SUBCAT_VIDEO_SUBPIC:
    case SUBCAT_INPUT_SCODEC:
        return QStringLiteral("subtitles");
    case SUBCAT_INPUT_GENERAL:
    case SUBCAT_PLAYLIST_GENERAL:
        return QStringLiteral("playback");
    case SUBCAT_INPUT_ACCESS:
    case SUBCAT_ADVANCED_NETWORK:
        return QStringLiteral("network");
    case SUBCAT_INPUT_DEMUX:
    case SUBCAT_INPUT_VCODEC:
    case SUBCAT_INPUT_ACODEC:
    case SUBCAT_INPUT_STREAM_FILTER:
    case SUBCAT_ADVANCED_MISC:
        return QStringLiteral("performance");
    case SUBCAT_SOUT_GENERAL:
    case SUBCAT_SOUT_STREAM:
    case SUBCAT_SOUT_MUX:
    case SUBCAT_SOUT_ACO:
    case SUBCAT_SOUT_PACKETIZER:
    case SUBCAT_SOUT_VOD:
    case SUBCAT_SOUT_RENDERER:
        return QStringLiteral("recording");
    case SUBCAT_PLAYLIST_SD:
    case SUBCAT_PLAYLIST_EXPORT:
        return QStringLiteral("library");
    default:
        return QStringLiteral("general");
    }
}

/* Options the interface applies as soon as they change: MainCtx reads
 * them again (reloadPrefs), or they are read when used, or they only say
 * how VLC starts. */
bool appliesLive(const QString& name)
{
    return isOneOf(name, { "qt-accent-color", "qt-bgcone", "qt-titlebar", "qt-smooth-scrolling",
                           "qt-max-volume", "qt-pin-controls", "qt-play-adds-to-queue",
                           "qt-fs-opacity", "qt-safe-area", "qt-reduce-motion", "qt-error-dialogs",
                           "qt-fullscreen-screennumber", "qt-minimal-view", "qt-start-minimized",
                           "video-on-top", "mouse-hide-timeout" });
}

bool isDeveloperOption(const QString& name)
{
    return isOneOf(name, { "qt-qmljsdebugger", "qt-verbose", "ml-verbose", "tracer",
                           "avcodec-debug", "mft-debug", "color", "verbose",
                           "plugins-cache", "plugins-scan" });
}

}

namespace SettingsRegistry
{

const QStringList& pages()
{
    static const QStringList list {
        QStringLiteral("general"), QStringLiteral("interface"), QStringLiteral("privacy"),
        QStringLiteral("controls"), QStringLiteral("playback"), QStringLiteral("video"),
        QStringLiteral("audio"), QStringLiteral("subtitles"), QStringLiteral("library"),
        QStringLiteral("network"), QStringLiteral("performance"), QStringLiteral("recording"),
        QStringLiteral("extensions"),
    };
    return list;
}

Placement place(const SettingsOption& option)
{
    Placement placement;

    if (option.subcategory == SUBCAT_HIDDEN || isHiddenModule(option.module)
        || isCommandLineOnly(option.name))
        return placement;

    if (option.type == CONFIG_ITEM_KEY)
    {
        // the global keys belong to the Keyboard Shortcuts editor only;
        // the global hotkeys module reads them once
        if (option.name.startsWith(QLatin1String("global-")))
        {
            placement.effect = EffectRestart;
            return placement;
        }
        placement.page = QStringLiteral("controls");
        placement.section = qtr("Keyboard shortcuts");
        placement.shortcut = true;
        // Keyboard Shortcuts reloads the core key map
        return placement;
    }

    placement.page = pageOfName(option.name);
    if (placement.page.isEmpty() && !isUngrouped(option.module))
        placement.page = pageOfModule(option.module);
    if (placement.page.isEmpty())
        placement.page = pageOfSubcategory(option.subcategory);

    if (!isUngrouped(option.module))
    {
        placement.group = option.module;
        placement.groupLabel = option.moduleLabel.isEmpty() ? option.module : option.moduleLabel;
    }

    placement.section = option.section;
    if (placement.section.isEmpty() && isUngrouped(option.module))
    {
        const char* name = vlc_config_subcat_GetName(static_cast<vlc_config_subcat>(option.subcategory));
        placement.section = name ? qfu(name) : qtr("Other");
    }

    placement.developer = isDeveloperOption(option.name);

    /* The interfaces and the core's own options are read when VLC starts,
     * the others when a file opens its decoders, outputs and filters. */
    const QString modulePage = pageOfModule(option.module);
    if (appliesLive(option.name))
        placement.effect = EffectImmediate;
    else if (option.module == QLatin1String("qt") || option.module == QLatin1String("lua")
        || modulePage == QLatin1String("general") || modulePage == QLatin1String("controls")
        || (option.subcategory >= SUBCAT_INTERFACE_GENERAL && option.subcategory <= SUBCAT_INTERFACE_HOTKEYS)
        || option.subcategory == SUBCAT_ADVANCED_MISC || option.subcategory == SUBCAT_PLAYLIST_SD)
        placement.effect = EffectRestart;
    else
        placement.effect = EffectNextFile;
    return placement;
}

const QVector<Alias>& aliases()
{
    static const QVector<Alias> list {
        { "visualization-width", "audio", N_("Visualization width"),
          { "effect-width", "goom-width", "glspectrum-width", "projectm-width", "vsxu-width" } },
        { "visualization-height", "audio", N_("Visualization height"),
          { "effect-height", "goom-height", "glspectrum-height", "projectm-height", "vsxu-height" } },
    };
    return list;
}

QVector<SettingsOption> options()
{
    QVector<SettingsOption> result;

    size_t count = 0;
    module_t** modules = module_list_get(&count);

    for (size_t i = 0; i < count; ++i)
    {
        const module_t* module = modules[i];

        unsigned size = 0;
        module_config_t* config = module_config_get(module, &size);
        if (!config)
            continue;

        const bool core = module_is_main(module);
        const QString object = core ? QStringLiteral("core") : qfu(module_get_object(module));
        const QString label = core ? QString() : qfu(module_get_name(module, true));

        QString filterChain;
        if (module_provides(module, "video filter"))
            filterChain = QStringLiteral("video-filter");
        else if (module_provides(module, "audio filter"))
            filterChain = QStringLiteral("audio-filter");
        else if (module_provides(module, "sub source"))
            filterChain = QStringLiteral("sub-source");
        else if (module_provides(module, "sub filter"))
            filterChain = QStringLiteral("sub-filter");

        /* module_config_get() leaves out the private and obsolete options */
        int subcategory = SUBCAT_UNKNOWN;
        QString section;
        for (unsigned j = 0; j < size; ++j)
        {
            const module_config_t& item = config[j];

            if (item.i_type == CONFIG_SUBCATEGORY)
            {
                subcategory = item.value.i;
                section.clear();
                continue;
            }
            if (item.i_type == CONFIG_SECTION)
            {
                section = item.psz_text ? qtr(item.psz_text) : QString();
                continue;
            }
            if (!CONFIG_ITEM(item.i_type) || !item.psz_name)
                continue;

            SettingsOption option;
            option.name = qfu(item.psz_name);
            option.module = object;
            option.moduleLabel = label;
            option.subcategory = subcategory;
            option.section = section;
            option.type = item.i_type;
            option.label = item.psz_text ? qtr(item.psz_text) : option.name;
            option.help = item.psz_longtext ? qtr(item.psz_longtext) : QString();
            option.hasChoices = item.list_count > 0;
            option.filterChain = filterChain;
            if (CONFIG_CLASS(item.i_type) == CONFIG_ITEM_FLOAT)
            {
                option.minimum = item.min.f;
                option.maximum = item.max.f;
            }
            else if (CONFIG_CLASS(item.i_type) == CONFIG_ITEM_INTEGER)
            {
                option.minimum = static_cast<double>(item.min.i);
                option.maximum = static_cast<double>(item.max.i);
            }
            result.append(std::move(option));
        }

        module_config_free(config);
    }

    module_list_free(modules);
    return result;
}

}
