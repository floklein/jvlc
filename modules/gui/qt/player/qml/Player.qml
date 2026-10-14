/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
import QtQml.Models
import QtQuick.Window
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player
import VLC.PlayerControls
import VLC.Dialogs
import VLC.Util

FocusScope {
    id: rootPlayer

    // Properties

    //behave like a Page
    property var pagePrefix: []

    readonly property int positionSliderY: controlBar.y + controlBar.sliderY

    // the Track menu, opened by the buttons of the Control bar
    readonly property alias trackMenu: trackMenu

    readonly property string coverSource: {
        if (Player.artwork &&
            Player.artwork.toString())
            return Player.artwork
        else if (Player.hasVideoOutput)
            return VLCStyle.noArtVideoCover
        else
            return VLCStyle.noArtAlbumCover
    }

    // Only applicable in video mode:
    property bool displayFadeRectangles: !_controlsUnderVideo && (height > (implicitFadeRectangleHeight * 3))
    property real implicitFadeRectangleHeight: VLCStyle.dp(206, VLCStyle.scale)

    // Private

    // The bars are docked above and below the video in a window. In fullscreen and
    // in Hide UI they float over the video and are revealed from the edges.
    property bool _controlsUnderVideo: (MainCtx.intfMainWindow.visibility !== Window.FullScreen)
                                       && !MainCtx.hideUI

    // the pointer reveals the bars within this distance of the top or bottom edge
    readonly property real _edgeZone: VLCStyle.dp(64, VLCStyle.scale)

    property bool _keyPressed: false

    // what the arrow up reaches from the Control bar (or the
    // continue-watching pill above it)
    readonly property Item _itemAboveControlBar: {
        if (rightPanel.adjustmentsOpened)
            return adjustmentsPanel
        if (playlistVisibility.isPlaylistVisible)
            return playlistpopup
        if (Player.isInteractive)
            return toggleControlBarButton
        if (!MainCtx.hasEmbededVideo)
            return playerSpecializationLoader
        return topBar
    }

    // Settings

    Accessible.role: Accessible.Client
    Accessible.name: qsTr("Player")
    AccessibleCompat.id: "playerView"

    // Events

    Component.onCompleted: {
        MainCtx.preferHotkeys = true

        // the adjustments were asked for from the library
        const pending = MainCtx.takePendingAdjustments()
        if (pending === MainCtx.AdjustmentsLastTab)
            rightPanel.toggleAdjustments()
        else if (pending >= 0)
            rightPanel.openAdjustments(pending)
    }
    Component.onDestruction: {
        MainCtx.preferHotkeys = false
        MainCtx.adjustmentsOpened = false
    }

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => {
        if (event.accepted)
            return

        _keyPressed = true

        rootPlayer.Navigation.defaultKeyAction(event)

        //unhandled keys are forwarded as hotkeys
        if (!event.accepted || controlBar.state !== "visible")
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    Keys.onReleased: (event) => {
        if (event.accepted || _keyPressed === false)
            return

        _keyPressed = false

        rootPlayer.Navigation.defaultKeyReleaseAction(event)
    }

    // Functions

    function lockUnlockAutoHide(lock) {
        if (lock) {
            playerToolbarVisibilityFSM.lock()
        } else {
            playerToolbarVisibilityFSM.unlock()
        }
    }

    // Private

    function _onNavigationCancel() {
        if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP) {
            MainPlaylistController.stop()
        }

        History.previous()
    }

    // The right-hand panel slides over the video in fullscreen and Hide UI,
    // and is docked beside it in a window
    readonly property bool _rightPanelOverlaid: (MainCtx.intfMainWindow.visibility === Window.FullScreen)
                                                || MainCtx.hideUI
    readonly property bool _rightPanelDocked: !_rightPanelOverlaid
                                              && (playlistpopup.visible || adjustmentsPanel.visible)

    // the panel in the right-hand slot, the one sliding in while they swap
    readonly property Item _rightPanel: adjustmentsPanel.visible
                                        && (rightPanel.adjustmentsOpened || !playlistpopup.visible)
                                        ? adjustmentsPanel : playlistpopup

    // the Track menu tabs (TrackMenuFSM.Tab) in the sidebar
    function _adjustmentsTab(trackMenuTab) {
        switch (trackMenuTab) {
        case TrackMenuFSM.Video:
            return RightPanelFSM.Video
        case TrackMenuFSM.Audio:
            return RightPanelFSM.Audio
        default:
            return RightPanelFSM.Subtitles
        }
    }

    function _isInEdgeZone(y) {
        return (y < Math.max(_edgeZone, topBar.height))
                || (y > height - Math.max(_edgeZone, controlBar.height))
    }

    // Esc in the bars: back to the video
    function _focusVideo() {
        playerToolbarVisibilityFSM.dismiss()
        playerSpecializationLoader.forceActiveFocus(Qt.OtherFocusReason)
    }

    // Whether the bars sit over the video on a Tint
    readonly property bool _panelsOverVideo: MainCtx.hasEmbededVideo
                                             && !_controlsUnderVideo
                                             && !displayFadeRectangles

    //we draw both the view and the window here
    ColorContext {
        id: windowTheme

        // NOTE: We force the dark theme when the controls are drawn right over the
        //       video scrims. Panels over the video use the Theme's Tint.
        palette: (MainCtx.hasEmbededVideo && rootPlayer.displayFadeRectangles)
                 ? VLCStyle.darkPalette
                 : VLCStyle.palette

        colorSet: ColorContext.Window
    }

    // scrims darkening the video under the controls
    ColorContext {
        id: scrimTheme

        palette: VLCStyle.darkPalette
        colorSet: ColorContext.View
    }

    BarRevealFSM {
        id: playerToolbarVisibilityFSM

        overlay: MainCtx.hasEmbededVideo && !rootPlayer._controlsUnderVideo
        paused: Player.playingState === Player.PLAYING_STATE_PAUSED
        interactive: Player.isInteractive

        onForceUnlock:{
            controlBar.forceUnlock()

            topBar.forceUnlock()

            trackMenu.close()
        }

        onIsVisibleChanged: {
            // the keyboard focus does not stay in hidden bars
            if (!isVisible && MainCtx.hasEmbededVideo
                    && (controlBar.activeFocus || topBar.activeFocus))
                playerSpecializationLoader.forceActiveFocus(Qt.OtherFocusReason)
        }
    }

    PlayerPlaylistVisibilityFSM {
        id: playlistVisibility

        onIsPlaylistVisibleChanged: {
            lockUnlockAutoHide(isPlaylistVisible)
            if (isPlaylistVisible)
                rightPanel.queueOpened()
        }
    }

    // The Queue panel and the Adjustments sidebar share the right-hand slot:
    // only one of them is open at a time
    RightPanelFSM {
        id: rightPanel

        queueVisible: playlistVisibility.isPlaylistVisible
        music: Player.isStarted && !Player.hasVideoOutput

        // the sidebar opens on its last tab, also after a restart
        tab: Number(MainCtx.settingValue("MainWindow/adjustments-tab", RightPanelFSM.Playback))
        onTabChanged: MainCtx.setSettingValue("MainWindow/adjustments-tab", tab)

        onRequestCloseQueue: playlistVisibility.togglePlaylistVisibility()

        onAdjustmentsOpenedChanged: {
            rootPlayer.lockUnlockAutoHide(adjustmentsOpened)
            MainCtx.adjustmentsOpened = adjustmentsOpened
        }
    }

    Connections {
        target: MainCtx

        function onAdjustmentsToggled() {
            rightPanel.toggleAdjustments()
        }
        function onAdjustmentsRequested(tab) {
            rightPanel.openAdjustments(tab)
        }
    }

    Connections {
        target: MainCtx

        //playlist
        function onHasEmbededVideoChanged() {
            playlistVisibility.updateVideoEmbed()
        }
        function onAskShow() {
            playerToolbarVisibilityFSM.askShow()
        }
    }


    Connections {
        target: MainCtx.playqueuePanel

        //playlist
        function onDockedChanged() {
            playlistVisibility.updatePlaylistDocked()
        }
        function onVisibleChanged() {
            playlistVisibility.updatePlaylistVisible()
        }
    }


    Loader {
        id: playerSpecializationLoader

        objectName: "playerSpecializationLoader"

        anchors {
            left: parent.left
            // a docked right-hand panel shrinks the video
            right: rootPlayer._rightPanelDocked ? rootPlayer._rightPanel.left : parent.right
            rightMargin: rootPlayer._rightPanelDocked ? VLCStyle.spacing_20 : 0
            top: (MainCtx.hasEmbededVideo && rootPlayer._controlsUnderVideo) ? topBar.bottom : parent.top
            bottom: (MainCtx.hasEmbededVideo && rootPlayer._controlsUnderVideo) ? controlBar.top : parent.bottom
        }

        sourceComponent: MainCtx.hasEmbededVideo ? videoComponent : audioComponent

        property int cursorShape

        // Have padding here, so that the content (unlike background) does not go behind the top bar or the control bar:
        property real topPadding: (anchors.top === parent.top) ? topBar.height : 0
        property real bottomPadding: (anchors.bottom === parent.bottom) ? controlBar.height : 0

        component TouchDragHandler : DragHandler {
            id: touchDragHandler

            acceptedDevices: PointerDevice.TouchScreen

            minimumPointCount: 1
            maximumPointCount: 1

            target: null

            dragThreshold: VLCStyle.margin_normal

            property vector2d lastActiveTranslation

            // TODO: Use `createComponent()` and load directly from module instead, since
            //       it is pointless to keep the `QQmlComponent` around for this object.
            readonly property Component wheelToVLCConverterComponent: Component { WheelToVLCConverter { } }

            property WheelToVLCConverter wheelToVLCConverter

            // We synthesize wheel event with pixel delta. It is a to-do to handle
            // touch properly with precision adjustment, like on Android:
            onActiveTranslationChanged: {
                if (active) {
                    console.assert(wheelToVLCConverter)

                    const delta = Qt.point(activeTranslation.x - lastActiveTranslation.x,
                                           activeTranslation.y - lastActiveTranslation.y)

                    // WheelToVLCConverter can not handle sole pixel delta now, so we also
                    // use angle delta. This is similar to wheel events on some platforms:
                    wheelToVLCConverter.customWheelEvent(delta,
                                                         delta,
                                                         Qt.NoButton,
                                                         Qt.NoModifier,
                                                         true)

                    lastActiveTranslation = activeTranslation
                }
            }

            onActiveChanged: {
                if (active) {
                    if (!wheelToVLCConverter) {
                        wheelToVLCConverter = wheelToVLCConverterComponent.createObject(touchDragHandler)
                        wheelToVLCConverter.vlcWheelKey.connect(MainCtx, MainCtx.sendVLCHotkey)
                    }
                } else {
                    lastActiveTranslation = Qt.vector2d(0, 0)
                }
            }
        }

        Component {
            id: videoComponent

            FocusScope {
                // Video

                focus: true

                VideoSurface {
                    id: videoSurface

                    anchors.fill: parent

                    // With regard to `ViewBlockingRectangle`, we
                    // do not need to prevent painting anything
                    // behind because there is already nothing
                    // behind (unlike pip player):
                    renderingEnabled: false

                    videoSurfaceProvider: MainCtx.videoSurfaceProvider

                    visible: MainCtx.hasEmbededVideo

                    focus: true

                    cursorShape: playerSpecializationLoader.cursorShape

                    // x and y are in device pixels
                    function onMouseMove(x, y) {
                        //short interval for mouse events
                        if (Player.isInteractive)
                            interactiveAutoHideTimer.restart()

                        const dpr = MainCtx.effectiveDevicePixelRatio(Window.window)
                        const pos = videoSurface.mapToItem(rootPlayer, x / dpr, y / dpr)

                        playerToolbarVisibilityFSM.pointerMoved(rootPlayer._isInEdgeZone(pos.y))
                    }

                    function onMouseButton() {
                        if (Player.isInteractive)
                            interactiveAutoHideTimer.restart()

                        playerToolbarVisibilityFSM.pointerMoved(playerToolbarVisibilityFSM.pointerInZone)
                    }

                    Component.onCompleted: {
                        mouseMoved.connect(videoSurface.onMouseMove)
                        mousePressed.connect(videoSurface.onMouseButton)
                        mouseReleased.connect(videoSurface.onMouseButton)
                        mouseDblClicked.connect(videoSurface.onMouseButton)
                    }

                    // A single click pauses or resumes. The double click
                    // toggles fullscreen in the core (video_window.c).
                    // Interactive media (DVD menus) use the clicks.
                    ClickGesture {
                        id: clickGesture

                        enabled: !Player.isInteractive

                        // mouseMoved reports device pixels
                        dragThreshold: Qt.styleHints.startDragDistance * Screen.devicePixelRatio

                        onSingleClicked: MainPlaylistController.togglePlayPause()
                    }

                    onMouseMoved: (x, y) => clickGesture.move(x, y)
                    onMousePressed: (button) => {
                        if (button === 0) // left
                            clickGesture.press()
                    }
                    onMouseReleased: (button) => {
                        if (button === 0)
                            clickGesture.release()
                    }
                    onMouseDblClicked: (button) => {
                        if (button === 0)
                            clickGesture.doubleClick()
                    }

                    // the pointer hides when idle over the video, in fullscreen and Hide UI
                    Binding on cursorShape {
                        when: playerToolbarVisibilityFSM.started
                            && playerToolbarVisibilityFSM.pointerHidden
                            && !interactiveAutoHideTimer.running
                        value: Qt.BlankCursor
                    }

                    TouchDragHandler {

                    }
                }

                component FadeRectangle : Rectangle {
                    implicitHeight: rootPlayer.implicitFadeRectangleHeight

                    SGManipulator {
                        // The rectangle is the bottom-most item in the interface,
                        // it does not need blending even though it is not opaque.
                        // Since `Rectangle` does not provide explicit control over
                        // blending, we are using `SGManipulator`:
                        blending: false
                    }
                }

                FadeRectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right

                    opacity: topBar.opacity
                    visible: rootPlayer.displayFadeRectangles && !topBarAcrylicBg.visible

                    gradient: Gradient {
                        GradientStop { position: 0; color: scrimTheme.bg.secondary.alpha(.8) }
                        GradientStop { position: 1; color: scrimTheme.bg.secondary.alpha(0) }
                    }
                }

                FadeRectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right

                    opacity: controlBar.opacity

                    gradient: Gradient {
                        GradientStop { position: 0; color: scrimTheme.bg.secondary.alpha(0) }
                        GradientStop { position: .64; color: scrimTheme.bg.secondary.alpha(.8) }
                        GradientStop { position: 1; color: scrimTheme.bg.secondary }
                    }

                    visible: rootPlayer.displayFadeRectangles && !(controlBar.background && controlBar.background.visible)
                }
            }
        }

        Component {
            id: audioComponent

            FocusScope {
                id: audioFocusScope
                // Audio

                focus: true

                property real topPadding: playerSpecializationLoader.topPadding
                property real bottomPadding: playerSpecializationLoader.bottomPadding

                // Whether the full lyrics view is active
                readonly property bool lyricsMode: MainCtx.lyricsMode
                readonly property bool lyricsLayoutActive: lyricsMode
                                                        && Player.hasLyrics
                                                        && centerContent.width > VLCStyle.colWidth(6)

                // Reset sync when entering lyrics mode
                onLyricsModeChanged: {
                    if (lyricsMode && lyricsLoader.item)
                        lyricsLoader.item.lyricsSyncToPlayback = true
                }

                // background image
                Widgets.DualKawaseBlur {
                    id: blurredBackground

                    // Six pass is free here since we release the intermediate layers
                    // through unsetting `live` (see `liveTimer`):
                    mode: Widgets.DualKawaseBlur.Mode.SixPass
                    radius: 2

                    // We disable live when everything settles through `liveTimer`,
                    // which is important to release intermediate layers with static
                    // source:
                    live: true

                    //destination aspect ratio
                    readonly property real dar: parent.width / parent.height

                    anchors.centerIn: parent
                    width: (cover.sar < dar) ? parent.width :  parent.height * cover.sar
                    height: (cover.sar < dar) ? parent.width / cover.sar :  parent.height

                    source: textureProviderIndirection

                    postprocess: true
                    tint: bgtheme.bg.secondary
                    tintStrength: 0.5
                    backgroundColor: bgtheme.bg.primary

                    readonly property ColorContext colorContext: ColorContext {
                        id: bgtheme
                        colorSet: ColorContext.View
                    }

                    // The window naturally clips the content, but having this saves some
                    // video memory, depending on the excess content in the last layer:
                    viewportRect: Qt.rect((width - parent.width) / 2, (height - parent.height) / 2, parent.width, parent.height)

                    sourceTextureProviderObserver.onTextureChanged: {
                        liveTimer.transientTurnOnLive()
                    }

                    TextureProviderIndirection {
                        id: textureProviderIndirection

                        // This should not be necessary anymore since `DualKawaseBlur`
                        // does not create layer for the source implicitly as `MultiEffect`
                        // or `FastBlur` does as they deem necessary (in this case, it
                        // is not necessary). But due to a Qt bug when `mipmap: true` is
                        // used where texture sampling becomes broken, we need this as
                        // `QSGTextureView` has a workaround for that bug. This is totally
                        // acceptable as there is virtually no overhead.
                        source: cover
                    }

                    Component.onCompleted: {
                        // Blur layers are effect-size dependent, so once the user starts resizing the window (hence the effect),
                        // we should either momentarily turn on live, or repeatedly call `scheduleUpdate()`. Due to the optimization,
                        // calling `scheduleUpdate()` would continuously create and release intermediate layers, which would be a
                        // really bad idea. So instead, we turn on live and after some time passes turn it off again.
                        widthChanged.connect(liveTimer, liveTimer.transientTurnOnLive)
                        heightChanged.connect(liveTimer, liveTimer.transientTurnOnLive)

                        // In case texture provider observer never signals texture change:
                        liveTimer.transientTurnOnLive()
                    }

                    Timer {
                        id: liveTimer

                        repeat: false
                        interval: VLCStyle.duration_humanMoment

                        function transientTurnOnLive() {
                            if (!blurredBackground.sourceTextureIsValid)
                                return
                            blurredBackground.live = true
                            liveTimer.restart()
                        }

                        onTriggered: {
                            blurredBackground.live = false
                        }
                    }
                }

                MouseArea {
                    id: centerContent

                    readonly property ColorContext colorContext: ColorContext {
                        id: centerTheme
                        colorSet: ColorContext.View
                    }

                    anchors.fill: parent
                    anchors.topMargin: VLCStyle.margin_xsmall + audioFocusScope.topPadding
                    anchors.bottomMargin: VLCStyle.margin_xsmall + audioFocusScope.bottomPadding

                    onWheel: (wheel) => {
                        wheel.accepted = true
                        wheelToVlc.qmlWheelEvent(wheel)
                    }

                    TouchDragHandler {
                        wheelToVLCConverter: wheelToVlc
                    }

                    WheelToVLCConverter {
                        id: wheelToVlc

                        onVlcWheelKey: (key) => MainCtx.sendVLCHotkey(key)
                    }

                    // ── Two-panel lyrics layout ────────────────────────
                    Item {
                        id: leftSideParent

                        visible: (opacity > 0.0) || (width > 0.0)

                        opacity: (lyricsLoader.item ? 1.0 : 0.0)

                        Behavior on opacity {
                            id: opacityBehavior

                            enabled: false

                            NumberAnimation {
                                duration: VLCStyle.duration_long

                                easing.type: Easing.InOutSine
                            }

                            Component.onCompleted: {
                                // The animation should not be used at initialization:
                                Qt.callLater(() => { opacityBehavior.enabled = true })
                            }
                        }

                        anchors.top: parent.top
                        anchors.topMargin: VLCStyle.margin_large
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left

                        width: lyricsLoader.item ? (parent.width * 0.6) : 0.0

                        Behavior on width {
                            id: widthBehavior

                            enabled: false

                            NumberAnimation {
                                duration: VLCStyle.duration_long

                                easing.type: Easing.InOutSine
                            }

                            Component.onCompleted: {
                                // The animation should not be used at initialization:
                                Qt.callLater(() => { widthBehavior.enabled = true })
                            }
                        }

                        Loader {
                            id: lyricsLoader

                            active: audioFocusScope.lyricsLayoutActive
                            sourceComponent: T.Pane {
                                // Swallow wheel events so scrolling the lyrics
                                // does not bubble up to the player and change
                                // the volume.
                                wheelEnabled: true

                                // Expose the inner Flickable's sync flag so the
                                // Loader's `item.lyricsSyncToPlayback` reaches
                                // through to LyricsFlickable in both directions.
                                property alias lyricsSyncToPlayback: lyricsFlickable.lyricsSyncToPlayback

                                implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                                        implicitContentWidth + leftPadding + rightPadding)
                                implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                                         implicitContentHeight + topPadding + bottomPadding)

                                contentItem: LyricsFlickable {
                                    id: lyricsFlickable

                                    onPlayerPositionChangeRequested: (time) => {
                                        Player.setTime(time)
                                        lyricsFlickable.lyricsSyncToPlayback = true
                                    }
                                }
                            }
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter

                            height: Math.min(implicitHeight, parent.height)
                        }
                    }

                    Item {
                        id: rightSideParent

                        visible: leftSideParent.visible

                        anchors.top: parent.top
                        anchors.topMargin: VLCStyle.margin_large
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.left: leftSideParent.right
                    }

                    ColumnLayout {
                        id: coverColumn

                        parent: leftSideParent.visible ? rightSideParent : centerContent

                        anchors.centerIn: parent
                        spacing: 0

                        // The music player (Figma 4767:23726): a large cover with
                        // rounded corners, then "title - artist"
                        Item {
                            id: coverItem
                            Layout.preferredHeight: Math.min(centerContent.height * heightRatio,
                                                             coverColumn.parent.width - VLCStyle.margin_xlarge * 2)
                            Layout.preferredWidth: cover.paintedWidth
                            Layout.maximumHeight: centerContent.height
                            Layout.alignment: Qt.AlignHCenter

                            // the cover takes three quarters of the view, less
                            // when the current lyric shows under it
                            readonly property real heightRatio: (Player.hasLyrics && !audioFocusScope.lyricsLayoutActive) ? 0.6 : 0.75

                            Image {
                                id: cover

                                //source aspect ratio
                                readonly property real sar: paintedWidth / paintedHeight
                                readonly property int maximumWidth: Helpers.alignUp((Screen.desktopAvailableWidth * eDPR * 0.75), 32)
                                readonly property int maximumHeight: Helpers.alignUp((Screen.desktopAvailableHeight * eDPR * 0.75), 32)

                                readonly property int maximumSize: Math.min(maximumWidth, maximumHeight)

                                readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

                                readonly property url targetSource: VLCAccessImage.uri(rootPlayer.coverSource)

                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: targetSource
                                fillMode: Image.PreserveAspectFit
                                mipmap: true
                                cache: false
                                asynchronous: true

                                onTargetSourceChanged: {
                                    cover.source = targetSource
                                }

                                onSourceChanged: {
                                    liveTimer.transientTurnOnLive()
                                }

                                onStatusChanged: {
                                    if (status === Image.Error) {
                                        cover.source = VLCStyle.noArtAlbumCover
                                    }
                                }

                                sourceSize: Qt.size(maximumSize, maximumSize)

                                // still sizes the cover and feeds the blurred background,
                                // while the rounded cover below draws it
                                visible: !roundedCover.visible

                                Accessible.role: Accessible.Graphic
                                Accessible.name: qsTr("Cover")

                                Component.onCompleted: {
                                    // After the update on source change, there can be another update when the mipmaps are generated.
                                    // We intentionally do not wait for this, initially using non-mipmapped source should be okay. As
                                    // the user should not be greeted with a black background until the mipmaps are ready, let alone
                                    // the possibility of knowing if the mipmaps are actually going to be ready as expected.
                                    // If the texture is not valid yet (which is signalled the latest), blur effect is going to queue
                                    // an update itself similar to the case when the source itself changes, so we do not check validity
                                    // of the texture here.
                                    blurredBackground.sourceTextureProviderObserver.hasMipmapsChanged.connect(blurredBackground,
                                                                                                              (hasMipmaps /*: bool */) => {
                                                                                                                  if (hasMipmaps) {
                                                                                                                      blurredBackground.scheduleUpdate()
                                                                                                                  }
                                                                                                              })
                                }
                            }

                            // Sampling the mipmapped `cover` texture here draws it
                            // squeezed, so this loads its own copy
                            Widgets.ImageExt {
                                id: roundedCover

                                anchors.fill: cover

                                visible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

                                source: cover.source
                                sourceSize: cover.sourceSize
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                radius: VLCStyle.dp(20, VLCStyle.scale)

                                // a faint glow around the cover
                                Widgets.RoundedRectangleShadow {
                                    color: bgtheme.fg.primary.alpha(.05)
                                    blurRadius: VLCStyle.dp(90, VLCStyle.scale)
                                }
                            }
                        }

                        Widgets.SubtitleLabel {
                            id: titleLabel

                            Layout.alignment: Qt.AlignHCenter
                            Layout.maximumWidth: coverColumn.parent.width - VLCStyle.margin_xlarge * 2
                            Layout.topMargin: Math.round(centerContent.height * 0.074)

                            Binding on visible {
                                delayed: true
                                when: titleLabel.componentCompleted
                                value: centerContent.height > (titleLabel.y + titleLabel.height)
                            }

                            // "title - artist"
                            text: {
                                const title = Player.title || Player.name
                                return Player.artist ? qsTr("%1 - %2").arg(title).arg(Player.artist)
                                                     : title
                            }
                            elide: Text.ElideRight
                            font.pixelSize: VLCStyle.fontSize_xxlarge
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            color: centerTheme.fg.secondary
                            Accessible.description: qsTr("title and artist")

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }
                        }

                        Widgets.SubtitleLabel {
                            id: lyricsLabel

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_large
                            // Pin width and height so the surrounding centered
                            // column does not shift when the current lyric
                            // changes between empty / 1 line / 2 lines. Text
                            // exceeding two lines wraps then elides on line 2.
                            Layout.preferredWidth: parent.parent.width - VLCStyle.margin_xlarge * 2
                            Layout.preferredHeight: (lyricsLabelTextMetrics.height * 2) + VLCStyle.margin_xxxsmall

                            visible: false

                            Binding on visible {
                                delayed: true
                                when: lyricsLabel.componentCompleted

                                // Stay visible (reserving the 2-line slot) for the
                                // entire track when it has any lyrics, so the
                                // layout does not collapse between lyric lines.
                                value: Player.hasLyrics && !audioFocusScope.lyricsLayoutActive &&
                                       (centerContent.height > (lyricsLabel.y + lyricsLabel.height))
                            }

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }

                            elide: Text.ElideRight

                            text: Player.currentLyric.text
                            font.pixelSize: VLCStyle.fontSize_xlarge
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.WordWrap
                            color: centerTheme.fg.primary

                            // TODO: Do not use `Binding` when minimum Qt is 6.12:
                            Binding {
                                target: lyricsLabel
                                property: "mutabilityGroup"
                                when: (lyricsLabel.mutabilityGroup !== undefined)
                                value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.ModerateMutabilityGroup
                                                                                              : Item.StaticMutabilityGroup
                            }

                            TextMetrics {
                                id: lyricsLabelTextMetrics
                                font: lyricsLabel.font
                                text: "TEXT"
                            }

                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("Lyrics")
                            Accessible.description: text
                        }

                        // "Sync to Playback" checkbox pinned to bottom of right panel
                        Widgets.CheckBoxExt {
                            id: syncToPlaybackCheckBox

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: VLCStyle.margin_small

                            visible: false

                            Binding on visible {
                                delayed: true
                                when: syncToPlaybackCheckBox.componentCompleted
                                value: audioFocusScope.lyricsLayoutActive &&
                                       centerContent.height > (syncToPlaybackCheckBox.y + syncToPlaybackCheckBox.height)
                            }

                            property bool componentCompleted: false

                            Component.onCompleted: {
                                componentCompleted = true
                            }

                            text: qsTr("Sync lyrics to playback")
                            // Use a Binding element rather than an inline binding
                            // so manual clicks (which write `checked` directly)
                            // don't permanently break the source-of-truth link.
                            Binding on checked {
                                when: lyricsLoader.item
                                value: lyricsLoader.item ? lyricsLoader.item.lyricsSyncToPlayback : true
                                restoreMode: Binding.RestoreBindingOrValue
                            }
                            onClicked: {
                                if (lyricsLoader.item)
                                    lyricsLoader.item.lyricsSyncToPlayback = checked
                            }

                            Accessible.role: Accessible.CheckBox
                            Accessible.name: qsTr("Sync lyrics to playback position")
                            AccessibleCompat.id: "syncToPlaybackCheckBox"

                            Navigation.parentItem: rootPlayer
                            Navigation.upItem: topBar
                            Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar
                        }
                    }

                }
            }
        }
    }

    // Video blur beneath the bars while they float over the video. It paints
    // nothing in the scene: the compositor draws it under the interface.
    component BarVideoBlur : VideoBlur {
        required property Item bar
        required property bool overVideo
        // the bar's reveal slide
        required property Translate slide

        x: bar.x
        y: bar.y + slide.y
        width: bar.width
        height: bar.height
        opacity: bar.opacity

        visible: bar.visible && overVideo

        provider: MainCtx.videoBlurProvider
    }

    BarVideoBlur {
        bar: topBar
        slide: topBarReveal.translate
        overVideo: MainCtx.hasEmbededVideo && !rootPlayer._controlsUnderVideo
    }

    BarVideoBlur {
        bar: controlBar
        slide: controlBarReveal.translate
        overVideo: MainCtx.hasEmbededVideo && !rootPlayer._controlsUnderVideo
    }

    TopBar {
        id: topBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        z: 1

        topMargin: VLCStyle.applicationVerticalMargin
        sideMargin: VLCStyle.applicationHorizontalMargin

        focus: true

        // the Layout of the player shown, like the Control bar
        identifier: controlBar.identifier

        pinControls: rootPlayer._controlsUnderVideo

        transform: topBarReveal.translate

        showCSD: MainCtx.clientSideDecoration && (MainCtx.intfMainWindow.visibility !== Window.FullScreen)

        Navigation.parentItem: rootPlayer
        Navigation.cancelAction: MainCtx.hasEmbededVideo ? rootPlayer._focusVideo : null
        Navigation.downItem: {
            if (rightPanel.adjustmentsOpened)
                return adjustmentsPanel
            if (playlistVisibility.isPlaylistVisible)
                return playlistpopup
            if (MainCtx.hasEmbededVideo)
                return playerSpecializationLoader
            if (Player.isInteractive)
                return toggleControlBarButton
            return controlBar
        }

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playerToolbarVisibilityFSM.started
            value: (playerToolbarVisibilityFSM.isVisible || rootPlayer._controlsUnderVideo) ? "visible" : "hidden"
        }

        onRequestLockUnlockAutoHide: (lock) => {
            rootPlayer.lockUnlockAutoHide(lock)
        }

        BarRevealStateGroup {
            id: topBarReveal

            target: topBar
            edge: Qt.TopEdge
            animate: playerToolbarVisibilityFSM.animate
            reducedMotion: MainCtx.reduceMotion
        }

        // TODO: Make TopBar a Control and use background
        Widgets.AcrylicBackground {
            id: topBarAcrylicBg

            z: -1

            anchors.fill: parent

            opacity: {
                if (MainCtx.intfMainWindow.visibility === Window.FullScreen && MainCtx.hasEmbededVideo) {
                    return MainCtx.pinOpacity
                } else {
                    return 1.0
                }
            }

            tintColor: windowTheme.bg.primary

            visible: MainCtx.pinVideoControls && !MainCtx.hasEmbededVideo
        }

        // docked above the video: the Theme's window background (Figma BG/A3)
        Rectangle {
            z: -1

            anchors.fill: parent

            color: windowTheme.bg.primary
            opacity: topBarAcrylicBg.opacity

            visible: rootPlayer._controlsUnderVideo && MainCtx.hasEmbededVideo
        }

        Rectangle {
            z: -1

            anchors.fill: parent

            color: windowTheme.tint

            visible: rootPlayer._panelsOverVideo
        }
    }

    // top center of the video, below the Top bar while it shows
    ShortcutMessagePill {
        anchors.horizontalCenter: parent.horizontalCenter

        y: ((topBar.visible && topBar.state !== "hidden") ? topBar.y + topBar.height : 0)
           + VLCStyle.margin_normal

        z: 2
    }

    // The right-hand panel: the Queue panel (Figma 491:1781), 400 px, from
    // under the Top bar to above the Control bar
    Loader {
        id: playlistpopup

        anchors {
            top: topBar.bottom
            right: parent.right
            bottom: parent.bottom

            bottomMargin: parent.height - rootPlayer.positionSliderY + VLCStyle.spacing_20
        }

        width: VLCStyle.sidePanel_width

        focus: false

        active: MainCtx.playqueuePanel.docked

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playlistVisibility.started
            value: (status === Loader.Ready && playlistVisibility.isPlaylistVisible) ? "expanded" : "retracted"
        }

        Component.onCompleted: {
            Qt.callLater(() => { playlistTransition.enabled = true; })
        }

        // the Queue button opens the panel on the Queue tab
        onStateChanged: {
            if (state === "expanded" && item)
                item.currentTab = QueuePanel.Tab.Queue
        }

        states: [
            State {
                name: "expanded"
                PropertyChanges {
                    target: playlistpopup
                    anchors.rightMargin: VLCStyle.spacing_20
                    visible: true
                }
           }, State {
                name: "retracted"
                PropertyChanges {
                    target: playlistpopup
                    anchors.rightMargin: - (playlistpopup.width + VLCStyle.spacing_20)
                    visible: false
                }
           }
        ]

        // slides in and out, 150 ms
        transitions: Transition {
            id: playlistTransition
            enabled: false

            from: "retracted"; to: "expanded";
            reversible: true

            SequentialAnimation {
                PropertyAction { property: "visible" }

                NumberAnimation {
                    property: "anchors.rightMargin"
                    duration: VLCStyle.duration_short * 1.5
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.23, 1, 0.32, 1, 1, 1]
                }
            }
        }

        sourceComponent: QueuePanel {
            id: queuePanel

            height: playlistpopup.height
            width: playlistpopup.width

            focus: true

            wheelEnabled: true

            // the Tint over the video, the view background when docked
            background: Rectangle {
                radius: VLCStyle.radius_large
                color: rootPlayer._rightPanelOverlaid ? windowTheme.tint : queuePanel.colorContext.bg.primary
            }

            Navigation.parentItem: rootPlayer
            Navigation.upItem: topBar
            Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar
            Navigation.leftAction: closePlaylist
            Navigation.cancelAction: closePlaylist

            function closePlaylist() {
                playlistVisibility.togglePlaylistVisibility()
                controlBar.forceActiveFocus()
            }
        }
    }

    // The Adjustments sidebar (Figma 2238:9432), in the same slot as the
    // Queue panel and sliding the same way
    Loader {
        id: adjustmentsPanel

        anchors {
            top: topBar.bottom
            right: parent.right
            bottom: parent.bottom

            bottomMargin: parent.height - rootPlayer.positionSliderY + VLCStyle.spacing_20
        }

        width: VLCStyle.sidePanel_width

        focus: false

        // loaded while shown, and while it slides out
        active: rightPanel.adjustmentsOpened || visible

        state: rightPanel.adjustmentsOpened ? "expanded" : "retracted"

        Component.onCompleted: {
            Qt.callLater(() => { adjustmentsTransition.enabled = true; })
        }

        states: [
            State {
                name: "expanded"
                PropertyChanges {
                    target: adjustmentsPanel
                    anchors.rightMargin: VLCStyle.spacing_20
                    visible: true
                }
           }, State {
                name: "retracted"
                PropertyChanges {
                    target: adjustmentsPanel
                    anchors.rightMargin: - (adjustmentsPanel.width + VLCStyle.spacing_20)
                    visible: false
                }
           }
        ]

        // slides in and out, 150 ms
        transitions: Transition {
            id: adjustmentsTransition
            enabled: false

            from: "retracted"; to: "expanded";
            reversible: true

            SequentialAnimation {
                PropertyAction { property: "visible" }

                NumberAnimation {
                    property: "anchors.rightMargin"
                    duration: MainCtx.reduceMotion ? 0 : VLCStyle.duration_short * 1.5
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: [0.23, 1, 0.32, 1, 1, 1]
                }
            }
        }

        sourceComponent: AdjustmentsSidebar {
            id: adjustmentsSidebar

            height: adjustmentsPanel.height
            width: adjustmentsPanel.width

            focus: true

            fsm: rightPanel

            // the Tint over the video, the view background when docked
            background: Rectangle {
                radius: VLCStyle.radius_large
                color: rootPlayer._rightPanelOverlaid ? windowTheme.tint : adjustmentsSidebar.colorContext.bg.primary
            }

            Navigation.parentItem: rootPlayer
            Navigation.upItem: topBar
            Navigation.downItem: Player.isInteractive ? toggleControlBarButton : controlBar
            Navigation.cancelAction: closeAdjustments

            function closeAdjustments() {
                rightPanel.closeAdjustments()
                controlBar.forceActiveFocus()
            }
        }
    }

    // "Show Statistics" of the Playback tab
    Loader {
        anchors {
            top: topBar.bottom
            left: parent.left
            topMargin: VLCStyle.spacing_20
            leftMargin: VLCStyle.spacing_20 + VLCStyle.applicationHorizontalMargin
        }

        active: MainCtx.showStatistics && Player.isStarted

        sourceComponent: StatisticsOverlay { }
    }

    Dialogs {
        z: 10
        bgContent: rootPlayer

        anchors {
            bottom: controlBar.visible ? controlBar.top : rootPlayer.bottom
            left: parent.left
            right: parent.right

            bottomMargin: (rootPlayer._controlsUnderVideo || !controlBar.visible)
                          ? 0 : - VLCStyle.margin_large
        }
    }

    Timer {
        // NavigationBox's visibility depends on this timer
        id: interactiveAutoHideTimer
        running: false
        repeat: false
        interval: 3000
    }

    NavigationBox {
        id: navBox
        visible: Player.isInteractive && navBox.show
                    && (interactiveAutoHideTimer.running
                    || navBox.hovered || !MainCtx.hasEmbededVideo)

        x: rootPlayer.x + VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
        y: controlBar.y - navBox.height - VLCStyle.margin_normal

        dragXMin: 0
        dragXMax: rootPlayer.width - navBox.width
        dragYMin: 0
        dragYMax: rootPlayer.height - navBox.height

        Drag.onDragStarted: (controlId) => {
            navBox.x = drag.x
            navBox.y = drag.y
        }
    }

    Connections {
        target: MainCtx
        function onNavBoxToggled() { interactiveAutoHideTimer.restart() }
    }

    Connections {
        target: DialogsProvider

        // Jump to Time opens over the seek bar, which must be shown first
        function onShowJumpToTime() {
            playerToolbarVisibilityFSM.askShow()
            Qt.callLater(controlBar.openJumpToTime)
        }
    }

    Connections {
        target: rootPlayer
        function onWidthChanged() {
            if (navBox.x > navBox.dragXMax)
                navBox.x = navBox.dragXMax
        }
        function onHeightChanged() {
            if (navBox.y > navBox.dragYMax)
                navBox.y = navBox.dragYMax
        }
    }

    Widgets.ButtonExt {
        id: toggleControlBarButton
        visible: Player.isInteractive
                 && MainCtx.hasEmbededVideo
                 && !rootPlayer._controlsUnderVideo
                 && (interactiveAutoHideTimer.running === true
                     || controlBar.state !== "hidden" || toggleControlBarButton.hovered)
        focus: true
        anchors {
            bottom: controlBar.state === "hidden" ? parent.bottom : controlBar.top
            horizontalCenter: parent.horizontalCenter
        }
        iconSize: VLCStyle.icon_large
        iconTxt: controlBar.state === "hidden" ? VLCIcons.expand_inverted : VLCIcons.expand

        Navigation.parentItem: rootPlayer
        Navigation.upItem: rightPanel.adjustmentsOpened ? adjustmentsPanel
                           : playlistVisibility.isPlaylistVisible ? playlistpopup
                           : (MainCtx.hasEmbededVideo ? playerSpecializationLoader : topBar)
        Navigation.downItem: controlBar

        onClicked:{
            playerToolbarVisibilityFSM.askShow();
        }
    }

    ContinueWatchingPill {
        id: continueWatchingPill

        // Figma: centered, 24 px above the Control bar
        anchors {
            bottom: controlBar.top
            bottomMargin: VLCStyle.dp(24, VLCStyle.scale)
            horizontalCenter: parent.horizontalCenter
        }

        width: Math.min(implicitWidth, rootPlayer.width - 2 * VLCStyle.margin_large)

        colorContext.palette: windowTheme.palette

        Navigation.parentItem: rootPlayer
        Navigation.upItem: rootPlayer._itemAboveControlBar
        Navigation.downItem: controlBar

        onRequestLockUnlockAutoHide: (lock) => rootPlayer.lockUnlockAutoHide(lock)

        // the focus goes back to the Control bar with the pill
        onClosing: {
            if (activeFocus)
                controlBar.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    Widgets.FloatingNotification {
        id: notif

        anchors {
            bottom: continueWatchingPill.visible ? continueWatchingPill.top : controlBar.top
            left: parent.left
            right: parent.right
            margins: VLCStyle.margin_large
        }
    }

    ControlBar {
        id: controlBar

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        hoverEnabled: true

        focus: true

        // the Two-row Layout puts the seek bar on its own row
        seekBarOwnRow: true

        // Figma 491:1779: 20 px on the sides, 11 px above and below the rows
        rightPadding: VLCStyle.applicationHorizontalMargin + VLCStyle.spacing_20
        leftPadding: VLCStyle.applicationHorizontalMargin + VLCStyle.spacing_20
        topPadding: VLCStyle.dp(11, VLCStyle.scale)
        bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.dp(11, VLCStyle.scale)

        textPosition: (rootPlayer._controlsUnderVideo)
                      ? ControlBar.TimeTextPosition.LeftRightSlider
                      : ControlBar.TimeTextPosition.AboveSlider

        // hide right text so that it won't overlap with play queue
        showRemainingTime: (textPosition !== ControlBar.TimeTextPosition.AboveSlider)
                           || !(playlistVisibility.isPlaylistVisible || rightPanel.adjustmentsOpened)

        onStateChanged: {
            if (state === "visible")
                showChapterMarks()
        }

        Navigation.parentItem: rootPlayer
        Navigation.cancelAction: MainCtx.hasEmbededVideo ? rootPlayer._focusVideo
                                                         : () => History.previous(Qt.BacktabFocusReason)
        Navigation.upItem: continueWatchingPill.visible ? continueWatchingPill
                                                        : rootPlayer._itemAboveControlBar

        //initial state value is "", using a binding avoid animation on startup
        Binding on state {
            when: playerToolbarVisibilityFSM.started
            value: (playerToolbarVisibilityFSM.isVisible || rootPlayer._controlsUnderVideo) ? "visible" : "hidden"
        }

        onRequestLockUnlockAutoHide: (lock) => rootPlayer.lockUnlockAutoHide(lock)

        identifier: (Player.hasVideoOutput) ? PlayerControlbarModel.Videoplayer
                                            : PlayerControlbarModel.Audioplayer

        onHoveredChanged: rootPlayer.lockUnlockAutoHide(hovered)

        background: Rectangle {
            id: controlBarBackground

            visible: !MainCtx.hasEmbededVideo || rootPlayer._controlsUnderVideo || !rootPlayer.displayFadeRectangles

            opacity: {
                if (rootPlayer._panelsOverVideo) {
                    return 1.0 // the Tint is translucent
                } else if ((Window.visibility === Window.FullScreen) && MainCtx.hasEmbededVideo) {
                    return MainCtx.pinOpacity
                } else if (!MainCtx.hasEmbededVideo) {
                    return 0.7
                } else {
                    return 1.0
                }
            }

            color: rootPlayer._panelsOverVideo ? windowTheme.tint : windowTheme.bg.primary

            // Figma: Divider/Dim along the top edge
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right

                height: VLCStyle.border

                color: windowTheme.separator
            }
        }

        transform: controlBarReveal.translate

        BarRevealStateGroup {
            id: controlBarReveal

            target: controlBar
            edge: Qt.BottomEdge
            animate: playerToolbarVisibilityFSM.animate
            reducedMotion: MainCtx.reduceMotion
        }
    }

    QmlAudioContextMenu {
        id: audioContextMenu

        ctx: MainCtx
    }

    // opened by the audio and subtitles buttons of the Control bar
    TrackMenu {
        id: trackMenu

        readonly property real _margin: VLCStyle.dp(20, VLCStyle.scale)

        x: rootPlayer.width - width - _margin - VLCStyle.applicationHorizontalMargin
        y: rootPlayer.positionSliderY - height - _margin

        maximumWidth: rootPlayer.width - 2 * _margin - 2 * VLCStyle.applicationHorizontalMargin
        maximumHeight: rootPlayer.positionSliderY - (topBar.visible ? topBar.y + topBar.height : 0)
                       - 2 * _margin

        colorContext.palette: windowTheme.palette

        onOpened: rootPlayer.lockUnlockAutoHide(true)
        onClosed: rootPlayer.lockUnlockAutoHide(false)

        // "More settings": the Adjustments sidebar on the same tab
        onRequestAdjustments: (tab) => rightPanel.openAdjustments(rootPlayer._adjustmentsTab(tab))
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        enabled: !MainCtx.hasEmbededVideo

        onTapped: (eventPoint, button) => {
            if (button & Qt.RightButton) {
                audioContextMenu.popup(eventPoint.globalPosition)
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        propagateComposedEvents: true
        cursorShape: undefined
        onClicked: {
            MainCtx.playerView = false
        }
    }

    //filter key events to keep toolbar
    //visible when user navigates within the control bar
    KeyEventFilter {
    id: filter
    target: MainCtx.intfMainWindow

        Keys.onPressed: (event) => {
            if (Player.isInteractive) {
                interactiveAutoHideTimer.restart()
                return
            }

            // Tab and F6 from the video reveal the bars instantly, with the focus on Play
            const revealKey = (event.key === Qt.Key_Tab
                               || event.key === Qt.Key_Backtab
                               || event.key === Qt.Key_F6)
            if (revealKey && MainCtx.hasEmbededVideo
                    && (!playerToolbarVisibilityFSM.isVisible || playerSpecializationLoader.activeFocus)) {
                playerToolbarVisibilityFSM.keyboardReveal()
                controlBar.focusPlayButton(Qt.TabFocusReason)
                event.accepted = true
                return
            }

            playerToolbarVisibilityFSM.keyboardMove()
        }
    }
}
