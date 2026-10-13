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

#include "queue_store.hpp"

#include "playlist_controller.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

/*
 * File format:
 * {
 *   "version": 1,
 *   "current": <index of the current item, -1 if none>,
 *   "items": [ { "mrl": "...", "name": "...", "duration": <ms> }, ... ]
 * }
 *
 * The file is untrusted input when it is read back: only plain MRLs and
 * display metadata are kept, never input options (":sout=...", etc.), and
 * everything is checked and bounded. Credentials embedded in MRLs are never
 * written.
 */

namespace vlc {
namespace playlist {

static const int FORMAT_VERSION = 1;

static const qint64 MAX_FILE_SIZE = 32 * 1024 * 1024;
static const int MAX_ITEMS = 100000;
static const int MAX_MRL_LENGTH = 8192;
static const int MAX_NAME_LENGTH = 1024;
static const qint64 MAX_DURATION_MS = qint64(100) * 365 * 24 * 3600 * 1000;

/* The MRL without the "user:password@" part of its authority, or a null
 * string if it is not an absolute URL */
static QString sanitizedMrl(const QString &mrl, bool *hadCredentials = nullptr)
{
    if (hadCredentials)
        *hadCredentials = false;
    if (mrl.isEmpty() || mrl.size() > MAX_MRL_LENGTH)
        return {};

    QUrl url(mrl, QUrl::TolerantMode);
    if (!url.isValid() || url.scheme().isEmpty())
        return {};

    if (url.userInfo().isEmpty())
        return mrl;

    if (hadCredentials)
        *hadCredentials = true;
    url.setUserInfo(QString());
    return url.toString(QUrl::FullyEncoded);
}

QueueStore::QueueStore(PlaylistController *controller, const QString &filePath,
                       QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_filePath(filePath)
{
    assert(controller);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(std::chrono::seconds(2));
    connect(&m_saveTimer, &QTimer::timeout, this, &QueueStore::save);

    const auto scheduleSave = [this]() { m_saveTimer.start(); };
    connect(controller, &PlaylistController::itemsReset, this, scheduleSave);
    connect(controller, &PlaylistController::itemsAdded, this, scheduleSave);
    connect(controller, &PlaylistController::itemsMoved, this, scheduleSave);
    connect(controller, &PlaylistController::itemsRemoved, this, scheduleSave);
    connect(controller, &PlaylistController::itemsUpdated, this, scheduleSave);
    connect(controller, &PlaylistController::currentIndexChanged, this, scheduleSave);
}

QueueStore::~QueueStore()
{
    if (m_saveTimer.isActive())
        save();
}

void QueueStore::setSaveDelay(std::chrono::milliseconds delay)
{
    m_saveTimer.setInterval(delay);
}

void QueueStore::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
    {
        m_saveTimer.stop();
        QFile::remove(m_filePath);
    }
}

void QueueStore::save()
{
    m_saveTimer.stop();

    if (!m_enabled)
    {
        QFile::remove(m_filePath);
        return;
    }

    if (!m_controller)
        return;

    vlc_playlist_t *playlist = m_controller->getPlaylist().m_playlist;

    QJsonArray items;
    ssize_t current = -1;
    {
        vlc_playlist_locker locker(playlist);

        const ssize_t playlistCurrent = vlc_playlist_GetCurrentIndex(playlist);
        const size_t count = vlc_playlist_Count(playlist);
        for (size_t i = 0; i < count && items.size() < MAX_ITEMS; ++i)
        {
            input_item_t *media =
                vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist, i));

            vlc_mutex_locker itemLocker(&media->lock);

            bool hadCredentials;
            const QString mrl = sanitizedMrl(qfu(media->psz_uri), &hadCredentials);
            if (mrl.isNull())
                continue;

            QJsonObject item;
            item.insert("mrl", mrl);
            /* the name may be the MRL itself, credentials included: drop it
             * then, the core names the restored item after its MRL */
            const QString name = qfu(media->psz_name);
            if (!hadCredentials && !name.isEmpty() && name.size() <= MAX_NAME_LENGTH)
                item.insert("name", name);
            if (media->i_duration > 0)
                item.insert("duration", static_cast<qint64>(MS_FROM_VLC_TICK(media->i_duration)));

            if (static_cast<ssize_t>(i) == playlistCurrent)
                current = items.size();
            items.append(item);
        }
    }

    QJsonObject root;
    root.insert("version", FORMAT_VERSION);
    root.insert("current", static_cast<qint64>(current));
    root.insert("items", items);

    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

bool QueueStore::restore()
{
    if (!m_enabled || !m_controller)
        return false;

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MAX_FILE_SIZE)
        return false;

    const QJsonDocument document = QJsonDocument::fromJson(file.read(MAX_FILE_SIZE));
    if (!document.isObject())
        return false;
    const QJsonObject root = document.object();
    if (root.value("version").toInt() != FORMAT_VERSION)
        return false;

    const QJsonArray items = root.value("items").toArray();
    const qint64 savedCurrent = root.value("current").toInteger(-1);

    QVector<Media> medias;
    qint64 current = -1;
    for (qsizetype i = 0; i < items.size() && medias.size() < MAX_ITEMS; ++i)
    {
        const QJsonObject item = items.at(i).toObject();

        /* only plain MRLs, without credentials */
        const QJsonValue mrlValue = item.value("mrl");
        if (!mrlValue.isString())
            continue;
        const QString mrl = sanitizedMrl(mrlValue.toString());
        if (mrl.isNull())
            continue;

        QString name;
        const QJsonValue nameValue = item.value("name");
        if (nameValue.isString())
            name = nameValue.toString().left(MAX_NAME_LENGTH);

        /* a null name lets the core name the item after its MRL */
        Media media(mrl, name.isEmpty() ? QString() : name);

        const QJsonValue duration = item.value("duration");
        if (duration.isDouble())
        {
            const qint64 ms = duration.toInteger(-1);
            if (ms > 0 && ms <= MAX_DURATION_MS)
                input_item_SetDuration(media.raw(), VLC_TICK_FROM_MS(ms));
        }

        if (i == savedCurrent)
            current = medias.size();
        medias.push_back(media);
    }
    if (medias.isEmpty())
        return false;

    vlc_playlist_t *playlist = m_controller->getPlaylist().m_playlist;
    vlc_playlist_locker locker(playlist);

    /* Items queued before the restore (from the command line, for example)
     * take precedence over the saved Queue */
    if (vlc_playlist_Count(playlist) > 0)
        return false;

    QVector<input_item_t *> rawMedias;
    rawMedias.reserve(medias.size());
    for (const Media &media : medias)
        rawMedias.push_back(media.raw());
    if (vlc_playlist_Append(playlist, rawMedias.constData(), rawMedias.size())
            != VLC_SUCCESS)
        return false;

    if (current >= 0)
        vlc_playlist_GoTo(playlist, current);

    return true;
}

} // namespace playlist
} // namespace vlc
