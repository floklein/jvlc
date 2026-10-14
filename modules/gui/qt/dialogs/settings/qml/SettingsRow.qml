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

    readonly property bool changed: !model.isDefault
    property bool helpShown: false

    implicitHeight: content.implicitHeight + 2 * VLCStyle.spacing_5

    function write(value) {
        model.value = value
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    RowLayout {
        id: content

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

            focus: true

            sourceComponent: {
                switch (model.control) {
                case SettingsModel.Toggle: return toggleComponent
                case SettingsModel.Choice: return choiceComponent
                case SettingsModel.Segmented:
                case SettingsModel.Swatches: return segmentedComponent
                case SettingsModel.Slider: return sliderComponent
                case SettingsModel.Text: return textComponent
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
                    const path = root.settingsModel.browseSaveFile(root.row.label, pathField.text)
                    if (path !== "")
                        root.write(path)
                }
            }
        }
    }
}

