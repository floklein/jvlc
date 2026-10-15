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

// Hovering a zoom, aspect ratio or crop choice applies it until the list
// closes (spec #30, ticket #52)
Item {
    TestCase {
        id: root
        name: "ChoicePreview"

        Component {
            id: previewComponent

            ChoicePreview { }
        }

        property ChoicePreview preview: null
        // the rows applied to the player, in order
        property var applied: []

        function init() {
            applied = []
            preview = createTemporaryObject(previewComponent, root)
            verify(preview)
            preview.applyRequested.connect((row) => { root.applied = root.applied.concat([row]) })
        }

        function test_hoveringAppliesTheChoice() {
            preview.begin(2)
            preview.hover(4)
            compare(applied, [4])
            compare(preview.previewing, true)
        }

        function test_hoveringTheSameChoiceTwiceAppliesItOnce() {
            preview.begin(2)
            preview.hover(4)
            preview.hover(4)
            compare(applied, [4])
        }

        function test_hoveringTheCurrentChoiceAppliesNothing() {
            preview.begin(2)
            preview.hover(2)
            compare(applied, [])
        }

        function test_closingWithoutPickingRestoresTheChoice() {
            preview.begin(2)
            preview.hover(4)
            preview.hover(5)
            preview.cancel()
            compare(applied, [4, 5, 2])
            compare(preview.previewing, false)
        }

        function test_closingWithoutHoveringAppliesNothing() {
            preview.begin(2)
            preview.cancel()
            compare(applied, [])
        }

        function test_pickingTheHoveredChoiceKeepsIt() {
            preview.begin(2)
            preview.hover(4)
            preview.commit(4)
            preview.cancel() // the list closes after the pick
            compare(applied, [4])
        }

        function test_pickingAnotherChoiceAppliesIt() {
            preview.begin(2)
            preview.hover(4)
            preview.commit(1)
            compare(applied, [4, 1])
        }

        function test_hoveringOutsideAPreviewAppliesNothing() {
            preview.hover(4)
            preview.cancel()
            compare(applied, [])
        }

        // no current choice to go back to (a custom crop)
        function test_noChoiceToRestoreLeavesThePreview() {
            preview.begin(-1)
            preview.hover(3)
            preview.cancel()
            compare(applied, [3])
        }
    }
}
