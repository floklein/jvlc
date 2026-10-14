/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#include "mlrecentmediamodel.hpp"

#include "medialib.hpp"

MLRecentMediaModel::MLRecentMediaModel( QObject* parent )
    : MLMediaModel( parent )
{
    connect( this, &MLBaseModel::mlChanged, this, &MLRecentMediaModel::onMlChanged );
}

void MLRecentMediaModel::onMlChanged()
{
    disconnect( m_hideListConnection );
    if ( m_mediaLib )
        m_hideListConnection = connect( m_mediaLib->recentHideList(), &MLRecentHideList::changed,
                                        this, &MLRecentMediaModel::resetRequested );
}

QVariant MLRecentMediaModel::itemRoleData(const MLItem *item , int role ) const
{
    const MLMedia* media = static_cast<const MLMedia *>(item);
    if ( !media )
        return QVariant();

    switch (role)
    {
    case Qt::DisplayRole:
    case RECENT_MEDIA_URL:
        return QVariant::fromValue( QUrl::fromEncoded( media->mrl().toUtf8() ).toString( QUrl::PreferLocalFile | QUrl::RemovePassword ) );
    case RECENT_GROUP:
    {
        // "This week" is the last 7 days
        const qint64 days = media->lastPlayedDate().date().daysTo( QDate::currentDate() );
        if ( days <= 0 )
            return qtr( "Today" );
        if ( days == 1 )
            return qtr( "Yesterday" );
        if ( days < 7 )
            return qtr( "This week" );
        return qtr( "Earlier" );
    }
    default :
        return MLMediaModel::itemRoleData(item, role);
    }
}

QHash<int, QByteArray> MLRecentMediaModel::roleNames() const
{
    QHash<int, QByteArray> hash = MLMediaModel::roleNames();

    hash.insert({
        { RECENT_MEDIA_URL, QByteArrayLiteral("url") },
        { RECENT_GROUP, QByteArrayLiteral("recent_group") },
    });

    return hash;
}

void MLRecentMediaModel::setHistoryType( HistoryType type )
{
    if ( m_historyType == type )
        return;
    m_historyType = type;
    emit historyTypeChanged();
    emit resetRequested();
}

void MLRecentMediaModel::clearRecent()
{
    // never vlc_ml_clear_history(): it also resets play counts and progress
    assert( m_mediaLib );
    m_mediaLib->recentHideList()->clear( QDateTime::currentSecsSinceEpoch() );
}

void MLRecentMediaModel::removeFromRecent(const QModelIndexList &indexes)
{
    assert( m_mediaLib );
    MLRecentHideList* hideList = m_mediaLib->recentHideList();

    for ( const QModelIndex &index : indexes )
    {
        const auto media = static_cast<const MLMedia *>( item( index.row() ) );
        if ( media )
            hideList->hide( media->getId().id, media->lastPlayedDate().toSecsSinceEpoch() );
    }
}

void MLRecentMediaModel::onVlcMlEvent( const MLEvent &event )
{
    switch ( event.i_type )
    {
        case VLC_ML_EVENT_HISTORY_CHANGED:
        {
            emit resetRequested();
            return;
        }
        default:
            break;
    }
    MLBaseModel::onVlcMlEvent( event );
}

std::unique_ptr<MLListCacheLoader>
MLRecentMediaModel::createMLLoader() const
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLRecentMediaModel::Loader>(*this));
}

MLRecentMediaModel::Loader::Loader(const MLRecentMediaModel& model)
    : MLOp(model)
    , m_historyType(static_cast<vlc_ml_history_type_t>(model.m_historyType))
{
    if (model.m_mediaLib)
        m_hideList = model.m_mediaLib->recentHideList()->source();
}

MLRecentHideList::Filter MLRecentMediaModel::Loader::filter() const
{
    return m_hideList ? m_hideList->get() : MLRecentHideList::Filter{};
}

