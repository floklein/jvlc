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

// A single click on the video pauses, a double click toggles fullscreen:
// the single click must wait out the double-click interval.
TestCase {
    id: root

    name: "ClickGesture"

    ClickGesture {
        id: gesture

        interval: 100
        dragThreshold: 10
    }

    SignalSpy {
        id: spy

        target: gesture
        signalName: "singleClicked"
    }

    function init() {
        gesture.enabled = true
        gesture.cancel()
        spy.clear()
    }

    function click(x, y) {
        gesture.move(x, y)
        gesture.press()
        gesture.release()
    }

    // waits long enough for a pending single click to fire
    function settle() {
        wait(gesture.interval * 3)
    }

    function test_singleClickWaitsForTheDoubleClickInterval() {
        click(10, 10)
        compare(spy.count, 0)
        tryCompare(spy, "count", 1)
        settle()
        compare(spy.count, 1)
    }

    function test_doubleClickIsNotASingleClick() {
        click(10, 10)
        click(10, 10)
        settle()
        compare(spy.count, 0)
    }

    function test_doubleClickEventCancelsTheSingleClick() {
        click(10, 10)
        gesture.press()
        gesture.doubleClick()
        gesture.release()
        settle()
        compare(spy.count, 0)
    }

    function test_twoSlowClicksAreTwoSingleClicks() {
        click(10, 10)
        tryCompare(spy, "count", 1)
        click(10, 10)
        tryCompare(spy, "count", 2)
    }

    function test_dragIsNotAClick() {
        gesture.move(10, 10)
        gesture.press()
        gesture.move(40, 10)
        gesture.release()
        settle()
        compare(spy.count, 0)
    }

    function test_jitterIsStillAClick() {
        gesture.move(10, 10)
        gesture.press()
        gesture.move(13, 12)
        gesture.release()
        tryCompare(spy, "count", 1)
    }

    function test_releaseWithoutPressIsNotAClick() {
        gesture.move(10, 10)
        gesture.release()
        settle()
        compare(spy.count, 0)
    }

    function test_disabledIgnoresClicks() {
        gesture.enabled = false
        click(10, 10)
        settle()
        compare(spy.count, 0)
    }

    function test_disablingCancelsAPendingClick() {
        click(10, 10)
        gesture.enabled = false
        settle()
        compare(spy.count, 0)
    }
}
