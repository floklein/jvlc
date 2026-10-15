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

#include <QCollator>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>

#include <vlc_configuration.h>
#include <vlc_plugin.h>

#include <algorithm>
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

/* Filter chains are written "a:b", sometimes "a,b" by hand. */
QStringList splitChain(const QString& chain)
{
    static const QRegularExpression separators(QStringLiteral("[:,]"));
    QStringList filters = chain.split(separators, Qt::SkipEmptyParts);
    for (QString& filter : filters)
        filter = filter.trimmed();
    filters.removeAll(QString());
    return filters;
}

QVariantList stringChoices(const char* name)
{
    QVariantList list;
    char** values = nullptr;
    char** texts = nullptr;
    const ssize_t count = config_GetPszChoices(name, &values, &texts);
    for (ssize_t i = 0; i < count; ++i)
    {
        const QString value = qfu(values[i]);
        const QString text = texts[i] ? qfu(texts[i]) : QString();
        list.append(SettingsModel::choice(value, text.isEmpty() ? value : text));
        free(values[i]);
        free(texts[i]);
    }
    free(values);
    free(texts);
    return list;
}

QVariantList intChoices(const char* name)
{
    QVariantList list;
    int64_t* values = nullptr;
    char** texts = nullptr;
    const ssize_t count = config_GetIntChoices(name, &values, &texts);
    for (ssize_t i = 0; i < count; ++i)
    {
        const QString text = texts[i] ? qfu(texts[i]) : QString();
        list.append(SettingsModel::choice(static_cast<int>(values[i]),
                                          text.isEmpty() ? QString::number(values[i]) : text));
        free(texts[i]);
    }
    free(values);
    free(texts);
    return list;
}

/* the chains the video outputs run, whose filters follow the variables of
 * the output; audio filters read their options when they start */
bool isLiveChain(const QString& chain)
{
    return chain == QLatin1String("video-filter") || chain == QLatin1String("sub-source")
        || chain == QLatin1String("sub-filter");
}

QString groupKey(const QString& page, const QString& group)
{
    return page + QLatin1Char('/') + group;
}

const QString EXPANDED_KEY = QStringLiteral("Settings/advanced-expanded");

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

    m_choicesLookup = &SettingsModel::configChoices;
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

QVariantList SettingsModel::configChoices(const QString& option)
{
    const QByteArray name = option.toUtf8();
    switch (config_GetType(name.constData()))
    {
    case VLC_VAR_STRING:
        return stringChoices(name.constData());
    case VLC_VAR_INTEGER:
        return intChoices(name.constData());
    default:
        return {};
    }
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
    row.options = options;
}

void SettingsModel::bindFilter(Row& row, const QString& chainOption, const QString& filter)
{
    const QByteArray name = chainOption.toUtf8();

    row.isAvailable = [name]() {
        return config_GetType(name.constData()) == VLC_VAR_STRING;
    };
    row.read = [name, filter]() {
        return QVariant(splitChain(readConfig(name.constData()).toString()).contains(filter));
    };
    row.readDefault = [name, filter]() {
        return QVariant(splitChain(readConfigDefault(name.constData()).toString()).contains(filter));
    };
    row.write = [this, name, chainOption, filter](const QVariant& value) {
        QStringList filters = splitChain(readConfig(name.constData()).toString());
        filters.removeAll(filter);
        if (value.toBool())
            filters.append(filter);
        if (!writeConfig(name.constData(), filters.join(QLatin1Char(':'))))
            return false;
        if (m_liveFilters.toggle)
            m_liveFilters.toggle(chainOption, filter, value.toBool());
        return true;
    };
    row.savesConfig = true;
    // the chain is shared by every filter, it keeps its own row
    row.options.clear();
    row.filterToggle = chainOption + QLatin1Char(':') + filter;
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
    QVector<Row> rows;
    rows.append(std::move(row));
    appendRows(rows);
}

