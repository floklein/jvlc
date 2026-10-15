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
#ifndef OPENSUBTITLES_MODELS_HPP
#define OPENSUBTITLES_MODELS_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "opensubtitles.hpp"

#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVector>

class MediaLib;

/**
 * What both OpenSubtitles sources of Add Subtitles share: the language, the
 * quota, the busy state and the errors.
 */
class OpenSubtitlesSource : public QAbstractListModel
{
    Q_OBJECT

    // false without a bundled API key: the dialog explains it
    Q_PROPERTY(bool available READ available CONSTANT FINAL)
    // an OpenSubtitles language code, "en", "pt-br"…
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged FINAL)
    // { code, name } maps
    Q_PROPERTY(QVariantList languages READ languages CONSTANT FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged FINAL)
    // "3 of 5 downloads left today"
    Q_PROPERTY(QString quotaText READ quotaText NOTIFY quotaChanged FINAL)
    // signing in or VIP
    Q_PROPERTY(QString quotaNote READ quotaNote NOTIFY quotaChanged FINAL)
    Q_PROPERTY(int remainingDownloads READ remainingDownloads NOTIFY quotaChanged FINAL)
    // a search has been started: an empty list then means no result
    Q_PROPERTY(bool searched READ searched NOTIFY searchedChanged FINAL)

public:
    OpenSubtitlesSource(qt_intf_t *intf, MediaLib *ml, opensubtitles::Client *client,
                        QObject *parent = nullptr);

    bool available() const;
    QString language() const { return m_language; }
    void setLanguage(const QString &language);
    QVariantList languages() const;
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QString quotaText() const;
    QString quotaNote() const;
    int remainingDownloads() const;
    bool searched() const { return m_searched; }

    /* where downloads go when the folder of the video can't be written
     * to; VLC's cache by default */
    void setCacheFolder(const QString &path) { m_cacheFolder = path; }

    Q_INVOKABLE virtual void search() = 0;
    // download the chosen subtitles, attach and remember them
    Q_INVOKABLE virtual void save() = 0;

signals:
    void languageChanged();
    void busyChanged();
    void errorChanged();
    void quotaChanged();
    void searchedChanged();
    // Save is done: the dialog closes
    void saved();

protected:
    void setBusy(bool busy);
    void setError(const QString &error);
    void setSearched();
    /* the file a subtitle of this video downloads to, its extension
     * replaced by the one of the subtitle */
    QString destinationFor(const QString &mediaMrl) const;

    qt_intf_t *m_intf;
    MediaLib *m_ml;
    QPointer<opensubtitles::Client> m_client;
    QString m_language;
    bool m_busy = false;
    bool m_searched = false;
    QString m_error;
    QString m_cacheFolder;
};

/**
 * Add Subtitles, To This File, From OpenSubtitles: the search pre-filled
 * with the parsed name, exact (hash) matches first. Save downloads the
 * checked results next to the video, or in the cache.
 */
class OpenSubtitlesModel : public OpenSubtitlesSource
{
    Q_OBJECT

    Q_PROPERTY(QString media READ media CONSTANT FINAL)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedCountChanged FINAL)

public:
    enum Roles {
        NameRole = Qt::UserRole,
        LanguageRole,
        DownloadsRole,
        ExactMatchRole,
        HearingImpairedRole,
        CheckedRole,
    };
    Q_ENUM(Roles)

    OpenSubtitlesModel(qt_intf_t *intf, MediaLib *ml, opensubtitles::Client *client,
                       const QString &mediaMrl, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString media() const { return m_media; }
    QString query() const { return m_query; }
    void setQuery(const QString &query);
    int count() const { return m_results.size(); }
    int checkedCount() const;

    Q_INVOKABLE void toggle(int row);
    void search() override;
    void save() override;

signals:
    void queryChanged();
    void countChanged();
    void checkedCountChanged();

private:
    void downloadNext(QVector<int> rows, QStringList done);

    QString m_media;
    QString m_hash;
    QString m_query;
    QVector<opensubtitles::Result> m_results;
    QVector<bool> m_checked;
};

/**
 * Add Subtitles, To This Folder, From OpenSubtitles: the videos of the
 * folder, each with the best result pre-picked while downloads are left.
 * Nothing downloads until Save.
 */
class OpenSubtitlesFolderModel : public OpenSubtitlesSource
{
    Q_OBJECT

    // the folder, as " / " separated names, or the Playlist
    Q_PROPERTY(QString folder READ folder NOTIFY countChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(int pickedCount READ pickedCount NOTIFY pickedCountChanged FINAL)

public:
    enum State {
        Searching,
        Picked,
        // the search found nothing that fits
        NoMatch,
        // the downloads left ran out before this episode
        NotPicked,
        // set to None by the user
        None,
        // downloaded and attached by Save
        Saved,
    };
    Q_ENUM(State)

    enum Roles {
        EpisodeRole = Qt::UserRole,
        StateRole,
        PickNameRole,
        PickLanguageRole,
        ExactMatchRole,
        CandidateCountRole,
    };
    Q_ENUM(Roles)

    // folderPath is a local folder; its videos are the episodes
    OpenSubtitlesFolderModel(qt_intf_t *intf, MediaLib *ml, opensubtitles::Client *client,
                             const QString &folderPath, QObject *parent = nullptr);
    // the episodes come with setEpisodes()
    OpenSubtitlesFolderModel(qt_intf_t *intf, MediaLib *ml, opensubtitles::Client *client,
                             QObject *parent = nullptr);

    /* the episodes, such as those of a Show or a Playlist found by the
     * matching of local files: their names as shown and their MRLs; the
     * label names the folder or Playlist */
    void setEpisodes(const QString &label, const QStringList &names, const QStringList &mrls);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString folder() const { return m_folder; }
    int count() const { return m_rows.size(); }
    int pickedCount() const;

    // the results of an episode, best first: { name, language, downloads, exactMatch } maps
    Q_INVOKABLE QVariantList candidates(int row) const;
    // pick a candidate of the episode, -1 for None
    Q_INVOKABLE void pick(int row, int candidate);

    void search() override;
    void save() override;

signals:
    void countChanged();
    void pickedCountChanged();

private:
    struct Row {
        QString name;
        QString mrl;
        QString path;
        State state = Searching;
        QVector<opensubtitles::Result> candidates;
        int picked = -1;
    };

    void searchNext(int row, unsigned generation);
    void downloadNext(int row, int saved);
    void rowChanged(int row);

    QString m_folder;
    QVector<Row> m_rows;
    unsigned m_generation = 0;
};

#endif // OPENSUBTITLES_MODELS_HPP
