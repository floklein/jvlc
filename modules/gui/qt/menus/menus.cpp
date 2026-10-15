/*****************************************************************************
 * menus.cpp : Qt menus
 *****************************************************************************
 * Copyright © 2006-2011 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_intf_strings.h>
#include <vlc_actions.h>
#include <vlc_interface.h>

#include <algorithm>

#include "menus.hpp"

#include "maininterface/mainctx.hpp"                     /* View modifications */
#include "maininterface/mainctx_submodels.hpp"
#include "dialogs/dialogs_provider.hpp"                   /* Dialogs display */
#include "player/player_controller.hpp"                      /* Input Management */
#include "player/control_list_model.hpp"                  /* Folded controls */
#include "player/waveform.hpp"                               /* Show waveform */
#include "player/sleep_timer.hpp"                            /* Sleep Timer */
#include "playlist/playlist_controller.hpp"
#include "dialogs/extensions/extensions_manager.hpp"                 /* Extensions menu */
#include "dialogs/settings/profiles_model.hpp"                       /* Profile menu */
#include "util/varchoicemodel.hpp"
#include "util/vlchotkeyconverter.hpp"
#include "util/glyph_icon.hpp"
#include "style/vlcicons.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlplaylistmanager.hpp"
#include "medialibrary/mlrecentmediamodel.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"


#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QWindow>
#include <QPointer>
#include <QDesktopServices>
#include <QUrl>

using namespace vlc::playlist;

/*
  This file defines the menus of the interface:
  - the VLC menu, the "…" button of the Top bar and of the library;
  - the right-click menu of the player, also opened by the Menu key;
  - the entries shared with the system tray menu.

  Menus always show the current key of an action, read from the core hotkeys
  ("key-*" options) or from the interface's own actions (see
  actionKeyText()).
  */

namespace {

/* the text of a menu entry, followed by the current key of its action, which
 * QMenu shows in its shortcut column */
QString withKey( qt_intf_t *p_intf, const QString &text, const char *keyAction )
{
    if( EMPTY_STR( keyAction ) )
        return text;

    const QString key = actionKeyText( VLC_OBJECT( vlc_object_instance( p_intf ) ),
                                       QString::fromLatin1( keyAction ) );
    if( key.isEmpty() )
        return text;
    return text + QLatin1Char( '\t' ) + key;
}

template<typename Functor>
QAction *addEntry( qt_intf_t *p_intf, QMenu *menu, const QString &text,
                   const char *keyAction, Functor functor )
{
    QAction *action = menu->addAction( withKey( p_intf, text, keyAction ) );
    QObject::connect( action, &QAction::triggered, menu, functor );
    return action;
}

QAction *addCheckableEntry( qt_intf_t *p_intf, QMenu *menu, const QString &text,
                            const char *keyAction, QObject *model, const char *property )
{
    QAction *action = new BooleanPropertyAction( withKey( p_intf, text, keyAction ),
                                                 model, property, menu );
    menu->addAction( action );
    return action;
}

QMenu *addSubmenu( QMenu *menu, const QString &title, QAbstractListModel *model,
                   QActionGroup::ExclusionPolicy policy )
{
    QMenu *submenu = new CheckableListMenu( title, model, policy, menu );
    menu->addMenu( submenu );
    return submenu;
}

/* a user's text in a menu: '&' is not a mnemonic */
QString escapeMenuText( QString text )
{
    return text.replace( QLatin1Char( '&' ), QLatin1String( "&&" ) );
}

/* run a core hotkey action, as its key would */
void triggerAction( qt_intf_t *p_intf, const char *keyAction )
{
    const QByteArray option = QByteArray( "key-" ) + keyAction;
    const vlc_action_id_t id = vlc_actions_get_id( option.constData() );
    if( id != ACTIONID_NONE )
        var_SetInteger( vlc_object_instance( p_intf ), "key-action", id );
}

void setIcon( QAction *action, const QMenu *menu, QChar base, QChar accent )
{
#ifndef __APPLE__ /* We don't set icons in menus in MacOS X */
    action->setIcon( glyphIcon( base, accent, menu ) );
#else
    VLC_UNUSED( action ); VLC_UNUSED( menu ); VLC_UNUSED( base ); VLC_UNUSED( accent );
#endif
}

/* where to open a menu requested by a click or by the keyboard: at the
 * pointer, or in the middle of the window when the pointer is away */
QPoint popupPosition( qt_intf_t *p_intf )
{
    const QPoint cursor = QCursor::pos();
    QWindow *window = p_intf->p_mi ? p_intf->p_mi->intfMainWindow() : nullptr;
    if( window && !window->geometry().contains( cursor ) )
        return window->geometry().center();
    return cursor;
}

}

/*****************************************************************************
 * Menus
 *****************************************************************************/

VLCMenuBar::VLCMenuBar(QObject* parent)
    : QObject(parent)
{}

/**
 * Open entries, in the VLC menu and in the right-click menu's "Open Media"
 **/