void SettingsModel::appendRows(QVector<Row>& rows)
{
    if (rows.isEmpty())
        return;

    const int count = m_entries.count();
    beginInsertRows({}, count, count + rows.count() - 1);
    for (Row& row : rows)
    {
        Entry entry;
        entry.row = std::move(row);
        if (entry.row.effect != Immediate && isAvailable(entry))
            entry.startValue = readValue(entry);
        m_entries.append(std::move(entry));
    }
    endInsertRows();
}

void SettingsModel::addAdvancedRows(const QVector<SettingsOption>& options)
{
    // the options of the rows already there
    QSet<QString> used;
    // and the filters a row already turns on and off
    QSet<QString> usedFilters;
    for (const Entry& entry : m_entries)
    {
        for (const QString& name : entry.row.options)
            used.insert(name);
        if (!entry.row.filterToggle.isEmpty())
            usedFilters.insert(entry.row.filterToggle);
    }

    QHash<QString, int> aliasOf;
    const QVector<SettingsRegistry::Alias>& aliases = SettingsRegistry::aliases();
    for (int i = 0; i < aliases.count(); ++i)
        for (const QString& name : aliases.at(i).names)
            aliasOf.insert(name, i);

    struct Pending
    {
        const SettingsOption* option;
        SettingsRegistry::Placement placement;
        QStringList alsoUsedBy;
    };
    QVector<Pending> pending;
    QHash<QString, int> pendingOf;
    // the names and the plugins of each alias found
    QMap<int, QStringList> aliasNames;
    QMap<int, QStringList> aliasUsers;

    for (const SettingsOption& option : options)
    {
        // a filter plugin without options: its group, for the filter toggle
        if (option.name.isEmpty())
        {
            if (option.filterChain.isEmpty())
                continue;
            SettingsRegistry::Placement placement = SettingsRegistry::place(option);
            if (!placement.page.isEmpty() && !placement.group.isEmpty())
                pending.append({ &option, std::move(placement), {} });
            continue;
        }

        if (used.contains(option.name))
            continue;

        const auto alias = aliasOf.constFind(option.name);
        if (alias != aliasOf.cend())
        {
            QStringList& names = aliasNames[alias.value()];
            if (!names.contains(option.name))
                names.append(option.name);
            QStringList& users = aliasUsers[alias.value()];
            const QString user = option.moduleLabel.isEmpty() ? option.module : option.moduleLabel;
            if (!users.contains(user))
                users.append(user);
            continue;
        }

        // the same option declared by another plugin: one row
        const auto known = pendingOf.constFind(option.name);
        if (known != pendingOf.cend())
        {
            Pending& first = pending[known.value()];
            if (option.module != first.option->module)
                first.alsoUsedBy.append(option.moduleLabel.isEmpty() ? option.module : option.moduleLabel);
            continue;
        }

        SettingsRegistry::Placement placement = SettingsRegistry::place(option);
        if (placement.page.isEmpty())
            continue;

        pendingOf.insert(option.name, pending.count());
        pending.append({ &option, std::move(placement), {} });
    }

    struct Group
    {
        QString id;
        QString label;
        QString filterChain;
        QVector<Row> rows;
    };
    struct Page
    {
        QVector<Row> rows;
        QVector<Group> groups;
    };
    QHash<QString, Page> pages;
    QVector<Row> shortcuts;

    // the group of a plugin on its page, added the first time
    const auto groupOf = [&pages](const SettingsRegistry::Placement& placement,
                                  const QString& filterChain) -> Group& {
        QVector<Group>& groups = pages[placement.page].groups;
        auto group = std::find_if(groups.begin(), groups.end(), [&placement](const Group& g) {
            return g.id == placement.group;
        });
        if (group != groups.end())
            return *group;
        groups.append({ placement.group, placement.groupLabel, filterChain, {} });
        return groups.last();
    };

    for (const Pending& entry : pending)
    {
        const SettingsOption& option = *entry.option;
        const SettingsRegistry::Placement& placement = entry.placement;

        if (option.name.isEmpty())
        {
            groupOf(placement, option.filterChain);
            continue;
        }

        Row row;
        row.id = option.name;
        row.page = placement.page;
        row.section = placement.section;
        row.label = option.label.isEmpty() ? option.name : option.label;
        row.help = option.help;
        if (!entry.alsoUsedBy.isEmpty())
        {
            if (!row.help.isEmpty())
                row.help += QStringLiteral("\n\n");
            row.help += qtr("Also used by: %1").arg(entry.alsoUsedBy.join(QStringLiteral(", ")));
        }
        row.advanced = true;
        row.group = placement.group;
        row.groupLabel = placement.groupLabel;
        // the title of a plugin group stands for the sections of the plugin
        if (!row.group.isEmpty())
            row.section.clear();
        row.developer = placement.developer;
        row.searchOnly = placement.shortcut;
        row.effect = static_cast<Effect>(placement.effect);
        row.minimum = option.minimum;
        row.maximum = option.maximum;

        const QByteArray name = option.name.toUtf8();
        /* a plugin can list the choices of its options when asked (audio
         * devices...): a drop-down when it does */
        const bool pluginListed = !option.hasChoices && !placement.group.isEmpty();
        const auto listedByPlugin = [this, &row, &option](Control unlisted) {
            row.control = Choice;
            row.lazyChoices = true;
            row.unlistedControl = unlisted;
            const QString optionName = option.name;
            row.choicesProvider = [this, optionName]() { return m_choicesLookup(optionName); };
        };
        switch (option.type)
        {
        case CONFIG_ITEM_BOOL:
            row.control = Toggle;
            break;
        case CONFIG_ITEM_INTEGER:
            row.control = option.hasChoices ? Choice : Number;
            if (option.hasChoices)
                row.choicesProvider = [name]() { return intChoices(name.constData()); };
            else if (pluginListed)
                listedByPlugin(Number);
            break;
        case CONFIG_ITEM_RGB:
            row.control = Color;
            break;
        case CONFIG_ITEM_FLOAT:
            row.control = Number;
            row.step = 0.;
            break;
        case CONFIG_ITEM_PASSWORD:
            row.control = Password;
            break;
        case CONFIG_ITEM_LOADFILE:
            row.control = OpenFile;
            break;
        case CONFIG_ITEM_SAVEFILE:
            row.control = SaveFile;
            break;
        case CONFIG_ITEM_DIRECTORY:
            row.control = Directory;
            break;
        case CONFIG_ITEM_KEY:
            row.control = Shortcut;
            break;
        case CONFIG_ITEM_MODULE:
        case CONFIG_ITEM_MODULE_CAT:
            row.control = Choice;
            row.choicesProvider = [name]() { return stringChoices(name.constData()); };
            break;
        case CONFIG_ITEM_STRING:
            row.control = option.hasChoices ? Choice : Text;
            if (option.hasChoices)
                row.choicesProvider = [name]() { return stringChoices(name.constData()); };
            else if (pluginListed)
                listedByPlugin(Text);
            break;
        default:
            // fonts and module lists are typed in
            row.control = Text;
            break;
        }

        bindConfig(row, { option.name });

        /* the options of a running video or subtitle filter follow the
         * player's variables */
        if (m_liveFilters.option && isLiveChain(option.filterChain))
        {
            row.effect = Immediate;
            const QString chain = option.filterChain;
            const QString optionName = option.name;
            row.write = [this, write = std::move(row.write), chain, optionName](const QVariant& value) {
                if (!write(value))
                    return false;
                m_liveFilters.option(chain, optionName, value);
                return true;
            };
        }

        if (placement.shortcut)
        {
            shortcuts.append(std::move(row));
            continue;
        }

        if (placement.group.isEmpty())
        {
            pages[placement.page].rows.append(std::move(row));
            continue;
        }

        groupOf(placement, option.filterChain).rows.append(std::move(row));
    }

    // one row for the options known under several names
    for (auto it = aliasUsers.cbegin(); it != aliasUsers.cend(); ++it)
    {
        const SettingsRegistry::Alias& alias = aliases.at(it.key());
        Row row;
        row.id = QString::fromLatin1(alias.id);
        row.page = QString::fromLatin1(alias.page);
        row.section = qtr("Visualizations");
        row.label = qtr(alias.label);
        row.help = qtr("Used by: %1").arg(it.value().join(QStringLiteral(", ")));
        row.advanced = true;
        row.control = Number;
        row.effect = NextFile;
        row.minimum = 1.;
        row.maximum = 65535.;
        bindConfig(row, aliasNames.value(it.key()));
        pages[row.page].rows.append(std::move(row));
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    QVector<Row> rows;
    for (const QString& pageId : SettingsRegistry::pages())
    {
        auto pageIt = pages.find(pageId);
        if (pageIt == pages.end())
            continue;
        Page& page = pageIt.value();

        Row header;
        header.id = QStringLiteral("advanced:") + pageId;
        header.page = pageId;
        header.label = qtr("Advanced Options");
        header.kind = AdvancedHeader;
        header.advanced = true;
        rows.append(std::move(header));

        for (Row& row : page.rows)
            rows.append(std::move(row));

        std::sort(page.groups.begin(), page.groups.end(), [&collator](const Group& a, const Group& b) {
            return collator.compare(a.label, b.label) < 0;
        });
        for (Group& group : page.groups)
        {
            Row groupHeader;
            groupHeader.id = QStringLiteral("group:%1:%2").arg(pageId, group.id);
            groupHeader.page = pageId;
            groupHeader.label = group.label;
            groupHeader.kind = GroupHeader;
            groupHeader.advanced = true;
            groupHeader.group = group.id;
            groupHeader.groupLabel = group.label;
            rows.append(std::move(groupHeader));

            // a filter is used once it is in its chain
            if (!group.filterChain.isEmpty()
                && !usedFilters.contains(group.filterChain + QLatin1Char(':') + group.id))
            {
                Row filter;
                filter.id = QStringLiteral("filter:") + group.id;
                filter.page = pageId;
                filter.label = qtr("Use this filter");
                filter.help = qtr("Adds “%1” to the filters VLC applies.").arg(group.label);
                filter.advanced = true;
                filter.group = group.id;
                filter.groupLabel = group.label;
                filter.effect = m_liveFilters.toggle ? Immediate : NextFile;
                bindFilter(filter, group.filterChain, group.id);
                rows.append(std::move(filter));
            }

            for (Row& row : group.rows)
                rows.append(std::move(row));
        }
    }
    for (Row& row : shortcuts)
        rows.append(std::move(row));

    appendRows(rows);
}

void SettingsModel::setStateSettings(QSettings* settings)
{
    m_stateSettings = settings;
    if (!settings)
        return;
    const QStringList pages = settings->value(EXPANDED_KEY).toStringList();
    m_expandedPages = QSet<QString>(pages.cbegin(), pages.cend());
    emit expansionChanged();
}

bool SettingsModel::isAdvancedExpanded(const QString& page) const
{
    return m_expandedPages.contains(page);
}

void SettingsModel::setAdvancedExpanded(const QString& page, bool expanded)
{
    if (isAdvancedExpanded(page) == expanded)
        return;
    if (expanded)
        m_expandedPages.insert(page);
    else
        m_expandedPages.remove(page);

    if (m_stateSettings)
    {
        QStringList pages = m_expandedPages.values();
        pages.sort();
        if (pages.isEmpty())
            m_stateSettings->remove(EXPANDED_KEY);
        else
            m_stateSettings->setValue(EXPANDED_KEY, pages);
    }

    headerChanged(AdvancedHeader, page, {});
    emit expansionChanged();
}

bool SettingsModel::isGroupExpanded(const QString& page, const QString& group) const
{
    return m_expandedGroups.contains(groupKey(page, group));
}

void SettingsModel::setGroupExpanded(const QString& page, const QString& group, bool expanded)
{
    if (isGroupExpanded(page, group) == expanded)
        return;
    if (expanded)
        m_expandedGroups.insert(groupKey(page, group));
    else
        m_expandedGroups.remove(groupKey(page, group));

    headerChanged(GroupHeader, page, group);
    emit expansionChanged();
}

void SettingsModel::headerChanged(Kind kind, const QString& page, const QString& group)
{
    for (int i = 0; i < m_entries.count(); ++i)
    {
        const Row& row = m_entries.at(i).row;
        if (row.kind == kind && row.page == page && row.group == group)
        {
            const QModelIndex modelIndex = index(i);
            emit dataChanged(modelIndex, modelIndex, { ExpandedRole });
        }
    }
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
        return row.labelProvider ? row.labelProvider() : row.label;
    case HelpRole:
        return row.helpProvider ? row.helpProvider() : row.help;
    case ControlRole:
        if (row.lazyChoices && choicesOf(entry).isEmpty())
            return row.unlistedControl;
        return row.control;
    case ValueRole:
        return readValue(entry);
    case DefaultValueRole:
        return readDefault(entry);
    case IsDefaultRole:
        return sameValue(readValue(entry), readDefault(entry));
    case ChoicesRole:
        return choicesOf(entry);
    case KnownChoicesRole:
        if (row.lazyChoices && !entry.choicesRead)
            return QVariantList();
        return choicesOf(entry);
    case PanelRole:
        return row.panel;
    case PendingRole:
        return row.effect != Immediate && isAvailable(entry)
            && !sameValue(readValue(entry), entry.startValue);
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
    case KindRole:
        return row.kind;
    case AdvancedRole:
        return row.advanced;
    case GroupRole:
        return row.group;
    case GroupLabelRole:
        return row.groupLabel;
    case SearchOnlyRole:
        return row.searchOnly;
    case ButtonTextRole:
        return row.buttonText;
    case ConfirmTextRole:
        return row.confirmText;
    case OptionsRole:
        return row.options;
    case ExpandedRole:
        if (row.kind == AdvancedHeader)
            return isAdvancedExpanded(row.page);
        if (row.kind == GroupHeader)
            return isGroupExpanded(row.page, row.group);
        return false;
    default:
        return {};
    }
}

