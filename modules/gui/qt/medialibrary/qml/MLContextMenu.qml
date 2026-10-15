
/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQml

import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Dialogs
import VLC.Player

import VLC.Util

// @brief - the menu of the items of the library:
// - a folder (of Videos, Songs or Home's Frequently Visited Folders) gets the
//   folder menu: Play, Shuffle, Add to Queue, Play next, Preset ▸, Pin to
//   Sidebar, Mark as Watched / Unwatched, Add Subtitles to This Folder…,
//   Show in File Explorer, Properties, then Remove from Library (a folder the
//   user added) or Exclude from Library (a subfolder);
// - files get the file menu: Play, Play next, Add to Queue, Add to Playlist ▸,
//   Mark as Watched / Unwatched, Favorite, Add Subtitle Tracks…, Add Audio
//   Track…, Stream & Cast…, Convert…, Show in File Explorer, Go to Source,
//   Properties & Metadata, then Remove from Recent / Playlist where it applies;
// - other items (albums, artists, genres...) get the group actions.
NativeMenu {
    id: root

    // Properties

    required property MLBaseModel model

    property string idDataRole: "id"
    property string albumIdDataRole: "album_id"

    // applies to the albums, artists and genres, not to files
    property bool showPlayAsAudioAction: false

    // This is only applicable when the model is `MLAudioModel`:
    property bool showPlayAsAlbumAction: implicitShowPlayAsAlbumAction
    property bool implicitShowPlayAsAlbumAction: !showPlayAsAudioAction

    // Signals

    signal showMediaInformation(int index)


    // required by TableViewExt
    property var tableView_popup: function (index/* reserved for future */, selectedIndexes, globalPos) {
        return popup(selectedIndexes, globalPos)
    }

    // Settings

    actions: [
        // the folder menu
        {
            "text": qsTr("Play"),
            "action": (dataList) => FolderActions.play(_folderId(dataList), _folderType(dataList[0])),
            "visible": _isFolder
        }, {
            "text": qsTr("Shuffle"),
            "action": (dataList) => FolderActions.shuffle(_folderId(dataList), _folderType(dataList[0])),
            "visible": _isFolder
        }, {
            "text": qsTr("Add to Queue"),
            "action": (dataList) => FolderActions.addToQueue(_folderId(dataList), _folderType(dataList[0])),
            "visible": _isFolder
        }, {
            "text": qsTr("Play Next"),
            "action": (dataList) => FolderActions.playNext(_folderId(dataList), _folderType(dataList[0])),
            "visible": _isFolder
        }, {
            "text": qsTr("Preset"),
            "children": presetItems,
            // music folders too: an equalizer per album
            "visible": (dataList) => _isFolder(dataList) && _folderPath(dataList) !== ""
        }, {
            "text": qsTr("Pin to Sidebar"),
            "action": pinFolder,
            "visible": _showPinFolder
        }, {
            "text": qsTr("Unpin from Sidebar"),
            "action": unpinFolder,
            "visible": _showUnpinFolder
        }, {
            "text": qsTr("Mark as Watched"),
            "action": (dataList) => FolderActions.setWatched(_folderId(dataList), _folderType(dataList[0]), true),
            "visible": (dataList) => _canMarkFolder(dataList) && dataList[0].folder.unwatched > 0
        }, {
            "text": qsTr("Mark as Unwatched"),
            "action": (dataList) => FolderActions.setWatched(_folderId(dataList), _folderType(dataList[0]), false),
            "visible": (dataList) => _canMarkFolder(dataList) && dataList[0].folder.unwatched === 0
        }, {
            "text": qsTr("Add Subtitles to This Folder…"),
            "action": (dataList) => DialogsProvider.addFolderSubtitlesDialog(_folderMrl(dataList)),
            "visible": (dataList) => _isFolder(dataList) && _folderPath(dataList) !== ""
                                     && _folderType(dataList[0]) !== FolderActions.Songs
        }, {
            "separator": true,
            "visible": _isFolder
        }, {
            "text": _showInFileManagerText,
            "action": (dataList) => Qt.openUrlExternally(_folderMrl(dataList)),
            "visible": (dataList) => _isFolder(dataList) && _folderPath(dataList) !== ""
        }, {
            "text": qsTr("Properties"),
            "action": (dataList) => FolderActions.showProperties(_folderId(dataList), _folderType(dataList[0])),
            "visible": _isFolder
        }, {
            "separator": true,
            "visible": _isFolder
        }, {
            "text": qsTr("Remove from Library"),
            "action": (dataList) => FolderActions.requestRemoval(dataList[0].folder),
            "visible": (dataList) => _isFolder(dataList) && dataList[0].folder?.added === true
        }, {
            "text": qsTr("Exclude from Library"),
            "action": (dataList) => FolderActions.requestRemoval(dataList[0].folder),
            "visible": (dataList) => _isFolder(dataList) && dataList[0].folder?.added === false
        },

        // the file menu
        {
            "text": qsTr("Play"),
            "action": addAndPlay,
            "visible": _isFiles
        }, {
            "text": qsTr("Play Next"),
            "action": playNext,
            "visible": _isFiles
        }, {
            "text": qsTr("Add to Queue"),
            "action": enqueue,
            "visible": _isFiles
        }, {
            "text": qsTr("Add to Playlist"),
            "children": addToPlaylistItems,
            "visible": _isFiles
        }, {
            "text": qsTr("Mark as Watched"),
            "action": (dataList) => MediaLib.setMediaWatched(_mlIDList(dataList), true),
            "visible": (dataList) => _areVideos(dataList) && !dataList.every(_isWatched)
        }, {
            "text": qsTr("Mark as Unwatched"),
            "action": (dataList) => MediaLib.setMediaWatched(_mlIDList(dataList), false),
            "visible": (dataList) => _areVideos(dataList) && dataList.every(_isWatched)
        }, {
            "text": qsTr("Favorite"),
            "action": addFavorite,
            "visible": (dataList) => _isFiles(dataList) && _checkRole(dataList, "isFavorite", false)
        }, {
            "text": qsTr("Remove from Favorites"),
            "action": removeFavorite,
            "visible": (dataList) => _isFiles(dataList) && _checkRole(dataList, "isFavorite", true)
        }, {
            "text": qsTr("Add Subtitle Tracks…"),
            "action": (dataList) => DialogsProvider.addSubtitlesDialog(dataList[0].mrl),
            "visible": _isLocalVideo
        }, {
            "text": qsTr("Add Audio Track…"),
            "action": (dataList) => DialogsProvider.addAudioTrackDialog(dataList[0].mrl),
            "visible": _isLocalVideo
        }, {
            "text": qsTr("Stream & Cast…"),
            "action": (dataList) => DialogsProvider.streamAndCastMediaDialog(dataList[0].mrl, dataList[0].title),
            "visible": (dataList) => _isFiles(dataList) && dataList.length === 1
        }, {
            "text": qsTr("Convert…"),
            "action": (dataList) => DialogsProvider.convertDialog(dataList.map((data) => data.mrl)),
            "visible": _isFiles
        }, {
            "separator": true,
            "visible": _isFiles
        }, {
            "text": _showInFileManagerText,
            "action": (dataList) => Qt.openUrlExternally(_parentUrl(dataList[0].mrl)),
            "visible": _isLocalFile
        }, {
            "text": qsTr("Go to Source"),
            "action": goToSource,
            "visible": (dataList) => _isLocalFile(dataList) && !_isFolderTree()
        }, {
            "text": qsTr("Properties & Metadata"),
            "action": (dataList) => DialogsProvider.mediaInfoDialog(dataList[0][idDataRole]),
            "visible": (dataList) => _isFiles(dataList) && dataList.length === 1
                                     && !(dataList[0][idDataRole].hasParent())
        }, {
            "separator": true,
            "visible": _isFiles
        }, {
            "text": qsTr("Remove from Recent"),
            "action": removeFromRecent,
            "visible": (dataList) => _isFiles(dataList) && root.model instanceof MLRecentMediaModel
        }, {
            "text": qsTr("Remove from Playlist"),
            "action": (dataList, options, indexes) => root.model.remove(indexes),
            "visible": (dataList) => _isFiles(dataList) && root.model instanceof MLPlaylistModel
        },

        // the other items: albums, artists, genres...
        {
            "text": qsTr("Play"),
            "action": addAndPlay,
            "visible": _isOther
        }, {
            "text": qsTr("Play as audio"),
            "action": playAsAudio,
            "visible": (dataList) => _isOther(dataList) && root.showPlayAsAudioAction
        }, {
            "text": qsTr("Play as album"),
            "action": playAsAlbum,
            "visible": (dataList) => _isOther(dataList) && root.showPlayAsAlbumAction
                                     && (root.model instanceof MLAudioModel)
        }, {
            "text": qsTr("Play Next"),
            "action": playNext,
            "visible": _isOther
        }, {
            "text": qsTr("Add to Queue"),
            "action": enqueue,
            "visible": _isOther
        }, {
            "text": qsTr("Add Album to Queue"),
            "action": enqueueAsAlbum,
            "visible": (dataList) => _isOther(dataList) && root.showPlayAsAlbumAction
                                     && (root.model instanceof MLAudioModel)
        }, {
            "text": qsTr("Favorite"),
            "action": addFavorite,
            "visible": (dataList) => _isOther(dataList) && _checkRole(dataList, "isFavorite", false)
        }, {
            "text": qsTr("Remove from Favorites"),
            "action": removeFavorite,
            "visible": (dataList) => _isOther(dataList) && _checkRole(dataList, "isFavorite", true)
        }, {
            "text": qsTr("Add to Playlist"),
            "children": addToPlaylistItems,
            "visible": _isOther
        }, {
            "text": qsTr("Delete"),
            "action": deleteStream,
            "visible": (dataList) => _isOther(dataList) && _checkRole(dataList, "isDeletable", true)
        }, {
            "text": qsTr("Information"),
            "action": _signalShowInformation,
            "visible": (dataList, options, indexes) => _isOther(dataList)
                                                       && showInformationAvailable(dataList, options, indexes)
        }
    ]

    // Events

    // a folder's menu also needs its description (what it holds, whether it
    // was added by the user...), from the media library
    onRequestData: (requestID, indexes) => {
        model.getData(indexes, function (data) {
            if (data.length !== 1 || !root._rowIsFolder(data[0])) {
                setData(requestID, data)
                return
            }

            FolderActions.describe(data[0][root.idDataRole], root._folderType(data[0]), (folder) => {
                setData(requestID, [Object.assign({}, data[0], { "folder": folder })])
            })
        })
    }

    // Functions

    function addAndPlay(dataList, options, indexes) {
        // the model applies the Play rule (the rest of the view after a media)
        model.addAndPlay(indexes, _playerOptions(options))
    }

    function playNext(dataList, options, indexes) {
        model.ml.playNext(_mlIDList(dataList), _playerOptions(options))
    }

    function playAsAudio(dataList, options, indexes) {
        model.ml.addAndPlay(_mlIDList(dataList), _playerOptions(options, ":no-video"))
    }

    function playAsAlbum(dataList, options, indexes) {
        model.ml.addAndPlay(_mlIDList(dataList, root.albumIdDataRole), _playerOptions(options))
    }

    function enqueue(dataList, options, indexes) {
        model.ml.addToPlaylist(_mlIDList(dataList), _playerOptions(options))
    }

    function enqueueAsAlbum(dataList, options, indexes) {
        model.ml.addToPlaylist(_mlIDList(dataList, root.albumIdDataRole), _playerOptions(options))
    }

    // The "Add to Playlist" submenu: the most recently changed Playlists,
    // "More…" (the picker) and "+ New Playlist…"
    function addToPlaylistItems(dataList, options, indexes) {
        const ids = _mlIDList(dataList)
        const items = PlaylistManager.recentPlaylists.map((playlist) => ({
            "text": playlist.name,
            "action": () => PlaylistManager.append(playlist.id, ids)
        }))

        if (items.length > 0)
            items.push({ "separator": true })

        items.push({
            "text": qsTr("More…"),
            "action": () => PlaylistManager.requestPicker(ids)
        }, {
            "text": qsTr("+ New Playlist…"),
            "action": () => PlaylistManager.requestNewPlaylist(ids)
        })
        return items
    }

    // The folder's "Preset ▸" submenu: Default, the Presets, "+ New Preset…".
    // The check marks the Preset its files use, its own or a parent's.
    function presetItems(dataList, options, indexes) {
        const folder = _folderPath(dataList)
        const presets = Presets.model
        const current = presets.presetOf(folder)

        const items = [{
            "text": qsTr("Default"),
            "checked": current === "",
            // as in the sidebar: a parent's Preset stops here
            "action": () => Presets.chooseForFolder(folder, "")
        }]

        for (let row = 0; row < presets.count; ++row) {
            const index = presets.index(row, 0)
            const id = presets.data(index, PresetsModel.ID)
            items.push({
                "text": presets.data(index, PresetsModel.NAME),
                "checked": id === current,
                // shows "Using <Preset> for <folder>"
                "action": () => Presets.chooseForFolder(folder, id)
            })
        }

        items.push({ "separator": true }, {
            "text": qsTr("+ New Preset…"),
            "action": () => FolderActions.requestNewPreset(folder, dataList[0].folder.name)
        })
        return items
    }

    function addFavorite(dataList, options, indexes) {
        _setFavorite(dataList, indexes, true)
    }

    function removeFavorite(dataList, options, indexes) {
        _setFavorite(dataList, indexes, false)
    }

    // a folder of Music ▸ Songs opens there
    function _pinType(data) {
        return _folderType(data) === FolderActions.Songs ? Pins.MUSIC_FOLDER : Pins.FOLDER
    }

    function pinFolder(dataList, options, indexes) {
        Pins.pin(_pinType(dataList[0]), dataList[0][idDataRole].toString(), dataList[0].title)
    }

    function unpinFolder(dataList, options, indexes) {
        Pins.unpin(_pinType(dataList[0]), dataList[0][idDataRole].toString())
    }

    // hides them from Recent, their watched state and progress stay
    function removeFromRecent(dataList, options, indexes) {
        model.removeFromRecent(indexes)
    }

    // opens the file's folder in the library
    function goToSource(dataList, options, indexes) {
        _sourceFolder.isVideo = dataList[0].isVideo !== false
        _sourceFolder.pending = true
        _sourceFolder.url = dataList[0].mrl
        _goToSourceWhenFound()
    }

    function deleteStream(dataList, options, indexes) {
        model.deleteStream(dataList[0][idDataRole])
    }

    function showInformationAvailable(dataList, options, indexes) {
        return indexes.length === 1
                && Helpers.isInteger(options?.["information"] ?? null)
    }

    // Private

    readonly property string _showInFileManagerText: (Qt.platform.os === "windows")
                                                     ? qsTr("Show in File Explorer")
                                                     : qsTr("Show in File Manager")

    readonly property MLMediaFolder _sourceFolder: MLMediaFolder {
        // Go to Source is waiting for the folder
        property bool pending: false
        property bool isVideo: true

        ml: MediaLib

        onLoadingChanged: root._goToSourceWhenFound()
    }

    function _goToSourceWhenFound() {
        if (!_sourceFolder.pending || _sourceFolder.loading)
            return
        _sourceFolder.pending = false
        LibraryPages.showFolder(_sourceFolder.folderId, _sourceFolder.isVideo, Qt.OtherFocusReason)
    }

    function _checkRole(dataList, role, expected) {
        if (dataList.length !== 1)
            return false

        if (!(role in dataList[0]))
            return false

        return (dataList[0][role] === expected)
    }

    // the type of media of the folders: the videos in Videos, the songs in
    // Songs; on Home, where folders of both are listed, what the folder holds
    function _folderType(data) {
        if (root.model instanceof MLMusicFolderTreeModel)
            return FolderActions.Songs
        if (root.model instanceof MLVideoFoldersModel && root.model.anyMedia && data) {
            if (!data.nb_videos)
                return FolderActions.Songs
            return data.nb_audio ? FolderActions.AnyMedia : FolderActions.Videos
        }
        if (root.model instanceof MLFolderTreeModel || root.model instanceof MLVideoFoldersModel)
            return FolderActions.Videos
        return FolderActions.AnyMedia
    }

    // the view is a level of a folder tree, where files are at their source
    function _isFolderTree() {
        return root.model instanceof MLFolderTreeModel || root.model instanceof MLMusicFolderTreeModel
    }

    // a folder of a tree or of Home's Frequently Visited Folders
    function _rowIsFolder(data) {
        return data.isFolder === true || root.model instanceof MLVideoFoldersModel
    }

    function _isFolder(dataList) {
        return dataList.length === 1 && _rowIsFolder(dataList[0])
    }

    function _folderId(dataList) {
        return dataList[0][idDataRole]
    }

    function _folderMrl(dataList) {
        return dataList[0].folder?.mrl ?? dataList[0].mrl ?? ""
    }

    // the folder on disk, empty if it isn't local
    function _folderPath(dataList) {
        return dataList[0].folder?.path ?? ""
    }

    function _canMarkFolder(dataList) {
        return _isFolder(dataList) && _folderType(dataList[0]) !== FolderActions.Songs
                && (dataList[0].folder?.media ?? 0) > 0
    }

    function _showPinFolder(dataList, options, indexes) {
        return _isFolder(dataList) && !Pins.isPinned(_pinType(dataList[0]), dataList[0][idDataRole].toString())
    }

    function _showUnpinFolder(dataList, options, indexes) {
        return _isFolder(dataList) && Pins.isPinned(_pinType(dataList[0]), dataList[0][idDataRole].toString())
    }

    // media files: rows with an MRL that aren't folders
    function _isFiles(dataList) {
        return dataList.length > 0
                && dataList.every((data) => !_rowIsFolder(data) && ("mrl" in data) && !!data.mrl)
    }

    function _isOther(dataList) {
        return dataList.length > 0 && !_isFolder(dataList) && !_isFiles(dataList)
    }

    function _areVideos(dataList) {
        return _isFiles(dataList) && dataList.every((data) => data.isVideo === true)
    }

    // played to the end at least once, and not started again
    function _isWatched(data) {
        return (data.playcount ?? 0) > 0 && !((data.progress ?? 0) > 0)
    }

    function _isLocalFile(dataList) {
        return _isFiles(dataList) && dataList.length === 1 && dataList[0].isLocal === true
    }

    function _isLocalVideo(dataList) {
        return _isLocalFile(dataList) && dataList[0].isVideo === true
    }

    // the folder of a file's MRL
    function _parentUrl(mrl) {
        return mrl.substring(0, mrl.lastIndexOf("/") + 1)
    }

    function _setFavorite(dataList, indexes, favorite) {
        // a media model shows the change at once
        if (typeof root.model.setMediaIsFavorite === "function")
            root.model.setMediaIsFavorite(indexes[0], favorite)
        else
            MediaLib.setMediaFavorite(_mlIDList(dataList), favorite)
    }

    function _signalShowInformation(dataList, options) {
        const index = options?.["information"] ?? null
        console.assert(Helpers.isInteger(index))
        showMediaInformation(index)
    }

    function _playerOptions(options, extraOptions) {
        const playerOpts = options?.["player-options"] ?? []
        return playerOpts.concat(extraOptions)
    }

    function _mlIDList(dataList, role = root.idDataRole) {
        const idList = []
        for (let i in dataList) {
            console.assert(dataList[i][role] !== undefined)
            idList.push(dataList[i][role])
        }

        return idList
    }
}
