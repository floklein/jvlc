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

// Keyboard Shortcuts (Figma 2437:14832): a dialog over the main window. Find
// and rebind every action. Changes apply on Save, into the active Profile;
// Cancel, Close or Esc drop them.
ModalDialog {
    id: root

    parent: T.Overlay.overlay

    // also the dialog's name for screen readers
    title: qsTr("Keyboard Shortcuts")

    // within the window, at large scales or in a small window too: the list
    // scrolls
    width: Math.min(VLCStyle.dp(1048, VLCStyle.scale), (parent?.width ?? 0) - 2 * VLCStyle.margin_large)
    height: Math.min(VLCStyle.dp(800, VLCStyle.scale), (parent?.height ?? 0) - 2 * VLCStyle.margin_large)

    padding: 0
    topPadding: 0
    spacing: 0

    // Esc goes through the Shortcut below: it closes a prompt or the search
    // first
    closePolicy: T.Popup.NoAutoClose

    background: Rectangle {
        color: theme.bg.primary
        radius: VLCStyle.radius_large
        border.color: theme.border
    }

    // the title and Close, on the search bar's color, as drawn
    header: Rectangle {
        implicitHeight: VLCStyle.dp(50, VLCStyle.scale)
        color: theme.bg.secondary
        topLeftRadius: VLCStyle.radius_large
        topRightRadius: VLCStyle.radius_large

        // the header takes no palette from the content
        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.dp(18, VLCStyle.scale)
            anchors.rightMargin: VLCStyle.spacing_10
            spacing: VLCStyle.spacing_10

            Text {
                Layout.fillWidth: true
                text: root.title
                elide: Text.ElideRight
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily

                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Widgets.IconToolButton {
                text: VLCIcons.close
                description: qsTr("Close")
                font.pixelSize: VLCStyle.icon_small
                // NOTE: The glyph stays small, the target is 24 dp (WCAG 2.5.8).
                implicitWidth: VLCStyle.icon_normal
                implicitHeight: VLCStyle.icon_normal
                onClicked: root.cancel()
            }
        }
    }

    // the key cell listening for a key
    property string listeningName: ""
    property bool listeningGlobal: false

    // a key used by another action, waiting for "Replace?"
    property string promptName: ""
    property bool promptGlobal: false
    property int promptKey: 0
    property bool promptReset: false
    property string promptUser: ""

    readonly property bool searching: searchField.text.trim() !== ""

    function openOn(action: string) {
        searchField.text = ""
        filterCombo.currentIndex = 0
        cancelPrompt()
        stopListening()
        root.open()
        if (action !== "")
            Qt.callLater(root.showAction, action)
    }

    function showAction(action: string) {
        const row = rowsModel.rowOf(action)
        if (row < 0)
            return
        rowsView.currentIndex = row
        rowsView.positionViewAtIndex(row, ListView.Center)
    }

    function listen(name: string, global: bool) {
        cancelPrompt()
        root.listeningName = name
        root.listeningGlobal = global
    }

    function stopListening() {
        root.listeningName = ""
    }

    // Esc cancels, Backspace clears, a taken key asks first
    function keyPressed(name: string, global: bool, key: int, modifiers: int) {
        if (modifiers === Qt.NoModifier && key === Qt.Key_Escape) {
            stopListening()
            return
        }
        if (modifiers === Qt.NoModifier && key === Qt.Key_Backspace) {
            shortcuts.clear(name, global)
            stopListening()
            return
        }
        const code = shortcuts.keyFromEvent(key, modifiers)
        if (code === 0)
            return // modifiers alone: keep listening
        stopListening()

        const user = shortcuts.conflict(name, global, code)
        if (user === "") {
            shortcuts.assign(name, global, code)
            return
        }
        root.promptName = name
        root.promptGlobal = global
        root.promptKey = code
        root.promptReset = false
        root.promptUser = user
    }

    function resetAction(name: string) {
        stopListening()
        cancelPrompt()
        const user = shortcuts.resetConflict(name)
        if (user === "") {
            shortcuts.reset(name)
            return
        }
        root.promptName = name
        root.promptReset = true
        root.promptUser = user
    }

    function replace() {
        if (root.promptReset)
            shortcuts.reset(root.promptName)
        else
            shortcuts.assign(root.promptName, root.promptGlobal, root.promptKey)
        cancelPrompt()
    }

    function cancelPrompt() {
        root.promptName = ""
        root.promptUser = ""
    }

    function cancel() {
        shortcuts.revert()
        root.close()
    }

    function save() {
        shortcuts.save()
        root.close()
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: {
            if (root.promptName !== "")
                root.cancelPrompt()
            else if (root.searching)
                searchField.text = ""
            else
                root.cancel()
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: searchField.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    ShortcutsModel {
        id: shortcuts
        ctx: MainCtx
    }

    ShortcutsFilterModel {
        id: rowsModel
        sourceModel: shortcuts
        searchPattern: searchField.text.trim()
        field: filterCombo.currentValue ?? ShortcutsFilterModel.AllFields
    }

    // widths of the columns, see Figma
    readonly property real keyColumnWidth: VLCStyle.dp(270, VLCStyle.scale)
    readonly property real resetColumnWidth: VLCStyle.dp(32, VLCStyle.scale)

    contentItem: FocusScope {
        focus: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // the contexts of the items below inherit this palette
            readonly property ColorContext colorContext: ColorContext {
                id: theme
                palette: VLCStyle.palette
                colorSet: ColorContext.View
            }

            // search and filter
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.dp(50, VLCStyle.scale)
                color: theme.bg.secondary

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: VLCStyle.spacing_8
                    anchors.rightMargin: VLCStyle.spacing_8
                    spacing: VLCStyle.spacing_10

                    Rectangle {
                        Layout.fillWidth: true
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

                            focus: true
                            placeholderText: qsTr("Search Keyboard Shortcuts…")
                            placeholderTextColor: theme.fg.secondary
                            color: theme.fg.primary
                            selectionColor: theme.bg.highlight
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.family: VLCStyle.fontFamily
                            verticalAlignment: Text.AlignVCenter

                            Accessible.name: qsTr("Search Keyboard Shortcuts")

                            onTextChanged: {
                                root.cancelPrompt()
                                rowsView.currentIndex = -1
                            }

                            KeyNavigation.right: filterCombo
                            KeyNavigation.down: rowsView

                            Text {
                                anchors.fill: parent
                                visible: searchField.text === ""
                                verticalAlignment: Text.AlignVCenter
                                text: searchField.placeholderText
                                color: theme.fg.secondary
                                font: searchField.font
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Widgets.ComboBoxExt {
                        id: filterCombo

                        Layout.preferredWidth: VLCStyle.dp(200, VLCStyle.scale)
                        Layout.preferredHeight: VLCStyle.dp(34, VLCStyle.scale)

                        textRole: "label"
                        valueRole: "value"
                        model: [
                            { label: qsTr("All Fields"), value: ShortcutsFilterModel.AllFields },
                            { label: qsTr("Action"), value: ShortcutsFilterModel.Action },
                            { label: qsTr("Key"), value: ShortcutsFilterModel.Key },
                        ]

                        font.pixelSize: VLCStyle.fontSize_normal
                        leftPadding: VLCStyle.spacing_14
                        rightPadding: VLCStyle.spacing_10
                        color: theme.fg.secondary
                        Accessible.name: qsTr("Search in")

                        onActivated: rowsView.currentIndex = -1

                        KeyNavigation.down: rowsView

                        contentItem: Text {
                            rightPadding: filterCombo.spacing + filterCombo.indicator.width
                            text: filterCombo.displayText
                            color: filterCombo.color
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.family: VLCStyle.fontFamily
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            radius: VLCStyle.radius_normal
                            color: filterCombo.hovered ? theme.bg.highlight : windowTheme.bg.primary
                            border.width: filterCombo.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                            border.color: theme.accent
                        }
                    }
                }
            }

            // column titles
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_normal
                Layout.rightMargin: VLCStyle.margin_normal
                Layout.preferredHeight: VLCStyle.dp(64, VLCStyle.scale)
                spacing: VLCStyle.spacing_10

                component ColumnTitle: Text {
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    Accessible.role: Accessible.ColumnHeader
                    Accessible.name: text
                }

                ColumnTitle {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("Action")
                }

                ColumnTitle {
                    Layout.preferredWidth: root.keyColumnWidth
                    text: qsTr("Key")
                }

                RowLayout {
                    Layout.fillWidth: false
                    Layout.preferredWidth: root.keyColumnWidth
                    spacing: VLCStyle.spacing_4

                    Item { Layout.fillWidth: true }

                    ColumnTitle {
                        text: qsTr("Global key")
                    }

                    Widgets.IconToolButton {
                        text: VLCIcons.info
                        font.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
                        // NOTE: The glyph stays small, the target is 24 dp (WCAG 2.5.8).
                        implicitWidth: VLCStyle.icon_normal
                        implicitHeight: VLCStyle.icon_normal
                        description: qsTr("Global keys work even when another app is in front. "
                                          + "Changes to them apply after jVLC restarts.")
                    }

                    Item { Layout.fillWidth: true }
                }

                Item { Layout.preferredWidth: root.resetColumnWidth }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.margin_normal
                Layout.rightMargin: VLCStyle.margin_normal
                Layout.preferredHeight: 1
                color: theme.separator
            }

            ListView {
                id: rowsView

                Layout.fillWidth: true
                Layout.fillHeight: true

                clip: true
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                currentIndex: -1
                highlightFollowsCurrentItem: false

                leftMargin: VLCStyle.margin_normal
                rightMargin: VLCStyle.margin_normal
                bottomMargin: VLCStyle.margin_normal

                model: rowsModel

                section.property: "groupLabel"
                section.criteria: ViewSection.FullString
                section.delegate: Item {
                    required property string section

                    width: rowsView.width - rowsView.leftMargin - rowsView.rightMargin
                    height: VLCStyle.dp(66, VLCStyle.scale)

                    Text {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: VLCStyle.spacing_14
                        text: parent.section
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_xlarge
                        font.family: VLCStyle.fontFamily
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                }

                delegate: FocusScope {
                    id: row

                    required property int index
                    required property string name
                    required property string label
                    required property var keys
                    required property var globalKeys
                    required property bool hasGlobalKey
                    required property bool isDefault

                    readonly property bool prompting: root.promptName === name

                    width: rowsView.width - rowsView.leftMargin - rowsView.rightMargin
                    height: content.implicitHeight + VLCStyle.spacing_14

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: -VLCStyle.spacing_8
                        anchors.rightMargin: -VLCStyle.spacing_8
                        visible: row.ListView.isCurrentItem
                        radius: VLCStyle.radius_normal
                        color: theme.bg.highlight
                    }

                    ColumnLayout {
                        id: content

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: VLCStyle.spacing_8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: VLCStyle.spacing_10

                            Text {
                                Layout.fillWidth: true
                                text: row.label
                                elide: Text.ElideRight
                                color: theme.fg.primary
                                font.pixelSize: VLCStyle.fontSize_normal
                                font.family: VLCStyle.fontFamily

                                // the keys differ from the defaults
                                Rectangle {
                                    visible: !row.isDefault
                                    anchors.right: parent.left
                                    anchors.rightMargin: VLCStyle.spacing_4
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: VLCStyle.dp(6, VLCStyle.scale)
                                    height: width
                                    radius: width / 2
                                    color: theme.accent
                                }
                            }

                            ShortcutKeyField {
                                id: keyField

                                Layout.preferredWidth: root.keyColumnWidth
                                keys: row.keys
                                listening: root.listeningName === row.name && !root.listeningGlobal
                                accessibleName: qsTr("Key of %1").arg(row.label)

                                onClicked: {
                                    rowsView.currentIndex = row.index
                                    root.listen(row.name, false)
                                    forceActiveFocus(Qt.MouseFocusReason)
                                }
                                onKeyPressed: (key, modifiers) => root.keyPressed(row.name, false, key, modifiers)
                                onActiveFocusChanged: {
                                    if (!activeFocus && listening && rowsView.Window.active)
                                        root.stopListening()
                                }
                            }

                            ShortcutKeyField {
                                id: globalKeyField

                                Layout.preferredWidth: root.keyColumnWidth
                                enabled: row.hasGlobalKey
                                keys: row.globalKeys
                                listening: root.listeningName === row.name && root.listeningGlobal
                                accessibleName: qsTr("Global key of %1").arg(row.label)

                                onClicked: {
                                    rowsView.currentIndex = row.index
                                    root.listen(row.name, true)
                                    forceActiveFocus(Qt.MouseFocusReason)
                                }
                                onKeyPressed: (key, modifiers) => root.keyPressed(row.name, true, key, modifiers)
                                onActiveFocusChanged: {
                                    if (!activeFocus && listening && rowsView.Window.active)
                                        root.stopListening()
                                }
                            }

                            Item {
                                Layout.preferredWidth: root.resetColumnWidth
                                Layout.preferredHeight: resetButton.implicitHeight

                                Widgets.IconToolButton {
                                    id: resetButton
                                    anchors.centerIn: parent
                                    visible: !row.isDefault
                                    text: VLCIcons.refresh
                                    font.pixelSize: VLCStyle.dp(18, VLCStyle.scale)
                                    // NOTE: The glyph stays small, the target is 24 dp (WCAG 2.5.8).
                                    implicitWidth: VLCStyle.icon_normal
                                    implicitHeight: VLCStyle.icon_normal
                                    description: qsTr("Reset “%1”").arg(row.label)
                                    onClicked: {
                                        rowsView.currentIndex = row.index
                                        root.resetAction(row.name)
                                    }
                                }
                            }
                        }

                        // "Used by X: Replace?"
                        RowLayout {
                            visible: row.prompting
                            Layout.fillWidth: true
                            spacing: VLCStyle.spacing_10

                            Accessible.role: Accessible.AlertMessage
                            Accessible.name: promptText.text

                            Item { Layout.fillWidth: true }

                            Text {
                                id: promptText
                                text: root.promptReset
                                      ? qsTr("A default key is used by “%1”: Replace?").arg(root.promptUser)
                                      : qsTr("%1 is used by “%2”: Replace?").arg(shortcuts.keyText(root.promptKey))
                                                                             .arg(root.promptUser)
                                color: theme.fg.primary
                                font.pixelSize: VLCStyle.fontSize_normal
                                font.family: VLCStyle.fontFamily

                                onTextChanged: {
                                    if (row.prompting)
                                        Qt.callLater(row.announcePrompt)
                                }
                            }

                            Widgets.ButtonExt {
                                text: qsTr("Cancel")
                                onClicked: root.cancelPrompt()
                            }

                            Widgets.ActionButtonPrimary {
                                id: replaceButton
                                text: qsTr("Replace")
                                onClicked: root.replace()
                            }

                            Item { Layout.preferredWidth: root.resetColumnWidth }
                        }
                    }

                    onPromptingChanged: {
                        if (prompting) {
                            replaceButton.forceActiveFocus(Qt.OtherFocusReason)
                            Qt.callLater(row.announcePrompt)
                        }
                    }

                    // screen readers read "Used by X: Replace?" when it shows or changes
                    function announcePrompt() {
                        if (row.prompting)
                            Helpers.announce(promptText, promptText.text)
                    }
                }

                T.ScrollBar.vertical: Widgets.ScrollBarExt { }

                Text {
                    anchors.centerIn: parent
                    visible: rowsView.count === 0
                    width: parent.width - 2 * VLCStyle.margin_large
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("No shortcuts match “%1”.").arg(searchField.text.trim())
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }
            }

            // Cancel, Reset all, Save
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: VLCStyle.margin_normal
                spacing: VLCStyle.spacing_10

                Widgets.ButtonExt {
                    text: qsTr("Cancel")
                    onClicked: root.cancel()
                }

                Item { Layout.fillWidth: true }

                Widgets.ButtonExt {
                    text: qsTr("Reset all")
                    Accessible.name: qsTr("Reset all shortcuts")
                    onClicked: {
                        root.stopListening()
                        root.cancelPrompt()
                        shortcuts.resetAll()
                    }
                }

                Widgets.ActionButtonPrimary {
                    text: qsTr("Save")
                    enabled: shortcuts.modified
                    onClicked: root.save()
                }
            }
        }
    }
}
