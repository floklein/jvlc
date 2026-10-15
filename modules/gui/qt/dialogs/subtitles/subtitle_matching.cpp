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
#include "subtitle_matching.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_input_item.h>
#include <vlc_interface.h>
#include <vlc_iso_lang.h>
#include <vlc_media_library.h>
#include <vlc_player.h>
#include <vlc_stream.h>
#include <vlc_stream_extractor.h>
#include <vlc_url.h>

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlhelper.hpp"
#include "player/player_controller.hpp"

#include <QCollator>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>

#include <algorithm>
#include <functional>

namespace subtitlematching {

namespace {

// a subtitle file name ending with a tag, not a language
const QStringList& tags()
{
    static const QStringList list = { "forced", "sdh", "hi", "cc", "default", "full" };
    return list;
}

// the matches below this similarity are too far to be picked
constexpr double MIN_SIMILARITY = 0.5;

using Number = EpisodeNumber;

Number parseNumber(const QString& name)
{
    return parseEpisode(name);
}

QString baseName(const QString& path)
{
    const QString name = path.section('/', -1);
    const int dot = name.lastIndexOf('.');
    return dot > 0 ? name.left(dot) : name;
}

const QRegularExpression& separators()
{
    static const QRegularExpression re(QStringLiteral("[\\s._\\-\\[\\]()]+"));
    return re;
}

// the code of a language named by this token, if it's a language
QString languageOf(const QString& token, bool nameOnly)
{
    if (token.isEmpty())
        return {};
    const QByteArray utf8 = token.toUtf8();
    const iso639_lang_t* lang = nullptr;
    if (!nameOnly && (token.size() == 2 || token.size() == 3))
    {
        lang = vlc_find_iso639(utf8.constData(), false);
        /* a 3 letter code only for the common languages: "day" or "web"
         * are words more often than codes */
        if (lang && token.size() == 3 && lang->psz_iso639_1[0] == '\0')
            lang = nullptr;
    }
    if (!lang && token.size() > 3)
    {
        /* the names of the table with a synonym ("Spanish; Castilian") */
        static const char* const aliases[][2] = {
            { "spanish", "es" }, { "castilian", "es" }, { "dutch", "nl" },
            { "flemish", "nl" }, { "catalan", "ca" }, { "romanian", "ro" },
            { "brazilian", "pt" },
        };
        for (const auto& alias : aliases)
            if (token.compare(QLatin1String(alias[0]), Qt::CaseInsensitive) == 0)
                return QString::fromLatin1(alias[1]);
        /* the English name of the language, as a whole */
        lang = vlc_find_iso639(utf8.constData(), true);
    }
    if (!lang)
        return {};
    return QString::fromLatin1(lang->psz_iso639_1[0] ? lang->psz_iso639_1 : lang->psz_iso639_2B);
}

// the name without its extension, language and tags, as lower case words
QString normalized(const QString& path)
{
    QStringList tokens = baseName(path).toLower().split(separators(), Qt::SkipEmptyParts);
    while (tokens.size() > 1 && tags().contains(tokens.last()))
        tokens.removeLast();
    if (tokens.size() > 1 && !languageOf(tokens.last(), false).isEmpty())
        tokens.removeLast();
    return tokens.join(' ');
}

QSet<QString> bigrams(const QString& text)
{
    QSet<QString> set;
    for (int i = 0; i + 1 < text.size(); ++i)
        set.insert(text.mid(i, 2));
    return set;
}

// 0 to 1: the Dice coefficient of the letter pairs, averaged with how much
// of the shorter name the longer holds
double similarity(const QString& a, const QString& b)
{
    const QSet<QString> x = bigrams(a), y = bigrams(b);
    if (x.isEmpty() || y.isEmpty())
        return a == b ? 1. : 0.;
    const double common = QSet<QString>(x).intersect(y).size();
    const double dice = 2. * common / (x.size() + y.size());
    const double containment = common / std::min(x.size(), y.size());
    return (dice + containment) / 2.;
}

// how well a candidate fits an episode
struct Score
{
    // 3: same season and episode, 2: same episode, 1: no number to compare,
    // 0: another episode
    int number = 1;
    bool language = false;
    double similarity = 0.;

