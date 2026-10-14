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

// Reveal and hide of the Top bar and Control bar (spec #30, ticket #36)
Item {
    TestCase {
        id: root
        name: "BarRevealFSM"

        Component {
            id: fsmComponent

            BarRevealFSM { }
        }

        property BarRevealFSM fsm: null

        function init() {
            fsm = createTemporaryObject(fsmComponent, root)
            verify(fsm)
        }

        // overlay = fullscreen or Hide UI over a video
        function makeOverlay() {
            fsm.overlay = true
            verify(!fsm.isVisible)
        }

        function test_defaultTimings() {
            compare(fsm.showDelay, 80)
            compare(fsm.hideDelay, 300)
            compare(fsm.pointerHideDelay, 2000)
        }

        function test_dockedBarsStayVisible() {
            verify(fsm.isVisible)

            fsm.pointerMoved(false)
            wait(fsm.hideDelay * 2)
            verify(fsm.isVisible)
            verify(!fsm.pointerHidden)
        }

        function test_pointerNearEdgeRevealsAfterShowDelay() {
            makeOverlay()

            fsm.pointerMoved(true)
            // not instantly
            verify(!fsm.isVisible)

            tryCompare(fsm, "isVisible", true)
            // the pointer reveal slides in
            verify(fsm.animate)
        }

        function test_pointerPassingThroughEdgeDoesNotReveal() {
            makeOverlay()

            fsm.pointerMoved(true)
            fsm.pointerMoved(false)

            wait(fsm.showDelay * 3)
            verify(!fsm.isVisible)
        }

        function test_pointerAwayFromEdgeHidesAfterHideDelay() {
            makeOverlay()
            fsm.pointerMoved(true)
            tryCompare(fsm, "isVisible", true)

            fsm.pointerMoved(false)
            // not instantly
            verify(fsm.isVisible)

            tryCompare(fsm, "isVisible", false)
            verify(fsm.animate)
        }

        function test_pointerBackToEdgeKeepsBars() {
            makeOverlay()
            fsm.pointerMoved(true)
            tryCompare(fsm, "isVisible", true)

            fsm.pointerMoved(false)
            fsm.pointerMoved(true)

            wait(fsm.hideDelay * 2)
            verify(fsm.isVisible)
        }

        function test_interactiveMenusDoNotRevealOnPointer() {
            fsm.interactive = true
            makeOverlay()

            fsm.pointerMoved(true)
            wait(fsm.showDelay * 3)
            verify(!fsm.isVisible)
        }

        // pause

        function test_pauseRevealsInstantly() {
            makeOverlay()

            fsm.paused = true
            verify(fsm.isVisible)
            verify(!fsm.animate)
        }

        function test_barsStayWhilePaused() {
            makeOverlay()
            fsm.paused = true

            fsm.pointerMoved(false)
            fsm.dismiss()
            fsm.askShow()
            wait(fsm.hideDelay * 2)
            verify(fsm.isVisible)
        }

        function test_resumeHidesAfterHideDelay() {
            makeOverlay()
            fsm.paused = true
            fsm.pointerMoved(false)

            fsm.paused = false
            verify(fsm.isVisible)
            tryCompare(fsm, "isVisible", false)
        }

        function test_resumeWithPointerAtEdgeKeepsBars() {
            makeOverlay()
            fsm.paused = true
            fsm.pointerMoved(true)

            fsm.paused = false
            wait(fsm.hideDelay * 2)
            verify(fsm.isVisible)
        }

        function test_startingPausedInOverlayShowsBars() {
            fsm.paused = true
            fsm.overlay = true
            verify(fsm.isVisible)
        }

        // keyboard

        function test_keyboardRevealsInstantly() {
            makeOverlay()

            fsm.keyboardReveal()
            verify(fsm.isVisible)
            verify(!fsm.animate)
        }

        function test_keyboardRevealIgnoresPointerAwayFromEdge() {
            makeOverlay()
            fsm.keyboardReveal()

            fsm.pointerMoved(false)
            wait(fsm.hideDelay * 2)
            verify(fsm.isVisible)
        }

        function test_keysKeepKeyboardRevealedBars() {
            fsm.keyboardHideDelay = 400
            makeOverlay()
            fsm.keyboardReveal()

            for (let i = 0; i < 4; ++i) {
                wait(200)
                fsm.keyboardMove()
                verify(fsm.isVisible)
            }

            tryCompare(fsm, "isVisible", false)
        }

        function test_escapeHidesInstantly() {
            makeOverlay()
            fsm.keyboardReveal()

            fsm.dismiss()
            verify(!fsm.isVisible)
            verify(!fsm.animate)
        }

        function test_askShowToggles() {
            makeOverlay()

            fsm.askShow()
            verify(fsm.isVisible)
            fsm.askShow()
            verify(!fsm.isVisible)
        }

        // locks: hovered controls, open menus, the Queue shown

        function test_lockKeepsBars() {
            makeOverlay()
            fsm.pointerMoved(true)
            tryCompare(fsm, "isVisible", true)

            fsm.lock()
            fsm.lock()
            fsm.pointerMoved(false)
            fsm.unlock()
            wait(fsm.hideDelay * 2)
            verify(fsm.isVisible)

            fsm.unlock()
            tryCompare(fsm, "isVisible", false)
        }

        function test_hidingClosesMenus() {
            makeOverlay()
            fsm.keyboardReveal()

            let forced = 0
            fsm.forceUnlock.connect(() => { forced++ })
            fsm.dismiss()
            compare(forced, 1)
        }

        // Hide UI behaves like fullscreen in a window

        function test_hideUIHidesDockedBarsInstantly() {
            verify(fsm.isVisible)

            fsm.overlay = true
            verify(!fsm.isVisible)
            verify(!fsm.animate)

            fsm.overlay = false
            verify(fsm.isVisible)
            verify(!fsm.animate)
        }

        function test_leavingHideUIWhileLockedKeepsCount() {
            fsm.lock()
            fsm.overlay = true
            // the Queue is shown: the bars stay
            verify(fsm.isVisible)

            fsm.overlay = false
            fsm.unlock()
            compare(fsm.lockCount, 0)
            verify(fsm.isVisible)
        }

        // pointer

        function test_pointerHidesWhenIdleInOverlay() {
            makeOverlay()
            fsm.pointerHideDelay = 200

            fsm.pointerMoved(false)
            verify(!fsm.pointerHidden)
            tryCompare(fsm, "pointerHidden", true)

            fsm.pointerMoved(false)
            verify(!fsm.pointerHidden)
        }

        function test_pointerStaysWhenDocked() {
            fsm.pointerHideDelay = 100

            fsm.pointerMoved(false)
            wait(300)
            verify(!fsm.pointerHidden)
        }
    }

    TestCase {
        id: stateGroupCase
        name: "BarRevealStateGroup"

        Component {
            id: barComponent

            Item {
                id: barItem

                width: 100
                height: 40

                transform: reveal.translate

                property alias reveal: reveal

                BarRevealStateGroup {
                    id: reveal

                    target: barItem
                    edge: Qt.TopEdge
                    animate: false
                }
            }
        }

        property Item bar: null
        property BarRevealStateGroup reveal: null

        function init() {
            bar = createTemporaryObject(barComponent, stateGroupCase)
            verify(bar)
            reveal = bar.reveal
            bar.state = "visible"
        }

        function test_hiddenBarSlidesOutThroughItsEdge() {
            bar.state = "hidden"
            verify(!bar.visible)
            compare(bar.opacity, 0)
            compare(reveal.translate.y, -bar.height)

            reveal.edge = Qt.BottomEdge
            compare(reveal.translate.y, bar.height)
        }

        // NOTE: `visible` is the effective visibility, false here as the bar is
        //       in no window, so the shown state is checked through opacity.
        function test_visibleBarIsInPlace() {
            bar.state = "hidden"
            bar.state = "visible"
            compare(bar.opacity, 1)
            compare(reveal.translate.y, 0)
        }

        function test_reducedMotionOnlyFades() {
            reveal.reducedMotion = true
            bar.state = "hidden"
            compare(reveal.translate.y, 0)
            compare(bar.opacity, 0)
        }

        function test_slideTakes150ms() {
            compare(reveal.duration, 150)

            reveal.animate = true
            bar.state = "hidden"
            // animated: still on screen
            verify(bar.opacity > 0)
            verify(reveal.translate.y > -bar.height)

            tryCompare(reveal.translate, "y", -bar.height)
            tryCompare(bar, "opacity", 0)
        }
    }
}
