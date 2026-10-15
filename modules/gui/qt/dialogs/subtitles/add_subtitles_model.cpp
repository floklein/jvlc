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
#include "add_subtitles_model.hpp"
#include "http_request.hpp"

#include "medialibrary/medialib.hpp"
#include "player/player_controller.hpp"

#include <vlc_configuration.h>
#include <vlc_input_item.h>
#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_stream.h>
#include <vlc_stream_extractor.h>
#include <vlc_url.h>

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>

namespace {

/* the subtitle file extensions, as in the subtitle file filter, without
 * ".txt", too common for other files */
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

QString pathToMrl(const QString& path)
{
    char* uri = vlc_path2uri(qtu(QDir::toNativeSeparators(path)), "file");
    if (uri == nullptr)
        return {};
    QString mrl = qfu(uri);
    free(uri);
    return mrl;
}

/* the subtitle files of a folder, in natural order */
QStringList subtitleFiles(const QDir& dir)
{
    QStringList files;
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Readable);
    for (const QFileInfo& entry : entries)
        if (subtitleExtensions().contains(entry.suffix().toLower()))
            files.push_back(entry.absoluteFilePath());

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), [&collator](const QString& a, const QString& b) {
        return collator.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0;
    });
    return files;
}

/* the subtitle files From Link… takes */
const QStringList& linkExtensions()
{
    static const QStringList extensions = {
        QStringLiteral("srt"), QStringLiteral("vtt"), QStringLiteral("ass"), QStringLiteral("ssa"),
    };
    return extensions;
}

/* the subtitle files of an archive: { name, MRL } pairs, in natural order */
QVector<QPair<QString, QString>> listArchive(vlc_object_t* obj, const QString& archiveMrl)
{
    QVector<QPair<QString, QString>> files;
    stream_t* stream = vlc_stream_NewURL(obj, qtu(archiveMrl));
    if (stream == nullptr)
    {
        msg_Warn(obj, "unable to open the archive '%s'", qtu(archiveMrl));
        return files;
    }
    if (vlc_stream_directory_Attach(&stream, nullptr, nullptr, 0) != VLC_SUCCESS)
    {
        msg_Warn(obj, "unable to list the archive '%s'", qtu(archiveMrl));
        vlc_stream_Delete(stream);
        return files;
    }

    input_item_t* root = input_item_New(qtu(archiveMrl), "archive");
    if (root != nullptr)
    {
        /* subtitles are ignored by default, or attached to videos */
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
                input_item_node_t* child = parent->pp_children[i];
                /* an entry's name can't lead out of the folder, nor be
                 * anything but a subtitle */
                const QString name = opensubtitles::safeFileName(qfu(child->p_item->psz_name));
                if (opensubtitles::isDownloadExtension(QFileInfo(name).suffix().toLower()))
                    files.push_back({ name, qfu(child->p_item->psz_uri) });
                collect(child);
            }
        };
        collect(node);
    }
    if (node != nullptr)
        input_item_node_Delete(node);
    if (root != nullptr)
        input_item_Release(root);
    vlc_stream_Delete(stream);

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), [&collator](const auto& a, const auto& b) {
        return collator.compare(a.first, b.first) < 0;
    });
    return files;
}

} // anonymous namespace

AddSubtitlesModel::AddSubtitlesModel(qt_intf_t* intf, MediaLib* ml, const QString& mediaMrl,
                                     QObject* parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
    , m_ml(ml)
    , m_media(mediaMrl)
{
    char* cache = config_GetUserDir(VLC_CACHE_DIR);
    if (cache != nullptr)
    {
        m_cacheFolder = QDir(qfu(cache)).filePath(QStringLiteral("subtitles"));
        free(cache);
    }

    char* path = vlc_uri2path(qtu(mediaMrl));
    if (path != nullptr)
    {
        const QFileInfo file(qfu(path));
        free(path);
        m_folderPath = file.absolutePath();
        m_folder = QDir::fromNativeSeparators(m_folderPath)
                       .split('/', Qt::SkipEmptyParts).join(QStringLiteral(" / "));
        listFolder(m_folderPath);
    }
}

AddSubtitlesModel::~AddSubtitlesModel()
{
    /* stop the downloads before their folder goes */
    m_worker.reset();
    m_downloads.reset();
}

void AddSubtitlesModel::listFolder(const QString& path)
{
    const QDir dir(path);
    QStringList files = subtitleFiles(dir);

    /* and the "Subs" folder next to the video */
    const QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& folder : folders)
        if (folder.compare(QLatin1String("subs"), Qt::CaseInsensitive) == 0)
            files += subtitleFiles(QDir(dir.filePath(folder)));

    for (const QString& file : files)
        m_rows.push_back({ QFileInfo(file).fileName(), pathToMrl(file), false });
}

int AddSubtitlesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant AddSubtitlesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows[index.row()];
    switch (role)
    {
    case Qt::DisplayRole:
    case NameRole:
        return row.name;
    case UrlRole:
        return row.mrl;
    case CheckedRole:
        return row.checked;
    default:
        return {};
    }
}

QHash<int, QByteArray> AddSubtitlesModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { UrlRole, "url" },
        { CheckedRole, "checked" },
    };
}

int AddSubtitlesModel::checkedCount() const
{
    return std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row& row) { return row.checked; });
}

void AddSubtitlesModel::toggle(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    m_rows[row].checked = !m_rows[row].checked;
    emit dataChanged(index(row), index(row), { CheckedRole });
    emit checkedCountChanged();
}

void AddSubtitlesModel::addFiles(const QList<QUrl>& files)
{
    bool checked = false;
    for (const QUrl& url : files)
    {
        const QString mrl = url.isLocalFile() ? pathToMrl(url.toLocalFile())
                                              : url.toString(QUrl::FullyEncoded);
        if (mrl.isEmpty())
            continue;

        auto it = std::find_if(m_rows.begin(), m_rows.end(),
                               [&mrl](const Row& row) { return row.mrl == mrl; });
        if (it != m_rows.end())
        {
            if (!it->checked)
            {
                it->checked = true;
                const int row = std::distance(m_rows.begin(), it);
                emit dataChanged(index(row), index(row), { CheckedRole });
                checked = true;
            }
            continue;
        }

        const QString name = url.isLocalFile() ? QFileInfo(url.toLocalFile()).fileName()
                                               : url.fileName();
        beginInsertRows({}, m_rows.size(), m_rows.size());
        m_rows.push_back({ name, mrl, true });
        endInsertRows();
        emit countChanged();
        checked = true;
    }
    if (checked)
        emit checkedCountChanged();
}

void AddSubtitlesModel::browse()
{
    QString filter = qtr("Subtitle files") + QStringLiteral(" (");
    for (const QString& ext : subtitleExtensions())
        filter += QStringLiteral("*.") + ext + ' ';
    filter = filter.trimmed() + ')';

    const QList<QUrl> files = QFileDialog::getOpenFileUrls(
        nullptr, qtr("Add Subtitles"), QUrl::fromLocalFile(m_folderPath), filter);
    addFiles(files);
}

void AddSubtitlesModel::addRows(const QVector<Row>& rows)
{
    bool checked = false;
    for (const Row& row : rows)
    {
        beginInsertRows({}, m_rows.size(), m_rows.size());
        m_rows.push_back(row);
        endInsertRows();
        checked |= row.checked;
    }
    if (rows.isEmpty())
        return;
    emit countChanged();
    if (checked)
        emit checkedCountChanged();
}

void AddSubtitlesModel::setLinkError(const QString& error)
{
    if (error == m_linkError)
        return;
    m_linkError = error;
    emit linkErrorChanged();
}

void AddSubtitlesModel::addLink(const QString& link)
{
    const QUrl url = QUrl::fromUserInput(link.trimmed());
    if (!url.isValid() || (url.scheme() != QLatin1String("http")
                        && url.scheme() != QLatin1String("https")))
    {
        setLinkError(qtr("Paste a link starting with http:// or https://."));
        return;
    }

    const QString name = opensubtitles::safeFileName(url.fileName());
    const QString suffix = QFileInfo(name).suffix().toLower();
    const bool zip = suffix == QLatin1String("zip");
    if (!zip && !linkExtensions().contains(suffix))
    {
        setLinkError(qtr("The link must point to a .srt, .vtt, .ass or .zip file."));
        return;
    }

    if (!m_downloads)
    {
        m_downloads = std::make_unique<QTemporaryDir>();
        m_worker = std::make_unique<HttpWorker>();
    }
    if (!m_downloads->isValid())
    {
        setLinkError(qtr("The link couldn't be downloaded."));
        return;
    }

    QString target = m_downloads->filePath(name);
    for (int i = 2; QFileInfo::exists(target); ++i)
        target = m_downloads->filePath(QStringLiteral("%1 (%2).%3")
                                           .arg(QFileInfo(name).completeBaseName()).arg(i).arg(suffix));

    setLinkError({});
    ++m_fetching;
    emit fetchingChanged();

    vlc_object_t* obj = VLC_OBJECT(m_intf);
    /* the worker stops in the destructor: "this" outlives the task */
    m_worker->post([this, obj, url, name, target, zip]() {
        HttpRequest request;
        request.url = url;
        request.headers = { { "User-Agent", "VLC v" PACKAGE_VERSION } };
        const HttpResponse response = httpRequest(obj, request);

        QVector<Row> rows;
        QString error;
        QFile file(target);
        if (!response.ok() || response.body.isEmpty())
            error = qtr("The link couldn't be downloaded.");
        else if (!file.open(QIODevice::WriteOnly) || file.write(response.body) != response.body.size())
            error = qtr("The link couldn't be downloaded.");
        else
        {
            file.close();
            if (zip)
            {
                /* checked when there is only one to choose */
                const auto entries = listArchive(obj, pathToMrl(target));
                for (const auto& entry : entries)
                    rows.push_back({ entry.first, entry.second, entries.size() == 1, true });
                if (rows.isEmpty())
                    error = qtr("The .zip file has no subtitle files.");
            }
            else
                rows.push_back({ name, pathToMrl(target), true, true });
        }

        QMetaObject::invokeMethod(this, [this, rows, error]() {
            addRows(rows);
            setLinkError(error);
            --m_fetching;
            emit fetchingChanged();
        }, Qt::QueuedConnection);
    });
}

