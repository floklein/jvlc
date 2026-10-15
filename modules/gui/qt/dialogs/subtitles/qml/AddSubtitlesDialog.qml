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
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Dialogs
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

// Add Subtitles (Figma 1919:8707, target menu 1959:5325, source menu
// 1963:5465): a target and a source, then the subtitle files to attach.
// To This File, From Computer lists the files next to the video and in its
// "Subs" folder; Save attaches the checked ones as Subtitle matches.
ModalDialog {
    id: root

    // the AddSubtitlesModel given by DialogsProvider, which owns it
    property AddSubtitlesModel model: null

    // the dark "well" behind the header and the checked files: the
    // background of a text field
    readonly property ColorContext _wellTheme: ColorContext {
        palette: root.colorContext.palette
        colorSet: ColorContext.TextField
    }
    readonly property color _well: _wellTheme.bg.primary

    title: qsTr("Add Subtitles")

    padding: 0
    topPadding: 0

    width: Math.min(VLCStyle.dp(760, VLCStyle.scale), (parent ? parent.width : Infinity) - VLCStyle.margin_large * 2)
    height: Math.min(VLCStyle.dp(560, VLCStyle.scale), (parent ? parent.height : Infinity) - VLCStyle.margin_large * 2)

    function openWith(model) {
        root.model = model
        open()
        // no row is current until the keyboard moves to one
        list.currentIndex = -1
        list.forceActiveFocus(Qt.PopupFocusReason)
    }

    function save() {
        if (!root.model || root.model.checkedCount === 0)
            return
        root.model.save()
        close()
    }

    onClosed: root.model = null

    header: Rectangle {
        // NOTE: Popup sections are outside the item tree of the dialog colorContext.
        readonly property ColorContext colorContext: ColorContext {
            palette: root.colorContext.palette
            colorSet: ColorContext.Window
        }

        implicitHeight: headerColumn.implicitHeight
        color: root._well
        radius: VLCStyle.radius_large

        // square the bottom corners
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.radius
            color: parent.color
        }

        ColumnLayout {
            id: headerColumn

            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0

            Item {
                Layout.fillWidth: true
                implicitHeight: VLCStyle.dp(44, VLCStyle.scale)

                T.Label {
                    anchors.left: parent.left
                    anchors.leftMargin: VLCStyle.margin_normal
                    anchors.verticalCenter: parent.verticalCenter

                    text: root.title
                    color: root.colorContext.fg.primary
                    font.pixelSize: VLCStyle.fontSize_normal

                    Accessible.role: Accessible.Heading
                }

                Widgets.IconToolButton {
                    anchors.right: parent.right
                    anchors.rightMargin: VLCStyle.margin_xsmall
                    anchors.verticalCenter: parent.verticalCenter

                    text: VLCIcons.close
                    description: qsTr("Close")

                    onClicked: root.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_small
                Layout.rightMargin: VLCStyle.margin_small
                Layout.bottomMargin: VLCStyle.margin_small

                spacing: VLCStyle.margin_small

                // the target: only To This File for now, folders and
                // Playlists are matched by another flow
                Picker {
                    Accessible.name: qsTr("Add subtitles to")

                    model: [
                        { text: qsTr("To This File"), enabled: true },
                        { text: qsTr("To This Folder"), enabled: false },
                        { text: qsTr("To This Playlist"), enabled: false }
                    ]
                }

                // the source: OpenSubtitles and links come later
                Picker {
                    Accessible.name: qsTr("Add subtitles from")

                    model: [
                        { text: qsTr("From Computer"), enabled: true },
                        { text: qsTr("From OpenSubtitles"), enabled: false },
                        { text: qsTr("From Link…"), enabled: false }
                    ]
                }

                // the folder listed, and Browse… for files elsewhere
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)

                    radius: VLCStyle.radius_small
                    color: root.colorContext.bg.primary

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: VLCStyle.margin_xsmall
                        anchors.rightMargin: VLCStyle.margin_xxsmall

                        spacing: VLCStyle.margin_xsmall

                        Widgets.IconLabel {
                            text: VLCIcons.folder
                            color: root.colorContext.fg.secondary
                            font.pixelSize: VLCStyle.icon_normal
                        }

                        T.Label {
                            Layout.fillWidth: true

                            text: root.model?.folder ?? ""
                            elide: Text.ElideLeft
                            color: root.colorContext.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_normal
                        }

                        Widgets.TextToolButton {
                            text: qsTr("Browse…")

                            onClicked: root.model?.browse()
                        }
                    }
                }
            }
        }
    }

    contentItem: Item {
        readonly property ColorContext colorContext: ColorContext {
            palette: root.colorContext.palette
            colorSet: ColorContext.Window
        }

        ListView {
            id: list

            anchors.fill: parent
            anchors.margins: VLCStyle.margin_normal

            clip: true
            spacing: VLCStyle.dp(4, VLCStyle.scale)
            keyNavigationEnabled: true

            model: root.model

            Accessible.role: Accessible.List
            Accessible.name: qsTr("Subtitle files")

            T.ScrollBar.vertical: Widgets.ScrollBarExt { }

            delegate: T.ItemDelegate {
                id: row

                required property int index
                required property var model

                width: list.width
                implicitHeight: VLCStyle.dp(34, VLCStyle.scale)
                leftPadding: VLCStyle.margin_xsmall
                rightPadding: VLCStyle.margin_xsmall

                highlighted: ListView.isCurrentItem && list.activeFocus

                Accessible.role: Accessible.CheckBox
                Accessible.checkable: true
                Accessible.checked: row.model.checked
                Accessible.name: row.model.name

                onClicked: root.model.toggle(index)


                background: Rectangle {
                    radius: VLCStyle.radius_small
                    color: row.model.checked ? root._well
                         : row.hovered ? root.colorContext.bg.secondary : "transparent"
                    border.width: VLCStyle.focus_border
                    border.color: row.highlighted ? root.colorContext.accent : "transparent"
                }

                contentItem: RowLayout {
                    spacing: VLCStyle.margin_small

                    Widgets.IconLabel {
                        text: VLCIcons.subtitle
                        color: root.colorContext.fg.secondary
                        font.pixelSize: VLCStyle.icon_normal
                    }

                    T.Label {
                        Layout.fillWidth: true

                        text: row.model.name
                        elide: Text.ElideMiddle
                        color: root.colorContext.fg.primary
                        font.pixelSize: VLCStyle.fontSize_normal
                    }

                    Widgets.IconLabel {
                        text: row.model.checked ? VLCIcons.check : ""
                        color: root.colorContext.accent
                        font.pixelSize: VLCStyle.icon_normal
                    }
                }
            }

            Keys.onSpacePressed: {
                if (currentIndex >= 0)
                    root.model.toggle(currentIndex)
            }

            T.Label {
                anchors.centerIn: parent
                width: parent.width - VLCStyle.margin_large * 2

                visible: list.count === 0
                text: qsTr("No subtitle files next to this file. Use Browse… to pick some.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: root.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
            }
        }
    }

    footer: Item {
        readonly property ColorContext colorContext: ColorContext {
            palette: root.colorContext.palette
            colorSet: ColorContext.Window
        }

        implicitHeight: buttons.implicitHeight + VLCStyle.margin_normal * 2

        RowLayout {
            id: buttons

            anchors.fill: parent
            anchors.margins: VLCStyle.margin_normal

            Widgets.ButtonExt {
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                text: qsTr("Cancel")

                onClicked: root.close()
            }

            Item {
                Layout.fillWidth: true
            }

            Widgets.ActionButtonPrimary {
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                text: qsTr("Save")
                enabled: (root.model?.checkedCount ?? 0) > 0

                onClicked: root.save()
            }
        }
    }

    // a target or source menu; the entries that aren't available yet are
    // listed, disabled
    component Picker: Widgets.ComboBoxExt {
        id: picker

        Layout.preferredWidth: VLCStyle.dp(190, VLCStyle.scale)
        Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)

        textRole: "text"
        font.pixelSize: VLCStyle.fontSize_normal

        bgColor: root.colorContext.bg.primary

        delegate: T.ItemDelegate {
            id: entry

            required property var modelData
            required property int index

            width: picker.width
            height: implicitContentHeight + topPadding + bottomPadding
            padding: VLCStyle.margin_xsmall
            leftPadding: picker.leftPadding

            enabled: modelData.enabled
            highlighted: picker.highlightedIndex === index

            contentItem: RowLayout {
                Widgets.ListLabel {
                    Layout.fillWidth: true

                    text: entry.modelData.text
                    color: picker.color
                    opacity: entry.enabled ? 1.0 : 0.4
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Widgets.IconLabel {
                    text: picker.currentIndex === entry.index ? VLCIcons.check : ""
                    color: root.colorContext.accent
                    font.pixelSize: VLCStyle.icon_normal
                }
            }
        }
    }
}
