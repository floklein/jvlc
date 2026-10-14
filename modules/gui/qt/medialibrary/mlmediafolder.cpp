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

#include "mlmediafolder.hpp"

#include "medialib.hpp"
#include "mlhelper.hpp"

MLMediaFolder::MLMediaFolder(QObject *parent)
    : QObject(parent)
{
}

MLMediaFolder::~MLMediaFolder()
{
    if (m_mediaLib && m_task != 0)
        m_mediaLib->cancelMLTask(this, m_task);
}

void MLMediaFolder::setMl(MediaLib *ml)
{
    if (m_mediaLib == ml)
        return;
    if (m_mediaLib && m_task != 0)
        m_mediaLib->cancelMLTask(this, m_task);
    m_task = 0;
    m_mediaLib = ml;
    emit mlChanged();
    lookUp();
}

void MLMediaFolder::setUrl(const QUrl &url)
{
    if (m_url == url)
        return;
    m_url = url;
    emit urlChanged();
    lookUp();
}

void MLMediaFolder::lookUp()
{
    const bool wasLoading = loading();

    if (m_mediaLib && m_task != 0)
        m_mediaLib->cancelMLTask(this, m_task);
    m_task = 0;

    // the folder of another file is never shown for this one
    setFolderId(MLItemId());

    if (m_mediaLib && m_url.isLocalFile())
    {
        // the medialibrary keeps the MRLs of folders percent-encoded, with
        // a trailing slash, as QUrl writes them
        const QByteArray folderMrl = m_url.adjusted(QUrl::RemoveFilename | QUrl::RemoveQuery
                                                    | QUrl::RemoveFragment)
                                          .toEncoded();

        struct Ctx {
            MLItemId folderId;
        };

        m_task = m_mediaLib->runOnMLThread<Ctx>(this,
        // ML thread
        [folderMrl](vlc_medialibrary_t *ml, Ctx &ctx)
        {
            ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder_by_mrl(ml, folderMrl.constData()));
            if (folder)
                ctx.folderId = MLItemId(folder->i_id, VLC_ML_PARENT_FOLDER);
        },
        // UI thread
        [this](quint64, Ctx &ctx)
        {
            m_task = 0;
            setFolderId(ctx.folderId);
            emit loadingChanged();
        });
    }

    if (wasLoading != loading())
        emit loadingChanged();
}

void MLMediaFolder::setFolderId(MLItemId id)
{
    if (m_folderId == id)
        return;
    m_folderId = id;
    emit folderChanged();
}
