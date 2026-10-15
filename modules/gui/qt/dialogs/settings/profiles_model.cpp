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

#include "profiles_model.hpp"
#include "settings_registry.hpp"

#include <QSettings>
#include <QUuid>

#include <vlc_configuration.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

namespace {

const QString GROUP = QStringLiteral("Profiles");
const QString KEY_ACTIVE = QStringLiteral("Profiles/Active");
const QString ARRAY_LIST = QStringLiteral("List");
const QString KEY_ID = QStringLiteral("Id");
const QString KEY_NAME = QStringLiteral("Name");

/* the snapshot of a Profile: "Profiles/Data/<id>/Options/<option>" and
 * "Profiles/Data/<id>/Settings/<Qt settings key>" */
QString dataGroup(const QString& id)
{
    return QStringLiteral("Profiles/Data/") + id;
}

QString optionsGroup(const QString& id)
{
    return dataGroup(id) + QStringLiteral("/Options");
}

QString settingsGroup(const QString& id)
{
    return dataGroup(id) + QStringLiteral("/Settings");
}

/* Private options, left out of the module enumeration, that belong to a
 * Profile: the keys of the interface's own actions */
const char* const privateOptions[] = {
    "qt-app-keys",
};

QVariant readOption(const char* name)
{
    switch (config_GetType(name))
    {
    case VLC_VAR_BOOL:
    case VLC_VAR_INTEGER:
        return QVariant(static_cast<qlonglong>(config_GetInt(name)));
    case VLC_VAR_FLOAT:
        return QVariant(static_cast<double>(config_GetFloat(name)));
    case VLC_VAR_STRING:
    {
        char* value = config_GetPsz(name);
        QString result = qfu(value);
        free(value);
        return result;
    }
    default:
        return {};
    }
}

QVariant readOptionDefault(const char* name)
{
    const module_config_t* item = config_FindConfig(name);
    if (!item)
        return {};

    switch (config_GetType(name))
    {
    case VLC_VAR_BOOL:
    case VLC_VAR_INTEGER:
        return QVariant(static_cast<qlonglong>(item->orig.i));
    case VLC_VAR_FLOAT:
        return QVariant(static_cast<double>(item->orig.f));
    case VLC_VAR_STRING:
        return qfu(item->orig.psz);
    default:
        return {};
    }
}

// a stored value (a string, from INI files) as the type of the option
QVariant toOptionType(const char* name, const QVariant& value)
{
    switch (config_GetType(name))
    {
    case VLC_VAR_BOOL:
    case VLC_VAR_INTEGER:
        return QVariant(value.toLongLong());
    case VLC_VAR_FLOAT:
        return QVariant(value.toDouble());
    case VLC_VAR_STRING:
        return QVariant(value.toString());
    default:
        return {};
    }
}

void writeOption(const char* name, const QVariant& value)
{
    switch (config_GetType(name))
    {
    case VLC_VAR_BOOL:
    case VLC_VAR_INTEGER:
        config_PutInt(name, value.toLongLong());
        break;
    case VLC_VAR_FLOAT:
        config_PutFloat(name, value.toFloat());
        break;
    case VLC_VAR_STRING:
    {
        const QByteArray utf8 = value.toString().toUtf8();
        config_PutPsz(name, utf8.isEmpty() ? nullptr : utf8.constData());
        break;
    }
    default:
        break;
    }
}

bool isGroup(const QString& entry)
{
    return entry.endsWith(QLatin1Char('/'));
}

}

const QString ProfilesModel::DEFAULT_ID = QStringLiteral("default");

