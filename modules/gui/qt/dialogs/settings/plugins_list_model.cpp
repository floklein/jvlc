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

#include "plugins_list_model.hpp"

#include <QCollator>

#include "qt.hpp"

#include <vlc_modules.h>

#include <algorithm>

PluginsListModel::PluginsListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    size_t count = 0;
    module_t** modules = module_list_get(&count);
    for (size_t i = 0; i < count; ++i)
    {
        const module_t* module = modules[i];
        if (module_is_main(module))
            continue;

        Plugin plugin;
        plugin.object = qfu(module_get_object(module));
        plugin.name = qfu(module_get_name(module, true));
        if (plugin.name.isEmpty())
            plugin.name = plugin.object;
        plugin.capability = qfu(module_get_capability(module));
        plugin.description = qfu(module_get_help(module));
        m_plugins.append(std::move(plugin));
    }
    module_list_free(modules);

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(m_plugins.begin(), m_plugins.end(), [&collator](const Plugin& a, const Plugin& b) {
        return collator.compare(a.name, b.name) < 0;
    });

    filter();
}

void PluginsListModel::filter()
{
    m_shown.clear();
    const QStringList words = m_searchPattern.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (int i = 0; i < m_plugins.count(); ++i)
    {
        const Plugin& plugin = m_plugins.at(i);
        const QString haystack = plugin.name + QLatin1Char('\n') + plugin.object
                                 + QLatin1Char('\n') + plugin.capability;
        const bool matches = std::all_of(words.cbegin(), words.cend(), [&haystack](const QString& word) {
            return haystack.contains(word, Qt::CaseInsensitive);
        });
        if (matches)
            m_shown.append(i);
    }
}

void PluginsListModel::setSearchPattern(const QString& pattern)
{
    if (pattern == m_searchPattern)
        return;
    beginResetModel();
    m_searchPattern = pattern;
    filter();
    endResetModel();
    emit searchPatternChanged();
    emit countChanged();
}

int PluginsListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_shown.count();
}

QVariant PluginsListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Plugin& plugin = m_plugins.at(m_shown.at(index.row()));
    switch (role)
    {
    case Qt::DisplayRole:
    case NameRole:
        return plugin.name;
    case ObjectRole:
        return plugin.object;
    case CapabilityRole:
        return plugin.capability;
    case DescriptionRole:
        return plugin.description;
    default:
        return {};
    }
}

QHash<int, QByteArray> PluginsListModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { ObjectRole, "object" },
        { CapabilityRole, "capability" },
        { DescriptionRole, "description" },
    };
}
