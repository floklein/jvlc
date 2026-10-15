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

#include "shortcuts_model.hpp"

#include <vlc_common.h>
#include <vlc_actions.h>
#include <vlc_configuration.h>
#include <vlc_variables.h>

#include "util/vlchotkeyconverter.hpp"

#include <algorithm>

namespace {

const char appKeysOption[] = "qt-app-keys";

/* The groups and plain-word labels of the actions, in the order of the
 * dialog. Basic holds the keys Onboarding shows. */
const struct
{
    const char* name;
    ShortcutsModel::Group group;
    const char* label;
} actionTable[] = {
    { "play-pause",        ShortcutsModel::Basic, N_("Play or pause") },
    { "jump-short",        ShortcutsModel::Basic, N_("Skip back") },
    { "jump+short",        ShortcutsModel::Basic, N_("Skip forward") },
    { "prev",              ShortcutsModel::Basic, N_("Previous item") },
    { "next",              ShortcutsModel::Basic, N_("Next item") },
    { "frame-previous",    ShortcutsModel::Basic, N_("Previous frame") },
    { "frame-next",        ShortcutsModel::Basic, N_("Next frame") },
    { "subtitle-track",    ShortcutsModel::Basic, N_("Next subtitle track") },
    { "audio-track",       ShortcutsModel::Basic, N_("Next audio track") },
    { "toggle-fullscreen", ShortcutsModel::Basic, N_("Fullscreen") },
    { "vol-up",            ShortcutsModel::Basic, N_("Volume up") },
    { "vol-down",          ShortcutsModel::Basic, N_("Volume down") },
    { "show-queue",        ShortcutsModel::Basic, N_("Show or hide the Queue") },
    { "snapshot",          ShortcutsModel::Basic, N_("Take a snapshot") },
    { "record",            ShortcutsModel::Basic, N_("Record") },

    { "play",             ShortcutsModel::Playback, N_("Play") },
    { "pause",            ShortcutsModel::Playback, N_("Pause") },
    { "stop",             ShortcutsModel::Playback, N_("Stop") },
    { "faster",           ShortcutsModel::Playback, N_("Faster") },
    { "slower",           ShortcutsModel::Playback, N_("Slower") },
    { "rate-faster-fine", ShortcutsModel::Playback, N_("A bit faster") },
    { "rate-slower-fine", ShortcutsModel::Playback, N_("A bit slower") },
    { "rate-normal",      ShortcutsModel::Playback, N_("Normal speed") },
    { "random",           ShortcutsModel::Playback, N_("Shuffle") },
    { "loop",             ShortcutsModel::Playback, N_("Repeat") },
    { "position",         ShortcutsModel::Playback, N_("Show the time") },
    { "clear-playlist",   ShortcutsModel::Playback, N_("Clear the Queue") },

    { "jump-extrashort",  ShortcutsModel::Navigation, N_("Skip back a little") },
    { "jump+extrashort",  ShortcutsModel::Navigation, N_("Skip forward a little") },
    { "jump-medium",      ShortcutsModel::Navigation, N_("Skip back more") },
    { "jump+medium",      ShortcutsModel::Navigation, N_("Skip forward more") },
    { "jump-long",        ShortcutsModel::Navigation, N_("Skip back a lot") },
    { "jump+long",        ShortcutsModel::Navigation, N_("Skip forward a lot") },
    { "jump-to-time",     ShortcutsModel::Navigation, N_("Jump to time…") },
    { "add-bookmark",     ShortcutsModel::Navigation, N_("Add a bookmark") },
    { "chapter-prev",     ShortcutsModel::Navigation, N_("Previous chapter") },
    { "chapter-next",     ShortcutsModel::Navigation, N_("Next chapter") },
    { "title-prev",       ShortcutsModel::Navigation, N_("Previous title") },
    { "title-next",       ShortcutsModel::Navigation, N_("Next title") },
    { "program-sid-prev", ShortcutsModel::Navigation, N_("Previous program") },
    { "program-sid-next", ShortcutsModel::Navigation, N_("Next program") },
    { "disc-menu",        ShortcutsModel::Navigation, N_("Disc menu") },

    { "vol-mute",          ShortcutsModel::Audio, N_("Mute") },
    { "audiodelay-up",     ShortcutsModel::Audio, N_("Increase the audio delay") },
    { "audiodelay-down",   ShortcutsModel::Audio, N_("Decrease the audio delay") },
    { "audiodevice-cycle", ShortcutsModel::Audio, N_("Next audio device") },

    { "aspect-ratio",      ShortcutsModel::Video, N_("Next aspect ratio") },
    { "crop",              ShortcutsModel::Video, N_("Next crop") },
    { "zoom",              ShortcutsModel::Video, N_("Next zoom") },
    { "unzoom",            ShortcutsModel::Video, N_("Previous zoom") },
    { "toggle-autoscale",  ShortcutsModel::Video, N_("Fit the video to the window") },
    { "incr-scalefactor",  ShortcutsModel::Video, N_("Make the video bigger") },
    { "decr-scalefactor",  ShortcutsModel::Video, N_("Make the video smaller") },
    { "deinterlace",       ShortcutsModel::Video, N_("Deinterlace") },
    { "deinterlace-mode",  ShortcutsModel::Video, N_("Next deinterlace mode") },
    { "wallpaper",         ShortcutsModel::Video, N_("Video as wallpaper") },
    { "projection-toggle", ShortcutsModel::Video, N_("Switch the 360° projection") },
    { "viewpoint-fov-in",  ShortcutsModel::Video, N_("360°: zoom in") },
    { "viewpoint-fov-out", ShortcutsModel::Video, N_("360°: zoom out") },

    { "subtitle-revtrack",          ShortcutsModel::Subtitles, N_("Previous subtitle track") },
    { "subtitle-toggle",            ShortcutsModel::Subtitles, N_("Show or hide subtitles") },
    { "subdelay-up",                ShortcutsModel::Subtitles, N_("Increase the subtitle delay") },
    { "subdelay-down",              ShortcutsModel::Subtitles, N_("Decrease the subtitle delay") },
    { "subtitle-control-secondary", ShortcutsModel::Subtitles, N_("Delay keys change the second subtitles") },
    { "subsync-markaudio",          ShortcutsModel::Subtitles, N_("Sync: mark the audio") },
    { "subsync-marksub",            ShortcutsModel::Subtitles, N_("Sync: mark the subtitle") },
    { "subsync-apply",              ShortcutsModel::Subtitles, N_("Sync: apply") },
    { "subsync-reset",              ShortcutsModel::Subtitles, N_("Sync: reset") },
    { "subpos-up",                  ShortcutsModel::Subtitles, N_("Move subtitles up") },
    { "subpos-down",                ShortcutsModel::Subtitles, N_("Move subtitles down") },
    { "subtitle-text-scale-up",     ShortcutsModel::Subtitles, N_("Bigger subtitles") },
    { "subtitle-text-scale-down",   ShortcutsModel::Subtitles, N_("Smaller subtitles") },
    { "subtitle-text-scale-normal", ShortcutsModel::Subtitles, N_("Normal subtitle size") },

    { "open-file",            ShortcutsModel::Interface, N_("Open file…") },
    { "open-folder",          ShortcutsModel::Interface, N_("Open folder…") },
    { "open-network",         ShortcutsModel::Interface, N_("Open network stream…") },
    { "open-disc",            ShortcutsModel::Interface, N_("Open disc…") },
    { "open-capture",         ShortcutsModel::Interface, N_("Open capture device…") },
    { "open-clipboard",       ShortcutsModel::Interface, N_("Open the address in the clipboard") },
    { "save-queue",           ShortcutsModel::Interface, N_("Save the Queue as a playlist…") },
    { "convert",              ShortcutsModel::Interface, N_("Convert…") },
    { "stream",               ShortcutsModel::Interface, N_("Stream & Cast…") },
    { "library-search",       ShortcutsModel::Interface, N_("Search the library") },
    { "library-pane",         ShortcutsModel::Interface, N_("Library pane") },
    { "hide-ui",              ShortcutsModel::Interface, N_("Hide UI") },
    { "picture-in-picture",   ShortcutsModel::Interface, N_("Picture-in-picture") },
    { "interface-fullscreen", ShortcutsModel::Interface, N_("Fullscreen interface") },
    { "leave-fullscreen",     ShortcutsModel::Interface, N_("Leave fullscreen") },
    { "intf-show",            ShortcutsModel::Interface, N_("Show the controls in fullscreen") },
    { "intf-popup-menu",      ShortcutsModel::Interface, N_("Right-click menu") },
    { "adjustments",          ShortcutsModel::Interface, N_("Adjustments") },
    { "media-info",           ShortcutsModel::Interface, N_("Properties & Metadata") },
    { "codec-info",           ShortcutsModel::Interface, N_("Codec information") },
    { "messages",             ShortcutsModel::Interface, N_("Messages") },
    { "settings",             ShortcutsModel::Interface, N_("Settings") },
    { "help",                 ShortcutsModel::Interface, N_("Online help") },
    { "about",                ShortcutsModel::Interface, N_("About jVLC") },
    { "quit",                 ShortcutsModel::Interface, N_("Quit") },

    { "nav-activate",             ShortcutsModel::Advanced, N_("Disc menu: select") },
    { "nav-up",                   ShortcutsModel::Advanced, N_("Disc menu: up") },
    { "nav-down",                 ShortcutsModel::Advanced, N_("Disc menu: down") },
    { "nav-left",                 ShortcutsModel::Advanced, N_("Disc menu: left") },
    { "nav-right",                ShortcutsModel::Advanced, N_("Disc menu: right") },
    { "crop-top",                 ShortcutsModel::Advanced, N_("Crop a line at the top") },
    { "uncrop-top",               ShortcutsModel::Advanced, N_("Uncrop a line at the top") },
    { "crop-bottom",              ShortcutsModel::Advanced, N_("Crop a line at the bottom") },
    { "uncrop-bottom",            ShortcutsModel::Advanced, N_("Uncrop a line at the bottom") },
    { "crop-left",                ShortcutsModel::Advanced, N_("Crop a column on the left") },
    { "uncrop-left",              ShortcutsModel::Advanced, N_("Uncrop a column on the left") },
    { "crop-right",               ShortcutsModel::Advanced, N_("Crop a column on the right") },
    { "uncrop-right",             ShortcutsModel::Advanced, N_("Uncrop a column on the right") },
    { "zoom-quarter",             ShortcutsModel::Advanced, N_("Zoom to a quarter") },
    { "zoom-half",                ShortcutsModel::Advanced, N_("Zoom to half") },
    { "zoom-original",            ShortcutsModel::Advanced, N_("Original size") },
    { "zoom-double",              ShortcutsModel::Advanced, N_("Zoom to double") },
    { "viewpoint-roll-clock",     ShortcutsModel::Advanced, N_("360°: roll clockwise") },
    { "viewpoint-roll-anticlock", ShortcutsModel::Advanced, N_("360°: roll counterclockwise") },
    { "intf-boss",                ShortcutsModel::Advanced, N_("Boss key: hide jVLC") },
};

QString groupLabel(ShortcutsModel::Group group)
{
    switch (group)
    {
    case ShortcutsModel::Basic: return qtr("Basic");
    case ShortcutsModel::Playback: return qtr("Playback");
    case ShortcutsModel::Navigation: return qtr("Navigation");
    case ShortcutsModel::Audio: return qtr("Audio");
    case ShortcutsModel::Video: return qtr("Video");
    case ShortcutsModel::Subtitles: return qtr("Subtitles");
    case ShortcutsModel::Interface: return qtr("Interface");
    case ShortcutsModel::Advanced: return qtr("Advanced");
    }
    return {};
}

}

