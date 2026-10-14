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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "playlist_model.hpp"
#include "playlist_model_p.hpp"
#include <algorithm>
#include <cassert>
#include "util/shared_input_item.hpp"
#include "playlist_controller.hpp"

namespace vlc {
namespace playlist {

namespace {

static QVector<PlaylistItem> toVec(vlc_playlist_item_t *const items[],
                                   size_t len)
{
    QVector<PlaylistItem> vec;
    for (size_t i = 0; i < len; ++i)
        vec.push_back(items[i]);
    return vec;
}

template <typename RAW, typename WRAPPER>
static QVector<RAW> toRaw(const QVector<WRAPPER> &items)
{
    QVector<RAW> vec;
    int count = items.size();
    vec.reserve(count);
    for (int i = 0; i < count; ++i)
        vec.push_back(items[i].raw());
    return vec;
}
}

extern "C" { // for C callbacks

static void
on_playlist_items_reset(vlc_playlist_t *playlist,
                        vlc_playlist_item_t *const items[],
                        size_t len, void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    QVector<PlaylistItem> newContent = toVec(items, len);
    that->callAsync([=]() {
        if (that->m_playlist != playlist)
            return;
        that->onItemsReset(std::move(newContent));
    });
}

static void
on_playlist_items_added(vlc_playlist_t *playlist, size_t index,
                        vlc_playlist_item_t *const items[], size_t len,
                        void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    QVector<PlaylistItem> added = toVec(items, len);
    that->callAsync([=]() {
        if (that->m_playlist != playlist)
            return;
        that->onItemsAdded(std::move(added), index);
    });
}

static void
on_playlist_items_moved(vlc_playlist_t *playlist, size_t index, size_t count,
                        size_t target, void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    that->callAsync([=]() {
        if (that->m_playlist != playlist)
            return;
        that->onItemsMoved(index, count, target);
    });
}

static void
on_playlist_items_removed(vlc_playlist_t *playlist, size_t index, size_t count,
                          void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        that->onItemsRemoved(index, count);
    });
}

static void
on_playlist_items_updated(vlc_playlist_t *playlist, size_t index,
                          vlc_playlist_item_t *const items[], size_t len,
                          void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    QVector<PlaylistItem> updated = toVec(items, len);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        int count = updated.size();
        for (int i = 0; i < count; ++i)
            that->m_items[index + i] = updated[i]; /* sync metadata */
        that->notifyItemsChanged(index, count);
        that->updateRemaining();
    });
}

static void
on_playlist_current_item_changed(vlc_playlist_t *playlist, ssize_t index,
                                 void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        that->onCurrentIndexChanged(index);
    });
}

} // extern "C"

static const struct vlc_playlist_callbacks playlist_callbacks = []{
    struct vlc_playlist_callbacks cbs {};
    cbs.on_items_reset = on_playlist_items_reset;
    cbs.on_items_added = on_playlist_items_added;
    cbs.on_items_moved = on_playlist_items_moved;
    cbs.on_items_removed = on_playlist_items_removed;
    cbs.on_items_updated = on_playlist_items_updated;
    cbs.on_current_index_changed = on_playlist_current_item_changed;
    return cbs;
}();

// private API

PlaylistListModelPrivate::PlaylistListModelPrivate(PlaylistListModel* playlistListModel)
    : q_ptr(playlistListModel)
{
}

PlaylistListModelPrivate::~PlaylistListModelPrivate()
{
    if (m_playlist && m_listener)
    {
        vlc_playlist_locker locker(m_playlist);
        vlc_playlist_RemoveListener(m_playlist, m_listener);
    }
}

void PlaylistListModelPrivate::onItemsReset(const QVector<PlaylistItem>&& newContent)
{
    Q_Q(PlaylistListModel);
    q->beginResetModel();
    m_items = newContent;
    q->endResetModel();

    m_duration = VLCDuration();
    if (m_items.size())
    {
        for(const auto& i : m_items)
        {
            m_duration += i.getDuration();
        }
    }

    emit q->countChanged(m_items.size());
    updateRemaining();
}

