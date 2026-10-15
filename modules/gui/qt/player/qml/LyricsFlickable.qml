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
import VLC.MainInterface
import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

Flickable {
    id: lyricsFlickable

    Accessible.role: Accessible.List
    Accessible.name: qsTr("Lyrics")

    property bool lyricsSyncToPlayback: true
    property bool animationsEnabled: true
    property bool fadingEdgeEnabled: true

    readonly property ColorContext colorContext: ColorContext {
        colorSet: ColorContext.View
    }

    implicitWidth: contentWidth
    implicitHeight: contentHeight

    contentWidth: lyricsColumn.width
    contentHeight: lyricsColumn.height

    interactive: height < implicitHeight

    T.ScrollBar.vertical: Widgets.ScrollBarExt {}

    boundsBehavior: Flickable.StopAtBounds
    clip: !fadingEdge.implicitClipping && (height < implicitHeight)

    // TODO: `vlcTime` type can not be used due to Qt 6.2:
    signal playerPositionChangeRequested(var time)

    function snapToCurrentLyric() {
        const idx = Player.currentLyricIndex
        if (idx < 0)
            return

        const item = lyricsRepeater.itemAt(idx)
        if (!item)
            return

        const targetY = item.y + item.height / 2 - lyricsFlickable.height / 2
        lyricsFlickable.contentY = Math.max(0, Math.min(
            targetY, lyricsFlickable.contentHeight - lyricsFlickable.height))
    }

    function scheduleSnapToCurrentLyric() {
        if (lyricsSyncToPlayback)
            Qt.callLater(lyricsFlickable.snapToCurrentLyric)
    }

    // Disable auto-scroll when user manually flicks
    onFlickStarted: lyricsSyncToPlayback = false
    onDragStarted: lyricsSyncToPlayback = false
    onLyricsSyncToPlaybackChanged: scheduleSnapToCurrentLyric()
    onHeightChanged: scheduleSnapToCurrentLyric()
    onContentHeightChanged: scheduleSnapToCurrentLyric()

    Component.onCompleted: scheduleSnapToCurrentLyric()

    // The keyboard: the arrows move between the lines and leave the list at
    // its ends, Enter seeks. The focus comes in on the line being sung.
    LineKeyNavigation {
        id: lineKeys

        count: lyricsRepeater.count

        onActivated: (index) => {
            const line = lyricsRepeater.itemAt(index)
            if (line)
                lyricsFlickable.playerPositionChangeRequested(line.modelData.time)
        }
        onLeftUp: lyricsFlickable.Navigation.defaultNavigationUp()
        onLeftDown: lyricsFlickable.Navigation.defaultNavigationDown()
    }

    Binding {
        target: lineKeys
        property: "currentIndex"
        value: Math.max(Player.currentLyricIndex, 0)
        when: !lyricsFlickable.activeFocus
        restoreMode: Binding.RestoreNone
    }

    function _focusLine(index) {
        const line = lyricsRepeater.itemAt(index)
        if (line)
            line.forceActiveFocus(Qt.TabFocusReason)
    }

    function _showLine(line) {
        Helpers.positionFlickableToContainItem(lyricsFlickable, line)
    }

    onActiveFocusChanged: {
        if (activeFocus)
            _focusLine(lineKeys.currentIndex)
    }

    Keys.onPressed: (event) => {
        if ((event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier) {
            const previous = lineKeys.currentIndex
            if (lineKeys.handleKey(event.key)) {
                event.accepted = true
                if (lineKeys.currentIndex !== previous) {
                    // reading on: the lyrics stop following the playback
                    lyricsFlickable.lyricsSyncToPlayback = false
                    _focusLine(lineKeys.currentIndex)
                }
                return
            }
        }
        Navigation.defaultKeyAction(event)
    }

    Behavior on contentY {
        enabled: animationsEnabled && lyricsSyncToPlayback
        SmoothedAnimation {
            velocity: VLCStyle.dp(300, VLCStyle.scale)
            duration: VLCStyle.duration_veryLong
        }
    }

    Connections {
        target: Player
        enabled: lyricsSyncToPlayback
        function onCurrentLyricIndexChanged(idx) {
            lyricsFlickable.snapToCurrentLyric()
        }
    }

    Column {
        id: lyricsColumn

        width: lyricsFlickable.width

        Repeater {
            id: lyricsRepeater

            model: Player.syltLyrics

            // A line seeks to its time when clicked, or from the keyboard
            // with Enter or Space. Only the keyboard's current line takes the
            // focus, so Tab and the player's arrows reach the list once.
            delegate: T.AbstractButton {
                id: lyricDelegate

                required property int index
                // TODO: `timedText` type can not be used due to Qt 6.2:
                required property var modelData

                readonly property bool isCurrent: index === Player.currentLyricIndex

                anchors.left: parent.left
                anchors.right: parent.right
                implicitHeight: implicitContentHeight + topPadding + bottomPadding
                height: (index === (lyricsRepeater.count - 1))
                    ? Math.max(implicitHeight, lyricsFlickable.height / 2)
                    : implicitHeight
                padding: VLCStyle.margin_small

                focusPolicy: (index === lineKeys.currentIndex) ? Qt.TabFocus : Qt.NoFocus

                text: modelData.text

                Accessible.description: qsTr("Go to %1").arg(modelData.time.formatHMS(0))

                onActiveFocusChanged: {
                    if (!activeFocus)
                        return
                    lineKeys.currentIndex = index
                    lyricsFlickable._showLine(lyricDelegate)
                }

                onClicked: lyricsFlickable.playerPositionChangeRequested(modelData.time)

                // the keyboard focus ring, around the line as it is drawn
                background: Rectangle {
                    color: "transparent"
                    radius: VLCStyle.radius_small
                    border.width: VLCStyle.focus_border
                    border.color: lyricsFlickable.colorContext.accent
                    visible: lyricDelegate.visualFocus
                }

                contentItem: Text {
                    id: lyricText

                    text: lyricDelegate.text

                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap

                    // Use a single pixel size and rely on `scale` to differentiate
                    // the current line; this is cheaper than re-rasterizing the
                    // font on every animation frame.
                    font.pixelSize: VLCStyle.fontSize_xlarge
                    font.weight: lyricDelegate.isCurrent ? Font.Bold : Font.Normal

                    color: lyricsFlickable.colorContext.fg.primary
                    opacity: lyricDelegate.isCurrent || lyricDelegate.visualFocus ? 1.0 : 0.45
                    scale: lyricDelegate.isCurrent ? 1.0 : (VLCStyle.fontSize_large / VLCStyle.fontSize_xlarge)

                    Behavior on scale {
                        // Native text rendering does not handle scaled glyphs
                        // gracefully, so skip the animation in that case.
                        enabled: lyricsFlickable.animationsEnabled
                                 && lyricText.renderType !== Text.NativeRendering
                        NumberAnimation { duration: VLCStyle.duration_long }
                    }
                    Behavior on opacity {
                        enabled: lyricsFlickable.animationsEnabled
                        OpacityAnimator { duration: VLCStyle.duration_long }
                    }
                }
            }
        }
    }

    Widgets.FadingEdge {
        id: fadingEdge

        parent: lyricsFlickable

        anchors.fill: parent

        backgroundColor: "transparent"

        sourceItem: lyricsFlickable.contentItem

        sourceX: lyricsFlickable.contentX
        sourceY: lyricsFlickable.contentY

        orientation: Qt.Vertical

        // Scale the fade with the lyrics' default (non-current) line size,
        // similar to how list views derive it from delegate height.
        fadeSize: VLCStyle.fontSize_large / 2

        // `fadingEdgeEnabled` is the master toggle; the per-edge disabler
        // Bindings below override these when the view is scrolled to the
        // corresponding edge.
        enableBeginningFade: lyricsFlickable.fadingEdgeEnabled
        enableEndFade: lyricsFlickable.fadingEdgeEnabled

        Binding on enableBeginningFade {
            when: lyricsFlickable.atYBeginning
            value: false
        }

        Binding on enableEndFade {
            when: lyricsFlickable.atYEnd
            value: false
        }
    }
}