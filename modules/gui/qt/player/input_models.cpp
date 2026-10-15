/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
#include "input_models.hpp"

#include "player/player_controller.hpp"

#include <vlc_arrays.h>

#include <algorithm>
#include <cstring>

#include <QCryptographicHash>

//***************************
//  track list model
//***************************

/* Take out of the item the slave whose tracks have this string id (see
 * isExternalTrack()), so that it doesn't come back when the item plays again.
 * Returns the slave MRL. */
static QString takeSlave(input_item_t *item, const QString &strId)
{
    const QString source = strId.section('/', 0, 0);
    QString mrl;

    vlc_mutex_locker lock{ &item->lock };
    for (int i = 0; i < item->i_slaves; ++i)
    {
        input_item_slave_t *slave = item->pp_slaves[i];

        /* the MD5 of the MRL, as InputSourceNew() makes it */
        const QByteArray hash = QCryptographicHash::hash(QByteArray(slave->psz_uri),
                                                         QCryptographicHash::Md5).toHex();
        if (source != QLatin1StringView(hash))
            continue;

        mrl = qfu(slave->psz_uri);
        TAB_ERASE(item->i_slaves, item->pp_slaves, i);
        input_item_slave_Delete(slave);
        break;
    }
    return mrl;
}

/* The tracks of an associated media (an input slave, e.g. a subtitle file)
 * have string ids starting with the hash of their source, while those of the
 * media itself start with their category (see EsOutCreateStrId) */
static bool isExternalTrack(vlc_es_id_t *es_id)
{
    const char *str_id = vlc_es_id_GetStrId(es_id);
    if (str_id == nullptr)
        return false;
    const char *end = strchr(str_id, '/');
    if (end == nullptr)
        return false;
    const QLatin1StringView first(str_id, end - str_id);
    for (const char *cat : { "video", "audio", "spu", "data", "unknown" })
        if (first == QLatin1StringView(cat))
            return false;
    return true;
}

TrackListModel::TrackListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags TrackListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int TrackListModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_data.size())
        return QVariant{};
    if (role == Qt::DisplayRole)
        return m_data[row].m_title;
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(m_data[row].m_selected);
    else if (role == ExternalRole)
        return QVariant::fromValue<bool>(m_data[row].m_external);
    else if (role == SecondaryRole)
        return QVariant::fromValue<bool>(m_secondary != nullptr
                                         && m_data[row].m_id.get() == m_secondary);
    else if (role == StyledRole)
        return QVariant::fromValue<bool>(m_data[row].m_styled);
    else if (role == LanguageRole)
        return m_data[row].m_language;
    return QVariant{};
}

bool TrackListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row >= m_data.size())
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    {
        vlc_player_locker lock{ m_player };

        if (select)
        {
            vlc_es_id_t *es_id = m_data[row].m_id.get();
            const enum es_format_category_e cat = vlc_es_id_GetCat(es_id);
            enum vlc_player_select_policy policy =
                ( cat ==  VIDEO_ES ) || m_multiSelect ? VLC_PLAYER_SELECT_SIMULTANEOUS
                                                      : VLC_PLAYER_SELECT_EXCLUSIVE;
            vlc_player_SelectEsId(m_player, es_id, policy);
        }
        else
            vlc_player_UnselectEsId(m_player, m_data[row].m_id.get());
    }
    if (select)
        emit picked(m_data[row].m_language);
    return true;
}

void TrackListModel::select(int row)
{
    if (row < 0 || row >= m_data.size())
        return;
    vlc_es_id_t *es_id = m_data[row].m_id.get();

    const QString language = m_data[row].m_language;
    const int secondaryRow = rowOf(m_secondary);
    if (secondaryRow == row)
    {
        /* the second subtitle becomes the only one: restart it as the
         * first one */
        setSecondary(nullptr);
        {
            vlc_player_locker lock{ m_player };
            vlc_player_UnselectTrackCategory(m_player, vlc_es_id_GetCat(es_id));
            vlc_player_SelectEsId(m_player, es_id, VLC_PLAYER_SELECT_EXCLUSIVE);
        }
        emit picked(language);
        return;
    }

    {
        vlc_player_locker lock{ m_player };
        if (secondaryRow < 0)
            vlc_player_SelectEsId(m_player, es_id, VLC_PLAYER_SELECT_EXCLUSIVE);
        else
            selectPair(es_id, m_data[secondaryRow].m_id.get());
    }
    emit picked(language);
}

