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

#include "settings_model.hpp"

#include <QSettings>

#include <vlc_configuration.h>

#include <cmath>

namespace {

constexpr int DEFAULT_SAVE_DELAY = 500;

QVariant readConfig(const char* name)
{
    switch (config_GetType(name))
    {
    case VLC_VAR_BOOL:
        return QVariant(config_GetInt(name) != 0);
    case VLC_VAR_INTEGER:
        return QVariant(static_cast<int>(config_GetInt(name)));
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

QVariant readConfigDefault(const char* name)
{
    const module_config_t* item = config_FindConfig(name);
    if (!item)
        return {};

    switch (config_GetType(name))
    {
    case VLC_VAR_BOOL:
        return QVariant(item->orig.i != 0);
    case VLC_VAR_INTEGER:
        return QVariant(static_cast<int>(item->orig.i));
    case VLC_VAR_FLOAT:
        return QVariant(static_cast<double>(item->orig.f));
    case VLC_VAR_STRING:
        return qfu(item->orig.psz);
    default:
        return {};
    }
}

bool writeConfig(const char* name, const QVariant& value)
{
    switch (config_GetType(name))
    {
    case VLC_VAR_BOOL:
        config_PutInt(name, value.toBool() ? 1 : 0);
        return true;
    case VLC_VAR_INTEGER:
        config_PutInt(name, value.toLongLong());
        return true;
    case VLC_VAR_FLOAT:
        config_PutFloat(name, value.toFloat());
        return true;
    case VLC_VAR_STRING:
    {
        const QByteArray utf8 = value.toString().toUtf8();
        config_PutPsz(name, utf8.isEmpty() ? nullptr : utf8.constData());
        return true;
    }
    default:
        return false;
    }
}

bool sameValue(const QVariant& a, const QVariant& b)
{
    if (!a.isValid() || !b.isValid())
        return a.isValid() == b.isValid();

    const int typeA = a.typeId();
    const int typeB = b.typeId();
    if (typeA == QMetaType::Double || typeB == QMetaType::Double
        || typeA == QMetaType::Float || typeB == QMetaType::Float)
    {
        const double x = a.toDouble();
        const double y = b.toDouble();
        return std::fabs(x - y) <= 1e-6 * std::max(1., std::max(std::fabs(x), std::fabs(y)));
    }
    if (typeA == QMetaType::Bool || typeB == QMetaType::Bool)
        return a.toBool() == b.toBool();
    if (typeA == QMetaType::QString || typeB == QMetaType::QString)
        return a.toString() == b.toString();
    return a == b;
}

}

SettingsModel::SettingsModel(qt_intf_t* intf, QObject* parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
    , m_profileName(qtr("Default"))
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(DEFAULT_SAVE_DELAY);
    connect(&m_saveTimer, &QTimer::timeout, this, &SettingsModel::save);

    m_save = [this]() {
        if (m_intf)
            config_SaveConfigFile(m_intf);
    };
}

SettingsModel::~SettingsModel()
{
    // don't lose a change made just before closing
    if (m_saveTimer.isActive())
        save();
}

QVariantMap SettingsModel::choice(const QVariant& value, const QString& label)
{
    return {
        { QStringLiteral("value"), value },
        { QStringLiteral("label"), label },
    };
}

void SettingsModel::bindConfig(Row& row, const QStringList& options)
{
    QVector<QByteArray> names;
    for (const QString& option : options)
        names.append(option.toUtf8());

    const auto firstExisting = [names]() -> const char* {
        for (const QByteArray& name : names)
            if (config_GetType(name.constData()) != 0)
                return name.constData();
        return nullptr;
    };

    row.isAvailable = [firstExisting]() {
        return firstExisting() != nullptr;
    };
    row.read = [firstExisting]() -> QVariant {
        const char* name = firstExisting();
        return name ? readConfig(name) : QVariant();
    };
    row.readDefault = [firstExisting]() -> QVariant {
        const char* name = firstExisting();
        return name ? readConfigDefault(name) : QVariant();
    };
    row.write = [names](const QVariant& value) {
        bool written = false;
        for (const QByteArray& name : names)
            written |= writeConfig(name.constData(), value);
        return written;
    };
    row.savesConfig = true;
}

void SettingsModel::bindSettings(Row& row, QSettings* settings, const QString& key, const QVariant& defaultValue)
{
    assert(settings);
    const int type = defaultValue.typeId();

    row.isAvailable = []() { return true; };
    row.read = [settings, key, defaultValue, type]() {
        QVariant value = settings->value(key, defaultValue);
        // QSettings returns strings from INI files
        if (value.typeId() != type)
            value.convert(QMetaType(type));
        return value;
    };
    row.readDefault = [defaultValue]() { return defaultValue; };
    row.write = [settings, key, defaultValue](const QVariant& value) {
        if (sameValue(value, defaultValue))
            settings->remove(key);
        else
            settings->setValue(key, value);
        return true;
    };
}

void SettingsModel::addRow(Row row)
{
    Entry entry { std::move(row), {} };
    if (entry.row.effect == Restart && isAvailable(entry))
        entry.startValue = readValue(entry);

    const int count = m_entries.count();
    beginInsertRows({}, count, count);
    m_entries.append(std::move(entry));
    endInsertRows();
}

int SettingsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.count();
}

QVariant SettingsModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Entry& entry = m_entries.at(index.row());
    const Row& row = entry.row;

