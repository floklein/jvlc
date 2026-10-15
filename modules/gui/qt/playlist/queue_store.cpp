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

#include <algorithm>
#include <vector>

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
 * everything is checked and bounded. Credentials embedded in MRLs or names
 * are never written, and the file is only readable by the user, like the
 * other history of the media they played. The rest of an MRL, its query
 * included, is kept so that signed URLs still play once restored.
 */

namespace vlc {
namespace playlist {

static const int FORMAT_VERSION = 1;

static const qint64 MAX_FILE_SIZE = 32 * 1024 * 1024;
static const int MAX_ITEMS = 100000;
static const int MAX_MRL_LENGTH = 8192;
static const int MAX_NAME_LENGTH = 1024;
static const qint64 MAX_DURATION_MS = qint64(100) * 365 * 24 * 3600 * 1000;

static bool isSchemeChar(QChar c)
{
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z')
        || (c >= u'0' && c <= u'9') || c == u'+' || c == u'-' || c == u'.';
}

/* The position of the "user:password@" part of the authority of the URL whose
 * "://" is at schemeEnd, found as the core finds it (vlc_UrlParse): the
 * authority ends at the first '/', '?' or '#', the user information at its
 * last '@'. Returns the end of the user information ('@' included), or -1 */
static qsizetype userInfoEnd(const QString &url, qsizetype schemeEnd)
{
    const qsizetype authority = schemeEnd + 3;
    qsizetype end = authority;
    while (end < url.size() && url.at(end) != u'/' && url.at(end) != u'?'
           && url.at(end) != u'#')
        ++end;
    for (qsizetype i = end - 1; i >= authority; --i)
        if (url.at(i) == u'@')
            return i + 1;
    return -1;
}

/* Whether text contains a URL with user information, wherever it is */
static bool containsCredentials(const QString &text)
{
    for (qsizetype schemeEnd = text.indexOf(QLatin1String("://")); schemeEnd > 0;
         schemeEnd = text.indexOf(QLatin1String("://"), schemeEnd + 3))
    {
        if (isSchemeChar(text.at(schemeEnd - 1)) && userInfoEnd(text, schemeEnd) >= 0)
            return true;
    }
    return false;
}

/* The MRL without the "user:password@" part of its authority, the rest kept
 * as is (signed URLs must stay valid), or a null string if it is not an
 * absolute URL */
static QString sanitizedMrl(const QString &mrl, bool *hadCredentials = nullptr)
{
    if (hadCredentials)
        *hadCredentials = false;
    if (mrl.isEmpty() || mrl.size() > MAX_MRL_LENGTH)
        return {};

    QString result = mrl;
    const qsizetype schemeEnd = mrl.indexOf(QLatin1String("://"));
    if (schemeEnd > 0
     && std::all_of(mrl.cbegin(), mrl.cbegin() + schemeEnd, isSchemeChar))
    {
        const qsizetype end = userInfoEnd(mrl, schemeEnd);
        if (end >= 0)
        {
            result.remove(schemeEnd + 3, end - (schemeEnd + 3));
            if (hadCredentials)
                *hadCredentials = true;
        }
    }

    const QUrl url(result, QUrl::TolerantMode);
    if (!url.isValid() || url.scheme().isEmpty())
        return {};
    return result;
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

    /* only copied under the playlist lock, which the player shares: the
     * checks and the JSON come after */
    struct Entry
    {
        QString uri;
        QString name;
        vlc_tick_t duration;
    };
    std::vector<Entry> entries;
    ssize_t playlistCurrent;
    {
        vlc_playlist_locker locker(playlist);

        playlistCurrent = vlc_playlist_GetCurrentIndex(playlist);
        const size_t count = vlc_playlist_Count(playlist);
        /* the MRLs refused below may leave room for more */
        entries.reserve(std::min<size_t>(count, 2 * MAX_ITEMS));
        for (size_t i = 0; i < count && entries.size() < 2 * MAX_ITEMS; ++i)
        {
            input_item_t *media =
                vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist, i));

            vlc_mutex_locker itemLocker(&media->lock);
            entries.push_back({ qfu(media->psz_uri), qfu(media->psz_name), media->i_duration });
        }
    }

    QJsonArray items;
    ssize_t current = -1;
    for (size_t i = 0; i < entries.size() && items.size() < MAX_ITEMS; ++i)
    {
        const Entry &entry = entries[i];

        bool hadCredentials;
        const QString mrl = sanitizedMrl(entry.uri, &hadCredentials);
        if (mrl.isNull())
            continue;

        QJsonObject item;
        item.insert("mrl", mrl);
        /* the name may be the MRL itself, credentials included, or any
         * URL with credentials: drop it then, the core names the restored
         * item after its MRL */
        if (!hadCredentials && !entry.name.isEmpty() && entry.name.size() <= MAX_NAME_LENGTH
         && !containsCredentials(entry.name))
            item.insert("name", entry.name);
        if (entry.duration > 0)
            item.insert("duration", static_cast<qint64>(MS_FROM_VLC_TICK(entry.duration)));

        if (static_cast<ssize_t>(i) == playlistCurrent)
            current = items.size();
        items.append(item);
    }

    QJsonObject root;
    root.insert("version", FORMAT_VERSION);
    root.insert("current", static_cast<qint64>(current));
    root.insert("items", items);

    /* Written to a temporary file renamed over the previous one, so a crash
     * never leaves a truncated Queue */
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    /* The MRLs may carry access tokens: only the user may read them. Set
     * before anything is written, whatever the permissions of the previous
     * file. On Windows, the file inherits the per-user ACL of its folder. */
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner))
    {
        file.cancelWriting();
        return;
    }
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
        if (containsCredentials(name))
            name.clear();

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
