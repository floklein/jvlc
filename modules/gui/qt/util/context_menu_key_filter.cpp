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

#include "context_menu_key_filter.hpp"

#include <QKeyEvent>
#include <QCoreApplication>

bool ContextMenuKeyFilter::eventFilter( QObject *object, QEvent *event )
{
    if( event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease )
        return QObject::eventFilter( object, event );

    const auto keyEvent = static_cast<QKeyEvent *>( event );
    if( keyEvent->key() != Qt::Key_F10 || keyEvent->modifiers() != Qt::ShiftModifier )
        return QObject::eventFilter( object, event );

    QKeyEvent menuKey( event->type(), Qt::Key_Menu, Qt::NoModifier, QString(),
                       keyEvent->isAutoRepeat(), keyEvent->count() );
    QCoreApplication::sendEvent( object, &menuKey );
    event->setAccepted( menuKey.isAccepted() );
    return true;
}