QString AddSubtitlesModel::copyFetched(const Row& row)
{
    QString folder = m_folderPath;
    if (folder.isEmpty() || !QFileInfo(folder).isWritable())
        folder = m_cacheFolder;
    if (folder.isEmpty() || !QDir().mkpath(folder))
        return {};

    /* the name comes from a link or an archive: kept a subtitle file, in
     * the folder */
    const QString name = opensubtitles::safeFileName(row.name);
    QString base = QFileInfo(name).completeBaseName();
    if (base.isEmpty())
        base = QStringLiteral("subtitle");
    const QString suffix = opensubtitles::downloadSuffix(name);
    QString path = QDir(folder).filePath(base + QLatin1Char('.') + suffix);
    for (int i = 2; QFileInfo::exists(path); ++i)
        path = QDir(folder).filePath(QStringLiteral("%1 (%2).%3").arg(base).arg(i).arg(suffix));

    /* through a stream: the file may be inside a .zip */
    stream_t* stream = vlc_stream_NewMRL(m_intf, qtu(row.mrl));
    if (stream == nullptr)
        return {};
    QFile out(path);
    bool ok = out.open(QIODevice::WriteOnly);
    char buffer[16384];
    ssize_t read;
    while (ok && (read = vlc_stream_Read(stream, buffer, sizeof (buffer))) > 0)
        ok = out.write(buffer, read) == read;
    vlc_stream_Delete(stream);
    out.close();
    if (!ok)
    {
        out.remove();
        msg_Warn(m_intf, "unable to copy the subtitles '%s'", qtu(row.mrl));
        return {};
    }
    return pathToMrl(path);
}

void AddSubtitlesModel::save()
{
    QStringList chosen;
    for (const Row& row : m_rows)
    {
        if (!row.checked)
            continue;
        const QString mrl = row.fetched ? copyFetched(row) : row.mrl;
        if (!mrl.isEmpty())
            chosen.push_back(mrl);
    }
    attachSubtitles(m_intf, m_ml, m_media, chosen);
}

namespace opensubtitles {

bool isDownloadExtension(const QString& suffix)
{
    static const QStringList extensions = {
        QStringLiteral("srt"), QStringLiteral("vtt"), QStringLiteral("ass"), QStringLiteral("ssa"),
        QStringLiteral("sub"), QStringLiteral("smi"), QStringLiteral("idx"),
    };
    return extensions.contains(suffix);
}

QString downloadSuffix(const QString& fileName)
{
    const QString suffix = QFileInfo(safeFileName(fileName)).suffix().toLower();
    return isDownloadExtension(suffix) ? suffix : QStringLiteral("srt");
}

QString safeFileName(const QString& name)
{
    const qsizetype slash = std::max(name.lastIndexOf(QLatin1Char('/')),
                                     name.lastIndexOf(QLatin1Char('\\')));
    QString leaf = name.mid(slash + 1);
    for (QChar& c : leaf)
        if (c.unicode() < 0x20 || QStringView(u"<>:\"|?*").contains(c))
            c = QLatin1Char('_');
    /* Windows drops trailing dots and spaces: ".." would be left */
    while (leaf.endsWith(QLatin1Char('.')) || leaf.endsWith(QLatin1Char(' ')))
        leaf.chop(1);
    return leaf.trimmed();
}

} // namespace opensubtitles

void attachSubtitles(qt_intf_t* intf, MediaLib* ml, const QString& mediaMrl,
                     const QStringList& chosen)
{
    if (chosen.isEmpty())
        return;

    if (ml)
        ml->addSubtitleMatches(mediaMrl, chosen);

    /* the file is playing: attach them now */
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

    /* the first one turns on if no subtitle is showing */
    bool shown = vlc_player_GetSelectedTrack(player, SPU_ES) != nullptr;
    for (const QString& mrl : chosen)
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
