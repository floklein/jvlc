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
#ifndef SUBTITLE_MATCH_MODEL_HPP
#define SUBTITLE_MATCH_MODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "medialibrary/mlqmltypes.hpp"
#include "subtitle_matching.hpp"

#include <QAbstractListModel>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVector>

class MediaLib;

/**
 * Add Subtitles, To This Folder or To This Playlist, From Computer: every
 * episode of the Show (the video files of the folder and its subfolders) or
 * of the Playlist, with the subtitle file matched to it from the source (the
 * Show's folder by default, another folder or a .zip).
 *
 * Save stores the matches as Subtitle matches; for a folder, the Show
 * remembers its source so that the episodes added later are matched when
 * they play (ShowSubtitleMatcher).
 */
class SubtitleMatchModel : public QAbstractListModel
{
    Q_OBJECT

    // the Show (folder name) or the Playlist name
    Q_PROPERTY(QString target READ target NOTIFY targetChanged FINAL)
    Q_PROPERTY(bool playlist READ isPlaylist NOTIFY targetChanged FINAL)
    // the source, as " / " separated names
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged FINAL)
    Q_PROPERTY(bool sourceIsZip READ sourceIsZip NOTIFY sourceChanged FINAL)
    // the languages of the source by name, then "All"; empty with less than
    // two languages
    Q_PROPERTY(QStringList languages READ languages NOTIFY sourceChanged FINAL)
    Q_PROPERTY(int languageIndex READ languageIndex WRITE setLanguageIndex NOTIFY languageIndexChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(int matchedCount READ matchedCount NOTIFY matchedCountChanged FINAL)

public:
    enum Roles {
        NameRole = Qt::UserRole,
        // the path in the Show (or the folder, for a Playlist)
        PathRole,
        UrlRole,
        // the matched subtitle file names, ", " separated, empty for none
        MatchRole,
        MatchedRole,
        // the match was picked with ⇆ (or set to None)
        OverriddenRole,
    };
    Q_ENUM(Roles)

    // ml may be null: nothing is saved then
    SubtitleMatchModel(qt_intf_t *intf, MediaLib *ml, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString target() const { return m_target; }
    bool isPlaylist() const { return m_playlist; }
    QString source() const;
    bool sourceIsZip() const { return m_source.isZip(); }
    QStringList languages() const;
    int languageIndex() const { return m_languageIndex; }
    void setLanguageIndex(int index);
    int count() const { return m_rows.size(); }
    int matchedCount() const;

    // To This Folder: the episodes of the folder (an MRL or a path), matched
    // from the source the Show remembers, or from the folder itself
    Q_INVOKABLE void setFolder(const QString &folder);
    // To This Playlist: its media, matched from the folder of the first one
    void setPlaylist(const QString &name, const QStringList &mediaMrls);
    // the same, listing the media of the Playlist from the media library
    Q_INVOKABLE void loadPlaylist(const MLItemId &playlistId, const QString &name);
    // the source: a folder or a .zip file
    Q_INVOKABLE void setSource(const QUrl &source);
    // back to the Show's folder (the folder of the first media of a Playlist)
    Q_INVOKABLE void resetSource();
    // pick another folder, or a .zip, with the system picker
    Q_INVOKABLE void browseFolder();
    Q_INVOKABLE void browseZip();

    // ⇆: the subtitle files for this episode, best first, as
    // { id, name, path, language, current }
    Q_INVOKABLE QVariantList candidates(int row) const;
    // set the match of an episode to the candidate of this id, -1 for None
    Q_INVOKABLE void setMatch(int row, int candidateId);

    // save the matches, attach them to the playing episode
    Q_INVOKABLE void save();

signals:
    void targetChanged();
    void sourceChanged();
    void languageIndexChanged();
    void countChanged();
    void matchedCountChanged();

private:
    struct Row {
        QString name;
        QString path;
        QString mrl;
        QVector<int> matches;
        bool overridden = false;
    };

    void setEpisodes(const QStringList &mrls, const QString &root);
    void loadSource(const subtitlematching::Source &source, const QString &language, bool all);
    void rematch();
    subtitlematching::Filter filter() const;

    qt_intf_t *m_intf;
    MediaLib *m_ml;
    QString m_target;
    bool m_playlist = false;
    // the folder of the Show, empty for a Playlist
    QString m_show;
    // the source by default
    QString m_defaultSource;
    subtitlematching::Source m_source;
    QVector<subtitlematching::Candidate> m_candidates;
    // language codes, by name
    QStringList m_languageCodes;
    int m_languageIndex = -1;
    QVector<Row> m_rows;
};

#endif // SUBTITLE_MATCH_MODEL_HPP
