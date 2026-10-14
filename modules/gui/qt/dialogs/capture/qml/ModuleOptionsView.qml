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
import VLC.Dialogs
import VLC.Widgets as Widgets
import VLC.Style

// A form over a ModuleOptionsModel: one row per option, grouped by section.
// A Repeater rather than a ListView, so that Tab reaches every field.
ColumnLayout {
    id: root

    // ModuleOptionsModel
    required property var options

    spacing: VLCStyle.margin_small

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Repeater {
        model: root.options

        delegate: ColumnLayout {
            id: row

            required property int index
            required property string text
            required property string description
            required property string section
            required property int type
            required property var value
            required property var minimum
            required property var maximum
            required property var choices
            required property bool modified

            Layout.fillWidth: true
            spacing: VLCStyle.margin_small

            function apply(newValue) {
                root.options.setValue(row.index, newValue)
            }

            // the first option of a section shows its title
            Widgets.SubtitleLabel {
                Layout.fillWidth: true

                visible: row.section.length > 0
                         && (row.index === 0
                             || root.options.data(root.options.index(row.index - 1, 0),
                                                  ModuleOptionsModel.SectionRole) !== row.section)
                topPadding: VLCStyle.margin_small

                text: row.section
                font.pixelSize: VLCStyle.fontSize_large
                color: theme.fg.primary
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: VLCStyle.margin_normal

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: VLCStyle.margin_xxxsmall

                    Widgets.LabelExt {
                        Layout.fillWidth: true

                        text: row.text + (row.modified ? " *" : "")
                        wrapMode: Text.Wrap
                        color: theme.fg.primary
                        font.pixelSize: VLCStyle.fontSize_normal
                    }

                    Widgets.CaptionLabel {
                        Layout.fillWidth: true

                        visible: row.description.length > 0 && row.description !== row.text
                        text: row.description
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        maximumLineCount: 3
                        color: theme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                }

                Loader {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignVCenter

                    sourceComponent: {
                        if (row.type === ModuleOptionsModel.Bool)
                            return boolEditor
                        if (row.choices.length > 0)
                            return choiceEditor
                        if (row.type === ModuleOptionsModel.Integer)
                            return integerEditor
                        return textEditor
                    }
                }
            }

            Component {
                id: boolEditor

                Widgets.CheckBoxExt {
                    checked: row.value
                    onToggled: row.apply(checked)
                    Accessible.name: row.text
                }
            }

            Component {
                id: choiceEditor

                Widgets.ComboBoxExt {
                    model: row.choices
                    textRole: "text"
                    valueRole: "value"
                    // unlike indexOfValue(), this follows the value
                    currentIndex: {
                        for (let i = 0; i < row.choices.length; ++i) {
                            if (row.choices[i].value == row.value)
                                return i
                        }
                        return -1
                    }
                    onActivated: row.apply(currentValue)
                    Accessible.name: row.text
                }
            }

            Component {
                id: integerEditor

                Widgets.SpinBoxExt {
                    // SpinBox values are 32-bit
                    from: Math.max(row.minimum, -2147483647)
                    to: Math.min(row.maximum, 2147483647)
                    value: row.value
                    editable: true
                    onValueModified: row.apply(value)
                    Accessible.name: row.text
                }
            }

            Component {
                id: textEditor

                Widgets.TextFieldExt {
                    text: row.value
                    validator: row.type === ModuleOptionsModel.Float ? floatValidator : null
                    onEditingFinished: row.apply(row.type === ModuleOptionsModel.Float ? Number(text) : text)
                    onActiveFocusChanged: MainCtx.useGlobalShortcuts = !activeFocus
                    Accessible.name: row.text

                    DoubleValidator {
                        id: floatValidator
                        locale: "C"
                        bottom: row.minimum !== undefined ? row.minimum : -Infinity
                        top: row.maximum !== undefined ? row.maximum : Infinity
                    }
                }
            }
        }
    }
}