void TrackListModel::selectSecondary(int row)
{
    if (row < 0 || row >= m_data.size())
        return;
    vlc_es_id_t *es_id = m_data[row].m_id.get();
    if (vlc_es_id_GetCat(es_id) != SPU_ES || es_id == m_secondary)
        return;

    int primaryRow = -1;
    for (int i = 0; i < m_data.size(); ++i)
    {
        if (m_data[i].m_selected && m_data[i].m_id.get() != m_secondary)
        {
            primaryRow = i;
            break;
        }
    }

    if (primaryRow == row)
        return;

    if (primaryRow < 0)
    {
        /* nothing shown: this becomes the subtitle */
        select(row);
        return;
    }

    {
        vlc_player_locker lock{ m_player };
        selectPair(m_data[primaryRow].m_id.get(), es_id);
    }
    setSecondary(es_id);
}

void TrackListModel::unselectSecondary()
{
    const int row = rowOf(m_secondary);
    if (row < 0)
        return;
    setSecondary(nullptr);
    vlc_player_locker lock{ m_player };
    vlc_player_UnselectEsId(m_player, m_data[row].m_id.get());
}

void TrackListModel::unselectAll()
{
    if (m_data.isEmpty())
        return;
    setSecondary(nullptr);
    {
        vlc_player_locker lock{ m_player };
        vlc_player_UnselectTrackCategory(m_player,
                                         vlc_es_id_GetCat(m_data.first().m_id.get()));
    }
    emit turnedOff();
}

bool TrackListModel::remove(int row)
{
    if (row < 0 || row >= m_data.size() || !m_data[row].m_external)
        return false;

    vlc_es_id_t *es_id = m_data[row].m_id.get();
    const QString strId = qfu(vlc_es_id_GetStrId(es_id));

    if (es_id == m_secondary)
        setSecondary(nullptr);

    QString mediaMrl;
    QString sourceMrl;
    {
        vlc_player_locker lock{ m_player };
        if (m_data[row].m_selected)
            vlc_player_UnselectEsId(m_player, es_id);
        input_item_t *item = vlc_player_GetCurrentMedia(m_player);
        if (item != nullptr)
        {
            mediaMrl = qfu(item->psz_uri);
            sourceMrl = takeSlave(item, strId);
        }
    }

    m_removed.append(strId);

    beginRemoveRows({}, row, row);
    m_data.removeAt(row);
    endRemoveRows();
    emit countChanged();
    updateHasSelection();

    emit externalTrackRemoved(strId, mediaMrl, sourceMrl);
    return true;
}

int TrackListModel::rowOf(const vlc_es_id_t *trackid) const
{
    if (trackid == nullptr)
        return -1;
    for (int i = 0; i < m_data.size(); ++i)
        if (m_data[i].m_id.get() == trackid)
            return i;
    return -1;
}

/* show both tracks, the first one as the main track; the other tracks of the
 * category are unselected and the selected ones keep their place */
void TrackListModel::selectPair(vlc_es_id_t *first, vlc_es_id_t *second)
{
    vlc_es_id_t *list[] = { first, second, nullptr };
    vlc_player_SelectEsIdList(m_player, vlc_es_id_GetCat(first), list);
}

void TrackListModel::setSecondary(vlc_es_id_t *trackid)
{
    if (trackid == m_secondary)
        return;
    const int oldRow = rowOf(m_secondary);
    m_secondary = trackid;
    const int newRow = rowOf(m_secondary);
    if (oldRow >= 0)
        emit dataChanged(index(oldRow), index(oldRow), { SecondaryRole });
    if (newRow >= 0)
        emit dataChanged(index(newRow), index(newRow), { SecondaryRole });
}

void TrackListModel::updateHasSelection()
{
    const bool styledSelected = std::any_of(m_data.cbegin(), m_data.cend(),
                                            [](const Data& t) { return t.m_selected && t.m_styled; });
    if (styledSelected != m_styledSelected)
    {
        m_styledSelected = styledSelected;
        emit styledSelectedChanged();
    }

    const bool hasSelection = std::any_of(m_data.cbegin(), m_data.cend(),
                                          [](const Data& t) { return t.m_selected; });
    if (hasSelection == m_hasSelection)
        return;
    m_hasSelection = hasSelection;
    emit hasSelectionChanged();
}

