/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

Item {
    id: root

    //---------------------------------------------------------------------------------------------
    // Properties
    //---------------------------------------------------------------------------------------------

    property Item bgContent: null

    //---------------------------------------------------------------------------------------------
    // Signal
    //---------------------------------------------------------------------------------------------

    signal restoreFocus();

    //---------------------------------------------------------------------------------------------
    // Events
    //---------------------------------------------------------------------------------------------

    Component.onCompleted: {
        // an error happened while the other view was shown
        if (DialogErrorModel.repeatedMessageCount && DialogErrorModel.noticesEnabled())
            errorNotice.show()
    }

    Component.onDestruction: {
        if (questionDialog.dialogId !== null) {
            vlcDialog.dismiss(questionDialog.dialogId)
            questionDialog.dialogId = null
        } if (loginDialog.dialogId !== null) {
            vlcDialog.dismiss(loginDialog.dialogId)
            loginDialog.dialogId = null
        }
    }

    //---------------------------------------------------------------------------------------------
    // Functions
    //---------------------------------------------------------------------------------------------

    function ask(text, acceptCb, rejectCb, buttons) {
        customDialog.ask(text, acceptCb, rejectCb, buttons)
    }

    //---------------------------------------------------------------------------------------------
    // Connections
    //---------------------------------------------------------------------------------------------

    VLCDialog
    {
        id: vlcDialog

        Component.onCompleted: {
            VLCDialogModel.provider = vlcDialog
        }

        Component.onDestruction: {
            // Only clear the provider if we are still the current one.
            if (VLCDialogModel.provider === vlcDialog)
                VLCDialogModel.provider = null
        }

        onLogin: (dialogId, title, text, defaultUsername, askStore) => {
            loginDialog.dialogId = dialogId
            loginDialog.title = title
            loginDialog.text = text
            loginDialog.defaultUsername = defaultUsername
            loginDialog.askStore = askStore
            loginDialog.open()
        }

        onQuestion: (dialogId, title, text, type, cancel, action1, action2) => {
            questionDialog.dialogId = dialogId
            questionDialog.title = title
            questionDialog.text = text
            questionDialog.cancelTxt = cancel
            questionDialog.action1Txt = action1
            questionDialog.action2Txt = action2
            questionDialog.open()
        }

        onProgress: (dialogId, title, text, indeterminate, position, cancel) => {
            progressDialog.dialogId = dialogId
            progressDialog.title = title
            progressDialog.text = text
            progressDialog.cancelTxt = cancel
            progressDialog.position = position
            progressDialog.interderminate = indeterminate
            progressDialog.open()
        }

        onProgressUpdated: (dialogId, position, text) => {
            if (progressDialog.dialogId !== dialogId) {
                console.warn("progress event on an inexisting dialog")
                return
            }
            progressDialog.text = text
            progressDialog.position = position
        }

        onCancelled: (dialogId) => {
            if (questionDialog.dialogId === dialogId) {
                questionDialog.close()
                questionDialog.dialogId = null
                dismiss(dialogId)
            } else if (loginDialog.dialogId === dialogId)  {
                loginDialog.close()
                loginDialog.dialogId = null
                dismiss(dialogId)
            } else if (progressDialog.dialogId === dialogId) {
                progressDialog.close()
                progressDialog.dialogId = null
                dismiss(dialogId)
            } else {
                dismiss(dialogId)
            }
        }
    }

    Connections
    {
        target: DialogErrorModel

        function onCountChanged() {
            if (DialogErrorModel.noticesEnabled())
                errorNotice.show()
        }
    }

    //---------------------------------------------------------------------------------------------
    // Childs
    //---------------------------------------------------------------------------------------------

    ErrorNotice {
        id: errorNotice

        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: VLCStyle.margin_normal
        }

        width: Math.min(implicitWidth, root.width - VLCStyle.margin_normal * 2)
        z: 10
    }

    // VLC's own prompts stay modal

    ModalDialog {
        id: loginDialog

        //use var here as DialogId is a QGadget and passed by value
        property var dialogId: null
        property string defaultUsername: ""
        property string text: ""
        property bool askStore: false

        onAboutToHide: restoreFocus()
        rootWindow: root.bgContent

        contentItem: GridLayout {
            columns: 2
            columnSpacing: VLCStyle.margin_small
            rowSpacing: VLCStyle.margin_small

            readonly property ColorContext colorContext: ColorContext {
                id: loginContentTheme
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            T.Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.maximumWidth: VLCStyle.dp(400, VLCStyle.scale)

                visible: text.length > 0
                text: loginDialog.text
                color: loginContentTheme.fg.secondary
                font.pixelSize: VLCStyle.fontSize_normal
                wrapMode: Text.WordWrap
            }

            T.Label {
                text: qsTr("User")
                color: loginContentTheme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.TextFieldExt {
                id: username

                focus: true
                text: loginDialog.defaultUsername

                Layout.fillWidth: true
                Layout.minimumWidth: VLCStyle.dp(240, VLCStyle.scale)

                Navigation.downItem: password
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)
            }

            T.Label {
                text: qsTr("Password")
                color: loginContentTheme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }

            Widgets.TextFieldExt {
                id: password

                echoMode: TextInput.Password
                Layout.fillWidth: true

                Navigation.upItem: username
                Navigation.downItem: savePassword.visible ? savePassword : loginOk
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                Keys.onReturnPressed: loginOk.clicked()
                Keys.onEnterPressed: loginOk.clicked()
            }

            Item {
                visible: savePassword.visible
                implicitWidth: 1
            }

            Widgets.CheckBoxExt {
                id: savePassword

                visible: loginDialog.askStore
                text: qsTr("Save password")

                Navigation.upItem: password
                Navigation.downItem: loginOk
                Keys.priority: Keys.AfterItem
                Keys.onPressed: (event) => Navigation.defaultKeyAction(event)
            }
        }

        footer: FocusScope {
            implicitHeight: loginButtons.implicitHeight + VLCStyle.margin_normal

            RowLayout {
                id: loginButtons

                anchors.right: parent.right
                anchors.rightMargin: VLCStyle.margin_normal
                anchors.top: parent.top

                spacing: VLCStyle.margin_xsmall

                Widgets.ButtonExt {
                    id: loginCancel
                    text: qsTr("Cancel")

                    Navigation.upItem: savePassword.visible ? savePassword : password
                    Navigation.rightItem: loginOk
                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                    onClicked: {
                        loginDialog.reject()
                        loginDialog.close()
                    }
                }

                Widgets.ActionButtonPrimary {
                    id: loginOk
                    text: qsTr("Log In")

                    Navigation.upItem: savePassword.visible ? savePassword : password
                    Navigation.leftItem: loginCancel
                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                    onClicked: {
                        loginDialog.accept()
                        loginDialog.close()
                    }
                }
            }
        }

        onAccepted: {
            if (loginDialog.dialogId !== null) {
                vlcDialog.post_login(loginDialog.dialogId, username.text, password.text, savePassword.checked)
                loginDialog.dialogId = null
            }
            password.text = ""
        }
        onRejected: {
            if (loginDialog.dialogId !== null) {
                vlcDialog.dismiss(loginDialog.dialogId)
                loginDialog.dialogId = null
            }
            password.text = ""
        }
    }

    ModalDialog {
        id: progressDialog

        property var dialogId: null

        property string text : ""
        property string cancelTxt: ""
        property bool interderminate: false
        property real position: 0.0

        onAboutToHide: restoreFocus()
        rootWindow: root.bgContent

        contentItem: ColumnLayout {
            spacing: VLCStyle.margin_small

            readonly property ColorContext colorContext: ColorContext {
                id: progressContentTheme
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            T.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: VLCStyle.dp(400, VLCStyle.scale)

                focus: false
                font.pixelSize: VLCStyle.fontSize_normal
                color: progressContentTheme.fg.primary
                text: progressDialog.text
                wrapMode: Text.WordWrap
            }

            Widgets.ProgressBarExt {
                Layout.fillWidth:true
                Layout.minimumWidth: VLCStyle.dp(280, VLCStyle.scale)

                from: 0.0
                to: 1.0

                indeterminate: progressDialog.interderminate
                value: progressDialog.position
            }
        }

        footer: FocusScope {
            focus: true
            visible: progressDialog.cancelTxt !== ""
            implicitHeight: progressCancel.implicitHeight + VLCStyle.margin_normal

            Widgets.ButtonExt {
                id: progressCancel

                anchors.right: parent.right
                anchors.rightMargin: VLCStyle.margin_normal
                anchors.top: parent.top

                focus: true
                text: progressDialog.cancelTxt

                onClicked: {
                    vlcDialog.dismiss(progressDialog.dialogId)
                    progressDialog.dialogId = null
                    progressDialog.close()
                }
            }
        }
    }

    ModalDialog {
        id: questionDialog

        //use var here as DialogId is a QGadget and passed by value
        property var dialogId: null
        property alias text: content.text
        property alias cancelTxt: cancel.text
        property alias action1Txt: action1.text
        property alias action2Txt: action2.text

        rootWindow: root.bgContent

        onAboutToHide: restoreFocus()

        contentItem: T.Label {
            id: content
            focus: false
            font.pixelSize: VLCStyle.fontSize_normal
            color: questionDialog.colorContext.fg.primary
            wrapMode: Text.WordWrap
        }

        footer: FocusScope {
            focus: true
            implicitHeight: questionButtons.implicitHeight + VLCStyle.margin_normal

            RowLayout {
                id: questionButtons

                anchors.right: parent.right
                anchors.rightMargin: VLCStyle.margin_normal
                anchors.top: parent.top

                spacing: VLCStyle.margin_xsmall

                Widgets.ButtonExt {
                    id: cancel

                    focus: true
                    visible: cancel.text !== ""

                    Navigation.rightItem: action2.visible ? action2 : action1
                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                    onClicked: {
                        vlcDialog.dismiss(questionDialog.dialogId)
                        questionDialog.dialogId = null
                        questionDialog.close()
                    }
                }

                Widgets.ButtonExt {
                    id: action2
                    visible: action2.text !== ""

                    Navigation.leftItem: cancel
                    Navigation.rightItem: action1
                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                    onClicked: {
                        vlcDialog.post_action2(questionDialog.dialogId)
                        questionDialog.dialogId = null
                        questionDialog.close()
                    }
                }

                // the first action is the main one
                Widgets.ActionButtonPrimary {
                    id: action1

                    visible: action1.text !== ""

                    Navigation.leftItem: action2.visible ? action2 : cancel
                    Keys.priority: Keys.AfterItem
                    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

                    onClicked: {
                        vlcDialog.post_action1(questionDialog.dialogId)
                        questionDialog.dialogId = null
                        questionDialog.close()
                    }
                }
            }
        }
    }

    CustomDialog {
        id: customDialog
        rootWindow: root.bgContent
        onAboutToHide: restoreFocus()
    }

    Loader {
        id: toolbarEditorDialogLoader
        active: false
        source: "qrc:///qt/qml/VLC/Dialogs/ToolbarEditorDialog.qml"

        Connections {
            target: DialogsProvider

            function onShowToolbarEditorDialog() {
                toolbarEditorDialogLoader.active = true
                toolbarEditorDialogLoader.item.open()
            }
        }
    }

    Loader {
        id: openDiscDialogLoader
        active: false

        source: "qrc:///qt/qml/VLC/Dialogs/OpenDiscDialog.qml"

        onLoaded: {
            item.rootWindow = Qt.binding(() => root.bgContent)
            item.aboutToHide.connect(root.restoreFocus)
        }

        Connections {
            target: DialogsProvider

            function onShowOpenDiscDialog() {
                openDiscDialogLoader.active = true
                // a popup can't open in the event loop iteration that creates it
                Qt.callLater(() => openDiscDialogLoader.item?.open())
            }
        }
    }

    // created on first use, then kept: it knows which changes need a restart
    Loader {
        id: settingsDialogLoader
        active: false
        source: "qrc:///qt/qml/VLC/Dialogs/SettingsDialog.qml"

        Connections {
            target: DialogsProvider

            function onShowSettingsDialog(page, advanced) {
                settingsDialogLoader.active = true
                settingsDialogLoader.item.openPage(page, advanced)
            }

            function onHideSettingsDialog() {
                if (settingsDialogLoader.item)
                    settingsDialogLoader.item.close()
            }
        }
    }
}

