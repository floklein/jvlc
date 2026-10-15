/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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
import QtQuick.Controls.Basic


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

Widgets.IconToolButton {
    id: control

    // Aliases

    property alias popup: popup

    // Signals

    signal requestLockUnlockAutoHide(bool lock)
    signal menuOpened(var menu)

    // Settings

    color: (popup.visible) ? control.colorContext.accent : control.colorContext.fg.primary

    // FIXME: We can't use upItem because a Popup is not an Item.
    Navigation.upAction: function() {
        if (popup.visible) {
            popup.forceActiveFocus(Qt.TabFocusReason)

            return
        }

        const parent = Navigation.parentItem;

        if (parent)
            parent.Navigation.defaultNavigationUp()
    }

    // the popup opens above the bar that holds the button (an ancestor named
    // "controlBar", from the top of its background), not over it
    property real _barOffset: 0

    function _barTop() : real {
        for (let item = control.parent; item; item = item.parent) {
            if (item.objectName === "controlBar")
                return control.mapFromItem(item, 0, item.topInset ?? 0).y
        }
        return 0
    }

    // Events

    onClicked: popup.open()

    // Children

    // A Dialog rather than a Popup: screen readers read it as a dialog named
    // by its title. QML can't name the item of a Popup.
    Dialog {
        id: popup

        x: (parent.width - width) / 2
        y: control._barOffset - height - VLCStyle.margin_xxxsmall

        onAboutToShow: control._barOffset = Math.min(control._barTop(), 0)

        // the accessible name only, it shows no header
        title: control.description
        header: null
        footer: null

        padding: VLCStyle.margin_small

        focus: true

        // This popup should not exceed the boundaries of the scene.
        // Setting margins to >=0 makes it sure that this is satisfied.
        margins: MainCtx.windowExtendedMargin

        // Events

        onOpened: {
            control.requestLockUnlockAutoHide(true)

            control.menuOpened(popup)
        }

        onClosed: {
            control.requestLockUnlockAutoHide(false)

            control.forceActiveFocus()

            control.menuOpened(null)
        }

        // NOTE: The opacity should be stronger on a light background for readability.
        background: Widgets.TintBackground {
            colorContext: ColorContext {
                palette: control.colorContext.palette
                colorSet: ColorContext.Window
            }

            radius: VLCStyle.dp(8, VLCStyle.scale)
        }
    }

    function forceUnlock () {
        popup.close()
    }
}
