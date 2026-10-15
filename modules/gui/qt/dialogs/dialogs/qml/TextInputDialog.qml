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
import VLC.Widgets as Widgets
import VLC.Style

// Asks for one line of text, such as a name or a link:
// ask(title, label, text, onAccepted)
ModalDialog {
    id: root

    property alias label: fieldLabel.text
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property string acceptText: qsTr("OK")
    // whether an empty text can be accepted
    property bool acceptEmpty: false
    // optional check of the text: function (text) returning why it can't
    // be accepted, or "" when it can
    property var validate: null

    readonly property string _error: (validate && field.text.trim() !== "") ? validate(field.text) : ""
    readonly property bool _acceptable: (acceptEmpty || field.text.trim() !== "") && _error === ""

    property var _onAccepted: null

    signal textAccepted(string text)

    function ask(title, label, text, onAccepted) {
        root.title = title
        root.label = label
        root.text = text ?? ""
        root._onAccepted = onAccepted ?? null
        root.open()
    }

    function _accept() {
        if (!_acceptable)
            return
        const callback = root._onAccepted
        root._onAccepted = null
        root.close()
        root.textAccepted(field.text)
        if (callback)
            callback(field.text)
    }

    width: Math.min(VLCStyle.dp(480, VLCStyle.scale),
                    (T.Overlay.overlay?.width ?? Infinity) - 2 * VLCStyle.margin_large)

    padding: VLCStyle.spacing_20
    spacing: 0

    onOpened: {
        field.forceActiveFocus(Qt.TabFocusReason)
        field.selectAll()
    }

    background: Rectangle {
        color: root.colorContext.bg.primary
        radius: VLCStyle.radius_xlarge

        Widgets.RoundedRectangleShadow {
            yOffset: VLCStyle.dp(8, VLCStyle.scale)
            blurRadius: VLCStyle.dp(15, VLCStyle.scale)
            radius: parent.radius
            opacity: 0.46
            color: root.colorContext.shadow.alpha(1.0)
        }
    }

    header: Rectangle {
        id: headerBar

        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.View
        }

        implicitHeight: VLCStyle.dp(50, VLCStyle.scale)

        // only the top corners are rounded
        radius: VLCStyle.radius_xlarge
        color: headerBar.colorContext.bg.secondary

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.radius
            color: parent.color
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: VLCStyle.spacing_8
            anchors.leftMargin: VLCStyle.spacing_20

            T.Label {
                text: root.title
                color: headerBar.colorContext.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Widgets.IconToolButton {
                text: VLCIcons.close
                description: qsTr("Close")
                font.pixelSize: VLCStyle.icon_normal
                Layout.preferredWidth: VLCStyle.dp(32, VLCStyle.scale)
                Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
                focusPolicy: Qt.NoFocus
                onClicked: root.close()
            }
        }
    }

    contentItem: ColumnLayout {
        // popup items don't inherit the window palette
        readonly property ColorContext colorContext: ColorContext {
            id: contentTheme
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        spacing: VLCStyle.spacing_10

        T.Label {
            id: fieldLabel
            visible: text !== ""
            color: contentTheme.fg.primary
            font.pixelSize: VLCStyle.fontSize_large
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Widgets.TextFieldExt {
            id: field

            radius: VLCStyle.radius_small
            selectByMouse: true

            Layout.fillWidth: true
            Layout.preferredHeight: VLCStyle.dp(36, VLCStyle.scale)

            Accessible.name: fieldLabel.text || root.title

            onAccepted: root._accept()
        }

        T.Label {
            visible: text !== ""
            text: root._error
            color: contentTheme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
            wrapMode: Text.Wrap
            Layout.fillWidth: true

            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
    }

    footer: RowLayout {
        spacing: VLCStyle.spacing_10

        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        Widgets.ButtonExt {
            text: qsTr("Cancel")
            colorContext.colorSet: ColorContext.Window
            radius: VLCStyle.radius_normal
            Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
            Layout.margins: VLCStyle.spacing_20
            onClicked: root.close()
        }

        Item { Layout.fillWidth: true }

        Widgets.ButtonExt {
            text: root.acceptText
            enabled: root._acceptable
            colorContext.colorSet: ColorContext.ButtonAccent
            radius: VLCStyle.radius_normal
            Layout.preferredWidth: VLCStyle.dp(100, VLCStyle.scale)
            Layout.preferredHeight: VLCStyle.dp(32, VLCStyle.scale)
            Layout.margins: VLCStyle.spacing_20
            onClicked: root._accept()
        }
    }
}
