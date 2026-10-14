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

// The shortcuts of the interface's own actions. Their keys come from
// MainCtx.actionSequence(), as the keys shown in the menus do; the player
// actions are core hotkeys.
Item {

    ShortcutExt{ context: Qt.WindowShortcut; sequence: MainCtx.actionSequence("open-file"); onActivated: DialogsProvider.simpleOpenDialog(); }
    ShortcutExt{ context: Qt.WindowShortcut; sequence: MainCtx.actionSequence("open-folder"); onActivated: DialogsProvider.PLOpenDir(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("open-disc"); onActivated: DialogsProvider.openDiscDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("open-network"); onActivated: DialogsProvider.openNetDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("open-capture"); onActivated: DialogsProvider.openCaptureDialog(); }
    // A focused text field takes Paste first (ShortcutOverride)
    ShortcutExt{ context: Qt.WindowShortcut; sequences: [StandardKey.Paste]; onActivated: MainCtx.pasteFromClipboard(); }
    ShortcutExt{ context: Qt.WindowShortcut; sequence: MainCtx.actionSequence("save-queue"); onActivated: DialogsProvider.savePlayingToPlaylist(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("convert"); onActivated: DialogsProvider.openAndTranscodingDialogs(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("stream"); onActivated: DialogsProvider.openAndStreamingDialogs(); }
    ShortcutExt{ sequence:"Ctrl+Q"; onActivated: DialogsProvider.quit(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("adjustments"); onActivated: DialogsProvider.extendedDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("media-info"); onActivated: DialogsProvider.mediaInfoDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("codec-info"); onActivated: DialogsProvider.mediaCodecDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("messages"); onActivated: DialogsProvider.messagesDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("settings"); onActivated: DialogsProvider.prefsDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("jump-to-time"); onActivated: DialogsProvider.gotoTimeDialog(); }
    ShortcutExt{ sequence: MainCtx.actionSequence("about"); onActivated: DialogsProvider.aboutDialog(); }

    ShortcutExt{ sequence: MainCtx.actionSequence("show-queue"); onActivated: MainCtx.playqueuePanel.visible = !MainCtx.playqueuePanel.visible; }

    ShortcutExt{ sequence:"F1"; onActivated: Qt.openUrlExternally("https://docs.videolan.me/vlc-user/") }
    ShortcutExt{ sequence: MainCtx.actionSequence("interface-fullscreen"); onActivated: MainCtx.toggleInterfaceFullScreen() }
    ShortcutExt{ sequence: MainCtx.actionSequence("hide-ui"); onActivated: MainCtx.minimalView = !MainCtx.minimalView; }

    ShortcutExt{
        context: Qt.WindowShortcut
        sequence: MainCtx.actionSequence("library-search")
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
