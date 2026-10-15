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
#ifndef ADD_SUBTITLES_MODEL_HPP
#define ADD_SUBTITLES_MODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QUrl>
#include <QVector>

#include <memory>

class MediaLib;
class HttpWorker;
class QTemporaryDir;

/* remember the subtitle MRLs as Subtitle matches of the media (if ml isn't
 * null) and attach them to the player when the media is playing; the first
 * one turns on if no subtitle is showing */
void attachSubtitles(qt_intf_t *intf, MediaLib *ml, const QString &mediaMrl,
                     const QStringList &subtitleMrls);

/* Names from a server, a link or an archive decide where a download is
 * saved (From Link… and OpenSubtitles): these keep it a subtitle file inside
 * the chosen folder. */
namespace opensubtitles {

/* the extensions a downloaded subtitle may be saved with, lower case */
bool isDownloadExtension(const QString &suffix);

/* the extension to save a download named so with: its suffix in lower case
 * if it's a subtitle one, else "srt" ("evil.exe" gives "srt") */
QString downloadSuffix(const QString &fileName);

/* the last path component of a name, without the characters a file name
 * can't hold on Windows; empty if nothing is left ("../x.srt" gives
 * "x.srt", ".." gives "") */
QString safeFileName(const QString &name);

} // namespace opensubtitles

/**
 * Add Subtitles, To This File, From Computer: the subtitle files next to the
 * video and in its "Subs" folder, plus the files picked with Browse…, to
 * check. Save attaches the checked ones to the file as Subtitle matches, and
 * to the player when the file is playing.
 *
 * From Link… adds the subtitle file of a link, or the subtitle files of a
 * .zip link, to the same list; they are copied next to the video on Save.
 */
class AddSubtitlesModel : public QAbstractListModel
{
    Q_OBJECT

    // the MRL of the file the subtitles are added to
    Q_PROPERTY(QString media READ media CONSTANT FINAL)
    // the folder of the file, as " / " separated names
    Q_PROPERTY(QString folder READ folder CONSTANT FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedCountChanged FINAL)
    // a link is being downloaded
    Q_PROPERTY(bool fetching READ fetching NOTIFY fetchingChanged FINAL)
    // why the last link failed
    Q_PROPERTY(QString linkError READ linkError NOTIFY linkErrorChanged FINAL)
    /* From OpenSubtitles, To This File (an OpenSubtitlesModel); null when
     * not offered. To This Folder has its own, on the SubtitleMatchModel. */
    Q_PROPERTY(QObject* openSubtitles READ openSubtitles CONSTANT FINAL)

public:
    enum Roles {
        NameRole = Qt::UserRole,
        UrlRole,
        CheckedRole,
    };
    Q_ENUM(Roles)

    // ml may be null: the subtitles are then only attached to the player
    AddSubtitlesModel(qt_intf_t *intf, MediaLib *ml, const QString &mediaMrl,
                      QObject *parent = nullptr);
    ~AddSubtitlesModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString media() const { return m_media; }
    QString folder() const { return m_folder; }
    // the folder of the file on disk, empty if it isn't a local file
    QString folderPath() const { return m_folderPath; }
    int count() const { return m_rows.size(); }
    int checkedCount() const;
    bool fetching() const { return m_fetching > 0; }
    QString linkError() const { return m_linkError; }
    QObject *openSubtitles() const { return m_openSubtitles; }
    // the OpenSubtitles source, owned by the caller (usually a child)
    void setOpenSubtitles(QObject *model) { m_openSubtitles = model; }

    /* where links are copied when the folder of the video can't be
     * written to; VLC's cache by default */
    void setCacheFolder(const QString &path) { m_cacheFolder = path; }

    Q_INVOKABLE void toggle(int row);
    // Browse…: list the files and check them
    Q_INVOKABLE void addFiles(const QList<QUrl> &files);
    // pick files with the system picker, then addFiles()
    Q_INVOKABLE void browse();
    /* From Link…: download a .srt, .vtt, .ass or .zip link and list its
     * subtitle files, checked */
    Q_INVOKABLE void addLink(const QString &link);
    // attach the checked files and remember them
    Q_INVOKABLE void save();

signals:
    void countChanged();
    void checkedCountChanged();
    void fetchingChanged();
    void linkErrorChanged();

private:
    struct Row {
        QString name;
        QString mrl;
        bool checked = false;
        // downloaded from a link: copied next to the video on Save
        bool fetched = false;
    };

    void listFolder(const QString &path);
    void addRows(const QVector<Row> &rows);
    void setLinkError(const QString &error);
    QString copyFetched(const Row &row);

    qt_intf_t *m_intf;
    MediaLib *m_ml;
    QString m_media;
    QString m_folder;
    QString m_folderPath;
    QString m_cacheFolder;
    QVector<Row> m_rows;
    QObject *m_openSubtitles = nullptr;

    int m_fetching = 0;
    QString m_linkError;
    std::unique_ptr<QTemporaryDir> m_downloads;
    std::unique_ptr<HttpWorker> m_worker;
};

#endif // ADD_SUBTITLES_MODEL_HPP