void PlaylistListModelPrivate::onItemsAdded(const QVector<PlaylistItem>&& added, size_t index)
{
    Q_Q(PlaylistListModel);
    int count = added.size();
    q->beginInsertRows({}, index, index + count - 1);
    m_items.insert(index, count, nullptr);
    std::move(added.cbegin(), added.cend(), m_items.begin() + index);
    q->endInsertRows();

    for(const auto& i : added)
    {
        m_duration += i.getDuration();
    }

    emit q->countChanged(m_items.size());
    updateRemaining();
}

void PlaylistListModelPrivate::onItemsMoved(size_t index, size_t count, size_t target)
{
    Q_Q(PlaylistListModel);
    size_t qtTarget = target;
    if (qtTarget > index)
        /* Qt interprets the target index as the index of the insertion _before_
       * the move, while the playlist core interprets it as the new index of
       * the slice _after_ the move. */
        qtTarget += count;

    q->beginMoveRows({}, index, index + count - 1, {}, qtTarget);
    if (index < target)
        std::rotate(m_items.begin() + index,
                    m_items.begin() + index + count,
                    m_items.begin() + target + count);
    else
        std::rotate(m_items.begin() + target,
                    m_items.begin() + index,
                    m_items.begin() + index + count);
    q->endMoveRows();

    updateRemaining();
}

void PlaylistListModelPrivate::onItemsRemoved(size_t index, size_t count)
{
    Q_Q(PlaylistListModel);

    for(size_t i = index; i <= index + count - 1; ++i)
    {
        m_duration -= m_items.at(i).getDuration();
    }

    q->beginRemoveRows({}, index, index + count - 1);
    m_items.remove(index, count);
    q->endRemoveRows();

    //maybe we removed the only valid elements
    if (m_duration.toVLCTick() == 0 && m_duration.valid())
    {
        m_duration = VLCDuration();
        for(const auto& i : m_items)
            m_duration += i.getDuration();
    }

    emit q->countChanged(m_items.size());
    updateRemaining();
}


void
PlaylistListModelPrivate::notifyItemsChanged(int idx, int count, const QVector<int> &roles)
{
    Q_Q(PlaylistListModel);
    QModelIndex first = q->index(idx, 0);
    QModelIndex last = q->index(idx + count - 1);
    emit q->dataChanged(first, last, roles);
}

void PlaylistListModelPrivate::onCurrentIndexChanged(ssize_t index)
{
    Q_Q(PlaylistListModel);
    ssize_t oldCurrent = m_current;
    m_current = index;
    if (oldCurrent != -1)
        notifyItemsChanged(oldCurrent, 1, {PlaylistListModel::IsCurrentRole});
    if (index != -1)
        notifyItemsChanged(index, 1, {PlaylistListModel::IsCurrentRole});
    /* moves can change which rows come before the current one, so refresh
     * them all */
    if (!m_items.isEmpty())
        notifyItemsChanged(0, m_items.size(), {PlaylistListModel::IsPlayedRole});

    if (oldCurrent != index)
        m_currentTime = VLCTime{};
    updateRemaining();

    emit q->currentIndexChanged(index);
}

void PlaylistListModelPrivate::updateRemaining()
{
    Q_Q(PlaylistListModel);
    m_remainingItemsDuration = VLCDuration{};
    for (qsizetype i = std::max<qsizetype>(m_current, 0); i < m_items.size(); ++i)
        m_remainingItemsDuration += m_items.at(i).getDuration();
    emit q->remainingChanged();
}

// public API

PlaylistListModel::PlaylistListModel(QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(new PlaylistListModelPrivate(this))
{
}

PlaylistListModel::PlaylistListModel(vlc_playlist_t *raw_playlist, QObject *parent)
    : QAbstractListModel(parent)
    , d_ptr(new PlaylistListModelPrivate(this))
{
    setPlaylist(Playlist(raw_playlist));
}

PlaylistListModel::~PlaylistListModel()
{
}

QHash<int, QByteArray>
PlaylistListModel::roleNames() const
{
    return {
        { FilterRole, QByteArrayLiteral("filter") },
        { PreparsedRole, QByteArrayLiteral("preparsed") },
        { TitleRole, QByteArrayLiteral("title") },
        { DurationRole, QByteArrayLiteral("duration") },
        { IsCurrentRole, QByteArrayLiteral("isCurrent") },
        { ArtistRole , QByteArrayLiteral("artist") },
        { AlbumRole  , QByteArrayLiteral("album") },
        { ArtworkRole, QByteArrayLiteral("artwork") },
        { UrlRole, QByteArrayLiteral("url") },
        { IsPlayedRole, QByteArrayLiteral("isPlayed") }
    };
}

int
PlaylistListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const PlaylistListModel);
    if (! d->m_playlist)
        return 0;
    return d->m_items.size();
}

