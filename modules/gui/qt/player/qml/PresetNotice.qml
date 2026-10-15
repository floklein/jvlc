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
import VLC.Player
import VLC.Style

// "Using Anime for The Wand" when a Show starts using a Preset, and
// "Anime deleted · Undo" for a few seconds after a Preset is deleted
Rectangle {
    id: root

    property string message: ""
    // the deleted Preset can be brought back
    property bool undoable: false

    function show(text, undo) {
        root.message = text
        root.undoable = undo
        timer.restart()
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme

        palette: VLCStyle.palette
        colorSet: ColorContext.Window
    }

    Connections {
        target: Presets

        function onAttached(presetName, folderName) {
            root.show(qsTr("Using %1 for %2").arg(presetName).arg(folderName), false)
        }
    }

    Connections {
        target: Presets.model

        function onCanUndoRemoveChanged() {
            if (Presets.model.canUndoRemove)
                root.show(qsTr("%1 deleted").arg(Presets.model.removedName), true)
            else if (root.undoable)
                root.message = ""
        }
    }

    Timer {
        id: timer

        interval: 5000

        onTriggered: {
            if (root.undoable)
                Presets.model.dropUndo()
            root.message = ""
        }
    }

    visible: opacity > 0
    opacity: message !== "" ? 1 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: VLCStyle.duration_short
            easing.type: Easing.InOutQuad
        }
    }

    implicitWidth: layout.implicitWidth + VLCStyle.margin_normal * 2
    implicitHeight: layout.implicitHeight + VLCStyle.margin_xsmall * 2

    radius: height / 2
    color: theme.bg.primary

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Widgets.DefaultShadow {
    }

    RowLayout {
        id: layout

        anchors.centerIn: parent

        spacing: VLCStyle.margin_small

        T.Label {
            id: label

            text: root.message

            font.pixelSize: VLCStyle.fontSize_normal
            color: theme.fg.primary
        }

        T.Label {
            visible: root.undoable

            text: "·"

            font.pixelSize: VLCStyle.fontSize_normal
            color: theme.fg.secondary
        }

        Widgets.TextToolButton {
            visible: root.undoable

            text: qsTr("Undo")

            colorContext.colorSet: ColorContext.ButtonAccent

            onClicked: Presets.model.undoRemove()
        }
    }
}