void VLCMenuBar::OpenEntries( qt_intf_t *p_intf, QMenu *menu, bool icons )
{
    QAction *action;

    action = addEntry( p_intf, menu, qtr( "Open &File(s)..." ), "open-file",
                       []() { THEDP->simpleOpenDialog(); } );
    if( icons )
        setIcon( action, menu, VLCIcons::file_tt_base, VLCIcons::file_tt_accent );

    action = addEntry( p_intf, menu, qtr( "Open F&older..." ), "open-folder",
                       []() { THEDP->PLOpenDir(); } );
    if( icons )
        setIcon( action, menu, VLCIcons::folder_tt_base, VLCIcons::folder_tt_accent );

    action = addEntry( p_intf, menu, qtr( "Open &Network Stream..." ), "open-network",
                       []() { THEDP->openNetDialog(); } );
    if( icons )
        setIcon( action, menu, VLCIcons::streaming_tt_base, VLCIcons::streaming_tt_accent );

    action = addEntry( p_intf, menu, qtr( "Open &Disc..." ), "open-disc",
                       []() { THEDP->openDiscDialog(); } );
    if( icons )
        setIcon( action, menu, VLCIcons::disc_tt_base, VLCIcons::disc_tt_accent );

    action = addEntry( p_intf, menu, qtr( "Open &Capture Device..." ), "open-capture",
                       []() { THEDP->openCaptureDialog(); } );
    if( icons )
        setIcon( action, menu, VLCIcons::capture_device_tt_base,
                 VLCIcons::capture_device_tt_accent );

    MainCtx *mi = p_intf->p_mi;
    action = addEntry( p_intf, menu, qtr( "Open from Clip&board" ), "open-clipboard",
                       [mi = QPointer<MainCtx>( mi )]() {
                           if( mi )
                               mi->pasteFromClipboard();
                       } );
    if( icons )
        setIcon( action, menu, VLCIcons::link_tt_base, VLCIcons::link_tt_accent );
}

/**
 * The VLC menu, the "…" button: opening media, the media tools,
 * customization and the application
 **/
void VLCMenuBar::VLCMenuEntries( qt_intf_t *p_intf, QMenu *menu )
{
    MainCtx *mi = p_intf->p_mi;
    QAction *action;

    OpenEntries( p_intf, menu, true );

    action = addEntry( p_intf, menu, qtr( "Conve&rt..." ), "convert",
                       []() { THEDP->openAndTranscodingDialogs(); } );
    setIcon( action, menu, VLCIcons::swap_tt_base, VLCIcons::swap_tt_accent );

    action = addEntry( p_intf, menu, qtr( "&Stream && Cast..." ), "stream",
                       []() { THEDP->openAndStreamingDialogs(); } );
    setIcon( action, menu, VLCIcons::upload_tt_base, VLCIcons::upload_tt_accent );

    if( mi && var_InheritBool( p_intf, "save-recentplay" ) && mi->hasMediaLibrary() )
    {
        MLRecentMediaModel* recentModel = new MLRecentMediaModel(nullptr);
        recentModel->setMl(mi->getMediaLibrary());
        recentModel->setLimit(10);
        QMenu* recentsMenu = new RecentMenu(recentModel, mi->getMediaLibrary(), menu);
        recentsMenu->setTitle(qtr( "Open &Recent" ) );
        recentModel->setParent(recentsMenu);
        action = menu->addMenu( recentsMenu );
        setIcon( action, menu, VLCIcons::recent_tt_base, VLCIcons::recent_tt_accent );
    }

    SleepTimerMenu( p_intf, menu );

    menu->addSeparator();

    /* Customization */
    if( mi && mi->profiles() )
    {
        QMenu *profileMenu = new QMenu( qtr( "&Profile (%1)" )
                                        .arg( escapeMenuText( mi->profiles()->activeName() ) ),
                                        menu );
        ProfileMenu( p_intf, profileMenu );
        action = menu->addMenu( profileMenu );
        setIcon( action, menu, VLCIcons::profile_tt_base, VLCIcons::profile_tt_accent );
    }

    action = addEntry( p_intf, menu, qtr( "&Keyboard Shortcuts..." ), nullptr,
                       []() { THEDP->keyboardShortcutsDialog(); } );
    setIcon( action, menu, VLCIcons::hotkeys_tt_base, VLCIcons::hotkeys_tt_accent );

    if( !p_intf->b_isDialogProvider )
    {
        action = addEntry( p_intf, menu, qtr( "C&ustomize Player..." ), nullptr,
                           []() { THEDP->showToolbarEditorDialog(); } );
        setIcon( action, menu, VLCIcons::paint_tt_base, VLCIcons::paint_tt_accent );
    }

    action = addEntry( p_intf, menu, qtr( "S&ettings..." ), "settings",
                       []() { THEDP->prefsDialog(); } );
    action->setMenuRole( QAction::PreferencesRole );
    setIcon( action, menu, VLCIcons::settings_tt_base, VLCIcons::settings_tt_accent );

    QMenu *toolsMenu = new QMenu( qtr( "&Tools" ), menu );
    ToolsMenu( p_intf, toolsMenu );
    action = menu->addMenu( toolsMenu );
    setIcon( action, menu, VLCIcons::plugins_tt_base, VLCIcons::plugins_tt_accent );

    menu->addSeparator();

    /* Application */
    addEntry( p_intf, menu, qtr( "Online &Help" ), "help",
              []() { QDesktopServices::openUrl( QUrl( QStringLiteral( "https://docs.videolan.me/vlc-user/" ) ) ); } );

    action = addEntry( p_intf, menu, qtr( "&About VLC" ), "about",
                       []() { THEDP->aboutDialog(); } );
    action->setMenuRole( QAction::AboutRole );
    setIcon( action, menu, VLCIcons::info_tt_base, VLCIcons::info_tt_accent );

    action = addEntry( p_intf, menu, qtr( "&Quit" ), "quit",
                       []() { THEDP->quit(); } );
    action->setMenuRole( QAction::QuitRole );
    setIcon( action, menu, VLCIcons::quit_tt_base, VLCIcons::quit_tt_accent );
}