ProfilesModel::ProfilesModel(qt_intf_t* intf, QSettings* settings, QObject* parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
    , m_settings(settings)
{
    assert(m_settings);

    connect(this, &QAbstractItemModel::rowsInserted, this, &ProfilesModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ProfilesModel::countChanged);

    m_save = [this]() {
        if (m_intf)
            config_SaveConfigFile(m_intf);
    };

    load();

    // what VLC started with, to tell when a switch needs a restart
    m_startOptions = liveOptions();
}

const QStringList& ProfilesModel::profileSettings()
{
    static const QStringList list {
        QStringLiteral("MainWindow/color-scheme-index"),
        QStringLiteral("MainWindow/interface-scale"),
        QStringLiteral("MainWindow/show-waveform"),
        QStringLiteral("MainWindow/remember-geometry"),
        QStringLiteral("PlayerLayouts/"),
    };
    return list;
}

void ProfilesModel::load()
{
    m_settings->beginGroup(GROUP);
    const int size = m_settings->beginReadArray(ARRAY_LIST);
    for (int i = 0; i < size; ++i)
    {
        m_settings->setArrayIndex(i);
        Profile profile;
        profile.id = m_settings->value(KEY_ID).toString();
        profile.name = m_settings->value(KEY_NAME).toString().trimmed();
        if (profile.id.isEmpty() || indexOf(profile.id) >= 0
            || (profile.name.isEmpty() && profile.id != DEFAULT_ID))
            continue;
        m_profiles.append(profile);
    }
    m_settings->endArray();
    m_settings->endGroup();

    // the Default Profile always exists, first
    const int defaultIndex = indexOf(DEFAULT_ID);
    if (defaultIndex < 0)
        m_profiles.prepend({ DEFAULT_ID, QString() });
    else if (defaultIndex > 0)
        m_profiles.move(defaultIndex, 0);

    m_activeId = m_settings->value(KEY_ACTIVE).toString();
    if (indexOf(m_activeId) < 0)
        m_activeId = DEFAULT_ID;
}

void ProfilesModel::saveList()
{
    m_settings->beginGroup(GROUP);
    m_settings->remove(ARRAY_LIST);
    m_settings->beginWriteArray(ARRAY_LIST, m_profiles.count());
    for (int i = 0; i < m_profiles.count(); ++i)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue(KEY_ID, m_profiles.at(i).id);
        m_settings->setValue(KEY_NAME, m_profiles.at(i).name);
    }
    m_settings->endArray();
    m_settings->endGroup();
    m_settings->setValue(KEY_ACTIVE, m_activeId);
    m_settings->sync();
}

int ProfilesModel::indexOf(const QString& id) const
{
    for (int i = 0; i < m_profiles.count(); ++i)
        if (m_profiles.at(i).id == id)
            return i;
    return -1;
}

QString ProfilesModel::nameOf(const Profile& profile) const
{
    if (profile.id == DEFAULT_ID)
        return qtr("Default");
    return profile.name;
}

int ProfilesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_profiles.count();
}

QVariant ProfilesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_profiles.count())
        return {};

    const Profile& profile = m_profiles.at(index.row());
    switch (role)
    {
    case IdRole:
        return profile.id;
    case Qt::DisplayRole:
    case NameRole:
        return nameOf(profile);
    case ActiveRole:
        return profile.id == m_activeId;
    default:
        return {};
    }
}

QHash<int, QByteArray> ProfilesModel::roleNames() const
{
    return {
        { IdRole, "id" },
        { NameRole, "name" },
        { ActiveRole, "active" },
    };
}

QString ProfilesModel::activeName() const
{
    const int index = indexOf(m_activeId);
    return index >= 0 ? nameOf(m_profiles.at(index)) : QString();
}

bool ProfilesModel::isNameAvailable(const QString& name) const
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;
    if (trimmed.compare(QLatin1String("Default"), Qt::CaseInsensitive) == 0)
        return false;
    for (const Profile& profile : m_profiles)
        if (trimmed.compare(nameOf(profile), Qt::CaseInsensitive) == 0)
            return false;
    return true;
}

