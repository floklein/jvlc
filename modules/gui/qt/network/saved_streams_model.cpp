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

#include "saved_streams_model.hpp"

#include "qt.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>

namespace {

const QString KEY_ID = QStringLiteral("id");
const QString KEY_NAME = QStringLiteral("name");
const QString KEY_URLS = QStringLiteral("urls");
const QString KEY_PLAYLIST = QStringLiteral("playlist");
const QString KEY_STREAMS = QStringLiteral("streams");

/* the file name of a playlist URL, decoded */
QString fileNameOf(const QString &url)
{
    return QUrl(url).fileName(QUrl::FullyDecoded);
}

} // namespace

SavedStreamsModel::SavedStreamsModel(const QString &filePath, QObject *parent)
    : QAbstractListModel(parent)
    , m_filePath(filePath)
{
    load();
}

QHash<int, QByteArray> SavedStreamsModel::roleNames() const
{
    return {
        { ID, "id" },
        { NAME, "name" },
        { URL, "url" },
        { URLS, "urls" },
        { IS_PLAYLIST, "isPlaylist" },
    };
}

int SavedStreamsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant SavedStreamsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role)
    {
    case ID:
        return entry.id;
    case Qt::DisplayRole:
    case NAME:
        return displayName(entry);
    case URL:
        return entry.urls.value(0);
    case URLS:
        return entry.urls;
    case IS_PLAYLIST:
        return entry.playlist;
    default:
        return {};
    }
}

int SavedStreamsModel::count() const
{
    return m_entries.size();
}

QString SavedStreamsModel::save(const QString &text, const QString &name)
{
    return save(urlsFromText(text), name);
}

QString SavedStreamsModel::save(const QStringList &urls, const QString &name)
{
    if (urls.isEmpty())
        return {};

    const QString trimmedName = name.trimmed();
    const bool playlist = urls.size() > 1 || isPlaylistUrl(urls.first());

    /* a Saved stream is kept once, the last saved on top */
    if (!playlist)
    {
        const auto it = std::find_if(m_entries.cbegin(), m_entries.cend(), [&urls](const Entry &entry) {
            return !entry.playlist && entry.urls == urls;
        });
        if (it != m_entries.cend())
        {
            const int row = it - m_entries.cbegin();
            if (row != 0)
            {
                beginMoveRows({}, row, row, {}, 0);
                m_entries.move(row, 0);
                endMoveRows();
            }
            if (!trimmedName.isEmpty() && m_entries[0].name != trimmedName)
            {
                m_entries[0].name = trimmedName;
                emit dataChanged(index(0), index(0), { NAME, Qt::DisplayRole });
            }
            store();
            return m_entries[0].id;
        }
    }

    Entry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.name = trimmedName;
    entry.urls = urls;
    entry.playlist = playlist;

    beginInsertRows({}, 0, 0);
    m_entries.prepend(entry);
    endInsertRows();
    emit countChanged();

    store();
    return entry.id;
}

void SavedStreamsModel::rename(const QString &id, const QString &name)
{
    const int row = rowOf(id);
    if (row < 0)
        return;

    const QString trimmedName = name.trimmed();
    if (m_entries[row].name == trimmedName)
        return;

    m_entries[row].name = trimmedName;
    emit dataChanged(index(row), index(row), { NAME, Qt::DisplayRole });
    store();
}

void SavedStreamsModel::remove(const QString &id)
{
    const int row = rowOf(id);
    if (row < 0)
        return;

    beginRemoveRows({}, row, row);
    m_entries.removeAt(row);
    endRemoveRows();
    emit countChanged();

    store();
}

int SavedStreamsModel::indexOf(const QString &id) const
{
    return rowOf(id);
}

QVariantMap SavedStreamsModel::get(const QString &id) const
{
    const int row = rowOf(id);
    if (row < 0)
        return {};

    QVariantMap result;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        result.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return result;
}

QStringList SavedStreamsModel::urlsFromText(const QString &text)
{
    /* "scheme://" then anything but spaces */
    static const QRegularExpression link(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*://\\S+$"));

    QStringList urls;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
    for (QString line : lines)
    {
        line = line.trimmed();
        /* copied links are often quoted or bracketed */
        if (line.size() >= 2
            && ((line.startsWith(u'"') && line.endsWith(u'"'))
                || (line.startsWith(u'\'') && line.endsWith(u'\''))
                || (line.startsWith(u'<') && line.endsWith(u'>'))))
            line = line.mid(1, line.size() - 2).trimmed();

        if (link.match(line).hasMatch())
            urls.push_back(line);
    }
    return urls;
}

bool SavedStreamsModel::isPlaylistUrl(const QString &url)
{
    /* .m3u8 is left out: it is how HTTP Live Streaming streams end */
    const QString suffix = QFileInfo(QUrl(url).path()).suffix().toLower();
    return suffix == QLatin1String("m3u")
        || suffix == QLatin1String("pls")
        || suffix == QLatin1String("xspf");
}

QString SavedStreamsModel::displayName(const Entry &entry) const
{
    if (!entry.name.isEmpty())
        return entry.name;

    if (entry.playlist)
    {
        /* named after the list it is read from */
        if (entry.urls.size() == 1)
        {
            const QString fileName = fileNameOf(entry.urls.first());
            if (!fileName.isEmpty())
                return fileName;
        }
        return qtr("Network Playlist");
    }

    return entry.urls.value(0);
}

int SavedStreamsModel::rowOf(const QString &id) const
{
    if (id.isEmpty())
        return -1;

    for (int i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries.at(i).id == id)
            return i;
    }
    return -1;
}

void SavedStreamsModel::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray streams = QJsonDocument::fromJson(file.readAll()).object().value(KEY_STREAMS).toArray();
    for (const QJsonValue &value : streams)
    {
        const QJsonObject object = value.toObject();

        Entry entry;
        entry.id = object.value(KEY_ID).toString();
        entry.name = object.value(KEY_NAME).toString();
        for (const QJsonValue &url : object.value(KEY_URLS).toArray())
        {
            if (!url.toString().isEmpty())
                entry.urls.push_back(url.toString());
        }
        entry.playlist = object.value(KEY_PLAYLIST).toBool();

        if (entry.id.isEmpty() || entry.urls.isEmpty())
            continue;
        m_entries.push_back(entry);
    }
}

void SavedStreamsModel::store() const
{
    QJsonArray streams;
    for (const Entry &entry : m_entries)
    {
        QJsonObject object;
        object.insert(KEY_ID, entry.id);
        if (!entry.name.isEmpty())
            object.insert(KEY_NAME, entry.name);
        object.insert(KEY_URLS, QJsonArray::fromStringList(entry.urls));
        if (entry.playlist)
            object.insert(KEY_PLAYLIST, true);
        streams.push_back(object);
    }

    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    /* written to a temporary file renamed over the previous one, so a crash
     * never leaves half a file */
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(QJsonObject { { KEY_STREAMS, streams } }).toJson());
    file.commit();
}
