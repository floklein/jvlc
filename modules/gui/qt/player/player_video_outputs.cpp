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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "player_video_outputs.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_variables.h>
#include <vlc_vout.h>

#include "player/player_controller.hpp"

PlayerVideoOutputs::PlayerVideoOutputs( PlayerController* player )
    : m_player( player )
{
}

QString PlayerVideoOutputs::filterChain() const
{
    const SharedVOutThread vout = m_player->getVout();
    if ( !vout )
        return {};
    char* chain = var_GetString( vout.get(), "video-filter" );
    const QString result = QString::fromUtf8( chain ? chain : "" );
    free( chain );
    return result;
}

void PlayerVideoOutputs::setFilterChain( const QString& chain )
{
    const QByteArray value = chain.toUtf8();
    for ( const SharedVOutThread& vout : m_player->getVouts() )
        var_SetString( vout.get(), "video-filter", value.constData() );
}

void PlayerVideoOutputs::setOption( const char* name, const QVariant& value )
{
    int type;
    vlc_value_t val;
    QByteArray string;
    switch ( value.typeId() )
    {
    case QMetaType::Bool:
        type = VLC_VAR_BOOL;
        val.b_bool = value.toBool();
        break;
    case QMetaType::Double:
    case QMetaType::Float:
        type = VLC_VAR_FLOAT;
        val.f_float = value.toFloat();
        break;
    case QMetaType::QString:
        type = VLC_VAR_STRING;
        string = value.toString().toUtf8();
        val.psz_string = string.data();
        break;
    default:
        type = VLC_VAR_INTEGER;
        val.i_int = value.toLongLong();
        break;
    }

    for ( const SharedVOutThread& vout : m_player->getVouts() )
    {
        /* the variable a filter would map on its output when it starts: the
         * filter inherits it, and follows it once running */
        if ( var_Type( vout.get(), name ) == 0 )
            var_Create( vout.get(), name, type | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
        var_SetChecked( vout.get(), name, type, val );
    }
}

bool PlayerVideoOutputs::hasPreference( const char* name ) const
{
    return config_FindConfig( name ) != nullptr;
}

QVariant PlayerVideoOutputs::preference( const char* name ) const
{
    switch ( config_GetType( name ) )
    {
    case VLC_VAR_STRING:
    {
        char* value = config_GetPsz( name );
        const QString result = QString::fromUtf8( value ? value : "" );
        free( value );
        return result;
    }
    case VLC_VAR_INTEGER:
    case VLC_VAR_BOOL:
        return QVariant::fromValue( static_cast<int>( config_GetInt( name ) ) );
    case VLC_VAR_FLOAT:
        return QVariant::fromValue( static_cast<double>( config_GetFloat( name ) ) );
    default:
        return {};
    }
}

void PlayerVideoOutputs::setPreference( const char* name, const QVariant& value )
{
    switch ( config_GetType( name ) )
    {
    case VLC_VAR_STRING:
        config_PutPsz( name, value.toString().toUtf8().constData() );
        break;
    case VLC_VAR_INTEGER:
    case VLC_VAR_BOOL:
        config_PutInt( name, value.toLongLong() );
        break;
    case VLC_VAR_FLOAT:
        config_PutFloat( name, value.toFloat() );
        break;
    default:
        break;
    }
}