/**
 * Profile: switch to another Profile, or create one from the active one
 **/
void VLCMenuBar::ProfileMenu( qt_intf_t *p_intf, QMenu *menu )
{
    MainCtx *mi = p_intf->p_mi;
    if( !mi || !mi->profiles() )
        return;

    QPointer<ProfilesModel> profiles = mi->profiles();
    QActionGroup *group = new QActionGroup( menu );
    for( int i = 0; i < profiles->rowCount(); ++i )
    {
        const QModelIndex index = profiles->index( i );
        const QString id = index.data( ProfilesModel::IdRole ).toString();

        QAction *action = menu->addAction(
            escapeMenuText( index.data( ProfilesModel::NameRole ).toString() ) );
        action->setCheckable( true );
        action->setChecked( index.data( ProfilesModel::ActiveRole ).toBool() );
        action->setActionGroup( group );
        QObject::connect( action, &QAction::triggered, menu, [profiles, id]() {
            if( profiles )
                profiles->activate( id );
        } );
    }

    menu->addSeparator();

    QAction *action = addEntry( p_intf, menu, qtr( "&New..." ), nullptr,
                                []() { THEDP->showNewProfileDialog(); } );
    setIcon( action, menu, VLCIcons::add, QChar() );
}

/**
 * The Control bar's controls folded or left out in a narrow window, in the
 * order of the Control bar, then a separator. A control doing a core hotkey
 * action does it; the others are handed to <trigger> (the player's QML)
 **/
void VLCMenuBar::FoldedEntries( qt_intf_t *p_intf, QMenu *menu, const QList<int> &folded,
                                const std::function<void(int)> &trigger )
{
    struct Entry
    {
        int control;
        const char *text;
        // the key shown; a core hotkey action ("stop") is also what it does
        const char *keyAction;
        QChar base;
        QChar accent;
    };

    using C = ControlListModel;
    static const Entry entries[] = {
        { C::AUDIO_BUTTON, N_( "&Audio" ), nullptr,
          VLCIcons::audio_tt_base, VLCIcons::audio_tt_accent },
        { C::SUBTITLES_BUTTON, N_( "Su&btitles" ), nullptr,
          VLCIcons::subtitle_tt_base, VLCIcons::subtitle_tt_accent },
        { C::EXTENDED_BUTTON, N_( "Ad&justments" ), "adjustments",
          VLCIcons::settings_tt_base, VLCIcons::settings_tt_accent },
        { C::PLAYLIST_BUTTON, N_( "&Queue" ), "show-queue", QChar(), QChar() },
        { C::PLAY_BUTTON, N_( "Play/Pa&use" ), "play-pause", QChar(), QChar() },
        { C::STOP_BUTTON, N_( "S&top" ), "stop", QChar(), QChar() },
        { C::PREVIOUS_BUTTON, N_( "Pre&vious" ), "prev", QChar(), QChar() },
        { C::NEXT_BUTTON, N_( "Ne&xt" ), "next", QChar(), QChar() },
        { C::SKIP_BACK_BUTTON, N_( "Step Back" ), "jump-short", QChar(), QChar() },
        { C::SKIP_FW_BUTTON, N_( "Step Forward" ), "jump+short", QChar(), QChar() },
        { C::CHAPTER_PREVIOUS_BUTTON, N_( "Previous Chapter" ), "chapter-prev", QChar(), QChar() },
        { C::CHAPTER_NEXT_BUTTON, N_( "Next Chapter" ), "chapter-next", QChar(), QChar() },
        { C::FRAME_PREV_BUTTON, N_( "Previous Frame" ), "frame-previous", QChar(), QChar() },
        { C::FRAME_NEXT_BUTTON, N_( "Next Frame" ), "frame-next", QChar(), QChar() },
        { C::SLOWER_BUTTON, N_( "Slower" ), "slower", QChar(), QChar() },
        { C::FASTER_BUTTON, N_( "Faster" ), "faster", QChar(), QChar() },
        { C::PLAYBACK_SPEED_BUTTON, N_( "Playback Speed..." ), nullptr, QChar(), QChar() },
        { C::RANDOM_BUTTON, N_( "Shuffle" ), "random", QChar(), QChar() },
        { C::LOOP_BUTTON, N_( "Repeat" ), "loop", QChar(), QChar() },
        { C::VOLUME, N_( "Mute" ), "vol-mute", QChar(), QChar() },
        { C::SNAPSHOT_BUTTON, N_( "Take Snapshot" ), "snapshot", QChar(), QChar() },
        { C::RECORD_BUTTON, N_( "Record" ), "record", QChar(), QChar() },
        { C::BOOKMARK_BUTTON, N_( "Add Bookmark" ), "add-bookmark", QChar(), QChar() },
        { C::DVD_MENUS_BUTTON, N_( "Disc Menu" ), "disc-menu", QChar(), QChar() },
        { C::INFO_BUTTON, N_( "Media Information..." ), "media-info", QChar(), QChar() },
        { C::RENDERER_BUTTON, N_( "Stream && Cast..." ), "stream", QChar(), QChar() },
        { C::PICTURE_IN_PICTURE_BUTTON, N_( "Picture-in-picture" ), "picture-in-picture",
          QChar(), QChar() },
        { C::FULLSCREEN_BUTTON, N_( "Fullscreen" ), "toggle-fullscreen", QChar(), QChar() },
    };

    vlc_object_t *libvlc = VLC_OBJECT( vlc_object_instance( p_intf ) );
    bool added = false;
    for( const int control : folded )
    {
        const auto entry = std::find_if( std::begin( entries ), std::end( entries ),
                                         [control]( const Entry &e ) { return e.control == control; } );
        if( entry == std::end( entries ) )
            continue;

        /* a core hotkey action: done here; else by the player */
        vlc_action_id_t id = ACTIONID_NONE;
        if( entry->keyAction )
        {
            const QByteArray option = QByteArray( "key-" ).append( entry->keyAction );
            id = vlc_actions_get_id( option.constData() );
        }
        QAction *action = addEntry( p_intf, menu, qtr( entry->text ), entry->keyAction,
                                    [trigger, control, libvlc, id]() {
            if( id != ACTIONID_NONE )
                var_SetInteger( libvlc, "key-action", id );
            else
                trigger( control );
        } );
        if( !entry->base.isNull() )
            setIcon( action, menu, entry->base, entry->accent );
        added = true;
    }

    if( added )
        menu->addSeparator();
}

