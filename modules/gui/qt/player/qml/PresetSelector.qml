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
import VLC.Player
import VLC.Dialogs
import VLC.Menus
import VLC.Style

// The Preset picker of the Adjustments sidebar's footer (Figma 2372:8834):
// "Default" (the Profile's values only), the Presets, each with Rename,
// Duplicate and Delete, and "+ New Preset". Choosing one attaches it to the
// playing file's folder.
T.AbstractButton {
    id: root

    readonly property bool _canChoose: Presets.folder !== ""

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ToolButton

        enabled: root.enabled
        focused: root.visualFocus
        hovered: root.hovered
        pressed: root.down
    }

    implicitWidth: Math.max(implicitBackgroundWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight, implicitContentHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xxsmall
    leftPadding: VLCStyle.margin_xsmall
    rightPadding: VLCStyle.margin_xsmall

    font.pixelSize: VLCStyle.fontSize_normal

    text: Presets.activeId !== "" ? Presets.activeName : qsTr("Default")

    Accessible.role: Accessible.ComboBox
    Accessible.name: qsTr("Preset")
    Accessible.description: text
    Accessible.onPressAction: root.clicked()

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    onClicked: popup.open()

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        color: theme.bg.primary
        border.color: root.visualFocus ? theme.visualFocus : Qt.alpha(theme.visualFocus, 0.0)
        radius: VLCStyle.button_radius
    }

    contentItem: RowLayout {
        spacing: VLCStyle.margin_xsmall

        T.Label {
            Layout.fillWidth: true

            text: root.text
            font: root.font
            color: theme.fg.primary
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter

            Accessible.ignored: true
        }

        Widgets.IconLabel {
            text: VLCIcons.chevron_down
            color: theme.fg.secondary
            font.pixelSize: VLCStyle.icon_small
        }
    }

    Widgets.ToolTipExt {
        colorContext.palette: theme.palette
        visible: root.hovered && !popup.opened
        delay: VLCStyle.delayToolTipAppear
        text: {
            if (!root._canChoose)
                return qsTr("Presets apply to the files of a folder")
            if (Presets.activeId === "")
                return qsTr("%1 uses the Profile's values").arg(Presets.folderName)
            if (Presets.activeFolder !== Presets.folder)
                return qsTr("%1 for the files of %2").arg(Presets.activeName)
                                                     .arg(Presets.activeFolder.split("/").pop())
            return qsTr("%1 for the files of %2").arg(Presets.activeName).arg(Presets.folderName)
        }
    }

    // the list, above the button
    T.Popup {
        id: popup

        x: 0
        y: -height - VLCStyle.margin_xxsmall
        width: Math.max(root.width, VLCStyle.dp(260, VLCStyle.scale))
        implicitHeight: implicitContentHeight + topPadding + bottomPadding

        padding: VLCStyle.margin_xxsmall
        modal: false
        focus: true
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

        onOpened: list.itemAt(0)?.forceActiveFocus(Qt.TabFocusReason)
        onClosed: root.forceActiveFocus(Qt.TabFocusReason)

        readonly property ColorContext colorContext: ColorContext {
            id: popupTheme
            // a popup is outside the sidebar's items: give it the palette
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        background: Rectangle {
            radius: VLCStyle.radius_normal
            color: popupTheme.bg.secondary
            border.width: VLCStyle.border
            border.color: popupTheme.border
        }

        contentItem: ColumnLayout {
            spacing: 0

            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Presets")

            Repeater {
                id: list

                // "Default", then the Presets
                model: [{ id: "", name: qsTr("Default") }].concat(root._presets())

                delegate: PresetRow {
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true

                    presetId: modelData.id
                    name: modelData.name

                    Keys.onUpPressed: (event) => {
                        if (index > 0)
                            list.itemAt(index - 1).forceActiveFocus(Qt.TabFocusReason)
                        event.accepted = true
                    }
                    Keys.onDownPressed: (event) => {
                        if (index < list.count - 1)
                            list.itemAt(index + 1).forceActiveFocus(Qt.TabFocusReason)
                        else
                            newButton.forceActiveFocus(Qt.TabFocusReason)
                        event.accepted = true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: VLCStyle.margin_xxsmall
                Layout.bottomMargin: VLCStyle.margin_xxsmall
                implicitHeight: VLCStyle.border
                color: popupTheme.border
            }

            Widgets.TextToolButton {
                id: newButton

                Layout.fillWidth: true

                text: qsTr("+ New Preset…")
                colorContext.palette: VLCStyle.palette

                Keys.onUpPressed: (event) => {
                    list.itemAt(list.count - 1)?.forceActiveFocus(Qt.TabFocusReason)
                    event.accepted = true
                }

                onClicked: {
                    popup.close()
                    nameDialog._renaming = ""
                    nameDialog.ask(qsTr("New Preset"),
                                   root._canChoose
                                       ? qsTr("It starts with this file's adjustments that differ from the Profile, and applies to the files of %1.").arg(Presets.folderName)
                                       : qsTr("It starts with this file's adjustments that differ from the Profile."),
                                   "",
                                   (name) => Presets.createFromCurrent(name))
                }
            }
        }
    }

    // the rows are rebuilt when the Presets change
    function _presets() {
        const model = Presets.model
        void model.count
        void _revision
        const list = []
        for (let row = 0; row < model.count; ++row) {
            const index = model.index(row, 0)
            list.push({ id: model.data(index, PresetsModel.ID), name: model.data(index, PresetsModel.NAME) })
        }
        return list
    }

    property int _revision: 0

    Connections {
        target: Presets.model

        function onDataChanged() { root._revision++ }
        function onRowsInserted() { root._revision++ }
        function onRowsRemoved() { root._revision++ }
    }

    TextInputDialog {
        id: nameDialog

        // the Preset being renamed, empty for a new one
        property string _renaming: ""

        acceptText: _renaming === "" ? qsTr("Create") : qsTr("Rename")
        placeholderText: qsTr("e.g., Anime")
        validate: (text) => Presets.model.isNameAvailable(text, nameDialog._renaming)
                            ? "" : qsTr("A Preset with this name already exists.")

        onClosed: root.forceActiveFocus(Qt.TabFocusReason)
    }

    StringListMenu {
        id: presetMenu

        property string presetId: ""

        ctx: MainCtx

        onSelected: (index, _) => {
            const id = presetMenu.presetId
            popup.close()
            switch (index) {
            case 0:
                nameDialog._renaming = id
                nameDialog.ask(qsTr("Rename Preset"), "", Presets.model.name(id),
                               (name) => Presets.model.rename(id, name))
                break
            case 1:
                Presets.model.duplicate(id)
                break
            case 2:
                Presets.model.remove(id)
                break
            }
        }
    }

    // a row of the list: the check of the active one, the name and "…"
    component PresetRow : T.ItemDelegate {
        id: row

        property string presetId
        property string name

        readonly property bool active: Presets.activeId === presetId

        enabled: root._canChoose || presetId !== ""

        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
        leftPadding: VLCStyle.margin_xsmall
        rightPadding: VLCStyle.margin_xxsmall

        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        Accessible.role: Accessible.RadioButton
        // the button resets the attached checked state to its own
        checked: active
        Accessible.name: name
        Accessible.checkable: true
        Accessible.checked: active
        Accessible.onPressAction: row.clicked()

        Keys.onReturnPressed: row.clicked()
        Keys.onSpacePressed: row.clicked()

        onClicked: {
            if (!root._canChoose)
                return
            Presets.choose(presetId)
            popup.close()
        }

        readonly property ColorContext colorContext: ColorContext {
            id: rowTheme
            palette: VLCStyle.palette
            colorSet: ColorContext.Item

            focused: row.activeFocus
            hovered: row.hovered
            enabled: row.enabled
        }

        background: Widgets.AnimatedBackground {
            enabled: rowTheme.initialized
            color: row.hovered || row.activeFocus ? rowTheme.bg.highlight : Qt.alpha(rowTheme.bg.highlight, 0.0)
            border.color: row.visualFocus ? rowTheme.visualFocus : Qt.alpha(rowTheme.visualFocus, 0.0)
            radius: VLCStyle.radius_small
        }

        contentItem: RowLayout {
            spacing: VLCStyle.margin_xsmall

            Widgets.IconLabel {
                Layout.preferredWidth: VLCStyle.icon_small

                text: VLCIcons.check
                visible: row.active
                color: popupTheme.accent
                font.pixelSize: VLCStyle.icon_small
            }

            Item {
                visible: !row.active
                Layout.preferredWidth: VLCStyle.icon_small
            }

            T.Label {
                Layout.fillWidth: true

                text: row.name
                color: rowTheme.fg.primary
                opacity: row.enabled ? 1 : 0.5
                font.pixelSize: VLCStyle.fontSize_normal
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Widgets.IconToolButton {
                visible: row.presetId !== ""

                text: VLCIcons.more
                colorContext.palette: VLCStyle.palette
                description: qsTr("More actions for %1").arg(row.name)
                font.pixelSize: VLCStyle.icon_small

                // NOTE: The glyph stays small, the target is 24 dp (WCAG 2.5.8).
                implicitWidth: VLCStyle.icon_normal
                implicitHeight: VLCStyle.icon_normal

                onClicked: {
                    presetMenu.presetId = row.presetId
                    presetMenu.popup(mapToGlobal(0, height),
                                     [qsTr("Rename…"), qsTr("Duplicate"), qsTr("Delete")])
                }
            }
        }
    }
}
