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
#include "opensubtitles_models.hpp"
#include "add_subtitles_model.hpp"

#include <vlc_configuration.h>
#include <vlc_interface.h>
#include <vlc_url.h>

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QFileInfo>

using namespace opensubtitles;

namespace {

/* the local path of a file MRL, empty for other MRLs */
QString mrlToPath(const QString &mrl)
{
    char *path = vlc_uri2path(qtu(mrl));
    if (path == nullptr)
        return {};
    QString result = qfu(path);
    free(path);
    return result;
}

QString pathToMrl(const QString &path)
{
    char *uri = vlc_path2uri(qtu(QDir::toNativeSeparators(path)), "file");
    if (uri == nullptr)
        return {};
    QString mrl = qfu(uri);
    free(uri);
    return mrl;
}

QString resultName(const Result &result)
{
    return result.release.isEmpty() ? result.fileName : result.release;
}

QVariantMap resultMap(const Result &result)
{
    return {
        { QStringLiteral("name"), resultName(result) },
        { QStringLiteral("language"), result.language },
        { QStringLiteral("downloads"), result.downloads },
        { QStringLiteral("exactMatch"), result.exactMatch },
    };
}

} // anonymous namespace

/* OpenSubtitlesSource */

OpenSubtitlesSource::OpenSubtitlesSource(qt_intf_t *intf, MediaLib *ml, Client *client,
                                         QObject *parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
    , m_ml(ml)
    , m_client(client)
{
    m_language = preferredLanguage(VLC_OBJECT(intf));

    char *cache = config_GetUserDir(VLC_CACHE_DIR);
    if (cache != nullptr)
    {
        m_cacheFolder = QDir(qfu(cache)).filePath(QStringLiteral("subtitles"));
        free(cache);
    }

    if (m_client)
    {
        connect(m_client, &Client::quotaChanged, this, &OpenSubtitlesSource::quotaChanged);
        if (m_client->available() && !m_client->quota().signedIn)
            m_client->restoreSession();
    }
}

bool OpenSubtitlesSource::available() const
{
    return m_client && m_client->available();
}

void OpenSubtitlesSource::setLanguage(const QString &language)
{
    if (language == m_language)
        return;
    m_language = language;
    emit languageChanged();
}

QVariantList OpenSubtitlesSource::languages() const
{
    return opensubtitles::languages();
}

QString OpenSubtitlesSource::quotaText() const
{
    return m_client ? opensubtitles::quotaText(m_client->quota()) : QString();
}

QString OpenSubtitlesSource::quotaNote() const
{
    return m_client ? opensubtitles::quotaNote(m_client->quota()) : QString();
}

int OpenSubtitlesSource::remainingDownloads() const
{
    return m_client ? m_client->quota().remaining : 0;
}

void OpenSubtitlesSource::setBusy(bool busy)
{
    if (busy == m_busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void OpenSubtitlesSource::setSearched()
{
    if (m_searched)
        return;
    m_searched = true;
    emit searchedChanged();
}

void OpenSubtitlesSource::setError(const QString &error)
{
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

QString OpenSubtitlesSource::destinationFor(const QString &mediaMrl) const
{
    const QString path = mrlToPath(mediaMrl);
    QString base;
    QString folder;
    if (!path.isEmpty())
    {
        const QFileInfo file(path);
        base = file.completeBaseName();
        const QFileInfo dir(file.absolutePath());
        if (dir.isDir() && dir.isWritable())
            folder = file.absolutePath();
    }
    else
    {
        /* a stream's URL names the file in the cache folder */
        base = QFileInfo(safeFileName(QUrl(mediaMrl).path())).completeBaseName();
    }
    if (base.isEmpty())
        base = QStringLiteral("subtitle");
    if (folder.isEmpty())
        folder = m_cacheFolder;

    /* "<video>.<language>.srt", the extension replaced on download */
    return QDir(folder).filePath(base + QLatin1Char('.') + m_language + QStringLiteral(".srt"));
}

/* OpenSubtitlesModel */

OpenSubtitlesModel::OpenSubtitlesModel(qt_intf_t *intf, MediaLib *ml, Client *client,
                                       const QString &mediaMrl, QObject *parent)
    : OpenSubtitlesSource(intf, ml, client, parent)
    , m_media(mediaMrl)
{
    const QString path = mrlToPath(mediaMrl);
    QString name;
    if (!path.isEmpty())
    {
        name = QFileInfo(path).fileName();
        m_hash = movieHash(path);
    }
    else
    {
        name = QUrl(mediaMrl).fileName();
    }
    m_query = parseName(name).query();
}

int OpenSubtitlesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_results.size();
}

QVariant OpenSubtitlesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return {};
    const Result &result = m_results[index.row()];
    switch (role)
    {
    case Qt::DisplayRole:
    case NameRole:
        return resultName(result);
    case LanguageRole:
        return result.language;
    case DownloadsRole:
        return result.downloads;
    case ExactMatchRole:
        return result.exactMatch;
    case HearingImpairedRole:
        return result.hearingImpaired;
    case CheckedRole:
        return m_checked[index.row()];
    default:
        return {};
    }
}

QHash<int, QByteArray> OpenSubtitlesModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { LanguageRole, "language" },
        { DownloadsRole, "downloads" },
        { ExactMatchRole, "exactMatch" },
        { HearingImpairedRole, "hearingImpaired" },
        { CheckedRole, "checked" },
    };
}