    bool operator>(const Score& other) const
    {
        if (number != other.number)
            return number > other.number;
        if (language != other.language)
            return language;
        return similarity > other.similarity;
    }

    // good enough to be picked
    bool matches() const
    {
        return number >= 2 || (number == 1 && similarity >= MIN_SIMILARITY);
    }
};

Score score(const Number& episode, const QString& episodeName,
            const Candidate& candidate, const QString& language)
{
    Score result;
    const Number number = parseNumber(baseName(candidate.path));
    if (episode.episode >= 0 && number.episode >= 0)
    {
        if (episode.episode != number.episode
         || (episode.season >= 0 && number.season >= 0 && episode.season != number.season))
            result.number = 0;
        else
            result.number = episode.season >= 0 && number.season >= 0 ? 3 : 2;
    }
    result.language = !language.isEmpty() && candidate.language == language;
    result.similarity = similarity(episodeName, normalized(candidate.path));
    return result;
}

QCollator naturalCollator()
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    return collator;
}

// file names are case insensitive on Windows
Qt::CaseSensitivity pathCase()
{
#ifdef _WIN32
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

// a .zip entry or a relative path that stays inside its folder
bool isSafeRelativePath(const QString& path)
{
    const QString clean = QDir::cleanPath(path);
    return !clean.isEmpty() && !QDir::isAbsolutePath(clean) && !clean.contains(':')
        && clean != QLatin1String("..") && !clean.startsWith(QLatin1String("../"));
}

} // anonymous namespace

EpisodeNumber parseEpisode(const QString& name)
{
    static const QRegularExpression patterns[] = {
        // S01E02, s1.e2
        QRegularExpression(QStringLiteral("(?<![a-z0-9])s(\\d{1,2})[ ._-]*e(\\d{1,3})(?!\\d)"),
                           QRegularExpression::CaseInsensitiveOption),
        // 1x02, not 1920x1080, nor an audio and a video codec (DDP5.1x264)
        QRegularExpression(QStringLiteral("(?<![a-z0-9])(?<!\\d\\.)(\\d{1,2})x(?!26[45](?!\\d))(\\d{1,3})(?![a-z0-9])"),
                           QRegularExpression::CaseInsensitiveOption),
        // E02, Ep02, Episode 2
        QRegularExpression(QStringLiteral("(?<![a-z0-9])(?:e|ep|episode)[ ._-]*(\\d{1,3})(?!\\d)"),
                           QRegularExpression::CaseInsensitiveOption),
    };

    for (const QRegularExpression& pattern : patterns)
    {
        const QRegularExpressionMatch match = pattern.match(name);
        if (!match.hasMatch())
            continue;
        EpisodeNumber number;
        number.start = match.capturedStart(0);
        if (pattern.captureCount() == 2)
        {
            number.season = match.captured(1).toInt();
            number.episode = match.captured(2).toInt();
        }
        else
        {
            number.episode = match.captured(1).toInt();
            number.numberStart = match.capturedStart(1);
        }
        return number;
    }
    return {};
}

const QStringList& subtitleExtensions()
{
    static const QStringList extensions = [] {
        QStringList list;
        for (const QString& pattern : QString(EXTENSIONS_SUBTITLE).split(';', Qt::SkipEmptyParts))
        {
            const QString ext = pattern.mid(pattern.indexOf('.') + 1).toLower();
            if (!ext.isEmpty() && ext != QLatin1String("txt"))
                list.push_back(ext);
        }
        return list;
    }();
    return extensions;
}

bool isVideoFile(const QString& fileName)
{
    static const QSet<QString> extensions = [] {
        QSet<QString> set;
        for (const char* ext : { EXTENSIONS_VIDEO_CSV })
            set.insert(QString::fromLatin1(ext));
        return set;
    }();
    return extensions.contains(QFileInfo(fileName).suffix().toLower());
}

QString pathToMrl(const QString& path)
{
    char* uri = vlc_path2uri(qtu(QDir::toNativeSeparators(path)), "file");
    if (uri == nullptr)
        return {};
    QString mrl = qfu(uri);
    free(uri);
    return mrl;
}

QString language(const QString& path)
{
    QStringList tokens = baseName(path).split(separators(), Qt::SkipEmptyParts);
    while (tokens.size() > 1 && tags().contains(tokens.last().toLower()))
        tokens.removeLast();
    if (tokens.size() > 1)
    {
        const QString code = languageOf(tokens.last(), false);
        if (!code.isEmpty())
            return code;
    }

    /* "English/Show S01E02.srt" */
    const QStringList folders = path.split('/', Qt::SkipEmptyParts);
    if (folders.size() > 1)
        return languageOf(folders[folders.size() - 2], true);
    return {};
}

QString languageName(const QString& code)
{
    const iso639_lang_t* lang = vlc_find_iso639(qtu(code), false);
    /* "Spanish; Castilian": the first name */
    return lang ? qfu(vlc_gettext(lang->psz_eng_name)).section(';', 0, 0).trimmed() : code;
}

QString preferredLanguage(vlc_object_t* obj)
{
    /* the command line, else the Profile */
    char* languages = var_InheritString(obj, "sub-language");
    if (languages == nullptr)
        return {};
    const QString first = qfu(languages).section(',', 0, 0).trimmed();
    free(languages);
    if (first.isEmpty())
        return {};
    const iso639_lang_t* lang = vlc_find_iso639(qtu(first), true);
    if (lang == nullptr)
        return {};
    return QString::fromLatin1(lang->psz_iso639_1[0] ? lang->psz_iso639_1 : lang->psz_iso639_2B);
}

QStringList preferredLanguages(vlc_object_t* obj)
{
    QStringList codes;
    const QString preferred = preferredLanguage(obj);
    if (!preferred.isEmpty())
        codes.push_back(preferred);
    /* else the interface language, else English, as OpenSubtitles does */
    const QString system = QLocale().name().section('_', 0, 0);
    const iso639_lang_t* lang = system.isEmpty() ? nullptr : vlc_find_iso639(qtu(system), false);
    if (lang != nullptr && lang->psz_iso639_1[0])
        codes.push_back(QString::fromLatin1(lang->psz_iso639_1));
    codes.push_back(QStringLiteral("en"));
    codes.removeDuplicates();
    return codes;
}

QVector<ArchiveEntry> listArchive(vlc_object_t* obj, const QString& archiveMrl)
{
    QVector<ArchiveEntry> entries;
    stream_t* stream = vlc_stream_NewURL(obj, qtu(archiveMrl));
    if (stream == nullptr)
        return entries;
    if (vlc_stream_directory_Attach(&stream, nullptr, nullptr, 0) != VLC_SUCCESS)
    {
        msg_Warn(obj, "unable to list the archive '%s'", qtu(archiveMrl));
        vlc_stream_Delete(stream);
        return entries;
    }

    input_item_t* root = input_item_New(qtu(archiveMrl), "archive");
    if (root != nullptr)
    {
        /* every file, by its path in the archive */
        input_item_AddOption(root, "ignore-filetypes=\"\"", VLC_INPUT_OPTION_TRUSTED);
        input_item_AddOption(root, "no-sub-autodetect-file", VLC_INPUT_OPTION_TRUSTED);
        input_item_AddOption(root, "extractor-flatten", VLC_INPUT_OPTION_TRUSTED);
    }
    input_item_node_t* node = root ? input_item_node_Create(root) : nullptr;
    if (node != nullptr && vlc_stream_ReadDir(stream, node) == VLC_SUCCESS)
    {
        std::function<void(input_item_node_t*)> collect = [&](input_item_node_t* parent) {
            for (int i = 0; i < parent->i_children; ++i)
            {
                const input_item_t* item = parent->pp_children[i]->p_item;
                if (item->psz_name != nullptr && item->psz_uri != nullptr)
                    entries.push_back({ qfu(item->psz_name), qfu(item->psz_uri) });
                collect(parent->pp_children[i]);
            }
        };
        collect(node);
    }
    if (node != nullptr)
        input_item_node_Delete(node);
    if (root != nullptr)
        input_item_Release(root);
    vlc_stream_Delete(stream);
    return entries;
}

QByteArray readMrl(vlc_object_t* obj, const QString& mrl, qint64 maxSize)
{
    /* an MRL: it may name a file inside an archive */
    stream_t* stream = vlc_stream_NewMRL(obj, qtu(mrl));
    if (stream == nullptr)
        return {};
    /* stays null if empty */
    QByteArray data;
    char buffer[16384];
    ssize_t read;
    while ((read = vlc_stream_Read(stream, buffer, sizeof (buffer))) > 0)
    {
        if (read > maxSize - data.size())
        {
            msg_Warn(obj, "'%s' is too large", qtu(mrl));
            data = QByteArray();
            break;
        }
        data.append(buffer, read);
    }
    vlc_stream_Delete(stream);
    return data;
}

Source::Source(vlc_object_t* obj, const QString& path)
    : m_obj(obj)
    , m_path(QDir::fromNativeSeparators(QDir::cleanPath(path)))
{
}

bool Source::isZip() const
{
    return m_path.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive);
}

