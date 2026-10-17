import QtQuick

import VLC.MainInterface
import VLC.Style
import VLC.Util
import VLC.Menus

// The tools of a page header (Figma 6261:15133): sort, view and the search
// field, 20 px apart; in the Library pane (1106:5633), sort, view and a search
// key, 12 px apart.
NavigableRow {
    id: root

    readonly property bool compact: MainCtx.libraryPane

    spacing: compact ? VLCStyle.margin_small : VLCStyle.pageHeader_spacing
    enabled: gridListBtn.visible || sortControl.visible || searchBox.visible

    property SortMenu sortMenu: null
    required property SortCtx sort
    required property SearchCtx search

    SortControl {
        id: sortControl

        anchors.verticalCenter: parent.verticalCenter

        visible: root.sort.available
        enabled: visible
        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height
        font.pixelSize: VLCStyle.icon_banner
        description: qsTr("Sort")

        color: VLCStyle.pageToolColor(this)

        model: root.sort.model

        menu: root.sortMenu

        sortKey:  root.sort.criteria
        sortOrder: root.sort.order

        onSortSelected: (key) => {
            root.sort.criteria = key
        }
        onSortOrderSelected: (type) => {
            root.sort.order = type
        }
    }

    //TODO: visible value of root.sort.available and MainCtx.hasGridListMode is initialised correctly but on first load still shows up
    IconToolButton {
        id: gridListBtn

        anchors.verticalCenter: parent.verticalCenter

        // the Library pane is a list only
        visible: MainCtx.hasGridListMode && !MainCtx.libraryPane

        enabled: visible
        width: VLCStyle.bannerButton_width
        height: VLCStyle.bannerButton_height
        font.pixelSize: VLCStyle.icon_banner
        color: VLCStyle.pageToolColor(this)
        // Figma 970:2703, 970:3308: the icon shows the current view
        text: MainCtx.gridView ? VLCIcons.grid : VLCIcons.list
        description: qsTr("List/Grid")
        onClicked: MainCtx.gridView = !MainCtx.gridView
    }

    SearchBox {
        id: searchBox

        anchors.verticalCenter: parent.verticalCenter

        //TODO: initialise visible value with MainCtx
        visible: root.search.available
        buttonWidth: VLCStyle.bannerButton_width

        // the full-window header shows the field (Figma 6261:15136), the
        // Library pane a key that opens it (1106:5633)
        inline: !root.compact

        Binding {
            target: root.search
            property: "pattern"
            value: searchBox.searchPattern
        }

        Connections {
            target: root.search
            function onAskShow() {
                searchBox.expandAndFocus()
            }
        }
    }
}