/**
 * Tools: media information, messages and the program guide
 **/
void VLCMenuBar::ToolsMenu( qt_intf_t *p_intf, QMenu *menu )
{
    addEntry( p_intf, menu, qfut( I_MENU_INFO ), "media-info",
              []() { THEDP->mediaInfoDialog(); } );
    addEntry( p_intf, menu, qfut( I_MENU_CODECINFO ), "codec-info",
              []() { THEDP->mediaCodecDialog(); } );
    addEntry( p_intf, menu, qfut( I_MENU_MSG ), "messages",
              []() { THEDP->messagesDialog(); } );
    addEntry( p_intf, menu, qtr( "&Program Guide" ), nullptr,
              []() { THEDP->epgDialog(); } );

    /* Lua extensions */
    ExtensionsMenu( p_intf, menu );
}

/**
 * Extensions menu: populate the current menu with extensions
 **/
void VLCMenuBar::ExtensionsMenu( qt_intf_t *p_intf, QMenu *extMenu )
{
    /* Get ExtensionsManager and load extensions if needed */
    ExtensionsManager *extMgr = ExtensionsManager::getInstance( p_intf );

    if( !var_InheritBool( p_intf, "qt-autoload-extensions")
        && !extMgr->isLoaded() )
    {
        return;
    }

    if( !extMgr->isLoaded() && !extMgr->cannotLoad() )
    {
        extMgr->loadExtensions();
    }

    /* Let the ExtensionsManager build itself the menu */
    extMenu->addSeparator();
    extMgr->menu( extMenu );
}

void VLCMenuBar::VolumeEntries( qt_intf_t *p_intf, QMenu *current )
{
    current->addSeparator();

    addEntry( p_intf, current, qtr( "Volume &Up" ), "vol-up",
              [p_intf]() { THEMIM->setVolumeUp(); } );
    addEntry( p_intf, current, qtr( "Volume &Down" ), "vol-down",
              [p_intf]() { THEMIM->setVolumeDown(); } );
    addCheckableEntry( p_intf, current, qtr( "&Mute" ), "vol-mute", THEMIM, "muted" );
}

/**
 * Audio: tracks, output, stereo mode, volume and visualizations
 **/
void VLCMenuBar::AudioMenu( qt_intf_t *p_intf, QMenu * current )
{
    addSubmenu( current, qtr( "Audio &Track" ), THEMIM->getAudioTracks(),
                QActionGroup::ExclusionPolicy::ExclusiveOptional );
    addSubmenu( current, qtr( "&Output Device" ), THEMIM->getAudioDevices(),
                QActionGroup::ExclusionPolicy::Exclusive );

    VLCVarChoiceModel *mix_mode = THEMIM->getAudioMixMode();
    if (mix_mode->rowCount() == 0)
        addSubmenu( current, qtr( "&Stereo Mode" ), THEMIM->getAudioStereoMode(),
                    QActionGroup::ExclusionPolicy::Exclusive );
    else
        addSubmenu( current, qtr( "&Mix Mode" ), mix_mode,
                    QActionGroup::ExclusionPolicy::Exclusive );

    VolumeEntries( p_intf, current );

    current->addSeparator();
    addSubmenu( current, qtr( "&Visualizations" ), THEMIM->getAudioVisualizations(),
                QActionGroup::ExclusionPolicy::Exclusive );
}

/**
 * Subtitle: tracks, adding subtitles and showing them
 **/