QVector<Candidate> Source::list() const
{
    QVector<Candidate> candidates;
    auto add = [&candidates](const QString& path, const QString& mrl) {
        if (!subtitleExtensions().contains(QFileInfo(path).suffix().toLower()))
            return;
        candidates.push_back({ path.section('/', -1), path, language(path), mrl });
    };

    if (isZip())
    {
        /* through the archive access module */
        for (const ArchiveEntry& entry : listArchive(m_obj, pathToMrl(m_path)))
        {
            const QString path = QString(entry.path).replace('\\', '/');
            if (isSafeRelativePath(path))
                add(QDir::cleanPath(path), entry.mrl);
        }
    }
    else
    {
        const QDir root(m_path);
        QDirIterator it(m_path, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext())
            add(QDir::fromNativeSeparators(root.relativeFilePath(it.next())), {});
    }

    QCollator collator = naturalCollator();
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&collator](const Candidate& a, const Candidate& b) {
        return collator.compare(a.path, b.path) < 0;
    });
    return candidates;
}

QString Source::resolve(const Candidate& candidate) const
{
    if (!isSafeRelativePath(candidate.path))
        return {};
    if (!isZip())
        return pathToMrl(QDir(m_path).filePath(candidate.path));

    /* next to the .zip, in a folder named after it, or in VLC's data folder
     * if that one can't be written */
    const QFileInfo zipFile(m_path);
    QStringList folders = { zipFile.absolutePath() + '/' + zipFile.completeBaseName() };
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(m_path.toUtf8(), QCryptographicHash::Md5).toHex().left(8));
    char* userdata = config_GetUserDir(VLC_USERDATA_DIR);
    if (userdata)
    {
        folders.push_back(qfu(userdata) + QStringLiteral("/subtitles/")
                          + zipFile.completeBaseName() + '-' + hash);
        free(userdata);
    }

    const QByteArray data = readMrl(m_obj, candidate.mrl, maxSubtitleSize);
    if (data.isNull())
        return {};

    for (const QString& folder : folders)
    {
        const QString path = folder + '/' + candidate.path;
        QFile file(path);
        /* the same file, extracted before; never another file */
        if (file.open(QIODevice::ReadOnly))
        {
            if (file.size() == data.size() && file.readAll() == data)
                return pathToMrl(path);
            continue;
        }
        if (!QDir().mkpath(QFileInfo(path).absolutePath())
         || !file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            continue;
        if (file.write(data) != data.size())
        {
            file.remove();
            continue;
        }
        return pathToMrl(path);
    }
    return {};
}

