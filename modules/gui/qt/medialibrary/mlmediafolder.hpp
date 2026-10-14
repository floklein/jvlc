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

#ifndef MLMEDIAFOLDER_HPP
#define MLMEDIAFOLDER_HPP

#include <QObject>
#include <QUrl>

#include "mlqmltypes.hpp"

class MediaLib;
Q_MOC_INCLUDE("medialibrary/medialib.hpp")

// The library folder holding a file, found by the path of the file: the
// folder button of the Top bar opens the Library pane there.
//
// Only local files have one, and only when their folder is in the library.
class MLMediaFolder : public QObject
{
    Q_OBJECT

    Q_PROPERTY(MediaLib* ml READ ml WRITE setMl NOTIFY mlChanged FINAL)

    // the file, usually Player.url
    Q_PROPERTY(QUrl url READ url WRITE setUrl NOTIFY urlChanged FINAL)

    // the folder of the file, valid when found
    Q_PROPERTY(MLItemId folderId READ folderId NOTIFY folderChanged FINAL)
    Q_PROPERTY(bool found READ found NOTIFY folderChanged FINAL)

    // true while the folder is looked up
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)

public:
    explicit MLMediaFolder(QObject *parent = nullptr);
    ~MLMediaFolder() override;

    MediaLib* ml() const { return m_mediaLib; }
    void setMl(MediaLib *ml);

    QUrl url() const { return m_url; }
    void setUrl(const QUrl &url);

    MLItemId folderId() const { return m_folderId; }
    bool found() const { return m_folderId.isValid(); }

    bool loading() const { return m_task != 0; }

signals:
    void mlChanged();
    void urlChanged();
    void folderChanged();
    void loadingChanged();

private:
    void lookUp();
    void setFolderId(MLItemId id);

    MediaLib *m_mediaLib = nullptr;
    QUrl m_url;
    MLItemId m_folderId;
    quint64 m_task = 0;
};

#endif // MLMEDIAFOLDER_HPP
