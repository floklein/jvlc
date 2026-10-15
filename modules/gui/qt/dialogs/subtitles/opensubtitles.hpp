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
#ifndef OPENSUBTITLES_HPP
#define OPENSUBTITLES_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVector>

#include <functional>
#include <memory>

class QSettings;

namespace opensubtitles {

/* the parts of a video file name a search needs: "The.Wand.S01E01.720p.mkv"
 * gives the title "The Wand", season 1 and episode 1 */
struct ParsedName
{
    QString title;
    int season = -1;
    int episode = -1;

    // "The Wand S01E01", the search pre-filled in the dialog
    QString query() const;
};

ParsedName parseName(const QString &fileName);

/* the OpenSubtitles hash of a local file (16 hexadecimal digits), empty if
 * it can't be read */
QString movieHash(const QString &path);

/* the preferred subtitle language as an OpenSubtitles code ("en", "pt-br"),
 * from the "sub-language" option, else the system language */
QString preferredLanguage(vlc_object_t *obj);

/* the languages offered by the language picker: { code, name } maps */
QVariantList languages();

struct Result
{
    QString fileId;
    QString fileName;
    // the release name, as uploaded
    QString release;
    QString language;
    int downloads = 0;
    // the file has the same hash as the video
    bool exactMatch = false;
    bool hearingImpaired = false;
    int season = -1;
    int episode = -1;
};

struct SearchQuery
{
    QString query;
    QString language;
    QString movieHash;
    int season = -1;
    int episode = -1;
};

/* the downloads left today; -1 when unknown */
struct Quota
{
    int remaining = -1;
    int allowed = -1;
    bool signedIn = false;
    bool vip = false;
    QString user;
};

/* "3 of 5 downloads left today" */
QString quotaText(const Quota &quota);
/* how to get more downloads, or fewer ads */
QString quotaNote(const Quota &quota);

/* the best result for an episode, -1 if none fits: an exact match first,
 * then the right season and episode, then the most downloaded */
int bestResult(const QVector<Result> &results, int season, int episode);

/**
 * The OpenSubtitles REST API (https://opensubtitles.stoplight.io): search,
 * download within the daily quota, optional sign-in.
 *
 * The consumer API key is bundled at build time (meson option
 * opensubtitles_api_key); without it, the client isn't available. The
 * sign-in credentials live in VLC's keystore. Requests run one by one on a
 * worker thread; the callbacks run on the thread of the client.
 */
class Client : public QObject
{
    Q_OBJECT

public:
    using SearchCallback = std::function<void(const QVector<Result> &results, const QString &error)>;
    // path is the downloaded file, empty on error
    using DownloadCallback = std::function<void(const QString &path, const QString &error)>;
    using DoneCallback = std::function<void(const QString &error)>;

    /* obj gives the sockets, TLS and keystore modules. An empty apiKey
     * makes the client unavailable. */
    Client(vlc_object_t *obj, const QString &apiUrl, const QByteArray &apiKey,
           QObject *parent = nullptr);
    // with the key and endpoint bundled at build time
    explicit Client(vlc_object_t *obj, QObject *parent = nullptr);
    ~Client() override;

    static QByteArray bundledKey();
    static QString bundledUrl();

    bool available() const { return !m_apiKey.isEmpty(); }

    // where the last known anonymous quota is kept between runs
    void setQuotaStore(QSettings *settings);

    Quota quota() const { return m_quota; }

    void search(const SearchQuery &query, SearchCallback callback);
    /* download a result into destination (a file path; the extension of
     * the subtitle replaces its own); counts against the quota */
    void download(const QString &fileId, const QString &destination, DownloadCallback callback);

    /* sign in with the credentials in the keystore, if any, and read the
     * quota; done with an empty error when signed out too */
    void restoreSession(DoneCallback callback = {});
    // check the credentials, then store them in the keystore and sign in
    void signIn(const QString &user, const QString &password, DoneCallback callback = {});
    // forget the stored credentials
    void signOut(DoneCallback callback = {});
    // the user stored in the keystore, empty if none (read at start)
    QString storedUser() const { return m_storedUser; }

    // drop the pending requests and interrupt the running one
    void cancel();

signals:
    void quotaChanged();
    void storedUserChanged();

private:
    struct Worker;

    void post(std::function<void()> task);
    void setQuota(const Quota &quota);
    void loadStoredQuota();
    void loadStoredUser();
    void setStoredUser(const QString &user);

    QString m_apiUrl;
    QByteArray m_apiKey;
    Quota m_quota;
    QSettings *m_quotaStore = nullptr;
    QString m_storedUser;
    std::shared_ptr<Worker> m_worker;
};

} // namespace opensubtitles

#endif // OPENSUBTITLES_HPP