VLCDuration
PlaylistListModel::getDuration() const
{
    Q_D(const PlaylistListModel);
    return d->m_duration;
}

PlaylistItem
PlaylistListModel::itemAt(int index) const
{
    Q_D(const PlaylistListModel);
    return d->m_items[index];
}

void PlaylistListModel::removeItems(const QVector<int>& indexes)
{
    Q_D(PlaylistListModel);
    if (!d->m_playlist)
        return;
    if (indexes.size() == 0)
        return;
    QVector<vlc_playlist_item_t *> itemsToRemove;
    std::transform(indexes.begin(), indexes.end(),std::back_inserter(itemsToRemove), [&] (int index) {
        return d->m_items[index].raw();
    });

    {
        vlc_playlist_locker locker(d->m_playlist);
        int ret = vlc_playlist_RequestRemove(d->m_playlist, itemsToRemove.constData(),
                                             itemsToRemove.size(), indexes[0]);
        if (ret != VLC_SUCCESS)
            throw std::bad_alloc();
    }
}

/**
 * Return the target position *after* the move has been applied, knowing the
 * index *before* the move and the list of indexes to move.
 *
 * The core playlist interprets the move target as the index of the (first)
 * item *after* the move. During a drag&drop, we know the index *before* the
 * move.
 */
static int
getMovePostTarget(const QVector<int> &sortedIndexesToMove, int preTarget)
{
    int postTarget = preTarget;
    for (int index : sortedIndexesToMove)
    {
        if (index >= preTarget)
            break;
        postTarget--;
    }
    return postTarget;
}

void
PlaylistListModel::moveItems(const QVector<int> &sortedIndexes, int target,
                             bool isPreTarget)
{
    Q_D(PlaylistListModel);
    if (!d->m_playlist)
        return;
    if (sortedIndexes.size() == 0)
        return;
    QVector<vlc_playlist_item_t*> itemsToMove;
    std::transform(sortedIndexes.begin(), sortedIndexes.end(),
                   std::back_inserter(itemsToMove),
                   [&] (int index) {
                       return d->m_items[index].raw();
                   });

    if (isPreTarget) {
        /* convert pre-target to post-target */
        target = getMovePostTarget(sortedIndexes, target);
    }

    vlc_playlist_locker locker(d->m_playlist);
    int ret = vlc_playlist_RequestMove(d->m_playlist, itemsToMove.constData(),
                                       itemsToMove.size(), target,
                                       sortedIndexes[0]);
    if (ret != VLC_SUCCESS)
        throw std::bad_alloc();
}

void
PlaylistListModel::moveItemsPre(const QVector<int> &sortedIndexes, int preTarget)
{
    return moveItems(sortedIndexes, preTarget, true);
}

void
PlaylistListModel::moveItemsPost(const QVector<int> &sortedIndexes,
                                 int postTarget)
{
    return moveItems(sortedIndexes, postTarget, false);
}

void
PlaylistListModel::shiftItems(const QVector<int> &sortedIndexes, int offset)
{
    Q_D(PlaylistListModel);
    if (sortedIndexes.isEmpty() || offset == 0)
        return;

    const int count = d->m_items.size();
    const int last = count - sortedIndexes.size();
    if (last < 0)
        return;
    const int target = std::clamp(sortedIndexes.first() + offset, 0, last);

    /* already there, and together */
    if (target == sortedIndexes.first()
        && sortedIndexes.last() - sortedIndexes.first() == sortedIndexes.size() - 1)
        return;

    moveItems(sortedIndexes, target, false);
}

void
PlaylistListModel::moveItemsAfterCurrent(const QVector<int> &sortedIndexes)
{
    Q_D(PlaylistListModel);
    QVector<int> indexes = sortedIndexes;
    /* the current item stays where it is */
    indexes.removeAll(d->m_current);
    if (indexes.isEmpty())
        return;

    const int preTarget = (d->m_current >= 0) ? d->m_current + 1 : 0;
    moveItems(indexes, preTarget, true);
}

