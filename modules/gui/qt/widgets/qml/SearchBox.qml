/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Controls.Basic as Basic


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets

FocusScope {
    id: root

    // the field shown in place, rather than a key that opens it below
    // (Figma 6261:15136)
    property bool inline: false

    implicitWidth: inline ? VLCStyle.widthSearchInput : iconButton.implicitWidth
    implicitHeight: inline ? VLCStyle.pageHeader_rowHeight : iconButton.implicitHeight

    property alias popup: popup
    property real maxSearchFieldWidth: Number.MAX_VALUE
    property alias buttonWidth: iconButton.implicitWidth
    property string searchPattern
    property alias regexButtonToggled: regexToggleButton.checked
    property alias caseSensitiveButtonToggled: caseSensitiveToggleButton.checked
    property alias compressSearchPatternChanges: searchPatternBinding.delayed
    compressSearchPatternChanges: true
    property alias displayRegexToggleButton: regexToggleButton.visible
    displayRegexToggleButton: false
    property alias displayCaseSensitiveToggleButton: caseSensitiveToggleButton.visible
    displayCaseSensitiveToggleButton: false

    Binding on searchPattern {
        id: searchPatternBinding
        value: root.inline ? inlineField.text : textField.text
    }

    property bool popBelow: true
    property alias toggleButton: iconButton
    property alias textField: textField
    readonly property bool expanded: inline || popup.visible

    // public functions

    function expandAndFocus() {
        if (root.inline) {
            inlineField.forceActiveFocus(Qt.ShortcutFocusReason)
            return
        }
        popup.open()
        textField.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    function retract() {
        if (root.inline) {
            inlineField.clear()
            return
        }
        popup.close()
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
    }

    ColorContext {
        id: navTheme
        colorSet: ColorContext.Navigation
    }

    ColorContext {
        id: placeholderTheme
        colorSet: ColorContext.Navigation
        enabled: false
    }

    // Figma 6261:15136: BG/A5, radius 6, 8 px and 5 px inside, the search
    // icon (FG/A3), then "Search" (FG/A4) 10 px after it
    Rectangle {
        id: inlineBox

        anchors.fill: parent

        visible: root.inline

        radius: VLCStyle.radius_normal
        color: navTheme.bg.highlight

        border.width: inlineField.activeFocus ? VLCStyle.focus_border : 0
        border.color: theme.visualFocus

        Widgets.IconLabel {
            id: inlineIcon

            anchors.left: parent.left
            anchors.leftMargin: VLCStyle.margin_xsmall
            anchors.verticalCenter: parent.verticalCenter

            text: VLCIcons.search
            color: navTheme.indicator
            font.pixelSize: VLCStyle.icon_normal
        }

        Basic.TextField {
            id: inlineField

            anchors.left: inlineIcon.right
            anchors.leftMargin: VLCStyle.pageHeader_historySpacing
            anchors.right: inlineClear.visible ? inlineClear.left : parent.right
            anchors.rightMargin: VLCStyle.margin_xsmall
            anchors.verticalCenter: parent.verticalCenter

            focus: root.inline

            padding: 0
            leftPadding: 0
            rightPadding: 0
            verticalAlignment: Text.AlignVCenter

            font.pixelSize: VLCStyle.fontSize_normal
            color: theme.fg.primary
            selectionColor: theme.bg.highlight
            selectedTextColor: theme.fg.highlight

            placeholderText: qsTr("Search")
            placeholderTextColor: placeholderTheme.fg.primary

            selectByMouse: true

            // the field is the box around it
            background: null

            Accessible.searchEdit: true
            Accessible.name: qsTr("Search")

            property bool _keyPressed: false

            Navigation.parentItem: root
            Navigation.rightItem: inlineClear.visible ? inlineClear : null
            Navigation.cancelAction: function() {
                if (inlineField.text.length > 0)
                    inlineField.clear()
                else
                    root.Navigation.defaultNavigationUp()
            }

            onActiveFocusChanged: MainCtx.useGlobalShortcuts = !activeFocus

            Keys.priority: Keys.AfterItem

            Keys.onPressed: (event) => {
                _keyPressed = true

                //we don't want Navigation.cancelAction to match Backspace
                if (event.matches(StandardKey.Backspace))
                    event.accepted = true

                Navigation.defaultKeyAction(event)
            }

            Keys.onReleased: (event) => {
                if (_keyPressed === false)
                    return

                _keyPressed = false

                if (event.matches(StandardKey.Backspace))
                    event.accepted = true

                Navigation.defaultKeyReleaseAction(event)
            }
        }

        Widgets.IconToolButton {
            id: inlineClear

            anchors.right: parent.right
            anchors.rightMargin: VLCStyle.margin_xsmall
            anchors.verticalCenter: parent.verticalCenter

            visible: inlineField.text.length > 0

            font.pixelSize: VLCStyle.icon_normal
            text: VLCIcons.close
            description: qsTr("Clear")

            onClicked: {
                inlineField.clear()
                inlineField.forceActiveFocus(Qt.MouseFocusReason)
            }

            Navigation.parentItem: root
            Navigation.leftItem: inlineField
        }
    }

    Widgets.IconToolButton {
        id: iconButton

        anchors.fill: parent

        visible: !root.inline

        font.pixelSize: VLCStyle.icon_banner

        text: VLCIcons.search
        description: qsTr("Filter")

        focus: !root.inline
        checked: popup.visible
        color: VLCStyle.pageToolColor(this)

        Navigation.parentItem: root
        Navigation.downItem: root.popBelow ? textField : null
        Navigation.upItem: root.popBelow ? null : textField

        onClicked: {
            if (!popup.visible)
                expandAndFocus()
            else
                root.retract()
        }

        T.Popup {
            id: popup

            x: -width + parent.width
            y: root.popBelow ? parent.height : -height

            implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding)
            implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                     implicitContentHeight + topPadding + bottomPadding)

            closePolicy: T.Popup.NoAutoClose

            Binding on closePolicy {
                when: textField.text.length === 0
                value: T.Popup.CloseOnPressOutside
            }

            height: 0

            onOpened: {
                textField.forceActiveFocus(iconButton.focusReason)
            }

            onClosed: {
                iconButton.forceActiveFocus(iconButton.focusReason)
            }

            onAboutToHide: {
                textField.text = ""
            }

            component EnterExitTransition : Transition {
                id: transition

                property alias to: animation.to

                NumberAnimation {
                    id: animation

                    property: "height"
                    easing.type: Easing.InOutSine
                    duration: VLCStyle.duration_long
                }

                onRunningChanged: {
                    if (running)
                        textField.clip = true
                    else
                        textField.clip = false
                }
            }

            enter: EnterExitTransition {
                // NOTE: It is intentional that `from` is not used.
                to: popup.implicitHeight
            }

            exit: EnterExitTransition {
                // NOTE: It is intentional that `from` is not used.
                to: 0.0
            }

            contentItem: TextFieldExt {
                id: textField

                property bool _keyPressed: false

                colorContext.palette: theme.palette

                implicitWidth: VLCStyle.widthSearchInput

                padding: VLCStyle.dp(6, VLCStyle.scale)
                leftPadding: padding + VLCStyle.dp(4, VLCStyle.scale)
                rightPadding: leftPadding + (insideRow.visible ? insideRow.width : 0.0)

                radius: clearButton.radius

                selectByMouse: true

                placeholderText: qsTr("filter")

                Navigation.parentItem: root
                Navigation.upItem: root.popBelow ? iconButton : null
                Navigation.downItem: root.popBelow ? null : iconButton
                Navigation.rightItem: caseSensitiveToggleButton.visible ? caseSensitiveToggleButton
                                                                        : (regexToggleButton.visible ? regexToggleButton
                                                                                                     : (clearButton.visible ? clearButton : null))
                Navigation.cancelAction: function() {
                    root.retract()
                    iconButton.focusReason = Qt.ShortcutFocusReason
                }

                Accessible.searchEdit: true

                //ideally we should use Keys.onShortcutOverride but it doesn't
                //work with TextField before 5.13 see QTBUG-68711
                onActiveFocusChanged: {
                    if (activeFocus)
                        MainCtx.useGlobalShortcuts = false
                    else
                        MainCtx.useGlobalShortcuts = true
                }

                Keys.priority: Keys.AfterItem

                Keys.onPressed: (event) => {
                    _keyPressed = true

                    //we don't want Navigation.cancelAction to match Backspace
                    if (event.matches(StandardKey.Backspace))
                        event.accepted = true

                    Navigation.defaultKeyAction(event)
                }

                Keys.onReleased: (event) => {
                    if (_keyPressed === false)
                        return

                    _keyPressed = false

                    //we don't want Navigation.cancelAction to match Backspace
                    if (event.matches(StandardKey.Backspace))
                        event.accepted = true

                    Navigation.defaultKeyReleaseAction(event)
                }

                Row {
                    id: insideRow

                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: VLCStyle.margin_xxsmall

                    spacing: VLCStyle.margin_xxsmall

                    visible: parent.height > height

                    Widgets.IconToolButton {
                        id: caseSensitiveToggleButton

                        anchors.verticalCenter: parent.verticalCenter

                        // Animating font properties with native rendering is probably not a good idea:
                        font.pixelSize: 0.6 * (contentItem.renderType === Text.QtRendering ? textField.height
                                                                                           : textField.implicitHeight)
                        text: VLCIcons.ic_fluent_text_change_case_24_regular

                        description: qsTr("Case sensitive")

                        onClicked: {
                            checked = !checked
                        }

                        Navigation.parentItem: textField
                        Navigation.leftItem: textField
                        Navigation.rightItem: regexToggleButton
                    }

                    Widgets.IconToolButton {
                        id: regexToggleButton

                        anchors.verticalCenter: parent.verticalCenter

                        // Animating font properties with native rendering is probably not a good idea:
                        font.pixelSize: 0.6 * (contentItem.renderType === Text.QtRendering ? textField.height
                                                                                           : textField.implicitHeight)
                        text: VLCIcons.regex

                        description: qsTr("Use regular expression")

                        onClicked: {
                            checked = !checked
                        }

                        Navigation.parentItem: textField
                        Navigation.leftItem: caseSensitiveToggleButton
                        Navigation.rightItem: clearButton
                    }

                    Widgets.IconToolButton {
                        id: clearButton

                        anchors.verticalCenter: parent.verticalCenter

                        // Animating font properties with native rendering is probably not a good idea:
                        font.pixelSize: 0.6 * (contentItem.renderType === Text.QtRendering ? textField.height
                                                                                           : textField.implicitHeight)
                        text: VLCIcons.close

                        description: qsTr("Clear")

                        visible: (textField.text.length > 0)

                        onVisibleChanged: {
                            if (activeFocus && !visible && parent.visible) {
                                textField.focus = true
                                textField.focusReason = focusReason
                            }
                        }
                        onClicked: textField.clear()

                        Navigation.parentItem: textField
                        Navigation.leftItem: regexToggleButton
                    }
                }
            }
        }
    }
}
