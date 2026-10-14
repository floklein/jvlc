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
 * The right-hand slot of the player: whether the Adjustments sidebar is open,
 * on which tab, and its exclusivity with the Queue panel
 *
 * The Queue panel and the Adjustments sidebar share the slot, only one of
 * them is open at a time: opening the sidebar asks to close the Queue panel,
 * opening the Queue panel closes the sidebar. The sidebar remembers its last
 * tab. Music only has the Playback and Audio tabs.
 *
 * @startuml
 * [*] --> Closed
 * Closed --> Opened : toggleAdjustments / openAdjustments(tab)
 * Opened --> Opened : openAdjustments(tab) / selectTab(tab)
 * Opened --> Closed : toggleAdjustments / closeAdjustments / queueOpened
 * @enduml
 */
FSM {
    id: fsm

    enum Tab {
        Playback,
        Video,
        Audio,
        Subtitles
    }

    //incoming signals

    //the Queue panel opened in the slot
    signal queueOpened()
    //the adjustments button, Ctrl+E and the menus
    signal toggleAdjustments()
    //"More settings" of the Track menu: open on a tab
    signal openAdjustments(int tab)
    //a tab of the sidebar
    signal selectTab(int tab)
    //Esc in the sidebar
    signal closeAdjustments()

    //outgoing signals

    //close the Queue panel to make room for the sidebar
    signal requestCloseQueue()

    //inputs

    //the Queue panel is open in the slot
    property bool queueVisible: false
    //the player plays music: no Video and Subtitles tabs
    property bool music: false

    //exposed internal states

    property alias adjustmentsOpened: fsmOpened.active
    //the last tab picked
    property int tab: RightPanelFSM.Tab.Playback

    readonly property var tabs: music ? [RightPanelFSM.Tab.Playback, RightPanelFSM.Tab.Audio]
                                      : [RightPanelFSM.Tab.Playback, RightPanelFSM.Tab.Video,
                                         RightPanelFSM.Tab.Audio, RightPanelFSM.Tab.Subtitles]

    //the tab shown: music falls back to Playback for the video tabs
    readonly property int currentTab: tabs.indexOf(tab) >= 0 ? tab : RightPanelFSM.Tab.Playback

    initialState: fsmClosed

    signalMap: ({
        queueOpened: fsm.queueOpened,
        toggleAdjustments: fsm.toggleAdjustments,
        openAdjustments: fsm.openAdjustments,
        selectTab: fsm.selectTab,
        closeAdjustments: fsm.closeAdjustments,
    })

    FSMState {
        id: fsmClosed

        transitions: ({
            toggleAdjustments: {
                target: fsmOpened
            },
            openAdjustments: {
                action: (tab) => { fsm.tab = tab },
                target: fsmOpened
            },
        })
    }

    FSMState {
        id: fsmOpened

        function enter() {
            if (fsm.queueVisible)
                fsm.requestCloseQueue()
        }

        transitions: ({
            toggleAdjustments: {
                target: fsmClosed
            },
            openAdjustments: {
                action: (tab) => { fsm.tab = tab }
            },
            selectTab: {
                action: (tab) => { fsm.tab = tab }
            },
            closeAdjustments: {
                target: fsmClosed
            },
            queueOpened: {
                target: fsmClosed
            },
        })
    }
}
