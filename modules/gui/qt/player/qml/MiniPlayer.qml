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
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player
import VLC.Util

// The Mini player (Figma 970:3308): the video, or the cover of the music, in
// a corner of the full-window library while media plays. It can be dragged to
// any corner, and clicking the video returns to the player.
T.Control {
    id: root

    // Properties

    // where the Mini player moves, in the coordinates of its parent
    property rect area: Qt.rect(0, 0, parent?.width ?? 0, parent?.height ?? 0)

    // Qt.TopLeftCorner, Qt.TopRightCorner, Qt.BottomLeftCorner or
    // Qt.BottomRightCorner; kept across restarts
    property int corner: Qt.BottomRightCorner

    // the space between the Mini player and the edges of the area
    property real edgeMargin: VLCStyle.spacing_8

    // the video plays in the Mini player, else the cover shows
    readonly property bool showsVideo: MainCtx.hasEmbededVideo && MainCtx.canShowVideoPIP

    readonly property bool _playing: Player.playingState === Player.PLAYING_STATE_PLAYING

    // set while the Mini player glides to its corner
    property bool _snapping: false

    readonly property string _settingKey: "MainWindow/mini-player-corner"

    // Settings

    // there is no Mini player when nothing plays
    visible: Player.playingState !== Player.PLAYING_STATE_STOPPED
    enabled: visible

    // Figma: 390 x 340 in a 1920 px window; at most 2/5 of a narrow one
    width: Math.min(VLCStyle.dp(390, VLCStyle.scale), Math.round(area.width * 2 / 5))
    height: implicitHeight

    implicitHeight: implicitContentHeight + topPadding + bottomPadding

    padding: VLCStyle.spacing_8

    // not a Tab stop itself: it passes the focus on to "Back to the player", so
    // Shift+Tab from there would land on it and come straight back
    focusPolicy: Qt.NoFocus

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Mini player")
    Accessible.description: qsTr("Alt+arrow keys move it to another corner")
    AccessibleCompat.id: "miniPlayer"

    // Functions

    function placeAtCorner() {
        const pos = Helpers.cornerPosition(root.corner, Qt.size(root.width, root.height),
                                           root.area, root.edgeMargin)
        root.x = pos.x
        root.y = pos.y
    }

    readonly property bool _atLeft: (corner === Qt.TopLeftCorner || corner === Qt.BottomLeftCorner)
    readonly property bool _atTop: (corner === Qt.TopLeftCorner || corner === Qt.TopRightCorner)

    // the keyboard moves it to another corner, as dragging it does
    function moveToCorner(left, top) {
        const corner = top ? (left ? Qt.TopLeftCorner : Qt.TopRightCorner)
                           : (left ? Qt.BottomLeftCorner : Qt.BottomRightCorner)
        if (corner === root.corner)
            return

        MainCtx.setSettingValue(root._settingKey, corner)

        root._snapping = true
        snapEnd.restart()
        root.corner = corner
    }

    // F6 or Tab lands on the Mini player itself: the focus goes on to its first
    // control, "Back to the player", where it shows (then Tab reaches Play)
    onActiveFocusChanged: {
        if (activeFocus && Window.activeFocusItem === root)
            picture.forceActiveFocus(focusReason)
    }

    // Alt+arrow keys, while the Mini player has the focus. Shortcuts, as the
    // buttons inside take the arrow keys for the navigation.
    Shortcut {
        enabled: root.activeFocus
        sequence: "Alt+Left"
        onActivated: root.moveToCorner(true, root._atTop)
    }

    Shortcut {
        enabled: root.activeFocus
        sequence: "Alt+Right"
        onActivated: root.moveToCorner(false, root._atTop)
    }

    Shortcut {
        enabled: root.activeFocus
        sequence: "Alt+Up"
        onActivated: root.moveToCorner(root._atLeft, true)
    }

    Shortcut {
        enabled: root.activeFocus
        sequence: "Alt+Down"
        onActivated: root.moveToCorner(root._atLeft, false)
    }

    // Events

    Component.onCompleted: {
        const saved = parseInt(MainCtx.settingValue(_settingKey, Qt.BottomRightCorner))
        if ([Qt.TopLeftCorner, Qt.TopRightCorner, Qt.BottomLeftCorner, Qt.BottomRightCorner].includes(saved))
            corner = saved
        placeAtCorner()
    }

    onCornerChanged: placeAtCorner()
    onAreaChanged: if (!dragHandler.active) placeAtCorner()
    onWidthChanged: if (!dragHandler.active) placeAtCorner()
    onHeightChanged: if (!dragHandler.active) placeAtCorner()

    Behavior on x {
        enabled: root._snapping && !MainCtx.reduceMotion

        NumberAnimation {
            duration: VLCStyle.duration_short * 1.5
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.23, 1, 0.32, 1, 1, 1]
        }
    }

    Behavior on y {
        enabled: root._snapping && !MainCtx.reduceMotion

        NumberAnimation {
            duration: VLCStyle.duration_short * 1.5
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.23, 1, 0.32, 1, 1, 1]
        }
    }

    Timer {
        id: snapEnd

        interval: VLCStyle.duration_short * 2
        onTriggered: root._snapping = false
    }

    // dropped anywhere, it goes to the nearest corner
    DragHandler {
        id: dragHandler

        target: root

        cursorShape: active ? Qt.DragMoveCursor : undefined

        onActiveChanged: {
            if (active)
                return

            const corner = Helpers.nearestCorner(Qt.rect(root.x, root.y, root.width, root.height),
                                                 root.area)
            MainCtx.setSettingValue(root._settingKey, corner)

            root._snapping = true
            snapEnd.restart()
            if (corner === root.corner)
                root.placeAtCorner()
            else
                root.corner = corner
        }
    }

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    // a key drawn over the picture
    component PictureKey : Widgets.IconButton {
        visible: root._showKeys

        font.pixelSize: VLCStyle.dp(43, VLCStyle.scale)

        Navigation.parentItem: root
        Navigation.upItem: picture
        Navigation.downItem: folderButton.visible ? folderButton : queueButton
    }

    // the hover keys over the picture (Figma 975:4333); they show too while
    // "Back to the player" has the focus, so that Tab reaches them
    readonly property bool _showKeys: pictureHover.hovered || MainCtx.usingTouch
                                      || picture.activeFocus
                                      || playButton.hovered || playButton.visualFocus
                                      || previousButton.hovered || previousButton.visualFocus
                                      || nextButton.hovered || nextButton.visualFocus
                                      || stopButton.hovered || stopButton.visualFocus
                                      || expandButton.hovered || expandButton.visualFocus

    background: Rectangle {
        radius: VLCStyle.radius_xlarge
        color: theme.bg.primary
        border.width: VLCStyle.border
        border.color: theme.border

        // It is acceptable for effects to exceed the boundaries of the
        // parent. They are an exception to the "hypothetical clip test".
        Widgets.DefaultShadow {
        }
    }

    // Figma 970:3090: the picture and its progress 4 px apart, then 20 px down
    // the title and the buttons, 14 px apart in an 8 x 4 px padding
    contentItem: ColumnLayout {
        spacing: VLCStyle.spacing_4

        // the video, or the cover; clicking it returns to the player
        Item {
            id: picture

            Layout.fillWidth: true
            Layout.preferredHeight: Math.round(width * 9 / 16)

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Back to the player")
            Accessible.focusable: true
            Accessible.onPressAction: MainCtx.playerView = true

            // a button for the keyboard too: Enter or Space returns to the player
            activeFocusOnTab: true

            Keys.priority: Keys.AfterItem
            Keys.onPressed: (event) => {
                // not the keys of the Play button inside
                if (!playButton.activeFocus && KeyHelper.matchOk(event)) {
                    event.accepted = true
                    MainCtx.playerView = true
                    return
                }
                picture.Navigation.defaultKeyAction(event)
            }

            Navigation.parentItem: root
            Navigation.downItem: playButton

            Rectangle {
                anchors.fill: parent

                visible: !root.showsVideo

                radius: VLCStyle.radius_normal
                color: theme.bg.secondary

                Image {
                    anchors.fill: parent

                    source: (Player.artwork && Player.artwork.toString())
                            ? Player.artwork
                            : (Player.hasVideoOutput ? VLCStyle.noArtVideoCover
                                                     : VLCStyle.noArtAlbumCover)
                    fillMode: Image.PreserveAspectFit
                    sourceSize: Qt.size(width, height)
                    asynchronous: true
                }
            }

            Loader {
                anchors.fill: parent

                active: root.showsVideo

                sourceComponent: VideoSurface {
                    videoSurfaceProvider: MainCtx.videoSurfaceProvider
                }
            }

            DoubleClickIgnoringItem {
                anchors.fill: parent

                TapHandler {
                    gesturePolicy: TapHandler.WithinBounds

                    onTapped: MainCtx.playerView = true
                }

                HoverHandler {
                    id: pictureHover

                    cursorShape: Qt.PointingHandCursor
                }
            }

            // the keyboard focus ring of "Back to the player"
            Rectangle {
                anchors.fill: parent

                visible: picture.activeFocus && !playButton.activeFocus

                radius: VLCStyle.radius_small
                color: "transparent"
                border.width: VLCStyle.focus_border
                // the focus ring color: theme.visualFocus is only set on a focused context
                border.color: theme.accent
            }

            // while hovered, the picture dims under the keys
            Rectangle {
                anchors.fill: parent

                visible: root._showKeys

                radius: VLCStyle.radius_normal
                color: theme.shadow.alpha(0.6)
            }

            PictureKey {
                id: previousButton

                anchors.verticalCenter: playButton.verticalCenter
                anchors.right: playButton.left
                anchors.rightMargin: VLCStyle.dp(40, VLCStyle.scale)

                description: qsTr("Previous")
                text: VLCIcons.previous
                enabled: MainPlaylistController.hasPrev

                Navigation.rightItem: playButton

                onClicked: MainPlaylistController.prev()
            }

            // play and pause without leaving the library
            PictureKey {
                id: playButton

                anchors.centerIn: parent

                description: root._playing ? qsTr("Pause") : qsTr("Play")

                AccessibleCompat.id: "miniPlayerPlayButton"

                text: root._playing ? VLCIcons.pause_filled : VLCIcons.play_filled

                Navigation.leftItem: previousButton
                Navigation.rightItem: nextButton

                onClicked: MainPlaylistController.togglePlayPause()
            }

            PictureKey {
                id: nextButton

                anchors.verticalCenter: playButton.verticalCenter
                anchors.left: playButton.right
                anchors.leftMargin: VLCStyle.dp(40, VLCStyle.scale)

                description: qsTr("Next")
                text: VLCIcons.next
                enabled: MainPlaylistController.hasNext

                Navigation.leftItem: playButton

                onClicked: MainPlaylistController.next()
            }

            // × stops, which closes the Mini player
            PictureKey {
                id: stopButton

                x: VLCStyle.dp(14, VLCStyle.scale)
                y: VLCStyle.dp(14, VLCStyle.scale)

                font.pixelSize: VLCStyle.icon_normal

                description: qsTr("Stop")
                text: VLCIcons.close

                Navigation.rightItem: expandButton

                onClicked: MainPlaylistController.stop()
            }

            // ↗ back to the player
            PictureKey {
                id: expandButton

                x: parent.width - width - VLCStyle.dp(14, VLCStyle.scale)
                y: VLCStyle.dp(14, VLCStyle.scale)

                font.pixelSize: VLCStyle.icon_normal

                description: qsTr("Back to the player")
                text: VLCIcons.open

                Navigation.leftItem: stopButton

                onClicked: MainCtx.playerView = true
            }
        }

        // how far it played: the seek bar's look, 16 px high
        Rectangle {
            id: progress

            Layout.fillWidth: true
            Layout.preferredHeight: VLCStyle.dp(16, VLCStyle.scale)

            readonly property real _inset: VLCStyle.dp(2, VLCStyle.scale)
            readonly property real _played: (width - _inset * 2) * Helpers.clamp(Player.position, 0, 1)

            radius: VLCStyle.radius_normal
            color: progressTheme.bg.primary

            ColorContext {
                id: progressTheme
                colorSet: ColorContext.Slider
            }

            Accessible.role: Accessible.ProgressBar
            Accessible.name: qsTr("%1 of %2").arg(Player.time.formatHMS()).arg(Player.length.formatHMS())

            Rectangle {
                x: progress._inset
                y: progress._inset
                width: progress._played
                height: parent.height - progress._inset * 2

                radius: VLCStyle.radius_small
                color: progressTheme.bg.highlight
            }

            // with the keys, the handle at its end
            Rectangle {
                x: progress._inset + Math.max(0, progress._played - width)
                y: progress._inset
                width: VLCStyle.dp(8, VLCStyle.scale)
                height: parent.height - progress._inset * 2

                visible: root._showKeys

                radius: VLCStyle.radius_small
                color: progressTheme.fg.primary
            }
        }

        T.Label {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.spacing_20 + VLCStyle.spacing_4 - parent.spacing
            Layout.leftMargin: VLCStyle.spacing_8
            Layout.rightMargin: VLCStyle.spacing_8

            text: Player.title || Player.name
            elide: Text.ElideRight
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_xlarge
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: VLCStyle.spacing_14 - parent.spacing
            Layout.leftMargin: VLCStyle.spacing_8 - folderButton.leftPadding
            Layout.rightMargin: VLCStyle.spacing_8 - VLCStyle.spacing_4
            Layout.bottomMargin: VLCStyle.spacing_4

            spacing: VLCStyle.spacing_4

            // the folder of the playing file, in the library
            Widgets.ButtonExt {
                id: folderButton

                readonly property string folderName: Helpers.folderName(Player.url)

                Layout.maximumWidth: parent.width - queueButton.width - parent.spacing
                                     - (presetButton.visible ? presetButton.width + parent.spacing : 0)

                visible: folderName !== ""

                iconTxt: VLCIcons.folder
                text: folderName

                colorContext.colorSet: ColorContext.ToolButton

                Accessible.name: qsTr("Folder %1").arg(folderName)

                T.ToolTip.text: qsTr("Show %1 in the library").arg(folderName)

                Navigation.parentItem: root
                Navigation.upItem: playButton
                Navigation.rightItem: presetButton.visible ? presetButton : queueButton

                onClicked: LibraryPages.showFolder(mediaFolder.folderId, Player.hasVideoOutput,
                                                   Qt.MouseFocusReason)

                MLMediaFolder {
                    id: mediaFolder

                    ml: MainCtx.mediaLibrary
                    url: folderButton.visible ? Player.url : ""
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // the Preset of the playing file's Show, as in the sidebar footer
            PresetSelector {
                id: presetButton

                Layout.maximumWidth: VLCStyle.dp(120, VLCStyle.scale)

                visible: Presets.folder !== ""

                Navigation.parentItem: root
                Navigation.upItem: playButton
                Navigation.leftItem: folderButton.visible ? folderButton : null
                Navigation.rightItem: queueButton
            }

            Widgets.IconToolButton {
                id: queueButton

                // Figma "lists" (217:191), as the Control bar's
                text: checked ? VLCIcons.lists_active : VLCIcons.lists
                description: qsTr("Queue")

                checked: MainCtx.playqueuePanel.visible

                Navigation.parentItem: root
                Navigation.upItem: playButton
                Navigation.leftItem: presetButton.visible ? presetButton : (folderButton.visible ? folderButton : null)

                onClicked: MainCtx.playqueuePanel.visible = !MainCtx.playqueuePanel.visible
            }
        }
    }
}
