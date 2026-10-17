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
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Player

// The speaker button and the volume cone (Figma 491:1731, 244:184). The full
// cone spans 0 to 200% (Figma's scale), the default 100% fills half of it.
// The volume still stops at the maximum volume (125% by default): the cone
// past it is not reachable.
T.Pane {
    id: root

    property bool paintOnly: false

    Accessible.name: qsTr("Volume")

    readonly property var _player: paintOnly ? ({ muted: false, volume: .5 }) : Player
    // the cone works in percent
    readonly property int _maxvol: MainCtx.maxVolume
    // the volume of the full cone, and the part of the cone up to the maximum
    readonly property int _scale: Math.max(200, _maxvol)
    readonly property real _reach: _maxvol / _scale

    // how far the Control bar folds (ControlBarFolding.Level): in a narrow
    // window the cone collapses to the speaker, and pops up over it
    property int foldLevel: ControlBarFolding.Full

    readonly property bool compact: !paintOnly && (foldLevel >= ControlBarFolding.CompactVolume)

    readonly property real _compactWidth: leftPadding + rightPadding + volumeBtn.implicitWidth
    readonly property real _fullWidth: _compactWidth + volumeWidget.spacing + volControl.implicitWidth

    // the room the widget needs at each fold level
    readonly property var foldWidths: [_fullWidth, _fullWidth, _fullWidth, _compactWidth]

    // the pop-up cone shows while the pointer or the focus is on the speaker
    // or on the cone
    readonly property bool _popupWanted: compact && (volumeBtn.hovered || volumeBtn.visualFocus
                                                     || popupHover.hovered || volControl.pressed
                                                     || volControl.activeFocus)

    signal requestLockUnlockAutoHide(bool lock)

    on_PopupWantedChanged: {
        if (_popupWanted) {
            popupHideTimer.stop()
            volumePopup.open()
        } else if (!compact) {
            popupHideTimer.stop()
            volumePopup.close()
        } else {
            popupHideTimer.restart()
        }
    }

    states: State {
        name: "compact"
        when: root.compact

        ParentChange {
            target: volControl
            parent: popupContent
            x: 0
            y: 0
        }
    }

    Timer {
        id: popupHideTimer

        interval: 300
        onTriggered: volumePopup.close()
    }

    T.Popup {
        id: volumePopup

        parent: volumeBtn

        // centered over the speaker
        x: (parent.width - width) / 2
        y: -height - VLCStyle.margin_xsmall

        padding: VLCStyle.margin_xsmall

        implicitWidth: implicitContentWidth + leftPadding + rightPadding
        implicitHeight: implicitContentHeight + topPadding + bottomPadding

        modal: false
        focus: false
        closePolicy: T.Popup.CloseOnEscape

        onOpened: root.requestLockUnlockAutoHide(true)
        onClosed: root.requestLockUnlockAutoHide(false)

        // a popup is not in the item tree: it takes the palette of the bar
        readonly property ColorContext colorContext: ColorContext {
            id: popupTheme

            palette: volControl.colorContext.palette
            colorSet: ColorContext.Window
        }

        // the bar's own color, around the cone's track
        background: Rectangle {
            color: popupTheme.bg.primary
            radius: VLCStyle.radius_large
            border.color: popupTheme.border
        }

        contentItem: Item {
            id: popupContent

            Accessible.role: Accessible.Dialog
            Accessible.name: qsTr("Volume")

            implicitWidth: volControl.implicitWidth
            implicitHeight: volControl.implicitHeight

            HoverHandler {
                id: popupHover
            }
        }
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentWidth: volumeWidget.implicitWidth
    contentHeight: volumeWidget.implicitHeight

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    RowLayout {
        id: volumeWidget

        anchors.fill: parent
        spacing: VLCStyle.spacing_8

        Widgets.IconToolButton{
            id: volumeBtn

            focus: true
            paintOnly: root.paintOnly
            text: {
                if (_player.muted)
                    return VLCIcons.volume_muted
                else if (_player.volume === 0)
                    return VLCIcons.volume_zero
                else if (_player.volume < .33)
                    return VLCIcons.volume_low
                else if (_player.volume <= .66)
                    return VLCIcons.volume_medium
                else
                    return VLCIcons.volume_high
            }
            description: _player.muted ? qsTr("Unmute") : qsTr("Mute")
            shortcutAction: "vol-mute"

            AccessibleCompat.id: "muteButton"

            onClicked: Player.muted = !Player.muted

            Accessible.onIncreaseAction: {
                Player.muted = false
                Player.setVolumeUp()
            }

            Accessible.onDecreaseAction: {
                Player.muted = false
                Player.setVolumeDown()
            }

            Navigation.parentItem: root
            Navigation.rightItem: root.compact ? null : volControl

            // without the cone beside it, the arrows change the volume
            Keys.onUpPressed: (event) => {
                if (!root.compact) {
                    event.accepted = false
                    return
                }
                Player.muted = false
                Player.setVolumeUp()
            }

            Keys.onDownPressed: (event) => {
                if (!root.compact) {
                    event.accepted = false
                    return
                }
                Player.muted = false
                Player.setVolumeDown()
            }
        }

        Widgets.SliderExt {
            id: volControl

            implicitWidth: VLCStyle.dp(88, VLCStyle.scale)
            implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

            // the slider spans 0 to the maximum volume: the part of the cone
            // up to it. A press past it sets the maximum.
            rightPadding: Math.round(width * (1 - root._reach))

            // WARNING: Directly setting the insets break handle and background positioning.
            Binding on topInset {
                when: MainCtx.usingTouch
                value: VLCStyle.touchHandlerMargin
            }
            Binding on bottomInset {
                when: MainCtx.usingTouch
                value: VLCStyle.touchHandlerMargin
            }

            Layout.alignment: Qt.AlignVCenter

            property bool _inhibitPlayerVolumeUpdate: false

            // FIXME: Currently we are not updating the ShiftModifier status while dragging. This
            //        could be fixed with a custom Slider based on a MouseArea.
            property bool _shiftPressed: false

            property bool _keyPressed: false

            property bool filterEvents: false

            // in percent, so that screen readers read the volume as a percentage
            from: 0
            to: root._maxvol
            stepSize: 1

            handleWidth: 0

            enabled: !root.paintOnly // disables event handling depending on this

            toolTipTextProvider: function (value) {
                // the real value i.e 'Player.volume' bounds can be different
                // from bounds of this widget and we want to show the current
                // volume here, so directly use Player.volume instead of "value"

                return Math.round(Player.volume * 100) + "%"
            }

            Accessible.name: qsTr("Volume")
            AccessibleCompat.id: "volumeSlider"

            // the cone has no handle
            handle: Item {
                x: volControl.leftPadding + volControl.visualPosition * volControl.availableWidth
                y: volControl.topPadding + volControl.availableHeight / 2

                implicitWidth: 0
                implicitHeight: 0
            }

            background: Item {
                x: volControl.leftPadding
                y: volControl.topPadding

                implicitWidth: VLCStyle.dp(88, VLCStyle.scale)
                implicitHeight: VLCStyle.dp(28, VLCStyle.scale)

                // the whole cone, past the slider's reach
                width: volControl.width - volControl.leftPadding
                height: volControl.availableHeight

                // Figma: the track is BG/B2 in the dark frames (491:1733),
                // BG/A5 in the light one (2425:13645); the shade over the
                // level darkens it in the dark Theme, lightens it in the light one
                readonly property bool _dark: volControl.colorContext.palette?.isDark ?? true

                ColorContext {
                    id: lightTrackTheme
                    palette: volControl.colorContext.palette
                    // its BG/A5 (bg.highlight)
                    colorSet: ColorContext.Navigation
                }

                Image {
                    id: coneTrack

                    anchors.centerIn: parent

                    width: parent.width
                    height: parent.height * (27.3633 / 28)

                    sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                    fillMode: Image.Stretch

                    source: SVGColorImage.colorize("qrc:///misc/volume_cone_track.svg")
                                .color1(parent._dark ? volControl.colorContext.bg.primary
                                                     : lightTrackTheme.bg.highlight)
                                .uri()
                }

                // the level: the gradient cone, cut at the current volume
                Item {
                    x: parent.width * (1.5 / 88)
                    anchors.verticalCenter: parent.verticalCenter

                    width: (parent.width * (86 / 88)) * volControl.visualPosition * root._reach
                    height: coneLevel.height

                    clip: true

                    opacity: root._player.muted ? 0.5 : 1

                    Image {
                        id: coneLevel

                        width: coneTrack.width * (86 / 88)
                        height: coneTrack.height * (24.56 / 27.3633)

                        sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                        fillMode: Image.Stretch

                        // Figma 491:1733 / 2425:13645: black at 20 % over the
                        // gradient in the dark Theme, white at 20 % in the light one
                        source: SVGColorImage.colorize("qrc:///misc/volume_cone_level.svg")
                                    .color1(parent.parent._dark ? "black" : "white")
                                    .uri()
                    }
                }

                // keyboard focus ring
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -VLCStyle.focus_border

                    radius: VLCStyle.radius_small

                    color: "transparent"
                    border.width: VLCStyle.focus_border
                    border.color: volControl.colorContext.visualFocus

                    visible: volControl.visualFocus
                }
            }

            Keys.onPressed: (event) => {
                if (KeyHelper.matchOk(event)) {
                    event.accepted = true

                    _keyPressed = true
                } else {
                    Navigation.defaultKeyAction(event)
                }
            }

            Keys.onReleased: (event) => {
                if (_keyPressed === false)
                    return

                _keyPressed = false

                if (KeyHelper.matchOk(event)) {
                    Player.muted = !Player.muted
                }
            }

            function _syncVolumeWithPlayer() {
                volControl._inhibitPlayerVolumeUpdate = true
                // screen readers read the value: a whole percentage
                volControl.value = Helpers.accessibleValue(_player.volume * 100)
                volControl._inhibitPlayerVolumeUpdate = false
            }

            function _adjustPlayerVolume() {
                Player.muted = false

                let value = volControl.value

                // NOTE: We are clamping the value to make it easier to restore the default volume.
                if (_shiftPressed === false && Math.abs(value - 100) < 1)
                    value = 100

                Player.volume = value / 100

                volControl.value = value
            }

            Component.onCompleted: {
                root.paintOnlyChanged.connect(_syncVolumeWithPlayer)
                volControl._syncVolumeWithPlayer()
            }

            Connections {
                target: Player
                enabled: !paintOnly

                function onVolumeChanged() { volControl._syncVolumeWithPlayer() }
            }

            Binding on Widgets.PointingToolTipAttached.visible {
                when: sliderMouseArea.pressed
                value: true
            }

            Navigation.leftItem: volumeBtn
            Navigation.parentItem: root

            Keys.onUpPressed: (event) => {
                Player.muted = false
                Player.setVolumeUp()
            }

            Keys.onDownPressed: (event) => {
                Player.muted = false
                Player.setVolumeDown()
            }

            Keys.priority: Keys.BeforeItem

            onValueChanged: {
                if (paintOnly)
                    return

                if (!volControl._inhibitPlayerVolumeUpdate) {
                    if (filterEvents) {
                        Qt.callLater(volControl._adjustPlayerVolume)
                    } else {
                        volControl._adjustPlayerVolume()
                        filterEvents = true
                    }
                }
            }

            MouseArea {
                id: sliderMouseArea

                anchors.fill: parent

                acceptedButtons: (Qt.LeftButton | Qt.RightButton)

                onPressed: (mouse) => {
                    if (root.paintOnly) {
                        mouse.accepted = true
                        return
                    }

                    volControl._shiftPressed = (mouse.modifiers === Qt.ShiftModifier)

                    if (mouse.button === Qt.LeftButton) {
                        mouse.accepted = false

                        return
                    }

                    volControl.filterEvents = false
                    adjustVolume(mouse)
                }

                onPositionChanged: (mouse) => {
                    if (mouse.buttons & Qt.RightButton)
                        adjustVolume(mouse)
                }

                onWheel: (wheel) => {

                    if ((Math.abs(wheel.pixelDelta.x) % 120 > 0) || (Math.abs(wheel.pixelDelta.y) % 120 > 0)) {
                        let delta = 0
                        if (Math.abs(wheel.pixelDelta.x) > Math.abs(wheel.pixelDelta.y))
                            delta = wheel.pixelDelta.x
                        else
                            delta = wheel.pixelDelta.y

                        if (wheel.inverted)
                            delta = -delta
                        volControl.value += 0.1 * delta
                    }
                    else
                        wheelToVLC.qmlWheelEvent(wheel)

                    wheel.accepted = true
                }

                // right button: snap to 0, 50, 100 or the maximum
                function adjustVolume(mouse) {
                    mouse.accepted = true

                    // the cone spans 0 to `_scale`
                    const pos = Math.min(mouse.x * root._scale / width, root._maxvol)

                    if (pos < 25)
                        volControl.value = 0
                    else if (pos < 75)
                        volControl.value = 50
                    else if (pos < (100 + root._maxvol) / 2)
                        volControl.value = 100
                    else
                        volControl.value = root._maxvol
                }

                WheelToVLCConverter {
                    id: wheelToVLC

                    function handle(steps: int) {
                        if (steps > 0)
                            Player.setVolumeUp(steps)
                        else
                            Player.setVolumeDown(-steps)
                    }

                    Component.onCompleted: {
                        wheelUpDown.connect(wheelToVLC.handle)
                        wheelLeftRight.connect(wheelToVLC.handle)
                    }
                }
            }
        }
    }
}
