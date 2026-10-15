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

#ifndef MLFOLDERACTIONS_HPP
#define MLFOLDERACTIONS_HPP

#include <QJSValue>
#include <QObject>
#include <QVariantMap>

#include <functional>

#include "mlqmltypes.hpp"

class MediaLib;
Q_MOC_INCLUDE("medialibrary/medialib.hpp")

// The actions of a library folder's menu and of the page menu inside a
// folder.
//
// The media of a folder are those of the folder and of its subfolders, of
// one type (the videos in Videos, the songs in Songs), in the order of the
// folder tree: the subfolders first, by title in natural order (S2 before
// S10), then the folder's own media by title.
class MLFolderActions : public QObject
{
    Q_OBJECT

public:
    enum MediaType
    {
        AnyMedia = VLC_ML_MEDIA_TYPE_UNKNOWN,
        Videos = VLC_ML_MEDIA_TYPE_VIDEO,
        Songs = VLC_ML_MEDIA_TYPE_AUDIO,
    };
    Q_ENUM(MediaType)

    explicit MLFolderActions(MediaLib *ml, QObject *parent = nullptr);

    // Play and Shuffle replace the Queue (with Undo), like Play elsewhere
    Q_INVOKABLE void play(const MLItemId &folder, int type);
    Q_INVOKABLE void shuffle(const MLItemId &folder, int type);
    Q_INVOKABLE void addToQueue(const MLItemId &folder, int type);
    Q_INVOKABLE void playNext(const MLItemId &folder, int type);

    // Mark as Watched / Unwatched: every media of the folder
    Q_INVOKABLE void setWatched(const MLItemId &folder, int type, bool watched);

    // Remove from Library, for a folder the user added: its media leave the
    // library, the files stay
    Q_INVOKABLE void removeFromLibrary(const QString &mrl);
    // Exclude from Library, for a subfolder: the library skips it from now on
    Q_INVOKABLE void excludeFromLibrary(const QString &mrl);
    // Rescan: the folder, or the whole library for an empty MRL
    Q_INVOKABLE void rescan(const QString &mrl);

    // What the menus and Properties show of a folder, as a map:
    // id, type, name, mrl, path (local, empty otherwise), added (a folder the user
    // added), media, folders (the subfolders holding some), watched,
    // unwatched (in progress counts as unwatched), duration (ms), size
    // (bytes). An empty map if the folder is gone.
    void describe(const MLItemId &folder, int type, std::function<void(const QVariantMap &)> callback);
    Q_INVOKABLE void describe(const MLItemId &folder, int type, QJSValue callback);

    // Properties: describes the folder, then asks for its window
    Q_INVOKABLE void showProperties(const MLItemId &folder, int type);

    // The folder dialogs (FolderDialogs.qml), opened from any folder menu:
    // "+ New Preset…" of Preset ▸, for the folder at this path
    Q_INVOKABLE void requestNewPreset(const QString &path, const QString &name) { emit newPresetRequested(path, name); }
    // the confirmation of Remove from Library or Exclude from Library
    Q_INVOKABLE void requestRemoval(const QVariantMap &description) { emit removalRequested(description); }

signals:
    void propertiesRequested(const QVariantMap &description);
    void newPresetRequested(const QString &path, const QString &name);
    void removalRequested(const QVariantMap &description);

private:
    enum QueueAction { Play, Shuffle, Enqueue, PlayNext };
    void toQueue(const MLItemId &folder, int type, QueueAction action);

    MediaLib *m_mediaLib;
};

#endif // MLFOLDERACTIONS_HPP
