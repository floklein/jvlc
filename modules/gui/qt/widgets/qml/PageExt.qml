/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Templates as T

import VLC.MainInterface
import VLC.Style
import VLC.Menus
import VLC.Util

T.Page {
    id: root

    //path of the current page loader
    property var pagePrefix: []

    //note that T.Page introduce an intermediate Item, so to access our delegate,
    //we need to use contentChildren[0] rather than contentItem
    readonly property Item _firstChild: contentChildren[0] ?? null

    //indicates whether the subview support grid/list mode
    property bool hasGridListMode: _firstChild?.hasGridListMode ?? false

    property bool isSearchable: _firstChild?.isSearchable ?? false

    property var sortModel: _firstChild?.sortModel ?? null

    //property is *not* readOnly, a PageLoader may define a localMenuDelegate common for its subviews (music, video)
    property SortMenu sortMenu: null

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    // the trail of folders leading to the page, as a list of strings ending
    // with the page itself; replaces the title when set
    property var breadcrumb: []

    // the number of items of the view, shown in the header when not negative
    property int itemCount: -1

    // buttons shown at the end of the header
    property Component headerActions: null

    readonly property SearchCtx search: _searchCtx
    readonly property SortCtx sort: _sortCtx

    // a crumb other than the last one was clicked
    signal breadcrumbClicked(int index, int reason)


    SearchCtx {
        id: _searchCtx
        available: root.isSearchable
    }

    SortCtx {
        id: _sortCtx

        readonly property string _sortCriteriaKey: ["sortCriteria", ...root.pagePrefix].join("/")
        readonly property string _sortOrderKey:  ["sortOrder", ...root.pagePrefix].join("/")

        available: Helpers.isArray(root.sortModel) && (root.sortModel).length > 0
        model: root.sortModel

        property bool _restored: false

        //restore the setting of the page, and save it as soon as it changes:
        //the next page of the same section (a subfolder) may be created
        //before this one is destroyed
        Component.onCompleted: {
            const criteria = MainCtx.settingValue(_sortCriteriaKey, undefined)
            if (criteria !== undefined)
                _sortCtx.criteria = criteria

            const order = MainCtx.settingValue(_sortOrderKey, undefined)
            if (order !== undefined)
                _sortCtx.order = parseInt(order)

            _restored = true
        }

        onCriteriaChanged: {
            if (_restored)
                MainCtx.setSettingValue(_sortCriteriaKey, _sortCtx.criteria)
        }

        onOrderChanged: {
            if (_restored)
                MainCtx.setSettingValue(_sortOrderKey, _sortCtx.order)
        }
    }

    Binding {
        target: MainCtx
        property: "hasGridListMode";
        value: root.hasGridListMode
    }

    // Ctrl+F searches the current view
    Connections {
        target: MainCtx.search

        function onAskShow() {
            if (root.visible && _searchCtx.available)
                _searchCtx.askShow()
        }
    }

    on_FirstChildChanged: {
        if (!_firstChild)
            return
        _firstChild.Navigation.parentItem = root
        _firstChild.Navigation.upItem = root.header
    }

    function positionContentAtBeginning() {
        _firstChild?.positionContentAtBeginning?.()
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    header: DefaultPageHeader {
        text: root.title

        breadcrumb: root.breadcrumb
        itemCount: root.itemCount
        actions: root.headerActions

        leftPadding: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_left_margin + root.leftPadding
        rightPadding: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_right_margin + root.rightPadding

        sortMenu: root.sortMenu

        search: _searchCtx
        sort: _sortCtx

        onBreadcrumbClicked: (index, reason) => root.breadcrumbClicked(index, reason)

        Navigation.parentItem: root
        Navigation.downAction: function() {
            if (typeof root.setCurrentItemFocus === "function")
                root.setCurrentItemFocus(Qt.TabFocusReason)
            else if (typeof root._firstChild?.setCurrentItemFocus === "function")
             root._firstChild.setCurrentItemFocus(Qt.TabFocusReason)
            else if (root._firstChild)
                root._firstChild.forceActiveFocus(Qt.TabFocusReason)
        }
    }

    component DefaultPageHeader: T.ToolBar {
        id: defaultPageHeader

        // Properties
        property alias text: label.text

        property var breadcrumb: []
        property int itemCount: -1
        property alias actions: actionsLoader.sourceComponent

        property alias sortMenu: gridSortFilter.sortMenu

        property alias search: gridSortFilter.search
        property alias sort: gridSortFilter.sort

        signal breadcrumbClicked(int index, int reason)

        position: T.ToolBar.Header

        // Figma 6261:15118: 20 px above the header, 17 px between it and the
        // divider that ends it
        topPadding: VLCStyle.pageHeader_topPadding
        bottomPadding: VLCStyle.pageHeader_bottomPadding

        // FIXME: `GridItem`'s background extends beyond its
        //        bounding rect, violating the hypothetical
        //        clip test (see 7e6b23db).
        bottomInset: (MainCtx.hasGridListMode && MainCtx.gridView) ? VLCStyle.gridItemSelectedBorder : undefined

        // Settings

        implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                implicitContentWidth + leftPadding + rightPadding)
        implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                                 implicitContentHeight + topPadding + bottomPadding)

        // the search field sets the height of the row (Figma 6261:15136)
        readonly property real _rowHeight: compact ? VLCStyle.icon_banner : VLCStyle.pageHeader_rowHeight

        Navigation.navigable: gridSortFilter.enabled

        // Children

        readonly property ColorContext colorContext: ColorContext {
            id: theme
            colorSet: ColorContext.View
        }

        // the colors of the header (Figma 6261:15118)
        ColorContext {
            id: navTheme
            colorSet: ColorContext.Navigation
        }

        background: Rectangle {
            // the Library pane draws its own background (the Tint over the video)
            color: defaultPageHeader.compact ? "transparent" : theme.bg.primary

            // the divider under the header, at its bottom even when the
            // background leaves room for the grid
            Rectangle {
                x: defaultPageHeader.leftPadding
                width: parent.width - defaultPageHeader.leftPadding - defaultPageHeader.rightPadding
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -(defaultPageHeader.bottomInset || 0)
                height: VLCStyle.pageHeader_dividerHeight

                color: navTheme.border
            }
        }

        // In the Library pane (Figma 187:328, 1106:5633), the header takes two
        // rows: the history, the location, the page actions and ↗ above; the
        // item count and the sort, view and search controls below.
        readonly property bool compact: MainCtx.libraryPane

        GridLayout {
            id: row

            anchors.fill: parent

            // Figma: 20 px between the groups of the header, 12 px in the Library pane
            columnSpacing: defaultPageHeader.compact ? VLCStyle.margin_small : VLCStyle.pageHeader_spacing
            rowSpacing: VLCStyle.pageHeader_spacing

            // back and forward through the history of the whole interface
            NavigableRow {
                id: historyButtons

                Layout.row: 0
                Layout.column: 0

                spacing: VLCStyle.margin_small

                Navigation.parentItem: defaultPageHeader
                Navigation.rightItem: crumbs.Navigation.navigable ? crumbs : gridSortFilter

                // Figma: a step that can't be taken is the icon at 30 %
                IconToolButton {
                    font.pixelSize: VLCStyle.icon_banner
                    width: VLCStyle.bannerButton_width
                    height: VLCStyle.bannerButton_height

                    color: enabled ? colorContext.fg.primary : navTheme.fg.primary
                    opacity: enabled ? 1.0 : VLCStyle.pageHeader_disabledOpacity

                    text: VLCIcons.arrow_left
                    description: qsTr("Back")

                    enabled: !History.previousEmpty

                    onClicked: History.previous(Qt.MouseFocusReason)
                }

                IconToolButton {
                    font.pixelSize: VLCStyle.icon_banner
                    width: VLCStyle.bannerButton_width
                    height: VLCStyle.bannerButton_height

                    color: enabled ? colorContext.fg.primary : navTheme.fg.primary
                    opacity: enabled ? 1.0 : VLCStyle.pageHeader_disabledOpacity

                    text: VLCIcons.arrow_right
                    description: qsTr("Forward")

                    enabled: !History.nextEmpty

                    onClicked: History.next(Qt.MouseFocusReason)
                }
            }

            // a page without trail reads as a trail of one (Figma 6261:15078)
            T.Label {
                id: label

                Layout.row: 0
                Layout.column: 1
                Layout.fillWidth: true
                Layout.leftMargin: VLCStyle.pageHeader_historySpacing - row.columnSpacing
                Layout.minimumHeight: defaultPageHeader._rowHeight

                visible: !crumbs.visible

                font.pixelSize: VLCStyle.fontSize_xlarge
                font.weight: Font.Normal
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.PlainText

                color: navTheme.fg.primary
            }

            // a clickable trail of folders, the last one is the page itself
            Item {
                id: crumbs

                Layout.row: 0
                Layout.column: 1
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: VLCStyle.pageHeader_historySpacing - row.columnSpacing
                Layout.minimumHeight: defaultPageHeader._rowHeight
                Layout.preferredHeight: historyButtons.implicitHeight

                visible: Helpers.isArray(defaultPageHeader.breadcrumb)
                         && defaultPageHeader.breadcrumb.length > 0

                clip: true

                Accessible.role: Accessible.Grouping
                Accessible.name: qsTr("Location")

                Navigation.navigable: crumbRepeater.count > 1
                Navigation.parentItem: defaultPageHeader
                Navigation.leftItem: historyButtons
                Navigation.rightItem: gridSortFilter

                // the button of a crumb
                function _crumbButton(index) {
                    return crumbRepeater.itemAt(index)?.button ?? null
                }

                onActiveFocusChanged: {
                    if (activeFocus && crumbRepeater.count > 1)
                        _crumbButton(crumbRepeater.count - 2).forceActiveFocus(focusReason)
                }

                Row {
                    id: crumbRow

                    anchors.verticalCenter: parent.verticalCenter

                    // a trail too long for the header loses its first
                    // crumbs, the current folder stays visible
                    x: Math.min(0, crumbs.width - crumbRow.width)

                    spacing: VLCStyle.pageHeader_crumbSpacing

                    Repeater {
                        id: crumbRepeater

                        model: crumbs.visible ? defaultPageHeader.breadcrumb : []

                        delegate: Row {
                            id: crumb

                            required property int index
                            required property var modelData

                            readonly property bool isLast: index === defaultPageHeader.breadcrumb.length - 1
                            // Figma 970:2703: the current folder of a trail stands out,
                            // but not in the Library pane (848:1995)
                            readonly property bool emphasized: isLast && (index > 0) && !defaultPageHeader.compact
                            readonly property Item button: crumbButton

                            spacing: VLCStyle.pageHeader_crumbSpacing

                            // Figma: "▶", 8 px, FG/A4
                            IconLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: -VLCStyle.dp(1, VLCStyle.scale)

                                visible: crumb.index > 0

                                width: VLCStyle.pageHeader_crumbSeparatorWidth
                                height: width

                                text: VLCIcons.crumb_separator
                                color: navTheme.separator
                                font.pixelSize: VLCStyle.pageHeader_crumbSeparatorWidth
                            }

                            TextToolButton {
                                id: crumbButton

                                anchors.verticalCenter: parent.verticalCenter

                                // the focus ring hugs the text
                                leftPadding: 0
                                rightPadding: 0

                                // the current folder elides rather than lose its first
                                // letters when it alone is wider than the trail
                                width: crumb.isLast ? Math.min(implicitWidth, crumbs.width) : implicitWidth

                                text: crumb.modelData
                                font.pixelSize: VLCStyle.fontSize_xlarge
                                font.weight: crumb.emphasized ? Font.DemiBold : Font.Normal

                                enabled: !crumb.isLast

                                Accessible.name: crumb.isLast ? qsTr("%1, current folder").arg(text) : text

                                contentItem: T.Label {
                                    text: crumbButton.text
                                    font: crumbButton.font
                                    color: crumb.emphasized ? navTheme.fg.highlight
                                                            : (crumbButton.hovered || crumbButton.visualFocus)
                                                              ? crumbButton.colorContext.fg.primary : navTheme.fg.primary
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight

                                    Accessible.ignored: true
                                }

                                Navigation.parentItem: crumbs
                                Navigation.leftAction: function() {
                                    if (crumb.index > 0)
                                        crumbs._crumbButton(crumb.index - 1).forceActiveFocus(Qt.BacktabFocusReason)
                                    else
                                        crumbs.Navigation.defaultNavigationLeft()
                                }
                                Navigation.rightAction: function() {
                                    if (crumb.index < crumbRepeater.count - 2)
                                        crumbs._crumbButton(crumb.index + 1).forceActiveFocus(Qt.TabFocusReason)
                                    else
                                        crumbs.Navigation.defaultNavigationRight()
                                }

                                onClicked: defaultPageHeader.breadcrumbClicked(crumb.index, Qt.MouseFocusReason)
                            }
                        }
                    }
                }
            }

            CaptionLabel {
                Layout.row: defaultPageHeader.compact ? 1 : 0
                Layout.column: defaultPageHeader.compact ? 0 : 2

                visible: defaultPageHeader.itemCount >= 0

                text: defaultPageHeader.itemCount === 1 ? qsTr("1 item")
                                                         : qsTr("%1 items").arg(defaultPageHeader.itemCount)
                // Figma: FG/A3
                color: navTheme.indicator
            }

            GridSortFilterControls {
                id: gridSortFilter

                Layout.row: defaultPageHeader.compact ? 1 : 0
                Layout.column: defaultPageHeader.compact ? 1 : 3
                Layout.columnSpan: defaultPageHeader.compact ? 3 : 1
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                focus: true
                sort: _sortCtx
                search: _searchCtx
                Navigation.parentItem: defaultPageHeader
                Navigation.leftItem: crumbs.Navigation.navigable ? crumbs : historyButtons
                Navigation.rightItem: actionsLoader.item
            }

            Loader {
                id: actionsLoader

                Layout.row: 0
                Layout.column: defaultPageHeader.compact ? 2 : 4

                visible: !!item

                Navigation.parentItem: defaultPageHeader
                Navigation.leftItem: gridSortFilter
                Navigation.rightItem: expandButton.visible ? expandButton : null
            }

            // ↗: from the Library pane to the full-window library, the video
            // goes on in the Mini player
            IconToolButton {
                id: expandButton

                Layout.row: 0
                Layout.column: 3

                visible: defaultPageHeader.compact

                font.pixelSize: VLCStyle.icon_banner
                width: VLCStyle.bannerButton_width
                height: VLCStyle.bannerButton_height

                color: VLCStyle.pageToolColor(this)

                text: VLCIcons.open
                description: qsTr("Open the full library")

                Navigation.parentItem: defaultPageHeader
                Navigation.leftItem: actionsLoader.item ?? gridSortFilter

                onClicked: {
                    LibraryPages.paneOpen = false
                    MainCtx.playerView = false
                }
            }
        }
    }
}
