/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#include "player_controlbar_model.hpp"

#include <QMetaEnum>
#include <QJSEngine>

#include <algorithm>

#include "qt.hpp"
#include "control_list_model.hpp"

decltype (PlayerControlbarModel::playerIdentifierDictionary)
    PlayerControlbarModel::playerIdentifierDictionary {
        {Videoplayer, N_("Video player")},
        {Audioplayer, N_("Music player")}
    };

QJSValue PlayerControlbarModel::getPlaylistIdentifierListModel(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)

    static const QMetaEnum metaEnum = QMetaEnum::fromType<PlayerIdentifier>();

    QJSValue array = scriptEngine->newArray();

    int index = 0;
    for (int i = 0; i < metaEnum.keyCount(); ++i)
    {
       const int val = metaEnum.value(i);

       QJSValue obj = scriptEngine->newObject();

       obj.setProperty("identifier", val);

       QString key;
       if ( playerIdentifierDictionary.contains(static_cast<PlayerControlbarModel::PlayerIdentifier>(val)) )
       {
           key = qfut( playerIdentifierDictionary[static_cast<PlayerControlbarModel::PlayerIdentifier>(val)] );
       }
       else
       {
           key = metaEnum.key(i);
       }

       obj.setProperty("name", key);

       array.setProperty(index++, obj);
    }

    QJSValue value = scriptEngine->newObject();
    value.setProperty("model", array);

    return value;
}

PlayerControlbarModel::PlayerControlbarModel(QObject *parent) : QObject(parent)
{
    m_left = new ControlListModel(this);
    m_center = new ControlListModel(this);
    m_right = new ControlListModel(this);

    m_topLeft = new ControlListModel(this);
    m_topCenter = new ControlListModel(this);
    m_topRight = new ControlListModel(this);

    for (ControlListModel* zone : zones())
    {
        connect(zone, &ControlListModel::countChanged, this, &PlayerControlbarModel::contentChanged);
        connect(zone, &QAbstractListModel::dataChanged, this, &PlayerControlbarModel::contentChanged);
        connect(zone, &QAbstractListModel::rowsMoved, this, &PlayerControlbarModel::contentChanged);
    }
}

PlayerControlbarModel::~PlayerControlbarModel()
{
    setDirty(false);
}

bool PlayerControlbarModel::dirty() const
{
    return m_dirty;
}

bool PlayerControlbarModel::empty() const
{
    return m_empty;
}

PlayerControlbarModel::Zones PlayerControlbarModel::serializeModels() const
{
    Zones ret;
    const auto list = zones();
    for (size_t i = 0; i < list.size(); ++i)
        ret[i] = list[i]->getControls();
    return ret;
}

void PlayerControlbarModel::loadModels(const Zones &zones)
{
    const auto list = this->zones();
    for (size_t i = 0; i < list.size(); ++i)
        list[i]->setControls(zones[i]);
}

PlayerControlbarModel::Zones PlayerControlbarModel::sanitized(Zones zones)
{
    QVector<int> placed;
    bool hasMenu = false;

    for (int zone = 0; zone < ZoneCount; ++zone)
    {
        const bool topBar = (zone >= TopLeft);

        QVector<int> list;
        for (const int control : zones[zone])
        {
            if (topBar && control == ControlListModel::SEEK_BAR)
                continue;

            if (!repeatable(control))
            {
                if (placed.contains(control))
                    continue;
                placed.append(control);
            }

            if (control == ControlListModel::VLC_MENU_BUTTON)
                hasMenu = true;

            list.append(control);
        }
        zones[zone] = list;
    }

    if (!hasMenu)
        zones[TopLeft].prepend(ControlListModel::VLC_MENU_BUTTON);

    return zones;
}

ControlListModel *PlayerControlbarModel::left() const
{
    return m_left;
}

ControlListModel *PlayerControlbarModel::center() const
{
    return m_center;
}

ControlListModel *PlayerControlbarModel::right() const
{
    return m_right;
}

ControlListModel *PlayerControlbarModel::topLeft() const
{
    return m_topLeft;
}

ControlListModel *PlayerControlbarModel::topCenter() const
{
    return m_topCenter;
}

ControlListModel *PlayerControlbarModel::topRight() const
{
    return m_topRight;
}

bool PlayerControlbarModel::seekBarInline() const
{
    return m_seekBarInline;
}

void PlayerControlbarModel::setSeekBarInline(bool seekBarInline)
{
    if (seekBarInline == m_seekBarInline)
        return;

    if (seekBarInline)
    {
        m_center->insert(m_center->rowCount(), {{"id", ControlListModel::SEEK_BAR}});
    }
    else
    {
        for (ControlListModel* zone : zones())
        {
            const int index = zone->getControls().indexOf(ControlListModel::SEEK_BAR);
            if (index >= 0)
                zone->remove(index);
        }
    }
}

bool PlayerControlbarModel::repeatable(int control)
{
    switch (control)
    {
    case ControlListModel::WIDGET_SPACER:
    case ControlListModel::WIDGET_SPACER_EXTEND:
    case ControlListModel::DIVIDER:
        return true;
    default:
        return false;
    }
}

bool PlayerControlbarModel::removable(int control)
{
    return (control != ControlListModel::VLC_MENU_BUTTON);
}

