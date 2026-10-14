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
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Dialogs
import VLC.Network
import VLC.Playlist
import VLC.Widgets as Widgets
import VLC.Style

// Open Network Stream: one URL per line and an optional Playlist name.
// Play plays and saves into Network, Save only saves. One URL becomes a Saved
// stream; several URLs, or a list URL (.m3u), become a Network Playlist.
ModalDialog {
    id: root

    title: qsTr("Open Network Stream")

    width: Math.min(VLCStyle.dp(640, VLCStyle.scale),
                    (T.Overlay.overlay?.width ?? Infinity) - 2 * VLCStyle.margin_large)

    padding: 0
    topPadding: VLCStyle.spacing_20
    bottomPadding: VLCStyle.spacing_20
    leftPadding: VLCStyle.spacing_20
    rightPadding: VLCStyle.spacing_20
    spacing: 0

    readonly property var urls: SavedStreams.urlsFromText(urlArea.text)
    readonly property bool canOpen: urls.length > 0

    // save what was entered, returns the saved row or null
    function _save() {
        if (!canOpen)
            return null
        const id = SavedStreams.save(urlArea.text, nameField.text)
        return (id !== "") ? SavedStreams.get(id) : null
    }

    function play() {
        const stream = _save()
        if (!stream)
            return
        MainPlaylistController.playMedia(stream.urls)
        root.close()
    }

    function save() {
        if (_save())
            root.close()
    }

    onAboutToShow: {
        urlArea.clear()
        nameField.clear()
    }

    onOpened: urlArea.forceActiveFocus(Qt.TabFocusReason)

    background: Rectangle {
        color: root.colorContext.bg.primary
        radius: VLCStyle.radius_xlarge

        Widgets.RoundedRectangleShadow {
            yOffset: VLCStyle.dp(8, VLCStyle.scale)
            blurRadius: VLCStyle.dp(15, VLCStyle.scale)
            radius: parent.radius
            opacity: 0.46
            color: root.colorContext.shadow.alpha(1.0)
        }
    }

    header: Rectangle {
        id: headerBar

        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        implicitHeight: VLCStyle.dp(50, VLCStyle.scale)

        // only the top corners are rounded
        radius: VLCStyle.radius_xlarge
        color: headerBar.colorContext.bg.secondary

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.radius
            color: parent.color
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.spacing_8
            anchors.leftMargin: VLCStyle.spacing_20

            T.Label {
                text: root.title
                color: headerBar.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Widgets.IconToolButton {
                text: VLCIcons.close
                description: qsTr("Close")
                font.pixelSize: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                focusPolicy: Qt.NoFocus
                onClicked: root.close()
            }
        }
    }

    // scrolls when the window is too small for the dialog
    contentItem: Flickable {
        id: flickable

        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        contentWidth: width
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: contentHeight > height

        // popup items don't inherit the window palette
        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

        ColumnLayout {
            id: column

            width: flickable.width
            spacing: VLCStyle.spacing_20

            // Network URL
            ColumnLayout {
                spacing: VLCStyle.spacing_10
                Layout.fillWidth: true

                SectionTitle {
                    text: qsTr("Network URL")
                    description: qsTr("You can enter multiple links to load.")
                }

                ScrollView {
                    id: urlScroll

                    Layout.fillWidth: true
                    Layout.preferredHeight: VLCStyle.dp(150, VLCStyle.scale)

                    TextArea {
                        id: urlArea

                        readonly property ColorContext colorContext: ColorContext {
                            id: urlTheme
                            palette: VLCStyle.palette
                            colorSet: ColorContext.TextField
                            focused: urlArea.activeFocus
                            hovered: urlArea.hovered
                            enabled: urlArea.enabled
                        }

                        placeholderText: qsTr("e.g., https://www.example.com/stream.avi")
                        placeholderTextColor: urlTheme.fg.secondary
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                        textFormat: TextEdit.PlainText

                        color: urlTheme.fg.primary
                        selectionColor: urlTheme.bg.highlight
                        selectedTextColor: urlTheme.fg.highlight
                        font.pixelSize: VLCStyle.fontSize_normal
                        padding: VLCStyle.spacing_12

                        KeyNavigation.priority: KeyNavigation.BeforeItem
                        KeyNavigation.tab: nameField

                        Accessible.name: qsTr("Network URL")

                        // Ctrl+Enter plays, Enter starts a new line
                        Keys.onPressed: (event) => {
                            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                    && (event.modifiers & Qt.ControlModifier)) {
                                event.accepted = true
                                root.play()
                            }
                        }

                        background: Rectangle {
                            color: urlTheme.bg.primary
                            border.width: urlArea.activeFocus ? VLCStyle.focus_border : 0
                            border.color: urlTheme.visualFocus
                            radius: VLCStyle.radius_small
                        }
                    }
                }
            }

            Separator { }

            // Playlist name
            ColumnLayout {
                spacing: VLCStyle.spacing_10
                Layout.fillWidth: true

                SectionTitle {
                    text: qsTr("Playlist Name")
                    optionalText: qsTr("(optional)")
                    description: qsTr("The playlist will be saved and displayed with this title.")
                }

                Widgets.TextFieldExt {
                    id: nameField

                    placeholderText: qsTr("e.g., Favorite TV Channels")
                    radius: VLCStyle.radius_small
                    selectByMouse: true

                    Layout.fillWidth: true
                    Layout.preferredHeight: VLCStyle.dp(36, VLCStyle.scale)

                    Accessible.name: qsTr("Playlist Name")

                    onAccepted: root.play()
                }
            }
        }
    }

    footer: RowLayout {
        spacing: VLCStyle.spacing_10

        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        Widgets.ButtonExt {
            text: qsTr("Cancel")
            colorContext.colorSet: ColorContext.Window
            radius: VLCStyle.radius_normal
            Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
            Layout.margins: VLCStyle.spacing_20
            onClicked: root.close()
        }

        Item { Layout.fillWidth: true }

        Widgets.ButtonExt {
            text: qsTr("Save")
            enabled: root.canOpen
            colorContext.colorSet: ColorContext.ButtonStandard
            radius: VLCStyle.radius_normal
            Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
            Layout.topMargin: VLCStyle.spacing_20
            Layout.bottomMargin: VLCStyle.spacing_20
            onClicked: root.save()
        }

        Widgets.ButtonExt {
            text: qsTr("Play")
            enabled: root.canOpen
            colorContext.colorSet: ColorContext.ButtonAccent
            radius: VLCStyle.radius_normal
            Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
            Layout.margins: VLCStyle.spacing_20
            Layout.leftMargin: 0
            onClicked: root.play()
        }
    }

    component SectionTitle: ColumnLayout {
        id: section

        property alias text: titleLabel.text
        property alias optionalText: optionalLabel.text
        property alias description: descriptionLabel.text

        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        spacing: VLCStyle.spacing_5
        Layout.fillWidth: true

        RowLayout {
            spacing: VLCStyle.spacing_5
            Layout.fillWidth: true

            T.Label {
                id: titleLabel
                color: section.colorContext.fg.primary
                font.pixelSize: VLCStyle.fontSize_xlarge
            }

            T.Label {
                id: optionalLabel
                visible: text !== ""
                color: section.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_xlarge
                Layout.fillWidth: true
            }
        }

        T.Label {
            id: descriptionLabel
            visible: text !== ""
            color: section.colorContext.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    component Separator: Rectangle {
        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        implicitHeight: VLCStyle.border
        color: colorContext.separator
        Layout.fillWidth: true
    }
}
