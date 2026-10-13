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
import VLC.Playlist
import VLC.Widgets as Widgets
import VLC.Style

// Open Disc: pick a drive, its disc type is detected, then Play.
ModalDialog {
    id: root

    title: qsTr("Open Disc")

    width: Math.min(VLCStyle.dp(560, VLCStyle.scale),
                    (T.Overlay.overlay?.width ?? Infinity) - 2 * VLCStyle.margin_large)

    padding: 0
    topPadding: VLCStyle.spacing_20
    bottomPadding: VLCStyle.spacing_20
    leftPadding: VLCStyle.spacing_20
    rightPadding: VLCStyle.spacing_20
    spacing: 0

    // the drive row, -1 when there is none
    readonly property int driveIndex: driveBox.currentIndex
    readonly property int detectedType: (driveIndex >= 0) ? (driveBox.currentValue ?? DiscModel.NO_DISC)
                                                          : DiscModel.NO_DISC
    readonly property int discType: typeBox.currentValue ?? DiscModel.DVD
    readonly property bool canPlay: driveIndex >= 0 && detectedType !== DiscModel.NO_DISC
                                    && !discModel.scanning

    readonly property bool hasMenus: discType === DiscModel.DVD || discType === DiscModel.BLURAY
    readonly property bool hasChapters: discType === DiscModel.DVD || discType === DiscModel.BLURAY
    readonly property bool hasTracks: discType !== DiscModel.AUDIO_CD

    function play() {
        if (!canPlay)
            return
        if (discModel.play(driveIndex, discType, menusSwitch.checked,
                           titleSpin.value, hasChapters ? chapterSpin.value : 0,
                           hasTracks ? audioSpin.value : 0, hasTracks ? subtitleSpin.value : 0))
            root.close()
    }

    onAboutToShow: {
        menusSwitch.checked = true
        moreOptions.expanded = false
        titleSpin.value = 0
        chapterSpin.value = 0
        audioSpin.value = 0
        subtitleSpin.value = 0
        flickable.contentY = 0
        discModel.refresh()
    }

    onOpened: driveBox.forceActiveFocus(Qt.TabFocusReason)

    DiscModel {
        id: discModel

        playlistController: MainPlaylistController

        onScanningChanged: {
            if (!scanning)
                driveBox.currentIndex = defaultIndex
        }
    }

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

        Keys.onReturnPressed: root.play()
        Keys.onEnterPressed: root.play()

        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

        ColumnLayout {
            id: column

            width: flickable.width
            spacing: VLCStyle.spacing_20

            // Drive
            ColumnLayout {
                spacing: VLCStyle.spacing_10
                Layout.fillWidth: true

                SectionTitle {
                    text: qsTr("Drive")
                    description: {
                        if (discModel.scanning)
                            return qsTr("Looking for discs…")
                        if (driveBox.count === 0)
                            return qsTr("No disc drive was found.")
                        switch (root.detectedType) {
                        case DiscModel.NO_DISC:
                            return qsTr("There is no disc in this drive.")
                        case DiscModel.UNKNOWN:
                            return qsTr("Unrecognized disc: choose its type.")
                        default: {
                            const index = typeBox.indexOfValue(root.detectedType)
                            return qsTr("%1 detected.").arg(typeBox.textAt(index))
                        }
                        }
                    }
                }

                RowLayout {
                    spacing: VLCStyle.spacing_10
                    Layout.fillWidth: true

                    Widgets.ComboBoxExt {
                        id: driveBox

                        model: discModel
                        textRole: "name"
                        valueRole: "type"

                        enabled: count > 0
                        displayText: count > 0 ? currentText : qsTr("No drive")

                        font.pixelSize: VLCStyle.fontSize_normal

                        Accessible.name: qsTr("Drive")

                        Layout.fillWidth: true
                        Layout.preferredHeight: VLCStyle.dp(36, VLCStyle.scale)

                        onCurrentValueChanged: {
                            // follow the detection, keep DVD for an unrecognized disc
                            const index = typeBox.indexOfValue(currentValue)
                            typeBox.currentIndex = (index >= 0) ? index : 0
                        }
                    }

                    Widgets.ComboBoxExt {
                        id: typeBox

                        enabled: root.canPlay

                        textRole: "text"
                        valueRole: "type"
                        model: [
                            { text: qsTr("DVD"), type: DiscModel.DVD },
                            { text: qsTr("Blu-ray"), type: DiscModel.BLURAY },
                            { text: qsTr("Audio CD"), type: DiscModel.AUDIO_CD },
                            { text: qsTr("VCD"), type: DiscModel.VCD }
                        ]

                        font.pixelSize: VLCStyle.fontSize_normal

                        Accessible.name: qsTr("Disc type")

                        Layout.preferredWidth: VLCStyle.dp(130, VLCStyle.scale)
                        Layout.preferredHeight: VLCStyle.dp(36, VLCStyle.scale)
                    }

                    Widgets.IconToolButton {
                        text: VLCIcons.ic_fluent_arrow_sync_24_regular
                        description: qsTr("Look for discs again")
                        enabled: !discModel.scanning
                        font.pixelSize: VLCStyle.icon_normal
                        Layout.preferredWidth: VLCStyle.dp(36, VLCStyle.scale)
                        Layout.preferredHeight: VLCStyle.dp(36, VLCStyle.scale)
                        onClicked: discModel.refresh()
                    }
                }
            }

            Separator { }

            // Use disc menus
            RowLayout {
                spacing: VLCStyle.spacing_20
                enabled: root.hasMenus
                opacity: enabled ? 1.0 : 0.5
                Layout.fillWidth: true

                SectionTitle {
                    text: qsTr("Use disc menus")
                    description: root.hasMenus ? qsTr("Start from the menu of the disc.")
                                               : qsTr("This disc has no menus.")
                    Layout.fillWidth: true
                }

                // CheckButton misplaces its handle when it is resized while
                // checked: keep the layout from resizing it, and check it once
                // it is laid out (onAboutToShow)
                Item {
                    implicitWidth: menusSwitch.width
                    implicitHeight: menusSwitch.height

                    Widgets.CheckButton {
                        id: menusSwitch
                        text: qsTr("Use disc menus")
                        Accessible.name: text
                    }
                }
            }

            Separator { }

            // More options
            ColumnLayout {
                id: moreOptions

                property bool expanded: false

                // bring the options into view in a small window
                onExpandedChanged: {
                    if (expanded)
                        Qt.callLater(() => flickable.contentY = Math.max(0, flickable.contentHeight - flickable.height))
                }

                spacing: VLCStyle.spacing_10
                Layout.fillWidth: true

                T.AbstractButton {
                    id: moreButton

                    text: qsTr("More options")
                    onClicked: moreOptions.expanded = !moreOptions.expanded

                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.onPressAction: moreButton.clicked()

                    implicitWidth: implicitContentWidth + leftPadding + rightPadding
                    implicitHeight: implicitContentHeight + topPadding + bottomPadding
                    padding: VLCStyle.spacing_4
                    leftPadding: 0
                    rightPadding: 0

                    Layout.fillWidth: true

                    readonly property ColorContext colorContext: ColorContext {
                        id: moreTheme
                        colorSet: ColorContext.ButtonStandard
                        focused: moreButton.visualFocus
                        hovered: moreButton.hovered
                        pressed: moreButton.down
                    }

                    contentItem: RowLayout {
                        spacing: VLCStyle.spacing_8

                        T.Label {
                            text: moreButton.text
                            color: moreTheme.fg.primary
                            font.pixelSize: VLCStyle.fontSize_xlarge
                            Layout.fillWidth: true
                        }

                        Widgets.IconLabel {
                            text: VLCIcons.expand
                            rotation: moreOptions.expanded ? 180 : 0
                            color: moreTheme.fg.primary
                            font.pixelSize: VLCStyle.icon_normal
                        }
                    }

                    background: Rectangle {
                        color: "transparent"
                        radius: VLCStyle.radius_small
                        border.width: VLCStyle.focus_border
                        border.color: moreButton.visualFocus ? moreTheme.visualFocus : "transparent"
                    }
                }

                GridLayout {
                    visible: moreOptions.expanded

                    columns: 4
                    columnSpacing: VLCStyle.spacing_12
                    rowSpacing: VLCStyle.spacing_10
                    Layout.fillWidth: true

                    OptionLabel {
                        text: {
                            switch (root.discType) {
                            case DiscModel.AUDIO_CD: return qsTr("Track")
                            case DiscModel.VCD: return qsTr("Entry")
                            default: return qsTr("Title")
                            }
                        }
                    }
                    OptionSpin { id: titleSpin; Accessible.name: qsTr("Title") }

                    OptionLabel {
                        visible: root.hasChapters
                        text: qsTr("Chapter")
                    }
                    OptionSpin {
                        id: chapterSpin
                        visible: root.hasChapters
                        // a chapter needs a title
                        enabled: titleSpin.value > 0
                        Accessible.name: qsTr("Chapter")
                    }

                    OptionLabel {
                        visible: root.hasTracks
                        text: qsTr("Audio track")
                    }
                    OptionSpin { id: audioSpin; visible: root.hasTracks; Accessible.name: qsTr("Audio track") }

                    OptionLabel {
                        visible: root.hasTracks
                        text: qsTr("Subtitle track")
                    }
                    OptionSpin { id: subtitleSpin; visible: root.hasTracks; Accessible.name: qsTr("Subtitle track") }
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
            text: qsTr("Play")
            enabled: root.canPlay
            colorContext.colorSet: ColorContext.ButtonAccent
            radius: VLCStyle.radius_normal
            Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
            Layout.margins: VLCStyle.spacing_20
            onClicked: root.play()
        }
    }

    component SectionTitle: ColumnLayout {
        id: section

        property alias text: titleLabel.text
        property alias description: descriptionLabel.text

        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        spacing: VLCStyle.spacing_5
        Layout.fillWidth: true

        T.Label {
            id: titleLabel
            color: section.colorContext.fg.primary
            font.pixelSize: VLCStyle.fontSize_xlarge
            Layout.fillWidth: true
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

    component OptionLabel: T.Label {
        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        color: colorContext.fg.primary
        font.pixelSize: VLCStyle.fontSize_normal
        Layout.fillWidth: true
    }

    component OptionSpin: Widgets.SpinBoxExt {
        from: 0
        to: 999
        editable: true
        font.pixelSize: VLCStyle.fontSize_normal
        textFromValue: (value, locale) => (value === 0) ? qsTr("Default") : Number(value).toLocaleString(locale, 'f', 0)
        valueFromText: (text, locale) => {
            const value = Number.fromLocaleString(locale, text)
            return isNaN(value) ? 0 : value
        }
        Layout.preferredWidth: VLCStyle.dp(110, VLCStyle.scale)
        Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
    }
}