QVariantList SettingsModel::choicesOf(const Entry& entry) const
{
    if (!entry.row.choicesProvider)
        return entry.row.choices;
    /* the lists of modules are long to build: keep them, except the ones
     * that change while the window is open (screens) */
    if (!entry.row.advanced)
        return entry.row.choicesProvider();
    if (!entry.choicesRead)
    {
        entry.choices = entry.row.choicesProvider();
        entry.choicesRead = true;
    }
    return entry.choices;
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
        { KindRole, "kind" },
        { AdvancedRole, "advanced" },
        { GroupRole, "group" },
        { GroupLabelRole, "groupLabel" },
        { SearchOnlyRole, "searchOnly" },
        { OptionsRole, "options" },
        { ExpandedRole, "expanded" },
        { ButtonTextRole, "buttonText" },
        { ConfirmTextRole, "confirmText" },
        { KnownChoicesRole, "knownChoices" },
        { PanelRole, "panel" },
        { PendingRole, "pending" },
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

void SettingsModel::trigger(const QString& id)
{
    const int row = rowOf(id);
    if (row < 0 || !m_entries.at(row).row.action)
        return;
    m_entries.at(row).row.action();
}

void SettingsModel::resetAll()
{
    // the VLC options of the settings shared by all Profiles are kept
    QVector<QPair<int, QVariant>> shared;
    for (int i = 0; i < m_entries.count(); ++i)
    {
        const Entry& entry = m_entries.at(i);
        if (entry.row.kind == Setting && entry.row.allProfiles && entry.row.savesConfig
            && isAvailable(entry))
            shared.append({ i, readValue(entry) });
    }

    // every VLC option of this Profile, hotkeys included
    config_ResetAll();

    for (const auto& [row, value] : shared)
        m_entries.at(row).row.write(value);

    // the settings stored elsewhere
    for (const Entry& entry : m_entries)
    {
        const Row& row = entry.row;
        if (row.kind != Setting || row.allProfiles || row.savesConfig || !isAvailable(entry))
            continue;
        const QVariant defaultValue = readDefault(entry);
        if (defaultValue.isValid() && !sameValue(readValue(entry), defaultValue))
            row.write(defaultValue);
    }

    m_saveTimer.start();

    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(m_entries.count() - 1));
    updateRestartRequired();
    emit allReset();
}

