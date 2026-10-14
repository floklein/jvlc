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
import QtTest

import VLC.Util

TestCase {
    id: root
    name: "TrackMenuFSM"

    property var adjustmentsRequests: []

    TrackMenuFSM {
        id: fsm

        onRequestAdjustments: (tab) => root.adjustmentsRequests.push(tab)
    }

    // a click on a bar button with the mouse: the popover closes on the
    // release outside it, before the button sees its click
    function mouseClick(tab) {
        const wasOpened = fsm.opened
        fsm.buttonPressed(tab)
        if (wasOpened)
            fsm.dismiss()
        fsm.buttonClicked(tab)
    }

    // a click with the keyboard: the popover stays open until the click
    function keyClick(tab) {
        fsm.buttonPressed(tab)
        fsm.buttonClicked(tab)
    }

    function init() {
        if (fsm.opened)
            fsm.dismiss()
        compare(fsm.opened, false)
        adjustmentsRequests = []
    }

    function test_opensOnTheButtonTab_data() {
        return [
            { tag: "audio", tab: TrackMenuFSM.Audio },
            { tag: "subtitles", tab: TrackMenuFSM.Subtitles },
        ]
    }

    function test_opensOnTheButtonTab(data) {
        mouseClick(data.tab)
        compare(fsm.opened, true)
        compare(fsm.tab, data.tab)
    }

    function test_videoTabReachableFromTheTabs() {
        mouseClick(TrackMenuFSM.Audio)
        fsm.selectTab(TrackMenuFSM.Video)
        compare(fsm.opened, true)
        compare(fsm.tab, TrackMenuFSM.Video)
    }

    function test_staysOpenWhilePickingTracks() {
        mouseClick(TrackMenuFSM.Subtitles)
        fsm.trackPicked()
        fsm.trackPicked()
        compare(fsm.opened, true)
        compare(fsm.tab, TrackMenuFSM.Subtitles)
    }

    function test_outsideClickOrEscCloses() {
        mouseClick(TrackMenuFSM.Subtitles)
        fsm.dismiss()
        compare(fsm.opened, false)
    }

    function test_sameButtonCloses_data() {
        return [
            { tag: "mouse", click: mouseClick },
            { tag: "keyboard", click: keyClick },
        ]
    }

    function test_sameButtonCloses(data) {
        data.click(TrackMenuFSM.Audio)
        compare(fsm.opened, true)
        data.click(TrackMenuFSM.Audio)
        compare(fsm.opened, false)
        // and opens it again
        data.click(TrackMenuFSM.Audio)
        compare(fsm.opened, true)
    }

    function test_otherButtonSwitchesTab_data() {
        return [
            { tag: "mouse", click: mouseClick },
            { tag: "keyboard", click: keyClick },
        ]
    }

    function test_otherButtonSwitchesTab(data) {
        data.click(TrackMenuFSM.Audio)
        data.click(TrackMenuFSM.Subtitles)
        compare(fsm.opened, true)
        compare(fsm.tab, TrackMenuFSM.Subtitles)
    }

    function test_sameButtonAfterSwitchingTabReopensOnIt() {
        mouseClick(TrackMenuFSM.Audio)
        fsm.selectTab(TrackMenuFSM.Subtitles)
        // the audio button no longer matches the shown tab
        mouseClick(TrackMenuFSM.Audio)
        compare(fsm.opened, true)
        compare(fsm.tab, TrackMenuFSM.Audio)
    }

    function test_pressReleasedElsewhereDoesNotSwallowTheNextClick() {
        mouseClick(TrackMenuFSM.Audio)
        // press on the button, release away from it: no click
        fsm.buttonPressed(TrackMenuFSM.Audio)
        fsm.dismiss()
        compare(fsm.opened, false)

        mouseClick(TrackMenuFSM.Audio)
        compare(fsm.opened, true)
    }

    function test_moreSettingsOpensAdjustmentsOnTheSameTab_data() {
        return [
            { tag: "video", tab: TrackMenuFSM.Video },
            { tag: "audio", tab: TrackMenuFSM.Audio },
            { tag: "subtitles", tab: TrackMenuFSM.Subtitles },
        ]
    }

    function test_moreSettingsOpensAdjustmentsOnTheSameTab(data) {
        mouseClick(TrackMenuFSM.Audio)
        fsm.selectTab(data.tab)
        fsm.openMoreSettings()
        compare(fsm.opened, false)
        compare(adjustmentsRequests, [data.tab])
    }

    function test_closedIgnoresMenuEvents() {
        fsm.trackPicked()
        fsm.selectTab(TrackMenuFSM.Video)
        fsm.openMoreSettings()
        compare(fsm.opened, false)
        compare(adjustmentsRequests, [])
    }
}