void VLCMenuBar::SubtitleMenu( qt_intf_t *p_intf, QMenu *current )
{
    addSubmenu( current, qtr( "Subtitle &Track" ), THEMIM->getSubtitleTracks(),
                QActionGroup::ExclusionPolicy::ExclusiveOptional );
    addEntry( p_intf, current, qtr( "&Add Subtitles..." ), nullptr,
              []() { THEDP->addSubtitlesDialog(); } );
    current->addSeparator();
    addEntry( p_intf, current, qtr( "&Show / Hide Subtitles" ), "subtitle-toggle",
              [p_intf]() { triggerAction( p_intf, "subtitle-toggle" ); } );
}

/**
 * Video: tracks, window, picture and snapshot
 **/
void VLCMenuBar::VideoMenu( qt_intf_t *p_intf, QMenu *current )
{
    MainCtx *mi = p_intf->p_mi;

    addSubmenu( current, qtr( "Video &Track" ), THEMIM->getVideoTracks(),
                QActionGroup::ExclusionPolicy::None );

    current->addSeparator();
    /* Window */
    addCheckableEntry( p_intf, current, qtr( "&Fullscreen" ), "toggle-fullscreen",
                       THEMIM, "fullscreen" );
    if( mi )
    {
        addCheckableEntry( p_intf, current, qtr( "Picture-in-pict&ure" ), "picture-in-picture",
                           mi, "pictureInPicture" );
        addCheckableEntry( p_intf, current, qtr( "Always on &Top" ), nullptr,
                           mi, "interfaceAlwaysOnTop" );
    }
    QAction *action = addCheckableEntry( p_intf, current, qtr( "Fit &Window to Video" ),
                                         "toggle-autoscale", THEMIM, "autoscale" );
    action->setEnabled( THEMIM->hasVideoOutput() );
    connect( THEMIM, &PlayerController::hasVideoOutputChanged, action, &QAction::setEnabled );

    current->addSeparator();
    /* Picture */
    addSubmenu( current, qtr( "&Zoom" ), THEMIM->getZoom(),
                QActionGroup::ExclusionPolicy::Exclusive );
    addSubmenu( current, qtr( "&Aspect Ratio" ), THEMIM->getAspectRatio(),
                QActionGroup::ExclusionPolicy::Exclusive );
    addSubmenu( current, qtr( "&Crop" ), THEMIM->getCrop(),
                QActionGroup::ExclusionPolicy::Exclusive );

    /* Deinterlacing, then its mode */
    QMenu *deinterlace = addSubmenu( current, qtr( "&Deinterlace" ), THEMIM->getDeinterlace(),
                                     QActionGroup::ExclusionPolicy::Exclusive );
    deinterlace->addSeparator();
    QMenu *deinterlaceMode = new CheckableListMenu( qtr( "&Mode" ), THEMIM->getDeinterlaceMode(),
                                                    QActionGroup::ExclusionPolicy::Exclusive,
                                                    deinterlace );
    deinterlace->addMenu( deinterlaceMode );

    current->addSeparator();
    addEntry( p_intf, current, qtr( "Take &Snapshot" ), "snapshot",
              [p_intf]() { THEMIM->snapshot(); } );

    /* the Video tab of the Adjustments sidebar (flip, rotation, filters) */
    if( mi && !p_intf->b_isDialogProvider )
    {
        /* RightPanelFSM.Video */
        static const int ADJUSTMENTS_VIDEO_TAB = 1;
        addEntry( p_intf, current, qtr( "Video &Adjustments..." ), nullptr,
                  [mi = QPointer<MainCtx>( mi )]() {
                      if( mi )
                          mi->openAdjustments( ADJUSTMENTS_VIDEO_TAB );
                  } );
    }
}

/**
 * Playback: speed, jumps, frames, titles, chapters, programs, bookmarks,
 * repeat and shuffle
 **/
void VLCMenuBar::PlaybackMenu( qt_intf_t *p_intf, QMenu *menu )
{
    PopupMenuControlEntries( menu, p_intf );

    addEntry( p_intf, menu, qtr( "Previous F&rame" ), "frame-previous",
              [p_intf]() { THEMIM->framePrev(); } );
    addEntry( p_intf, menu, qtr( "Next &Frame" ), "frame-next",
              [p_intf]() { THEMIM->frameNext(); } );

    menu->addSeparator();

    QMenu *submenu;
    QAction *action;

    submenu = addSubmenu( menu, qtr( "T&itle" ), THEMIM->getTitles(),
                          QActionGroup::ExclusionPolicy::Exclusive );
    action = submenu->menuAction();
    action->setEnabled( THEMIM->hasTitles() );
    connect( THEMIM, &PlayerController::hasTitlesChanged, action, &QAction::setEnabled );

    submenu = addSubmenu( menu, qtr( "&Chapter" ), THEMIM->getChapters(),
                          QActionGroup::ExclusionPolicy::Exclusive );
    action = submenu->menuAction();
    action->setEnabled( THEMIM->hasChapters() );
    connect( THEMIM, &PlayerController::hasChaptersChanged, action, &QAction::setEnabled );

    submenu = addSubmenu( menu, qtr( "&Program" ), THEMIM->getPrograms(),
                          QActionGroup::ExclusionPolicy::Exclusive );
    action = submenu->menuAction();
    action->setEnabled( THEMIM->hasPrograms() );
    connect( THEMIM, &PlayerController::hasProgramsChanged, action, &QAction::setEnabled );

    MainCtx *mi = p_intf->p_mi;
    if( mi && mi->hasMediaLibrary() )
    {
        BookmarkMenu *bookmarks = new BookmarkMenu( mi->getMediaLibrary(), p_intf->p_player, menu );
        bookmarks->setTitle( qfut( I_MENU_BOOKMARK ) );

        QAction *addBookmark = new QAction( withKey( p_intf, qtr( "&Add Bookmark" ), "add-bookmark" ),
                                            bookmarks );
        connect( addBookmark, &QAction::triggered, mi, &MainCtx::addBookmark );
        bookmarks->insertAction( bookmarks->actions().value( 0 ), addBookmark );

        action = menu->addMenu( bookmarks );
        action->setData( "bookmark" );
    }

    SleepTimerMenu( p_intf, menu );

    menu->addSeparator();

    RepeatAndShuffleEntries( p_intf, menu );
}

