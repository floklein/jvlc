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
#include "dialogs/shortcuts/app_shortcuts_model.hpp"
#include "util/color_scheme_model.hpp"
#include "util/vlchotkeyconverter.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QSettings>

#include <vlc_actions.h>
#include <vlc_configuration.h>

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

    addGeneralRows();
    addInterfaceRows();
    addControlsRows();

    /* Advanced Options: every other option, then General's reset */
    qt_intf_t* p_intf = intf();
    setStateSettings(getSettings());
    addAdvancedRows(SettingsRegistry::options());
    addResetRow();

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
        const struct { const char* value; const char* color; QString label; } swatches[] = {
            { "default", "#d37918", qtr("Orange") },
            { "#e15032", "#e15032", qtr("Red") },
            { "#c49e23", "#c49e23", qtr("Yellow") },
            { "#19a234", "#19a234", qtr("Green") },
            { "#18aad3", "#18aad3", qtr("Cyan") },
            { "#185dd3", "#185dd3", qtr("Blue") },
            { "#8e42ff", "#8e42ff", qtr("Purple") },
            { "#f052b3", "#f052b3", qtr("Pink") },
            { "#afafaf", "#afafaf", qtr("Gray") },
        };
        for (const auto& swatch : swatches)
        {
            QVariantMap entry = choice(QString::fromLatin1(swatch.value), swatch.label);
            entry.insert(QStringLiteral("color"), QString::fromLatin1(swatch.color));
            row.choices.append(entry);
        }
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

void AppSettingsModel::restartVLC()
{
    qt_intf_t* p_intf = intf();
    if (!p_intf)
        return;

    // save now, the new process reads the configuration at once
    config_SaveConfigFile(p_intf);
    getSettings()->sync();

    QStringList arguments = QCoreApplication::arguments();
    if (!arguments.isEmpty())
        arguments.removeFirst();
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments))
    {
        msg_Err(p_intf, "could not restart VLC");
        return;
    }

    THEDP->quit();
}