ShortcutsModel::ShortcutsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void ShortcutsModel::setStore(ReadFunction read, WriteFunction write)
{
    m_read = std::move(read);
    m_write = std::move(write);
}

void ShortcutsModel::setCommitFunction(std::function<void()> commit)
{
    m_commit = std::move(commit);
}

QString ShortcutsModel::readOption(const QString& option) const
{
    if (m_read)
        return m_read(option);

    // unknown options (the Qt module's, in tests) have no value
    const QByteArray name = option.toUtf8();
    if (!config_GetType(name.constData()) && !var_Type(m_obj, name.constData()))
        return {};
    char* value = var_InheritString(m_obj, name.constData());
    const QString result = value ? qfu(value) : QStringLiteral("");
    free(value);
    return result;
}

void ShortcutsModel::writeOption(const QString& option, const QString& value)
{
    if (m_write)
    {
        m_write(option, value);
        return;
    }

    const QByteArray name = option.toUtf8();
    if (config_FindConfig(name.constData()))
        config_PutPsz(name.constData(), qtu(value));
}

ShortcutsModel::Keys ShortcutsModel::parseKeys(const QString& value)
{
    Keys keys;
    for (const QString& name : value.split('\t', Qt::SkipEmptyParts))
    {
        const uint32_t key = vlc_str2keycode(qtu(name));
        if (key != KEY_UNSET && !keys.contains(key))
            keys.append(key);
    }
    return keys;
}

