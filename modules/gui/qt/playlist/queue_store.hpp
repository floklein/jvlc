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

#ifndef VLC_QT_QUEUE_STORE_HPP_
#define VLC_QT_QUEUE_STORE_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <chrono>

namespace vlc {
namespace playlist {

class PlaylistController;

/**
 * Keeps the Queue and its current item across restarts.
 *
 * The Queue is written to a JSON file shortly after each change and when the
 * store is destroyed, and read back by restore().
 */
class QueueStore : public QObject
{
    Q_OBJECT

public:
    /* controller must outlive the store */
    QueueStore(PlaylistController *controller, const QString &filePath,
               QObject *parent = nullptr);
    ~QueueStore();

    /**
     * Fill the Queue with the saved items and select the saved current item,
     * without starting playback. Does nothing if the Queue is not empty.
     *
     * @return true if items were restored
     */
    bool restore();

    /* Write the Queue now */
    void save();

    /* Delay between a change of the Queue and its write */
    void setSaveDelay(std::chrono::milliseconds delay);

    /* When disabled (the user turned off saving recently played media),
     * nothing is restored and the saved Queue is deleted instead of written */
    void setEnabled(bool enabled);

private:
    QPointer<PlaylistController> m_controller;
    const QString m_filePath;
    QTimer m_saveTimer;
    bool m_enabled = true;
};

} // namespace playlist
} // namespace vlc

#endif