QString ProfilesModel::create(const QString& name)
{
    if (!isNameAvailable(name))
        return {};

    // the active Profile keeps its settings, the new one starts with them
    emit aboutToSwitch();
    storeLiveState(m_activeId);

    const Profile profile { QUuid::createUuid().toString(QUuid::WithoutBraces), name.trimmed() };
    const int row = m_profiles.count();
    beginInsertRows({}, row, row);
    m_profiles.append(profile);
    endInsertRows();

    const int previous = indexOf(m_activeId);
    m_activeId = profile.id;
    saveList();

    emit dataChanged(index(previous), index(previous), { ActiveRole });
    emit dataChanged(index(row), index(row), { ActiveRole });
    emit activeChanged();
    return profile.id;
}

bool ProfilesModel::activate(const QString& id)
{
    const int row = indexOf(id);
    if (row < 0)
        return false;
    if (id == m_activeId)
        return true;

    emit aboutToSwitch();
    storeLiveState(m_activeId);
    applyState(id);

    const int previous = indexOf(m_activeId);
    m_activeId = id;
    saveList();
    if (m_save)
        m_save();

    emit dataChanged(index(previous), index(previous), { ActiveRole });
    emit dataChanged(index(row), index(row), { ActiveRole });
    emit activeChanged();
    updateRestartRequired();
    emit switched();
    return true;
}

bool ProfilesModel::rename(const QString& id, const QString& name)
{
    const int row = indexOf(id);
    if (row < 0 || id == DEFAULT_ID)
        return false;

    // the Profile's own name, in another case, is available
    const QString trimmed = name.trimmed();
    if (trimmed.compare(m_profiles.at(row).name, Qt::CaseInsensitive) != 0 && !isNameAvailable(trimmed))
        return false;
    if (trimmed.isEmpty())
        return false;
    if (trimmed == m_profiles.at(row).name)
        return true;

    m_profiles[row].name = trimmed;
    saveList();
    emit dataChanged(index(row), index(row), { NameRole, Qt::DisplayRole });
    if (id == m_activeId)
        emit activeChanged();
    return true;
}

bool ProfilesModel::canRemove(const QString& id) const
{
    return id != DEFAULT_ID && indexOf(id) >= 0;
}

bool ProfilesModel::remove(const QString& id)
{
    if (!canRemove(id))
        return false;

    if (id == m_activeId && !activate(DEFAULT_ID))
        return false;

    const int row = indexOf(id);
    beginRemoveRows({}, row, row);
    m_profiles.removeAt(row);
    endRemoveRows();

    m_settings->remove(dataGroup(id));
    saveList();
    return true;
}

QStringList ProfilesModel::optionNames() const
{
    if (!m_optionNames.isEmpty())
        return m_optionNames;

    QSet<QString> seen;

    size_t count = 0;
    module_t** modules = module_list_get(&count);
    for (size_t i = 0; i < count; ++i)
    {
        unsigned size = 0;
        module_config_t* config = module_config_get(modules[i], &size);
        if (!config)
            continue;

        /* module_config_get() leaves out the private and volatile options:
         * the interface's state, such as the first-run answer, is shared */
        for (unsigned j = 0; j < size; ++j)
        {
            const module_config_t& item = config[j];
            if (!CONFIG_ITEM(item.i_type) || !item.psz_name)
                continue;
            const QString name = qfu(item.psz_name);
            if (!seen.contains(name))
            {
                seen.insert(name);
                m_optionNames.append(name);
            }
        }
        module_config_free(config);
    }
    module_list_free(modules);

    for (const char* name : privateOptions)
        if (!seen.contains(qfu(name)))
            m_optionNames.append(qfu(name));

    return m_optionNames;
}

ProfilesModel::Options ProfilesModel::liveOptions() const
{
    Options options;
    for (const QString& name : optionNames())
    {
        const QByteArray utf8 = name.toUtf8();
        if (config_GetType(utf8.constData()) == 0)
            continue;
        const QVariant value = readOption(utf8.constData());
        if (value != readOptionDefault(utf8.constData()))
            options.insert(name, value);
    }
    return options;
}