QString ShortcutsModel::formatKeys(const Keys& keys)
{
    QStringList names;
    for (uint32_t key : keys)
    {
        char* name = vlc_keycode2str(key, false);
        if (name)
            names.append(qfu(name));
        free(name);
    }
    return names.join('\t');
}

QStringList ShortcutsModel::keyTexts(const Keys& keys) const
{
    QStringList texts;
    for (uint32_t key : keys)
        texts.append(vlcKeyText(key));
    return texts;
}

void ShortcutsModel::load(vlc_object_t* obj)
{
    beginResetModel();

    m_obj = obj;
    m_entries.clear();

    QStringList names;
    for (const char* const* name = vlc_actions_get_key_names(obj); *name; ++name)
        names.append(qfu(*name));
    const QStringList appNames = appActionNames();
    names.append(appNames);

    const QHash<QString, QString> appKeys = parseAppKeys(readOption(appKeysOption));

    // in the order of the table, actions missing from it go to Advanced
    QVector<Entry> missing;
    QVector<Entry> entries;
    for (const auto& action : actionTable)
    {
        Entry entry;
        entry.name = QString::fromLatin1(action.name);
        entry.group = action.group;
        entry.label = qtr(action.label);
        if (names.contains(entry.name))
            entries.append(entry);
    }
    for (const QString& name : names)
    {
        const bool known = std::any_of(entries.cbegin(), entries.cend(),
                                       [&name](const Entry& entry) { return entry.name == name; });
        if (known)
            continue;
        Entry entry;
        entry.name = name;
        entry.group = Advanced;
        const module_config_t* config = config_FindConfig(qtu(QStringLiteral("key-") + name));
        entry.label = config && config->psz_text ? qfu(vlc_gettext(config->psz_text)) : name;
        missing.append(entry);
    }
    entries.append(missing);
    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry& a, const Entry& b) { return a.group < b.group; });

    for (Entry& entry : entries)
    {
        entry.app = appNames.contains(entry.name);
        if (entry.app)
        {
            const uint32_t key = qtSequenceToVLCKey(appActionDefaultSequence(entry.name));
            if (key != KEY_UNSET)
                entry.defaultKeys = { key };
            const auto it = appKeys.constFind(entry.name);
            entry.savedKeys = it != appKeys.cend() ? parseKeys(*it) : entry.defaultKeys;
        }
        else
        {
            const QString option = QStringLiteral("key-") + entry.name;
            const QString globalOption = QStringLiteral("global-") + option;

            const module_config_t* config = config_FindConfig(qtu(option));
            entry.defaultKeys = parseKeys(config ? qfu(config->orig.psz) : QString());
            config = config_FindConfig(qtu(globalOption));
            entry.defaultGlobalKeys = parseKeys(config ? qfu(config->orig.psz) : QString());

            entry.savedKeys = parseKeys(readOption(option));
            entry.savedGlobalKeys = parseKeys(readOption(globalOption));
        }
        entry.keys = entry.savedKeys;
        entry.globalKeys = entry.savedGlobalKeys;
    }
    m_entries = std::move(entries);

    endResetModel();
    updateModified();
}

int ShortcutsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant ShortcutsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry& entry = m_entries.at(index.row());

    switch (role)
    {
    case NameRole: return entry.name;
    case GroupRole: return static_cast<int>(entry.group);
    case GroupLabelRole: return groupLabel(entry.group);
    case Qt::DisplayRole:
    case LabelRole: return entry.label;
    case KeysRole: return keyTexts(entry.keys);
    case GlobalKeysRole: return keyTexts(entry.globalKeys);
    case HasGlobalKeyRole: return !entry.app;
    case IsDefaultRole:
        return entry.keys == entry.defaultKeys && entry.globalKeys == entry.defaultGlobalKeys;
    }
    return {};
}

QHash<int, QByteArray> ShortcutsModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { GroupRole, "group" },
        { GroupLabelRole, "groupLabel" },
        { LabelRole, "label" },
        { KeysRole, "keys" },
        { GlobalKeysRole, "globalKeys" },
        { HasGlobalKeyRole, "hasGlobalKey" },
        { IsDefaultRole, "isDefault" },
    };
}

int ShortcutsModel::rowOf(const QString& name) const
{
    for (int row = 0; row < m_entries.size(); ++row)
    {
        if (m_entries.at(row).name == name)
            return row;
    }
    return -1;
}

int ShortcutsModel::keyFromEvent(int key, int modifiers) const
{
    switch (key)
    {
    case 0:
    case Qt::Key_unknown:
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    // Tab moves the keyboard focus
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return 0;
    }

    const uint32_t vlcKey = qtKeyToVLC(key, Qt::KeyboardModifiers(modifiers));
    if ((vlcKey & ~KEY_MODIFIER) == 0)
        return 0;
    return static_cast<int>(vlcKey);
}