    switch (role)
    {
    case IdRole:
        return row.id;
    case PageRole:
        return row.page;
    case SectionRole:
        return row.section;
    case Qt::DisplayRole:
    case LabelRole:
        return row.label;
    case HelpRole:
        return row.help;
    case ControlRole:
        return row.control;
    case ValueRole:
        return readValue(entry);
    case DefaultValueRole:
        return readDefault(entry);
    case IsDefaultRole:
        return sameValue(readValue(entry), readDefault(entry));
    case ChoicesRole:
        return row.choicesProvider ? row.choicesProvider() : row.choices;
    case MinimumRole:
        return row.minimum;
    case MaximumRole:
        return row.maximum;
    case StepRole:
        return row.step;
    case ScaleRole:
        return row.scale;
    case UnitRole:
        return row.unit;
    case EffectRole:
        return row.effect;
    case AllProfilesRole:
        return row.allProfiles;
    case DeveloperRole:
        return row.developer;
    case AvailableRole:
        return isAvailable(entry);
    case PendingRestartRole:
        return isPendingRestart(entry);
    default:
        return {};
    }
}

bool SettingsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != ValueRole)
        return false;
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return false;
    return writeRow(index.row(), value);
}

Qt::ItemFlags SettingsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemNeverHasChildren;
}

QHash<int, QByteArray> SettingsModel::roleNames() const
{
    return {
        { IdRole, "settingId" },
        { PageRole, "page" },
        { SectionRole, "section" },
        { LabelRole, "label" },
        { HelpRole, "help" },
        { ControlRole, "control" },
        { ValueRole, "value" },
        { DefaultValueRole, "defaultValue" },
        { IsDefaultRole, "isDefault" },
        { ChoicesRole, "choices" },
        { MinimumRole, "minimum" },
        { MaximumRole, "maximum" },
        { StepRole, "step" },
        { ScaleRole, "scale" },
        { UnitRole, "unit" },
        { EffectRole, "effect" },
        { AllProfilesRole, "allProfiles" },
        { DeveloperRole, "developer" },
        { AvailableRole, "available" },
        { PendingRestartRole, "pendingRestart" },
    };
}

QVariant SettingsModel::value(const QString& id) const
{
    const int row = rowOf(id);
    if (row < 0)
        return {};
    return readValue(m_entries.at(row));
}

bool SettingsModel::setValue(const QString& id, const QVariant& value)
{
    const int row = rowOf(id);
    if (row < 0)
        return false;
    return writeRow(row, value);
}

void SettingsModel::reset(const QString& id)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    writeRow(row, readDefault(m_entries.at(row)));
}

bool SettingsModel::restartRequired() const
{
    return m_restartRequired;
}

QString SettingsModel::profileName() const
{
    return m_profileName;
}

void SettingsModel::setSaveFunction(std::function<void()> save)
{
    m_save = std::move(save);
}

