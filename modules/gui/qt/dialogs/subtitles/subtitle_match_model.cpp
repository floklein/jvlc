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
#include "subtitle_match_model.hpp"

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlhelper.hpp"

#include <vlc_url.h>

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>

using namespace subtitlematching;

namespace {

QString localPath(const QString& mrlOrPath)
{
    if (!mrlOrPath.contains(QLatin1String("://")))
        return QDir::fromNativeSeparators(QDir::cleanPath(mrlOrPath));
    char* path = vlc_uri2path(qtu(mrlOrPath));
    if (path == nullptr)
        return {};
    QString result = QDir::fromNativeSeparators(QDir::cleanPath(qfu(path)));
    free(path);
    return result;
}

QString displayPath(const QString& path)
{
    return QDir::fromNativeSeparators(path).split('/', Qt::SkipEmptyParts)
               .join(QStringLiteral(" / "));
}

} // anonymous namespace

SubtitleMatchModel::SubtitleMatchModel(qt_intf_t* intf, MediaLib* ml, QObject* parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
    , m_ml(ml)
{
}

int SubtitleMatchModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant SubtitleMatchModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows[index.row()];
    switch (role)
    {
    case Qt::DisplayRole:
    case NameRole:
        return row.name;
    case PathRole:
        return row.path;
    case UrlRole:
        return row.mrl;
    case MatchRole:
    {
        QStringList names;
        for (int match : row.matches)
            names.push_back(m_candidates[match].name);
        return names.join(QStringLiteral(", "));
    }
    case MatchedRole:
        return !row.matches.isEmpty();
    case OverriddenRole:
        return row.overridden;
    default:
        return {};
    }
}

QHash<int, QByteArray> SubtitleMatchModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { PathRole, "path" },
        { UrlRole, "url" },
        { MatchRole, "match" },
        { MatchedRole, "matched" },
        { OverriddenRole, "overridden" },
    };
}

QString SubtitleMatchModel::source() const
{
    return displayPath(m_source.path());
}

QStringList SubtitleMatchModel::languages() const
{
    if (m_languageCodes.size() < 2)
        return {};
    QStringList names;
    for (const QString& code : m_languageCodes)
        names.push_back(languageName(code));
    //: the language filter entry that attaches every language
    names.push_back(qtr("All"));
    return names;
}

void SubtitleMatchModel::setLanguageIndex(int index)
{
    if (index == m_languageIndex || index < 0 || index > m_languageCodes.size()
     || m_languageCodes.size() < 2)
        return;
    m_languageIndex = index;
    emit languageIndexChanged();
    rematch();
}

int SubtitleMatchModel::matchedCount() const
{
    return std::count_if(m_rows.cbegin(), m_rows.cend(),
                         [](const Row& row) { return !row.matches.isEmpty(); });
}

Filter SubtitleMatchModel::filter() const
{
    if (m_languageCodes.size() < 2 || m_languageIndex < 0)
        return {};
    if (m_languageIndex >= m_languageCodes.size())
        return { {}, true };
    return { m_languageCodes[m_languageIndex], false };
}

void SubtitleMatchModel::setEpisodes(const QStringList& mrls, const QString& root)
{
    QVector<Row> rows;
    for (const QString& mrl : mrls)
    {
        const QString path = localPath(mrl);
        Row row;
        row.mrl = mrl;
        row.name = path.isEmpty() ? QUrl(mrl).fileName() : path.section('/', -1);
        row.path = (!root.isEmpty() && path.startsWith(root + '/')) ? path.mid(root.size() + 1)
                                                                    : row.name;
        rows.push_back(row);
    }

    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
    rematch();
}

void SubtitleMatchModel::setFolder(const QString& folder)
{
    const QString root = localPath(folder);
    if (root.isEmpty())
        return;
    m_show = root;
    m_playlist = false;
    m_target = root.section('/', -1, -1, QString::SectionSkipEmpty);
    emit targetChanged();

    /* its videos first, then its subfolders' */
    QStringList files;
    QDirIterator it(root, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString path = QDir::fromNativeSeparators(it.next());
        if (isVideoFile(path))
            files.push_back(path.mid(root.size() + 1));
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), [&collator](const QString& a, const QString& b) {
        const int depthA = a.count('/'), depthB = b.count('/');
        if (depthA != depthB)
            return depthA < depthB;
        return collator.compare(a, b) < 0;
    });

    QStringList mrls;
    for (const QString& file : files)
        mrls.push_back(pathToMrl(root + '/' + file));

    /* the source the Show remembers, else the Show's folder */
    m_defaultSource = root;
    m_rows.clear();
    const std::optional<ShowSubtitleSources::Show> remembered =
        ShowSubtitleSources(m_intf->mainSettings).showOf(root);
    if (remembered && QFileInfo::exists(remembered->source))
        loadSource(Source(remembered->source), remembered->language, remembered->all);
    else
        loadSource(Source(root), {}, false);
    setEpisodes(mrls, root);
}

void SubtitleMatchModel::setPlaylist(const QString& name, const QStringList& mediaMrls)
{
    m_show.clear();
    m_playlist = true;
    m_target = name;
    emit targetChanged();

    /* the folder of the first local media */
    QString folder;
    for (const QString& mrl : mediaMrls)
    {
        const QString path = localPath(mrl);
        if (!path.isEmpty())
        {
            folder = QFileInfo(path).absolutePath();
            break;
        }
    }

    m_defaultSource = folder;
    m_rows.clear();
    loadSource(Source(folder), {}, false);
    setEpisodes(mediaMrls, {});
}

