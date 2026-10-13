/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Leon Vitanos <leon.vitanos@gmail.com>
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


import VLC.MainInterface
import VLC.Style
import VLC.Util

T.Pane {
    id: root

    // Properties
    required property Item view

    leftPadding: view?.contentLeftMargin ?? 0
    rightPadding: view?.contentRightMargin ?? 0

    bottomPadding: VLCStyle.layoutTitle_bottom_padding
    topPadding: VLCStyle.layoutTitle_top_padding

    // Aliases

    property alias text: label.text

    property alias seeAllButton: button

    // Shows the "See All" button as a chevron right after the title
    property bool seeAllChevron: false

    // Signals

    signal seeAllButtonClicked(int reason)

    // Settings

    width: view.width
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    Navigation.parentItem: view
    Navigation.navigable: button.visible

    Component.onCompleted: {
        // Qt Quick Pane sets a cursor for itself, unset it so that if the view has
        // busy cursor, it is visible over the header:
        MainCtx.unsetCursor(this)
    }

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // Events
    onContentItemChanged: {
      console.assert(contentItem === root.row)
    }

    RowLayout {
        id: row

        anchors.fill: parent

        SubtitleLabel {
            id: label

            Layout.fillWidth: !root.seeAllChevron

            color: theme.fg.primary
        }

        TextToolButton {
            id: button

            visible: false

            Layout.preferredWidth: implicitWidth

            focus: true

            Binding on focusReason {
                // NOTE: This is an explicit binding so that it does not get overridden:
                value: root.focusReason
            }

            text: root.seeAllChevron ? VLCIcons.ic_fluent_chevron_right_24 : qsTr("See All")

            font.family: root.seeAllChevron ? VLCIcons.fontFamily : VLCStyle.fontFamily
            font.pixelSize: root.seeAllChevron ? VLCStyle.fontSize_xlarge : VLCStyle.fontSize_large

            Accessible.name: qsTr("See All")

            Navigation.parentItem: root

            onClicked: root.seeAllButtonClicked(focusReason)
        }

        Item {
            visible: root.seeAllChevron

            Layout.fillWidth: true
        }
    }
}