/* Sleep Timer ▸: the choices of the Playback tab, the running one checked
 * (a custom duration is set in the Playback tab) */
void VLCMenuBar::SleepTimerMenu( qt_intf_t *p_intf, QMenu *menu )
{
    QPointer<SleepTimer> timer = THEMIM->getSleepTimer();
    if( !timer )
        return;

    QMenu *submenu = menu->addMenu( qtr( "Sl&eep Timer" ) );
    QActionGroup *group = new QActionGroup( submenu );

    const auto add = [&]( const QString &text, bool checked, std::function<void()> start ) {
        QAction *action = submenu->addAction( text );
        action->setCheckable( true );
        action->setChecked( checked );
        action->setActionGroup( group );
        connect( action, &QAction::triggered, timer, [timer, start]() {
            if( timer )
                start();
        } );
    };

    const SleepTimer::Mode mode = timer->mode();
    add( qtr( "Off" ), mode == SleepTimer::Off, [timer]() { timer->cancel(); } );
    for( const int minutes : { 15, 30, 45, 60, 90 } )
        add( qtr( "%1 min" ).arg( minutes ),
             mode == SleepTimer::Duration && timer->minutes() == minutes,
             [timer, minutes]() { timer->start( minutes ); } );
    add( qtr( "End of This Item" ), mode == SleepTimer::EndOfItem,
         [timer]() { timer->startEndOfItem(); } );
    add( qtr( "End of Queue" ), mode == SleepTimer::EndOfQueue,
         [timer]() { timer->startEndOfQueue(); } );
}

/* Repeat ▸ and Shuffle, of the Playback and Queue menus */
void VLCMenuBar::RepeatAndShuffleEntries( qt_intf_t *p_intf, QMenu *menu )
{
    QMenu *repeatMenu = new QMenu( qtr( "&Repeat" ), menu );
    QActionGroup *group = new QActionGroup( repeatMenu );

    const struct {
        const char *text;
        PlaylistController::PlaybackRepeat mode;
    } modes[] = {
        { N_( "&Off" ), PlaylistController::PLAYBACK_REPEAT_NONE },
        { N_( "&All" ), PlaylistController::PLAYBACK_REPEAT_ALL },
        { N_( "&One" ), PlaylistController::PLAYBACK_REPEAT_CURRENT },
    };
    for( const auto &mode : modes )
    {
        QAction *action = repeatMenu->addAction( qtr( mode.text ) );
        action->setCheckable( true );
        action->setChecked( THEMPL->getRepeatMode() == mode.mode );
        group->addAction( action );
        connect( action, &QAction::triggered, THEMPL, [p_intf, m = mode.mode]() {
            THEMPL->setRepeatMode( m );
        } );
    }
    menu->addMenu( repeatMenu );

    addCheckableEntry( p_intf, menu, qtr( "S&huffle" ), "random", THEMPL, "random" );
}

/**
 * Queue: the panel, playback order and saving
 **/
void VLCMenuBar::QueueMenu( qt_intf_t *p_intf, QMenu *menu )
{
    MainCtx *mi = p_intf->p_mi;

    if( mi )
        addCheckableEntry( p_intf, menu, qtr( "&Show Queue" ), "show-queue",
                           mi->getPlayqueuePanel(), "visible" );

    menu->addSeparator();
    RepeatAndShuffleEntries( p_intf, menu );

    menu->addSeparator();
    QAction *action = addEntry( p_intf, menu, qtr( "&Clear Queue" ), "play-clear",
                                [p_intf]() { THEMPL->clear(); } );
    action->setEnabled( !THEMPL->isEmpty() );

    MediaLib *ml = mi ? mi->getMediaLibrary() : nullptr;
    if( ml )
    {
        QPointer<MediaLib> medialib = ml;
        action = addEntry( p_intf, menu, qtr( "Save Queue as &Playlist..." ), nullptr,
                           [medialib]() {
            if( medialib )
                medialib->playlistManager()->saveQueueAsPlaylist();
        } );
        action->setEnabled( !THEMPL->isEmpty() );
    }

    addEntry( p_intf, menu, qtr( "Save Queue to &File..." ), "save-queue",
              []() { THEDP->savePlayingToPlaylist(); } );
}

/*****************************************************************************
 * Popup menus - Right Click menus                                           *
 *****************************************************************************/

