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

#include "mlrecenthidelist.hpp"

#include <algorithm>

#include <QMutexLocker>
#include <QSettings>
#include <QStringList>

#define HIDDEN_BEFORE_KEY "Recent/HiddenBefore"
#define HIDDEN_KEY "Recent/Hidden"

bool MLRecentHideList::Filter::isHidden(int64_t mediaId, int64_t lastPlayed) const
{
    if (lastPlayed <= hiddenBefore)
        return true;
    const auto it = hidden.constFind(mediaId);
    return it != hidden.cend() && lastPlayed <= it.value();
}

MLRecentHideList::Filter MLRecentHideList::Source::get() const
{
    QMutexLocker locker(&m_lock);
    return m_filter;
}

MLRecentHideList::MLRecentHideList(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_source(std::make_shared<Source>())
{
    if (!m_settings)
        return;

    Filter filter;
    filter.hiddenBefore = m_settings->value(HIDDEN_BEFORE_KEY, 0).toLongLong();

    // "<media id>:<last played date>"
    const QStringList entries = m_settings->value(HIDDEN_KEY).toStringList();
    for (const QString &entry : entries)
    {
        const QStringList fields = entry.split(':');
        if (fields.size() != 2)
            continue;
        bool idOk, dateOk;
        const int64_t id = fields[0].toLongLong(&idOk);
        const int64_t lastPlayed = fields[1].toLongLong(&dateOk);
        if (idOk && dateOk && lastPlayed > filter.hiddenBefore)
            filter.hidden.insert(id, lastPlayed);
    }

    m_filter = filter;
    m_source->m_filter = filter;
}

void MLRecentHideList::hide(int64_t mediaId, int64_t lastPlayed)
{
    if (m_filter.isHidden(mediaId, lastPlayed))
        return;

    Filter filter = m_filter;
    filter.hidden.insert(mediaId, lastPlayed);
    update(filter);
}

void MLRecentHideList::clear(int64_t now)
{
    Filter filter;
    filter.hiddenBefore = std::max(m_filter.hiddenBefore, now);
    // the marker covers every media removed so far, keep the others
    for (auto it = m_filter.hidden.cbegin(); it != m_filter.hidden.cend(); ++it)
        if (it.value() > filter.hiddenBefore)
            filter.hidden.insert(it.key(), it.value());
    update(filter);
}

void MLRecentHideList::update(const Filter &filter)
{
    m_filter = filter;
    {
        QMutexLocker locker(&m_source->m_lock);
        m_source->m_filter = filter;
    }

    if (m_settings)
    {
        QStringList entries;
        for (auto it = filter.hidden.cbegin(); it != filter.hidden.cend(); ++it)
            entries.push_back(QStringLiteral("%1:%2").arg(it.key()).arg(it.value()));

        m_settings->setValue(HIDDEN_BEFORE_KEY, static_cast<qlonglong>(filter.hiddenBefore));
        m_settings->setValue(HIDDEN_KEY, entries);
        m_settings->sync();
    }

    emit changed();
}