QVector<int> rank(const QString& episode, const QVector<Candidate>& candidates,
                  const Filter& filter)
{
    const Number number = parseNumber(baseName(episode));
    const QString name = normalized(episode);
    QVector<Score> scores;
    QVector<int> order;
    for (int i = 0; i < candidates.size(); ++i)
    {
        scores.push_back(score(number, name, candidates[i], filter.language));
        order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&scores](int a, int b) {
        return scores[a] > scores[b];
    });
    return order;
}

QVector<int> match(const QString& episode, const QVector<Candidate>& candidates,
                   const Filter& filter)
{
    const Number number = parseNumber(baseName(episode));
    const QString name = normalized(episode);

    QVector<int> best;
    QStringList languages;
    for (int index : rank(episode, candidates, filter))
    {
        const Candidate& candidate = candidates[index];
        if (!score(number, name, candidate, filter.language).matches())
            continue;
        if (filter.all)
        {
            /* the best of each language */
            if (languages.contains(candidate.language))
                continue;
            languages.push_back(candidate.language);
            best.push_back(index);
            continue;
        }
        /* another language than the filter's: an unknown one only */
        if (!filter.language.isEmpty() && !candidate.language.isEmpty()
         && candidate.language != filter.language)
            continue;
        best.push_back(index);
        break;
    }
    return best;
}

