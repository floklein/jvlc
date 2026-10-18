/*****************************************************************************
 * menus.hpp : Menus handling
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_MENUS_H_
#define QVLC_MENUS_H_

#include "qt.hpp"

#include "custom_menus.hpp"
#include "widgets/native/qvlcframe.hpp"

#include <QObject>

#include <functional>

class VLCMenu : public QMenu
{
    Q_OBJECT

public:
    explicit VLCMenu(qt_intf_t* p_intf) : QMenu()
    {
        assert(p_intf);
        if (isWindow())
        {
            QVLCDialog::setWindowTransientParent(this, nullptr, p_intf);
        }
    }

    explicit VLCMenu(const QString& title, qt_intf_t* p_intf) : VLCMenu(p_intf)
    {
        setTitle(title);
    }

    explicit VLCMenu(const QString &title, QWidget *parent) : QMenu(title, parent)
    {
        assert(parent); // use VLCMenu(qt_intf_t* p_intf) if parent is null pointer
    }
};

class VLCMenuBar : public QObject
{
    Q_OBJECT
    friend class MenuFunc;

public:
    VLCMenuBar(QObject* parent = nullptr);

    /* Popups Menus */
    static QMenu* PopupMenu( qt_intf_t *, bool );
    static QMenu* AudioPopupMenu( qt_intf_t *, bool );
    static QMenu* VideoPopupMenu( qt_intf_t *, bool );
    static QMenu* MiscPopupMenu( qt_intf_t *, bool );


    /* The VLC menu: opening media, media tools, customization and the app */
    static void VLCMenuEntries( qt_intf_t *, QMenu * );

    /* The Control bar's controls folded in a narrow window (ControlListModel
     * ids), at the top of the VLC menu: trigger runs the picked one */
    static void FoldedEntries( qt_intf_t *, QMenu *, const QList<int> &folded,
                               const std::function<void(int)> &trigger );

    static void OpenEntries( qt_intf_t *, QMenu *, bool icons );

    static void ToolsMenu( qt_intf_t *, QMenu * );

    /* The Profiles, the active one checked, and "New" */
    static void ProfileMenu( qt_intf_t *, QMenu * );

    static void ExtensionsMenu( qt_intf_t *p_intf, QMenu * );

    /* Submenus of the right-click menu */
    static void PlaybackMenu( qt_intf_t *, QMenu * );

    static void VideoMenu( qt_intf_t *, QMenu * );

    static void AudioMenu( qt_intf_t *, QMenu * );

    static void SubtitleMenu( qt_intf_t *, QMenu * );

    static void QueueMenu( qt_intf_t *, QMenu * );

    static void RepeatAndShuffleEntries( qt_intf_t *, QMenu * );
    static QMenu *SleepTimerMenu( qt_intf_t *, QMenu * );

    /* Entries of the right-click menu, also used by the systray menu */
    static void PopupMenuStaticEntries( QMenu *menu, qt_intf_t *p_intf );
    static void PopupMenuPlaylistEntries( QMenu *menu, qt_intf_t *p_intf, bool icons = false );
    static void PopupMenuControlEntries( QMenu *menu, qt_intf_t *p_intf, bool b = true );

    static void VolumeEntries( qt_intf_t *p_intf, QMenu *current );
};

#endif