QString ShortcutsModel::keyText(int key) const
{
    return vlcKeyText(static_cast<uint32_t>(key));
}

int ShortcutsModel::otherRowWith(int row, bool global, uint32_t key) const
{
    for (int other = 0; other < m_entries.size(); ++other)
    {
        if (other == row)
            continue;
        const Entry& entry = m_entries.at(other);
        if ((global ? entry.globalKeys : entry.keys).contains(key))
            return other;
    }
    return -1;
}

QString ShortcutsModel::conflict(const QString& name, bool global, int key) const
{
    const int row = rowOf(name);
    if (row < 0 || key == 0)
        return {};
    const int other = otherRowWith(row, global, static_cast<uint32_t>(key));
    return other >= 0 ? m_entries.at(other).label : QString();
}

void ShortcutsModel::setKeys(int row, bool global, const Keys& keys)
{
    Entry& entry = m_entries[row];
    Keys& target = global ? entry.globalKeys : entry.keys;
    if (target == keys)
        return;
    target = keys;
    const QModelIndex index = this->index(row);
    emit dataChanged(index, index, { KeysRole, GlobalKeysRole, IsDefaultRole });
}

void ShortcutsModel::takeKey(int row, bool global, uint32_t key)
{
    for (int other = otherRowWith(row, global, key); other >= 0;
         other = otherRowWith(row, global, key))
    {
        Keys keys = global ? m_entries.at(other).globalKeys : m_entries.at(other).keys;
        keys.removeAll(key);
        setKeys(other, global, keys);
    }
}

void ShortcutsModel::assign(const QString& name, bool global, int key)
{
    const int row = rowOf(name);
    if (row < 0 || key == 0 || (global && m_entries.at(row).app))
        return;

    takeKey(row, global, static_cast<uint32_t>(key));
    setKeys(row, global, { static_cast<uint32_t>(key) });
    updateModified();
}

void ShortcutsModel::clear(const QString& name, bool global)
{
    const int row = rowOf(name);
    if (row < 0)
        return;
    setKeys(row, global, {});
    updateModified();
}

QString ShortcutsModel::resetConflict(const QString& name) const
{
    const int row = rowOf(name);
    if (row < 0)
        return {};
    const Entry& entry = m_entries.at(row);
    for (bool global : { false, true })
    {
        for (uint32_t key : global ? entry.defaultGlobalKeys : entry.defaultKeys)
        {
            const int other = otherRowWith(row, global, key);
            if (other >= 0)
                return m_entries.at(other).label;
        }
    }
    return {};
}

void ShortcutsModel::reset(const QString& name)
{
    const int row = rowOf(name);
    if (row < 0)
        return;
    const Entry& entry = m_entries.at(row);
    const Keys defaultKeys = entry.defaultKeys;
    const Keys defaultGlobalKeys = entry.defaultGlobalKeys;

    for (uint32_t key : defaultKeys)
        takeKey(row, false, key);
    for (uint32_t key : defaultGlobalKeys)
        takeKey(row, true, key);
    setKeys(row, false, defaultKeys);
    setKeys(row, true, defaultGlobalKeys);
    updateModified();
}

