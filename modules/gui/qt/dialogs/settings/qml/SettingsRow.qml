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

// One setting: label, help, tags, its control and Reset. Instantiated as a
// delegate of a SettingsFilterModel, it reads the row through `model`.
FocusScope {
    id: root

    // the AppSettingsModel, for its helpers
    property var settingsModel
    // shown above the label in search results
    property string path

    readonly property bool isSetting: model.kind === SettingsModel.Setting
    readonly property bool changed: isSetting && !model.isDefault
    property bool helpShown: false

    implicitHeight: {
        switch (model.kind) {
        case SettingsModel.AdvancedHeader: return advancedHeader.implicitHeight + VLCStyle.spacing_20
        case SettingsModel.GroupHeader: return groupHeader.implicitHeight + VLCStyle.spacing_4
        }
        return content.implicitHeight + 2 * VLCStyle.spacing_5
    }

    function write(value) {
        model.value = value
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    // the Advanced Options section of the page, below its basic rows
    Widgets.ButtonExt {
        id: advancedHeader

        visible: model.kind === SettingsModel.AdvancedHeader
        focus: visible

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)

        text: model.label
        iconTxt: model.expanded ? VLCIcons.chevron_up : VLCIcons.chevron_down
        iconSize: VLCStyle.icon_small
        font.pixelSize: VLCStyle.fontSize_normal
        font.weight: Font.Normal

        Accessible.role: Accessible.Button
        Accessible.name: model.label
        Accessible.checkable: true
        Accessible.checked: model.expanded

        background: Rectangle {
            radius: VLCStyle.radius_normal
            color: advancedHeader.hovered || advancedHeader.visualFocus ? theme.bg.highlight : theme.bg.secondary
            border.width: advancedHeader.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: theme.accent
        }

        onClicked: root.settingsModel.setAdvancedExpanded(model.page, !model.expanded)
    }

    // the group of one plugin, collapsed until opened
    T.AbstractButton {
        id: groupHeader

        visible: model.kind === SettingsModel.GroupHeader
        focus: visible
        focusPolicy: Qt.StrongFocus

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        implicitHeight: VLCStyle.dp(36, VLCStyle.scale)

        Accessible.role: Accessible.Button
        Accessible.name: model.groupLabel
        Accessible.checkable: true
        Accessible.checked: model.expanded
        Accessible.onPressAction: groupHeader.clicked()

        background: Rectangle {
            radius: VLCStyle.radius_normal
            color: groupHeader.hovered ? theme.bg.highlight : "transparent"
            border.width: groupHeader.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
            border.color: theme.accent
        }

        contentItem: RowLayout {
            spacing: VLCStyle.spacing_8

            Widgets.IconLabel {
                Layout.leftMargin: VLCStyle.spacing_4
                text: VLCIcons.chevron_down
                rotation: model.expanded ? 0 : -90
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.icon_small
            }

            Text {
                Layout.fillWidth: true
                text: model.groupLabel
                elide: Text.ElideRight
                color: theme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
                font.weight: Font.Medium
                font.family: VLCStyle.fontFamily
            }
        }

        onClicked: root.settingsModel.setGroupExpanded(model.page, model.group, !model.expanded)
    }

    RowLayout {
        id: content

        visible: root.isSetting
        // the rows of a plugin group are indented under its title, but not
        // in search results
        anchors.leftMargin: model.group !== "" && root.path === "" ? VLCStyle.margin_large : 0

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        spacing: VLCStyle.spacing_10

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: VLCStyle.spacing_4

            Text {
                visible: root.path !== ""
                Layout.fillWidth: true
                text: root.path
                elide: Text.ElideRight
                color: theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.spacing_10

                Text {
                    id: label

                    Layout.fillWidth: implicitWidth > width
                    Layout.maximumWidth: implicitWidth
                    text: model.label
                    elide: Text.ElideRight
                    color: theme.fg.secondary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily

                    // the value differs from its default
                    Rectangle {
                        visible: root.changed
                        anchors.right: parent.left
                        anchors.rightMargin: VLCStyle.spacing_8
                        anchors.verticalCenter: parent.verticalCenter
                        width: VLCStyle.dp(6, VLCStyle.scale)
                        height: width
                        radius: width / 2
                        color: theme.accent

                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTr("Changed")
                    }
                }

                Widgets.IconToolButton {
                    id: infoButton

                    visible: model.help !== ""
                    text: VLCIcons.info
                    font.pixelSize: VLCStyle.dp(18, VLCStyle.scale)
                    description: qsTr("About “%1”").arg(model.label)
                    checked: root.helpShown

                    onClicked: root.helpShown = !root.helpShown
                }

                Rectangle {
                    visible: model.allProfiles
                    implicitWidth: profilesTag.implicitWidth + 2 * VLCStyle.spacing_6
                    implicitHeight: profilesTag.implicitHeight + VLCStyle.spacing_4
                    radius: VLCStyle.radius_small
                    color: theme.bg.highlight

                    Text {
                        id: profilesTag
                        anchors.centerIn: parent
                        text: qsTr("All profiles")
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                        font.family: VLCStyle.fontFamily
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Text {
                visible: model.effect !== SettingsModel.Immediate
                Layout.fillWidth: true
                text: model.effect === SettingsModel.Restart ? qsTr("Applies after restart")
                                                              : qsTr("Applies to the next file")
                color: model.pendingRestart ? theme.accent : theme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_small
                font.family: VLCStyle.fontFamily
            }

            // help opens on a click on the info button, not on hover
            Rectangle {
                visible: root.helpShown
                Layout.fillWidth: true
                Layout.maximumWidth: VLCStyle.dp(440, VLCStyle.scale)
                Layout.topMargin: VLCStyle.spacing_4
                implicitHeight: helpText.implicitHeight + 2 * VLCStyle.spacing_8
                radius: VLCStyle.radius_normal
                color: theme.bg.secondary

                Text {
                    id: helpText

                    anchors.fill: parent
                    anchors.margins: VLCStyle.spacing_8
                    text: model.help
                    wrapMode: Text.WordWrap
                    color: theme.fg.primary
                    font.pixelSize: VLCStyle.fontSize_normal
                    font.family: VLCStyle.fontFamily
                }
            }
        }

        Widgets.TextToolButton {
            visible: root.changed
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Reset")
            Accessible.name: qsTr("Reset “%1”").arg(model.label)

            onClicked: root.settingsModel.reset(model.settingId)
        }

        Loader {
            id: controlLoader

            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            Layout.preferredWidth: model.control === SettingsModel.Toggle
                                   ? implicitWidth : VLCStyle.dp(320, VLCStyle.scale)
            Layout.preferredHeight: implicitHeight

            active: root.isSetting
            focus: root.isSetting

            sourceComponent: {
                switch (model.control) {
                case SettingsModel.Toggle: return toggleComponent
                case SettingsModel.Choice: return choiceComponent
                case SettingsModel.Segmented:
                case SettingsModel.Swatches: return segmentedComponent
                case SettingsModel.Slider: return sliderComponent
                case SettingsModel.Text: return textComponent
                case SettingsModel.Password: return passwordComponent
                case SettingsModel.Number: return numberComponent
                case SettingsModel.Color: return colorComponent
                case SettingsModel.Shortcut: return shortcutComponent
                case SettingsModel.Action: return actionComponent
                case SettingsModel.OpenFile:
                case SettingsModel.Directory:
                case SettingsModel.SaveFile: return fileComponent
                }
                return null
            }
        }
    }

    // the controls below have a `model` of their own: they read the row
    // through `root.row`
    readonly property var row: model

    Component {
        id: toggleComponent

        SettingsToggle {
            checked: !!root.row.value
            Accessible.name: root.row.label
            onClicked: root.write(!root.row.value)
        }
    }

    Component {
        id: choiceComponent

        Widgets.ComboBoxExt {
            id: combo

            model: root.row.choices
            textRole: "label"
            valueRole: "value"

            // follows the setting, also after the user picks a choice
            Binding on currentIndex {
                value: (root.row.choices || []).findIndex((choice) => choice.value === root.row.value)
            }

            Accessible.name: root.row.label

            font.pixelSize: VLCStyle.fontSize_normal
            leftPadding: VLCStyle.spacing_14
            rightPadding: VLCStyle.spacing_10
            implicitHeight: VLCStyle.dp(36, VLCStyle.scale)

            color: theme.fg.secondary

            contentItem: Text {
                rightPadding: combo.spacing + combo.indicator.width
                text: combo.displayText
                color: combo.color
                font.pixelSize: VLCStyle.fontSize_normal
                font.family: VLCStyle.fontFamily
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: VLCStyle.radius_normal
                color: combo.bgColor
                border.width: combo.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                border.color: theme.accent

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: VLCStyle.dp(2, VLCStyle.scale)
                    radius: VLCStyle.radius_small
                    color: combo.hovered ? theme.bg.highlight : combo.colorContext.bg.secondary
                }
            }

            onActivated: (index) => root.write(combo.valueAt(index))
        }
    }

    Component {
        id: segmentedComponent

        SettingsSegmented {
            choices: root.row.choices
            value: root.row.value
            accessibleName: root.row.label
            onActivated: (value) => root.write(value)
        }
    }

    Component {
        id: sliderComponent

        SettingsSlider {
            from: root.row.minimum
            to: root.row.maximum
            stepSize: root.row.step
            scale: root.row.scale
            unit: root.row.unit
            value: root.row.value
            accessibleName: root.row.label
            // the interface scale changes the Settings window too: apply it
            // when the slider is released
            liveUpdate: root.row.settingId !== "interface-scale"
            onMoved: (value) => root.write(value)
        }
    }

    Component {
        id: textComponent

        Widgets.TextFieldExt {
            text: root.row.value
            Accessible.name: root.row.label
            implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
            radius: VLCStyle.radius_normal
            onEditingFinished: root.write(text)
        }
    }

    Component {
        id: fileComponent

        RowLayout {
            spacing: VLCStyle.spacing_10

            Widgets.TextFieldExt {
                id: pathField

                Layout.fillWidth: true
                implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
                radius: VLCStyle.radius_normal
                text: root.row.value
                Accessible.name: root.row.label
                onEditingFinished: root.write(text)
            }

            Widgets.ButtonExt {
                id: browseButton

                Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                text: qsTr("Browse")
                font.weight: Font.DemiBold
                Accessible.name: qsTr("Browse for “%1”").arg(root.row.label)

                background: Rectangle {
                    radius: VLCStyle.radius_normal
                    color: browseButton.hovered || browseButton.visualFocus ? theme.bg.highlight
                                                                            : fieldTheme.bg.secondary
                    border.width: browseButton.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                    border.color: theme.accent
                }

                readonly property ColorContext fieldContext: ColorContext {
                    id: fieldTheme
                    colorSet: ColorContext.ComboBox
                }

                onClicked: {
                    let path
                    if (root.row.control === SettingsModel.OpenFile)
                        path = root.settingsModel.browseOpenFile(root.row.label, pathField.text)
                    else if (root.row.control === SettingsModel.Directory)
                        path = root.settingsModel.browseDirectory(root.row.label, pathField.text)
                    else
                        path = root.settingsModel.browseSaveFile(root.row.label, pathField.text)
                    if (path !== "")
                        root.write(path)
                }
            }
        }
    }

    Component {
        id: passwordComponent

        Widgets.TextFieldExt {
            text: root.row.value
            echoMode: TextInput.Password
            Accessible.name: root.row.label
            implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
            radius: VLCStyle.radius_normal
            onEditingFinished: root.write(text)
        }
    }

    // a number typed in; a step of 0 accepts decimals
    Component {
        id: numberComponent

        Widgets.TextFieldExt {
            readonly property bool decimals: root.row.step === 0

            // options store floats: hide the digits a float can't hold
            text: root.row.value === undefined ? ""
                  : decimals ? String(parseFloat(Number(root.row.value).toPrecision(7)))
                             : String(root.row.value)
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: decimals ? doubleValidator : intValidator
            Accessible.name: root.row.label
            implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
            radius: VLCStyle.radius_normal

            IntValidator {
                id: intValidator
                bottom: Math.max(root.row.minimum, -2147483647)
                top: Math.min(root.row.maximum, 2147483647)
            }

            DoubleValidator {
                id: doubleValidator
                // options without a range have min and max at 0
                bottom: root.row.minimum < root.row.maximum ? root.row.minimum : -Infinity
                top: root.row.minimum < root.row.maximum ? root.row.maximum : Infinity
                locale: "C"
                notation: DoubleValidator.StandardNotation
            }

            onEditingFinished: {
                const value = decimals ? parseFloat(text) : parseInt(text, 10)
                if (!isNaN(value))
                    root.write(value)
            }
        }
    }

    // an RGB color written #rrggbb
    Component {
        id: colorComponent

        RowLayout {
            spacing: VLCStyle.spacing_10

            function hex(value) {
                return "#" + ((value >>> 0) & 0xffffff).toString(16).padStart(6, "0")
            }

            Rectangle {
                Layout.preferredWidth: VLCStyle.dp(24, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(24, VLCStyle.scale)
                radius: VLCStyle.radius_small
                color: parent.hex(root.row.value)
                border.width: VLCStyle.dp(1, VLCStyle.scale)
                border.color: theme.border
            }

            Widgets.TextFieldExt {
                Layout.fillWidth: true
                text: parent.hex(root.row.value)
                validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{6}/ }
                Accessible.name: root.row.label
                implicitHeight: VLCStyle.dp(36, VLCStyle.scale)
                radius: VLCStyle.radius_normal
                onEditingFinished: root.write(parseInt(text.replace("#", ""), 16))
            }
        }
    }

    // the keys of a shortcut action: opens Keyboard Shortcuts on it
    Component {
        id: shortcutComponent

        Widgets.ButtonExt {
            id: shortcutButton

            // "key-play-pause", "app-key-open-folder"
            readonly property string actionName: root.row.settingId.replace(/^(app-)?key-/, "")
            readonly property string keys: {
                const keys = (root.row.value || "").split("\t").filter((key) => key !== "")
                // letters as printed on the keyboard: "Ctrl+Q"
                return keys.map((key) => {
                    const start = key.lastIndexOf("+", key.length - 2) + 1
                    return key.length - start === 1 ? key.substring(0, start) + key.substring(start).toUpperCase()
                                                    : key
                }).join(", ")
            }

            implicitHeight: VLCStyle.dp(32, VLCStyle.scale)
            text: keys !== "" ? keys : qsTr("None")
            font.family: VLCStyle.fontFamily_mono
            Accessible.name: qsTr("%1: %2, change the key").arg(root.row.label).arg(text)

            background: Rectangle {
                radius: VLCStyle.radius_normal
                color: shortcutButton.hovered || shortcutButton.visualFocus ? theme.bg.highlight
                                                                            : shortcutTheme.bg.primary
                border.width: shortcutButton.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                border.color: theme.accent
            }

            readonly property ColorContext shortcutContext: ColorContext {
                id: shortcutTheme
                colorSet: ColorContext.ComboBox
            }

            onClicked: DialogsProvider.keyboardShortcutsDialog(shortcutButton.actionName)
        }
    }

    // a button, asking for confirmation first when the row has a
    // confirmText
    Component {
        id: actionComponent

        RowLayout {
            id: action

            property bool confirming: false

            spacing: VLCStyle.spacing_10

            Widgets.TextToolButton {
                visible: action.confirming
                text: qsTr("Cancel")
                onClicked: action.confirming = false
            }

            Widgets.ButtonExt {
                id: actionButton

                Layout.fillWidth: true
                Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                text: action.confirming ? root.row.confirmText : root.row.buttonText
                font.weight: Font.DemiBold
                Accessible.name: action.confirming ? qsTr("Confirm “%1”").arg(root.row.label)
                                                   : qsTr("%1: %2").arg(root.row.label).arg(root.row.buttonText)

                background: Rectangle {
                    radius: VLCStyle.radius_normal
                    color: action.confirming ? theme.accent
                                             : (actionButton.hovered || actionButton.visualFocus ? theme.bg.highlight
                                                                                                 : theme.bg.secondary)
                    border.width: actionButton.visualFocus ? VLCStyle.dp(2, VLCStyle.scale) : 0
                    border.color: theme.accent
                }

                onClicked: {
                    if (!action.confirming && root.row.confirmText !== "") {
                        action.confirming = true
                        return
                    }
                    action.confirming = false
                    root.settingsModel.trigger(root.row.settingId)
                }
            }
        }
    }
}