void VLCMenuBar::PopupMenuPlaylistEntries( QMenu *menu, qt_intf_t *p_intf, bool icons )
{
    QAction *action;
    bool hasInput = THEMIM->hasInput();

    /* Play or Pause action and icon */
    if( !hasInput || THEMIM->getPlayingState() != PlayerController::PLAYING_STATE_PLAYING )
    {
        action = addEntry( p_intf, menu, qtr( "&Play" ), "play-pause", [p_intf]() {
            if ( THEMPL->isEmpty() )
                THEDP->simpleOpenDialog();
            else
                THEMPL->togglePlayPause();
        });
        if( icons )
            setIcon( action, menu, {}, VLCIcons::playback_tt_accent );
    }
    else
    {
        action = addEntry( p_intf, menu, qtr( "&Pause" ), "play-pause",
                           [p_intf]() { THEMPL->togglePlayPause(); } );
        if( icons )
            setIcon( action, menu, {}, VLCIcons::pause );
    }

    /* Stop */
    action = addEntry( p_intf, menu, qtr( "&Stop" ), "stop",
                       [p_intf]() { THEMPL->stop(); } );
    if( icons )
        setIcon( action, menu, {}, VLCIcons::stop_tt_accent );
    if( !hasInput )
        action->setEnabled( false );

    /* Next / Previous */
    bool bPlaylistEmpty = THEMPL->isEmpty();
    QAction* previousAction = addEntry( p_intf, menu, qtr( "Pre&vious" ), "prev",
                                        [p_intf]() { THEMPL->prev(); } );
    if( icons )
        setIcon( previousAction, menu, {}, VLCIcons::previous_tt_accent );
    previousAction->setEnabled( !bPlaylistEmpty );
    connect( THEMPL, &PlaylistController::isEmptyChanged, previousAction, &QAction::setDisabled);

    QAction* nextAction = addEntry( p_intf, menu, qtr( "Ne&xt" ), "next",
                                    [p_intf]() { THEMPL->next(); } );
    if( icons )
        setIcon( nextAction, menu, {}, VLCIcons::next_tt_accent );
    nextAction->setEnabled( !bPlaylistEmpty );
    connect( THEMPL, &PlaylistController::isEmptyChanged, nextAction, &QAction::setDisabled);

    action = addCheckableEntry( p_intf, menu, qtr( "&Record" ), "record", THEMIM, "recording" );
    if( icons )
        setIcon( action, menu, {}, VLCIcons::record_tt_accent );
    if( !hasInput )
        action->setEnabled( false );
    menu->addSeparator();
}

void VLCMenuBar::PopupMenuControlEntries( QMenu *menu, qt_intf_t *p_intf,
                                        bool b_normal )
{
    QMenu *rateMenu = new QMenu( qtr( "Sp&eed" ), menu );

    if( b_normal )
        addEntry( p_intf, rateMenu, qtr( "&Faster" ), "faster",
                  [p_intf]() { THEMIM->faster(); } );

    addEntry( p_intf, rateMenu, qtr( "Faster (fine)" ), "rate-faster-fine",
              [p_intf]() { THEMIM->littlefaster(); } );
    addEntry( p_intf, rateMenu, qtr( "N&ormal Speed" ), "rate-normal",
              [p_intf]() { THEMIM->normalRate(); } );
    addEntry( p_intf, rateMenu, qtr( "Slower (fine)" ), "rate-slower-fine",
              [p_intf]() { THEMIM->littleslower(); } );

    if( b_normal )
        addEntry( p_intf, rateMenu, qtr( "Slo&wer" ), "slower",
                  [p_intf]() { THEMIM->slower(); } );

    menu->addMenu( rateMenu );

    menu->addSeparator();

    if( !b_normal ) return;

    addEntry( p_intf, menu, qtr( "&Jump Forward" ), "jump+short",
              [p_intf]() { THEMIM->jumpFwd(); } );
    addEntry( p_intf, menu, qtr( "Jump Bac&kward" ), "jump-short",
              [p_intf]() { THEMIM->jumpBwd(); } );
    addEntry( p_intf, menu, qfut( I_MENU_GOTOTIME ), "jump-to-time",
              []() { THEDP->gotoTimeDialog(); } );
}

void VLCMenuBar::PopupMenuStaticEntries( QMenu *menu, qt_intf_t *p_intf )
{
    QMenu *openmenu = new QMenu( qtr( "Open &Media" ), menu );
    OpenEntries( p_intf, openmenu, false );
    QAction *action = menu->addMenu( openmenu );
    setIcon( action, menu, VLCIcons::file_tt_base, VLCIcons::file_tt_accent );

    action = addEntry( p_intf, menu, qtr( "&Quit" ), "quit", []() { THEDP->quit(); } );
    action->setMenuRole( QAction::QuitRole );
    setIcon( action, menu, VLCIcons::quit_tt_base, VLCIcons::quit_tt_accent );
}

/* Video Tracks and Subtitles tracks */
QMenu* VLCMenuBar::VideoPopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);
    VideoMenu(p_intf, menu);
    if( show )
        menu->popup( popupPosition( p_intf ) );
    return menu;
}

/* Audio Tracks */
QMenu* VLCMenuBar::AudioPopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);
    AudioMenu(p_intf, menu);
    if( show )
        menu->popup( popupPosition( p_intf ) );
    return menu;
}

