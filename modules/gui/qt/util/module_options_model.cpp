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

#include "module_options_model.hpp"

#include "qt.hpp"

#include <algorithm>

#include <vlc_configuration.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

namespace {

QVariantMap choice(const QVariant& value, const QString& text)
{
    return QVariantMap{ { "value", value }, { "text", text } };
}

QVariantList choicesOf(const char* name, int type)
{
    QVariantList list;
    if (CONFIG_CLASS(type) == CONFIG_ITEM_STRING)
    {
        char** values = nullptr;
        char** texts = nullptr;
        const ssize_t count = config_GetPszChoices(name, &values, &texts);
        for (ssize_t i = 0; i < count; ++i)
        {
            const QString value = qfu(values[i]);
            const QString text = texts[i] ? qfu(texts[i]) : QString();
            list.append(choice(value, text.isEmpty() ? value : text));
            free(values[i]);
            free(texts[i]);
        }
        free(values);
        free(texts);
    }
    else if (CONFIG_CLASS(type) == CONFIG_ITEM_INTEGER)
    {
        int64_t* values = nullptr;
        char** texts = nullptr;
        const ssize_t count = config_GetIntChoices(name, &values, &texts);
        for (ssize_t i = 0; i < count; ++i)
        {
            const QString text = texts[i] ? qfu(texts[i]) : QString::number(values[i]);
            list.append(choice(qlonglong(values[i]), text));
            free(texts[i]);
        }
        free(values);
        free(texts);
    }
    return list;
}

bool sameValue(const QVariant& a, const QVariant& b, ModuleOptionsModel::OptionType type)
{
    switch (type)
    {
    case ModuleOptionsModel::Bool:
        return a.toBool() == b.toBool();
    case ModuleOptionsModel::Integer:
        return a.toLongLong() == b.toLongLong();
    case ModuleOptionsModel::Float:
        return qFuzzyCompare(1. + a.toDouble(), 1. + b.toDouble());
    case ModuleOptionsModel::String:
        return a.toString() == b.toString();
    }
    return false;
}

} // namespace

ModuleOptionsModel::ModuleOptionsModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void ModuleOptionsModel::setModule(const QString& module)
{
    if (m_module == module)
        return;
    m_module = module;
    emit moduleChanged();
    load();
}

void ModuleOptionsModel::setExcludedOptions(const QStringList& names)
{
    if (m_excluded == names)
        return;
    m_excluded = names;
    emit excludedOptionsChanged();
    load();
}

bool ModuleOptionsModel::modified() const
{
    return std::any_of(m_options.cbegin(), m_options.cend(), [](const Option& option) {
        return !sameValue(option.value, option.initial, option.type);
    });
}

QStringList ModuleOptionsModel::options() const
{
    QStringList list;
    for (const Option& option : m_options)
    {
        if (sameValue(option.value, option.initial, option.type))
            continue;

        switch (option.type)
        {
        case Bool:
            list << (option.value.toBool() ? QStringLiteral(":") : QStringLiteral(":no-")) + option.name;
            break;
        case Integer:
            list << QStringLiteral(":%1=%2").arg(option.name).arg(option.value.toLongLong());
            break;
        case Float:
            list << QStringLiteral(":%1=%2").arg(option.name).arg(option.value.toDouble());
            break;
        case String:
            list << QStringLiteral(":%1=%2").arg(option.name, option.value.toString());
            break;
        }
    }
    return list;
}

int ModuleOptionsModel::indexOf(const QString& name) const
{
    for (size_t i = 0; i < m_options.size(); ++i)
        if (m_options[i].name == name)
            return static_cast<int>(i);
    return -1;
}

bool ModuleOptionsModel::setValue(int row, const QVariant& value)
{
    if (row < 0 || static_cast<size_t>(row) >= m_options.size())
        return false;

    Option& option = m_options[row];
    const QVariant newValue = normalized(option, value);
    if (sameValue(option.value, newValue, option.type))
        return true;

    option.value = newValue;
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, { ValueRole, ModifiedRole });
    emit optionsChanged();
    return true;
}

void ModuleOptionsModel::reset()
{
    if (!modified())
        return;
    for (Option& option : m_options)
        option.value = option.initial;
    emit dataChanged(index(0), index(rowCount() - 1), { ValueRole, ModifiedRole });
    emit optionsChanged();
}

int ModuleOptionsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_options.size());
}

