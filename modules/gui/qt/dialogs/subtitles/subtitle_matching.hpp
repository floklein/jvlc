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
#ifndef SUBTITLE_MATCHING_HPP
#define SUBTITLE_MATCHING_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <optional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class MediaLib;
class QSettings;

/* Matching subtitle files to episodes: by episode number (S01E02, 1x02,
 * E02), then by name similarity, within a language. */
namespace subtitlematching {

// the subtitle file extensions, without ".txt", too common for other files
const QStringList& subtitleExtensions();
bool isVideoFile(const QString &fileName);
// the MRL VLC plays for a local path
QString pathToMrl(const QString &path);

// the language of a subtitle file from its name ("Show.S01E02.fr.srt",
// "Show S01E02 [English].srt") or its folder ("English/Show S01E02.srt"):
// an ISO 639-1 code, else ISO 639-2; empty if none is found
QString language(const QString &path);
// the name of a language code, in the interface language
QString languageName(const QString &code);
// the code of the preferred subtitle language of the Profile, if any
QString preferredLanguage(vlc_object_t *obj);

// a subtitle file of a source
struct Candidate
{
    QString name;
    // the path in the source, '/' separated
    QString path;
    QString language;
};

// a folder or a .zip file of subtitle files
class Source
{
public:
    Source() = default;
    explicit Source(const QString &path);

    bool isValid() const { return !m_path.isEmpty(); }
    bool isZip() const;
    QString path() const { return m_path; }

    // the subtitle files, in natural order (subfolders of a folder included)
    QVector<Candidate> list() const;
    // the MRL to play a candidate; a file of a .zip is extracted first,
    // into a folder named after the .zip
    QString resolve(const Candidate &candidate) const;

private:
    QString m_path;
};

struct Filter
{
    // the language to match; empty matches any
    QString language;
    // the best file of every language
    bool all = false;
};

// all the candidates, best first for this episode file name
QVector<int> rank(const QString &episode, const QVector<Candidate> &candidates,
                  const Filter &filter);
// the candidates matched to this episode: none, one, or one per language
// with Filter::all
QVector<int> match(const QString &episode, const QVector<Candidate> &candidates,
                   const Filter &filter);

// attach subtitle files to the player if this media is playing; the first
// one turns on if no subtitle is showing
void attachToPlayer(qt_intf_t *intf, const QString &mediaMrl, const QStringList &subtitleMrls);

} // namespace subtitlematching

/**
 * The subtitle source each Show remembers (shared by every Profile), with its
 * language and the episodes already matched (or set to None).
 */
class ShowSubtitleSources
{
public:
    struct Show
    {
        // the folder of the Show and the source, '/' separated paths
        QString folder;
        QString source;
        QString language;
        bool all = false;
        // the episodes already handled, by path in the Show
        QStringList episodes;
    };

    explicit ShowSubtitleSources(QSettings *settings);

    // add or replace the Show of this folder
    void remember(const Show &show);
    // the Show holding this file, the innermost one
    std::optional<Show> showOf(const QString &filePath) const;

private:
    QVector<Show> load() const;
    void store(const QVector<Show> &shows);

    QSettings *m_settings;
};

/**
 * Matches an episode silently when it plays, from the source its Show
 * remembers, if it was added to the Show after the Show was matched.
 */
class ShowSubtitleMatcher : public QObject
{
    Q_OBJECT

public:
    ShowSubtitleMatcher(qt_intf_t *intf, MediaLib *ml, QSettings *settings,
                        QObject *parent = nullptr);

    // the media of this MRL starts playing
    void matchOnPlay(const QString &mediaMrl);

signals:
    // the episode got these Subtitle matches
    void matched(const QString &mediaMrl, const QStringList &subtitleMrls);

private:
    qt_intf_t *m_intf;
    MediaLib *m_ml;
    QSettings *m_settings;
};

#endif // SUBTITLE_MATCHING_HPP
