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
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util
import VLC.Dialogs

// Properties & Metadata: tags (General), codec details (Codec) and live
// statistics (Statistics) of one media. MainInterface.qml creates one window
// per DialogsProvider.showMediaProperties() model; the window owns the model.
WindowDialog {
    id: root

    required property MediaPropertiesModel properties
    property int currentTab: MediaPropertiesModel.GeneralTab

    title: qsTr("Properties & Metadata")

    width: VLCStyle.dp(620, VLCStyle.scale)
    height: VLCStyle.dp(700, VLCStyle.scale)
    minimumWidth: VLCStyle.dp(480, VLCStyle.scale)
    minimumHeight: VLCStyle.dp(420, VLCStyle.scale)

    color: colorContext.bg.primary

    standardButtons: DialogButtonBox.Close

    readonly property var _tabs: {
        const tabs = [
            { tab: MediaPropertiesModel.GeneralTab, text: qsTr("General") },
            { tab: MediaPropertiesModel.CodecTab, text: qsTr("Codec") }
        ]
        if (properties.live)
            tabs.push({ tab: MediaPropertiesModel.StatisticsTab, text: qsTr("Statistics") })
        return tabs
    }

    component SectionLabel: T.Label {
        color: root.colorContext.fg.primary
        font.pixelSize: VLCStyle.fontSize_normal
        font.weight: Font.DemiBold
        textFormat: Text.PlainText
        elide: Text.ElideRight
    }

    component FieldLabel: T.Label {
        color: root.colorContext.fg.secondary
        font.pixelSize: VLCStyle.fontSize_small
        textFormat: Text.PlainText
        elide: Text.ElideRight
    }

    component TagField: ColumnLayout {
        id: tagField

        property alias label: fieldLabel.text
        property string value
        signal edited(string text)

        spacing: VLCStyle.margin_xxsmall

        FieldLabel {
            id: fieldLabel
            Layout.fillWidth: true
        }

        Widgets.TextFieldExt {
            Layout.fillWidth: true

            text: tagField.value
            readOnly: !root.properties.canSave
            selectByMouse: true
            radius: VLCStyle.radius_small

            Accessible.name: fieldLabel.text

            // not onTextEdited: assistive technologies set the text directly
            onTextChanged: {
                if (activeFocus)
                    tagField.edited(text)
            }
        }
    }

    // A list of named groups of name/value rows (Codec and Statistics)
    component InfoGroups: Flickable {
        id: infoFlickable

        property var groups: []
        property string rowsRole: "rows"

        clip: true
        contentWidth: width
        contentHeight: groupsColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Widgets.ScrollBarExt { }

        ColumnLayout {
            id: groupsColumn

            width: infoFlickable.width - VLCStyle.margin_small
            spacing: VLCStyle.margin_normal

            Repeater {
                model: infoFlickable.groups

                delegate: ColumnLayout {
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: VLCStyle.margin_xxsmall

                    SectionLabel {
                        Layout.fillWidth: true
                        text: modelData.name
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: VLCStyle.margin_normal
                        rowSpacing: VLCStyle.margin_xxxsmall

                        Repeater {
                            model: modelData[infoFlickable.rowsRole]

                            delegate: FieldLabel {
                                required property var modelData
                                required property int index

                                Layout.row: index
                                Layout.column: 0
                                Layout.preferredWidth: VLCStyle.dp(170, VLCStyle.scale)
                                text: modelData.name
                            }
                        }

                        Repeater {
                            model: modelData[infoFlickable.rowsRole]

                            delegate: TextEdit {
                                required property var modelData
                                required property int index

                                Layout.row: index
                                Layout.column: 1
                                Layout.fillWidth: true

                                text: modelData.value
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.Wrap
                                textFormat: TextEdit.PlainText
                                color: root.colorContext.fg.primary
                                selectionColor: root.colorContext.bg.highlight
                                selectedTextColor: root.colorContext.fg.highlight
                                font.pixelSize: VLCStyle.fontSize_small
                            }
                        }
                    }
                }
            }
        }
    }

    component EmptyMessage: T.Label {
        color: root.colorContext.fg.secondary
        font.pixelSize: VLCStyle.fontSize_normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }

    // Emitted when the window closes, just before it destroys itself
    signal finished()

    onVisibleChanged: {
        if (!visible) {
            finished()
            root.destroy()
        }
    }

    Component.onDestruction: {
        // the window owns its model
        if (root.properties)
            root.properties.deleteLater()
    }

    contentComponent: FocusScope {
        focus: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.margin_xsmall
            spacing: VLCStyle.margin_normal

            // Tab bar
            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.margin_xsmall

                Repeater {
                    model: root._tabs

                    delegate: T.TabButton {
                        id: tabButton

                        required property var modelData

                        text: modelData.text
                        checked: root.currentTab === modelData.tab

                        padding: VLCStyle.margin_xsmall
                        implicitWidth: implicitContentWidth + leftPadding + rightPadding
                        implicitHeight: implicitContentHeight + topPadding + bottomPadding

                        onClicked: root.currentTab = modelData.tab

                        ColorContext {
                            id: tabTheme
                            colorSet: ColorContext.TabButton
                            focused: tabButton.visualFocus
                            hovered: tabButton.hovered
                            pressed: tabButton.down
                            enabled: tabButton.enabled
                        }

                        contentItem: T.Label {
                            text: tabButton.text
                            color: tabButton.checked ? tabTheme.fg.primary : tabTheme.fg.secondary
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.weight: tabButton.checked ? Font.DemiBold : Font.Normal
                            textFormat: Text.PlainText
                        }

                        background: Rectangle {
                            color: "transparent"
                            border.width: tabButton.visualFocus ? VLCStyle.dp(1, VLCStyle.scale) : 0
                            border.color: tabTheme.visualFocus
                            radius: VLCStyle.radius_small

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: VLCStyle.dp(2, VLCStyle.scale)
                                visible: tabButton.checked
                                color: root.colorContext.accent
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                currentIndex: Math.max(0, root._tabs.findIndex((t) => t.tab === root.currentTab))

                // General
                Item {
                    EmptyMessage {
                        anchors.fill: parent
                        visible: !root.properties.hasMedia
                        text: qsTr("Nothing is playing.")
                    }

                    Flickable {
                        id: generalFlickable

                        anchors.fill: parent
                        visible: root.properties.hasMedia

                        clip: true
                        contentWidth: width
                        contentHeight: generalColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: Widgets.ScrollBarExt { }

                        ColumnLayout {
                            id: generalColumn

                            width: generalFlickable.width - VLCStyle.margin_small
                            spacing: VLCStyle.margin_normal

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: VLCStyle.margin_normal

                                // Artwork
                                ColumnLayout {
                                    Layout.alignment: Qt.AlignTop
                                    spacing: VLCStyle.margin_xsmall

                                    Rectangle {
                                        Layout.preferredWidth: VLCStyle.cover_normal
                                        Layout.preferredHeight: VLCStyle.cover_normal

                                        radius: VLCStyle.radius_normal
                                        color: root.colorContext.bg.secondary
                                        clip: true

                                        Image {
                                            anchors.fill: parent

                                            source: root.properties.artwork.toString() !== ""
                                                    ? VLCAccessImage.uri(root.properties.artwork)
                                                    : VLCStyle.noArtAlbumCover
                                            sourceSize.width: VLCStyle.cover_normal
                                            sourceSize.height: VLCStyle.cover_normal
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true

                                            Accessible.role: Accessible.Graphic
                                            Accessible.name: qsTr("Artwork")
                                        }
                                    }

                                    Widgets.ButtonExt {
                                        Layout.preferredWidth: VLCStyle.cover_normal

                                        text: qsTr("Choose…")
                                        visible: root.properties.canSave

                                        onClicked: {
                                            const url = DialogsProvider.getArtworkFileUrl()
                                            if (url.toString() !== "")
                                                root.properties.artwork = url
                                        }
                                    }
                                }

                                // Tags
                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignTop

                                    columns: 4
                                    columnSpacing: VLCStyle.margin_small
                                    rowSpacing: VLCStyle.margin_small

                                    TagField {
                                        Layout.columnSpan: 4
                                        Layout.fillWidth: true
                                        label: qsTr("Title")
                                        value: root.properties.title
                                        onEdited: (text) => root.properties.title = text
                                    }

                                    TagField {
                                        Layout.columnSpan: 4
                                        Layout.fillWidth: true
                                        label: qsTr("Artist")
                                        value: root.properties.artist
                                        onEdited: (text) => root.properties.artist = text
                                    }

                                    TagField {
                                        Layout.columnSpan: 3
                                        Layout.fillWidth: true
                                        label: qsTr("Album")
                                        value: root.properties.album
                                        onEdited: (text) => root.properties.album = text
                                    }

                                    TagField {
                                        Layout.preferredWidth: VLCStyle.dp(80, VLCStyle.scale)
                                        label: qsTr("Date")
                                        value: root.properties.date
                                        onEdited: (text) => root.properties.date = text
                                    }

                                    TagField {
                                        Layout.columnSpan: 2
                                        Layout.fillWidth: true
                                        label: qsTr("Genre")
                                        value: root.properties.genre
                                        onEdited: (text) => root.properties.genre = text
                                    }

                                    TagField {
                                        Layout.preferredWidth: VLCStyle.dp(64, VLCStyle.scale)
                                        label: qsTr("Track")
                                        value: root.properties.trackNumber
                                        onEdited: (text) => root.properties.trackNumber = text
                                    }

                                    TagField {
                                        Layout.preferredWidth: VLCStyle.dp(80, VLCStyle.scale)
                                        label: qsTr("of")
                                        value: root.properties.trackTotal
                                        onEdited: (text) => root.properties.trackTotal = text
                                    }
                                }
                            }

                            // Description
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: VLCStyle.margin_xxsmall

                                FieldLabel {
                                    Layout.fillWidth: true
                                    text: qsTr("Description")
                                }

                                TextArea {
                                    id: descriptionArea

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: VLCStyle.dp(88, VLCStyle.scale)

                                    readonly property ColorContext colorContext: ColorContext {
                                        id: descriptionTheme
                                        colorSet: ColorContext.TextField
                                        focused: descriptionArea.activeFocus
                                        hovered: descriptionArea.hovered
                                        enabled: descriptionArea.enabled
                                    }

                                    text: root.properties.description
                                    readOnly: !root.properties.canSave
                                    selectByMouse: true
                                    wrapMode: TextEdit.Wrap
                                    textFormat: TextEdit.PlainText

                                    color: descriptionTheme.fg.primary
                                    selectionColor: descriptionTheme.bg.highlight
                                    selectedTextColor: descriptionTheme.fg.highlight
                                    font.pixelSize: VLCStyle.fontSize_normal

                                    Accessible.name: qsTr("Description")

                                    background: Rectangle {
                                        color: descriptionTheme.bg.primary
                                        border.width: VLCStyle.dp(2, VLCStyle.scale)
                                        border.color: descriptionTheme.border
                                        radius: VLCStyle.radius_small
                                    }

                                    onTextChanged: {
                                        if (activeFocus)
                                            root.properties.description = text
                                    }
                                }
                            }

                            // Identify song
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: root.properties.canIdentify
                                spacing: VLCStyle.margin_xsmall

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: VLCStyle.margin_small

                                    Widgets.ButtonExt {
                                        text: qsTr("Identify song")
                                        enabled: root.properties.identifyState !== MediaPropertiesModel.IdentifySearching
                                        busy: root.properties.identifyState === MediaPropertiesModel.IdentifySearching

                                        T.ToolTip.text: qsTr("Find the title and artist with an AcoustID audio fingerprint")

                                        onClicked: root.properties.identify()
                                    }

                                    T.Label {
                                        Layout.fillWidth: true

                                        color: root.colorContext.fg.secondary
                                        font.pixelSize: VLCStyle.fontSize_small
                                        elide: Text.ElideRight
                                        textFormat: Text.PlainText

                                        text: {
                                            switch (root.properties.identifyState) {
                                            case MediaPropertiesModel.IdentifySearching:
                                                return qsTr("Listening to the song…")
                                            case MediaPropertiesModel.IdentifyFound:
                                                return qsTr("Pick the matching song:")
                                            case MediaPropertiesModel.IdentifyNotFound:
                                                return qsTr("No match found.")
                                            default:
                                                return ""
                                            }
                                        }
                                    }
                                }

                                Repeater {
                                    model: root.properties.identifyResults

                                    delegate: RowLayout {
                                        required property var modelData
                                        required property int index

                                        Layout.fillWidth: true
                                        spacing: VLCStyle.margin_small

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0

                                            T.Label {
                                                Layout.fillWidth: true
                                                text: modelData.title
                                                color: root.colorContext.fg.primary
                                                font.pixelSize: VLCStyle.fontSize_normal
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                                textFormat: Text.PlainText
                                            }

                                            FieldLabel {
                                                Layout.fillWidth: true
                                                text: modelData.artist
                                            }
                                        }

                                        Widgets.TextToolButton {
                                            text: qsTr("MusicBrainz")
                                            visible: !!modelData.url
                                            onClicked: Qt.openUrlExternally(modelData.url)
                                        }

                                        Widgets.ButtonExt {
                                            text: qsTr("Use")
                                            onClicked: root.properties.applyIdentity(index)
                                        }
                                    }
                                }

                                Widgets.TextToolButton {
                                    visible: root.properties.identifyState === MediaPropertiesModel.IdentifyFound
                                    text: qsTr("Discard")
                                    onClicked: root.properties.discardIdentities()
                                }
                            }

                            // Save
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: VLCStyle.margin_small

                                T.Label {
                                    id: saveMessage

                                    Layout.fillWidth: true

                                    property bool failed: false

                                    color: failed ? root.colorContext.fg.negative
                                                  : root.colorContext.fg.secondary
                                    font.pixelSize: VLCStyle.fontSize_small
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText

                                    text: {
                                        if (failed)
                                            return qsTr("The tags could not be written to the file.")
                                        if (!root.properties.canSave)
                                            return qsTr("Tags can only be saved to local files.")
                                        return ""
                                    }

                                    Connections {
                                        target: root.properties
                                        function onSaveFailed() { saveMessage.failed = true }
                                        function onSaved() { saveMessage.failed = false }
                                        function onMediaChanged() { saveMessage.failed = false }
                                    }
                                }

                                Widgets.ButtonExt {
                                    text: qsTr("Revert")
                                    visible: root.properties.canSave
                                    enabled: root.properties.dirty
                                    onClicked: root.properties.revert()
                                }

                                Widgets.ButtonExt {
                                    text: qsTr("Save")
                                    visible: root.properties.canSave
                                    enabled: root.properties.dirty
                                    colorContext.colorSet: ColorContext.ButtonAccent
                                    onClicked: root.properties.save()
                                }
                            }
                        }
                    }
                }

                // Codec
                Item {
                    EmptyMessage {
                        anchors.fill: parent
                        visible: root.properties.codecInfo.length === 0
                        text: root.properties.hasMedia ? qsTr("No codec details yet. They appear once the media is opened.")
                                                       : qsTr("Nothing is playing.")
                    }

                    InfoGroups {
                        anchors.fill: parent
                        visible: root.properties.codecInfo.length > 0
                        groups: root.properties.codecInfo
                        rowsRole: "infos"
                    }
                }

                // Statistics
                Item {
                    EmptyMessage {
                        anchors.fill: parent
                        visible: root.properties.statistics.length === 0
                        text: qsTr("Statistics appear while the media plays.")
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        visible: root.properties.statistics.length > 0
                        spacing: VLCStyle.margin_small

                        FieldLabel {
                            Layout.fillWidth: true
                            text: qsTr("Input bitrate, last minute")
                        }

                        // One bar per second, newest on the right (no Canvas
                        // in the static Qt build)
                        Item {
                            id: bitrateGraph

                            Layout.fillWidth: true
                            Layout.preferredHeight: VLCStyle.dp(72, VLCStyle.scale)

                            readonly property var history: root.properties.bitrateHistory
                            readonly property real maxValue: Math.max(1, ...history)
                            readonly property int slots: 60
                            readonly property real gap: VLCStyle.dp(1, VLCStyle.scale)
                            readonly property real barWidth: Math.max(1, (width - (slots - 1) * gap) / slots)

                            Accessible.role: Accessible.Chart
                            Accessible.name: qsTr("Input bitrate, last minute")

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: VLCStyle.dp(1, VLCStyle.scale)
                                color: root.colorContext.separator
                            }

                            Row {
                                anchors.right: parent.right
                                height: parent.height
                                spacing: bitrateGraph.gap

                                Repeater {
                                    model: bitrateGraph.history

                                    delegate: Rectangle {
                                        required property real modelData

                                        width: bitrateGraph.barWidth
                                        height: Math.max(VLCStyle.dp(1, VLCStyle.scale),
                                                         modelData / bitrateGraph.maxValue * (bitrateGraph.height - VLCStyle.dp(2, VLCStyle.scale)))
                                        y: parent.height - height
                                        radius: VLCStyle.radius_xxsmall
                                        color: root.colorContext.accent
                                    }
                                }
                            }
                        }

                        InfoGroups {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            groups: root.properties.statistics
                        }
                    }
                }
            }

            // Location
            RowLayout {
                Layout.fillWidth: true
                visible: root.properties.hasMedia
                spacing: VLCStyle.margin_small

                FieldLabel {
                    text: qsTr("Location")
                }

                TextEdit {
                    Layout.fillWidth: true

                    text: root.properties.location
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    textFormat: TextEdit.PlainText
                    color: root.colorContext.fg.primary
                    selectionColor: root.colorContext.bg.highlight
                    selectedTextColor: root.colorContext.fg.highlight
                    font.pixelSize: VLCStyle.fontSize_small

                    Accessible.name: qsTr("Location")
                }
            }
        }
    }
}