void OpenSubtitlesModel::setQuery(const QString &query)
{
    if (query == m_query)
        return;
    m_query = query;
    emit queryChanged();
}

int OpenSubtitlesModel::checkedCount() const
{
    return std::count(m_checked.cbegin(), m_checked.cend(), true);
}

void OpenSubtitlesModel::toggle(int row)
{
    if (row < 0 || row >= m_checked.size())
        return;
    m_checked[row] = !m_checked[row];
    emit dataChanged(index(row), index(row), { CheckedRole });
    emit checkedCountChanged();
}

void OpenSubtitlesModel::search()
{
    if (!available() || m_busy)
        return;

    SearchQuery query;
    query.query = m_query.trimmed();
    query.language = m_language;
    /* the hash only fits the file's own name */
    query.movieHash = m_hash;
    if (query.query.isEmpty() && query.movieHash.isEmpty())
        return;

    setBusy(true);
    setError({});
    setSearched();
    QPointer<OpenSubtitlesModel> self(this);
    m_client->search(query, [self](const QVector<Result> &results, const QString &error) {
        if (!self)
            return;
        self->beginResetModel();
        self->m_results = results;
        self->m_checked = QVector<bool>(results.size(), false);
        self->endResetModel();
        emit self->countChanged();
        emit self->checkedCountChanged();
        self->setError(error);
        self->setBusy(false);
    });
}

void OpenSubtitlesModel::save()
{
    if (!available() || m_busy)
        return;
    QVector<int> rows;
    for (int i = 0; i < m_checked.size(); ++i)
        if (m_checked[i])
            rows.push_back(i);
    if (rows.isEmpty())
        return;

    setBusy(true);
    setError({});
    downloadNext(rows, {});
}

void OpenSubtitlesModel::downloadNext(QVector<int> rows, QStringList done)
{
    if (rows.isEmpty())
    {
        attachSubtitles(m_intf, m_ml, m_media, done);
        setBusy(false);
        emit saved();
        return;
    }

    const int row = rows.takeFirst();
    QPointer<OpenSubtitlesModel> self(this);
    m_client->download(m_results[row].fileId, destinationFor(m_media),
                       [self, rows, done](const QString &path, const QString &error) mutable {
        if (!self)
            return;
        if (!error.isEmpty())
        {
            /* keep what was downloaded */
            attachSubtitles(self->m_intf, self->m_ml, self->m_media, done);
            self->setError(error);
            self->setBusy(false);
            return;
        }
        done.push_back(pathToMrl(path));
        self->downloadNext(rows, done);
    });
}

