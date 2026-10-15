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
import QtQuick.Window
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs

// Onboarding: library folders, Theme and Accent color, Privacy, then the
// basic shortcuts. Finish or Skip applies the choices and lands on Home.
Window {
    id: root

    transientParent: MainCtx.intfMainWindow
    flags: Qt.Dialog
    modality: Qt.WindowModal

    title: qsTr("Welcome to VLC")

    // NOTE: At large scales the sizes are clamped to the screen, the pages scroll.
    width: Math.min(VLCStyle.dp(980, VLCStyle.scale), Screen.desktopAvailableWidth * 0.9)
    height: Math.min(VLCStyle.dp(690, VLCStyle.scale), Screen.desktopAvailableHeight * 0.9)
    minimumWidth: Math.min(VLCStyle.dp(760, VLCStyle.scale), Screen.desktopAvailableWidth * 0.9)
    minimumHeight: Math.min(VLCStyle.dp(560, VLCStyle.scale), Screen.desktopAvailableHeight * 0.9)

    color: theme.bg.primary

    // the Theme step's swatches picture the Light and Dark Themes whatever
    // the palette in use, so they are not palette colours
    readonly property color _lightSwatch: "#b5b5b5"
    readonly property color _darkSwatch: "#222222"

    // the choices applied: the window can go
    signal finished()

    function openOn(step: int) {
        onboarding.setStep(step)
        root.show()
        root.raise()
        root.requestActivate()
    }

    Component.onCompleted: {
        if (MainCtx.createWindowWithoutRedirectionSurface) { // Win32
            const ret = MainCtx.createWindowWithoutRedirectionSurface(this)
            if (!ret)
                console.debug("MainCtx::createWindowWithoutRedirectionSurface(): returned false for window", this)
        }
    }

    // closing the window is Skip
    onClosing: {
        if (!onboarding.done)
            onboarding.skip()
    }

    OnboardingModel {
        id: onboarding
        ctx: MainCtx

        onFinished: {
            History.push(["home"])
            root.close()
            root.finished()
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: onboarding.skip()
    }

    readonly property real contentWidth: VLCStyle.dp(620, VLCStyle.scale)

    component Title: Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: root.contentWidth
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: theme.fg.primary
        font.pixelSize: VLCStyle.dp(28, VLCStyle.scale)
        font.family: VLCStyle.fontFamily
        Accessible.role: Accessible.Heading
        Accessible.name: text
    }

    component Paragraph: Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: root.contentWidth
        wrapMode: Text.WordWrap
        lineHeight: 1.4
        color: theme.fg.secondary
        font.pixelSize: VLCStyle.fontSize_large
        font.family: VLCStyle.fontFamily
        Accessible.role: Accessible.StaticText
        Accessible.name: text
    }

    // a key as drawn on the keyboard
    component KeyCap: Rectangle {
        property alias text: capText.text

        implicitWidth: Math.max(VLCStyle.dp(42, VLCStyle.scale), capText.implicitWidth + 2 * VLCStyle.spacing_14)
        implicitHeight: VLCStyle.dp(38, VLCStyle.scale)
        radius: VLCStyle.radius_normal
        color: theme.bg.secondary
        border.width: 1
        border.color: theme.border

        // the key's side
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.topMargin: VLCStyle.spacing_4
            anchors.bottomMargin: -VLCStyle.dp(4, VLCStyle.scale)
            radius: parent.radius
            color: theme.separator
        }

        Text {
            id: capText
            anchors.centerIn: parent
            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
            font.family: VLCStyle.fontFamily
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // the contexts of the items below inherit this palette
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        // fields and the video of the preview
        readonly property ColorContext viewColorContext: ColorContext {
            id: viewTheme
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        // one page per step
        Flickable {
            id: pages

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: VLCStyle.margin_large

            // the page fills the window, and scrolls when the window is too small for it
            contentWidth: width
            contentHeight: Math.max(height, [foldersPage, themePage, privacyPage, shortcutsPage]
                                                .find((page) => page.visible)?.implicitHeight ?? 0)
            clip: true
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds

            T.ScrollBar.vertical: Widgets.ScrollBarExt { }

            // 1. Welcome and library folders
            ColumnLayout {
                id: foldersPage

                anchors.fill: parent
                visible: onboarding.page === OnboardingModel.Folders
                spacing: VLCStyle.spacing_20

                Item { Layout.fillHeight: true }

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: VLCStyle.dp(96, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(96, VLCStyle.scale)
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:///misc/cone.svg"
                    Accessible.ignored: true
                }

                Title {
                    text: qsTr("Welcome to VLC!")
                }

                Paragraph {
                    text: qsTr("VLC is a free and open source cross-platform multimedia player "
                               + "that plays most multimedia files as well as DVDs, Audio CDs, "
                               + "VCDs, and various streaming protocols.")
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: root.contentWidth
                    Layout.topMargin: VLCStyle.spacing_14
                    text: qsTr("Choose the folders of your library")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    // the rows fill this column, not the window
                    Layout.fillWidth: false
                    Layout.preferredWidth: root.contentWidth
                    spacing: VLCStyle.spacing_8

                    Repeater {
                        model: onboarding.folders

                        delegate: Rectangle {
                            id: folderRow

                            required property int index
                            required property string modelData

                            readonly property string path: onboarding.folderPath(modelData)

                            Layout.fillWidth: true
                            implicitHeight: VLCStyle.dp(40, VLCStyle.scale)
                            radius: VLCStyle.radius_normal
                            color: viewTheme.bg.primary
                            border.width: 1
                            border.color: theme.border

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: VLCStyle.spacing_12
                                anchors.rightMargin: VLCStyle.spacing_4
                                spacing: VLCStyle.spacing_10

                                Widgets.IconLabel {
                                    text: VLCIcons.folder
                                    color: theme.fg.secondary
                                    font.pixelSize: VLCStyle.icon_small
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: folderRow.path
                                    elide: Text.ElideMiddle
                                    color: theme.fg.primary
                                    font.pixelSize: VLCStyle.fontSize_normal
                                    font.family: VLCStyle.fontFamily
                                }

                                Widgets.IconToolButton {
                                    text: VLCIcons.close
                                    font.pixelSize: VLCStyle.icon_small
                                    // NOTE: The glyph stays small, the target is 24 dp (WCAG 2.5.8).
                                    implicitWidth: VLCStyle.icon_normal
                                    implicitHeight: VLCStyle.icon_normal
                                    description: qsTr("Remove %1").arg(folderRow.path)
                                    onClicked: onboarding.removeFolder(folderRow.index)
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: onboarding.folders.length === 0
                        text: qsTr("No folder: add one now, or later from the library.")
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_normal
                        font.family: VLCStyle.fontFamily
                    }

                    Widgets.ButtonExt {
                        text: qsTr("Add folder…")
                        iconTxt: VLCIcons.add
                        onClicked: onboarding.chooseFolder()
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // 2. Theme and Accent color, over a preview of the player
            ColumnLayout {
                id: themePage

                anchors.fill: parent
                visible: onboarding.page === OnboardingModel.Theme
                spacing: VLCStyle.spacing_20

                Rectangle {
                    id: preview

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // room for the Themes between the title bar and the Control bar
                    Layout.minimumHeight: previewThemes.implicitHeight + previewTitleBar.height
                                          + previewControlBar.height + 2 * VLCStyle.spacing_20
                    radius: VLCStyle.radius_normal
                    color: viewTheme.bg.secondary
                    border.width: 1
                    border.color: theme.border
                    clip: true

                    Accessible.ignored: true

                    // title bar
                    Rectangle {
                        id: previewTitleBar
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 1
                        height: VLCStyle.dp(22, VLCStyle.scale)
                        radius: VLCStyle.radius_normal
                        color: theme.bg.primary

                        Image {
                            anchors.left: parent.left
                            anchors.leftMargin: VLCStyle.spacing_8
                            anchors.verticalCenter: parent.verticalCenter
                            width: VLCStyle.dp(12, VLCStyle.scale)
                            height: width
                            sourceSize: Qt.size(width, height)
                            source: "qrc:///misc/cone.svg"
                        }
                    }

                    // Control bar
                    Rectangle {
                        id: previewControlBar
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 1
                        height: VLCStyle.dp(30, VLCStyle.scale)
                        radius: VLCStyle.radius_normal
                        color: theme.bg.primary

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: VLCStyle.spacing_10
                            anchors.rightMargin: VLCStyle.spacing_10
                            spacing: VLCStyle.spacing_10

                            Rectangle {
                                Layout.preferredWidth: VLCStyle.dp(28, VLCStyle.scale)
                                Layout.preferredHeight: VLCStyle.dp(16, VLCStyle.scale)
                                radius: VLCStyle.radius_xsmall
                                color: theme.accent

                                Widgets.IconLabel {
                                    anchors.centerIn: parent
                                    text: VLCIcons.play_filled
                                    color: theme.bg.primary
                                    font.pixelSize: VLCStyle.dp(10, VLCStyle.scale)
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: VLCStyle.dp(8, VLCStyle.scale)
                                radius: height / 2
                                color: theme.bg.highlight

                                Rectangle {
                                    width: parent.width * 0.3
                                    height: parent.height
                                    radius: parent.radius
                                    color: theme.accent
                                }
                            }
                        }
                    }

                    // the Theme, over the video
                    ColumnLayout {
                        id: previewThemes

                        anchors.centerIn: parent
                        spacing: VLCStyle.spacing_20

                        Title {
                            text: qsTr("Select your preferred theme and color.")
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: VLCStyle.dp(40, VLCStyle.scale)

                            Repeater {
                                model: [
                                    { value: ColorSchemeModel.Light, label: qsTr("Light") },
                                    { value: ColorSchemeModel.Dark, label: qsTr("Dark") },
                                    { value: ColorSchemeModel.System, label: qsTr("System") },
                                ]

                                delegate: T.AbstractButton {
                                    id: themeButton

                                    required property var modelData

                                    readonly property bool current: onboarding.theme === modelData.value

                                    implicitWidth: VLCStyle.dp(112, VLCStyle.scale)
                                    implicitHeight: VLCStyle.dp(140, VLCStyle.scale)
                                    focusPolicy: Qt.StrongFocus

                                    text: modelData.label
                                    Accessible.role: Accessible.RadioButton
                                    // the button resets the attached checked state to its own
                                    checked: current
                                    Accessible.checkable: true
                                    Accessible.checked: current
                                    Accessible.name: text
                                    Accessible.onPressAction: themeButton.clicked()

                                    onClicked: onboarding.theme = modelData.value

                                    contentItem: ColumnLayout {
                                        spacing: VLCStyle.spacing_14

                                        Rectangle {
                                            Layout.alignment: Qt.AlignHCenter
                                            Layout.preferredWidth: VLCStyle.dp(108, VLCStyle.scale)
                                            Layout.preferredHeight: VLCStyle.dp(108, VLCStyle.scale)
                                            radius: VLCStyle.radius_large
                                            color: "transparent"
                                            border.width: VLCStyle.dp(3, VLCStyle.scale)
                                            border.color: themeButton.current ? theme.accent
                                                        : themeButton.visualFocus ? theme.visualFocus
                                                        : "transparent"

                                            // illustrations of the two Themes, not of the palette in use
                                            Rectangle {
                                                id: swatch
                                                anchors.fill: parent
                                                anchors.margins: VLCStyle.spacing_8
                                                radius: VLCStyle.radius_normal
                                                color: themeButton.modelData.value === ColorSchemeModel.Light ? root._lightSwatch : root._darkSwatch
                                                clip: true

                                                // System: half light, half dark
                                                Rectangle {
                                                    visible: themeButton.modelData.value === ColorSchemeModel.System
                                                    width: parent.width * 1.5
                                                    height: parent.height * 1.5
                                                    x: parent.width * 0.35
                                                    y: -parent.height * 0.25
                                                    rotation: -25
                                                    transformOrigin: Item.TopLeft
                                                    color: root._lightSwatch
                                                }
                                            }
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: themeButton.text
                                            color: theme.fg.secondary
                                            font.pixelSize: VLCStyle.fontSize_large
                                            font.family: VLCStyle.fontFamily
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Accent colors
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: VLCStyle.spacing_20

                    Repeater {
                        model: onboarding.accentChoices

                        delegate: T.AbstractButton {
                            id: accentButton

                            required property var modelData

                            readonly property bool current: onboarding.accentColor === modelData.value

                            implicitWidth: VLCStyle.dp(48, VLCStyle.scale)
                            implicitHeight: VLCStyle.dp(48, VLCStyle.scale)
                            focusPolicy: Qt.StrongFocus

                            text: modelData.label
                            Accessible.role: Accessible.RadioButton
                            // the button resets the attached checked state to its own
                            checked: current
                            Accessible.checkable: true
                            Accessible.checked: current
                            Accessible.name: qsTr("Accent color: %1").arg(text)
                            Accessible.onPressAction: accentButton.clicked()

                            onClicked: onboarding.accentColor = modelData.value

                            background: Rectangle {
                                radius: width / 2
                                color: "transparent"
                                border.width: VLCStyle.dp(3, VLCStyle.scale)
                                border.color: accentButton.current ? accentButton.modelData.color
                                            : accentButton.visualFocus ? theme.visualFocus
                                            : "transparent"
                            }

                            contentItem: Item {
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: VLCStyle.dp(36, VLCStyle.scale)
                                    height: width
                                    radius: width / 2
                                    color: accentButton.modelData.color
                                }
                            }
                        }
                    }
                }
            }

            // 3. Privacy
            ColumnLayout {
                id: privacyPage

                anchors.fill: parent
                visible: onboarding.page === OnboardingModel.Privacy
                spacing: VLCStyle.spacing_20

                Item { Layout.fillHeight: true }

                Widgets.IconLabel {
                    Layout.alignment: Qt.AlignHCenter
                    text: VLCIcons.topbar_network
                    color: theme.accent
                    font.pixelSize: VLCStyle.dp(64, VLCStyle.scale)
                    Accessible.ignored: true
                }

                Title {
                    text: qsTr("Find titles, artwork and details online.")
                }

                Paragraph {
                    text: qsTr("To find them, VLC sends the names and tags of your media "
                               + "to online services. VLC collects no other data about you.")
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: root.contentWidth
                    Layout.topMargin: VLCStyle.spacing_14
                    implicitHeight: VLCStyle.dp(56, VLCStyle.scale)
                    radius: VLCStyle.radius_normal
                    color: viewTheme.bg.primary

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: VLCStyle.spacing_14
                        anchors.rightMargin: VLCStyle.spacing_14
                        spacing: VLCStyle.spacing_10

                        Text {
                            id: fetchLabel
                            Layout.fillWidth: true
                            text: qsTr("Fetch titles, artwork and details online")
                            elide: Text.ElideRight
                            color: theme.fg.primary
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.family: VLCStyle.fontFamily
                        }

                        SettingsToggle {
                            checked: onboarding.fetchOnline
                            Accessible.name: fetchLabel.text
                            onClicked: onboarding.fetchOnline = !onboarding.fetchOnline
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // 4. The basic shortcuts, with the keys bound now
            ColumnLayout {
                id: shortcutsPage

                anchors.fill: parent
                visible: onboarding.page === OnboardingModel.Shortcuts
                spacing: VLCStyle.dp(32, VLCStyle.scale)

                Title {
                    Layout.topMargin: VLCStyle.margin_normal
                    text: qsTr("Learn the basic keyboard shortcuts.")
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_large
                    Layout.rightMargin: VLCStyle.margin_large
                    columns: 2
                    columnSpacing: VLCStyle.dp(48, VLCStyle.scale)
                    rowSpacing: VLCStyle.spacing_20
                    flow: GridLayout.TopToBottom
                    rows: Math.ceil(onboarding.shortcuts.length / 2)

                    Repeater {
                        model: onboarding.shortcuts

                        delegate: RowLayout {
                            id: shortcutRow

                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: VLCStyle.spacing_8

                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("%1: %2").arg(modelData.label)
                                                           .arg(modelData.keys.map((key) => key.text).join(" / "))

                            Text {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                text: shortcutRow.modelData.label
                                elide: Text.ElideRight
                                color: theme.fg.secondary
                                font.pixelSize: VLCStyle.fontSize_normal
                                font.family: VLCStyle.fontFamily
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                spacing: VLCStyle.spacing_8

                                Repeater {
                                    model: shortcutRow.modelData.keys

                                    delegate: RowLayout {
                                        required property var modelData
                                        required property int index

                                        spacing: VLCStyle.spacing_4

                                        Text {
                                            visible: index > 0
                                            text: "/"
                                            color: theme.fg.secondary
                                            font.pixelSize: VLCStyle.fontSize_normal
                                            font.family: VLCStyle.fontFamily
                                            rightPadding: VLCStyle.spacing_4
                                        }

                                        Repeater {
                                            model: modelData.caps
                                            delegate: KeyCap {
                                                required property string modelData
                                                text: modelData
                                            }
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Back, Skip, Next or Finish
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.margin_large
            Layout.rightMargin: VLCStyle.margin_large
            Layout.bottomMargin: VLCStyle.margin_large
            spacing: VLCStyle.spacing_10

            Text {
                id: fullListLink

                visible: onboarding.page === OnboardingModel.Shortcuts
                text: qsTr("Find the full list in <a href=\"#\">Keyboard Shortcuts</a>.")
                textFormat: Text.StyledText
                linkColor: theme.accent
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily

                Accessible.role: Accessible.Link
                Accessible.name: qsTr("Keyboard Shortcuts")
                Accessible.onPressAction: DialogsProvider.keyboardShortcutsDialog()

                onLinkActivated: DialogsProvider.keyboardShortcutsDialog()

                HoverHandler {
                    cursorShape: fullListLink.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Text {
                visible: !fullListLink.visible
                text: qsTr("Step %1 of %2").arg(onboarding.step + 1).arg(onboarding.stepCount)
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
            }

            Item { Layout.fillWidth: true }

            Widgets.ButtonExt {
                text: qsTr("Skip")
                Accessible.description: qsTr("Keep the default settings and close")
                onClicked: onboarding.skip()
            }

            Widgets.ButtonExt {
                enabled: onboarding.step > 0
                text: qsTr("Back")
                onClicked: onboarding.back()
            }

            Widgets.ActionButtonPrimary {
                id: nextButton
                focus: true
                text: onboarding.lastStep ? qsTr("Finish") : qsTr("Next")
                onClicked: onboarding.next()
            }
        }
    }
}
