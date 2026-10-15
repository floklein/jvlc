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

#ifndef VLC_QT_SETTINGS_PAGES_HPP
#define VLC_QT_SETTINGS_PAGES_HPP

#include <QUrl>

#include "settings_model.hpp"
#include "maininterface/mainctx.hpp"

/**
 * The Settings window's rows: the basic rows of each page, bound to the
 * running interface so that changes show at once.
 */
class AppSettingsModel : public SettingsModel
{
    Q_OBJECT

    Q_PROPERTY(MainCtx* ctx READ ctx WRITE setCtx NOTIFY ctxChanged FINAL)

public:
    explicit AppSettingsModel(QObject* parent = nullptr);

    MainCtx* ctx() const { return m_ctx; }
    void setCtx(MainCtx* ctx);

    // quit, then start VLC again with the same arguments
    Q_INVOKABLE void restartVLC();

    // system "save as" dialog, returns the chosen path or an empty string
    Q_INVOKABLE QString browseSaveFile(const QString& title, const QString& current) const;
    Q_INVOKABLE QString browseOpenFile(const QString& title, const QString& current) const;
    Q_INVOKABLE QString browseDirectory(const QString& title, const QString& current) const;
    Q_INVOKABLE QUrl localFileUrl(const QString& path) const;

signals:
    void ctxChanged();

private:
    void addGeneralRows();
    void addInterfaceRows();
    void addControlsRows();
    void addOpenSubtitlesRows();
    void addResetRow();
    void onSettingChanged(const QString& id);
    void applyQueueOptions();
    LiveFilters liveFilters();

    MainCtx* m_ctx = nullptr;
};

#endif // VLC_QT_SETTINGS_PAGES_HPP