/* OpenSubtitlesFolderModel */

OpenSubtitlesFolderModel::OpenSubtitlesFolderModel(qt_intf_t *intf, MediaLib *ml, Client *client,
                                                   const QString &folderPath, QObject *parent)
    : OpenSubtitlesSource(intf, ml, client, parent)
{
    static const QStringList videoExtensions = [] {
        QStringList list;
        for (const QString &pattern : QString(EXTENSIONS_VIDEO).split(';', Qt::SkipEmptyParts))
            list.push_back(pattern.mid(pattern.indexOf('.') + 1).toLower());
        return list;
    }();

    QStringList files;
    const QFileInfoList entries = QDir(folderPath).entryInfoList(QDir::Files | QDir::Readable);
    for (const QFileInfo &entry : entries)
        if (videoExtensions.contains(entry.suffix().toLower()))
            files.push_back(entry.absoluteFilePath());

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), [&collator](const QString &a, const QString &b) {
        return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
    });

    QStringList names, mrls;
    for (const QString &file : files)
    {
        names.push_back(QFileInfo(file).fileName());
        mrls.push_back(pathToMrl(file));
    }
    setEpisodes(QDir::fromNativeSeparators(folderPath).split('/', Qt::SkipEmptyParts)
                    .join(QStringLiteral(" / ")),
                names, mrls);
}

OpenSubtitlesFolderModel::OpenSubtitlesFolderModel(qt_intf_t *intf, MediaLib *ml, Client *client,
                                                   QObject *parent)
    : OpenSubtitlesSource(intf, ml, client, parent)
{
}

void OpenSubtitlesFolderModel::setEpisodes(const QString &label, const QStringList &names,
                                           const QStringList &mrls)
{
    /* drops a search under way */
    ++m_generation;
    beginResetModel();
    m_folder = label;
    m_rows.clear();
    for (qsizetype i = 0; i < mrls.size(); ++i)
    {
        Row row;
        row.name = names.value(i);
        row.mrl = mrls[i];
        row.path = mrlToPath(mrls[i]);
        if (row.name.isEmpty())
            row.name = QFileInfo(row.path.isEmpty() ? QUrl(row.mrl).path() : row.path).fileName();
        m_rows.push_back(row);
    }
    endResetModel();
    emit countChanged();
    emit pickedCountChanged();
    setBusy(false);
    m_searched = false;
    emit searchedChanged();
}

int OpenSubtitlesFolderModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant OpenSubtitlesFolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows[index.row()];
    const Result *picked = row.picked >= 0 ? &row.candidates[row.picked] : nullptr;
    switch (role)
    {
    case Qt::DisplayRole:
    case EpisodeRole:
        return row.name;
    case StateRole:
        return row.state;
    case PickNameRole:
        return picked ? resultName(*picked) : QString();
    case PickLanguageRole:
        return picked ? picked->language : QString();
    case ExactMatchRole:
        return picked && picked->exactMatch;
    case CandidateCountRole:
        return row.candidates.size();
    default:
        return {};
    }
}

QHash<int, QByteArray> OpenSubtitlesFolderModel::roleNames() const
{
    return {
        { EpisodeRole, "episode" },
        { StateRole, "state" },
        { PickNameRole, "pickName" },
        { PickLanguageRole, "pickLanguage" },
        { ExactMatchRole, "exactMatch" },
        { CandidateCountRole, "candidateCount" },
    };
}

int OpenSubtitlesFolderModel::pickedCount() const
{
    return std::count_if(m_rows.cbegin(), m_rows.cend(),
                         [](const Row &row) { return row.state == Picked; });
}

QVariantList OpenSubtitlesFolderModel::candidates(int row) const
{
    QVariantList list;
    if (row < 0 || row >= m_rows.size())
        return list;
    for (const Result &result : m_rows[row].candidates)
        list.push_back(resultMap(result));
    return list;
}