bool SettingsModel::restartRequired() const
{
    return m_restartRequired;
}

QString SettingsModel::profileName() const
{
    return m_profileName;
}

void SettingsModel::setProfileName(const QString& name)
{
    if (name == m_profileName)
        return;
    m_profileName = name;
    emit profileNameChanged();
}

void SettingsModel::refresh()
{
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(m_entries.count() - 1));
    updateRestartRequired();
}

void SettingsModel::setLiveFilters(LiveFilters live)
{
    m_liveFilters = std::move(live);
}

void SettingsModel::setChoicesLookup(std::function<QVariantList(const QString&)> lookup)
{
    m_choicesLookup = std::move(lookup);
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
    if (row.kind != Setting)
        return true;
    if (row.control == Action)
        return static_cast<bool>(row.action) && (!row.isAvailable || row.isAvailable());
    if (row.control == Panel)
        return !row.isAvailable || row.isAvailable();
    if (!row.read || !row.write)
        return false;
    return !row.isAvailable || row.isAvailable();
}

QVariant SettingsModel::readValue(const Entry& entry) const
{
    if (!entry.row.read || !isAvailable(entry))
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
    if (!entry.row.write || !isAvailable(entry))
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
    sort(0);
}

void SettingsFilterModel::setSourceModel(QAbstractItemModel* model)
{
    disconnect(m_expansionConnection);
    QSortFilterProxyModel::setSourceModel(model);
    if (auto settings = qobject_cast<SettingsModel*>(model))
        m_expansionConnection = connect(settings, &SettingsModel::expansionChanged,
                                        this, &SettingsFilterModel::refilter);
    sort(0);
}

