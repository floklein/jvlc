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

#include "programguidemodel.hpp"

#include <algorithm>

#include <QHash>
#include <QTimeZone>

#include <vlc_input_item.h>
#include <vlc_epg.h>

#include "qt.hpp"

ProgramGuideModel::ProgramGuideModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

namespace {

QString describe(const vlc_epg_event_t *evt)
{
    QStringList parts;
    const QString shortDescription = qfu(evt->psz_short_description).trimmed();
    const QString description = qfu(evt->psz_description).trimmed();
    if (!shortDescription.isEmpty())
        parts << shortDescription;
    if (!description.isEmpty() && description != shortDescription)
        parts << description;
    for (int i = 0; i < evt->i_description_items; ++i)
        parts << QStringLiteral("%1: %2").arg(qfu(evt->description_items[i].psz_key),
                                              qfu(evt->description_items[i].psz_value));
    return parts.join(QStringLiteral("\n\n"));
}

}

void ProgramGuideModel::update(input_item_t *item)
{
    QVector<Event> events;

    if (item)
    {
        QVector<int> channelOrder;
        QHash<int, QString> channelNames;
        QHash<QPair<int, int>, qsizetype> indexes;

        vlc_mutex_lock(&item->lock);
        const int64_t epgTime = item->i_epg_time;

        for (int i = 0; i < item->i_epg; ++i)
        {
            const vlc_epg_t *epg = item->pp_epg[i];
            const int programId = epg->i_source_id;

            if (!channelNames.contains(programId))
            {
                channelOrder.append(programId);
                channelNames.insert(programId, QString());
            }
            if (channelNames[programId].isEmpty() && epg->psz_name)
                channelNames[programId] = qfu(epg->psz_name);

            for (size_t j = 0; j < epg->i_event; ++j)
            {
                const vlc_epg_event_t *evt = epg->pp_event[j];
                const int64_t end = evt->i_start + evt->i_duration;

                /* The guide shows what is on air and what comes next */
                if (epgTime > 0 && end <= epgTime)
                    continue;

                Event event;
                event.programId = programId;
                event.eventId = evt->i_id;
                event.name = qfu(evt->psz_name);
                event.description = describe(evt);
                event.start = QDateTime::fromSecsSinceEpoch(evt->i_start, QTimeZone::systemTimeZone());
                event.end = QDateTime::fromSecsSinceEpoch(end, QTimeZone::systemTimeZone());
                event.current = (epg->p_current == evt);
                event.rating = evt->i_rating;

                /* A later table of the channel updates the same event */
                const auto key = qMakePair(programId, event.eventId);
                const auto it = indexes.constFind(key);
                if (it != indexes.cend())
                {
                    event.current |= events[*it].current;
                    events[*it] = std::move(event);
                }
                else
                {
                    indexes.insert(key, events.size());
                    events.append(std::move(event));
                }
            }
        }
        vlc_mutex_unlock(&item->lock);

        for (Event &event : events)
        {
            const QString &name = channelNames[event.programId];
            event.channel = name.isEmpty() ? qtr("Channel %1").arg(event.programId) : name;
        }

        std::stable_sort(events.begin(), events.end(),
                         [&channelOrder](const Event &a, const Event &b) {
            const auto ca = channelOrder.indexOf(a.programId);
            const auto cb = channelOrder.indexOf(b.programId);
            if (ca != cb)
                return ca < cb;
            return a.start < b.start;
        });
    }

    if (events == m_events)
        return;

    const bool countChanges = events.size() != m_events.size();
    beginResetModel();
    m_events = std::move(events);
    endResetModel();
    if (countChanges)
        emit countChanged();
}

QVariant ProgramGuideModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_events.size())
        return {};

    const Event &event = m_events.at(index.row());
    switch (role)
    {
    case CHANNEL:
        return event.channel;
    case PROGRAM_ID:
        return event.programId;
    case Qt::DisplayRole:
    case NAME:
        return event.name;
    case DESCRIPTION:
        return event.description;
    case START:
        return event.start;
    case END:
        return event.end;
    case CURRENT:
        return event.current;
    case RATING:
        return event.rating;
    default:
        return {};
    }
}

int ProgramGuideModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_events.size();
}

QHash<int, QByteArray> ProgramGuideModel::roleNames() const
{
    return {
        { CHANNEL, "channel" },
        { PROGRAM_ID, "programId" },
        { NAME, "name" },
        { DESCRIPTION, "description" },
        { START, "start" },
        { END, "end" },
        { CURRENT, "current" },
        { RATING, "rating" },
    };
}
