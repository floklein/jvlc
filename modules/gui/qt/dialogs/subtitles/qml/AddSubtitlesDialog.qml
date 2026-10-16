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

// Add Subtitles (Figma 1919:8707, from OpenSubtitles 1919:9278, target menu
// 1959:5325, source menu 1963:5465): a target and a source, then the
// subtitles to attach.
// - To This File, From Computer lists the files next to the video and in
//   its "Subs" folder; From Link… adds the files of a link to the same list.
// - To This Folder / Playlist, From Computer (folder matching 1911:7545,
//   match row 1915:8397) matches a subtitle file to every episode, from the
//   Show's folder, another folder or a .zip, with ⇆ to change a match.
// - From OpenSubtitles searches for the file, exact matches first, or
//   pre-picks the best subtitle of every episode while downloads are left.
// Save attaches the chosen ones as Subtitle matches.
ModalDialog {
    id: root

    // the AddSubtitlesModel given by DialogsProvider, which owns it
    property AddSubtitlesModel model: null

    // To This Folder or To This Playlist: the SubtitleMatchModel given by
    // DialogsProvider, which owns it
    property SubtitleMatchModel matchModel: null

    // the target, the index of the target picker: To This File (0), To This
    // Folder (1) or To This Playlist (2)
    readonly property int _toFile: 0
    readonly property int _toFolder: 1
    readonly property int _toPlaylist: 2
    property int target: _toFile

    // From Computer, From OpenSubtitles, From Link…
    readonly property int sourceComputer: 0
    readonly property int sourceOnline: 1
    readonly property int sourceLink: 2
    property int source: sourceComputer

    // matching local files to the episodes of a folder or Playlist
    readonly property bool _matching: target !== root._toFile && source === root.sourceComputer

    // the OpenSubtitles model of the target
    readonly property var online: root.target === root._toFile ? (root.model?.openSubtitles ?? null)
                                                               : (root.matchModel?.openSubtitles ?? null)
    readonly property bool onlineShown: root.source === root.sourceOnline && !!root.online

    // the dark "well" behind the header and the checked files: the
    // background of a text field
    readonly property ColorContext _wellTheme: ColorContext {
        palette: root.colorContext.palette
        colorSet: ColorContext.TextField
    }
    readonly property color _well: _wellTheme.bg.primary

    // the semantic colors (errors) are defined for views
    readonly property ColorContext _semantic: ColorContext {
        palette: root.colorContext.palette
        colorSet: ColorContext.View
    }

    title: qsTr("Add Subtitles")

    padding: 0
    topPadding: 0

    width: Math.min(VLCStyle.dp(760, VLCStyle.scale), (parent ? parent.width : Infinity) - VLCStyle.margin_large * 2)
    height: Math.min(VLCStyle.dp(560, VLCStyle.scale), (parent ? parent.height : Infinity) - VLCStyle.margin_large * 2)

    function openWith(model) {
        root.model = model
        root.matchModel = null
        root.target = root._toFile
        root.source = root.sourceComputer
        open()
        // no row is current until the keyboard moves to one
        files.currentIndex = -1
        files.forceActiveFocus(Qt.PopupFocusReason)
    }

    // To This Folder or Playlist; keeps the file and the source of an open
    // dialog, so that the target can go back to the file
    function openMatching(matchModel) {
        if (!root.opened) {
            root.model = null
            root.source = root.sourceComputer
        }
        root.matchModel = matchModel
        root.target = matchModel.playlist ? root._toPlaylist
                                          : root._toFolder
        open()
        matchList.currentIndex = -1
        matchList.forceActiveFocus(Qt.PopupFocusReason)
        searchIfNeeded()
    }

    // the target picker changed
    function setTarget(target) {
        if (target === root.target)
            return
        if (target === root._toFile && root.model) {
            root.target = target
            files.forceActiveFocus(Qt.PopupFocusReason)
        } else if (target === root._toFolder) {
            if (root.matchModel && !root.matchModel.playlist) {
                root.target = target
                matchList.forceActiveFocus(Qt.PopupFocusReason)
            } else if (root.model) {
                // the folder of the file: opens through openMatching()
                const media = root.model.media
                DialogsProvider.addFolderSubtitlesDialog(media.substring(0, media.lastIndexOf("/")))
            }
        }
        searchIfNeeded()
    }

    // the source picker changed; a link adds to a file only
    function setSource(source) {
        root.source = source
        if (source === root.sourceLink)
            root.target = root._toFile
        searchIfNeeded()
    }

    // the search is pre-filled: run it when OpenSubtitles opens
    function searchIfNeeded() {
        if (!root.onlineShown || !root.online.available || root.online.busy)
            return
        if (!root.online.searched)
            root.online.search()
    }

    function canSave() {
        if (root.source === root.sourceOnline) {
            if (!root.online || !root.online.available || root.online.busy)
                return false
            return root.target === root._toFile ? root.online.checkedCount > 0
                                                : root.online.pickedCount > 0
        }
        if (root._matching)
            return (root.matchModel?.matchedCount ?? 0) > 0
        return !!root.model && root.model.checkedCount > 0 && !root.model.fetching
    }

    function save() {
        if (!canSave())
            return
        if (root.source === root.sourceOnline) {
            // downloads first: the dialog closes on saved()
            root.online.save()
            return
        }
        if (root._matching)
            root.matchModel.save()
        else
            root.model.save()
        close()
    }

    onClosed: {
        root.model = null
        root.matchModel = null
        root.source = root.sourceComputer
        root.target = root._toFile
    }

    Connections {
        target: root.onlineShown ? root.online : null
        ignoreUnknownSignals: true

        function onSaved() {
            root.close()
        }
    }

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

                // the target: the file, its folder, or the Playlist the
                // dialog was opened for
                Picker {
                    Accessible.name: qsTr("Add subtitles to")

                    model: [
                        { text: qsTr("To This File"), enabled: !!root.model },
                        { text: qsTr("To This Folder"),
                          enabled: !!root.model || (!!root.matchModel && !root.matchModel.playlist) },
                        { text: qsTr("To This Playlist"),
                          enabled: !!root.matchModel && root.matchModel.playlist }
                    ]

                    currentIndex: root.target

                    onActivated: (index) => {
                        root.setTarget(index)
                        currentIndex = Qt.binding(() => root.target)
                    }
                }

                // the source; a link adds to a file
                Picker {
                    Accessible.name: qsTr("Add subtitles from")

                    model: [
                        { text: qsTr("From Computer"), enabled: true },
                        { text: qsTr("From OpenSubtitles"), enabled: true },
                        { text: qsTr("From Link…"), enabled: !!root.model }
                    ]

                    currentIndex: root.source

                    onActivated: (index) => {
                        root.setSource(index)
                        currentIndex = Qt.binding(() => root.source)
                    }
                }

                // matching: the source of the subtitle files, the Show's
                // folder, another folder or a .zip
                Picker {
                    id: sourcePicker

                    Layout.fillWidth: true

                    visible: root._matching

                    Accessible.name: qsTr("Subtitle files from")

                    model: [
                        { text: (root.matchModel?.playlist ?? false) ? qsTr("Folder of the First Item")
                                                                     : qsTr("Show's Folder"),
                          enabled: true },
                        { text: qsTr("Other Folder…"), enabled: true },
                        { text: qsTr(".zip File…"), enabled: true }
                    ]

                    // the path, not the entry
                    currentIndex: -1
                    // its folder (or .zip) and the folder above
                    displayText: (root.matchModel?.source ?? "").split(" / ").slice(-2).join(" / ")

                    onActivated: (index) => {
                        currentIndex = -1
                        if (!root.matchModel)
                            return
                        if (index === 0)
                            root.matchModel.resetSource()
                        else if (index === 1)
                            root.matchModel.browseFolder()
                        else
                            root.matchModel.browseZip()
                    }
                }

                // matching, with several languages: the one to attach, or All
                Picker {
                    Layout.preferredWidth: VLCStyle.dp(140, VLCStyle.scale)

                    visible: root._matching && (root.matchModel?.languages.length ?? 0) > 0

                    Accessible.name: qsTr("Subtitle language")

                    model: (root.matchModel?.languages ?? []).map((name) => ({ text: name, enabled: true }))

                    currentIndex: root.matchModel?.languageIndex ?? -1

                    onActivated: (index) => {
                        if (root.matchModel)
                            root.matchModel.languageIndex = index
                        currentIndex = Qt.binding(() => root.matchModel?.languageIndex ?? -1)
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)

                    visible: !root._matching

                    currentIndex: root.source === root.sourceComputer ? 0
                                : root.source === root.sourceLink ? 1
                                : root.target === root._toFile ? 2 : 3

                    // From Computer: the folder listed, and Browse… for
                    // files elsewhere
                    Field {
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

                    // From Link…: a .srt, .vtt, .ass or .zip link
                    Field {
                        Widgets.IconLabel {
                            text: VLCIcons.link
                            color: root.colorContext.fg.secondary
                            font.pixelSize: VLCStyle.icon_normal
                        }

                        FieldInput {
                            id: linkField

                            placeholderText: qsTr("https://… .srt, .vtt, .ass or .zip")
                            Accessible.name: qsTr("Subtitle link")

                            onAccepted: addLink.clicked()
                        }

                        Widgets.TextToolButton {
                            id: addLink

                            text: qsTr("Add")
                            enabled: linkField.text.trim() !== "" && !(root.model?.fetching ?? false)

                            onClicked: {
                                root.model?.addLink(linkField.text)
                                if (root.model && root.model.linkError === "")
                                    linkField.text = ""
                            }
                        }
                    }

                    // From OpenSubtitles, To This File: the search and
                    // the language
                    Field {
                        enabled: root.online?.available ?? false

                        Widgets.IconLabel {
                            text: VLCIcons.search
                            color: root.colorContext.fg.secondary
                            font.pixelSize: VLCStyle.icon_normal
                        }

                        FieldInput {
                            id: queryField

                            text: root.model?.openSubtitles?.query ?? ""
                            Accessible.name: qsTr("Search OpenSubtitles")

                            onTextEdited: {
                                if (root.model?.openSubtitles)
                                    root.model.openSubtitles.query = text
                            }
                            onAccepted: root.model?.openSubtitles?.search()
                        }

                        LanguagePicker {
                            source: root.model?.openSubtitles ?? null
                        }
                    }

                    // From OpenSubtitles, To This Folder: the folder and
                    // the language
                    Field {
                        enabled: root.online?.available ?? false

                        Widgets.IconLabel {
                            text: VLCIcons.folder
                            color: root.colorContext.fg.secondary
                            font.pixelSize: VLCStyle.icon_normal
                        }

                        T.Label {
                            Layout.fillWidth: true

                            text: root.matchModel?.openSubtitles?.folder ?? ""
                            elide: Text.ElideLeft
                            color: root.colorContext.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_normal
                        }

                        LanguagePicker {
                            source: root.matchModel?.openSubtitles ?? null
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

        StackLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_normal

            currentIndex: root._matching ? 4
                        : !root.onlineShown ? 0
                        : !root.online.available ? 3
                        : root.target === root._toFile ? 1 : 2

            // From Computer and From Link…: the files to check
            ColumnLayout {
                spacing: VLCStyle.margin_small

                ErrorLabel {
                    text: root.source === root.sourceLink ? (root.model?.linkError ?? "") : ""
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: files

                        anchors.fill: parent

                        clip: true
                        spacing: VLCStyle.spacing_4
                        keyNavigationEnabled: true

                        model: root.model

                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Subtitle files")

                        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                        delegate: CheckRow {
                            required property int index
                            required property var model

                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem && files.activeFocus
                            checked: model.checked
                            name: model.name

                            onClicked: root.model.toggle(index)
                        }

                        Keys.onSpacePressed: {
                            if (currentIndex >= 0)
                                root.model.toggle(currentIndex)
                        }
                    }

                    Message {
                        visible: files.count === 0 || (root.model?.fetching ?? false)
                        busy: root.model?.fetching ?? false
                        text: root.model?.fetching ? qsTr("Downloading the link…")
                            : root.source === root.sourceLink
                              ? qsTr("Paste a link to a .srt, .vtt or .ass file, or to a .zip of subtitles.")
                              : qsTr("No subtitle files next to this file. Use Browse… to pick some.")
                    }
                }
            }

            // From OpenSubtitles, To This File: the results, exact
            // matches first
            ColumnLayout {
                readonly property var search: root.model?.openSubtitles ?? null

                spacing: VLCStyle.margin_small

                ErrorLabel {
                    text: parent.search?.error ?? ""
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: results

                        anchors.fill: parent

                        clip: true
                        spacing: VLCStyle.spacing_4
                        keyNavigationEnabled: true

                        model: root.model?.openSubtitles ?? null

                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("OpenSubtitles results")

                        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                        delegate: CheckRow {
                            required property int index
                            required property var model

                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem && results.activeFocus
                            checked: model.checked
                            name: model.name
                            language: model.language
                            exactMatch: model.exactMatch
                            detail: qsTr("%1 downloads").arg(Number(model.downloads).toLocaleString(Qt.locale(), "f", 0))

                            onClicked: results.model.toggle(index)
                        }

                        Keys.onSpacePressed: {
                            if (currentIndex >= 0)
                                results.model.toggle(currentIndex)
                        }
                    }

                    Message {
                        readonly property var search: root.model?.openSubtitles ?? null

                        visible: results.count === 0 && (search?.error ?? "") === ""
                        busy: search?.busy ?? false
                        text: search?.busy ? qsTr("Searching OpenSubtitles…")
                            : search?.searched ? qsTr("No subtitles found. Try another name or language.")
                            : qsTr("Search OpenSubtitles for this file.")
                    }
                }
            }

            // From OpenSubtitles, To This Folder: every video with its
            // pick, ⇆ to change it
            ColumnLayout {
                readonly property var folder: root.matchModel?.openSubtitles ?? null

                spacing: VLCStyle.margin_small

                ErrorLabel {
                    text: parent.folder?.error ?? ""
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: episodes

                        anchors.fill: parent

                        clip: true
                        spacing: VLCStyle.spacing_4
                        keyNavigationEnabled: true

                        model: root.matchModel?.openSubtitles ?? null

                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Videos of the folder")

                        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                        delegate: EpisodeRow {
                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem && episodes.activeFocus
                        }
                    }

                    Message {
                        visible: episodes.count === 0
                        text: qsTr("No videos in this folder.")
                    }
                }
            }

            // From OpenSubtitles without a key
            Item {
                Message {
                    text: qsTr("OpenSubtitles isn't available in this build of jVLC: "
                               + "it was built without an OpenSubtitles API key. "
                               + "Use From Computer or From Link… instead.")
                }
            }

            // To This Folder / Playlist, From Computer: each episode and its
            // match; ⇆ (or Enter on a row) opens the candidates, best first,
            // then None
            Item {
                ListView {
                    id: matchList

                    anchors.fill: parent

                    clip: true
                    spacing: VLCStyle.spacing_4
                    keyNavigationEnabled: true

                    model: root.matchModel

                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Episodes")

                    T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                    delegate: T.ItemDelegate {
                        id: match

                        required property int index
                        required property var model

                        width: matchList.width
                        implicitHeight: VLCStyle.dp(34, VLCStyle.scale)
                        leftPadding: VLCStyle.margin_xsmall
                        rightPadding: VLCStyle.margin_xxsmall

                        highlighted: ListView.isCurrentItem && matchList.activeFocus

                        Accessible.role: Accessible.ListItem
                        Accessible.name: match.model.matched
                                         ? qsTr("%1, matched with %2").arg(match.model.name).arg(match.model.match)
                                         : qsTr("%1, no match").arg(match.model.name)

                        onClicked: matchList.currentIndex = index

                        function openCandidates() {
                            matchList.currentIndex = index
                            candidates.forceActiveFocus(Qt.PopupFocusReason)
                            candidates.popup.open()
                        }

                        background: Rectangle {
                            radius: VLCStyle.radius_small
                            color: match.model.matched ? root._well
                                 : match.hovered ? root.colorContext.bg.secondary : "transparent"
                            border.width: VLCStyle.focus_border
                            border.color: match.highlighted ? root.colorContext.accent : "transparent"
                        }

                        contentItem: RowLayout {
                            spacing: VLCStyle.margin_small

                            T.Label {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1

                                text: match.model.path
                                elide: Text.ElideMiddle
                                color: root.colorContext.fg.primary
                                font.pixelSize: VLCStyle.fontSize_normal
                            }

                            Widgets.IconLabel {
                                text: VLCIcons.subtitle
                                color: match.model.matched ? root.colorContext.accent
                                                           : root.colorContext.fg.secondary
                                font.pixelSize: VLCStyle.icon_normal
                            }

                            // the match, and ⇆ to change it
                            Widgets.ComboBoxExt {
                                id: candidates

                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                Layout.preferredHeight: VLCStyle.dp(28, VLCStyle.scale)

                                focusPolicy: Qt.NoFocus
                                font.pixelSize: VLCStyle.fontSize_normal
                                bgColor: root.colorContext.bg.primary
                                color: match.model.matched ? root.colorContext.fg.primary
                                                           : root.colorContext.fg.secondary

                                textRole: "text"
                                // the candidates, read again when the match changes
                                model: {
                                    void match.model.match
                                    const list = root.matchModel?.candidates(match.index) ?? []
                                    return list.map((candidate) => ({
                                        id: candidate.id,
                                        text: candidate.language ? candidate.path + " (" + candidate.language + ")"
                                                                 : candidate.path
                                    })).concat([{ id: -1, text: qsTr("None") }])
                                }

                                currentIndex: -1
                                displayText: match.model.matched
                                             ? match.model.match
                                             : (match.model.overridden ? qsTr("None") : qsTr("No match"))

                                // ⇆
                                indicator: Widgets.IconLabel {
                                    x: candidates.width - width - candidates.rightPadding
                                    y: candidates.topPadding + (candidates.availableHeight - height) / 2
                                    text: VLCIcons.swap
                                    color: root.colorContext.accent
                                    font.pixelSize: VLCStyle.icon_normal
                                }

                                Accessible.name: qsTr("Change the match of %1").arg(match.model.name)

                                onActivated: (index) => {
                                    currentIndex = -1
                                    root.matchModel?.setMatch(match.index, model[index].id)
                                }

                                Connections {
                                    target: candidates.popup

                                    function onClosed() {
                                        matchList.forceActiveFocus(Qt.PopupFocusReason)
                                    }
                                }
                            }
                        }
                    }

                    Keys.onReturnPressed: (event) => {
                        if (currentIndex >= 0)
                            currentItem.openCandidates()
                        else
                            event.accepted = false
                    }
                    Keys.onEnterPressed: (event) => {
                        if (currentIndex >= 0)
                            currentItem.openCandidates()
                        else
                            event.accepted = false
                    }
                    Keys.onSpacePressed: {
                        if (currentIndex >= 0)
                            currentItem.openCandidates()
                    }
                }

                Message {
                    visible: matchList.count === 0
                    text: root.matchModel?.playlist ?? false ? qsTr("No local media in this Playlist.")
                                                            : qsTr("No video files in this folder.")
                }
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

            spacing: VLCStyle.margin_small

            Widgets.ButtonExt {
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                text: qsTr("Cancel")

                onClicked: root.close()
            }

            // the downloads left today, and how to get more
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_small

                visible: root.onlineShown && root.online.available
                spacing: 0

                T.Label {
                    Layout.fillWidth: true

                    text: root.online?.quotaText ?? ""
                    elide: Text.ElideRight
                    color: root.colorContext.fg.primary
                    font.pixelSize: VLCStyle.fontSize_normal
                }

                T.Label {
                    Layout.fillWidth: true

                    visible: text !== ""
                    text: root.online?.quotaNote ?? ""
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    color: root.colorContext.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }
            }

            Item {
                Layout.fillWidth: true
                visible: !(root.onlineShown && root.online.available)
            }

            Widgets.BusyIndicatorExt {
                Layout.preferredWidth: VLCStyle.icon_small
                Layout.preferredHeight: VLCStyle.icon_small

                visible: root.onlineShown && root.online.busy
                running: visible
                color: root.colorContext.fg.secondary
            }

            Widgets.ActionButtonPrimary {
                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                text: qsTr("Save")
                enabled: root.canSave()

                onClicked: root.save()
            }
        }
    }

    // the field of the header row
    component Field: Rectangle {
        default property alias content: fieldRow.data

        radius: VLCStyle.radius_small
        color: root.colorContext.bg.primary
        opacity: enabled ? 1.0 : 0.5

        RowLayout {
            id: fieldRow

            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_xsmall
            anchors.rightMargin: VLCStyle.margin_xxsmall

            spacing: VLCStyle.margin_xsmall
        }
    }

    component FieldInput: T.TextField {
        Layout.fillWidth: true
        Layout.fillHeight: true

        color: root.colorContext.fg.primary
        placeholderTextColor: root.colorContext.fg.secondary
        selectedTextColor: root.colorContext.fg.highlight
        selectionColor: root.colorContext.bg.highlight
        font.pixelSize: VLCStyle.fontSize_normal
        verticalAlignment: Text.AlignVCenter
        selectByMouse: true
    }

    // the language of an OpenSubtitles search; changing it searches again
    component LanguagePicker: Widgets.ComboBoxExt {
        id: languages

        property var source: null

        Layout.preferredWidth: VLCStyle.dp(150, VLCStyle.scale)
        Layout.fillHeight: true
        Layout.topMargin: VLCStyle.dp(2, VLCStyle.scale)
        Layout.bottomMargin: VLCStyle.dp(2, VLCStyle.scale)

        Accessible.name: qsTr("Subtitle language")

        model: languages.source?.languages ?? []
        textRole: "name"
        valueRole: "code"
        font.pixelSize: VLCStyle.fontSize_normal
        bgColor: root._well

        currentIndex: {
            const list = languages.source?.languages ?? []
            const code = languages.source?.language ?? ""
            for (let i = 0; i < list.length; ++i)
                if (list[i].code === code)
                    return i
            return -1
        }

        onActivated: (index) => {
            if (!languages.source)
                return
            languages.source.language = languages.model[index].code
            languages.source.search()
        }
    }

    // why the last action failed, above the list
    component ErrorLabel: T.Label {
        Layout.fillWidth: true

        visible: text !== ""
        wrapMode: Text.Wrap
        color: root._semantic.fg.negative
        font.pixelSize: VLCStyle.fontSize_normal
    }

    // a centered message over a list: what to do, or a wait
    component Message: ColumnLayout {
        property alias text: messageLabel.text
        property bool busy: false

        anchors.centerIn: parent
        width: parent.width - VLCStyle.margin_large * 2

        spacing: VLCStyle.margin_small

        Widgets.BusyIndicatorExt {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: VLCStyle.icon_normal
            Layout.preferredHeight: VLCStyle.icon_normal

            visible: parent.busy
            running: visible
            color: root.colorContext.fg.secondary
        }

        T.Label {
            id: messageLabel

            Layout.fillWidth: true

            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: root.colorContext.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
        }
    }

    // a subtitle to check: a file, or an OpenSubtitles result
    component CheckRow: T.ItemDelegate {
        id: row

        // "checked" is the one of the button, set by the model
        property string name
        property string language
        property bool exactMatch: false
        property string detail

        implicitHeight: VLCStyle.dp(34, VLCStyle.scale)
        leftPadding: VLCStyle.margin_xsmall
        rightPadding: VLCStyle.margin_xsmall

        Accessible.role: Accessible.CheckBox
        Accessible.checkable: true
        Accessible.checked: row.checked
        Accessible.name: row.exactMatch ? qsTr("%1, exact match").arg(row.name) : row.name

        background: Rectangle {
            radius: VLCStyle.radius_small
            color: row.checked ? root._well
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

                text: row.name
                elide: Text.ElideMiddle
                color: root.colorContext.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Tag {
                visible: row.exactMatch
                text: qsTr("Exact match")
                accent: true
            }

            Tag {
                visible: row.language !== ""
                text: row.language.toUpperCase()
            }

            T.Label {
                Layout.preferredWidth: VLCStyle.dp(110, VLCStyle.scale)

                visible: row.detail !== ""
                text: row.detail
                horizontalAlignment: Text.AlignRight
                color: root.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
            }

            Widgets.IconLabel {
                text: row.checked ? VLCIcons.check : ""
                color: root.colorContext.accent
                font.pixelSize: VLCStyle.icon_normal
            }
        }
    }

    component Tag: Rectangle {
        property alias text: tagLabel.text
        property bool accent: false

        implicitWidth: tagLabel.implicitWidth + VLCStyle.margin_xsmall * 2
        implicitHeight: tagLabel.implicitHeight + VLCStyle.dp(4, VLCStyle.scale)
        radius: height / 2
        color: accent ? accentTheme.bg.primary : root.colorContext.bg.secondary

        // the text drawn on the accent reads like on accent buttons
        readonly property ColorContext accentTheme: ColorContext {
            colorSet: ColorContext.ButtonAccent
        }

        T.Label {
            id: tagLabel

            anchors.centerIn: parent
            color: parent.accent ? parent.accentTheme.fg.primary : root.colorContext.fg.secondary
            font.pixelSize: VLCStyle.fontSize_small
        }
    }

    // a video of the folder, its pick, and ⇆
    component EpisodeRow: T.ItemDelegate {
        id: episode

        required property int index
        required property var model

        readonly property var folderModel: root.matchModel?.openSubtitles ?? null

        implicitHeight: VLCStyle.dp(44, VLCStyle.scale)
        leftPadding: VLCStyle.margin_xsmall
        rightPadding: VLCStyle.margin_xxsmall

        Accessible.role: Accessible.ListItem
        Accessible.name: qsTr("%1: %2").arg(episode.model.episode).arg(pickText.text)

        background: Rectangle {
            radius: VLCStyle.radius_small
            color: episode.model.state === OpenSubtitlesFolderModel.Picked
                   || episode.model.state === OpenSubtitlesFolderModel.Saved
                   ? root._well
                   : episode.hovered ? root.colorContext.bg.secondary : "transparent"
            border.width: VLCStyle.focus_border
            border.color: episode.highlighted ? root.colorContext.accent : "transparent"
        }

        contentItem: RowLayout {
            spacing: VLCStyle.margin_small

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_xsmall
                spacing: 0

                T.Label {
                    Layout.fillWidth: true

                    text: episode.model.episode
                    elide: Text.ElideMiddle
                    color: root.colorContext.fg.primary
                    font.pixelSize: VLCStyle.fontSize_normal
                }

                T.Label {
                    id: pickText

                    Layout.fillWidth: true

                    text: {
                        switch (episode.model.state) {
                        case OpenSubtitlesFolderModel.Searching: return qsTr("Searching…")
                        case OpenSubtitlesFolderModel.NoMatch: return qsTr("No match")
                        case OpenSubtitlesFolderModel.NotPicked: return qsTr("Not picked: no download left for it today")
                        case OpenSubtitlesFolderModel.None: return qsTr("None")
                        case OpenSubtitlesFolderModel.Saved: return qsTr("Saved: %1").arg(episode.model.pickName)
                        default: return episode.model.pickName
                        }
                    }
                    elide: Text.ElideMiddle
                    color: episode.model.state === OpenSubtitlesFolderModel.Picked ? root.colorContext.accent
                                                                                  : root.colorContext.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_small
                }
            }

            Tag {
                visible: episode.model.exactMatch
                         && episode.model.state === OpenSubtitlesFolderModel.Picked
                text: qsTr("Exact match")
                accent: true
            }

            Tag {
                visible: episode.model.pickLanguage !== ""
                         && episode.model.state === OpenSubtitlesFolderModel.Picked
                text: episode.model.pickLanguage.toUpperCase()
            }

            // ⇆: the candidates, best first, and None
            Widgets.IconToolButton {
                id: swap

                text: VLCIcons.swap
                color: root.colorContext.accent
                description: qsTr("Change the subtitle of %1").arg(episode.model.episode)
                enabled: episode.model.candidateCount > 0
                         && episode.model.state !== OpenSubtitlesFolderModel.Saved
                         && !(episode.folderModel?.busy ?? true)

                onClicked: candidates.open()

                T.Popup {
                    id: candidates

                    y: swap.height
                    x: swap.width - width
                    width: VLCStyle.dp(380, VLCStyle.scale)
                    height: Math.min(candidateList.contentHeight + padding * 2, VLCStyle.dp(260, VLCStyle.scale))
                    padding: VLCStyle.margin_xxsmall
                    modal: true
                    focus: true

                    background: Rectangle {
                        radius: VLCStyle.radius_small
                        color: root.colorContext.bg.primary
                        border.width: VLCStyle.dp(1, VLCStyle.scale)
                        border.color: root.colorContext.border
                    }

                    contentItem: ListView {
                        id: candidateList

                        clip: true
                        keyNavigationEnabled: true
                        model: candidates.opened ? episode.folderModel.candidates(episode.index).concat([null]) : []

                        T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                        delegate: T.ItemDelegate {
                            id: candidate

                            required property var modelData
                            required property int index

                            width: ListView.view.width
                            implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
                            leftPadding: VLCStyle.margin_xsmall
                            rightPadding: VLCStyle.margin_xsmall

                            highlighted: ListView.isCurrentItem
                            Accessible.name: candidate.modelData ? candidate.modelData.name : qsTr("None")

                            background: Rectangle {
                                radius: VLCStyle.radius_small
                                color: candidate.hovered || candidate.highlighted
                                       ? root.colorContext.bg.secondary : "transparent"
                            }

                            contentItem: RowLayout {
                                spacing: VLCStyle.margin_xsmall

                                T.Label {
                                    Layout.fillWidth: true

                                    text: candidate.modelData ? candidate.modelData.name : qsTr("None")
                                    elide: Text.ElideMiddle
                                    color: root.colorContext.fg.primary
                                    font.pixelSize: VLCStyle.fontSize_normal
                                }

                                Tag {
                                    visible: !!candidate.modelData && candidate.modelData.exactMatch
                                    text: qsTr("Exact match")
                                    accent: true
                                }

                                Tag {
                                    visible: !!candidate.modelData
                                    text: candidate.modelData ? candidate.modelData.language.toUpperCase() : ""
                                }
                            }

                            onClicked: {
                                episode.folderModel.pick(episode.index,
                                                         candidate.modelData ? candidate.index : -1)
                                candidates.close()
                            }
                        }
                    }
                }
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

        leftPadding: VLCStyle.margin_small

        // Figma 1911:7545: regular weight, as the rest of the dialog
        contentItem: Widgets.ListLabel {
            rightPadding: picker.spacing + picker.indicator.width

            text: picker.displayText
            color: picker.color
            font.weight: Font.Normal
        }

        delegate: T.ItemDelegate {
            id: entry

            required property var modelData
            required property int index

            width: picker.width
            height: implicitContentHeight + topPadding + bottomPadding
            padding: VLCStyle.margin_xsmall
            leftPadding: picker.leftPadding

            text: modelData.text
            enabled: modelData.enabled
            highlighted: picker.highlightedIndex === index

            contentItem: RowLayout {
                Widgets.ListLabel {
                    Layout.fillWidth: true

                    text: entry.modelData.text
                    color: picker.color
                    font.weight: Font.Normal
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