int SettingsFilterModel::indexOf(const QString& id) const
{
    for (int i = 0; i < rowCount(); ++i)
        if (index(i, 0).data(SettingsModel::IdRole).toString() == id)
            return i;
    return -1;
}

void SettingsFilterModel::refilter()
{
    invalidateFilter();
    emit countChanged();
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

    const int kind = index.data(SettingsModel::KindRole).toInt();

    // search covers every page, Advanced Options and the shortcut actions
    if (!m_searchWords.isEmpty())
        return kind == SettingsModel::Setting && matchesSearch(index);

    const QString page = index.data(SettingsModel::PageRole).toString();
    if (page != m_page)
        return false;
    if (index.data(SettingsModel::SearchOnlyRole).toBool())
        return false;
    if (kind == SettingsModel::AdvancedHeader || !index.data(SettingsModel::AdvancedRole).toBool())
        return true;

    // Advanced Options
    const auto settings = qobject_cast<const SettingsModel*>(sourceModel());
    if (!settings || !settings->isAdvancedExpanded(page))
        return false;
    const QString group = index.data(SettingsModel::GroupRole).toString();
    if (kind == SettingsModel::GroupHeader || group.isEmpty())
        return true;
    return settings->isGroupExpanded(page, group);
}

bool SettingsFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto tier = [](const QModelIndex& index) {
        if (index.data(SettingsModel::KindRole).toInt() == SettingsModel::AdvancedHeader)
            return 1;
        if (!index.data(SettingsModel::AdvancedRole).toBool())
            return 0;
        return index.data(SettingsModel::GroupRole).toString().isEmpty() ? 2 : 3;
    };
    const int leftTier = tier(left);
    const int rightTier = tier(right);
    if (leftTier != rightTier)
        return leftTier < rightTier;
    return left.row() < right.row();
}

bool SettingsFilterModel::matchesSearch(const QModelIndex& index) const
{
    QStringList texts {
        index.data(SettingsModel::LabelRole).toString(),
        index.data(SettingsModel::SectionRole).toString(),
        index.data(SettingsModel::HelpRole).toString(),
        index.data(SettingsModel::GroupLabelRole).toString(),
    };
    texts += index.data(SettingsModel::OptionsRole).toStringList();
    const QVariantList choices = index.data(SettingsModel::KnownChoicesRole).toList();
    for (const QVariant& choice : choices)
        texts.append(choice.toMap().value(QStringLiteral("label")).toString());

    const QString haystack = texts.join(QLatin1Char('\n'));
    for (const QString& word : m_searchWords)
        if (!haystack.contains(word, Qt::CaseInsensitive))
            return false;
    return true;
}
