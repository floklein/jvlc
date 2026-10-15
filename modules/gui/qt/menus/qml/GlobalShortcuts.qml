/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick

import VLC.MainInterface
import VLC.Dialogs
import VLC.Player

// The shortcuts of the interface's own actions. Their keys come from
// MainCtx.actionSequence(), as the keys shown in the menus do; the player
// actions are core hotkeys.
Item {
    id: root

    // the key of an interface action, updated when Keyboard Shortcuts saves
    function sequence(action: string) : string {
        return MainCtx.keyBindingsRevision >= 0 ? MainCtx.actionSequence(action) : ""
    }

    ShortcutExt{ context: Qt.WindowShortcut; sequence: root.sequence("open-file"); onActivated: DialogsProvider.simpleOpenDialog(); }
    ShortcutExt{ context: Qt.WindowShortcut; sequence: root.sequence("open-folder"); onActivated: DialogsProvider.PLOpenDir(); }
    ShortcutExt{ sequence: root.sequence("open-disc"); onActivated: DialogsProvider.openDiscDialog(); }
    ShortcutExt{ sequence: root.sequence("open-network"); onActivated: DialogsProvider.openNetDialog(); }
    ShortcutExt{ sequence: root.sequence("open-capture"); onActivated: DialogsProvider.openCaptureDialog(); }
    // A focused text field takes Paste first (ShortcutOverride)
    ShortcutExt{ context: Qt.WindowShortcut; sequences: [StandardKey.Paste]; onActivated: MainCtx.pasteFromClipboard(); }
    ShortcutExt{ context: Qt.WindowShortcut; sequence: root.sequence("save-queue"); onActivated: DialogsProvider.savePlayingToPlaylist(); }
    ShortcutExt{ sequence: root.sequence("convert"); onActivated: DialogsProvider.openAndTranscodingDialogs(); }
    ShortcutExt{ sequence: root.sequence("stream"); onActivated: DialogsProvider.openAndStreamingDialogs(); }
    // Quit is a core hotkey: the same key, rebound with it
    ShortcutExt{ sequence: root.sequence("quit"); onActivated: DialogsProvider.quit(); }
    ShortcutExt{ sequence: root.sequence("adjustments"); onActivated: MainCtx.toggleAdjustments(); }
    ShortcutExt{ sequence: root.sequence("media-info"); onActivated: DialogsProvider.mediaInfoDialog(); }
    ShortcutExt{ sequence: root.sequence("codec-info"); onActivated: DialogsProvider.mediaCodecDialog(); }
    ShortcutExt{ sequence: root.sequence("messages"); onActivated: DialogsProvider.messagesDialog(); }
    ShortcutExt{ sequence: root.sequence("settings"); onActivated: DialogsProvider.prefsDialog(); }
    ShortcutExt{ sequence: root.sequence("jump-to-time"); onActivated: DialogsProvider.gotoTimeDialog(); }
    ShortcutExt{ sequence: root.sequence("about"); onActivated: DialogsProvider.aboutDialog(); }

    ShortcutExt{ sequence: root.sequence("show-queue"); onActivated: MainCtx.playqueuePanel.visible = !MainCtx.playqueuePanel.visible; }

    ShortcutExt{ sequence: root.sequence("help"); onActivated: Qt.openUrlExternally("https://docs.videolan.me/vlc-user/") }
    // the Library pane beside the video
    ShortcutExt{ sequence: root.sequence("library-pane"); onActivated: if (MainCtx.playerView) LibraryPages.paneOpen = !LibraryPages.paneOpen }
    ShortcutExt{ sequence: root.sequence("interface-fullscreen"); onActivated: MainCtx.toggleInterfaceFullScreen() }
    ShortcutExt{ sequence: root.sequence("hide-ui"); onActivated: MainCtx.hideUI = !MainCtx.hideUI; }
    // in while something plays, out at any time
    ShortcutExt{
        sequence: root.sequence("picture-in-picture")
        onActivated: MainCtx.pictureInPicture = !MainCtx.pictureInPicture && Player.isStarted
    }

    ShortcutExt{
        context: Qt.WindowShortcut
        sequence: root.sequence("library-search")
        onActivated: {
            MainCtx.playerView = false
            MainCtx.search.askShow()
        }
    }

    ShortcutExt{ sequences: [StandardKey.ZoomIn, "Ctrl+="]; onActivated: MainCtx.incrementIntfUserScaleFactor(true) }
    ShortcutExt{ sequences: [StandardKey.ZoomOut, "Ctrl+Shift+-"]; onActivated: MainCtx.incrementIntfUserScaleFactor(false) }

    Loader {
        active: MainCtx.mediaLibraryAvailable
        source: "qrc:///qt/qml/VLC/MediaLibrary/MLGlobalShortcuts.qml"
    }
}