bool PlayerControlbarModel::isPlaced(int control) const
{
    for (const ControlListModel* zone : zones())
    {
        if (zone->contains(control))
            return true;
    }
    return false;
}

bool PlayerControlbarModel::canAdd(int control) const
{
    return repeatable(control) || !isPlaced(control);
}

bool PlayerControlbarModel::isTopBarZone(const ControlListModel *zone) const
{
    return (zone == m_topLeft || zone == m_topCenter || zone == m_topRight);
}

bool PlayerControlbarModel::canPlace(int control, ControlListModel *zone) const
{
    if (!zone || barOf(zone)[0] == nullptr)
        return false;

    if (control == ControlListModel::SEEK_BAR && isTopBarZone(zone))
        return false;

    // the center zones keep their natural width
    if (control == ControlListModel::WIDGET_SPACER_EXTEND && (zone == m_center || zone == m_topCenter))
        return false;

    return true;
}

bool PlayerControlbarModel::addControl(ControlListModel *zone, int index, int control)
{
    if (!canAdd(control) || !canPlace(control, zone))
        return false;

    if (index < 0 || index > zone->rowCount())
        index = zone->rowCount();

    zone->insert(index, {{"id", control}});
    return true;
}

bool PlayerControlbarModel::removeControl(ControlListModel *zone, int index)
{
    if (!zone || barOf(zone)[0] == nullptr || index < 0 || index >= zone->rowCount())
        return false;

    if (!removable(zone->getControls().at(index)))
        return false;

    zone->remove(index);
    return true;
}

bool PlayerControlbarModel::moveControl(ControlListModel *from, int fromIndex,
                                        ControlListModel *to, int toIndex)
{
    if (!from || !to || barOf(from)[0] == nullptr || barOf(to)[0] == nullptr)
        return false;

    if (fromIndex < 0 || fromIndex >= from->rowCount())
        return false;

    const int control = from->getControls().at(fromIndex);

    if (from == to)
    {
        if (toIndex < 0 || toIndex >= to->rowCount())
            toIndex = to->rowCount() - 1;
        from->move(fromIndex, toIndex);
        return true;
    }

    if (!canPlace(control, to))
        return false;

    if (toIndex < 0 || toIndex > to->rowCount())
        toIndex = to->rowCount();

    // remove first, so that the item is never placed twice
    from->remove(fromIndex);
    to->insert(toIndex, {{"id", control}});
    return true;
}

QVariantMap PlayerControlbarModel::shiftControl(ControlListModel *zone, int index, int step)
{
    const auto bar = barOf(zone);
    if (bar[0] == nullptr || index < 0 || index >= zone->rowCount() || (step != -1 && step != 1))
        return {};

    const int newIndex = index + step;
    if (newIndex >= 0 && newIndex < zone->rowCount())
    {
        zone->move(index, newIndex);
        return { {"zone", QVariant::fromValue(zone)}, {"index", newIndex} };
    }

    // cross into the next zone of the same bar
    const int zoneIndex = std::find(bar.begin(), bar.end(), zone) - bar.begin();
    const int nextZoneIndex = zoneIndex + step;
    if (nextZoneIndex < 0 || nextZoneIndex >= int(bar.size()))
        return {};

    ControlListModel* nextZone = bar[nextZoneIndex];
    const int target = (step > 0) ? 0 : nextZone->rowCount();
    if (!moveControl(zone, index, nextZone, target))
        return {};

    return { {"zone", QVariant::fromValue(nextZone)}, {"index", target} };
}

std::array<ControlListModel *, PlayerControlbarModel::ZoneCount> PlayerControlbarModel::zones() const
{
    return { m_left, m_center, m_right, m_topLeft, m_topCenter, m_topRight };
}

std::array<ControlListModel *, 3> PlayerControlbarModel::barOf(const ControlListModel *zone) const
{
    if (zone == m_left || zone == m_center || zone == m_right)
        return { m_left, m_center, m_right };
    if (isTopBarZone(zone))
        return { m_topLeft, m_topCenter, m_topRight };
    return { nullptr, nullptr, nullptr };
}

bool PlayerControlbarModel::operator==(const PlayerControlbarModel &model) const
{
    // Deep comparison
    const auto list = zones();
    const auto other = model.zones();
    for (size_t i = 0; i < list.size(); ++i)
    {
        assert(list[i] && other[i]);
        if (*list[i] != *other[i])
            return false;
    }

    return true;
}

void PlayerControlbarModel::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;

    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}

void PlayerControlbarModel::contentChanged()
{
    setDirty(true);

    {
        // the Control bar shows when it holds an item
        bool empty = true;
        if ((m_left && m_left->rowCount() > 0) ||
            (m_center && m_center->rowCount() > 0) ||
            (m_right && m_right->rowCount() > 0))
            empty = false;

        if (empty != m_empty)
        {
            m_empty = empty;
            emit emptyChanged(empty);
        }
    }

    {
        const bool seekBarInline = m_left->contains(ControlListModel::SEEK_BAR)
                                   || m_center->contains(ControlListModel::SEEK_BAR)
                                   || m_right->contains(ControlListModel::SEEK_BAR);
        if (seekBarInline != m_seekBarInline)
        {
            m_seekBarInline = seekBarInline;
            emit seekBarInlineChanged();
        }
    }

    emit controlListChanged();
}
