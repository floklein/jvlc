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
 * Track menu state machine: whether the Track menu is open and on which tab
 *
 * The audio and subtitles buttons of the Control bar open it on their tab.
 * Picking a track keeps it open, a click outside or Esc close it. The popover
 * closes on a release outside, so a press on the button of the open tab marks
 * that its click closes rather than reopens the menu.
 *
 * @startuml
 * [*] --> Closed
 * Closed --> Opened : buttonClicked(tab)
 * Opened --> Opened : buttonClicked(other tab) / selectTab(tab) / trackPicked
 * Opened --> Closed : buttonClicked(same tab) / dismiss / openMoreSettings
 * @enduml
 */
FSM {
    id: fsm

    enum Tab {
        Video,
        Audio,
        Subtitles
    }

    //incoming signals

    //a bar button that opens the Track menu on its tab
    signal buttonPressed(int tab)
    signal buttonClicked(int tab)
    //a tab of the Track menu
    signal selectTab(int tab)
    //a track or an output device was picked
    signal trackPicked()
    //Esc or a click outside
    signal dismiss()
    //"More settings"
    signal openMoreSettings()

    //outgoing signals

    //open the Adjustments sidebar on the given tab
    signal requestAdjustments(int tab)

    //exposed internal states
    property alias opened: fsmOpened.active
    property int tab: TrackMenuFSM.Tab.Subtitles

    //private

    //the click of the pressed button closes the menu
    property bool _closeOnClick: false

    initialState: fsmClosed

    signalMap: ({
        buttonPressed: fsm.buttonPressed,
        buttonClicked: fsm.buttonClicked,
        selectTab: fsm.selectTab,
        trackPicked: fsm.trackPicked,
        dismiss: fsm.dismiss,
        openMoreSettings: fsm.openMoreSettings,
    })

    FSMState {
        id: fsmClosed

        transitions: ({
            //a press released elsewhere left no click
            buttonPressed: {
                action: (tab) => { fsm._closeOnClick = false }
            },
            buttonClicked: [{
                guard: (tab) => fsm._closeOnClick,
                action: (tab) => { fsm._closeOnClick = false }
            }, {
                action: (tab) => { fsm.tab = tab },
                target: fsmOpened
            }],
        })
    }

    FSMState {
        id: fsmOpened

        function enter() {
            fsm._closeOnClick = false
        }

        transitions: ({
            buttonPressed: {
                guard: (tab) => tab === fsm.tab,
                action: (tab) => { fsm._closeOnClick = true }
            },
            buttonClicked: [{
                guard: (tab) => tab === fsm.tab,
                action: (tab) => { fsm._closeOnClick = false },
                target: fsmClosed
            }, {
                action: (tab) => { fsm.tab = tab }
            }],
            selectTab: {
                action: (tab) => { fsm.tab = tab }
            },
            dismiss: {
                target: fsmClosed
            },
            openMoreSettings: {
                action: () => { fsm.requestAdjustments(fsm.tab) },
                target: fsmClosed
            },
        })
    }
}
