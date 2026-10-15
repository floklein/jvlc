
/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQml

import VLC.MainInterface
import VLC.Menus

// @brief - a class that can be used to create native menus with support
// to asyncronously retreive data from MLBaseModel like model

QtObject {
    id: root

    /**
      actions - list of action to show

      each action is an Object and contains following keys
        "text" - (string) display text of the action

        "action" - (function(dataList, options, indexes)) a function that will be called when action is selected

        "visible" - (Boolean or function(dataList,options, indexes) -> Boolean) (optional) a boolean value or function which
                    controls whether the action is shown in the menu following 'popup' call

        "children" - (function(dataList, options, indexes) -> [{text, action, checked} or {separator: true}]) (optional)
                     makes the action a submenu of these items, in place of "action"; a child
                     with a "checked" boolean has a check mark when it is true

        "separator" - (Boolean) (optional) the entry is a line between groups; a line is never
                      shown first, last or next to another one

        "icon" - ([base, accent]) (optional) the two-tone icon of the entry: VLCIcons glyphs,
                 either one may be ""

       e.g see MLContextMenu.qml
    **/

    property var actions: []

    property alias visible: menu.visible

    readonly property int currentRequest: _currentRequest

    signal requestData(var requestID, var indexes)


    property var _options: null

    property var _indexes: []

    property var _dataList: null

    property int _currentRequest: 0

    property var _effectiveActions: null

    property point _popupPoint

    // requests data for '_indexes' and shows popup menu
    // with 'NativeMenu::actions' based on '_options'
    // returns unique request id
    function popup(_indexes, point, _options) {
        root._options = _options
        root._indexes = _indexes
        _dataList = null
        _popupPoint = point

        const requestID = ++_currentRequest
        requestData(requestID, _indexes)
        return requestID
    }

    function setData(id, data) {
        if (_currentRequest !== id)
            return;

        _dataList = data

        const textStrings = []
        _effectiveActions = []
        for (let i in actions) {
            const action = actions[i]

            if (!action.hasOwnProperty("visible")
                    || (typeof action.visible === "boolean" && action.visible)
                    || (typeof action.visible === "function" && action.visible(_dataList, _options, _indexes))) {
                if (action.separator === true) {
                    // no line first or after another one
                    if (_effectiveActions.length > 0
                            && _effectiveActions[_effectiveActions.length - 1].separator !== true) {
                        _effectiveActions.push(action)
                        textStrings.push("")
                    }
                    continue
                }

                if (typeof action.children === "function") {
                    // a submenu: [{text, action, checked} or {separator: true}]
                    const children = action.children(_dataList, _options, _indexes)
                    _effectiveActions.push({ "text": action.text, "children": children })
                    textStrings.push({
                        "text": action.text,
                        "icon": action.icon ?? [],
                        "children": children.map((child) => {
                            if (child.separator)
                                return ""
                            if (typeof child.checked === "boolean")
                                return { "text": child.text, "checked": child.checked }
                            return child.text
                        })
                    })
                    continue
                }

                _effectiveActions.push(action)
                textStrings.push(Array.isArray(action.icon) ? { "text": action.text, "icon": action.icon }
                                                            : action.text)
            }
        }

        // no line last
        if (_effectiveActions.length > 0
                && _effectiveActions[_effectiveActions.length - 1].separator === true) {
            _effectiveActions.pop()
            textStrings.pop()
        }

        menu.popup(_popupPoint, textStrings)
    }

    function _executeAction(index) {
        const action = root._effectiveActions[index]
        action.action(_dataList, _options, _indexes)
    }

    function _executeChildAction(index, childIndex) {
        const child = root._effectiveActions[index].children[childIndex]
        child.action(_dataList, _options, _indexes)
    }


    readonly property StringListMenu _menu: StringListMenu {
        id: menu

        ctx: MainCtx

        onSelected: (index, _) => {
            root._executeAction(index)
        }

        onChildSelected: (index, childIndex) => {
            root._executeChildAction(index, childIndex)
        }
    }
}
