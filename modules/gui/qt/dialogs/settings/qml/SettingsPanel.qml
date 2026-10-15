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
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs
import VLC.Player
import VLC.Network

// The lists Settings manages in place (SettingsModel.Panel rows): Profiles,
// Presets, library folders, service sources, add-ons and the built-in
// plugins.
Item {
    id: root

    // see SettingsModel::Row::panel
    property string panel
    // the window's TextInputDialog, for names and links
    property var textDialog
    // the AppSettingsModel, for the folder picker
    property var settingsModel

    implicitHeight: loader.implicitHeight

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    function askText(title: string, label: string, text: string, acceptText: string,
                     placeholder: string, validate, onAccepted) {
        root.textDialog.acceptText = acceptText
        root.textDialog.placeholderText = placeholder
        root.textDialog.validate = validate
        root.textDialog.ask(title, label, text, onAccepted)
    }

    // the last part of a folder path
    function folderName(path: string) : string {
        const parts = path.split("/").filter((part) => part !== "")
        return parts.length > 0 ? parts[parts.length - 1] : path
    }

    Loader {
        id: loader

        anchors.left: parent.left
        anchors.right: parent.right

        sourceComponent: {
            switch (root.panel) {
            case "profiles": return profilesComponent
            case "presets": return presetsComponent
            case "library-folders": return foldersComponent
            case "service-sources": return servicesComponent
            case "addons": return addonsComponent
            case "plugins": return pluginsComponent
            }
            return null
        }
    }

    // a rounded box holding the entries of a list
    component ListFrame : Rectangle {
        default property alias content: column.data
        property string emptyText: ""
        property int count: 0

        implicitHeight: column.implicitHeight + 2 * VLCStyle.spacing_8
        radius: VLCStyle.radius_normal
        color: theme.bg.secondary

        ColumnLayout {
            id: column

            anchors.fill: parent
            anchors.margins: VLCStyle.spacing_8
            spacing: VLCStyle.spacing_4

            Text {
                visible: parent.parent.count === 0 && text !== ""
                Layout.fillWidth: true
                Layout.margins: VLCStyle.spacing_4
                text: parent.parent.emptyText
                wrapMode: Text.WordWrap
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
            }
        }
    }

    // one entry: its title and details, then its buttons
    component Entry : RowLayout {
        property string title
        property string detail
        default property alias actions: actionRow.data

        spacing: VLCStyle.spacing_10

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: VLCStyle.spacing_4
            spacing: 0

            Text {
                Layout.fillWidth: true
                text: parent.parent.title
                elide: Text.ElideMiddle
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
            }

            Text {
                visible: text !== ""
                Layout.fillWidth: true
                text: parent.parent.detail
                elide: Text.ElideRight
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
            }
        }

        Row {
            id: actionRow
            spacing: VLCStyle.spacing_4
        }
    }

    // a button asking once more before it acts
    component ConfirmButton : Widgets.TextToolButton {
        id: confirmButton

        property string label
        property string confirmText
        property bool confirming: false

        signal confirmed()

        text: confirming ? confirmText : label
        font.weight: confirming ? Font.DemiBold : Font.Normal

        onActiveFocusChanged: if (!activeFocus) confirming = false

        onClicked: {
            if (!confirming) {
                confirming = true
                return
            }
            confirming = false
            confirmed()
        }
    }

    component AddButton : Widgets.TextToolButton {
        font.weight: Font.DemiBold
    }

    /* Profiles: Default can't be renamed nor deleted */
    Component {
        id: profilesComponent

        ColumnLayout {
            spacing: VLCStyle.spacing_8

            ListFrame {
                Layout.fillWidth: true
                count: profileRepeater.count

                Repeater {
                    id: profileRepeater

                    model: MainCtx.profiles

                    delegate: Entry {
                        id: profileEntry

                        required property var model
                        required property int index

                        Layout.fillWidth: true
                        title: model.name
                        detail: model.active ? qsTr("In use") : ""

                        Widgets.TextToolButton {
                            visible: !profileEntry.model.active
                            text: qsTr("Switch")
                            Accessible.name: qsTr("Switch to %1").arg(profileEntry.model.name)
                            onClicked: MainCtx.profiles.activate(profileEntry.model.id)
                        }

                        Widgets.TextToolButton {
                            visible: MainCtx.profiles.canRemove(profileEntry.model.id)
                            text: qsTr("Rename…")
                            Accessible.name: qsTr("Rename %1").arg(profileEntry.model.name)
                            onClicked: {
                                const id = profileEntry.model.id
                                const current = profileEntry.model.name
                                root.askText(qsTr("Rename Profile"), "", current, qsTr("Rename"), "",
                                             (text) => (text.trim().toLowerCase() === current.toLowerCase()
                                                        || MainCtx.profiles.isNameAvailable(text))
                                                       ? "" : qsTr("A profile with this name already exists."),
                                             (name) => MainCtx.profiles.rename(id, name))
                            }
                        }

                        ConfirmButton {
                            visible: MainCtx.profiles.canRemove(profileEntry.model.id)
                            label: qsTr("Delete")
                            confirmText: qsTr("Delete %1").arg(profileEntry.model.name)
                            Accessible.name: confirming ? confirmText : qsTr("Delete %1").arg(profileEntry.model.name)
                            onConfirmed: MainCtx.profiles.remove(profileEntry.model.id)
                        }
                    }
                }
            }

            AddButton {
                text: qsTr("+ New Profile…")
                onClicked: root.askText(qsTr("New Profile"),
                                        qsTr("It starts as a copy of %1.").arg(MainCtx.profiles.activeName),
                                        "", qsTr("Create"), qsTr("e.g., Movies"),
                                        (text) => MainCtx.profiles.isNameAvailable(text)
                                                  ? "" : qsTr("A profile with this name already exists."),
                                        (name) => MainCtx.profiles.create(name))
            }
        }
    }

    /* Presets, shared by every Profile, with the folders using them */
    Component {
        id: presetsComponent

        ColumnLayout {
            spacing: VLCStyle.spacing_8

            ListFrame {
                Layout.fillWidth: true
                count: presetRepeater.count
                emptyText: qsTr("No Presets yet. Create one from the footer of the Adjustments sidebar.")

                Repeater {
                    id: presetRepeater

                    model: Presets.model

                    delegate: Entry {
                        id: presetEntry

                        required property var model
                        required property int index

                        Layout.fillWidth: true
                        title: model.name
                        detail: {
                            const folders = model.folders ?? []
                            if (folders.length === 0)
                                return qsTr("Not attached to a folder")
                            return qsTr("Used by %1").arg(folders.map((f) => root.folderName(f)).join(", "))
                        }

                        Widgets.TextToolButton {
                            text: qsTr("Rename…")
                            Accessible.name: qsTr("Rename %1").arg(presetEntry.model.name)
                            onClicked: {
                                const id = presetEntry.model.id
                                root.askText(qsTr("Rename Preset"), "", presetEntry.model.name, qsTr("Rename"),
                                             qsTr("e.g., Anime"),
                                             (text) => Presets.model.isNameAvailable(text, id)
                                                       ? "" : qsTr("A Preset with this name already exists."),
                                             (name) => Presets.model.rename(id, name))
                            }
                        }

                        Widgets.TextToolButton {
                            text: qsTr("Duplicate")
                            Accessible.name: qsTr("Duplicate %1").arg(presetEntry.model.name)
                            onClicked: Presets.model.duplicate(presetEntry.model.id)
                        }

                        Widgets.TextToolButton {
                            text: qsTr("Delete")
                            Accessible.name: qsTr("Delete %1").arg(presetEntry.model.name)
                            // Undo brings it back, with its folders
                            onClicked: Presets.model.remove(presetEntry.model.id)
                        }
                    }
                }
            }

            RowLayout {
                visible: Presets.model.canUndoRemove
                spacing: VLCStyle.spacing_10

                Text {
                    text: qsTr("%1 deleted").arg(Presets.model.removedName)
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }

                Widgets.TextToolButton {
                    text: qsTr("Undo")
                    onClicked: Presets.model.undoRemove()
                }
            }
        }
    }

    /* Library folders, and the folders excluded from them */
    Component {
        id: foldersComponent

        ColumnLayout {
            spacing: VLCStyle.spacing_8

            MLFolderModel {
                id: folderModel
                ctx: MainCtx
            }

            MLBannedFolderModel {
                id: bannedModel
                ctx: MainCtx
            }

            ListFrame {
                Layout.fillWidth: true
                count: folderRepeater.count
                emptyText: qsTr("No library folders yet.")

                Repeater {
                    id: folderRepeater

                    model: folderModel

                    delegate: Entry {
                        id: folderEntry

                        required property var model
                        required property int index

                        Layout.fillWidth: true
                        title: root.folderName(model.display_url)
                        detail: model.display_url

                        Widgets.TextToolButton {
                            text: qsTr("Rescan")
                            Accessible.name: qsTr("Rescan %1").arg(folderEntry.title)
                            onClicked: folderModel.reload(folderEntry.index)
                        }

                        ConfirmButton {
                            label: qsTr("Remove")
                            confirmText: qsTr("Remove from Library")
                            Accessible.name: confirming ? confirmText : qsTr("Remove %1").arg(folderEntry.title)
                            onConfirmed: folderModel.removeAt(folderEntry.index)
                        }
                    }
                }
            }

            AddButton {
                text: qsTr("+ Add Folder…")
                onClicked: DialogsProvider.addLibraryFolderDialog()
            }

            Text {
                Layout.topMargin: VLCStyle.spacing_8
                text: qsTr("Excluded folders")
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
                font.weight: Font.Medium
                font.family: VLCStyle.fontFamily
            }

            ListFrame {
                Layout.fillWidth: true
                count: bannedRepeater.count
                emptyText: qsTr("Folders inside your library folders that VLC leaves out.")

                Repeater {
                    id: bannedRepeater

                    model: bannedModel

                    delegate: Entry {
                        id: bannedEntry

                        required property var model
                        required property int index

                        Layout.fillWidth: true
                        title: root.folderName(model.display_url)
                        detail: model.display_url

                        Widgets.TextToolButton {
                            text: qsTr("Include Again")
                            Accessible.name: qsTr("Include %1 again").arg(bannedEntry.title)
                            onClicked: bannedModel.removeAt(bannedEntry.index)
                        }
                    }
                }
            }

            AddButton {
                text: qsTr("+ Exclude a Folder…")
                onClicked: {
                    const path = root.settingsModel.browseDirectory(qsTr("Exclude a folder"), "")
                    if (path !== "")
                        bannedModel.add(root.settingsModel.localFileUrl(path))
                }
            }
        }
    }

    /* The services of the Network section; Podcasts lists its feeds */
    Component {
        id: servicesComponent

        ListFrame {
            count: serviceRepeater.count

            Repeater {
                id: serviceRepeater

                model: NetworkSourcesModel {
                    ctx: MainCtx
                }

                delegate: ColumnLayout {
                    id: service

                    required property var model

                    readonly property bool isPodcast: model.name === "podcast"
                    property bool configuring: false

                    visible: model.type !== NetworkSourcesModel.TYPE_DUMMY
                    Layout.fillWidth: true
                    spacing: VLCStyle.spacing_4

                    Entry {
                        Layout.fillWidth: true
                        title: service.isPodcast ? qsTr("Podcasts")
                                                 : service.model.name === "icecast" ? qsTr("Radio")
                                                                                    : service.model.long_name
                        detail: service.isPodcast ? qsTr("%n feed(s)", "", PodcastFeeds.urls.length) : ""

                        Widgets.TextToolButton {
                            visible: service.isPodcast
                            text: service.configuring ? qsTr("Done") : qsTr("Configure")
                            onClicked: service.configuring = !service.configuring
                        }
                    }

                    // the feeds of Podcasts
                    ColumnLayout {
                        visible: service.configuring
                        Layout.fillWidth: true
                        Layout.leftMargin: VLCStyle.margin_large
                        spacing: VLCStyle.spacing_4

                        Repeater {
                            model: service.configuring ? PodcastFeeds.urls : []

                            delegate: Entry {
                                id: feedEntry

                                required property string modelData

                                Layout.fillWidth: true
                                title: modelData

                                Widgets.TextToolButton {
                                    text: qsTr("Remove")
                                    Accessible.name: qsTr("Remove %1").arg(feedEntry.modelData)
                                    onClicked: PodcastFeeds.remove(feedEntry.modelData)
                                }
                            }
                        }

                        AddButton {
                            text: qsTr("+ Add Podcast…")
                            onClicked: root.askText(qsTr("Add Podcast"), qsTr("The link of the podcast's feed"),
                                                    "", qsTr("Add"), "https://",
                                                    (text) => PodcastFeeds.contains(text.trim())
                                                              ? qsTr("This feed is already there.") : "",
                                                    (url) => PodcastFeeds.add(url.trim()))
                        }
                    }
                }
            }
        }
    }

    /* Add-ons: the installed ones, and the repository's on demand */
    Component {
        id: addonsComponent

        ColumnLayout {
            id: addons

            property bool findMore: false

            spacing: VLCStyle.spacing_8

            ServicesDiscoveryModel {
                id: addonsModel

                ctx: MainCtx
                stateFilter: addons.findMore ? ServicesDiscoveryModel.STATE_NONE
                                             : ServicesDiscoveryModel.STATE_INSTALLED
            }

            ListFrame {
                Layout.fillWidth: true
                count: addonRepeater.count
                emptyText: addonsModel.loading ? qsTr("Loading…") : qsTr("No add-ons installed.")

                Repeater {
                    id: addonRepeater

                    model: addonsModel

                    delegate: Entry {
                        id: addonEntry

                        required property var model
                        required property int index

                        Layout.fillWidth: true
                        title: model.name
                        detail: model.author ? qsTr("%1 by %2").arg(ServicesDiscoveryModel.getLabelForType(model.type))
                                                               .arg(model.author)
                                             : ServicesDiscoveryModel.getLabelForType(model.type)

                        Widgets.TextToolButton {
                            enabled: addonEntry.model.state === ServicesDiscoveryModel.STATE_INSTALLED
                                     || addonEntry.model.state === ServicesDiscoveryModel.STATE_NOTINSTALLED
                            text: {
                                switch (addonEntry.model.state) {
                                case ServicesDiscoveryModel.STATE_INSTALLED: return qsTr("Remove")
                                case ServicesDiscoveryModel.STATE_INSTALLING: return qsTr("Installing…")
                                case ServicesDiscoveryModel.STATE_UNINSTALLING: return qsTr("Removing…")
                                }
                                return qsTr("Install")
                            }
                            Accessible.name: qsTr("%1 %2").arg(text).arg(addonEntry.model.name)
                            onClicked: {
                                if (addonEntry.model.state === ServicesDiscoveryModel.STATE_INSTALLED)
                                    addonsModel.removeService(addonEntry.index)
                                else
                                    addonsModel.installService(addonEntry.index)
                            }
                        }
                    }
                }
            }

            AddButton {
                visible: !addons.findMore
                text: qsTr("Find More…")
                onClicked: {
                    addons.findMore = true
                    addonsModel.loadFromDefaultRepository()
                }
            }
        }
    }

    /* The built-in plugins, read-only */
    Component {
        id: pluginsComponent

        ColumnLayout {
            spacing: VLCStyle.spacing_8

            PluginsListModel {
                id: pluginsModel
            }

            Widgets.TextFieldExt {
                Layout.fillWidth: true
                implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
                radius: VLCStyle.radius_normal
                placeholderText: qsTr("Filter plugins")
                Accessible.name: placeholderText
                onTextChanged: pluginsModel.searchPattern = text
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.dp(320, VLCStyle.scale)
                radius: VLCStyle.radius_normal
                color: theme.bg.secondary

                ListView {
                    id: pluginsView

                    anchors.fill: parent
                    anchors.margins: VLCStyle.spacing_8
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: pluginsModel

                    delegate: RowLayout {
                        required property string name
                        required property string capability
                        required property string object

                        // clear of the scroll bar
                        width: pluginsView.width - VLCStyle.margin_small
                        spacing: VLCStyle.spacing_10

                        Text {
                            Layout.fillWidth: true
                            text: parent.name
                            elide: Text.ElideRight
                            color: theme.fg.primary
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.family: VLCStyle.fontFamily
                        }

                        Text {
                            text: parent.capability
                            color: theme.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_small
                            font.family: VLCStyle.fontFamily
                        }
                    }

                    T.ScrollBar.vertical: Widgets.ScrollBarExt { }
                }
            }

            Text {
                text: qsTr("%n plugin(s)", "", pluginsModel.count)
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
            }
        }
    }
}
