/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

import VLC.Style
import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player

import VLC.Util
import VLC.Dialogs

FocusScope {
    id: g_mainDisplay

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Main display")
    AccessibleCompat.id: "mainDisplay"

    // Properties

    property bool hasMiniPlayer: miniPlayer.visible

    //MainDisplay behave as a PageLoader
    property alias pagePrefix: stackView.pagePrefix

    property bool _showCSD: MainCtx.clientSideDecoration
        && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    // functions

    //MainDisplay behave as a PageLoader
    function loadView(path, properties, focusReason) {
        stackView.loadView(path, properties, focusReason)
    }

    function loadCurrentHistoryView(focusReason) {
        loadView(History.viewPath, History.viewProp, focusReason)
        contextSaver.restore(History.viewPath)
    }

    Component.onCompleted: {
        // the video can only go on in the Mini player when the compositor can
        // show it there
        if (MainCtx.hasEmbededVideo && !MainCtx.canShowVideoPIP)
            MainPlaylistController.stop()

        // the Library pane may already have taken the user somewhere
        if (History.viewPath.length === 0) {
            History.update(["home"])
        }
        loadCurrentHistoryView(Qt.OtherFocusReason)
    }

    Navigation.cancelAction: function() {
        History.previous(Qt.BacktabFocusReason)
    }

    // F6 moves the focus to the next area of the window, Shift+F6 to the previous one
    function cycleFocusArea(backward) {
        const areas = [sidebar, localTopbar, stackView, playlistLoader, miniPlayer]
            .filter((area) => area.visible && area.enabled && area.width > 0)
        if (areas.length === 0)
            return

        let current = areas.findIndex((area) => area.activeFocus)
        if (current === -1)
            current = backward ? 0 : areas.length - 1

        const next = (current + (backward ? areas.length - 1 : 1)) % areas.length
        areas[next].forceActiveFocus(backward ? Qt.BacktabFocusReason : Qt.TabFocusReason)
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_F6
            && (event.modifiers & ~Qt.ShiftModifier) === Qt.NoModifier) {
            cycleFocusArea(event.modifiers & Qt.ShiftModifier)
            event.accepted = true
            return
        }
        if (KeyHelper.matchSearch(event)) {
            MainCtx.search.askShow()
            event.accepted = true
        }
        //unhandled keys are forwarded as hotkeys
        if (!event.accepted)
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    readonly property var pageModel: LibraryPages.pageModel

    PinchHandler {
        id: interfaceScalePinchHandler

        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.TouchPad

        minimumPointCount: 2
        maximumPointCount: 2

        target: null

        // Qt version check in disguise, only available starting with 6.5:
        enabled: !!scaleAxis?.activeValueChanged

        // Default in QPlatformTheme::defaultThemeHint is 10
        dragThreshold: ((Application.styleHints?.startDragDistance ?? 10) * 4)

        property double accumulatedDelta: 0.0

        function adjustScaleFactor() {
            // We don't want too fine change, since changing it requires reloading
            // all images from disk:
            if (accumulatedDelta >= 0.5) {
                MainCtx.setIntfUserScaleFactor(+((MainCtx.getIntfUserScaleFactor() * accumulatedDelta).toFixed(2)))
                accumulatedDelta = 0.0
            }
        }

        Component.onCompleted: {
            if (interfaceScalePinchHandler?.scaleAxis?.activeValueChanged) {
                scaleAxis.activeValueChanged.connect(interfaceScalePinchHandler, onScaleAxisActiveValueChanged)
            }
        }

        function onScaleAxisActiveValueChanged(delta) {
            if (delta < 0.25)
                return

            accumulatedDelta += delta

            // Naive compression:
            Qt.callLater(interfaceScalePinchHandler.adjustScaleFactor)
        }

        onActiveChanged: {
            if (!active) {
                accumulatedDelta = 0.0
            }
        }
    }

    ModelSortSettingHandler {
        id: contextSaver
    }

    Connections {
        target: MainCtx.sort

        function onCriteriaChanged(criteria) {
            contextSaver.save(History.viewPath)
        }

        function onOrderChanged(order) {
            contextSaver.save(History.viewPath)
        }
    }

    Connections {
        target: History
        function onNavigate(focusReason) {
            loadCurrentHistoryView(focusReason)
        }
    }

    ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.View
    }

    Loader {
        id: voronoiSnowLoader

        z: 3.5
        source: "qrc:///qt/qml/VLC/Widgets/VoronoiSnow.qml"
        anchors.fill: parent
        active: false

        function toggleActive() {
            voronoiSnowLoader.active = !voronoiSnowLoader.active
        }

        Component.onCompleted: {
            if (MainCtx.useXmasCone()) {
                MainCtx.kc_pressed.connect(voronoiSnowLoader.toggleActive)
            }
        }
    }

    LocalTopbar {
        id: localTopbar
        z: 5

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        leftPadding: VLCStyle.applicationHorizontalMargin
        rightPadding: VLCStyle.applicationHorizontalMargin
        topPadding: VLCStyle.applicationVerticalMargin

        plListView: {
            if (playlistLoader.active)
                return playlistLoader.item
            else if (playlistWindowLoader.status === Loader.Ready)
                return playlistWindowLoader.item.playlistView
            else
                return null
        }

        navigationVisible: pannelVisiblity.showNavigation
        playqueueVisible: pannelVisiblity.showPlayqueue

        Navigation.parentItem: g_mainDisplay
        Navigation.downItem: mainRow

        onToggleNavigationVisibility: pannelVisiblity.toggleNavigationVisibility()
        onTogglePlayqueueVisibility:  pannelVisiblity.togglePlayqueueVisibility()
    }

    FocusScope {
        id: mainRow

        anchors {
            top: localTopbar.bottom
            right: parent.right
            left: parent.left
            bottom: parent.bottom
        }

        z: 0

        Navigation.parentItem: g_mainDisplay

        Rectangle {
            id: stackViewParent

            // the background of the pages: an opaque Rectangle keeps the subpixel
            // font rendering
            z: 1

            anchors.fill: parent

            implicitWidth: stackView.implicitWidth
            implicitHeight: stackView.implicitHeight

            color: theme.bg.primary

            Widgets.PageLoader {
                id: stackView

                focus: true

                anchors.fill: parent
                anchors.leftMargin: (sidebar.visible && (VLCStyle.isScreenSmall ? (sidebar.state === "") : true)) ? sidebar.width : 0.0
                anchors.rightMargin: (playlistLoader.shown && !VLCStyle.isScreenSmall)
                                     ? playlistLoader.width
                                     : 0
                // Prevent animating x on collapse with overlayed side navigation bar:
                Binding on anchors.leftMargin {
                    when: (VLCStyle.isScreenSmall && sidebar.state === "" && sidebarTransition.running)
                    value: 0.0
                }

                pageModel: g_mainDisplay.pageModel

                leftPadding: sidebar.visible  ? 0 : VLCStyle.applicationHorizontalMargin

                rightPadding: playlistLoader.shown ? 0 : VLCStyle.applicationHorizontalMargin

                // Top-left corner rounding:
                Widgets.AcrylicBackground {
                    id: topLeftCornerBackground

                    z: 99
                    color: sidebar.background.color
                    visible: sidebar.visible && (stackViewTopLeftRadiusRectangle.topLeftRadius !== undefined)

                    anchors.left: stackView.left
                    anchors.leftMargin: -1
                    anchors.top: stackView.top

                    // (-1) is to prevent seams
                    width: stackViewTopLeftRadiusRectangle.width - 1
                    height: stackViewTopLeftRadiusRectangle.height - 1

                    Rectangle {
                        id: stackViewTopLeftRadiusRectangle

                        width: (stackViewTopLeftRadiusRectangle.topLeftRadius * 2)
                        height: (stackViewTopLeftRadiusRectangle.topLeftRadius * 2)

                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.leftMargin: 1

                        color: stackViewParent.color

                        property real implicitTopLeftRadius: VLCStyle.mainView_topLeftRadius

                        Component.onCompleted: {
                            if (stackViewTopLeftRadiusRectangle.topLeftRadius !== undefined) // TODO: Directly set when minimum Qt is 6.7
                                stackViewTopLeftRadiusRectangle.topLeftRadius = Qt.binding(() => stackViewTopLeftRadiusRectangle.implicitTopLeftRadius)
                        }
                    }
                }

                Navigation.parentItem: g_mainDisplay
                Navigation.upItem: localTopbar
                Navigation.rightItem: playlistLoader
                Navigation.leftItem: sidebar
                Navigation.downItem: miniPlayer
            }
        }

        Rectangle {
            id: smallScreenOverlay

            // overlay for smallscreens
            z: 2

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            height: parent.height

            visible: VLCStyle.isScreenSmall && (playlistLoader.shown || (sidebar.visible && (sidebar.state === "expanded")))
            color: theme.shadow.alpha(1.0)
            opacity: 0.4

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onClicked:  pannelVisiblity.hideVisiblePanels()

                // Capture WheelEvents before they reach stackView
                onWheel: (wheel) => {
                    wheel.accepted = true
                }
            }
        }

        SideNavigationPane {
            id: sidebar

            z: 3

            anchors {
                top : parent.top
                left: parent.left
            }

            width: VLCStyle.isScreenReallySmall ? 0.0 : collapsedWidth

            property real collapsedWidth: VLCStyle.icon_normal + VLCStyle.margin_small + VLCStyle.margin_xsmall + VLCStyle.applicationHorizontalMargin

            delegateShowText: (state === "expanded")

            visible: (width > 0.0)

            height: parent.height

            property int maximumWidth: Math.max(minimumWidth, (g_mainDisplay.width + sidebarResizeHandle.width) / 3)

            topPadding: 0
            leftPadding: 0
            rightPadding: sidebarResizeHandle.visible ? sidebarResizeHandle.visualBorder.width : 0.0
            bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_small

            safeAreaLeftMargin: VLCStyle.applicationHorizontalMargin

            useAcrylic: (VLCStyle.isScreenSmall ? (sidebar.state === "") : true) // See `694d51a7` for the reasoning.

            NavigationBarContextMenu {
                id: navigationBarContextMenu

                ctx: MainCtx

                model: sidebar.model

                onPageRequested: (uri) => {
                    if (stackView.isDefaulLoadedForPath([...uri]) ||
                        History.exactMatch(History.viewPath, uri)) {
                        stackView.positionContentAtBeginning()
                        return
                    }

                    History.push(uri)
                }
            }

            onItemClicked: (modelUri, item) => {
                console.assert(modelUri)

                console.assert(item)
                if ((sidebar.state !== 'expanded') && item.expandable) {
                    // TODO: Investigate opening the menu on hover, if
                    //       we switch to `Popup.Item` type of menus.
                    const mappedPos = item.mapToGlobal(item.width, 0)
                    navigationBarContextMenu.popup(item.index, mappedPos, !item.hovered)

                    return
                }

                if (stackView.isDefaulLoadedForPath([...modelUri]) ||
                    !!modelUri.length && History.exactMatch(History.viewPath, modelUri)) {
                    stackView.positionContentAtBeginning()
                    return
                }

                History.push(modelUri)
            }

            implicitWidth: Math.round(Helpers.clamp(MainCtx.navigationPanel.width, minimumWidth, maximumWidth))

            Navigation.parentItem: g_mainDisplay
            Navigation.upItem: localTopbar
            Navigation.downItem: miniPlayer
            Navigation.rightItem: stackView.currentItem

            state: pannelVisiblity.showNavigation ? "expanded" : ""

            onVisibleChanged: {
                if (!visible) {
                    stackView.focus = true
                }
            }

            Component.onCompleted: {
                Qt.callLater(() => { sidebarTransition.enabled = true; })
            }

            states: State {
                name: "expanded"
                PropertyChanges {
                    target: sidebar
                    width: sidebar.implicitWidth
                }
            }

            transitions: Transition {
                id: sidebarTransition
                enabled: false

                from: ""; to: "expanded";
                reversible: true

                NumberAnimation {
                    property: "width"
                    duration: VLCStyle.duration_short
                    easing.type: Easing.InOutSine
                }
            }

            PaneResizeHandle {
                id: sidebarResizeHandle

                parent: sidebar
                target: sidebar

                panelObject: MainCtx.navigationPanel
                atRight: true

                resizeHandle.visible: (sidebar.state === "expanded") && !VLCStyle.isScreenSmall
                visualBorder.visible: !topLeftCornerBackground.visible

                minimumWidth: sidebar.minimumWidth
                maximumWidth: sidebar.maximumWidth

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                }

            }
        }

        Loader {
            id: playlistLoader

            z: 3
            anchors {
                top: parent.top
                right: parent.right
            }

            width: 0

            visible: false
            height: parent.height

            active: MainCtx.playqueuePanel.docked

            state: ((status === Loader.Ready) && pannelVisiblity.showPlayqueue) ? "expanded" : ""

            readonly property bool shown: (status === Loader.Ready) && item.visible

            onVisibleChanged: {
                if (!visible) {
                    stackView.focus = true
                }
            }

            Component.onCompleted: {
                Qt.callLater(() => { playlistTransition.enabled = true; })
            }

            states: State {
                name: "expanded"
                PropertyChanges {
                    target: playlistLoader
                    width: playlistLoader.implicitWidth
                    visible: true
                }
            }

            transitions: Transition {
                id: playlistTransition
                enabled: false

                from: ""; to: "expanded";
                reversible: true

                SequentialAnimation {
                    PropertyAction { property: "visible" }

                    NumberAnimation {
                        property: "width"
                        duration: VLCStyle.duration_short
                        easing.type: Easing.InOutSine
                    }
                }
            }

            sourceComponent: PlaylistPane {
                id: playlist

                property int maximumWidth: Math.max(minimumWidth, (g_mainDisplay.width + playqueueResizeHandle.width)
                                                    / (pannelVisiblity.showNavigation ? 3 : 2))

                implicitWidth: Math.round(Helpers.clamp(MainCtx.playqueuePanel.width, minimumWidth, maximumWidth))

                focus: true

                leftPadding: playqueueResizeHandle.visualBorder.width
                rightPadding: VLCStyle.applicationHorizontalMargin
                bottomPadding: VLCStyle.applicationVerticalMargin
                topPadding: VLCStyle.layoutTitle_top_padding

                useAcrylic: !VLCStyle.isScreenSmall

                view.fadingEdge.pixelAlignedForDPR: !miniPlayer.visible

                Navigation.parentItem: g_mainDisplay
                Navigation.upItem: localTopbar
                Navigation.downItem: miniPlayer
                Navigation.leftItem: stackView.currentItem

                Navigation.cancelAction: function() {
                    MainCtx.playqueuePanel.visible = false
                    stackView.forceActiveFocus()
                }

                PaneResizeHandle {
                    id: playqueueResizeHandle

                    parent: playlist
                    target: playlist

                    minimumWidth: playlist.minimumWidth
                    maximumWidth: playlist.maximumWidth

                    panelObject: MainCtx.playqueuePanel
                    atRight: false

                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        left: parent.left
                    }
                }
            }
        }
    }

    //track the visiblity state of the side panels
    //FIXME do we want proper state machine?
    Item {
        id: pannelVisiblity
        property bool showNavigation: false
        property bool showPlayqueue: false

        Component.onCompleted: {
            onIsScreenSmallChanged()
        }

        onShowNavigationChanged: {
            if (VLCStyle.isScreenSmall && pannelVisiblity.showPlayqueue && MainCtx.playqueuePanel.docked && pannelVisiblity.showNavigation) {
                pannelVisiblity.showPlayqueue = false
            }
        }

        onShowPlayqueueChanged: {
            if (VLCStyle.isScreenSmall && pannelVisiblity.showPlayqueue && MainCtx.playqueuePanel.docked && pannelVisiblity.showNavigation) {
                pannelVisiblity.showNavigation = false
            }
        }

        function hideVisiblePanels() {
            pannelVisiblity.showNavigation = false
            MainCtx.navigationPanel.visible = false
            if (MainCtx.playqueuePanel.docked) {
                pannelVisiblity.showPlayqueue = false
                MainCtx.playqueuePanel.visible = false
            }
        }

        function toggleNavigationVisibility() {
            showNavigation = !showNavigation
            MainCtx.navigationPanel.visible = showNavigation
        }

        function togglePlayqueueVisibility() {
            showPlayqueue = !showPlayqueue
            MainCtx.playqueuePanel.visible = showPlayqueue
        }

        function onIsScreenSmallChanged() {
            //when screen becomes small hide side panels
            if (VLCStyle.isScreenSmall) {
                pannelVisiblity.showNavigation = false
                pannelVisiblity.showPlayqueue = MainCtx.playqueuePanel.visible
            } else {
                //reshow the navigation panels to original state
                pannelVisiblity.showNavigation = MainCtx.navigationPanel.visible
                pannelVisiblity.showPlayqueue =  MainCtx.playqueuePanel.visible
            }
        }

        Connections {
            target: VLCStyle

            function onIsScreenSmallChanged() {
                pannelVisiblity.onIsScreenSmallChanged()
            }
        }

        Connections {
            target: MainCtx.playqueuePanel

            function onVisibleChanged() {
                pannelVisiblity.showPlayqueue = MainCtx.playqueuePanel.visible
            }
        }

        Connections {
            target: MainCtx.navigationPanel

            function onVisibleChanged() {
                pannelVisiblity.showNavigation = MainCtx.navigationPanel.visible
            }
        }
    }

    Dialogs {
        z: 10
        bgContent: g_mainDisplay

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
    }

    Widgets.FloatingNotification {
        id: notif
        z: 11

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: VLCStyle.margin_large
        }
    }

    // the video, or the music, goes on in a corner of the library
    MiniPlayer {
        id: miniPlayer

        z: 4

        // below the header, beside the Queue panel
        area: Qt.rect(0, mainRow.y,
                      g_mainDisplay.width - (playlistLoader.shown ? playlistLoader.width : 0),
                      mainRow.height)

        Navigation.parentItem: g_mainDisplay
        Navigation.upItem: mainRow

        onVisibleChanged: {
            if (!visible && miniPlayer.activeFocus)
                stackView.forceActiveFocus()
        }
    }

    Connections {
        target: Player
        function onHasVideoOutputChanged() {
            if (Player.hasVideoOutput && MainCtx.hasEmbededVideo)
                MainCtx.playerView = true
        }
    }

    component PaneResizeHandle: Item {
        id: paneResizeHandle
        required property Item target
        required property QtObject panelObject
        property alias atRight: resizeHandle.atRight

        property alias visualBorder: visualBorder
        property alias resizeHandle: resizeHandle

        property alias minimumWidth: resizeHandle.minimumWidth
        property alias maximumWidth: resizeHandle.maximumWidth

        implicitWidth: resizeHandle.width

        Rectangle {
            id: visualBorder

            anchors {
                top: parent.top
                bottom: parent.bottom
                left: resizeHandle.atRight ? undefined : parent.left
                right: resizeHandle.atRight ? parent.right: undefined
            }

            width: VLCStyle.border
            color: theme.separator
        }

        Widgets.HorizontalResizeHandle {
            id: resizeHandle

            property bool _inhibitMainInterfaceUpdate: false

            anchors {
                top: parent.top
                bottom: parent.bottom
                left: resizeHandle.atRight ? undefined : parent.left
                right: resizeHandle.atRight ? parent.right: undefined
            }

            atRight: false
            currentWidth: target.width

            visible: !VLCStyle.isScreenSmall

            onRequestedWidthChanged: {
                if (!_inhibitMainInterfaceUpdate)
                    paneResizeHandle.panelObject.width = requestedWidth
            }

            Component.onCompleted:  _updateFromMainInterface()

            function _updateFromMainInterface() {
                if (requestedWidth === paneResizeHandle.panelObject.width)
                    return

                _inhibitMainInterfaceUpdate = true
                requestedWidth = paneResizeHandle.panelObject.width
                _inhibitMainInterfaceUpdate = false
            }

            Connections {
                target: paneResizeHandle.panelObject

                function onWidthChanged() {
                    resizeHandle._updateFromMainInterface()
                }
            }
        }
    }

    MouseArea {
        /// handles mouse navigation buttons
        z:9
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        cursorShape: undefined
        onClicked: History.previous()
    }
}
