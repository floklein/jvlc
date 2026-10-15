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
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util
import VLC.MainInterface

T.ProgressBar {
    id: control

    readonly property real _hoveredScalingFactor: 1.8
    property int barHeight: VLCStyle.dp(5, VLCStyle.scale)
    readonly property real _scaledBarHeight: control.barHeight * _hoveredScalingFactor
    readonly property real _scaledY: (-control.barHeight / 2) * (control._hoveredScalingFactor - 1)

    property bool _isSeekPointsShown: true
    readonly property int _seekPointsDistance: VLCStyle.dp(2, VLCStyle.scale)
    readonly property int _seekPointsRadius: VLCStyle.dp(0.5, VLCStyle.scale)
    readonly property real _scaledSeekPointsRadius: _seekPointsRadius * _hoveredScalingFactor

    property bool _currentChapterHovered: false
    property real _tooltipPosition: (Widgets.PointingToolTipAttached.instance?.pos.x ?? 0.0) / width

    property color backgroundColor: theme.bg.primary
    property real touchHandlerMargin: VLCStyle.touchHandlerMargin

    // The redesign's thick seek bar (Figma 1908:7387): a rounded track, the
    // played part inset in it, a bar-shaped handle at its end and one tick per
    // chapter. Thin bars still draw one segment per chapter.
    readonly property bool _thick: barHeight >= VLCStyle.dp(12, VLCStyle.scale)
    readonly property real _inset: _thick ? VLCStyle.dp(2, VLCStyle.scale) : 0

    // The waveform seek bar of the music player (Figma 4778:26191): loudness
    // buckets of the media in 0..1 drawn as bars instead of the played part.
    // Empty draws the plain bar.
    property var peaks: []
    readonly property bool _waveform: _thick && !!peaks && peaks.length > 0

    // a press this close to a chapter start selects the chapter
    readonly property real _chapterSnapDistance: VLCStyle.dp(4, VLCStyle.scale)

    // the hovered or dragged point of the bar, -1 when neither
    readonly property real _pointerX: {
        if (dragHandler.active)
            return Helpers.clamp(dragHandler.centroid.position.x, 0, control.width)
        if (hoverHandler.hovered)
            return Helpers.clamp(hoverHandler.point.position.x, 0, control.width)
        if (thumbnailer.testPosition >= 0) // test hook
            return thumbnailer.testPosition * control.width
        return -1
    }

    // the seek thumbnail (spec #30: 176x99 while hovering or dragging a video)
    readonly property bool _showThumbnail: control._thick
                                           && control.enabled
                                           && control._pointerX >= 0
                                           && Player.hasVideoOutput
                                           && Player.seekable

    Keys.onRightPressed: Player.jumpFwd()
    Keys.onLeftPressed: Player.jumpBwd()

    // read as "12:34 of 45:00"
    Accessible.role: Accessible.Slider
    Accessible.name: qsTr("%1 of %2").arg(Player.time.formatHMS(0)).arg(Player.length.formatHMS(0))
    Accessible.onIncreaseAction: Player.jumpFwd()
    Accessible.onDecreaseAction: Player.jumpBwd()

    function showChapterMarks() {
        _isSeekPointsShown = true
        seekpointTimer.restart()
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Slider

        enabled: control.enabled
        focused: control.visualFocus
        hovered: hoverHandler.hovered
    }

    Timer {
        id: seekpointTimer
        running: Player.hasChapters && !hoverHandler.hovered && _isSeekPointsShown
        interval: 3000
        onTriggered: control._isSeekPointsShown = false
    }

    // the seek thumbnail shows the time itself
    Widgets.PointingToolTipAttached.visible: !control._showThumbnail
                                             && (hoverHandler.hovered || control.visualFocus || dragHandler.active)

    Widgets.PointingToolTipAttached.text: {
        if (!Widgets.PointingToolTipAttached.instance?.visible)
            return ""

        let _text

        const length = Player.length
        if (hoverHandler.hovered)
            _text = length.scale(Widgets.PointingToolTipAttached.instance.pos.x / control.width)
        else
            _text = Player.time

        _text = _text.formatHMS(length.isSubSecond() ? VLCTick.SubSecondFormattedAsMS : 0)

        if (Player.hasChapters)
            _text += " - " + Player.chapters.getNameAtPosition(control._tooltipPosition)

        return _text
    }

    Widgets.PointingToolTipAttached.pos: Qt.point(hoverHandler.hovered ? Helpers.clamp(hoverHandler.point.position.x, 0, control.availableWidth)
                                                                       : (control.visualPosition * control.width), 0)

    FSM {
        id: fsm
        signal playerUpdatePosition(real position)
        signal pressControl(real position, bool forcePrecise)
        signal releaseControl(real position, bool forcePrecise)
        signal moveControl(real position, bool forcePrecise)
        signal inputChanged()

        //each signal is associated to a key, when a signal is received,
        //transitions of active state for the given key are evaluated
        signalMap: ({
            "playerUpdatePosition": fsm.playerUpdatePosition,
            "pressControl": fsm.pressControl,
            "releaseControl": fsm.releaseControl,
            "moveControl": fsm.moveControl,
            "inputChanged": fsm.inputChanged
        })

        initialState: fsmReleased

        function _setPositionFromValue(position) {
            control.value = position
        }

        // whether the video follows a drag with fast seeks, to be made precise
        // on release
        property bool _scrubbed: false

        function _seekToPosition(position, threshold, forcePrecise, scrub) {
            position = Helpers.clamp(position, 0., 1.)
            control.value = position
            if (!forcePrecise) {
                const chapter = Player.chapters.getClosestChapterFromPos(position, threshold)
                if (chapter !== -1) {
                    Player.chapters.selectChapter(chapter)
                    fsm._scrubbed = false
                    return
                }
            }
            if (scrub) {
                // live scrub: key frames keep up with the pointer
                Player.scrubToPos(position)
                fsm._scrubbed = true
            } else {
                Player.position = position
                fsm._scrubbed = false
            }
        }

        function _endScrub() {
            if (fsm._scrubbed) {
                fsm._scrubbed = false
                Player.position = control.value
            }
        }

        FSMState {
            id: fsmReleased

            transitions: ({
                "playerUpdatePosition": {
                    action: (position) => {
                        control.value = position
                    }
                },
                "pressControl": {
                    action: (position, forcePrecise) => {
                        control.forceActiveFocus()
                        fsm._seekToPosition(position, control._chapterSnapDistance / control.width, forcePrecise, false)
                    },
                    target: fsmHeld
                }
            })
        }

        FSMState  {
            id: fsmHeld

            transitions: ({
                "moveControl": {
                    action: (position, forcePrecise) => {
                        fsm._seekToPosition(position, VLCStyle.dp(2, VLCStyle.scale) / control.width, forcePrecise, true)
                    }
                },
                "releaseControl": {
                    action: fsm._endScrub,
                    target: fsmReleased
                },
                "inputChanged": {
                    target: fsmHeldWrongInput
                }
            })
        }

        FSMState  {
            id: fsmHeldWrongInput

            function enter() {
                fsm._setPositionFromValue(Player.position)
            }

            transitions: ({
                "playerUpdatePosition": {
                    action: fsm._setPositionFromValue
                },
                "releaseControl": {
                    target: fsmReleased
                }
            })
        }
    }

    Connections {
        target: Player
        function onPositionChanged() {  fsm.playerUpdatePosition(Player.position) }
        function onInputChanged() {  fsm.inputChanged() }
    }

    Component.onCompleted: value = Player.position

    implicitHeight: control.barHeight
    height: implicitHeight

    padding: 0

    //we use our own HoverHandler
    hoverEnabled: false

    HoverHandler {
        id: hoverHandler

        onHoveredChanged: () => {
            if (hovered) {
                if(Player.hasChapters)
                    control._isSeekPointsShown = true
            } else {
                if(Player.hasChapters)
                    seekpointTimer.restart()
            }
        }
    }

    SeekThumbnailer {
        id: thumbnailer

        mrl: control._thick ? Player.url : ""
        length: Player.length
        size: Qt.size(Math.round(thumbnailPopup.thumbnailWidth * Screen.devicePixelRatio),
                      Math.round(thumbnailPopup.thumbnailHeight * Screen.devicePixelRatio))

        // the media may become known while the pointer rests on the bar
        onMrlChanged: control._requestThumbnail()
        onLengthChanged: control._requestThumbnail()
        onSizeChanged: control._requestThumbnail()
    }

    function _requestThumbnail() {
        if (!control._showThumbnail)
            thumbnailer.stop()
        else
            thumbnailer.request(control._pointerX / control.width)
    }

    on_PointerXChanged: _requestThumbnail()
    on_ShowThumbnailChanged: _requestThumbnail()

    T.Popup {
        id: thumbnailPopup

        // spec #30 / #10: 176x99 with its time (Figma 2151:9673)
        readonly property real thumbnailWidth: VLCStyle.dp(176, VLCStyle.scale)
        readonly property real thumbnailHeight: VLCStyle.dp(99, VLCStyle.scale)

        readonly property real _position: Math.max(control._pointerX, 0) / control.width

        // follows the pointer without any transition; the window edges clamp it
        x: Math.max(control._pointerX, 0) - width / 2
        y: -height - VLCStyle.margin_xsmall
        margins: VLCStyle.margin_xsmall

        implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                implicitContentWidth + leftPadding + rightPadding)
        implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                 implicitContentHeight + topPadding + bottomPadding)

        padding: VLCStyle.spacing_10

        visible: control._showThumbnail
        closePolicy: T.Popup.NoAutoClose
        focus: false
        modal: false
        dim: false

        enter: null
        exit: null

        readonly property ColorContext colorContext: ColorContext {
            id: popupTheme
            palette: theme.palette
            colorSet: ColorContext.Window
        }

        readonly property ColorContext textColorContext: ColorContext {
            id: popupTextTheme
            palette: theme.palette
            colorSet: ColorContext.ToolButton
        }

        background: Widgets.TintBackground {
            colorContext: popupTheme
            radius: VLCStyle.radius_xlarge
        }

        contentItem: Column {
            spacing: VLCStyle.spacing_10

            Widgets.ImageExt {
                width: thumbnailPopup.thumbnailWidth
                height: thumbnailPopup.thumbnailHeight

                radius: VLCStyle.radius_small
                backgroundColor: "black"

                // until the first picture is ready
                Rectangle {
                    anchors.fill: parent
                    z: -1
                    radius: parent.radius
                    color: "black"
                }

                // served from the cache: it shows at once, the last picture
                // stays until the next one is ready
                source: thumbnailer.source
                asynchronous: false
                cache: false
                fillMode: Image.PreserveAspectCrop

                Accessible.ignored: true
            }

            T.Label {
                width: thumbnailPopup.thumbnailWidth

                text: {
                    if (!thumbnailPopup.visible)
                        return ""
                    const length = Player.length
                    let text = length.scale(thumbnailPopup._position)
                                     .formatHMS(length.isSubSecond() ? VLCTick.SubSecondFormattedAsMS : 0)
                    if (Player.hasChapters) {
                        const chapter = Player.chapters.getNameAtPosition(thumbnailPopup._position).trim()
                        if (chapter)
                            text += " - " + chapter
                    }
                    return text
                }

                color: popupTextTheme.fg.primary
                font.pixelSize: VLCStyle.fontSize_large
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight

                Accessible.ignored: true
            }
        }
    }

    contentItem: Item {
        implicitHeight: control.implicitHeight
        implicitWidth: control.implicitWidth

        //placing the TapHandler directly in the Control doesn't work with 6.2
        TapHandler {
            acceptedButtons: Qt.LeftButton

            gesturePolicy: TapHandler.WithinBounds

            // Make it easier to tap with touch screen:
            margin: MainCtx.usingTouch ? control.touchHandlerMargin : 0.0

            onPressedChanged: {
                if (pressed) {
                    fsm.pressControl(point.position.x / control.width, point.modifiers === Qt.ShiftModifier)
                } else {
                    // NOTE: Point is still valid at this point.
                    fsm.releaseControl(point.position.x / control.width, point.modifiers === Qt.ShiftModifier)
                }
            }
        }

        DragHandler {
            id: dragHandler
            acceptedButtons: Qt.LeftButton

            // Make it easier to drag with touch screen:
            margin: MainCtx.usingTouch ? control.touchHandlerMargin : 0

            target: null
            dragThreshold: 0
            grabPermissions: PointerHandler.CanTakeOverFromAnything

            property bool filterEvents: false

            function moveControl() {
                fsm.moveControl(dragHandler.centroid.position.x / control.width,
                                dragHandler.centroid.modifiers === Qt.ShiftModifier)
            }

            onActiveChanged: {
                if (active) {
                    filterEvents = false
                    fsm.pressControl(centroid.position.x / control.width, centroid.modifiers === Qt.ShiftModifier)
                } else {
                    fsm.releaseControl( centroid.position.x / control.width, centroid.modifiers === Qt.ShiftModifier)
                }
            }

            onCentroidChanged: {
                // FIXME: Qt 6.5 use xAxis.onActiveValueChanged in the DragHandler
                if (filterEvents) {
                    Qt.callLater(dragHandler.moveControl)
                } else {
                    dragHandler.moveControl()
                    filterEvents = true
                }
            }
        }

        WheelHandler {
            orientation: Qt.Vertical | Qt.Horizontal

            acceptedDevices: PointerDevice.AllDevices

            onWheel: (wheel) => {
                wheelToVLC.qmlWheelEvent(wheel)
                wheel.accepted = true
            }
        }

        WheelToVLCConverter {
            id: wheelToVLC

            function handleVertical(steps: int) {
                if (steps > 0)
                    Player.jumpFwd()
                else
                    Player.jumpBwd()
            }

            function handleHorizontal(steps: int) {
                if (steps > 0)
                    Player.jumpBwd()
                else
                    Player.jumpFwd()
            }

            Component.onCompleted: {
                wheelUpDown.connect(wheelToVLC, wheelToVLC.handleVertical)
                wheelLeftRight.connect(wheelToVLC, wheelToVLC.handleHorizontal)
            }
        }
    }

    background: Item {
        width: control.availableWidth
        implicitHeight: control.implicitHeight
        height: implicitHeight

        Rectangle {
            id: sliderRect
            visible: control._thick || !Player.hasChapters
            color: control.backgroundColor
            anchors.fill: parent
            radius: control._thick ? VLCStyle.radius_normal : implicitHeight

            // the track is as dark as the scrim over the video: outline it
            border.width: control._thick ? VLCStyle.dp(1, VLCStyle.scale) : 0
            border.color: theme.border

            // keyboard focus ring
            Rectangle {
                anchors.fill: parent
                anchors.margins: -VLCStyle.focus_border

                radius: parent.radius + VLCStyle.focus_border

                color: "transparent"
                border.width: VLCStyle.focus_border
                border.color: theme.visualFocus

                visible: control._thick && control.visualFocus
            }
        }

        Repeater {
            id: seekpointsRptr

            anchors.left: parent.left
            anchors.right: parent.right
            height: control.barHeight
            visible: !control._thick && Player.hasChapters

            model: Player.chapters
            Item {
                Rectangle {
                    id: seekpointsRect
                    readonly property real startPosition: model.startPosition ??  0.0
                    readonly property real endPosition: model.endPosition ?? 1.0

                    readonly property int _currentChapter: {
                        if (control.visualPosition < seekpointsRect.startPosition)
                            return 1
                        else if (control.visualPosition > seekpointsRect.endPosition)
                            return -1
                        return 0
                    }
                    on_CurrentChapterChanged: {
                        if(_hovered)
                            control._currentChapterHovered = _currentChapter === 0
                    }

                    readonly property bool _hovered: hoverHandler.hovered &&
                                            (hoverHandler.point.position.x > x && hoverHandler.point.position.x < x+width)

                    color: _currentChapter < 0 ? theme.fg.primary : control.backgroundColor
                    width: sliderRect.width * seekpointsRect.endPosition - x - control._seekPointsDistance
                    x: sliderRect.width * seekpointsRect.startPosition

                    Rectangle {
                        id: progressRepRect
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        radius: parent.radius

                        width: sliderRect.width * control.visualPosition - parent.x - control._seekPointsDistance
                        visible: parent._currentChapter === 0
                        color: theme.fg.primary

                        // We use dynamic mutability group because time interpolation
                        // may induce rapid change:
                        // TODO: Do not use `Binding` when minimum Qt is 6.12:
                        Binding {
                            target: progressRepRect
                            property: "mutabilityGroup"
                            when: (progressRepRect.mutabilityGroup !== undefined)
                            value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.DynamicMutabilityGroup
                                                                                          : Item.StaticMutabilityGroup
                        }
                    }
                }

                transitions: [
                    Transition {
                        to: "*"
                        SequentialAnimation{
                            PropertyAction { targets: control; property: "_currentChapterHovered" }
                            NumberAnimation {
                                targets: [seekpointsRect, progressRepRect]; properties: "height, y, radius"
                                duration: VLCStyle.duration_short; easing.type: Easing.InSine
                            }
                        }
                    }
                ]

                states:[
                    State {
                        name: "visible"
                        PropertyChanges {
                            target: control;
                            _currentChapterHovered: seekpointsRect._currentChapter === 0 ? false : control._currentChapterHovered
                        }
                        PropertyChanges { target: seekpointsRect; height: control.barHeight }
                        PropertyChanges { target: seekpointsRect; y: 0 }
                        PropertyChanges { target: seekpointsRect; radius: control._seekPointsRadius }
                    },
                    State {
                        name: "visibleLarge"
                        PropertyChanges {
                            target: control;
                            _currentChapterHovered: seekpointsRect._currentChapter === 0 ? true : control._currentChapterHovered
                        }
                        PropertyChanges { target: seekpointsRect; height: control._scaledBarHeight }
                        PropertyChanges { target: seekpointsRect; y: control._scaledY }
                        PropertyChanges { target: seekpointsRect; radius: control._scaledSeekPointsRadius }
                    }
                ]

                state: (seekpointsRect._hovered || (seekpointsRect._currentChapter === 0 && fsm._state === fsm.fsmHeld))
                       ? "visibleLarge"
                       : "visible"
            }
        }

        Rectangle {
            id: progressRect
            x: control._inset
            y: control._inset
            width: control.visualPosition * (parent.width - control._inset * 2)
            visible: (control._thick || !Player.hasChapters) && !control._waveform
            color: control._thick ? theme.bg.highlight : theme.fg.primary
            height: control.barHeight - control._inset * 2
            radius: control._thick ? VLCStyle.radius_small : control._seekPointsRadius

            // We use dynamic mutability group because time interpolation
            // may induce rapid change:
            // TODO: Do not use `Binding` when minimum Qt is 6.12:
            Binding {
                target: progressRect
                property: "mutabilityGroup"
                when: (progressRect.mutabilityGroup !== undefined)
                value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.DynamicMutabilityGroup
                                                                              : Item.StaticMutabilityGroup
            }
        }

        // the waveform: bars still to play, with the played ones over them up
        // to the handle
        Item {
            id: waveform

            anchors.fill: parent

            visible: control._waveform

            component Bars : WaveformBars {
                height: waveform.height

                peaks: control._waveform ? control.peaks : []

                barWidth: VLCStyle.dp(2, VLCStyle.scale)
                barSpacing: VLCStyle.dp(3, VLCStyle.scale)
                minimumBarHeight: VLCStyle.dp(2, VLCStyle.scale)
                maximumBarHeight: height - VLCStyle.dp(14, VLCStyle.scale)
            }

            Bars {
                id: waveformBars

                x: VLCStyle.dp(6, VLCStyle.scale)
                width: parent.width - x * 2

                color: theme.fg.secondary
            }

            Item {
                x: waveformBars.x
                width: Math.max(0, sliderHandle.x - x)
                height: parent.height

                clip: true

                Bars {
                    width: waveformBars.width

                    color: theme.fg.highlight
                }
            }
        }

        // chapter ticks of the thick bar; a press near one selects the chapter
        Repeater {
            id: chapterTicks

            model: control._thick ? Player.chapters : null

            Rectangle {
                required property var model

                readonly property real _position: model.startPosition ?? 0.0
                readonly property real _x: control._inset + _position * (control.availableWidth - control._inset * 2)
                readonly property bool _hovered: hoverHandler.hovered && !dragHandler.active
                                                 && Math.abs(control._pointerX - _x) < control._chapterSnapDistance

                // the first chapter starts with the bar
                visible: _position > 0

                x: Math.round(_x - width / 2)
                y: control._inset
                width: VLCStyle.dp(2, VLCStyle.scale)
                height: control.barHeight - control._inset * 2

                color: _hovered ? theme.fg.primary : theme.bg.secondary
            }
        }

        Rectangle {
            id: bufferRect
            property int bufferAnimWidth: VLCStyle.dp(100, VLCStyle.scale)
            property int bufferAnimPosition: 0
            property int bufferFrames: 1000
            property alias animateLoading: loadingAnim.running
            property bool display: false
            property bool buffering: Player.buffering > 0 && Player.buffering < 1

            height: control.barHeight
            opacity: 0.4
            color: theme.fg.neutral //FIXME buffer color ?
            radius: control.barHeight

            // TODO: Do not use `Binding` when minimum Qt is 6.12:
            Binding {
                target: bufferRect
                property: "mutabilityGroup"
                when: bufferRect.mutabilityGroup !== undefined
                value: bufferRect.buffering ? Item.DynamicMutabilityGroup
                                            : Item.StaticMutabilityGroup
            }

            Timer {
                id: bufferingTimer
                interval: VLCStyle.duration_humanMoment
                repeat: false
                running: false
                onTriggered: {
                    bufferRect.display = true
                }
            }

            onBufferingChanged: {
                if(buffering)
                {
                    bufferingTimer.start()
                }
                else
                {
                    bufferingTimer.stop()
                    bufferRect.display = false
                }
            }

            states: [
                State {
                    name: "hidden"
                    when: !control.visible
                    PropertyChanges {
                        target: bufferRect
                        width: bufferAnimWidth
                        visible: false
                        x: 0
                        animateLoading: false
                    }
                },
                State {
                    name: "buffering not started"
                    when: control.visible && Player.buffering === 0
                    PropertyChanges {
                        target: bufferRect
                        width: bufferAnimWidth
                        visible: true
                        x: (bufferAnimPosition / bufferFrames) * (parent.width - bufferAnimWidth)
                        animateLoading: true
                    }
                },
                State {
                    name: "time to start playing known"
                    when: control.visible && bufferRect.display
                    PropertyChanges {
                        target: bufferRect
                        width: Player.buffering * parent.width
                        visible: true
                        x: 0
                        animateLoading: false
                    }
                },
                State {
                    name: "playing from buffer"
                    when: control.visible && !bufferRect.display
                    PropertyChanges {
                        target: bufferRect
                        width: Player.buffering * parent.width
                        visible: false
                        x: 0
                        animateLoading: false
                    }
                }
            ]

            SequentialAnimation on bufferAnimPosition {
                id: loadingAnim
                running: bufferRect.animateLoading
                loops: Animation.Infinite
                PropertyAnimation {
                    from: 0.0
                    to: bufferRect.bufferFrames
                    duration: VLCStyle.durationSliderBouncing
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: VLCStyle.duration_veryLong
                }
                PropertyAnimation {
                    from: bufferRect.bufferFrames
                    to: 0.0
                    duration: VLCStyle.durationSliderBouncing
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: VLCStyle.duration_veryLong
                }
            }
        }
    }

    Rectangle {
        id: sliderHandle

        property int _size: control.barHeight * 3

        // thick bar: an 8 px bar ending the played part, always shown
        x: control._thick ? Math.max(control._inset,
                                     control._inset + control.visualPosition * (control.availableWidth - control._inset * 2) - width)
                          : (control.visualPosition * control.availableWidth) - width / 2
        y: (control.barHeight - height) / 2

        // the waveform's handle is a 4 px bar (Figma 4778:26192)
        implicitWidth: control._thick ? VLCStyle.dp(control._waveform ? 4 : 8, VLCStyle.scale)
                                      : sliderHandle._size
        implicitHeight: control._thick ? control.barHeight - control._inset * 2 : sliderHandle._size
        radius: control._thick ? VLCStyle.radius_small : VLCStyle.margin_small
        color: theme.fg.primary

        // We use dynamic mutability group because time interpolation
        // may induce rapid change:
        // TODO: Do not use `Binding` when minimum Qt is 6.12:
        Binding {
            target: sliderHandle
            property: "mutabilityGroup"
            when: (sliderHandle.mutabilityGroup !== undefined)
            value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.DynamicMutabilityGroup
                                                                          : Item.StaticMutabilityGroup
        }

        transitions: [
            Transition {
                to: "hidden"
                SequentialAnimation {
                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"
                        to: 0
                        duration: VLCStyle.duration_short; easing.type: Easing.OutSine
                    }
                    PropertyAction { target: sliderHandle; property: "visible"; value: false; }
                }
            },
            Transition {
                to: "visible"
                SequentialAnimation {
                    PropertyAction { target: sliderHandle; property: "visible"; value: true; }
                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"
                        to: sliderHandle._size
                        duration: VLCStyle.duration_short; easing.type: Easing.InSine
                    }
                }
            },
            Transition {
                to: "visibleLarge"
                SequentialAnimation {
                    PropertyAction { target: sliderHandle; property: "visible"; value: true; }
                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"
                        to: sliderHandle._size * (0.8 * control._hoveredScalingFactor)
                        duration: VLCStyle.duration_short; easing.type: Easing.InSine
                    }
                }
            }
        ]

        state: {
            if (control._thick)
                return ""
            return (hoverHandler.hovered || control.visualFocus)
                   ? ((control._currentChapterHovered || (Player.hasChapters && fsm._state === fsm.fsmHeld)) ? "visibleLarge" : "visible")
                   : "hidden"
        }
    }
}
