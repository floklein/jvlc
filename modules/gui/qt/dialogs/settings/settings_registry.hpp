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

#ifndef VLC_QT_SETTINGS_REGISTRY_HPP
#define VLC_QT_SETTINGS_REGISTRY_HPP

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * One VLC option, as declared by the core or a plugin.
 */
struct SettingsOption
{
    // empty for a filter plugin without options, which still gets a group
    // with its filter toggle
    QString name;
    // the plugin ("core" for the core), and its name for people
    QString module;
    QString moduleLabel;
    // SUBCAT_* of the option, and the CONFIG_SECTION above it
    int subcategory = 0;
    QString section;
    // CONFIG_ITEM_*
    int type = 0;
    QString label;
    QString help;
    // a static list of choices
    bool hasChoices = false;
    double minimum = 0.;
    double maximum = 0.;
    // the filter chain option the plugin is added to, if it is a filter
    QString filterChain;
};

/**
 * Where every VLC option goes in the Settings window.
 *
 * The core and Qt options are placed one by one, the plugins by their
 * subcategory. Options that only make sense on the command line, and the
 * capture device options (in Open Capture Device), are hidden. The keys of
 * the shortcut actions are only found by search.
 */
namespace SettingsRegistry
{

// when a change takes effect, as SettingsModel::Effect
enum Effect
{
    EffectImmediate,
    EffectNextFile,
    EffectRestart,
};

struct Placement
{
    // empty when the option is hidden
    QString page;
    // the plugin group, empty for the core and Qt options
    QString group;
    QString groupLabel;
    QString section;
    // a key of a shortcut action
    bool shortcut = false;
    // shown in developer mode only
    bool developer = false;
    Effect effect = EffectImmediate;
};

// the page ids, in the order of the Settings window
const QStringList& pages();

Placement place(const SettingsOption& option);

/* Options known under several names, shown as one row writing all of
 * them. */
struct Alias
{
    const char* id;
    const char* page;
    const char* label;
    QStringList names;
};
const QVector<Alias>& aliases();

// every option of the core and of the loaded plugins
QVector<SettingsOption> options();

}

#endif // VLC_QT_SETTINGS_REGISTRY_HPP