void ShortcutsModel::resetAll()
{
    for (int row = 0; row < m_entries.size(); ++row)
    {
        setKeys(row, false, m_entries.at(row).defaultKeys);
        setKeys(row, true, m_entries.at(row).defaultGlobalKeys);
    }
    updateModified();
}

void ShortcutsModel::save()
{
    bool appChanged = false;
    QHash<QString, QString> appKeys;

    for (Entry& entry : m_entries)
    {
        if (entry.app)
        {
            appChanged |= entry.keys != entry.savedKeys;
            if (entry.keys != entry.defaultKeys)
                appKeys.insert(entry.name, formatKeys(entry.keys.mid(0, 1)));
        }
        else
        {
            const QString option = QStringLiteral("key-") + entry.name;
            if (entry.keys != entry.savedKeys)
                writeOption(option, formatKeys(entry.keys));
            if (entry.globalKeys != entry.savedGlobalKeys)
                writeOption(QStringLiteral("global-") + option, formatKeys(entry.globalKeys));
        }
        entry.savedKeys = entry.keys;
        entry.savedGlobalKeys = entry.globalKeys;
    }

    if (appChanged)
        writeOption(appKeysOption, formatAppKeys(appKeys));

    updateModified();

    if (m_commit)
        m_commit();
}

void ShortcutsModel::revert()
{
    for (int row = 0; row < m_entries.size(); ++row)
    {
        setKeys(row, false, m_entries.at(row).savedKeys);
        setKeys(row, true, m_entries.at(row).savedGlobalKeys);
    }
    updateModified();
}

bool ShortcutsModel::modified() const
{
    return m_modified;
}

void ShortcutsModel::updateModified()
{
    const bool modified = std::any_of(m_entries.cbegin(), m_entries.cend(), [](const Entry& entry) {
        return entry.keys != entry.savedKeys || entry.globalKeys != entry.savedGlobalKeys;
    });
    if (modified == m_modified)
        return;
    m_modified = modified;
    emit modifiedChanged();
}

/* ShortcutsFilterModel */

ShortcutsFilterModel::ShortcutsFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &ShortcutsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ShortcutsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ShortcutsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ShortcutsFilterModel::countChanged);
}

void ShortcutsFilterModel::setSearchPattern(const QString& pattern)
{
    if (pattern == m_searchPattern)
        return;
    m_searchPattern = pattern;
    m_searchWords = pattern.split(' ', Qt::SkipEmptyParts);
    invalidateFilter();
    emit searchPatternChanged();
    emit countChanged();
}

void ShortcutsFilterModel::setField(Field field)
{
    if (field == m_field)
        return;
    m_field = field;
    invalidateFilter();
    emit fieldChanged();
    emit countChanged();
}

int ShortcutsFilterModel::count() const
{
    return rowCount();
}

int ShortcutsFilterModel::rowOf(const QString& name) const
{
    for (int row = 0; row < rowCount(); ++row)
    {
        if (index(row, 0).data(ShortcutsModel::NameRole).toString() == name)
            return row;
    }
    return -1;
}

bool ShortcutsFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_searchWords.isEmpty())
        return true;

    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    // every word appears in the action
    const QString label = index.data(ShortcutsModel::LabelRole).toString();
    const bool actionMatches = std::all_of(m_searchWords.cbegin(), m_searchWords.cend(),
        [&label](const QString& word) { return label.contains(word, Qt::CaseInsensitive); });

    // one of the keys starts with the search, spaces left out: "ctrl + o"
    QString pattern = m_searchPattern;
    pattern.remove(' ');
    QStringList keys = index.data(ShortcutsModel::KeysRole).toStringList();
    keys += index.data(ShortcutsModel::GlobalKeysRole).toStringList();
    const bool keyMatches = std::any_of(keys.cbegin(), keys.cend(),
        [&pattern](QString key) { return key.remove(' ').startsWith(pattern, Qt::CaseInsensitive); });

    switch (m_field)
    {
    case Action: return actionMatches;
    case Key: return keyMatches;
    case AllFields: break;
    }
    return actionMatches || keyMatches;
}