size_t MLRecentMediaModel::Loader::count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    const MLRecentHideList::Filter hidden = filter();
    if ( hidden.isEmpty() )
        return vlc_ml_count_history( ml, queryParams, m_historyType );

    ml_unique_ptr<vlc_ml_media_list_t> media_list = listAll( ml, queryParams );
    if ( media_list == nullptr )
        return 0;

    size_t count = 0;
    for( const vlc_ml_media_t &media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        if ( !hidden.isHidden( media.i_id, media.i_last_played_date ) )
            ++count;
    return count;
}

std::vector<std::unique_ptr<MLItem>>
MLRecentMediaModel::Loader::load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    const MLRecentHideList::Filter hidden = filter();
    if ( !hidden.isEmpty() )
        return loadVisible( ml, queryParams, hidden );

    const vlc_ml_query_params_t params = newestFirst( queryParams );
    ml_unique_ptr<vlc_ml_media_list_t> media_list{ vlc_ml_list_history(
                ml, &params, m_historyType ) };
    if ( media_list == nullptr )
        return {};

    std::vector<std::unique_ptr<MLItem>> res;

    for( vlc_ml_media_t &media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        if ( media.i_type != VLC_ML_MEDIA_TYPE_AUDIO )
            res.emplace_back( std::make_unique<MLVideo>( &media ) );
        else if ( media.i_type == VLC_ML_MEDIA_TYPE_AUDIO )
            res.emplace_back( std::make_unique<MLAudio>( ml, &media ) );
        else
            Q_UNREACHABLE();

    return res;
}

ml_unique_ptr<vlc_ml_media_list_t>
MLRecentMediaModel::Loader::listAll(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const
{
    vlc_ml_query_params_t params = newestFirst( queryParams );
    params.i_offset = 0;
    params.i_nbResults = 0;

    return ml_unique_ptr<vlc_ml_media_list_t>{ vlc_ml_list_history( ml, &params, m_historyType ) };
}

vlc_ml_query_params_t MLRecentMediaModel::Loader::newestFirst(const vlc_ml_query_params_t* queryParams)
{
    // the history is sorted by last played date, oldest first unless descending
    vlc_ml_query_params_t params = queryParams ? *queryParams : vlc_ml_query_params_create();
    params.i_sort = VLC_ML_SORTING_DEFAULT;
    params.b_desc = true;
    return params;
}

std::vector<std::unique_ptr<MLItem>>
MLRecentMediaModel::Loader::loadVisible(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams,
                                        const MLRecentHideList::Filter& hidden) const
{
    // the media library cannot skip the hidden media: page through the
    // visible ones here
    ml_unique_ptr<vlc_ml_media_list_t> media_list = listAll( ml, queryParams );
    if ( media_list == nullptr )
        return {};

    size_t skip = queryParams ? queryParams->i_offset : 0;
    const size_t limit = queryParams ? queryParams->i_nbResults : 0;

    std::vector<std::unique_ptr<MLItem>> res;

    for( vlc_ml_media_t &media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
    {
        if ( hidden.isHidden( media.i_id, media.i_last_played_date ) )
            continue;

        if ( skip > 0 )
        {
            --skip;
            continue;
        }

        if ( limit > 0 && res.size() >= limit )
            break;

        if ( media.i_type != VLC_ML_MEDIA_TYPE_AUDIO )
            res.emplace_back( std::make_unique<MLVideo>( &media ) );
        else
            res.emplace_back( std::make_unique<MLAudio>( ml, &media ) );
    }

    return res;
}

// TODO: can we keep this as virtual and not pure virtual?
//       it's same in models derived from another models.
std::unique_ptr<MLItem>
MLRecentMediaModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));

    if (media == nullptr)
        return nullptr;

    if (media->i_type != VLC_ML_MEDIA_TYPE_AUDIO)
        return std::make_unique<MLVideo>(media.get());
    else if (media->i_type == VLC_ML_MEDIA_TYPE_AUDIO)
        return std::make_unique<MLAudio>(ml, media.get());
    else
        Q_UNREACHABLE();
    return nullptr;
}
