/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "app_shortcuts_model.hpp"

#include <vlc_actions.h>
#include <vlc_configuration.h>

AppShortcutsModel::AppShortcutsModel(QObject* parent)
    : ShortcutsModel(parent)
{
}

void AppShortcutsModel::setCtx(MainCtx* ctx)
{
    if (m_ctx || !ctx)
        return;
    m_ctx = ctx;

    qt_intf_t* p_intf = ctx->getIntf();
    vlc_object_t* libvlc = VLC_OBJECT(vlc_object_instance(p_intf));

    setCommitFunction([ctx, p_intf, libvlc]() {
        config_SaveConfigFile(p_intf);
        // the core key map and the interface's shortcuts follow at once;
        // global keys are read when VLC starts
        vlc_actions_reload(libvlc);
        ctx->notifyKeyBindingsChanged();
    });
    load(libvlc);

    emit ctxChanged();
}