void SettingsModel::setSaveDelay(int ms)
{
    m_saveTimer.setInterval(ms);
}

void SettingsModel::setIntf(qt_intf_t* intf)
{
    m_intf = intf;
}

int SettingsModel::rowOf(const QString& id) const
{
    for (int i = 0; i < m_entries.count(); ++i)
        if (m_entries.at(i).row.id == id)
            return i;
    return -1;
}

bool SettingsModel::isAvailable(const Entry& entry) const
{
    const Row& row = entry.row;
    if (!row.read || !row.write)
        return false;
    return !row.isAvailable || row.isAvailable();
}

QVariant SettingsModel::readValue(const Entry& entry) const
{
    if (!isAvailable(entry))
        return {};
    return entry.row.read();
}

QVariant SettingsModel::readDefault(const Entry& entry) const
{
    if (!isAvailable(entry) || !entry.row.readDefault)
        return {};
    return entry.row.readDefault();
}

bool SettingsModel::isPendingRestart(const Entry& entry) const
{
    if (entry.row.effect != Restart || !isAvailable(entry))
        return false;
    return !sameValue(readValue(entry), entry.startValue);
}

bool SettingsModel::writeRow(int rowIndex, const QVariant& value)
{
    Entry& entry = m_entries[rowIndex];
    if (!isAvailable(entry))
        return false;

    if (sameValue(readValue(entry), value))
        return true;

    if (!entry.row.write(value))
        return false;

    if (entry.row.savesConfig)
        m_saveTimer.start();

    const QModelIndex modelIndex = index(rowIndex);
    emit dataChanged(modelIndex, modelIndex);
    emit settingChanged(entry.row.id, readValue(entry));

    updateRestartRequired();
    return true;
}

void SettingsModel::updateRestartRequired()
{
    bool required = false;
    for (const Entry& entry : m_entries)
    {
        if (isPendingRestart(entry))
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

void SettingsModel::save()
{
    m_saveTimer.stop();
    if (m_save)
        m_save();
}

//=================================================================================================
// SettingsFilterModel
//=================================================================================================

SettingsFilterModel::SettingsFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::rowsInserted, this, &SettingsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SettingsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &SettingsFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &SettingsFilterModel::countChanged);
}

void SettingsFilterModel::setPage(const QString& page)
{
    if (m_page == page)
        return;
    m_page = page;
    invalidateFilter();
    emit pageChanged();
    emit countChanged();
}

void SettingsFilterModel::setSearchPattern(const QString& pattern)
{
    if (m_searchPattern == pattern)
        return;
    m_searchPattern = pattern;
    m_searchWords = pattern.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    invalidateFilter();
    emit searchPatternChanged();
    emit countChanged();
}

void SettingsFilterModel::setShowDeveloper(bool show)
{
    if (m_showDeveloper == show)
        return;
    m_showDeveloper = show;
    invalidateFilter();
    emit showDeveloperChanged();
    emit countChanged();
}

int SettingsFilterModel::count() const
{
    return rowCount();
}

bool SettingsFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    if (!index.data(SettingsModel::AvailableRole).toBool())
        return false;
    if (!m_showDeveloper && index.data(SettingsModel::DeveloperRole).toBool())
        return false;

    // search covers every page
    if (!m_searchWords.isEmpty())
        return matchesSearch(index);

    return index.data(SettingsModel::PageRole).toString() == m_page;
}

bool SettingsFilterModel::matchesSearch(const QModelIndex& index) const
{
    QStringList texts {
        index.data(SettingsModel::LabelRole).toString(),
        index.data(SettingsModel::SectionRole).toString(),
        index.data(SettingsModel::HelpRole).toString(),
    };
    const QVariantList choices = index.data(SettingsModel::ChoicesRole).toList();
    for (const QVariant& choice : choices)
        texts.append(choice.toMap().value(QStringLiteral("label")).toString());

    const QString haystack = texts.join(QLatin1Char('\n'));
    for (const QString& word : m_searchWords)
        if (!haystack.contains(word, Qt::CaseInsensitive))
            return false;
    return true;
}
