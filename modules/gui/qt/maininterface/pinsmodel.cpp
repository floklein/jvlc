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

#include "pinsmodel.hpp"

#include "medialibrary/mlqmltypes.hpp"

#include <QSettings>

#include <iterator>

namespace {

// the type names stored in the settings
const char *const typeNames[] = { "folder", "playlist", "stream", "music-folder" };

bool typeFromName(const QString &name, PinsModel::PinType *type)
{
    for (size_t i = 0; i < std::size(typeNames); ++i)
    {
        if (name == QLatin1String(typeNames[i]))
        {
            *type = static_cast<PinsModel::PinType>(i);
            return true;
        }
    }
    return false;
}

}

PinsModel::PinsModel(QSettings *settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
{
    load();
}

bool PinsModel::pin(PinType type, const QString &key, const QString &label)
{
    if (key.isEmpty() || isPinned(type, key))
        return false;

    const int row = m_pins.size();
    beginInsertRows({}, row, row);
    m_pins.push_back({type, key, label});
    endInsertRows();
    emit countChanged();
    save();
    return true;
}

bool PinsModel::unpin(PinType type, const QString &key)
{
    return remove(indexOf(type, key));
}

bool PinsModel::isPinned(PinType type, const QString &key) const
{
    return indexOf(type, key) != -1;
}

int PinsModel::indexOf(PinType type, const QString &key) const
{
    for (int row = 0; row < m_pins.size(); ++row)
    {
        if (m_pins[row].type == type && m_pins[row].key == key)
            return row;
    }
    return -1;
}

bool PinsModel::move(int from, int to)
{
    if (from < 0 || from >= m_pins.size() || to < 0 || to >= m_pins.size() || from == to)
        return false;

    // beginMoveRows() wants the row before which the moved row lands
    beginMoveRows({}, from, from, {}, (to > from) ? to + 1 : to);
    m_pins.move(from, to);
    endMoveRows();
    save();
    return true;
}

bool PinsModel::remove(int row)
{
    if (row < 0 || row >= m_pins.size())
        return false;

    beginRemoveRows({}, row, row);
    m_pins.removeAt(row);
    endRemoveRows();
    emit countChanged();
    save();
    return true;
}

bool PinsModel::rename(PinType type, const QString &key, const QString &label)
{
    const int row = indexOf(type, key);
    if (row == -1)
        return false;

    if (m_pins[row].label != label)
    {
        m_pins[row].label = label;
        const QModelIndex modelIndex = index(row);
        emit dataChanged(modelIndex, modelIndex, { Qt::DisplayRole, LABEL });
        save();
    }
    return true;
}

int PinsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_pins.size();
}

QVariant PinsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_pins.size())
        return {};

    const Pin &pin = m_pins[index.row()];
    switch (role)
    {
    case Qt::DisplayRole:
    case LABEL:
        return pin.label;
    case TYPE:
        return pin.type;
    case KEY:
        return pin.key;
    case ML_ID:
    {
        if (pin.type == STREAM)
            return {};
        const MLItemId id = MLItemId::fromString(pin.key);
        if (!id.isValid())
            return {};
        return QVariant::fromValue(id);
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> PinsModel::roleNames() const
{
    return {
        { TYPE, QByteArrayLiteral("type") },
        { KEY, QByteArrayLiteral("key") },
        { LABEL, QByteArrayLiteral("label") },
        { ML_ID, QByteArrayLiteral("mlId") }
    };
}

void PinsModel::load()
{
    if (!m_settings)
        return;

    const int size = m_settings->beginReadArray(QStringLiteral("Pins"));
    for (int i = 0; i < size; ++i)
    {
        m_settings->setArrayIndex(i);
        PinType type;
        const QString key = m_settings->value(QStringLiteral("Key")).toString();
        if (!typeFromName(m_settings->value(QStringLiteral("Type")).toString(), &type)
            || key.isEmpty() || isPinned(type, key))
            continue;
        m_pins.push_back({type, key, m_settings->value(QStringLiteral("Label")).toString()});
    }
    m_settings->endArray();
}

void PinsModel::save()
{
    if (!m_settings)
        return;

    m_settings->remove(QStringLiteral("Pins"));
    m_settings->beginWriteArray(QStringLiteral("Pins"), m_pins.size());
    for (int i = 0; i < m_pins.size(); ++i)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue(QStringLiteral("Type"), QLatin1String(typeNames[m_pins[i].type]));
        m_settings->setValue(QStringLiteral("Key"), m_pins[i].key);
        m_settings->setValue(QStringLiteral("Label"), m_pins[i].label);
    }
    m_settings->endArray();
    m_settings->sync();
}