void TrackListModel::updateTracks(vlc_player_list_action action, const vlc_player_track *track_info)
{
    switch (action) {
    case VLC_PLAYER_LIST_ADDED:
    {
        if (m_removed.contains(qfu(vlc_es_id_GetStrId(track_info->es_id))))
            return;
        beginInsertRows({}, m_data.size(), m_data.size());
        m_data.append(Data{ track_info });
        endInsertRows();
        emit countChanged();
        updateHasSelection();
        break;
    }
    case VLC_PLAYER_LIST_REMOVED:
    {
        auto it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& t) {
            return t.m_id.get() == track_info->es_id;
        });
        if (it == m_data.end())
        {
            /* a removed external track is gone for good */
            m_removed.removeAll(qfu(vlc_es_id_GetStrId(track_info->es_id)));
            return;
        }

        if (it->m_id.get() == m_secondary)
            m_secondary = nullptr;

        int pos = std::distance(m_data.begin(), it);
        beginRemoveRows({}, pos, pos);
        m_data.erase(it);
        endRemoveRows();
        emit countChanged();
        updateHasSelection();
        break;
    }
    case VLC_PLAYER_LIST_UPDATED:
    {
        int pos = 0;
        bool found = false;
        for (Data& d : m_data)
        {
            if (d.m_id.get() == track_info->es_id)
            {
                d.update(track_info);
                found = true;
                break;
            }
            pos++;
        }
        if (!found)
            return;
        QModelIndex dataIndex = index(pos);
        emit dataChanged(dataIndex, dataIndex, { Qt::DisplayRole, Qt::CheckStateRole });
        updateHasSelection();
        break;
    }
    }
}

void TrackListModel::updateTrackSelection(vlc_es_id_t *trackid, bool selected)
{
    if (trackid == NULL)
        return;
    QList<Data>::iterator it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& track) {
        return trackid == track.m_id.get();
    });
    if (it == m_data.end())
        return;
    size_t pos = std::distance(m_data.begin(), it);
    it->m_selected = selected;
    QModelIndex dataIndex = index(pos);
    emit dataChanged(dataIndex, dataIndex, {  Qt::CheckStateRole });

    if (!selected && trackid == m_secondary)
        setSecondary(nullptr);
    updateHasSelection();
}

void TrackListModel::clear()
{
    beginRemoveRows({}, 0, m_data.size() - 1);
    m_data.clear();
    endRemoveRows();
    m_secondary = nullptr;
    m_removed.clear();
    emit countChanged();
    updateHasSelection();
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = QByteArrayLiteral("checked");
    roleNames[ExternalRole] = QByteArrayLiteral("external");
    roleNames[SecondaryRole] = QByteArrayLiteral("secondary");
    roleNames[StyledRole] = QByteArrayLiteral("styled");
    roleNames[LanguageRole] = QByteArrayLiteral("language");
    return roleNames;
}

void TrackListModel::setMultiSelect(bool multiSelect)
{
    if (m_multiSelect == multiSelect)
        return;

    m_multiSelect = multiSelect;
    if ( !m_multiSelect && getCount() > 1 )
    {
        int firstSelectedIndex = -1;
        for ( int i = 0; i < getCount(); i++ )
        {
            if ( data( index( i ), Qt::CheckStateRole ).toBool() )
            {
                if (firstSelectedIndex != -1)
                    setData( index(i), false, Qt::CheckStateRole );
                else
                    firstSelectedIndex = i;
            }
        }

        if ( firstSelectedIndex != -1 )
            setData( index(firstSelectedIndex), true, Qt::CheckStateRole );
    }
    emit multiSelectChanged(m_multiSelect);
}

TrackListModel::Data::Data(const vlc_player_track *track_info)
    : m_title( qfu(track_info->name) )
    , m_id( track_info->es_id, true )
    , m_selected( track_info->selected )
    , m_external( isExternalTrack(track_info->es_id) )
    , m_styled( track_info->fmt.i_codec == VLC_CODEC_SSA )
    , m_language( qfu(track_info->fmt.psz_language) )
{
}

void TrackListModel::Data::update(const vlc_player_track *track_info)
{
    m_id.reset(track_info->es_id, true);
    m_title = qfu(track_info->name);
    m_selected = track_info->selected;
    m_external = isExternalTrack(track_info->es_id);
    m_styled = track_info->fmt.i_codec == VLC_CODEC_SSA;
    m_language = qfu(track_info->fmt.psz_language);
}


//***************************
//  TitleListModel
//***************************


TitleListModel::TitleListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags TitleListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int TitleListModel::rowCount(const QModelIndex &) const
{
    return m_count;
}

