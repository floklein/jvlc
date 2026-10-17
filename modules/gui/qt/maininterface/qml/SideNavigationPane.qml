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
import QtQuick.Templates as T
import QtQml.Models

import VLC.Style
import VLC.Widgets as Widgets

import VLC.MainInterface
import VLC.Dialogs
import VLC.Util
import VLC.Network
import VLC.Playlist

T.Pane {
    id: root

    signal itemClicked(uri : var, item : Item)

    //safe area margins for delegate content,
    //so the background of the delegates fills the available space
    //but the content remains within the safe area
    property int safeAreaLeftMargin: 0
    property int safeAreaRightMargin: 0

    property bool useAcrylic: true

    property bool delegateShowText: true

    property alias model: navigationModel

    readonly property int minimumWidth: VLCStyle.expandNavigationPaneWidth + safeAreaLeftMargin + safeAreaRightMargin

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_normal

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Navigation
    }

    Accessible.name: qsTr("Sidebar")

    NavigationModel {
        id: navigationModel
        hasMedialib: MainCtx.mediaLibraryAvailable
    }

    Connections {
        target: History

        // NOTE: handles cases where the path changes not by clicking in the sidebar
        function onViewPathChanged(viewPath) {
            for (let i = 0; i < listView.count; ++i) {
                const rowIndex = navigationModel.index(i, 0)
                const uri = navigationModel.data(rowIndex, NavigationModel.URI)

                if (uri && History.match(viewPath, uri)) {
                    // handle back button
                    listView.currentIndex = i

                    // handle keyboard navigation
                    if (!navigationModel.data(rowIndex, NavigationModel.EXPANDED))
                        navigationModel.setData(rowIndex, true, NavigationModel.EXPANDED)

                    return
                }
            }
        }
    }

    // Pins

    // the icon of a pin, filled when it is shown
    function pinIcon(type, selected = false) {
        switch (type) {
        case Pins.FOLDER:
        case Pins.MUSIC_FOLDER:
            return selected ? VLCIcons.folder_selected : VLCIcons.folder
        case Pins.PLAYLIST:
            return selected ? VLCIcons.playlists_active : VLCIcons.playlists
        default:
            return selected ? VLCIcons.streaming_selected : VLCIcons.stream
        }
    }

    // the page a pin opens; a Saved stream plays
    function pinPath(type) {
        switch (type) {
        case Pins.FOLDER:
            return ["video", "folders"]
        case Pins.MUSIC_FOLDER:
            return ["music", "tracks"]
        case Pins.PLAYLIST:
            return ["playlists", "list"]
        default:
            return ["network", "playlist"]
        }
    }

    function openPin(row, focusReason) {
        const index = Pins.index(row, 0)
        const type = Pins.data(index, Pins.TYPE)
        const mlId = Pins.data(index, Pins.ML_ID)

        if ((type === Pins.FOLDER || type === Pins.MUSIC_FOLDER) && mlId) {
            History.push(pinPath(type), { parentId: mlId }, focusReason)
        } else if (type === Pins.PLAYLIST && mlId) {
            History.push(pinPath(type), { parentId: mlId, title: Pins.data(index, Pins.LABEL) }, focusReason)
        } else if (type === Pins.STREAM) {
            const stream = SavedStreams.get(Pins.data(index, Pins.KEY))
            if (!stream.id)
                return
            if (stream.isPlaylist)
                History.push(pinPath(type), { playlistId: stream.id }, focusReason)
            else
                MainPlaylistController.playMedia(stream.urls)
        }
    }

    function isPinShown(type, key) {
        if (!History.exactMatch(History.viewPath, pinPath(type)))
            return false
        if (type === Pins.STREAM)
            return History.viewProp.playlistId === key
        return (History.viewProp.parentId?.toString() ?? "") === key
    }

    NativeMenu {
        id: pinMenu

        actions: [{
                "text": qsTr("Open"),
                "action": (dataList) => root.openPin(dataList[0].row, Qt.OtherFocusReason)
            }, {
                "text": qsTr("Move Up"),
                "action": (dataList) => Pins.move(dataList[0].row, dataList[0].row - 1),
                "visible": (dataList) => dataList[0].row > 0
            }, {
                "text": qsTr("Move Down"),
                "action": (dataList) => Pins.move(dataList[0].row, dataList[0].row + 1),
                "visible": (dataList) => dataList[0].row < Pins.count - 1
            }, {
                "text": qsTr("Unpin from Sidebar"),
                "action": (dataList) => Pins.remove(dataList[0].row)
            }
        ]

        onRequestData: (requestID, indexes) => {
            setData(requestID, indexes.map((row) => ({ "row": row })))
        }
    }

    background: Widgets.AcrylicBackground {
        enabled: root.useAcrylic
        tintColor: theme.bg.primary
    }

    contentItem:  Item {
        id: contentItemId

        //don't use Layout as top item as items put as children of the Control
        //would be parented (and layouted) to the Layout
        ColumnLayout {
            anchors.fill: parent
            spacing: 0 // the rows end with their spacing

            Widgets.ListViewExt {
                id: listView

                Layout.fillWidth: true
                // the Pins follow the last section; on a short window the sections
                // keep at least half of the height and scroll
                Layout.preferredHeight: contentHeight
                Layout.minimumHeight: Math.min(contentHeight, contentItemId.height / 2)

                clip: !fadingEdge.implicitClipping && (height < contentHeight)
                focus: true

                model: navigationModel

                Binding {
                    target: listView.T.ScrollBar.vertical
                    property: "policy"
                    when: listView.T.ScrollBar.vertical?.size > (1.0 - Number.EPSILON)
                    value: T.ScrollBar.AlwaysOff
                }

                colorContext.colorSet: ColorContext.Navigation

                fadingEdge.backgroundColor:  (root.background && (root.background.color.a >= 1.0)) ? root.background.color
                                                                                                   : "transparent"

                component DelegateAnimation : NumberAnimation {
                    easing.type: Easing.InOutSine
                    duration: VLCStyle.duration_long
                }

                reuseItems: false // Not relevant here, we disable it because of QTBUG-131106 when transition is involved.

                // As the docs note, `ListView` transitions do not seem to handle height animation. For that reason,
                // we animate the height directly in the delegate (Qt's recommended workaround). Because of this, we
                // do not need to have `addDisplaced` transition here, but we can have `removeDisplaced` here:
                removeDisplaced: Transition {
                    DelegateAnimation { property: "y" }
                }

                onCurrentIndexChanged: {
                    const rowIndex = navigationModel.index(currentIndex, 0)

                    if (rowIndex.valid && !navigationModel.data(rowIndex, NavigationModel.EXPANDED)
                            && History.match(History.viewPath, navigationModel.data(rowIndex, NavigationModel.URI)))
                        navigationModel.setData(rowIndex, true, NavigationModel.EXPANDED)
                }

                // once every section fits, the list must not stay scrolled: following
                // the current section while Music's sections collapse scrolled it, and
                // Home was then cut at the top above an empty row
                function _scrollBackIfFits() {
                    if (height >= contentHeight - 0.5 && contentY !== originY)
                        contentY = originY
                }

                onHeightChanged: _scrollBackIfFits()
                onContentHeightChanged: _scrollBackIfFits()
                onContentYChanged: _scrollBackIfFits()

                property bool readyForAnimations: false

                onModelChanged: {
                    if (!listView.readyForAnimations)
                        Qt.callLater(() => { listView.readyForAnimations = true })
                }

                Navigation.parentItem: root
                Navigation.downItem: (pinsView.count > 0) ? pinsView
                                                          : (addPinButton.visible ? addPinButton : preferenceButton)

                // read as "Sections, list with 6 items", like the Pins below
                Accessible.name: qsTr("Sections")

                // Enter opens the focused section, like Space and a click
                onActionAtIndex: (index) => listView.itemAtIndex(index)?.clicked()

                delegate: SideNavigationDelegate {
                    id: delegate

                    width: ListView.view.contentWidth
                    height: preferredHeight

                    property real preferredHeight: shouldShow ? VLCStyle.buttonHeightNavigationPane : 0.0

                    leftPadding: root.safeAreaLeftMargin
                    rightPadding: root.safeAreaRightMargin

                    leftInset: root.safeAreaLeftMargin + implicitLeftInset
                    rightInset: root.safeAreaRightMargin + implicitRightInset

                    iconTxt: icon_alias
                    checkedIconTxt: icon_selected
                    text: title

                    showText: root.delegateShowText

                    required property string title
                    required property var uri
                    required property int depth
                    // TODO: Qt 6.2 has property `icon` marked final, so we can't override it:
                    required property string icon_alias
                    required property string icon_selected
                    required property bool expandable
                    required property bool expanded

                    required property int index

                    visible: (height > 0.0)

                    property bool shouldShow: showText || (iconTxt.length > 0)

                    onShouldShowChanged: {
                        if (listView.readyForAnimations)
                            heightBehavior.enabled = true // At this point, it can keep being enabled for good.
                    }

                    Binding on highlighted {
                        when: !!uri && History.match(History.viewPath, uri)
                        value: true
                    }
                    checked: (!showText || !expanded) && highlighted

                    onClicked: {
                        itemClicked(uri, this)
                        listView.currentIndex = index
                        listView.forceActiveFocus(focusReason)
                    }

                    Behavior on height {
                        id: heightBehavior

                        enabled: false

                        DelegateAnimation {
                            onRunningChanged: {
                                if (running) {
                                    delegate.clip = true
                                } else {
                                    delegate.clip = false
                                }
                            }
                        }
                    }

                    ListView.onAdd: {
                        if (listView.readyForAnimations && delegate.showText) {
                            heightBehavior.enabled = false
                            delegate.height = 0.0
                            heightBehavior.enabled = true
                            delegate.height = Qt.binding(() => delegate.preferredHeight)
                            heightBehavior.enabled = false
                        }
                    }
                }
            }

            // the Pins: drag or Alt+Up / Alt+Down reorder them
            Widgets.ListViewExt {
                id: pinsView

                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight

                visible: count > 0

                clip: !fadingEdge.implicitClipping && (height < contentHeight)

                // dragging a pin reorders it: flick only a list taller than its place
                interactive: height < contentHeight

                model: Pins

                Accessible.name: qsTr("Pins")

                colorContext.colorSet: ColorContext.Navigation

                fadingEdge.backgroundColor: listView.fadingEdge.backgroundColor

                Binding {
                    target: pinsView.T.ScrollBar.vertical
                    property: "policy"
                    when: pinsView.T.ScrollBar.vertical?.size > (1.0 - Number.EPSILON)
                    value: T.ScrollBar.AlwaysOff
                }

                Navigation.parentItem: root
                Navigation.upItem: listView
                Navigation.downItem: addPinButton.visible ? addPinButton : preferenceButton

                onActionAtIndex: (index) => root.openPin(index, Qt.TabFocusReason)

                menuKeyShowsContextMenu: true

                // a right click on a pin, or the Menu key on the current one
                onShowContextMenu: (globalPos) => {
                    const pos = contentItem.mapFromGlobal(globalPos.x, globalPos.y)
                    const row = indexAt(pos.x, pos.y)
                    if (row !== -1)
                        currentIndex = row
                    if (currentIndex >= 0)
                        pinMenu.popup([currentIndex], globalPos)
                }

                delegate: SideNavigationDelegate {
                    id: pinDelegate

                    required property int index
                    required property int type
                    required property string key
                    required property string label

                    AccessibleCompat.id: "pinNavBar"

                    width: ListView.view.contentWidth
                    height: VLCStyle.buttonHeightNavigationPane

                    leftPadding: root.safeAreaLeftMargin
                    rightPadding: root.safeAreaRightMargin

                    leftInset: root.safeAreaLeftMargin + implicitLeftInset
                    rightInset: root.safeAreaRightMargin + implicitRightInset

                    iconTxt: root.pinIcon(type)
                    checkedIconTxt: root.pinIcon(type, true)
                    text: label

                    showText: root.delegateShowText

                    // a pin of a media library item can't open without the library
                    enabled: (type === Pins.STREAM) || MainCtx.mediaLibraryAvailable

                    checked: root.isPinShown(type, key)

                    Accessible.description: qsTr("Pinned")

                    onClicked: {
                        pinsView.currentIndex = index
                        pinsView.forceActiveFocus(focusReason)
                        root.openPin(index, focusReason)
                    }

                    // drag a pin to its new place
                    DragHandler {
                        target: null
                        xAxis.enabled: false

                        // the list doesn't flick away a pin being dragged
                        grabPermissions: PointerHandler.CanTakeOverFromAnything

                        onCentroidChanged: {
                            if (!active)
                                return
                            // the delegate moves with its row: map from the scene
                            const pos = pinsView.contentItem.mapFromItem(null, centroid.scenePosition.x, centroid.scenePosition.y)
                            const to = pinsView.indexAt(pinsView.contentItem.width / 2, pos.y)
                            if (to !== -1 && to !== pinDelegate.index)
                                Pins.move(pinDelegate.index, to)
                        }
                    }

                    Keys.priority: Keys.BeforeItem
                    Keys.onPressed: (event) => {
                        if (event.modifiers !== Qt.AltModifier)
                            return
                        let to = -1
                        if (event.key === Qt.Key_Up)
                            to = index - 1
                        else if (event.key === Qt.Key_Down)
                            to = index + 1
                        else
                            return
                        event.accepted = true
                        if (Pins.move(index, to))
                            pinsView.currentIndex = to
                    }
                }
            }

            // "+ Add item": the picker of folders, Playlists and streams to Pin
            SideNavigationDelegate {
                id: addPinButton

                AccessibleCompat.id: "addPinButton"

                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.buttonHeightNavigationPane

                visible: MainCtx.mediaLibraryAvailable

                leftPadding: root.safeAreaLeftMargin
                rightPadding: root.safeAreaRightMargin

                leftInset: root.safeAreaLeftMargin + implicitLeftInset
                rightInset: root.safeAreaRightMargin + implicitRightInset

                iconTxt: VLCIcons.add
                text: qsTr("Add item")

                showText: root.delegateShowText

                onClicked: Pins.requestPicker()

                //SideNavigationDelegate is an ItemDelegate and has NoFocus by default
                focusPolicy: Qt.StrongFocus

                Navigation.parentItem: root
                Navigation.upItem: (pinsView.count > 0) ? pinsView : listView
                Navigation.downItem: preferenceButton

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (e) => Navigation.defaultKeyAction(e)
                Keys.onReleased: (e) => Navigation.defaultKeyReleaseAction(e)
            }

            Item {
                Layout.fillHeight: true
            }

            Loader {
                id: loaderProgress

                Layout.fillWidth: true

                active: (MainCtx.mediaLibraryAvailable && MainCtx.mediaLibrary.idle === false)
                visible: active

                source: "qrc:///qt/qml/VLC/MediaLibrary/ScanProgressBar.qml"

                onLoaded: {
                    if (item.background) {
                        item.background.opacity = 0.6
                        item.background.radius = Qt.binding(() => VLCStyle.dp(4, VLCStyle.scale))
                    }

                    item.leftPadding = Qt.binding(function() { return root.safeAreaLeftMargin + VLCStyle.margin_small })
                    item.rightPadding = Qt.binding(function() { return root.safeAreaRightMargin + VLCStyle.margin_small })

                    item.leftInset = Qt.binding(() => { return VLCStyle.margin_xxsmall + root.safeAreaLeftMargin })
                    item.rightInset = Qt.binding(() => { return VLCStyle.margin_xxsmall + root.safeAreaRightMargin })
                }
            }

            SideNavigationDelegate {
                id: preferenceButton

                AccessibleCompat.id: "preferencesButton"

                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.buttonHeightNavigationPane

                leftPadding: root.safeAreaLeftMargin
                rightPadding: root.safeAreaRightMargin

                leftInset: root.safeAreaLeftMargin + implicitLeftInset
                rightInset: root.safeAreaRightMargin + implicitRightInset

                iconTxt: VLCIcons.settings
                checkedIconTxt: VLCIcons.settings_active
                text: qsTr("Settings")

                checked: DialogsProvider.prefsDialogVisible

                showText: root.delegateShowText

                onClicked: DialogsProvider.prefsDialog()

                //SideNavigationDelegate is an ItemDelegate and has NoFocus by default
                focusPolicy: Qt.StrongFocus

                Navigation.parentItem: root
                Navigation.upItem: addPinButton.visible ? addPinButton
                                                        : ((pinsView.count > 0) ? pinsView : listView)

                Keys.priority: Keys.AfterItem
                Keys.onPressed: (e) => Navigation.defaultKeyAction(e)
                Keys.onReleased: (e) => Navigation.defaultKeyReleaseAction(e)
            }

        }
    }
}