void OpenSubtitlesFolderModel::pick(int row, int candidate)
{
    if (row < 0 || row >= m_rows.size() || m_busy)
        return;
    Row &r = m_rows[row];
    if (candidate >= r.candidates.size() || r.state == Saved)
        return;
    if (candidate < 0)
    {
        r.picked = -1;
        r.state = None;
    }
    else
    {
        r.picked = candidate;
        r.state = Picked;
    }
    rowChanged(row);
    emit pickedCountChanged();
}

void OpenSubtitlesFolderModel::rowChanged(int row)
{
    emit dataChanged(index(row), index(row));
}

void OpenSubtitlesFolderModel::search()
{
    if (!available() || m_rows.isEmpty())
        return;

    /* a new search drops the previous one */
    ++m_generation;
    beginResetModel();
    for (Row &row : m_rows)
    {
        row.state = Searching;
        row.candidates.clear();
        row.picked = -1;
    }
    endResetModel();
    emit pickedCountChanged();

    setBusy(true);
    setError({});
    setSearched();
    searchNext(0, m_generation);
}

void OpenSubtitlesFolderModel::searchNext(int row, unsigned generation)
{
    if (generation != m_generation)
        return;
    if (row >= m_rows.size())
    {
        setBusy(false);
        return;
    }

    const ParsedName parsed = parseName(m_rows[row].name);
    SearchQuery query;
    query.query = parsed.title;
    query.season = parsed.season;
    query.episode = parsed.episode;
    query.language = m_language;
    query.movieHash = movieHash(m_rows[row].path);

    QPointer<OpenSubtitlesFolderModel> self(this);
    m_client->search(query, [self, row, generation, parsed](const QVector<Result> &results,
                                                            const QString &error) {
        if (!self || generation != self->m_generation)
            return;
        Row &r = self->m_rows[row];
        r.candidates = results;
        /* the best one first, the others as they came */
        const int best = bestResult(results, parsed.season, parsed.episode);
        if (best > 0)
            std::rotate(r.candidates.begin(), r.candidates.begin() + best,
                        r.candidates.begin() + best + 1);

        if (!error.isEmpty())
        {
            self->setError(error);
            r.state = NoMatch;
        }
        else if (best < 0)
            r.state = NoMatch;
        else if (self->remainingDownloads() >= 0
              && self->pickedCount() >= self->remainingDownloads())
            /* nothing downloads past the quota */
            r.state = NotPicked;
        else
        {
            r.state = Picked;
            r.picked = 0;
        }
        self->rowChanged(row);
        emit self->pickedCountChanged();
        self->searchNext(row + 1, generation);
    });
}

void OpenSubtitlesFolderModel::save()
{
    if (!available() || m_busy || pickedCount() == 0)
        return;
    setBusy(true);
    setError({});
    downloadNext(0, 0);
}

void OpenSubtitlesFolderModel::downloadNext(int row, int saved)
{
    while (row < m_rows.size() && m_rows[row].state != Picked)
        ++row;
    if (row >= m_rows.size())
    {
        setBusy(false);
        emit this->saved();
        return;
    }

    const Row &r = m_rows[row];
    QPointer<OpenSubtitlesFolderModel> self(this);
    m_client->download(r.candidates[r.picked].fileId, destinationFor(r.mrl),
                       [self, row, saved](const QString &path, const QString &error) {
        if (!self)
            return;
        if (!error.isEmpty())
        {
            self->setError(saved > 0 ? qtr("%1 subtitles saved. %2").arg(saved).arg(error) : error);
            self->setBusy(false);
            return;
        }
        Row &r = self->m_rows[row];
        attachSubtitles(self->m_intf, self->m_ml, r.mrl, { pathToMrl(path) });
        /* done with this one */
        r.state = Saved;
        self->rowChanged(row);
        emit self->pickedCountChanged();
        self->downloadNext(row + 1, saved + 1);
    });
}