QVariant ModuleOptionsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || static_cast<size_t>(index.row()) >= m_options.size())
        return {};

    const Option& option = m_options[index.row()];
    switch (role)
    {
    case NameRole:
        return option.name;
    case Qt::DisplayRole:
    case TextRole:
        return option.text;
    case DescriptionRole:
        return option.description;
    case SectionRole:
        return option.section;
    case TypeRole:
        return option.type;
    case ValueRole:
        return option.value;
    case DefaultValueRole:
        return option.defaultValue;
    case MinimumRole:
        return option.minimum;
    case MaximumRole:
        return option.maximum;
    case ChoicesRole:
        return option.choices;
    case ModifiedRole:
        return !sameValue(option.value, option.initial, option.type);
    default:
        return {};
    }
}

bool ModuleOptionsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || (role != ValueRole && role != Qt::EditRole))
        return false;
    return setValue(index.row(), value);
}

Qt::ItemFlags ModuleOptionsModel::flags(const QModelIndex& index) const
{
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QHash<int, QByteArray> ModuleOptionsModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { TextRole, "text" },
        { DescriptionRole, "description" },
        { SectionRole, "section" },
        { TypeRole, "type" },
        { ValueRole, "value" },
        { DefaultValueRole, "defaultValue" },
        { MinimumRole, "minimum" },
        { MaximumRole, "maximum" },
        { ChoicesRole, "choices" },
        { ModifiedRole, "modified" },
    };
}

void ModuleOptionsModel::load()
{
    const bool wasModified = modified();
    const int oldCount = rowCount();

    beginResetModel();
    m_options.clear();

    module_t* module = m_module.isEmpty() ? nullptr : module_find(qtu(m_module));
    unsigned size = 0;
    module_config_t* config = module ? module_config_get(module, &size) : nullptr;

    /* module_config_get() already leaves out the internal (command-line
     * only) and obsolete options */
    QString section;
    for (unsigned i = 0; i < size; ++i)
    {
        const module_config_t& item = config[i];

        if (item.i_type == CONFIG_SECTION)
        {
            section = item.psz_text ? qtr(item.psz_text) : QString();
            continue;
        }
        if (!CONFIG_ITEM(item.i_type) || !item.psz_name || item.i_type == CONFIG_ITEM_KEY)
            continue;

        const QString name = qfu(item.psz_name);
        if (m_excluded.contains(name))
            continue;

        Option option;
        option.name = name;
        option.text = item.psz_text ? qtr(item.psz_text) : name;
        option.description = item.psz_longtext ? qtr(item.psz_longtext) : QString();
        option.section = section;

        switch (CONFIG_CLASS(item.i_type))
        {
        case CONFIG_ITEM_BOOL:
            option.type = Bool;
            option.initial = config_GetInt(item.psz_name) != 0;
            option.defaultValue = item.orig.i != 0;
            break;
        case CONFIG_ITEM_INTEGER:
            option.type = Integer;
            option.initial = qlonglong(config_GetInt(item.psz_name));
            option.defaultValue = qlonglong(item.orig.i);
            option.minimum = qlonglong(item.min.i);
            option.maximum = qlonglong(item.max.i);
            break;
        case CONFIG_ITEM_FLOAT:
            option.type = Float;
            option.initial = double(config_GetFloat(item.psz_name));
            option.defaultValue = double(item.orig.f);
            option.minimum = double(item.min.f);
            option.maximum = double(item.max.f);
            break;
        case CONFIG_ITEM_STRING:
        {
            option.type = String;
            char* value = config_GetPsz(item.psz_name);
            option.initial = qfu(value ? value : "");
            free(value);
            option.defaultValue = qfu(item.orig.psz ? item.orig.psz : "");
            break;
        }
        default:
            continue;
        }
        option.value = option.initial;
        option.choices = choicesOf(item.psz_name, item.i_type);
        m_options.push_back(std::move(option));
    }

    if (config)
        module_config_free(config);
    endResetModel();

    if (oldCount != rowCount())
        emit countChanged();
    if (wasModified)
        emit optionsChanged();
}

QVariant ModuleOptionsModel::normalized(const Option& option, const QVariant& value) const
{
    switch (option.type)
    {
    case Bool:
        return value.toBool();
    case Integer:
        return std::clamp(value.toLongLong(), option.minimum.toLongLong(), option.maximum.toLongLong());
    case Float:
        return std::clamp(value.toDouble(), option.minimum.toDouble(), option.maximum.toDouble());
    case String:
        return value.toString();
    }
    return value;
}
