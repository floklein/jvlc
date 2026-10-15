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
import QtQuick.Window
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models


import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Playlist
import VLC.Player
import VLC.Dialogs
import VLC.Style

// The Queue: played items dimmed, the current one highlighted, the upcoming
// ones, and the controls that manage it (Figma 463:831)
T.Pane {
    id: root

    property var model: PlaylistListModel {
        playlist: MainPlaylistController.playlist
        currentTime: Player.time
    }
    readonly property ListSelectionModel selectionModel: listView?.selectionModel ?? null

    property bool useAcrylic: true

    // the "Queue" title, for hosts that have no tab bar
    property bool showTitle: true

    readonly property real minimumWidth: contentItem.Layout.minimumWidth +
                                         leftPadding +
                                         rightPadding +
                                         2 * (VLCStyle.margin_xsmall)

    readonly property ListView listView: contentItem.listView

    property alias contextMenu: contextMenu

    property alias dragItem: dragItem

    property alias isDropAcceptableFunc: listView.isDropAcceptableFunc
    property alias acceptDropFunc: listView.acceptDropFunc

    property alias view: listView

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Queue")
    AccessibleCompat.id: "playqueuePane"

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View

        focused: root.activeFocus
        hovered: root.hovered
        enabled: root.enabled
    }

    // Functions

    // Remove the selected items
    function removeSelection() {
        const indexes = root.selectionModel.selectedIndexesFlat
        if (indexes.length > 0)
            root.model.removeItems(indexes)
    }

    // Move the selected items (or the current row) one row up or down
    function moveSelection(offset) {
        let indexes = root.selectionModel.selectedIndexesFlat.slice()
        if (indexes.length === 0) {
            if (listView.currentIndex < 0)
                return
            indexes = [listView.currentIndex]
        }
        indexes.sort((a, b) => a - b)
        root.model.shiftItems(indexes, offset)
    }

    // Children

    Widgets.DragItem {
        id: dragItem

        view: root.listView

        onRequestData: (indexes, resolve, reject) => {
            resolve(indexes.map((index) => {
                const item = root.model.itemAt(index)
                return {
                    "title": item.title,
                    "cover": (!!item.artwork && item.artwork.toString() !== "") ? item.artwork : VLCStyle.noArtAlbumCover,
                    "url": item.url
                }
            }))
        }

        onRequestInputItems: (indexes, data, resolve, reject) => {
            resolve(root.model.getItemsForIndexes((indexes?.length > 0) ? indexes : root.selectionModel.selectedIndexesFlat))
        }
    }

    PlaylistContextMenu {
        id: contextMenu
        model: root.model
        selectionModel: root.selectionModel
        controler: MainPlaylistController
        ctx: MainCtx

        onJumpToCurrentPlaying: listView.positionViewAtIndex( MainPlaylistController.currentIndex, ItemView.Center)
    }

    background: Widgets.AcrylicBackground {
        enabled: root.useAcrylic
        tintColor: theme.bg.primary
    }

    contentItem: ColumnLayout {
        spacing: 0

        Layout.minimumWidth: Math.min(Math.max(noContentInfoColumn.implicitWidth,
                                               headerRow.implicitWidth),
                                      /* Minimum width should not be too large, we rely on elision otherwise: */
                                      Window.width / 3)

        readonly property ListView listView: listView

        Widgets.SubtitleLabel {
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.spacing_20
            Layout.rightMargin: VLCStyle.spacing_20

            visible: root.showTitle

            text: qsTr("Queue")
            color: theme.fg.primary
            font.weight: Font.Bold
            font.pixelSize: VLCStyle.dp(24, VLCStyle.scale)
        }

        // Figma "Frame 60": what is left on the left, the controls on the right
        RowLayout {
            id: headerRow

            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.dp(28, VLCStyle.scale)
            Layout.rightMargin: VLCStyle.spacing_20
            Layout.topMargin: VLCStyle.spacing_20
            Layout.bottomMargin: VLCStyle.spacing_20

            spacing: VLCStyle.spacing_12

            Widgets.ListLabel {
                id: remainingLabel

                Layout.fillWidth: true

                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal

                text: {
                    if (root.model.count === 0)
                        return ""
                    const duration = root.model.remainingDuration
                    if (!duration.valid() || duration.toSeconds() <= 0)
                        return qsTr("%1 left").arg(root.model.remainingCount)
                    return qsTr("%1 left · %2").arg(root.model.remainingCount)
                                               .arg(duration.formatLong())
                }
            }

            Widgets.SortControl {
                id: sort

                text: VLCIcons.sort
                font.pixelSize: VLCStyle.icon_normal

                description: qsTr("Sort")

                AccessibleCompat.id: "playqueueSort"

                enabled: MainPlaylistController.count > 1

                popupAbove: false

                model: MainPlaylistController.sortKeyTitleList

                onSortSelected: key => {
                    MainPlaylistController.sortKey = key
                }

                onSortOrderSelected: type => {
                    if (type === Qt.AscendingOrder)
                        MainPlaylistController.sortOrder = PlaylistController.SORT_ORDER_ASC
                    else if (type === Qt.DescendingOrder)
                        MainPlaylistController.sortOrder = PlaylistController.SORT_ORDER_DESC

                    MainPlaylistController.sort()
                }

                sortOrder: {
                    if (MainPlaylistController.sortOrder === PlaylistController.SORT_ORDER_ASC) {
                        Qt.AscendingOrder
                    }
                    else if (MainPlaylistController.sortOrder === PlaylistController.SORT_ORDER_DESC) {
                        Qt.DescendingOrder
                    }
                }

                sortKey: MainPlaylistController.sortKey

                Navigation.parentItem: root
                Navigation.rightItem: repeatButton
                Navigation.downItem: listView
            }

            Widgets.IconToolButton {
                id: repeatButton

                font.pixelSize: VLCStyle.icon_normal

                checked: MainPlaylistController.repeatMode !== PlaylistController.PLAYBACK_REPEAT_NONE
                text: (MainPlaylistController.repeatMode === PlaylistController.PLAYBACK_REPEAT_CURRENT)
                      ? VLCIcons.repeat_one
                      : VLCIcons.repeat

                description: {
                    switch (MainPlaylistController.repeatMode) {
                    case PlaylistController.PLAYBACK_REPEAT_ALL:
                        return qsTr("Repeat: All")
                    case PlaylistController.PLAYBACK_REPEAT_CURRENT:
                        return qsTr("Repeat: One")
                    default:
                        return qsTr("Repeat: Off")
                    }
                }

                toggle: true

                onClicked: MainPlaylistController.toggleRepeatMode()

                Navigation.parentItem: root
                Navigation.leftItem: sort
                Navigation.rightItem: shuffleButton
                Navigation.downItem: listView
            }

            Widgets.IconToolButton {
                id: shuffleButton

                font.pixelSize: VLCStyle.icon_normal

                checked: MainPlaylistController.random
                text: VLCIcons.shuffle
                description: qsTr("Shuffle")

                toggle: true

                onClicked: MainPlaylistController.toggleRandom()

                Navigation.parentItem: root
                Navigation.leftItem: repeatButton
                Navigation.rightItem: removeButton
                Navigation.downItem: listView
            }

            Widgets.IconToolButton {
                id: removeButton

                font.pixelSize: VLCStyle.icon_normal

                enabled: (root.selectionModel?.selectedIndexesFlat.length ?? 0) > 0
                text: VLCIcons.remove
                description: qsTr("Remove selected")
                shortcut: "Delete"

                onClicked: root.removeSelection()

                Navigation.parentItem: root
                Navigation.leftItem: shuffleButton
                Navigation.rightItem: addButton
                Navigation.downItem: listView
            }

            Widgets.IconToolButton {
                id: addButton

                font.pixelSize: VLCStyle.icon_normal

                text: VLCIcons.add
                description: qsTr("Add files")

                onClicked: DialogsProvider.simpleOpenDialog(false)

                Navigation.parentItem: root
                Navigation.leftItem: removeButton
                Navigation.downItem: listView
            }
        }

        Widgets.ListViewExt {
            id: listView

            Layout.fillWidth: true
            Layout.fillHeight: true

            selectionModel: ListSelectionModel {
                model: listView.model
            }

            focus: true

            clip: !fadingEdge.implicitClipping && (height < contentHeight)

            model: root.model

            fadingEdge.backgroundColor: (root.background && (root.background.color.a >= 1.0)) ? root.background.color
                                                                                              : Qt.alpha(root.background.color, 0.0)

            isDropAcceptableFunc: function(drop, index) {
                if (drop.source === dragItem)
                    return Helpers.itemsMovable(listView.selectionModel.selectedIndexesFlat, index)
                else if (Helpers.isValidInstanceOf(drop.source, Widgets.DragItem))
                    return true
                else if (drop.hasUrls)
                    return true
                else
                    return false
            }

            acceptDropFunc: function(index, drop) {
                const item = drop.source;

                // NOTE: Move implementation.
                if (dragItem === item) {
                    root.model.moveItemsPre(listView.selectionModel.selectedIndexesFlat, index);
                    listView.forceActiveFocus();
                // NOTE: Dropping medialibrary content into the queue.
                } else if (Helpers.isValidInstanceOf(item, Widgets.DragItem)) {
                    return item.getSelectedInputItem().then((inputItems) => {
                            if (!Helpers.isArray(inputItems) || inputItems.length === 0) {
                                console.warn("can't convert items to input items");
                                return
                            }
                            MainPlaylistController.insert(index, inputItems, false)
                        }).then(() => { listView.forceActiveFocus(); })
                // NOTE: Dropping an external item (i.e. filesystem) into the queue.
                } else if (drop.hasUrls) {
                    const urlList = [];

                    for (let url in drop.urls)
                        urlList.push(drop.urls[url]);

                    MainPlaylistController.insert(index, urlList, false);

                    // NOTE This is required otherwise backend may handle the drop as well yielding double addition.
                    drop.accept(Qt.IgnoreAction);
                    listView.forceActiveFocus();
                }

                return Promise.resolve()
            }

            // right-click on empty space
            onShowContextMenu: (globalPos) => {
                contextMenu.popup(-1, globalPos)
            }

            Behavior on contentY {
                id: contentYBehavior

                enabled: false

                // NOTE: Usage of `SmoothedAnimation` is intentional here.
                SmoothedAnimation {
                    duration: VLCStyle.duration_veryLong
                    easing.type: Easing.InOutSine
                }
            }

            // scroll to the current item
            function positionAtCurrent(animate) {
                if (MainPlaylistController.currentIndex < 0)
                    return

                if (!animate) {
                    listView.positionViewAtIndex(MainPlaylistController.currentIndex, ListView.Center)
                    return
                }

                // FIXME: Qt does not provide the `contentY` with `positionViewAtIndex()` for us
                //        to animate. For that reason, we capture the new `contentY`, adjust
                //        `contentY` to it is old value then enable the animation and set `contentY`
                //        to its new value.
                const oldContentY = listView.contentY
                listView.positionViewAtIndex(MainPlaylistController.currentIndex, ListView.Contain)
                const newContentY = listView.contentY
                if (Math.abs(oldContentY - newContentY) >= Number.EPSILON) {
                    contentYBehavior.enabled = false
                    listView.contentY = oldContentY
                    contentYBehavior.enabled = true
                    listView.contentY = newContentY
                    contentYBehavior.enabled = false
                }
            }

            Component.onCompleted: {
                // WARNING: Tracking the current item and not the current index is intentional here.
                MainPlaylistController.currentItemChanged.connect(listView, () => {
                    listView.positionAtCurrent(true)
                })

                Qt.callLater(listView.positionAtCurrent, false)
            }

            Connections {
                target: listView.model

                function onRowsInserted() {
                    if (listView.currentIndex === -1)
                        listView.currentIndex = 0
                }

                function onModelReset() {
                    if (listView.currentIndex === -1 && listView.count > 0)
                        listView.currentIndex = 0
                }
            }

            delegate: PlaylistDelegate {
                id: delegate

                width: listView.contentWidth
                leftPadding: VLCStyle.spacing_20
                rightPadding: Math.max(listView.T.ScrollBar.vertical.width, VLCStyle.spacing_20)

                contextMenu: root.contextMenu

                dragItem: root.dragItem

                isDropAcceptable: listView.isDropAcceptableFunc
                acceptDrop: listView.acceptDropFunc

                onContainsDragChanged: listView.updateItemContainsDrag(this, containsDrag)

                onMoveRequested: (offset) => root.moveSelection(offset)

                onIsCurrentChanged: {
                    if (isCurrent)
                        listView.fadingEdge.excludeItem = delegate
                    else if (listView.fadingEdge.excludeItem === delegate)
                        listView.fadingEdge.excludeItem = null
                }
            }

            Keys.onDeletePressed: root.removeSelection()

            Navigation.parentItem: root
            Navigation.upItem: sort

            onActionAtIndex: (index) => {
                if (index < 0)
                    return

                MainPlaylistController.goTo(index, true)
            }

            Column {
                id: noContentInfoColumn

                anchors.centerIn: parent

                visible: false
                enabled: visible

                spacing: VLCStyle.margin_small

                Binding on visible {
                    delayed: true
                    value: (listView.count === 0 && !listView.footerItem.firstItemIndicatorVisible)
                }

                Widgets.TwoToneIcon {
                    anchors.horizontalCenter: parent.horizontalCenter

                    name: "queue"

                    pixelSize: VLCStyle.dp(48, VLCStyle.scale)
                }

                T.Label {
                    anchors.horizontalCenter: parent.horizontalCenter

                    horizontalAlignment: Text.AlignHCenter

                    text: qsTr("The Queue is empty")

                    color: theme.fg.primary

                    font.pixelSize: VLCStyle.fontSize_large
                }

                T.Label {
                    anchors.horizontalCenter: parent.horizontalCenter

                    horizontalAlignment: Text.AlignHCenter

                    text: qsTr("Drag files here or use +")

                    color: theme.fg.secondary

                    font.pixelSize: VLCStyle.fontSize_normal
                }
            }
        }
    }

    Keys.priority: Keys.AfterItem
    Keys.forwardTo: listView
    Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)

    component PlaylistToggleButton : Widgets.IconToolButton {
        id: playlistBtn

        focus: true

        font.pixelSize: VLCStyle.icon_banner
        text: VLCIcons.queue
        description: qsTr("Queue")

        AccessibleCompat.id: "playqueueToggle"

        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height

        property Item plListView

        DropArea {
            anchors.fill: parent

            onContainsDragChanged: {
                if (containsDrag) {
                    _timer.restart()

                    if (playlistBtn.plListView)
                        MainCtx.setCursor(Qt.DragCopyCursor)
                } else {
                    _timer.stop()

                    if (playlistBtn.plListView)
                        MainCtx.restoreCursor()
                }
            }

            onEntered: (drag) => {
                const plListView = playlistBtn.plListView
                if (plListView) {
                    console.assert(plListView.isDropAcceptableFunc)
                    console.assert(plListView.model)
                    if (plListView.isDropAcceptableFunc(drag, plListView.model.count)) {
                        drag.accept()
                    } else {
                        drag.accepted = false
                    }
                } else {
                    drag.accepted = false
                }
            }

            onDropped: (drop) => {
                const plListView = playlistBtn.plListView
                if (plListView) {
                    console.assert(plListView.acceptDropFunc)
                    plListView.acceptDropFunc(plListView.model.count, drop)
                }
            }

            Timer {
                id: _timer
                interval: VLCStyle.duration_humanMoment

                onTriggered: {
                    MainCtx.playqueuePanel.visible = true
                }
            }
        }
    }
}
