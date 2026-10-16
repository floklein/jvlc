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
import VLC.Util
import VLC.Dialogs

// The Settings window: categories grouped App, Media and System, search, and
// rows that apply as soon as they change.
Window {
    id: root

    transientParent: MainCtx.intfMainWindow
    flags: Qt.Dialog
    modality: Qt.NonModal

    title: qsTr("Settings")

    // NOTE: At large scales the sizes are clamped to the screen, the rows scroll.
    width: Math.min(VLCStyle.dp(1048, VLCStyle.scale), Screen.desktopAvailableWidth * 0.9)
    height: Math.min(VLCStyle.dp(865, VLCStyle.scale), Screen.desktopAvailableHeight * 0.9)
    minimumWidth: Math.min(VLCStyle.dp(720, VLCStyle.scale), Screen.desktopAvailableWidth * 0.9)
    minimumHeight: Math.min(VLCStyle.dp(480, VLCStyle.scale), Screen.desktopAvailableHeight * 0.9)

    color: theme.bg.primary

    property string currentPage: "general"
    // asked by a deep link, for the Advanced Options section of the page
    property bool advancedRequested: false

    readonly property bool searching: searchField.text.trim() !== ""

    // categories, see GLOSSARY.md "Settings"
    readonly property var pages: [
        { group: qsTr("App") },
        { page: "general",     title: qsTr("General"),     icon: "settings" },
        { page: "interface",   title: qsTr("Interface"),   icon: "interface" },
        { page: "privacy",     title: qsTr("Privacy"),     icon: "privacy" },
        { page: "controls",    title: qsTr("Controls"),    icon: "hotkeys" },
        { group: qsTr("Media") },
        { page: "playback",    title: qsTr("Playback"),    icon: "playback" },
        { page: "video",       title: qsTr("Video"),       icon: "video" },
        { page: "audio",       title: qsTr("Audio"),       icon: "audio" },
        { page: "subtitles",   title: qsTr("Subtitles"),   icon: "subtitles" },
        { page: "library",     title: qsTr("Library"),     icon: "folder" },
        { group: qsTr("System") },
        { page: "network",     title: qsTr("Network"),     icon: "streaming" },
        { page: "performance", title: qsTr("Performance"), icon: "preferences" },
        { page: "recording",   title: qsTr("Recording"),   icon: "record" },
        { page: "extensions",  title: qsTr("Extensions"),  icon: "plugins" },
    ]

    function pageTitle(page: string) : string {
        const entry = pages.find((p) => p.page === page)
        return entry ? entry.title : ""
    }

    // the icon of a category, its "_active" variant when selected
    function pageIcon(name: string, active: bool) : string {
        const glyph = active ? VLCIcons[name + "_active"] : undefined
        return glyph !== undefined ? glyph : (VLCIcons[name] ?? VLCIcons.settings)
    }

    function openPage(page: string, advanced: bool) {
        if (page !== "" && pageTitle(page) !== "")
            root.currentPage = page
        root.advancedRequested = advanced
        searchField.text = ""
        if (advanced) {
            appSettings.setAdvancedExpanded(root.currentPage, true)
            Qt.callLater(() => {
                const index = rowsModel.indexOf("advanced:" + root.currentPage)
                if (index >= 0)
                    rowsView.positionViewAtIndex(index, ListView.Beginning)
            })
        }
        root.show()
        root.raise()
        root.requestActivate()
    }

    // the categories, in the order of the list
    readonly property var _categoryPages: pages.filter((p) => p.page !== undefined)
                                               .map((p) => p.page)

    // the category Tab lands on in the list
    readonly property string _tabCategory: searching ? _categoryPages[0] : currentPage

    // open a category from the keyboard and keep the focus on it
    function _openCategory(page: string) {
        searchField.text = ""
        root.currentPage = page
        const index = pages.findIndex((p) => p.page === page)
        const loader = categoryRepeater.itemAt(index)
        if (loader && loader.item)
            loader.item.forceActiveFocus(Qt.TabFocusReason)
    }

    function _stepCategory(page: string, step: int) {
        const index = _categoryPages.indexOf(page) + step
        if (index >= 0 && index < _categoryPages.length)
            _openCategory(_categoryPages[index])
    }

    // where a row of the search results comes from
    function rowPath(row) : string {
        const parts = [root.pageTitle(row.page)]
        if (row.advanced && !row.searchOnly)
            parts.push(qsTr("Advanced Options"))
        const place = row.groupLabel !== "" ? row.groupLabel : row.section
        if (place !== "")
            parts.push(place)
        return parts.join(" › ")
    }

    onVisibleChanged: DialogsProvider.setSettingsDialogVisible(visible)

    Component.onCompleted: {
        root.developerMode = !!appSettings.value("developer-mode")

        if (MainCtx.createWindowWithoutRedirectionSurface) { // Win32
            const ret = MainCtx.createWindowWithoutRedirectionSurface(this)
            if (!ret)
                console.debug("MainCtx::createWindowWithoutRedirectionSurface(): returned false for window", this)
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: {
            if (root.searching)
                searchField.text = ""
            else
                root.close()
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: searchField.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    SettingsModel {
        id: appSettings
        ctx: MainCtx
    }

    // developer rows show in developer mode
    property bool developerMode: false

    Connections {
        target: appSettings
        function onSettingChanged(id, value) {
            if (id === "developer-mode")
                root.developerMode = !!value
        }
    }

    SettingsFilterModel {
        id: rowsModel
        sourceModel: appSettings
        page: root.currentPage
        searchPattern: searchField.text
        showDeveloper: root.developerMode
    }

    ColumnLayout {
        id: settingsContent

        anchors.fill: parent
        spacing: 0

        // the contexts of the items below inherit this palette
        readonly property ColorContext colorContext: ColorContext {
            id: theme
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        // header: search, page title, Profile, close
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: VLCStyle.dp(50, VLCStyle.scale)
            color: theme.bg.secondary

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.spacing_8
                anchors.rightMargin: VLCStyle.spacing_14
                spacing: 0

                Rectangle {
                    Layout.preferredWidth: VLCStyle.dp(200, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(34, VLCStyle.scale)
                    radius: VLCStyle.radius_normal
                    color: windowTheme.bg.primary

                    readonly property ColorContext colorContext: ColorContext {
                        id: windowTheme
                        colorSet: ColorContext.Window
                    }

                    Widgets.IconLabel {
                        id: searchIcon
                        anchors.left: parent.left
                        anchors.leftMargin: VLCStyle.spacing_8
                        anchors.verticalCenter: parent.verticalCenter
                        text: VLCIcons.search
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.icon_normal
                    }

                    T.TextField {
                        id: searchField

                        anchors.left: searchIcon.right
                        anchors.leftMargin: VLCStyle.spacing_10
                        anchors.right: parent.right
                        anchors.rightMargin: VLCStyle.spacing_8
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        placeholderText: qsTr("Search")
                        placeholderTextColor: theme.fg.secondary
                        color: theme.fg.primary
                        selectionColor: theme.bg.highlight
                        font.pixelSize: VLCStyle.fontSize_normal
                        font.family: VLCStyle.fontFamily
                        verticalAlignment: Text.AlignVCenter

                        Accessible.name: qsTr("Search settings")

                        KeyNavigation.down: rowsView
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: VLCStyle.margin_xlarge
                    text: root.searching ? qsTr("Search results") : root.pageTitle(root.currentPage)
                    elide: Text.ElideRight
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_xlarge
                    font.family: VLCStyle.fontFamily
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
                }

                // the Profile being edited
                Text {
                    text: qsTr("Profile: %1").arg(appSettings.profileName)
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // categories
            Item {
                Layout.preferredWidth: VLCStyle.dp(216, VLCStyle.scale)
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: VLCStyle.spacing_8
                    radius: VLCStyle.radius_normal
                    color: sidebarTheme.bg.primary

                    readonly property ColorContext colorContext: ColorContext {
                        id: sidebarTheme
                        colorSet: ColorContext.Window
                    }

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: VLCStyle.spacing_6
                        contentHeight: categories.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        // a tab list: one Tab stop, which lands on the current
                        // category (the first one while searching); the
                        // arrows move between the categories
                        FocusScope {
                            width: parent.width
                            height: categories.implicitHeight

                            activeFocusOnTab: true

                            Column {
                                id: categories
                                width: parent.width
                                spacing: VLCStyle.spacing_8

                                Accessible.role: Accessible.PageTabList
                                Accessible.name: qsTr("Settings categories")

                                Repeater {
                                    id: categoryRepeater

                                    model: root.pages

                                    delegate: Loader {
                                        required property var modelData
                                        width: categories.width
                                        focus: modelData.page !== undefined
                                               && modelData.page === root._tabCategory
                                        sourceComponent: modelData.group !== undefined ? groupComponent : categoryComponent
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // rows of the page or search results
            ListView {
                id: rowsView

                Layout.fillWidth: true
                Layout.fillHeight: true

                clip: true
                focus: true
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true

                // pages start with a section title that has its own space
                topMargin: root.searching ? VLCStyle.spacing_20 : 0
                bottomMargin: VLCStyle.margin_xlarge
                leftMargin: VLCStyle.margin_large
                rightMargin: VLCStyle.margin_large

                model: rowsModel

                section.property: root.searching ? "" : "section"
                section.criteria: ViewSection.FullString
                section.delegate: Item {
                    required property string section

                    width: rowsView.width - rowsView.leftMargin - rowsView.rightMargin
                    // 20 between sections (and above the first one), 10 around the title;
                    // the Advanced Options and plugin group titles are rows
                    height: section === "" ? 0
                                           : VLCStyle.spacing_20 + 2 * VLCStyle.spacing_10 + sectionLabel.implicitHeight
                    visible: section !== ""

                    Text {
                        id: sectionLabel
                        y: VLCStyle.spacing_20 + VLCStyle.spacing_10
                        text: parent.section
                        color: theme.fg.primary
                        font.pixelSize: VLCStyle.fontSize_xlarge
                        font.weight: Font.Medium
                        font.family: VLCStyle.fontFamily
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                }

                delegate: SettingsRow {
                    width: rowsView.width - rowsView.leftMargin - rowsView.rightMargin
                    settingsModel: appSettings
                    textDialog: nameDialog
                    path: root.searching ? root.rowPath(model) : ""
                }

                T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                Text {
                    anchors.centerIn: parent
                    visible: rowsView.count === 0
                    width: parent.width - 2 * VLCStyle.margin_large
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: root.searching ? qsTr("No settings match “%1”.").arg(searchField.text.trim())
                                         : qsTr("This page has no settings yet.")
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }
            }
        }

        // some changes need a restart
        Rectangle {
            id: restartNotice

            // a switch of Profile can need one too
            readonly property bool needed: appSettings.restartRequired
                                             || (MainCtx.profiles?.restartRequired ?? false)

            Layout.fillWidth: true
            Layout.preferredHeight: VLCStyle.dp(52, VLCStyle.scale)
            visible: needed
            color: theme.bg.secondary

            Accessible.role: Accessible.AlertMessage
            Accessible.name: restartText.text

            // screen readers read it when a change needs the restart
            onNeededChanged: {
                if (needed && root.visible)
                    Helpers.announce(restartNotice, restartText.text)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: VLCStyle.margin_large
                anchors.rightMargin: VLCStyle.margin_large
                spacing: VLCStyle.spacing_10

                Text {
                    id: restartText
                    Layout.fillWidth: true
                    text: qsTr("Some changes apply after jVLC restarts.")
                    elide: Text.ElideRight
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }

                Widgets.ActionButtonPrimary {
                    text: qsTr("Restart jVLC")
                    onClicked: appSettings.restartVLC()
                }
            }
        }
    }

    // names and links asked by the lists (Profiles, Presets, podcasts)
    TextInputDialog {
        id: nameDialog
    }

    // Keyboard Shortcuts, when a row of Settings opens it
    KeyboardShortcutsLoader {
        backdrop: settingsContent
    }

    Component {
        id: groupComponent

        Text {
            leftPadding: VLCStyle.spacing_10
            topPadding: VLCStyle.spacing_8
            bottomPadding: VLCStyle.spacing_8
            text: parent.modelData.group
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
            font.family: VLCStyle.fontFamily
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }
    }

    Component {
        id: categoryComponent

        T.AbstractButton {
            id: category

            readonly property var entry: parent.modelData
            readonly property bool current: !root.searching && root.currentPage === entry.page

            implicitHeight: VLCStyle.dp(40, VLCStyle.scale)
            padding: VLCStyle.spacing_4
            // one Tab stop for the list: the current category, or the first
            // one while searching
            // the list is the Tab stop, see _tabCategory
            focusPolicy: Qt.ClickFocus
            focus: true

            Keys.onUpPressed: root._stepCategory(entry.page, -1)
            Keys.onDownPressed: root._stepCategory(entry.page, 1)
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Home) {
                    root._openCategory(root._categoryPages[0])
                    event.accepted = true
                } else if (event.key === Qt.Key_End) {
                    root._openCategory(root._categoryPages[root._categoryPages.length - 1])
                    event.accepted = true
                }
            }

            Accessible.role: Accessible.PageTab
            Accessible.name: entry.title
            // the button resets the attached checked state to its own
            checked: current
            Accessible.checkable: true
            Accessible.checked: current
            Accessible.onPressAction: category.clicked()

            onClicked: {
                searchField.text = ""
                root.currentPage = entry.page
            }

            readonly property ColorContext colorContext: ColorContext {
                id: categoryTheme
                colorSet: ColorContext.Item
                focused: category.visualFocus
                hovered: category.hovered
            }

            background: Rectangle {
                radius: VLCStyle.radius_xxsmall
                color: category.current ? categoryTheme.bg.highlight : categoryTheme.bg.primary
                border.width: category.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                border.color: categoryTheme.visualFocus
            }

            contentItem: RowLayout {
                spacing: VLCStyle.spacing_8

                Widgets.IconLabel {
                    Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                    Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                    text: root.pageIcon(category.entry.icon, category.current)
                    font.pixelSize: VLCStyle.icon_normal
                    color: category.current ? categoryTheme.accent : categoryTheme.fg.secondary
                }

                Text {
                    Layout.fillWidth: true
                    text: category.entry.title
                    elide: Text.ElideRight
                    color: category.current || category.hovered ? categoryTheme.fg.highlight
                                                                : categoryTheme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }
            }
        }
    }
}
