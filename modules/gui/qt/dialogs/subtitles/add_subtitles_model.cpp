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

#include "medialibrary/medialib.hpp"
#include "player/player_controller.hpp"

#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_url.h>

#include <algorithm>

#include <QCollator>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QSet>

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

} // anonymous namespace

AddSubtitlesModel::AddSubtitlesModel(qt_intf_t* intf, MediaLib* ml, const QString& mediaMrl,
                                     QObject* parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
    , m_ml(ml)
    , m_media(mediaMrl)
{
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

void AddSubtitlesModel::save()
{
    QStringList chosen;
    for (const Row& row : m_rows)
        if (row.checked)
            chosen.push_back(row.mrl);
    if (chosen.isEmpty())
        return;

    if (m_ml)
        m_ml->addSubtitleMatches(m_media, chosen);

    /* the file is playing: attach them now */
    vlc_player_t* player = m_intf->p_player;
    vlc_player_locker lock{ player };
    input_item_t* item = vlc_player_GetCurrentMedia(player);
    if (item == nullptr || qfu(item->psz_uri) != m_media)
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
            msg_Warn(m_intf, "unable to add the subtitles '%s'", qtu(mrl));
    }
}