void attachToPlayer(qt_intf_t* intf, const QString& mediaMrl, const QStringList& subtitleMrls)
{
    vlc_player_t* player = intf->p_player;
    vlc_player_locker lock{ player };
    input_item_t* item = vlc_player_GetCurrentMedia(player);
    if (item == nullptr || qfu(item->psz_uri) != mediaMrl)
        return;

    QSet<QString> attached;
    {
        vlc_mutex_locker itemLock{ &item->lock };
        for (int i = 0; i < item->i_slaves; ++i)
            attached.insert(qfu(item->pp_slaves[i]->psz_uri));
    }

    bool shown = vlc_player_GetSelectedTrack(player, SPU_ES) != nullptr;
    for (const QString& mrl : subtitleMrls)
    {
        if (attached.contains(mrl))
            continue;
        if (vlc_player_AddAssociatedMedia(player, SPU_ES, qtu(mrl), !shown,
                                          false, false) == VLC_SUCCESS)
            shown = true;
        else
            msg_Warn(intf, "unable to add the subtitles '%s'", qtu(mrl));
    }
}

} // namespace subtitlematching

// ShowSubtitleSources

ShowSubtitleSources::ShowSubtitleSources(QSettings* settings)
    : m_settings(settings)
{
}

QVector<ShowSubtitleSources::Show> ShowSubtitleSources::load() const
{
    QVector<Show> shows;
    if (!m_settings)
        return shows;
    const int size = m_settings->beginReadArray(QStringLiteral("SubtitleSources"));
    for (int i = 0; i < size; ++i)
    {
        m_settings->setArrayIndex(i);
        Show show;
        show.folder = m_settings->value(QStringLiteral("Show")).toString();
        show.source = m_settings->value(QStringLiteral("Source")).toString();
        show.language = m_settings->value(QStringLiteral("Language")).toString();
        show.all = m_settings->value(QStringLiteral("All")).toBool();
        show.episodes = m_settings->value(QStringLiteral("Episodes")).toStringList();
        if (!show.folder.isEmpty() && !show.source.isEmpty())
            shows.push_back(show);
    }
    m_settings->endArray();
    return shows;
}

