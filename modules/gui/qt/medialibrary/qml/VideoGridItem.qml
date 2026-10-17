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

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

Widgets.GridItem {
    id: root

    property bool showNewIndicator: false
    
    property var labels

    // the view's model and the row of this item in it, so that Play follows
    // the Play rule (the video and the videos after it in the view)
    property var viewModel: null
    property int viewIndex: -1

    // a folder of the Videos tree opens instead of playing: its activation
    // (Enter, screen readers) shows its content
    readonly property bool isFolder: model?.isFolder === true

    function play() {
        if (isFolder) {
            root.itemDoubleClicked(Qt.NoModifier)
            return
        }

        if (viewModel && viewIndex >= 0) {
            viewModel.addAndPlay([viewModel.index(viewIndex, 0)])
            MainCtx.playerView = true
        } else if ( model.id !== undefined ) {
            MediaLib.addAndPlay( model.id )
            MainCtx.playerView = true
        }
    }

    // in progress: the played part; watched to the end: a full bar and a
    // dimmed title (Figma 970:4028)
    readonly property real progress: (model?.progress ?? 0) > 0 ? Helpers.clamp(model.progress, 0, 1) : 0
    readonly property bool watched: !isFolder && (model?.playcount ?? 0) > 0

    playCoverShowPlay: !isFolder && implicitPlayCoverShowPlay

    // a folder is the folder tile, never a mosaic (Figma 6284:13980); a
    // video without thumbnail is the plain tile (6270:13777)
    image: isFolder ? "" : (model.thumbnail || "")
    fallbackImage: VLCStyle.noArtTile
    fillMode: Image.Stretch

    title: model.title || qsTr("Unknown title")
    // Figma 6272:13852: a video card shows its title only, no duration
    subtitle: ""
    dimmed: watched && progress <= 0
    pictureWidth: VLCStyle.gridCover_video_width
    pictureHeight: VLCStyle.gridCover_video_height

    // Figma 6272:13854: 4 px of Accent/Main along the bottom of the cover,
    // inside it and cut by its corners, with no track
    Widgets.VideoProgressBar {
        id: progressBar

        parent: root.mediaCover

        z: 1

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        visible: !root.isFolder && (root.progress > 0 || root.watched)

        radius: root.pictureRadius
        value: root.progress > 0 ? root.progress : 1
        color: root.colorContext.accent
        trackColor: "transparent"
    }

    pictureOverlay: Item {
        implicitWidth: root.pictureWidth
        implicitHeight: root.pictureHeight

        // Figma 6284:13982: the 48 px folder outline in FG/A2, centered
        Widgets.IconLabel {
            anchors.centerIn: parent

            visible: root.isFolder

            text: VLCIcons.folder
            font.pixelSize: VLCStyle.gridCover_folderIcon
            color: root.colorContext.fg.value
        }

        // not in Figma: a dot of Accent/Main for a video never played, 8 px
        // from the corner or before the labels (as the "sort active" dot of
        // the header)
        Rectangle {
            anchors.right: labelsRow.visible ? labelsRow.left : parent.right
            anchors.rightMargin: VLCStyle.spacing_8
            y: labelsRow.visible ? labelsRow.y + Math.round((labelsRow.height - height) / 2)
                                 : VLCStyle.spacing_8

            width: VLCStyle.gridItem_newIndicator
            height: width
            radius: width / 2

            visible: root.showNewIndicator

            color: root.colorContext.accent
        }

        Widgets.VideoQualityLabels {
            id: labelsRow

            anchors {
                top: parent.top
                right: parent.right
                topMargin: VLCStyle.spacing_8
                rightMargin: VLCStyle.spacing_8
            }

            visible: (root.labels?.length ?? 0) > 0

            labels: root.labels
        }
    }

    onPlayClicked: root.play()
}
