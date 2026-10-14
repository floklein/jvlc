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

#ifndef QVLC_PROGRAM_GUIDE_MODEL_HPP_
#define QVLC_PROGRAM_GUIDE_MODEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

#include <vlc_common.h>

/**
 * The Program Guide of a TV stream: one row per event, grouped by channel
 * (in the stream's order) then sorted by start time.
 */
class ProgramGuideModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Roles
    {
        CHANNEL = Qt::UserRole + 1, //< channel name
        PROGRAM_ID,                 //< id to pass to PlayerController::changeProgram()
        NAME,
        DESCRIPTION,
        START,                      //< QDateTime
        END,                        //< QDateTime
        CURRENT,                    //< the event on air
        RATING                      //< minimum age, 0 when unknown
    };
    Q_ENUM(Roles)

    explicit ProgramGuideModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return rowCount(); }

    /// Reads the guide of an input item, or clears it with nullptr
    void update(input_item_t *item);

signals:
    void countChanged();

private:
    struct Event
    {
        QString channel;
        int programId;
        int eventId;
        QString name;
        QString description;
        QDateTime start;
        QDateTime end;
        bool current;
        int rating;

        bool operator==(const Event &other) const
        {
            return channel == other.channel && programId == other.programId
                && eventId == other.eventId && name == other.name
                && description == other.description && start == other.start
                && end == other.end && current == other.current
                && rating == other.rating;
        }
    };

    QVector<Event> m_events;
};

#endif