void ShowSubtitleSources::store(const QVector<Show>& shows)
{
    if (!m_settings)
        return;
    m_settings->remove(QStringLiteral("SubtitleSources"));
    m_settings->beginWriteArray(QStringLiteral("SubtitleSources"), shows.size());
    for (int i = 0; i < shows.size(); ++i)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue(QStringLiteral("Show"), shows[i].folder);
        m_settings->setValue(QStringLiteral("Source"), shows[i].source);
        m_settings->setValue(QStringLiteral("Language"), shows[i].language);
        m_settings->setValue(QStringLiteral("All"), shows[i].all);
        m_settings->setValue(QStringLiteral("Episodes"), shows[i].episodes);
    }
    m_settings->endArray();
    m_settings->sync();
}

void ShowSubtitleSources::remember(const Show& show)
{
    QVector<Show> shows = load();
    auto it = std::find_if(shows.begin(), shows.end(), [&show](const Show& other) {
        return other.folder == show.folder;
    });
    if (it != shows.end())
        *it = show;
    else
        shows.push_back(show);
    store(shows);
}

std::optional<ShowSubtitleSources::Show> ShowSubtitleSources::showOf(const QString& filePath) const
{
    const QString path = QDir::fromNativeSeparators(QDir::cleanPath(filePath));
    std::optional<Show> found;
    for (const Show& show : load())
    {
        if (path.compare(show.folder, subtitlematching::pathCase()) != 0
         && !path.startsWith(show.folder + QLatin1Char('/'), subtitlematching::pathCase()))
            continue;
        if (!found || show.folder.size() > found->folder.size())
            found = show;
    }
    return found;
}

// ShowSubtitleMatcher

ShowSubtitleMatcher::ShowSubtitleMatcher(qt_intf_t* intf, MediaLib* ml, QSettings* settings,
                                         QObject* parent)
    : QObject(parent)
    , m_intf(intf)
    , m_ml(ml)
    , m_settings(settings)
{
}

void ShowSubtitleMatcher::matchOnPlay(const QString& mediaMrl)
{
    if (!m_ml)
        return;
    char* localPath = vlc_uri2path(qtu(mediaMrl));
    if (localPath == nullptr)
        return;
    const QString path = QDir::fromNativeSeparators(qfu(localPath));
    free(localPath);

    ShowSubtitleSources sources(m_settings);
    std::optional<ShowSubtitleSources::Show> show = sources.showOf(path);
    if (!show)
        return;
    const QString episode = path.mid(show->folder.size() + 1);
    if (show->episodes.contains(episode))
        return;

    /* once: it was matched, or there was nothing to match */
    show->episodes.push_back(episode);
    sources.remember(*show);

    struct Ctx {
        QStringList subtitles;
    };
    m_ml->runOnMLThread<Ctx>(this,
    //ML thread
    [media = mediaMrl, source = show->source, language = show->language,
     all = show->all, episode, obj = VLC_OBJECT(m_intf)](vlc_medialibrary_t* ml, Ctx& ctx) {
        /* it has Subtitle matches already */
        ml_unique_ptr<vlc_ml_media_t> entry{ vlc_ml_get_media_by_mrl(ml, qtu(media)) };
        if (entry && entry->p_files)
            for (size_t i = 0; i < entry->p_files->i_nb_items; ++i)
                if (entry->p_files->p_items[i].i_type == VLC_ML_FILE_TYPE_SUBTITLE
                 && entry->p_files->p_items[i].b_external)
                    return;

        const subtitlematching::Source from(obj, source);
        const QVector<subtitlematching::Candidate> candidates = from.list();
        for (int index : subtitlematching::match(episode, candidates, { language, all }))
        {
            const QString mrl = from.resolve(candidates[index]);
            if (!mrl.isEmpty())
                ctx.subtitles.push_back(mrl);
        }
    },
    //UI thread
    [this, mediaMrl](quint64, Ctx& ctx) {
        if (ctx.subtitles.isEmpty())
            return;
        m_ml->addSubtitleMatches(mediaMrl, ctx.subtitles);
        subtitlematching::attachToPlayer(m_intf, mediaMrl, ctx.subtitles);
        emit matched(mediaMrl, ctx.subtitles);
    },
    "subtitle-matches");
}
