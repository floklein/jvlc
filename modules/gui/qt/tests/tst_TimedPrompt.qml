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

// The continue-watching pill shows for about 10 s, but never runs out while
// the viewer points at it or has it focused (WCAG 2.2.1, timing adjustable).
TestCase {
    name: "TimedPrompt"

    TimedPrompt {
        id: prompt

        duration: 100
    }

    SignalSpy {
        id: expiredSpy

        target: prompt
        signalName: "expired"
    }

    function init() {
        prompt.held = false
        prompt.hide()
        expiredSpy.clear()
    }

    // waits longer than the prompt's duration
    function outlive() {
        wait(prompt.duration * 3)
    }

    function test_hiddenByDefault() {
        compare(prompt.shown, false)
    }

    function test_expiresAfterItsDuration() {
        prompt.show()
        compare(prompt.shown, true)
        tryCompare(prompt, "shown", false)
        compare(expiredSpy.count, 1)
    }

    function test_hideDoesNotExpire() {
        prompt.show()
        prompt.hide()
        compare(prompt.shown, false)
        outlive()
        compare(expiredSpy.count, 0)
    }

    function test_heldPromptStays() {
        prompt.show()
        prompt.held = true
        outlive()
        compare(prompt.shown, true)
        compare(expiredSpy.count, 0)
    }

    function test_releasedPromptGetsAFullDurationAgain() {
        prompt.show()
        prompt.held = true
        outlive()
        prompt.held = false
        compare(prompt.shown, true)
        tryCompare(prompt, "shown", false)
        compare(expiredSpy.count, 1)
    }

    function test_heldBeforeShowingStays() {
        prompt.held = true
        prompt.show()
        outlive()
        compare(prompt.shown, true)
    }

    function test_releasingAHiddenPromptDoesNotShowIt() {
        prompt.held = true
        prompt.held = false
        outlive()
        compare(prompt.shown, false)
        compare(expiredSpy.count, 0)
    }
}