QVariant TitleListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_count)
        return QVariant{};
    const vlc_player_title* title = getTitleAt(row);

    if (role == Qt::DisplayRole)
        return qfu(title->name);
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(row == m_current);
    return QVariant{};
}

bool TitleListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row < 0 || row >= m_count)
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    if (select)
    {
        vlc_player_locker lock{ m_player };
        const vlc_player_title* title = getTitleAt(row);
        if (!title)
            return false;
        vlc_player_SelectTitle(m_player, title);
    }
    return true;
}

void TitleListModel::setCurrent(int current)
{
    if (m_count == 0 || m_current == current)
        return;
    int oldCurrent = m_current;
    m_current = current;

    QModelIndex oldIndex = index(oldCurrent);
    QModelIndex currentIndex = index(current);

    if (oldCurrent >= 0)
        emit dataChanged(oldIndex, oldIndex, { Qt::CheckStateRole });
    if ( current >= 0 )
        emit dataChanged(currentIndex, currentIndex, { Qt::CheckStateRole });
}

const vlc_player_title *TitleListModel::getTitleAt(size_t index) const
{
    return vlc_player_title_list_GetAt(m_titleList.get(), index);
}

void TitleListModel::resetTitles(vlc_player_title_list *newTitleList)
{
    beginResetModel();
    m_titleList.reset(newTitleList, true);
    m_current = -1;
    if (m_titleList)
        m_count = vlc_player_title_list_GetCount(m_titleList.get());
    else
        m_count = 0;
    endResetModel();
    emit countChanged();
}

QHash<int, QByteArray> TitleListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = QByteArrayLiteral("checked");
    return roleNames;
}

//***************************
//  ChapterListModel
//***************************

ChapterListModel::ChapterListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags ChapterListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int ChapterListModel::rowCount(const QModelIndex &) const
{
    return m_title ? m_title->chapter_count : 0;
}

QVariant ChapterListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (m_title == nullptr || row < 0 || (size_t)row >= m_title->chapter_count)
        return QVariant{};
    const vlc_player_chapter& chapter = m_title->chapters[row];

    if (role == Qt::DisplayRole)
        return qfu(chapter.name);
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(row == m_current);
    else if (role == ChapterListRoles::TimeRole )
        return QVariant::fromValue<VLCTime>(chapter.time);
    else if (role == ChapterListRoles::StartPositionRole && (m_title->length != 0))
        return QVariant::fromValue<float>(chapter.time / (float) m_title->length);
    else if (role == ChapterListRoles::EndPositionRole && (m_title->length != 0))
        return (size_t)row + 1 == m_title->chapter_count
                ? QVariant::fromValue<float> (1)
                : QVariant::fromValue<float> (m_title->chapters[row+1].time / (float) m_title->length);

    return QVariant{};
}

bool ChapterListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (m_title == nullptr || (size_t)row >= m_title->chapter_count)
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    if (select)
    {
        vlc_player_locker lock{ m_player };
        vlc_player_SelectChapter(m_player, m_title, row);
    }
    return true;
}

QHash<int, QByteArray> ChapterListModel::roleNames() const
{
    return QHash<int, QByteArray>{
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {Qt::CheckStateRole, QByteArrayLiteral("checked")},
        {ChapterListRoles::TimeRole, QByteArrayLiteral("time")},
        {ChapterListRoles::StartPositionRole, QByteArrayLiteral("startPosition")},
        {ChapterListRoles::EndPositionRole, QByteArrayLiteral("endPosition")}
    };
}

void ChapterListModel::setCurrent(int current)
{
    if (m_title == nullptr || m_title->chapter_count == 0 || m_current == current)
        return;
    int oldCurrent = m_current;
    m_current = current;

    QModelIndex oldIndex = index(oldCurrent);
    QModelIndex currentIndex = index(current);

    if (oldCurrent >= 0)
        emit dataChanged(oldIndex, oldIndex, { Qt::CheckStateRole });
    if ( current >= 0 )
        emit dataChanged(currentIndex, currentIndex, { Qt::CheckStateRole });
}

void ChapterListModel::resetTitle(const vlc_player_title *newTitle)
{
    beginResetModel();
    m_title =newTitle;
    m_current = -1;
    endResetModel();
    emit countChanged();
}

void ChapterListModel::selectChapter(int index)
{
    if (!m_title || index < 0 || static_cast<size_t>(index) >= m_title->chapter_count)
        return;
    {
        vlc_player_locker lock{ m_player };
        vlc_player_SelectChapter(m_player, m_title, index);
    }
}