/* Navigation stuff, and general menus ( open ), used only for skins */
QMenu* VLCMenuBar::MiscPopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);

    PopupMenuPlaylistEntries( menu, p_intf );

    menu->addSeparator();
    PopupMenuControlEntries( menu, p_intf );

    menu->addSeparator();
    PopupMenuStaticEntries( menu, p_intf );

    if( show )
        menu->popup( popupPosition( p_intf ) );
    return menu;
}

/* The right-click menu of the player, also opened by the Menu key and
 * Shift+F10: every player action */
QMenu* VLCMenuBar::PopupMenu( qt_intf_t *p_intf, bool show )
{
    QMenu* menu = new VLCMenu(p_intf);
    MainCtx *mi = p_intf->p_mi;
    const bool hasInput = THEMIM->hasInput();
    QMenu *submenu;
    QAction *action;

    PopupMenuPlaylistEntries( menu, p_intf, true );

    /* Hide UI */
    if( mi && !p_intf->b_isDialogProvider )
    {
        action = addEntry( p_intf, menu, qtr( "&Hide UI" ), "hide-ui",
                           [mi = QPointer<MainCtx>( mi )]( bool checked ) {
                               if( mi )
                                   mi->setHideUI( checked );
                           } );
        action->setCheckable( true );
        action->setChecked( mi->hideUI() );
        setIcon( action, menu, VLCIcons::ui_tt_base, VLCIcons::ui_tt_accent );
        menu->addSeparator();
    }

    /* Music player views: the waveform toggle only exists for local music */
    if( mi )
    {
        const bool b_waveform = THEMIM->getWaveform()->available();
        const bool b_lyrics = !THEMIM->getVout() && THEMIM->hasLyrics();
        if( b_waveform )
            menu->addAction( new BooleanPropertyAction( qtr( "Show &Waveform" ), mi, "showWaveform", menu ) );
        if( b_lyrics )
            menu->addAction( new BooleanPropertyAction( qtr( "&Lyrics View" ), mi, "lyricsMode", menu ) );
        if( b_waveform || b_lyrics )
            menu->addSeparator();
    }

    /* Playback, Video, Audio and Subtitle */
    submenu = new QMenu( qtr( "&Playback" ), menu );
    PlaybackMenu( p_intf, submenu );
    action = menu->addMenu( submenu );
    action->setEnabled( hasInput );
    setIcon( action, menu, VLCIcons::playback_selected_tt_base,
             VLCIcons::playback_selected_tt_accent );

    submenu = new QMenu( qtr( "&Video" ), menu );
    VideoMenu( p_intf, submenu );
    action = menu->addMenu( submenu );
    action->setEnabled( hasInput );
    setIcon( action, menu, VLCIcons::video_tt_base, VLCIcons::video_tt_accent );

    submenu = new QMenu( qtr( "&Audio" ), menu );
    AudioMenu( p_intf, submenu );
    action = menu->addMenu( submenu );
    action->setEnabled( hasInput );
    setIcon( action, menu, VLCIcons::audio_tt_base, VLCIcons::audio_tt_accent );

    submenu = new QMenu( qtr( "Subti&tle" ), menu );
    SubtitleMenu( p_intf, submenu );
    action = menu->addMenu( submenu );
    action->setEnabled( hasInput );
    setIcon( action, menu, VLCIcons::subtitle_tt_base, VLCIcons::subtitle_tt_accent );

    menu->addSeparator();

    /* Adjustments, Queue and opening */
    /* the Adjustments sidebar of the player; skins have no sidebar */
    if( mi && !p_intf->b_isDialogProvider )
    {
        action = addEntry( p_intf, menu, qtr( "Ad&justments" ), "adjustments",
                           [mi = QPointer<MainCtx>( mi )]() {
                               if( mi )
                                   mi->toggleAdjustments();
                           } );
        setIcon( action, menu, VLCIcons::settings_tt_base, VLCIcons::settings_tt_accent );
    }

    submenu = new QMenu( qtr( "&Queue" ), menu );
    QueueMenu( p_intf, submenu );
    action = menu->addMenu( submenu );
    setIcon( action, menu, VLCIcons::queue_tt_base, VLCIcons::queue_tt_accent );

    if( p_intf->b_isDialogProvider )
    {
        /* skins: the tools and the extensions in place of the Top bar */
        submenu = new QMenu( qtr( "&Interface" ), menu );
        ToolsMenu( p_intf, submenu );
        submenu->addSeparator();

        vlc_object_t* p_object = vlc_object_parent(p_intf->intf);

        /* Open skin dialog box */
        if (var_Type(p_object, "intf-skins-interactive") & VLC_VAR_ISCOMMAND)
        {
            QAction* openSkinAction = new QAction(qtr("Open skin..."), menu);
            connect(openSkinAction, &QAction::triggered, [=]() {
                var_TriggerCallback(p_object, "intf-skins-interactive");
            });
            submenu->addAction(openSkinAction);
        }

        VLCVarChoiceModel* skinmodel = new VLCVarChoiceModel(p_object, "intf-skins", submenu);
        CheckableListMenu* skinsubmenu = new CheckableListMenu(qtr("Interface"), skinmodel, QActionGroup::ExclusionPolicy::ExclusiveOptional, submenu);
        submenu->addMenu(skinsubmenu);

        menu->addMenu( submenu );
    }

    PopupMenuStaticEntries( menu, p_intf );

    if( show )
        menu->popup( popupPosition( p_intf ) );
    return menu;
}