void SubtitleMatchModel::loadPlaylist(const MLItemId& playlistId, const QString& name)
{
    if (!m_ml)
        return;

    struct Ctx {
        QStringList mrls;
    };
    m_ml->runOnMLThread<Ctx>(this,
    //ML thread
    [id = playlistId.id](vlc_medialibrary_t* ml, Ctx& ctx) {
        ml_unique_ptr<vlc_ml_media_list_t> list{ vlc_ml_list_playlist_media(ml, nullptr, id) };
        if (!list)
            return;
        for (const vlc_ml_media_t& media : ml_range_iterate<vlc_ml_media_t>(list))
        {
            if (media.p_files == nullptr)
                continue;
            for (size_t i = 0; i < media.p_files->i_nb_items; ++i)
            {
                const vlc_ml_file_t& file = media.p_files->p_items[i];
                if (file.i_type == VLC_ML_FILE_TYPE_MAIN && file.psz_mrl)
                {
                    ctx.mrls.push_back(qfu(file.psz_mrl));
                    break;
                }
            }
        }
    },
    //UI thread
    [this, name](quint64, Ctx& ctx) {
        setPlaylist(name, ctx.mrls);
    });
}

void SubtitleMatchModel::setSource(const QUrl& source)
{
    const QString path = source.isLocalFile() ? source.toLocalFile() : localPath(source.toString());
    if (path.isEmpty())
        return;
    loadSource(Source(path), {}, false);
    rematch();
}

void SubtitleMatchModel::resetSource()
{
    if (m_defaultSource.isEmpty())
        return;
    loadSource(Source(m_defaultSource), {}, false);
    rematch();
}

void SubtitleMatchModel::browseFolder()
{
    const QUrl folder = QFileDialog::getExistingDirectoryUrl(
        nullptr, qtr("Subtitles Folder"), QUrl::fromLocalFile(m_source.path()));
    if (!folder.isEmpty())
        setSource(folder);
}

void SubtitleMatchModel::browseZip()
{
    const QUrl zip = QFileDialog::getOpenFileUrl(
        nullptr, qtr("Subtitles Archive"), QUrl::fromLocalFile(m_source.path()),
        qtr("Zip archives") + QStringLiteral(" (*.zip)"));
    if (!zip.isEmpty())
        setSource(zip);
}

void SubtitleMatchModel::loadSource(const Source& source, const QString& language, bool all)
{
    m_source = source;
    m_candidates = source.list();

    QStringList codes;
    for (const Candidate& candidate : m_candidates)
        if (!candidate.language.isEmpty() && !codes.contains(candidate.language))
            codes.push_back(candidate.language);
    std::sort(codes.begin(), codes.end(), [](const QString& a, const QString& b) {
        return QString::localeAwareCompare(languageName(a), languageName(b)) < 0;
    });
    m_languageCodes = codes;

    /* the language the Show was matched with, else the preferred one, else
     * the first */
    if (codes.size() < 2)
        m_languageIndex = -1;
    else if (all)
        m_languageIndex = codes.size();
    else if (codes.contains(language))
        m_languageIndex = codes.indexOf(language);
    else
        m_languageIndex = std::max<int>(codes.indexOf(preferredLanguage(VLC_OBJECT(m_intf))), 0);

    for (Row& row : m_rows)
        row.overridden = false;
    emit sourceChanged();
    emit languageIndexChanged();
}

void SubtitleMatchModel::rematch()
{
    const Filter current = filter();
    for (Row& row : m_rows)
        if (!row.overridden)
            row.matches = subtitlematching::match(row.path, m_candidates, current);
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1),
                         { MatchRole, MatchedRole, OverriddenRole });
    emit matchedCountChanged();
}

QVariantList SubtitleMatchModel::candidates(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    const Row& episode = m_rows[row];
    QVariantList list;
    for (int id : rank(episode.path, m_candidates, filter()))
    {
        const Candidate& candidate = m_candidates[id];
        list.push_back(QVariantMap{
            { "id", id },
            { "name", candidate.name },
            { "path", candidate.path },
            { "language", candidate.language.isEmpty() ? QString() : languageName(candidate.language) },
            { "current", episode.matches.contains(id) },
        });
    }
    return list;
}

void SubtitleMatchModel::setMatch(int row, int candidateId)
{
    if (row < 0 || row >= m_rows.size() || candidateId < -1 || candidateId >= m_candidates.size())
        return;
    Row& episode = m_rows[row];
    const bool wasMatched = !episode.matches.isEmpty();
    episode.matches = candidateId < 0 ? QVector<int>{} : QVector<int>{ candidateId };
    episode.overridden = true;
    emit dataChanged(index(row), index(row), { MatchRole, MatchedRole, OverriddenRole });
    if (wasMatched != !episode.matches.isEmpty())
        emit matchedCountChanged();
}

void SubtitleMatchModel::save()
{
    QStringList episodes;
    for (const Row& row : m_rows)
    {
        episodes.push_back(row.path);
        QStringList subtitles;
        for (int match : row.matches)
        {
            const QString mrl = m_source.resolve(m_candidates[match]);
            if (mrl.isEmpty())
                msg_Warn(m_intf, "unable to read the subtitles '%s'", qtu(m_candidates[match].path));
            else
                subtitles.push_back(mrl);
        }
        if (subtitles.isEmpty())
            continue;
        if (m_ml)
            m_ml->addSubtitleMatches(row.mrl, subtitles);
        attachToPlayer(m_intf, row.mrl, subtitles);
    }

    /* the Show remembers its source, for the episodes added later */
    if (m_show.isEmpty() || !m_source.isValid())
        return;
    const Filter current = filter();
    ShowSubtitleSources(m_intf->mainSettings).remember({
        m_show, m_source.path(), current.language, current.all, episodes });
}
