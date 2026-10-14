/*****************************************************************************
 * Copyright (C) 2025 the VideoLAN team
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

#include "ui_notifier.hpp"

#include "maininterface/mainctx.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlplaylistmanager.hpp"

#include <QFileInfo>

UINotifier::UINotifier(MainCtx *ctx, QObject *parent)
    : QObject {parent}
    , m_ctx {ctx}
{
    setupNotifications();
}

void UINotifier::setupNotifications()
{
    connect(m_ctx, &MainCtx::intfScaleFactorChanged, this, [this]()
    {
        const QString scale = QString::number(m_ctx->getIntfScaleFactor() * 100);
        emit showNotification(Scale
                              , qtr("Scale: %1%").arg(scale));
    });

    connect(m_ctx, &MainCtx::mainInterfaceModesChanged, this, [this]()
    {
        if (m_ctx->isMinimalView())
            emit showNotification(MinimalView
                                  , qtr("Minimal View"));
    });

    connect(m_ctx, &MainCtx::clipboardHasNothingToOpen, this, [this]()
    {
        emit showNotification(NothingToOpen
                              , qtr("The clipboard has no link or file"));
    });

    if (MediaLib *ml = m_ctx->getMediaLibrary())
    {
        MLPlaylistManager *playlists = ml->playlistManager();

        connect(playlists, &MLPlaylistManager::appended, this, [this](const MLItemId &, const QString &name)
        {
            emit showNotification(PlaylistAppended, qtr("Added to %1").arg(name));
        });

        connect(playlists, &MLPlaylistManager::exported, this, [this](bool success, const QString &path)
        {
            const QString file = QFileInfo(path).fileName();
            emit showNotification(PlaylistExported, success ? qtr("Exported to %1").arg(file)
                                                            : qtr("Could not export to %1").arg(file));
        });
    }

    connect(m_ctx, &MainCtx::bookmarkAdded, this, [this](const QString &name)
    {
        emit showNotification(Bookmark, qtr("Added \"%1\"").arg(name));
    });

    // one-time hint, on the first use of Hide UI
    connect(m_ctx, &MainCtx::hideUIChanged, this, [this]()
    {
        static const QString hintShownKey = QStringLiteral("MainWindow/hide-ui-hint-shown");

        if (!m_ctx->hideUI() || m_ctx->settingValue(hintShownKey, false).toBool())
            return;

        m_ctx->setSettingValue(hintShownKey, true);
        emit showNotification(HideUI
                              , qtr("Point at the top or bottom edge to show the bars. Ctrl+H turns Hide UI off."));
    });
}
