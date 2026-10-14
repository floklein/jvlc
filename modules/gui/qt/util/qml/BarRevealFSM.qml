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

import VLC.Util

/**
 * Visibility of the player's Top bar and Control bar
 *
 * Docked (window, music): the bars are always shown.
 *
 * Overlay (fullscreen, Hide UI): the bars are hidden. They are revealed
 * `showDelay` ms after the pointer enters the edge zone, and hidden `hideDelay` ms
 * after it leaves it. Keyboard and state changes reveal them instantly. They stay
 * while paused or locked (hovered controls, open menus, the Queue shown).
 *
 * `animate` tells whether the last change of `isVisible` should be animated:
 * pointer reveals and hides slide, keyboard and state changes are instant.
 *
 * The edge zone itself is computed by the view, which reports each pointer move
 * with `pointerMoved(inEdgeZone)`.
 *
 * @startuml
 * state Docked {
 * }
 * state Overlay {
 *    state Shown {
 *        state Held {
 *        }
 *        state PointerRevealed {
 *        }
 *        state KeyboardRevealed {
 *        }
 *        state Leaving {
 *        }
 *    }
 *    state Hidden {
 *        state Idle {
 *        }
 *        state Pending {
 *        }
 *    }
 * }
 * @enduml
 */
FSM {
    id: fsm

    // Inputs

    // The bars float over a video (fullscreen or Hide UI)
    property bool overlay: false

    property bool paused: false

    // Disc menus use the pointer, it does not reveal the bars
    property bool interactive: false

    property int showDelay: 80
    property int hideDelay: 300
    property int keyboardHideDelay: 3000
    property int pointerHideDelay: 2000

    // Outputs

    readonly property bool isVisible: fsmDocked.active || fsmShown.active

    property bool animate: false

    // The pointer is idle over the video and should be hidden
    readonly property bool pointerHidden: overlay && _pointerIdle

    property bool pointerInZone: false

    property int lockCount: 0

    property bool _pointerIdle: false

    // Incoming signals

    signal pointerMoved(bool inZone)
    // Tab or F6
    signal keyboardReveal()
    // any other key
    signal keyboardMove()
    // toggles the bars (libvlc intf-toggle-fscontrol)
    signal askShow()
    // Esc
    signal dismiss()
    signal lock()
    signal unlock()

    signal updateOverlay()
    signal updatePaused()

    signal showTimeout()
    signal hideTimeout()
    signal keyboardTimeout()

    // Outgoing signals

    // all locks are dropped: menus and popups should close
    signal forceUnlock()

    initialState: overlay ? fsmOverlay : fsmDocked

    signalMap: ({
        pointerMoved: fsm.pointerMoved,
        keyboardReveal: fsm.keyboardReveal,
        keyboardMove: fsm.keyboardMove,
        askShow: fsm.askShow,
        dismiss: fsm.dismiss,
        lock: fsm.lock,
        unlock: fsm.unlock,
        updateOverlay: fsm.updateOverlay,
        updatePaused: fsm.updatePaused,
        showTimeout: fsm.showTimeout,
        hideTimeout: fsm.hideTimeout,
        keyboardTimeout: fsm.keyboardTimeout,
    })

    onOverlayChanged: {
        _wakePointer()
        fsm.updateOverlay()
    }

    onPausedChanged: fsm.updatePaused()

    onPointerMoved: (inZone) => {
        fsm.pointerInZone = inZone
        _wakePointer()
    }

    function _wakePointer() {
        fsm._pointerIdle = false
        pointerTimer.restart()
    }

    function _instant() {
        fsm.animate = false
    }

    function _animated() {
        fsm.animate = true
    }

    function _lock() {
        fsm.lockCount++
    }

    function _unlock() {
        if (fsm.lockCount > 0)
            fsm.lockCount--
    }

    FSMState {
        id: fsmDocked

        transitions: ({
            updateOverlay: {
                guard: () => fsm.overlay,
                action: fsm._instant,
                target: fsmOverlay
            },
            lock: {
                action: fsm._lock
            },
            unlock: {
                action: fsm._unlock
            }
        })
    }

    FSMState {
        id: fsmOverlay

        initialState: (fsm.paused || fsm.lockCount > 0) ? fsmShown : fsmHidden

        transitions: ({
            updateOverlay: {
                guard: () => !fsm.overlay,
                action: fsm._instant,
                target: fsmDocked
            }
        })

        FSMState {
            id: fsmShown

            // only entered by itself when paused or locked
            initialState: fsmHeld

            transitions: ({
                lock: {
                    action: fsm._lock,
                    target: fsmHeld
                },
                updatePaused: {
                    guard: () => fsm.paused,
                    target: fsmHeld
                },
                keyboardReveal: {
                    guard: () => !fsm.interactive,
                    target: fsmKeyboardRevealed
                },
                askShow: {
                    action: fsm._instant,
                    target: fsmHidden
                },
                dismiss: {
                    action: fsm._instant,
                    target: fsmHidden
                }
            })

            // paused or locked
            FSMState {
                id: fsmHeld

                transitions: ({
                    lock: {
                        action: fsm._lock
                    },
                    unlock: [{
                        guard: () => fsm.lockCount > 1 || fsm.paused,
                        action: fsm._unlock
                    }, {
                        guard: () => fsm.pointerInZone,
                        action: fsm._unlock,
                        target: fsmPointerRevealed
                    }, {
                        action: fsm._unlock,
                        target: fsmLeaving
                    }],
                    updatePaused: [{
                        guard: () => fsm.paused || fsm.lockCount > 0
                    }, {
                        guard: () => fsm.pointerInZone,
                        target: fsmPointerRevealed
                    }, {
                        target: fsmLeaving
                    }],
                    // the bars stay
                    keyboardReveal: {},
                    askShow: {},
                    dismiss: {}
                })
            }

            FSMState {
                id: fsmPointerRevealed

                transitions: ({
                    pointerMoved: {
                        guard: (inZone) => !inZone,
                        target: fsmLeaving
                    }
                })
            }

            FSMState {
                id: fsmKeyboardRevealed

                function enter() {
                    keyboardTimer.restart()
                }

                function exit() {
                    keyboardTimer.stop()
                }

                transitions: ({
                    keyboardReveal: {
                        target: fsmKeyboardRevealed
                    },
                    keyboardMove: {
                        target: fsmKeyboardRevealed
                    },
                    pointerMoved: {
                        guard: (inZone) => inZone,
                        target: fsmPointerRevealed
                    },
                    keyboardTimeout: {
                        action: fsm._animated,
                        target: fsmHidden
                    }
                })
            }

            FSMState {
                id: fsmLeaving

                function enter() {
                    hideTimer.restart()
                }

                function exit() {
                    hideTimer.stop()
                }

                transitions: ({
                    pointerMoved: {
                        guard: (inZone) => inZone,
                        target: fsmPointerRevealed
                    },
                    hideTimeout: {
                        action: fsm._animated,
                        target: fsmHidden
                    }
                })
            }
        }

        FSMState {
            id: fsmHidden

            initialState: fsmIdle

            function enter() {
                fsm.lockCount = 0
                fsm.forceUnlock()
            }

            transitions: ({
                lock: {
                    action: () => { fsm._lock(); fsm._instant() },
                    target: fsmHeld
                },
                updatePaused: {
                    guard: () => fsm.paused,
                    action: fsm._instant,
                    target: fsmHeld
                },
                keyboardReveal: {
                    guard: () => !fsm.interactive,
                    action: fsm._instant,
                    target: fsmKeyboardRevealed
                },
                askShow: {
                    action: fsm._instant,
                    target: fsmKeyboardRevealed
                }
            })

            FSMState {
                id: fsmIdle

                transitions: ({
                    pointerMoved: {
                        guard: (inZone) => inZone && !fsm.interactive,
                        target: fsmPending
                    }
                })
            }

            FSMState {
                id: fsmPending

                function enter() {
                    showTimer.restart()
                }

                function exit() {
                    showTimer.stop()
                }

                transitions: ({
                    pointerMoved: {
                        guard: (inZone) => !inZone,
                        target: fsmIdle
                    },
                    showTimeout: {
                        action: fsm._animated,
                        target: fsmPointerRevealed
                    }
                })
            }
        }
    }

    Timer {
        id: showTimer
        interval: fsm.showDelay
        repeat: false
        onTriggered: fsm.showTimeout()
    }

    Timer {
        id: hideTimer
        interval: fsm.hideDelay
        repeat: false
        onTriggered: fsm.hideTimeout()
    }

    Timer {
        id: keyboardTimer
        interval: fsm.keyboardHideDelay
        repeat: false
        onTriggered: fsm.keyboardTimeout()
    }

    Timer {
        id: pointerTimer
        interval: fsm.pointerHideDelay
        repeat: false
        onTriggered: fsm._pointerIdle = true
    }
}
