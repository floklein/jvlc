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
#ifndef VLC_QT_SAVED_STREAMS_MODEL_HPP
#define VLC_QT_SAVED_STREAMS_MODEL_HPP

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * The Saved streams and Network Playlists of the library's Network section,
 * newest first, kept in their own file.
 *
 * Open Network Stream (and the clipboard) save what they open here: one URL
 * becomes a Saved stream; several URLs, or a URL that is itself a playlist
 * (.m3u…), become a Network Playlist, which is re-read from its URLs each
 * time it is opened.
 */
class SavedStreamsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Roles {
        ID = Qt::UserRole + 1,
        NAME,
        URL,
        URLS,
        IS_PLAYLIST,
    };
    Q_ENUM(Roles)

    explicit SavedStreamsModel(const QString &filePath, QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    int count() const;

    /**
     * Save the URLs of some text, one per line (blank lines and lines that
     * are not links are ignored), under an optional name.
     *
     * @return the id of the Saved stream or Network Playlist, or an empty
     * string when the text holds no URL
     */
    Q_INVOKABLE QString save(const QString &text, const QString &name = {});
    QString save(const QStringList &urls, const QString &name = {});

    /* An empty name gives back the default one */
    Q_INVOKABLE void rename(const QString &id, const QString &name);
    Q_INVOKABLE void remove(const QString &id);

    Q_INVOKABLE int indexOf(const QString &id) const;
    /* the row as a map of role names to values, empty for an unknown id */
    Q_INVOKABLE QVariantMap get(const QString &id) const;

    /* The URLs of pasted or typed text: one per line */
    Q_INVOKABLE static QStringList urlsFromText(const QString &text);
    /* Whether a URL points to a list of streams rather than to a stream */
    static bool isPlaylistUrl(const QString &url);

signals:
    void countChanged();

private:
    struct Entry
    {
        QString id;
        QString name; /* empty: named after its URL */
        QStringList urls;
        bool playlist = false;
    };

    QString displayName(const Entry &entry) const;
    int rowOf(const QString &id) const;
    void load();
    void store() const;

    const QString m_filePath;
    QVector<Entry> m_entries;
};

#endif // VLC_QT_SAVED_STREAMS_MODEL_HPP
