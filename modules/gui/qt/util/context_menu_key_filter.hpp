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
#ifndef CONTEXT_MENU_KEY_FILTER_HPP
#define CONTEXT_MENU_KEY_FILTER_HPP

#include <QObject>

/**
 * Delivers Shift+F10 as the Menu key, so that both open the context menu
 * wherever the Menu key does (the player, library items, the Queue).
 */
class ContextMenuKeyFilter : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

protected:
    bool eventFilter( QObject *object, QEvent *event ) override;
};

#endif // CONTEXT_MENU_KEY_FILTER_HPP
