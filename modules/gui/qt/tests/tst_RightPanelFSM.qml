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

// The right-hand slot of the player: the Queue panel and the Adjustments
// sidebar share it, only one of them is open at a time (spec #30, ticket #50)
Item {
    TestCase {
        id: root
        name: "RightPanelFSM"

        Component {
            id: fsmComponent

            RightPanelFSM { }
        }

        property RightPanelFSM fsm: null
        property int queueCloseRequests: 0

        // the Queue panel of the player, as Player.qml drives it
        function openQueue() {
            fsm.queueVisible = true
            fsm.queueOpened()
        }

        function init() {
            queueCloseRequests = 0
            fsm = createTemporaryObject(fsmComponent, root)
            verify(fsm)
            fsm.requestCloseQueue.connect(() => {
                root.queueCloseRequests++
                fsm.queueVisible = false
            })
        }

        function test_closedAtFirst() {
            compare(fsm.adjustmentsOpened, false)
        }

        function test_adjustmentsButtonOpensAndClosesTheSidebar() {
            fsm.toggleAdjustments()
            compare(fsm.adjustmentsOpened, true)

            fsm.toggleAdjustments()
            compare(fsm.adjustmentsOpened, false)
        }

        function test_firstOpenShowsThePlaybackTab() {
            fsm.toggleAdjustments()
            compare(fsm.currentTab, RightPanelFSM.Playback)
        }

        function test_openingTheSidebarClosesTheQueue() {
            openQueue()

            fsm.toggleAdjustments()

            compare(fsm.adjustmentsOpened, true)
            compare(queueCloseRequests, 1)
            compare(fsm.queueVisible, false)
        }

        function test_openingTheQueueClosesTheSidebar() {
            fsm.toggleAdjustments()

            openQueue()

            compare(fsm.adjustmentsOpened, false)
            compare(queueCloseRequests, 0)
        }

        function test_neverBothOpen() {
            const steps = [
                () => fsm.toggleAdjustments(),
                () => openQueue(),
                () => fsm.openAdjustments(RightPanelFSM.Audio),
                () => openQueue(),
                () => fsm.toggleAdjustments(),
                () => fsm.toggleAdjustments(),
                () => openQueue(),
            ]
            for (const step of steps) {
                step()
                verify(!(fsm.adjustmentsOpened && fsm.queueVisible))
            }
        }

        function test_closingTheSidebarLeavesTheQueueClosed() {
            openQueue()
            fsm.toggleAdjustments()
            fsm.closeAdjustments()

            compare(fsm.adjustmentsOpened, false)
            compare(fsm.queueVisible, false)
        }

        function test_remembersTheLastTab() {
            fsm.toggleAdjustments()
            fsm.selectTab(RightPanelFSM.Subtitles)
            fsm.closeAdjustments()

            fsm.toggleAdjustments()
            compare(fsm.currentTab, RightPanelFSM.Subtitles)
        }

        function test_moreSettingsOpensOnItsTab_data() {
            return [
                { tag: "closed", opened: false },
                { tag: "opened", opened: true },
            ]
        }

        function test_moreSettingsOpensOnItsTab(data) {
            if (data.opened)
                fsm.toggleAdjustments()

            fsm.openAdjustments(RightPanelFSM.Audio)

            compare(fsm.adjustmentsOpened, true)
            compare(fsm.currentTab, RightPanelFSM.Audio)
        }

        function test_musicShowsOnlyPlaybackAndAudio() {
            fsm.music = true
            compare(fsm.tabs, [RightPanelFSM.Playback, RightPanelFSM.Audio])

            fsm.music = false
            compare(fsm.tabs, [RightPanelFSM.Playback, RightPanelFSM.Video,
                               RightPanelFSM.Audio, RightPanelFSM.Subtitles])
        }

        function test_musicFallsBackToPlaybackForVideoTabs() {
            fsm.openAdjustments(RightPanelFSM.Subtitles)

            fsm.music = true
            compare(fsm.currentTab, RightPanelFSM.Playback)

            // the video tab comes back with a video
            fsm.music = false
            compare(fsm.currentTab, RightPanelFSM.Subtitles)
        }
    }
}
