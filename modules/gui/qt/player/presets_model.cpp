/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "presets_model.hpp"

#include "qt.hpp"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

#include <algorithm>

namespace {

const char GROUP[] = "Presets";

#ifdef _WIN32
const Qt::CaseSensitivity PATH_CASE = Qt::CaseInsensitive;
#else
const Qt::CaseSensitivity PATH_CASE = Qt::CaseSensitive;
#endif

// "/a/b" → "/a", "C:/a" → "C:/", "/" and "C:/" have none
QString parentFolder(const QString& folder)
{
    const qsizetype slash = folder.lastIndexOf('/');
    if (slash < 0 || slash == folder.size() - 1)
        return {};
    QString parent = folder.left(slash);
    if (parent.isEmpty() || parent.endsWith(':'))
        parent += '/';
    return parent;
}

} // namespace

PresetsModel::PresetsModel(QSettings* settings, QObject* parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
{
    load();
}

int PresetsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_presets.size();
}

QVariant PresetsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_presets.size())
        return {};
    const Preset& preset = m_presets[index.row()];
    switch (role)
    {
    case ID:
        return preset.id;
    case Qt::DisplayRole:
    case NAME:
        return preset.name;
    case FOLDERS:
        return folders(preset.id);
    default:
        return {};
    }
}

QHash<int, QByteArray> PresetsModel::roleNames() const
{
    return {
        { ID, "id" },
        { NAME, "name" },
        { FOLDERS, "folders" },
    };
}

QString PresetsModel::removedName() const
{
    return m_removed ? m_removed->preset.name : QString();
}

QString PresetsModel::create(const QString& name, const QVariantMap& values)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    beginInsertRows({}, m_presets.size(), m_presets.size());
    m_presets.append({ id, name.trimmed(), values });
    endInsertRows();
    save();
    emit countChanged();
    return id;
}

bool PresetsModel::rename(const QString& id, const QString& name)
{
    const int row = rowOf(id);
    if (row < 0 || name.trimmed().isEmpty() || !isNameAvailable(name, id))
        return false;
    m_presets[row].name = name.trimmed();
    save();
    emit dataChanged(index(row), index(row), { Qt::DisplayRole, NAME });
    return true;
}

QString PresetsModel::duplicate(const QString& id)
{
    const int row = rowOf(id);
    if (row < 0)
        return {};
    const QString base = qtr("%1 Copy").arg(m_presets[row].name);
    QString copyName = base;
    for (int n = 2; !isNameAvailable(copyName); ++n)
        copyName = QStringLiteral("%1 %2").arg(base).arg(n);
    return create(copyName, m_presets[row].values);
}

bool PresetsModel::remove(const QString& id)
{
    const int row = rowOf(id);
    if (row < 0)
        return false;

    Removed removed{ m_presets[row], row, {} };
    for (qsizetype i = m_links.size() - 1; i >= 0; --i)
    {
        if (m_links[i].preset != id)
            continue;
        removed.links.prepend(m_links[i]);
        m_links.removeAt(i);
    }

    beginRemoveRows({}, row, row);
    m_presets.removeAt(row);
    endRemoveRows();
    m_removed = removed;
    save();
    emit countChanged();
    emit canUndoRemoveChanged();
    if (!removed.links.isEmpty())
        emitLinksChanged();
    return true;
}

void PresetsModel::undoRemove()
{
    if (!m_removed)
        return;
    Removed removed = *m_removed;
    m_removed.reset();

    const int row = std::min<int>(removed.row, m_presets.size());
    beginInsertRows({}, row, row);
    m_presets.insert(row, removed.preset);
    endInsertRows();
    // a folder attached elsewhere since keeps its new link
    for (const Link& l : removed.links)
        if (linkOf(l.folder) < 0)
            m_links.append(l);
    save();
    emit countChanged();
    emit canUndoRemoveChanged();
    emitLinksChanged();
}

void PresetsModel::dropUndo()
{
    if (!m_removed)
        return;
    m_removed.reset();
    emit canUndoRemoveChanged();
}

bool PresetsModel::isNameAvailable(const QString& name, const QString& except) const
{
    const QString trimmed = name.trimmed();
    for (const Preset& preset : m_presets)
        if (preset.id != except && preset.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return false;
    return true;
}

QString PresetsModel::name(const QString& id) const
{
    const int row = rowOf(id);
    return row < 0 ? QString() : m_presets[row].name;
}

bool PresetsModel::contains(const QString& id) const
{
    return rowOf(id) >= 0;
}

QVariantMap PresetsModel::values(const QString& id) const
{
    const int row = rowOf(id);
    return row < 0 ? QVariantMap() : m_presets[row].values;
}

void PresetsModel::setValue(const QString& id, const QString& key, const QVariant& value)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    QVariantMap& values = m_presets[row].values;
    if (values.contains(key) && values.value(key) == value)
        return;
    values.insert(key, value);
    save();
    emit valuesChanged(id);
}

void PresetsModel::clearValue(const QString& id, const QString& key)
{
    const int row = rowOf(id);
    if (row < 0 || !m_presets[row].values.remove(key))
        return;
    save();
    emit valuesChanged(id);
}