QString ChapterListModel::getNameAtPosition(float pos) const
{
    if(m_title != nullptr){

        vlc_tick_t posTime = pos * m_title->length;
        int prevChapterIndex = 0;

        for(unsigned int i=0;i<m_title->chapter_count;i++){

            vlc_tick_t currentChapterTime = m_title->chapters[i].time;

            if(currentChapterTime > posTime)
                return qfu(m_title->chapters[prevChapterIndex].name);

            else if(i == (m_title->chapter_count - 1))
                return qfu(m_title->chapters[i].name);

            prevChapterIndex = i;
        }
    }
    return QString();
}

int ChapterListModel::getClosestChapterFromPos(float pos, float threshold) const
{
    if(m_title == nullptr)
        return -1;

    vlc_tick_t posTime = pos * m_title->length;
    vlc_tick_t closestTime = VLC_TICK_INVALID;
    int chapterIndex = -1;

    for(size_t i=0; i<m_title->chapter_count; i++){

        vlc_tick_t currentChapterTime = m_title->chapters[i].time;
        if (currentChapterTime == VLC_TICK_INVALID)
            continue;
        if (closestTime == VLC_TICK_INVALID
            || qAbs(currentChapterTime - posTime) < qAbs(closestTime - posTime))
        {
            closestTime = currentChapterTime;
            chapterIndex = i;
        }
        else
            break;
    }

    if (closestTime == VLC_TICK_INVALID)
        return -1;
    else
    {
        float chapterPos = closestTime / (float)m_title->length;

        if (qAbs(chapterPos - pos) < threshold)
            return chapterIndex;
        return -1;
    }

}


//***************************
//  ProgramListModel
//***************************


ProgramListModel::ProgramListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags ProgramListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int ProgramListModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QVariant ProgramListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_data.size())
        return QVariant{};
    if (role == Qt::DisplayRole)
        return m_data[row].m_title;
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(m_data[row].m_selected);
    return QVariant{};
}

bool ProgramListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row >= m_data.size())
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    vlc_player_locker lock{ m_player };

    if (select)
        vlc_player_SelectProgram(m_player, m_data[row].m_id);
    return true;
}

void ProgramListModel::updatePrograms(vlc_player_list_action action, const vlc_player_program *program)
{
    assert(program);
    switch (action) {
    case VLC_PLAYER_LIST_ADDED:
        beginInsertRows({}, m_data.size(), m_data.size());
        m_data.append(Data{ program });
        endInsertRows();
        emit countChanged();
        break;

    case VLC_PLAYER_LIST_REMOVED:
    {
        auto it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& t) {
            return t.m_id == program->group_id;
        });
        if (it == m_data.end())
            return;
        int pos = std::distance(m_data.begin(), it);
        beginRemoveRows({}, pos, pos);
        m_data.erase(it);
        endRemoveRows();
        emit countChanged();
        break;
    }
    case VLC_PLAYER_LIST_UPDATED:
    {
        int pos = 0;
        bool found = false;
        for (Data& d : m_data)
        {
            if (d.m_id == program->group_id)
            {
                d.update(program);
                found = true;
                break;
            }
            pos++;
        }
        if (!found)
            return;
        QModelIndex dataIndex = index(pos);
        emit dataChanged(dataIndex, dataIndex, { Qt::DisplayRole, Qt::CheckStateRole });
        break;
    }
    }
}

void ProgramListModel::updateProgramSelection(int programid, bool selected)
{
    QList<Data>::iterator it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& program) {
        return programid == program.m_id;
    });
    if (it == m_data.end())
        return;
    size_t pos = std::distance(m_data.begin(), it);
    it->m_selected = selected;
    QModelIndex dataIndex = index(pos);
    emit dataChanged(dataIndex, dataIndex, {  Qt::CheckStateRole });
}

void ProgramListModel::clear()
{
    beginRemoveRows({}, 0, m_data.size() - 1);
    m_data.clear();
    endRemoveRows();
}

QHash<int, QByteArray> ProgramListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = QByteArrayLiteral("checked");
    return roleNames;
}

ProgramListModel::Data::Data(const vlc_player_program *program)
    : m_title(qfu(program->name))
    , m_id( program->group_id )
    , m_selected( program->selected )
    , m_scrambled( program->scrambled )
{
    assert(program);
}

void ProgramListModel::Data::update(const vlc_player_program *program)
{
    assert(program);
    m_title = qfu(program->name);
    m_id = program->group_id;
    m_selected = program->selected;
    m_scrambled = program->scrambled;
}