int PlaylistListModel::getCurrentIndex() const
{
    Q_D(const PlaylistListModel);
    return d->m_current;
}

int PlaylistListModel::getRemainingCount() const
{
    Q_D(const PlaylistListModel);
    return d->m_items.size() - std::max<qsizetype>(d->m_current, 0);
}

VLCDuration PlaylistListModel::getRemainingDuration() const
{
    Q_D(const PlaylistListModel);
    if (d->m_current < 0 || d->m_current >= d->m_items.size()
        || !d->m_currentTime.valid())
        return d->m_remainingItemsDuration;

    const VLCDuration currentDuration = d->m_items.at(d->m_current).getDuration();
    if (!currentDuration.valid())
        return d->m_remainingItemsDuration;

    const vlc_tick_t played = std::clamp<vlc_tick_t>(d->m_currentTime.toVLCTick(),
                                                     0, currentDuration.toVLCTick());
    return VLCDuration(d->m_remainingItemsDuration.toVLCTick() - played);
}

VLCTime PlaylistListModel::getCurrentTime() const
{
    Q_D(const PlaylistListModel);
    return d->m_currentTime;
}

void PlaylistListModel::setCurrentTime(VLCTime time)
{
    Q_D(PlaylistListModel);
    if (time.toVLCTick() == d->m_currentTime.toVLCTick())
        return;
    d->m_currentTime = time;
    emit remainingChanged();
}

/* Q_INVOKABLE */
QVariantList PlaylistListModel::getItemsForIndexes(const QVector<int> & indexes) const
{
    Q_D(const PlaylistListModel);

    QVariantList items;

    for (int index : indexes)
    {
        if (index < 0 || index >= d->m_items.count())
            continue;

        vlc_playlist_item_t * item = d->m_items[index].raw();

        if (item == nullptr)
            continue;

        input_item_t * media = vlc_playlist_item_GetMedia(item);

        if (media == nullptr)
            continue;

        items.append(QVariant::fromValue(SharedInputItem(media, true)));
    }

    return items;
}

Playlist PlaylistListModel::getPlaylist() const
{
    Q_D(const PlaylistListModel);
    if (!d->m_playlist)
        return {};
    return Playlist(d->m_playlist);
}

void PlaylistListModel::setPlaylist(vlc_playlist_t* playlist)
{
    Q_D(PlaylistListModel);
    if (d->m_playlist && d->m_listener)
    {
        vlc_playlist_locker locker(d->m_playlist);
        vlc_playlist_RemoveListener(d->m_playlist, d->m_listener);
        d->m_playlist = nullptr;
        d->m_listener = nullptr;
    }
    if (playlist)
    {
        vlc_playlist_locker locker(playlist);
        d->m_playlist = playlist;
        d->m_listener = vlc_playlist_AddListener(d->m_playlist, &playlist_callbacks, d, true);
    }
    emit playlistChanged( Playlist(d->m_playlist) );
}

void PlaylistListModel::setPlaylist(const Playlist& playlist)
{
    setPlaylist(playlist.m_playlist);
}

QVariant
PlaylistListModel::data(const QModelIndex &index, int role) const
{
    Q_D(const PlaylistListModel);
    if (!d->m_playlist)
        return {};

    ssize_t row = index.row();
    if (row < 0 || row >= d->m_items.size())
        return {};

    switch (role)
    {
    case FilterRole:
        return QVariant::fromValue<QString>(data(index, TitleRole).toString() +
                                            QStringLiteral("\n") +
                                            data(index, ArtistRole).toString() +
                                            QStringLiteral("\n") +
                                            data(index, AlbumRole).toString());
    case TitleRole:
        return d->m_items[row].getTitle();
    case IsCurrentRole:
        return row == d->m_current;
    case IsPlayedRole:
        return d->m_current >= 0 && row < d->m_current;
    case DurationRole:
        return QVariant::fromValue<VLCDuration>(d->m_items[row].getDuration());
    case ArtistRole:
        return d->m_items[row].getArtist();
    case AlbumRole:
        return d->m_items[row].getAlbum();
    case ArtworkRole:
        return d->m_items[row].getArtwork();
    case UrlRole:
        return d->m_items[row].getUrl();
    case PlaylistListModel::PreparsedRole:
        return d->m_items[row].preparsed();
    default:
        return {};
    }
}

} // namespace playlist
} // namespace vlc
