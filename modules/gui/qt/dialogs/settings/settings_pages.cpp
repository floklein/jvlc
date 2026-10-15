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

#include "settings_pages.hpp"

#include "maininterface/mainctx.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "dialogs/settings/profiles_model.hpp"
#include "dialogs/settings/settings_basic_rows.hpp"
#include "dialogs/subtitles/opensubtitles.hpp"
#include "dialogs/shortcuts/app_shortcuts_model.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlrecenthidelist.hpp"
#include "player/player_controller.hpp"
#include "player/player_video_outputs.hpp"
#include "player/preset_controller.hpp"
#include "player/subtitle_appearance.hpp"
#include "style/accent_colors.hpp"
#include "util/color_scheme_model.hpp"
#include "util/vlchotkeyconverter.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

#include <vlc_actions.h>
#include <vlc_configuration.h>
#include <vlc_player.h>
#include <vlc_playlist.h>

#ifdef _WIN32
# include "util/registry.hpp"
#endif

namespace {

using Row = SettingsModel::Row;

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

/* Interface languages with a translation. The names are shown in their own
 * language, so they are not translated. */
const struct
{
    const char iso[6];
    const char name[34];
} language_map[] = {
    { "en",    "American English" },
    { "ar",    "عربي" },
    { "bn",    "বাংলা" },
    { "pt_BR", "Português Brasileiro" },
    { "en_GB", "British English" },
    { "el",    "Νέα Ελληνικά" },
    { "bg",    "български език" },
    { "ca",    "Català" },
    { "zh_TW", "正體中文" },
    { "cs",    "Čeština" },
    { "cy",    "Cymraeg" },
    { "da",    "Dansk" },
    { "nl",    "Nederlands" },
    { "fi",    "Suomi" },
    { "et",    "eesti keel" },
    { "eu",    "Euskara" },
    { "fr",    "Français" },
    { "ga",    "Gaeilge" },
    { "gd",    "Gàidhlig" },
    { "gl",    "Galego" },
    { "ka",    "ქართული" },
    { "de",    "Deutsch" },
    { "he",    "עברית" },
    { "hr",    "hrvatski" },
    { "hu",    "Magyar" },
    { "hy",    "հայերեն" },
    { "is",    "íslenska" },
    { "id",    "Bahasa Indonesia" },
    { "it",    "Italiano" },
    { "ja",    "日本語" },
    { "ko",    "한국어" },
    { "lt",    "lietuvių" },
    { "mn",    "Монгол хэл" },
    { "ms",    "Melayu" },
    { "nb",    "Bokmål" },
    { "nn",    "Nynorsk" },
    { "kk",    "Қазақ тілі" },
    { "km",    "ភាសាខ្មែរ" },
    { "ne",    "नेपाली" },
    { "oc",    "Occitan" },
    { "fa",    "فارسی" },
    { "pl",    "Polski" },
    { "pt_PT", "Português" },
    { "pa",    "ਪੰਜਾਬੀ" },
    { "ro",    "Română" },
    { "ru",    "Русский" },
    { "zh_CN", "简体中文" },
    { "si",    "සිංහල" },
    { "sr",    "српски" },
    { "sk",    "Slovensky" },
    { "sl",    "slovenščina" },
    { "ckb",   "کوردیی سۆرانی" },
    { "es",    "Español" },
    { "sv",    "Svenska" },
    { "te",    "తెలుగు" },
    { "tr",    "Türkçe" },
    { "uk",    "украї́нська мо́ва" },
    { "vi",    "tiếng Việt" },
    { "wa",    "Walon" }
};

/* The interface language is read before libvlc starts: from the registry on
 * Windows (bin/winvlc.c), from a file in the configuration folder elsewhere
 * (bin/vlc.c). "auto" follows the system. */
#ifdef _WIN32
QString readLanguage()
{
    QVLCRegistry registry(HKEY_CURRENT_USER);
    char* lang = registry.ReadRegistry("Software\\VideoLAN\\VLC\\", "Lang", "auto");
    QString result = qfu(lang);
    free(lang);
    return result.isEmpty() ? QStringLiteral("auto") : result;
}

bool writeLanguage(const QString& lang)
{
    QVLCRegistry registry(HKEY_CURRENT_USER);
    if (lang == QLatin1String("auto"))
    {
        registry.DeleteValue("Software\\VideoLAN\\VLC\\", "Lang");
        registry.DeleteKey("Software\\VideoLAN\\", "VLC");
        registry.DeleteKey("Software\\", "VideoLAN");
    }
    else
        registry.WriteRegistry("Software\\VideoLAN\\VLC\\", "Lang", qtu(lang));
    return true;
}
#elif !defined(__APPLE__)
QString languageFilePath()
{
    char* dir = config_GetUserDir(VLC_CONFIG_DIR);
    if (!dir)
        return {};
    const QString path = qfu(dir) + QStringLiteral("/language");
    free(dir);
    return path;
}

QString readLanguage()
{
    QFile file(languageFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("auto");
    const QString lang = QString::fromUtf8(file.readLine()).trimmed();
    return lang.isEmpty() ? QStringLiteral("auto") : lang;
}

bool writeLanguage(const QString& lang)
{
    const QString path = languageFilePath();
    if (path.isEmpty())
        return false;
    if (lang == QLatin1String("auto"))
    {
        QFile::remove(path);
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    file.write(lang.toUtf8() + '\n');
    return true;
}
#endif

}

AppSettingsModel::AppSettingsModel(QObject* parent)
    : SettingsModel(nullptr, parent)
{
    connect(this, &SettingsModel::settingChanged, this, &AppSettingsModel::onSettingChanged);
    connect(this, &SettingsModel::allReset, this, [this]() {
        if (!m_ctx)
            return;
        m_ctx->reloadPrefs();
        // the keyboard shortcuts were reset too
        vlc_actions_reload(VLC_OBJECT(vlc_object_instance(intf())));
        m_ctx->notifyKeyBindingsChanged();
    });
}

void AppSettingsModel::setCtx(MainCtx* ctx)
{
    if (m_ctx || !ctx)
        return;
    m_ctx = ctx;
    setIntf(ctx->getIntf());

    // filters changed here also apply to the playing file
    setLiveFilters(liveFilters());

    addGeneralRows();
    addInterfaceRows();
    addControlsRows();

    /* the other pages, and the homes of the Profiles, Presets, library
     * folders, service sources and add-ons */
    SettingsBasicRows::Hooks hooks;
    hooks.applyQueueOptions = [this]() { applyQueueOptions(); };
    hooks.hasMediaLibrary = [ctx]() { return ctx->hasMediaLibrary(); };
    hooks.clearRecent = [ctx]() {
        if (MediaLib* ml = ctx->getMediaLibrary())
            ml->recentHideList()->clear(QDateTime::currentSecsSinceEpoch());
    };
    hooks.applySubtitleLook = [ctx]() {
        PlayerController* player = ctx->getIntf()->p_mainPlayerController;
        if (!player)
            return;
        // as after a switch of Profile: the playing file's Preset goes over
        player->getSubtitleAppearance()->reload();
        if (PresetController* presets = ctx->getPresets())
            presets->profileSwitched();
    };
    SettingsBasicRows::add(*this, hooks);

    addOpenSubtitlesRows();

    /* Advanced Options: every other option, then General's reset */
    qt_intf_t* p_intf = intf();
    setStateSettings(getSettings());
    addAdvancedRows(SettingsRegistry::options());
    addResetRow();

    /* the settings shown are those of the active Profile */
    if (ProfilesModel* profiles = ctx->profiles())
    {
        setProfileName(profiles->activeName());
        connect(profiles, &ProfilesModel::activeChanged, this, [this, profiles]() {
            setProfileName(profiles->activeName());
        });
        connect(profiles, &ProfilesModel::switched, this, &SettingsModel::refresh);
    }

    emit ctxChanged();
}

void AppSettingsModel::addResetRow()
{
    Row row = makeRow("reset-all", "general", qtr("Reset"), qtr("Reset all settings in this Profile"), Action);
    row.help = qtr("Puts back every setting of this Profile, keyboard shortcuts included, "
                   "as VLC first had them. The settings shared by all profiles are kept.");
    row.advanced = true;
    row.buttonText = qtr("Reset…");
    row.confirmText = qtr("Reset all");
    row.action = [this]() { resetAll(); };
    addRow(std::move(row));
}

void AppSettingsModel::addGeneralRows()
{
    qt_intf_t* p_intf = intf();
    MainCtx* ctx = m_ctx;

    /* Appearance */
    const QString appearance = qtr("Appearance");

    {
        Row row = makeRow("language", "general", appearance, qtr("Interface language"), Choice);
        row.help = qtr("The language of menus, buttons and messages. Shared by all profiles.");
        row.effect = Restart;
        row.allProfiles = true;
        row.choices.append(choice(QStringLiteral("auto"), qtr("System language")));
        for (const auto& language : language_map)
            row.choices.append(choice(QString::fromLatin1(language.iso), qfu(language.name)));
#ifndef __APPLE__
        row.read = []() { return QVariant(readLanguage()); };
        row.readDefault = []() { return QVariant(QStringLiteral("auto")); };
        row.write = [](const QVariant& value) { return writeLanguage(value.toString()); };
#endif
        addRow(std::move(row));
    }

    {
        Row row = makeRow("theme", "general", appearance, qtr("Theme"), Segmented);
        row.help = qtr("Light or dark colors. System follows your operating system.");
        row.choices = {
            choice(static_cast<int>(ColorSchemeModel::Dark), qtr("Dark")),
            choice(static_cast<int>(ColorSchemeModel::Light), qtr("Light")),
            choice(static_cast<int>(ColorSchemeModel::System), qtr("System")),
        };
        // the rows of ColorSchemeModel are in the order of its enum
        row.read = [ctx]() { return QVariant(ctx->getColorScheme()->currentIndex()); };
        row.readDefault = []() { return QVariant(static_cast<int>(ColorSchemeModel::System)); };
        row.write = [ctx, p_intf](const QVariant& value) {
            const int index = value.toInt();
            if (index < 0 || index >= ctx->getColorScheme()->rowCount({}))
                return false;
            ctx->getColorScheme()->setCurrentIndex(index);
            getSettings()->setValue("MainWindow/color-scheme-index", index);
            return true;
        };
        addRow(std::move(row));
    }

    {
        Row row = makeRow("accent-color", "general", appearance, qtr("Accent color"), Swatches);
        row.help = qtr("The color of selected and active items.");
        row.choices = accentColorChoices();
        bindConfig(row, { QStringLiteral("qt-accent-color") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("interface-scale", "general", appearance, qtr("Interface scale"), Slider);
        row.help = qtr("Makes text and controls larger or smaller. Ctrl and the mouse wheel also change it.");
        row.minimum = 0.75;
        row.maximum = 2.0;
        row.step = 0.05;
        row.scale = 100.;
        row.unit = QStringLiteral("%");
        row.read = [ctx]() { return QVariant(ctx->getIntfUserScaleFactor()); };
        row.readDefault = []() { return QVariant(1.0); };
        row.write = [ctx, p_intf](const QVariant& value) {
            ctx->setIntfUserScaleFactor(value.toDouble());
            getSettings()->setValue("MainWindow/interface-scale", QString::number(ctx->getIntfUserScaleFactor()));
            return true;
        };
        addRow(std::move(row));
    }

    /* Startup */
    const QString startup = qtr("Startup");

    {
        Row row = makeRow("error-dialogs", "general", startup, qtr("Show error dialogs"));
        row.help = qtr("Tell you when a file can't be opened or played. "
                       "When off, errors only go to the Messages log.");
        bindConfig(row, { QStringLiteral("qt-error-dialogs") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("welcome-screen", "general", startup, qtr("Show the welcome screen at next start"));
        row.help = qtr("Shows Onboarding again: library folders, Theme, online details "
                       "and the basic shortcuts.");
        row.advanced = true;
        // Onboarding turns it off once done
        bindConfig(row, { QStringLiteral("qt-privacy-ask") });
        // the option is on only until the first Onboarding
        row.readDefault = []() { return QVariant(false); };
        addRow(std::move(row));
    }

    /* Diagnostics */
    const QString diagnostics = qtr("Diagnostics");

    {
        Row row = makeRow("developer-mode", "general", diagnostics, qtr("Developer mode"));
        row.help = qtr("Shows more options to troubleshoot VLC.");
        bindSettings(row, getSettings(), QStringLiteral("Settings/developer-mode"), false);
        addRow(std::move(row));
    }

    {
        Row row = makeRow("file-logging", "general", diagnostics, qtr("Save a log"));
        row.help = qtr("Write VLC's messages to a text file, to send with a bug report.");
        row.effect = Restart;
        bindConfig(row, { QStringLiteral("file-logging") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("logfile", "general", diagnostics, qtr("Log file"), SaveFile);
        row.help = qtr("Where the log is saved. When empty, it is vlc-log.txt "
                       "on your desktop (Windows) or in your home folder.");
        row.effect = Restart;
        bindConfig(row, { QStringLiteral("logfile") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("log-verbose", "general", diagnostics, qtr("Log details"), Choice);
        row.help = qtr("Which messages go into the log.");
        row.effect = Restart;
        row.choices = {
            choice(-1, qtr("Same as the console")),
            choice(1, qtr("Errors only")),
            choice(2, qtr("Errors and warnings")),
            choice(3, qtr("Everything, for debugging")),
        };
        bindConfig(row, { QStringLiteral("log-verbose") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("qt-verbose", "general", diagnostics, qtr("Log the interface's internal messages"));
        row.help = qtr("Adds Qt's own messages to the log.");
        row.effect = Restart;
        row.developer = true;
        bindConfig(row, { QStringLiteral("qt-verbose") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("syslog", "general", diagnostics, qtr("Send the log to the system log"));
        row.help = qtr("Also write VLC's messages to the system log (syslog).");
        row.effect = Restart;
        row.developer = true;
        bindConfig(row, { QStringLiteral("syslog") });
        addRow(std::move(row));
    }
}

void AppSettingsModel::addInterfaceRows()
{
    qt_intf_t* p_intf = intf();

    /* Window */
    const QString window = qtr("Window");

    {
        Row row = makeRow("embedded-video", "interface", window, qtr("Play video inside the main window"));
        row.help = qtr("When off, video plays in a window of its own.");
        row.effect = NextFile;
        bindConfig(row, { QStringLiteral("embedded-video") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("video-autoresize", "interface", window, qtr("Resize the window to fit the video"), Choice);
        row.help = qtr("Whether the window takes the size of the video when it starts.");
        row.effect = Restart;
        row.choices = {
            choice(false, qtr("Never")),
            choice(true, qtr("When a video starts")),
        };
        bindConfig(row, { QStringLiteral("qt-video-autoresize") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("remember-geometry", "interface", window, qtr("Remember window size and position"));
        row.help = qtr("Open VLC where you left it. Applies the next time VLC starts.");
        bindSettings(row, getSettings(), QStringLiteral("MainWindow/remember-geometry"), true);
        addRow(std::move(row));
    }

    {
        Row row = makeRow("video-on-top", "interface", window, qtr("Keep the window above other windows"));
        row.help = qtr("VLC stays visible when you use other apps.");
        bindConfig(row, { QStringLiteral("video-on-top") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("window-decorations", "interface", window, qtr("Show window decorations"));
        row.help = qtr("Use your system's title bar and borders instead of VLC's.");
        bindConfig(row, { QStringLiteral("qt-titlebar") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("name-in-title", "interface", window, qtr("Show the file name in the title bar"));
        row.help = qtr("The taskbar and window switcher show what is playing.");
        row.effect = Restart;
        bindConfig(row, { QStringLiteral("qt-name-in-title") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("fullscreen-screen", "interface", window, qtr("Open fullscreen on"), Choice);
        row.help = qtr("The screen used when you switch to fullscreen.");
        row.choicesProvider = []() {
            QVariantList choices { choice(-1, qtr("Current screen")) };
            const QList<QScreen*> screens = QGuiApplication::screens();
            for (int i = 0; i < screens.count(); ++i)
                choices.append(choice(i, qtr("Screen %1 (%2)").arg(i + 1).arg(screens.at(i)->name())));
            return choices;
        };
        bindConfig(row, { QStringLiteral("qt-fullscreen-screennumber") });
        addRow(std::move(row));
    }

#ifdef _WIN32
    /* the Video blur: only the DirectComposition compositor draws it */
    {
        Row row = makeRow("video-blur", "interface", window, qtr("Blur the video behind panels"));
        row.help = qtr("Panels over the video show it blurred under a lighter tint. "
                       "Off by itself while Windows transparency effects are off or battery saver is on.");
        bindConfig(row, { QStringLiteral("qt-video-blur") });
        addRow(std::move(row));
    }
#endif

    /* Playback controls, Hide UI */
    const QString controls = qtr("Playback controls");

    {
        Row row = makeRow("start-hide-ui", "interface", controls, qtr("Start in Hide UI"));
        row.help = qtr("Start with both bars hidden. They appear when the pointer nears their edge.");
        bindConfig(row, { QStringLiteral("qt-minimal-view") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("auto-hide-controls", "interface", controls, qtr("Auto-hide the controls"), Choice);
        row.help = qtr("When the bars hide over the video until you move the pointer.");
        row.choices = {
            choice(false, qtr("Always")),
            choice(true, qtr("In fullscreen only")),
        };
        bindConfig(row, { QStringLiteral("qt-pin-controls") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("mouse-hide-timeout", "interface", controls, qtr("Hide the pointer after"), Slider);
        row.help = qtr("How long the pointer stays over the video without moving.");
        row.minimum = 0.;
        row.maximum = 10000.;
        row.step = 500.;
        row.scale = 0.001;
        row.unit = qtr("s");
        bindConfig(row, { QStringLiteral("mouse-hide-timeout") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("reduce-motion", "interface", controls, qtr("Reduce motion"));
        row.help = qtr("Fade the bars and panels in and out instead of sliding them. "
                       "Always on when your system asks for less motion.");
        bindConfig(row, { QStringLiteral("qt-reduce-motion") });
        addRow(std::move(row));
    }

    /* Notifications and system tray */
    const QString tray = qtr("Notifications and system tray");

    {
        Row row = makeRow("system-tray", "interface", tray, qtr("Show a system tray icon"));
        row.help = qtr("Control VLC from the notification area of the taskbar.");
        row.effect = Restart;
        bindConfig(row, { QStringLiteral("qt-system-tray") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("start-minimized", "interface", tray, qtr("Start minimized to the tray"));
        row.help = qtr("VLC starts with only its tray icon.");
        bindConfig(row, { QStringLiteral("qt-start-minimized") });
        addRow(std::move(row));
    }

    {
        Row row = makeRow("notification", "interface", tray, qtr("Show a popup when the track changes"), Choice);
        row.help = qtr("A notification with the title and artist of the new track.");
        row.effect = Restart;
        row.choices = {
            choice(static_cast<int>(NOTIFICATION_NEVER), qtr("Never")),
            choice(static_cast<int>(NOTIFICATION_MINIMIZED), qtr("When minimized")),
            choice(static_cast<int>(NOTIFICATION_ALWAYS), qtr("Always")),
        };
        bindConfig(row, { QStringLiteral("qt-notification") });
        addRow(std::move(row));
    }
}

void AppSettingsModel::addOpenSubtitlesRows()
{
    /* the optional OpenSubtitles sign-in: the credentials go to VLC's
     * keystore through the client, never to the settings */
    QPointer<opensubtitles::Client> client = THEDP->openSubtitles();
    connect(client, &opensubtitles::Client::storedUserChanged, this, &SettingsModel::refresh);
    connect(client, &opensubtitles::Client::quotaChanged, this, &SettingsModel::refresh);

    struct State
    {
        // the username typed, until the password comes
        QString user;
        bool typed = false;
        QString error;
        bool signingIn = false;
    };
    auto state = std::make_shared<State>();
    QPointer<AppSettingsModel> self(this);
    auto available = [client]() { return client && client->available(); };
    auto currentUser = [client, state]() {
        return state->typed ? state->user : (client ? client->storedUser() : QString());
    };

    const QString section = QStringLiteral("OpenSubtitles");

    {
        Row row = makeRow("opensubtitles-user", "subtitles", section, qtr("OpenSubtitles username"), Text);
        row.allProfiles = true;
        row.help = qtr("Optional. Signing in to OpenSubtitles raises your daily downloads: "
                       "type your username, then your password.");
        row.read = [currentUser]() { return QVariant(currentUser()); };
        row.readDefault = []() { return QVariant(QString()); };
        row.write = [self, client, state, available](const QVariant& value) {
            if (!available())
                return false;
            state->user = value.toString().trimmed();
            state->typed = true;
            state->error.clear();
            /* an empty username signs out */
            if (state->user.isEmpty() && !client->storedUser().isEmpty())
                client->signOut([self, state](const QString&) {
                    state->typed = false;
                    if (self)
                        self->refresh();
                });
            return true;
        };
        addRow(std::move(row));
    }

    {
        Row row = makeRow("opensubtitles-password", "subtitles", section, qtr("OpenSubtitles password"), Password);
        row.allProfiles = true;
        row.help = qtr("Kept in your system's keystore, not in VLC's settings.");
        row.read = []() { return QVariant(QString()); };
        row.readDefault = []() { return QVariant(QString()); };
        row.write = [self, client, state, available, currentUser](const QVariant& value) {
            const QString password = value.toString();
            const QString user = currentUser();
            if (!available() || password.isEmpty() || user.isEmpty())
                return false;
            state->signingIn = true;
            state->error.clear();
            if (self)
                QMetaObject::invokeMethod(self, &SettingsModel::refresh, Qt::QueuedConnection);
            client->signIn(user, password, [self, state](const QString& error) {
                state->signingIn = false;
                state->error = error;
                if (error.isEmpty())
                    state->typed = false;
                if (self)
                    self->refresh();
            });
            return true;
        };
        addRow(std::move(row));
    }

    {
        /* the state of the sign-in, and Sign Out */
        Row row = makeRow("opensubtitles-account", "subtitles", section, qtr("OpenSubtitles account"), Action);
        row.allProfiles = true;
        row.labelProvider = [client, state, available]() -> QString {
            if (!available())
                return qtr("OpenSubtitles isn't available in this build");
            if (state->signingIn)
                return qtr("Signing in…");
            if (!state->error.isEmpty())
                return qtr("Sign-in failed: %1").arg(state->error);
            const opensubtitles::Quota quota = client->quota();
            if (quota.signedIn)
                return qtr("Signed in as %1: %2").arg(quota.user, opensubtitles::quotaText(quota));
            if (!client->storedUser().isEmpty())
                return qtr("Signs in as %1 when you add subtitles").arg(client->storedUser());
            return qtr("Not signed in");
        };
        row.helpProvider = [client, available]() -> QString {
            if (!available())
                return qtr("This build of VLC has no OpenSubtitles API key.");
            if (client->storedUser().isEmpty())
                return qtr("Without an account, OpenSubtitles allows a few downloads a day.");
            return qtr("Sign Out forgets the sign-in of %1 on this computer.").arg(client->storedUser());
        };
        row.buttonText = qtr("Sign Out");
        row.confirmText = qtr("Sign out");
        row.action = [self, client, state]() {
            if (!client)
                return;
            state->typed = false;
            state->error.clear();
            client->signOut([self](const QString&) {
                if (self)
                    self->refresh();
            });
        };
        addRow(std::move(row));
    }
}

void AppSettingsModel::addControlsRows()
{
    MainCtx* ctx = m_ctx;

    /* Keyboard Shortcuts */
    const QString shortcuts = qtr("Keyboard Shortcuts");

    {
        Row row = makeRow("edit-shortcuts", "controls", shortcuts, qtr("All shortcuts"), Action);
        row.help = qtr("Find and change the keys of every action.");
        row.buttonText = qtr("Edit Keyboard Shortcuts");
        row.action = []() { THEDP->keyboardShortcutsDialog(); };
        addRow(std::move(row));
    }

    {
        Row row = makeRow("reset-shortcuts", "controls", shortcuts, qtr("Reset all shortcuts"), Action);
        row.help = qtr("Every action gets its default keys back, in this profile.");
        row.buttonText = qtr("Reset…");
        row.confirmText = qtr("Reset all");
        row.action = [ctx]() {
            AppShortcutsModel model;
            model.setCtx(ctx);
            model.resetAll();
            if (model.modified())
                model.save();
        };
        addRow(std::move(row));
    }

    /* The search also finds the shortcut actions, and opens Keyboard
     * Shortcuts on them. The core ones ("key-*" options) come with Advanced
     * Options, these are the interface's own. */
    vlc_object_t* libvlc = VLC_OBJECT(vlc_object_instance(intf()));
    ShortcutsModel actions;
    actions.load(libvlc);
    for (int i = 0; i < actions.rowCount(); ++i)
    {
        const QModelIndex index = actions.index(i);
        if (index.data(ShortcutsModel::HasGlobalKeyRole).toBool())
            continue;
        const QString name = index.data(ShortcutsModel::NameRole).toString();

        Row row;
        row.id = QStringLiteral("app-key-") + name;
        row.page = QStringLiteral("controls");
        row.section = shortcuts;
        row.label = index.data(ShortcutsModel::LabelRole).toString();
        row.control = Shortcut;
        row.searchOnly = true;
        row.read = [libvlc, name]() { return QVariant(appActionSequence(libvlc, name)); };
        row.readDefault = [name]() { return QVariant(appActionDefaultSequence(name)); };
        // changed in Keyboard Shortcuts only
        row.write = [](const QVariant&) { return false; };
        addRow(std::move(row));
    }
}

void AppSettingsModel::onSettingChanged(const QString& id)
{
    if (!m_ctx)
        return;

    /* MainCtx mirrors several options (window decorations, pinned
     * controls, pointer timeout, Accent color, reduced motion...): reload
     * them so that the interface follows at once. */
    static const char* const mirrored[] = {
        "accent-color", "video-on-top", "window-decorations",
        "auto-hide-controls", "mouse-hide-timeout", "reduce-motion",
        "osd-shortcut-keys", "video-blur", "max-volume", "play-adds-to-queue",
    };
    for (const char* name : mirrored)
    {
        if (id == QLatin1String(name))
        {
            m_ctx->reloadPrefs();
            return;
        }
    }
}

/* The Queue reads these options once, when VLC starts (libvlc.c): give them
 * to the running Queue and player too. */
void AppSettingsModel::applyQueueOptions()
{
    vlc_playlist_t* playlist = intf()->p_playlist;
    if (!playlist)
        return;

    const auto option = [](const char* name) {
        return config_GetType(name) != 0 && config_GetInt(name) != 0;
    };

    vlc_playlist_Lock(playlist);
    vlc_playlist_SetPlaybackOrder(playlist, option("random") ? VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM
                                                             : VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL);
    vlc_playlist_SetPlaybackRepeat(playlist, option("repeat") ? VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT
                                             : option("loop") ? VLC_PLAYLIST_PLAYBACK_REPEAT_ALL
                                                              : VLC_PLAYLIST_PLAYBACK_REPEAT_NONE);
    vlc_playlist_SetMediaStoppedAction(playlist, option("play-and-exit") ? VLC_PLAYLIST_MEDIA_STOPPED_EXIT
                                                 : option("play-and-stop") ? VLC_PLAYLIST_MEDIA_STOPPED_STOP
                                                                           : VLC_PLAYLIST_MEDIA_STOPPED_CONTINUE);
    vlc_player_t* player = vlc_playlist_GetPlayer(playlist);
    vlc_player_SetStartPaused(player, option("start-paused"));
    vlc_player_SetPauseOnCork(player, option("playlist-cork"));
    vlc_player_SetPlayAndPause(player, option("play-and-pause"));
    vlc_playlist_Unlock(playlist);
}

/* The video outputs run the video and subtitle filter chains, and follow
 * their variables; the audio output adds and removes its filters. */
SettingsModel::LiveFilters AppSettingsModel::liveFilters()
{
    LiveFilters live;
    qt_intf_t* p_intf = intf();

    live.toggle = [p_intf](const QString& chain, const QString& filter, bool enabled) {
        PlayerController* player = p_intf->p_mainPlayerController;
        if (!player)
            return;
        const QByteArray name = filter.toUtf8();
        if (chain == QLatin1String("audio-filter"))
        {
            vlc_player_aout_EnableFilter(player->getPlayer(), name.constData(), enabled);
            return;
        }

        const QByteArray variable = chain.toUtf8();
        for (const SharedVOutThread& vout : player->getVouts())
        {
            if (var_Type(vout.get(), variable.constData()) == 0)
                continue;
            char* current = var_GetString(vout.get(), variable.constData());
            QStringList filters = qfu(current).split(QRegularExpression(QStringLiteral("[:,]")),
                                                     Qt::SkipEmptyParts);
            free(current);
            filters.removeAll(filter);
            if (enabled)
                filters.append(filter);
            var_SetString(vout.get(), variable.constData(), qtu(filters.join(QLatin1Char(':'))));
        }
    };

    live.option = [p_intf](const QString&, const QString& option, const QVariant& value) {
        PlayerController* player = p_intf->p_mainPlayerController;
        if (!player)
            return;
        PlayerVideoOutputs outputs(player);
        outputs.setOption(qtu(option), value);
    };
    return live;
}

QString AppSettingsModel::browseSaveFile(const QString& title, const QString& current) const
{
    return QDir::toNativeSeparators(QFileDialog::getSaveFileName(nullptr, title, current));
}

QString AppSettingsModel::browseOpenFile(const QString& title, const QString& current) const
{
    return QDir::toNativeSeparators(QFileDialog::getOpenFileName(nullptr, title, current));
}

QString AppSettingsModel::browseDirectory(const QString& title, const QString& current) const
{
    return QDir::toNativeSeparators(QFileDialog::getExistingDirectory(nullptr, title, current));
}

QUrl AppSettingsModel::localFileUrl(const QString& path) const
{
    return QUrl::fromLocalFile(QDir::fromNativeSeparators(path));
}

void AppSettingsModel::restartVLC()
{
    THEDP->restartVLC();
}