void ProfilesModel::storeLiveState(const QString& id)
{
    m_settings->remove(dataGroup(id));

    // the VLC options that are not at their default
    const Options options = liveOptions();
    m_settings->beginGroup(optionsGroup(id));
    for (auto it = options.cbegin(); it != options.cend(); ++it)
        m_settings->setValue(it.key(), it.value());
    m_settings->endGroup();

    // the Qt settings of the Profile
    QHash<QString, QVariant> values;
    for (const QString& entry : profileSettings())
    {
        if (isGroup(entry))
        {
            m_settings->beginGroup(entry.chopped(1));
            const QStringList keys = m_settings->allKeys();
            for (const QString& key : keys)
                values.insert(entry + key, m_settings->value(key));
            m_settings->endGroup();
        }
        else if (m_settings->contains(entry))
            values.insert(entry, m_settings->value(entry));
    }

    m_settings->beginGroup(settingsGroup(id));
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        m_settings->setValue(it.key(), it.value());
    m_settings->endGroup();
}

void ProfilesModel::applyState(const QString& id)
{
    // the VLC options: the stored values, the default for the others
    m_settings->beginGroup(optionsGroup(id));
    Options stored;
    const QStringList storedNames = m_settings->childKeys();
    for (const QString& name : storedNames)
        stored.insert(name, m_settings->value(name));
    m_settings->endGroup();

    for (const QString& name : optionNames())
    {
        const QByteArray utf8 = name.toUtf8();
        if (config_GetType(utf8.constData()) == 0)
            continue;

        const QVariant target = stored.contains(name)
                ? toOptionType(utf8.constData(), stored.value(name))
                : readOptionDefault(utf8.constData());
        if (target != readOption(utf8.constData()))
            writeOption(utf8.constData(), target);
    }

    // the Qt settings of the Profile replace the live ones
    m_settings->beginGroup(settingsGroup(id));
    QHash<QString, QVariant> values;
    const QStringList keys = m_settings->allKeys();
    for (const QString& key : keys)
        values.insert(key, m_settings->value(key));
    m_settings->endGroup();

    for (const QString& entry : profileSettings())
        m_settings->remove(isGroup(entry) ? entry.chopped(1) : entry);

    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        bool belongs = false;
        for (const QString& entry : profileSettings())
            belongs |= isGroup(entry) ? it.key().startsWith(entry) : it.key() == entry;
        if (belongs)
            m_settings->setValue(it.key(), it.value());
    }
}

void ProfilesModel::updateRestartRequired()
{
    const Options now = liveOptions();

    QSet<QString> names;
    for (auto it = now.cbegin(); it != now.cend(); ++it)
        names.insert(it.key());
    for (auto it = m_startOptions.cbegin(); it != m_startOptions.cend(); ++it)
        names.insert(it.key());

    bool required = false;
    for (const QString& name : names)
    {
        if (now.value(name) != m_startOptions.value(name) && needsRestart(name))
        {
            required = true;
            break;
        }
    }

    if (required == m_restartRequired)
        return;
    m_restartRequired = required;
    emit restartRequiredChanged();
}

bool ProfilesModel::needsRestart(const QString& option) const
{
    if (m_restartCheck)
        return m_restartCheck(option);

    if (!m_restartOptionsRead)
    {
        m_restartOptionsRead = true;
        for (const SettingsOption& entry : SettingsRegistry::options())
            if (SettingsRegistry::place(entry).effect == SettingsRegistry::EffectRestart)
                m_restartOptions.insert(entry.name);
    }
    return m_restartOptions.contains(option);
}

void ProfilesModel::setSaveFunction(std::function<void()> save)
{
    m_save = std::move(save);
}

void ProfilesModel::setRestartCheck(std::function<bool(const QString&)> check)
{
    m_restartCheck = std::move(check);
}