void PresetsModel::attach(const QString& folder, const QString& id)
{
    const QString path = normalizeFolder(folder);
    if (path.isEmpty() || (!id.isEmpty() && !contains(id)))
        return;
    const int i = linkOf(path);
    if (i >= 0)
    {
        if (m_links[i].preset == id)
            return;
        m_links[i].preset = id;
    }
    else
        m_links.append({ path, id });
    save();
    emitLinksChanged();
}

void PresetsModel::detach(const QString& folder)
{
    const int i = linkOf(normalizeFolder(folder));
    if (i < 0)
        return;
    m_links.removeAt(i);
    save();
    emitLinksChanged();
}

std::optional<QString> PresetsModel::link(const QString& folder) const
{
    const int i = linkOf(normalizeFolder(folder));
    if (i < 0)
        return std::nullopt;
    return m_links[i].preset;
}

PresetsModel::Resolution PresetsModel::resolve(const QString& folder) const
{
    for (QString path = normalizeFolder(folder); !path.isEmpty(); path = parentFolder(path))
    {
        const int i = linkOf(path);
        if (i >= 0)
            return { m_links[i].preset, m_links[i].folder };
    }
    return {};
}

QStringList PresetsModel::folders(const QString& id) const
{
    QStringList list;
    for (const Link& l : m_links)
        if (l.preset == id)
            list.append(l.folder);
    return list;
}

QString PresetsModel::normalizeFolder(const QString& path)
{
    if (path.trimmed().isEmpty())
        return {};
    QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (clean.size() > 1 && clean.endsWith('/') && !clean.endsWith(QLatin1String(":/")))
        clean.chop(1);
    if (clean.endsWith(':'))
        clean += '/';
    return clean;
}

void PresetsModel::emitLinksChanged()
{
    emit linksChanged();
    if (!m_presets.isEmpty())
        emit dataChanged(index(0), index(m_presets.size() - 1), { FOLDERS });
}

int PresetsModel::rowOf(const QString& id) const
{
    if (id.isEmpty())
        return -1;
    for (int i = 0; i < m_presets.size(); ++i)
        if (m_presets[i].id == id)
            return i;
    return -1;
}

int PresetsModel::linkOf(const QString& folder) const
{
    for (int i = 0; i < m_links.size(); ++i)
        if (m_links[i].folder.compare(folder, PATH_CASE) == 0)
            return i;
    return -1;
}

void PresetsModel::load()
{
    if (!m_settings)
        return;
    m_settings->beginGroup(GROUP);

    const int count = m_settings->beginReadArray(QStringLiteral("List"));
    for (int i = 0; i < count; ++i)
    {
        m_settings->setArrayIndex(i);
        Preset preset;
        preset.id = m_settings->value(QStringLiteral("Id")).toString();
        preset.name = m_settings->value(QStringLiteral("Name")).toString();
        // the values as JSON: readable in the file, independent of QVariant streams
        preset.values = QJsonDocument::fromJson(m_settings->value(QStringLiteral("Values")).toString().toUtf8())
                            .object().toVariantMap();
        if (!preset.id.isEmpty() && rowOf(preset.id) < 0)
            m_presets.append(preset);
    }
    m_settings->endArray();

    const int links = m_settings->beginReadArray(QStringLiteral("Links"));
    for (int i = 0; i < links; ++i)
    {
        m_settings->setArrayIndex(i);
        const QString folder = normalizeFolder(m_settings->value(QStringLiteral("Folder")).toString());
        const QString preset = m_settings->value(QStringLiteral("Preset")).toString();
        if (!folder.isEmpty() && linkOf(folder) < 0 && (preset.isEmpty() || rowOf(preset) >= 0))
            m_links.append({ folder, preset });
    }
    m_settings->endArray();

    m_settings->endGroup();
}

void PresetsModel::save()
{
    if (!m_settings)
        return;
    m_settings->remove(GROUP);
    m_settings->beginGroup(GROUP);

    m_settings->beginWriteArray(QStringLiteral("List"), m_presets.size());
    for (int i = 0; i < m_presets.size(); ++i)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue(QStringLiteral("Id"), m_presets[i].id);
        m_settings->setValue(QStringLiteral("Name"), m_presets[i].name);
        const QJsonDocument values(QJsonObject::fromVariantMap(m_presets[i].values));
        m_settings->setValue(QStringLiteral("Values"),
                             QString::fromUtf8(values.toJson(QJsonDocument::Compact)));
    }
    m_settings->endArray();

    m_settings->beginWriteArray(QStringLiteral("Links"), m_links.size());
    for (int i = 0; i < m_links.size(); ++i)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue(QStringLiteral("Folder"), m_links[i].folder);
        m_settings->setValue(QStringLiteral("Preset"), m_links[i].preset);
    }
    m_settings->endArray();

    m_settings->endGroup();
    // no sync(): a slider writes on each step, QSettings writes the file
    // once the event loop is idle, and when it goes
}
