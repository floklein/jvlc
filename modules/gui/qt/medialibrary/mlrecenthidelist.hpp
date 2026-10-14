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
#ifndef ML_RECENT_HIDE_LIST_HPP
#define ML_RECENT_HIDE_LIST_HPP

#include <memory>

#include <QObject>
#include <QHash>
#include <QMutex>

class QSettings;

/**
 * What Recent no longer shows, without touching the media library history
 * (clearing it would also reset play counts and progress).
 *
 * "Clear Recent" sets a hidden-before marker: media last played before it are
 * hidden. "Remove from Recent" hides one media until it is played again: the
 * hide list keeps the last played date it had when it was removed.
 *
 * Dates are seconds since the epoch, as the media library gives them.
 */
class MLRecentHideList : public QObject
{
    Q_OBJECT

public:
    /* a copy of the hide list, safe to use on the media library thread */
    struct Filter
    {
        int64_t hiddenBefore = 0;
        QHash<int64_t, int64_t> hidden; // media id -> last played date when removed

        bool isEmpty() const { return hiddenBefore == 0 && hidden.isEmpty(); }
        bool isHidden(int64_t mediaId, int64_t lastPlayed) const;
    };

    /* the current filter, readable from any thread, alive as long as needed */
    class Source
    {
    public:
        Filter get() const;

    private:
        friend class MLRecentHideList;
        mutable QMutex m_lock;
        Filter m_filter;
    };

    /* settings may be null: the hide list then lasts for this session only */
    explicit MLRecentHideList(QSettings *settings, QObject *parent = nullptr);

    std::shared_ptr<const Source> source() const { return m_source; }

    // Remove from Recent: hide media <mediaId>, last played at <lastPlayed>
    void hide(int64_t mediaId, int64_t lastPlayed);

    // Clear Recent: hide every media last played at <now> or before
    void clear(int64_t now);

signals:
    void changed();

private:
    void update(const Filter &filter);

    QSettings *m_settings;
    // written on the UI thread only
    Filter m_filter;
    std::shared_ptr<Source> m_source;
};

#endif // ML_RECENT_HIDE_LIST_HPP
