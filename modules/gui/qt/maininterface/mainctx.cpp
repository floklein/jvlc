/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2011 VideoLAN and AUTHORS
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <QQmlEngine>
#include <QJSEngine>

#include "mainctx.hpp"
#include "mainctx_submodels.hpp"
#include "pinsmodel.hpp"
#include "medialibrary/mlhelper.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"

#include "compositor.hpp"
#include "window_chrome.hpp"
#include "util/renderer_manager.hpp"
#include "util/csdbuttonmodel.hpp"

#include "util/vlchotkeyconverter.hpp"
#include "util/context_menu_key_filter.hpp"
#include "util/qt_dirs.hpp"                     // toNativeSeparators

#include "util/color_scheme_model.hpp"

#include "playlist/playlist_controller.hpp"
#include "playlist/pasted_media.hpp"
#include "network/saved_streams_model.hpp"
#include "player/player_controller.hpp"
#include "player/player_preset_target.hpp"
#include "player/preset_controller.hpp"
#include "player/presets_model.hpp"
#include "player/shortcut_message.hpp"
#include "player/subtitle_appearance.hpp"
#include "player/waveform.hpp"
#include "player/subtitle_timeline.hpp"
#include "medialibrary/medialib.hpp"

#include "dialogs/dialogs_provider.hpp"
#include "dialogs/subtitles/subtitle_matching.hpp"
#include "dialogs/systray/systray.hpp"

#include "videosurface.hpp"

#include "menus/menus.hpp"                            // Menu creation

#include "dialogs/toolbar/controlbar_profile_model.hpp"
#include "dialogs/settings/profiles_model.hpp"

#include <QKeyEvent>

#include <QUrl>
#include <QDate>
#include <QMimeData>
#include <QClipboard>

#include <QQmlProperty>

#include <QWindow>
#include <QScreen>
#include <utility>

#include <QOperatingSystemVersion>

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QStyleHints>
#endif

#ifdef _WIN32
#include <QFileInfo>
#include <windows.h>
#endif

#include <vlc_interface.h>
#include <vlc_preparser.h>
#include <vlc_actions.h>

#define VLC_REFERENCE_SCALE_FACTOR 96.

using  namespace vlc::playlist;

// #define DEBUG_INTF

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfBossCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfRaiseMainCB( vlc_object_t *p_this, const char *psz_variable,
                           vlc_value_t old_val, vlc_value_t new_val,
                           void *param );

const QEvent::Type MainCtx::ToolbarsNeedRebuild =
        (QEvent::Type)QEvent::registerEventType();

namespace
{

template <typename T>
T loadVLCOption(vlc_object_t *obj, const char *name);

template <>
int loadVLCOption<int>(vlc_object_t *obj, const char *name)
{
    return var_InheritInteger(obj, name);
}

template <>
float loadVLCOption<float>(vlc_object_t *obj, const char *name)
{
    return var_InheritFloat(obj, name);
}

template <>
bool loadVLCOption<bool>(vlc_object_t *obj, const char *name)
{
    return var_InheritBool(obj, name);
}

template <>
QString loadVLCOption<QString>(vlc_object_t *obj, const char *name)
{
    char *value = var_InheritString(obj, name);
    QString result = qfu(value);
    free(value);
    return result;
}

}

MainCtx::MainCtx(qt_intf_t *_p_intf)
    : p_intf(_p_intf)
    , m_csdButtonModel {std::make_unique<CSDButtonModel>(this, this)}
{
    // If `playlist-autostart` is set, and there are initial files
    // we don't bother loading main display just to unload it
    // immediately after. Instead, we directly load the player
    // page:
    if (var_InheritBool(p_intf, "playlist-autostart"))
    {
        // We can not use `vlc_playlist_GetCurrentIndex()`, because
        // playlist starts playing after the interface is added.
        // We also can not use the playlist controller because it
        // is not updated yet.
        vlc_playlist_locker lock(p_intf->p_playlist);

        if (vlc_playlist_Count(p_intf->p_playlist) > 0)
        {
            m_mainInterfaceModes |= MAININTERFACE_MODE_PLAYER;
        }
    }

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/

    settings = getSettings();
    m_colorScheme = new ColorSchemeModel(this);

    m_windowButtonsOnLeft = (WindowChrome::desktopButtonsSide() == WindowChrome::ButtonsSide::Left);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(m_colorScheme, &ColorSchemeModel::currentChanged, qGuiApp, [colorScheme = m_colorScheme]() {
        QStyleHints *const styleHints = qGuiApp->styleHints();
        if (unlikely(!styleHints))
            return;

        Qt::ColorScheme scheme;
        switch (colorScheme->currentScheme())
        {
        case ColorSchemeModel::ColorScheme::Light:
            scheme = Qt::ColorScheme::Light;
            break;
        case ColorSchemeModel::ColorScheme::Dark:
            scheme = Qt::ColorScheme::Dark;
            break;
        case ColorSchemeModel::ColorScheme::System:
        default:
            styleHints->unsetColorScheme();
            return;
        }

        styleHints->setColorScheme(scheme);
    });
#endif

    // Shift+F10 opens context menus like the Menu key
    qApp->installEventFilter(new ContextMenuKeyFilter(this));

    m_sort = new SortCtx(this);
    m_search = new SearchCtx(this);
    m_shortcutMessage = new ShortcutMessage(p_intf->p_player, this);
    m_playqueuePanel = new SidePanelCtx(this);
    m_navigationPanel = new SidePanelCtx(this);

    // getOSInfo();
    QOperatingSystemVersion currentOS = QOperatingSystemVersion::current();
    switch (currentOS.type()) {
    case QOperatingSystemVersion::OSType::Windows:
        m_osName = Windows;
        break;

    default:
        m_osName = Unknown;
        break;
    }
    m_osVersion = (currentOS.majorVersion());

    loadPrefs(false);
    loadFromSettingsImpl(false);

    /* Test hook for screenshots without keyboard input: start in Hide UI */
    m_hideUI = qEnvironmentVariableIsSet("VLC_QT_TEST_HIDE_UI");

    /* Get the available interfaces */
    m_extraInterfaces = new VLCVarChoiceModel(VLC_OBJECT(p_intf->intf), "intf-add", this);

    vlc_medialibrary_t* ml = vlc_ml_instance_get( p_intf );
    b_hasMedialibrary = (ml != NULL);
    if (b_hasMedialibrary) {
        m_medialib = new MediaLib(p_intf, p_intf->p_mainPlaylistController);

        /* removing an external subtitle track forgets its Subtitle match */
        connect(THEMIM->getSubtitleTracks(), &TrackListModel::externalTrackRemoved,
                m_medialib, [ml = m_medialib](const QString&, const QString& media,
                                              const QString& source) {
            ml->removeSubtitleMatch(media, source);
        });

        /* an episode added to a Show after its subtitles were matched gets
         * matched from the Show's source when it plays */
        auto matcher = new ShowSubtitleMatcher(p_intf, m_medialib, p_intf->mainSettings, this);
        connect(THEMIM, &PlayerController::inputChanged, matcher, [this, matcher](bool hasInput) {
            if (!hasInput)
                return;
            QString mrl;
            {
                vlc_player_locker lock{ p_intf->p_player };
                input_item_t *item = vlc_player_GetCurrentMedia(p_intf->p_player);
                if (item)
                    mrl = qfu(item->psz_uri);
            }
            if (!mrl.isEmpty())
                matcher->matchOnPlay(mrl);
        });
    }

    /* the Saved streams and Network Playlists of the Network section */
    m_savedStreams = new SavedStreamsModel(QVLCUserDir(VLC_USERDATA_DIR) + QStringLiteral("/network.json"), this);

    /* Controlbar Profile Model Creation */
    m_controlbarProfileModel = new ControlbarProfileModel(p_intf->mainSettings, this);

    /* the Pins of the library sidebar */
    m_pins = new PinsModel(p_intf->mainSettings, this);

    /* Profiles: a switch replaces the options and the Qt settings of the
     * Profile, then the interface reads them again */
    m_profiles = new ProfilesModel(p_intf, p_intf->mainSettings, this);
    connect(m_profiles, &ProfilesModel::aboutToSwitch, this, &MainCtx::storeProfileSettings);
    connect(m_profiles, &ProfilesModel::switched, this, [this]() {
        loadPrefs(true);
        loadProfileSettings();
        m_controlbarProfileModel->reloadOrDefaults();
        // the keyboard shortcuts of the Profile; global keys need a restart
        vlc_actions_reload(vlc_object_instance(p_intf));
        notifyKeyBindingsChanged();
        // the subtitle look lives in the player too
        if (p_intf->p_mainPlayerController)
            p_intf->p_mainPlayerController->getSubtitleAppearance()->reload();
    });

    /* Presets and Shows: outside the Profiles, so every Profile shares them;
     * the playing file's Preset goes over the Profile's values */
    m_presets = new PresetsModel(p_intf->mainSettings, this);
    if (PlayerController* player = p_intf->p_mainPlayerController)
    {
        m_presetController = new PresetController(m_presets, new PlayerPresetTarget(player, this), this);
        connect(player, &PlayerController::inputChanged, m_presetController, [this, player](bool hasInput) {
            m_presetController->setMedia(hasInput ? player->getUrl().toString() : QString());
        });
        // after the Profile's options reached the player
        connect(m_profiles, &ProfilesModel::switched,
                m_presetController, &PresetController::profileSwitched);
    }

    const QUrl homeUrl = QUrl::fromLocalFile(QVLCUserDir( VLC_HOME_DIR ));
    m_dialogFilepath = getSettings()->value( "filedialog-path", homeUrl ).toUrl();

    QString platformName = QGuiApplication::platformName();

    b_hasWayland = platformName.startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);

    /*********************************
     * Create the Systray Management *
     *********************************/
    //postpone systray initialisation to speedup starting time
    QMetaObject::invokeMethod(this, &MainCtx::initSystray, Qt::QueuedConnection);

    /*************************************************************
     * Connect the input manager to the GUI elements it manages  *
     * Beware initSystray did some connects on input manager too *
     *************************************************************/
    /**
     * Connects on nameChanged()
     * Those connects are different because options can impeach them to trigger.
     **/
    /* Main Interface statusbar */
    /* and title of the Main Interface*/
    connect( THEMIM, &PlayerController::inputChanged, this, &MainCtx::onInputChanged );
    connect( THEMIM, &PlayerController::hasVideoOutputChanged, this, &MainCtx::onVOutChanged );

    /* the waveform is only decoded while the user shows it */
    THEMIM->getWaveform()->setEnabled( m_showWaveform );
    connect( this, &MainCtx::showWaveformChanged, THEMIM->getWaveform(), &SeekWaveform::setEnabled );

    /* the subtitle lines are only read while the user shows the timeline */
    THEMIM->getSubtitleTimeline()->setEnabled( m_showSubtitleTimeline );
    connect( this, &MainCtx::showSubtitleTimelineChanged, THEMIM->getSubtitleTimeline(), &SubtitleTimeline::setEnabled );

    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    connect( this, &MainCtx::askToQuit, THEDP, &DialogsProvider::quit, Qt::QueuedConnection  );

    /** END of CONNECTS**/


    /************
     * Callbacks
     ************/
    libvlc_int_t* libvlc = vlc_object_instance(p_intf);
    var_AddCallback( libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_AddCallback( libvlc, "intf-boss", IntfBossCB, p_intf );
    var_AddCallback( libvlc, "intf-show", IntfRaiseMainCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    /* Onboarding on first start, or when asked again from Settings. While
     * it is pending the media library does not start: it may add folders.
     * VLC_QT_ONBOARDING=<step> shows it anyway, from that step (1 = first),
     * without touching the user's first-start state: a hook for tests and
     * screenshots. */
    const bool privacyAsk = var_InheritBool( p_intf, "qt-privacy-ask" );
    const QByteArray forcedOnboarding = qgetenv( "VLC_QT_ONBOARDING" );
    if( privacyAsk || !forcedOnboarding.isEmpty() )
    {
        const int step = std::max( forcedOnboarding.toInt() - 1, 0 );
        //postpone dialog call, as composition might not be ready yet
        QMetaObject::invokeMethod(this, [step](){
            THEDP->onboarding( step );
        }, Qt::QueuedConnection);
    }
    if( !privacyAsk && m_medialib )
    {
        QMetaObject::invokeMethod(m_medialib, &MediaLib::reload, Qt::QueuedConnection);
    }

    const struct vlc_preparser_cfg cfg = []{
        struct vlc_preparser_cfg cfg{};
        cfg.types = VLC_PREPARSER_TYPE_PARSE;
        cfg.max_parser_threads = 1;
        cfg.timeout = 0;
        cfg.external_process = false;
        return cfg;
    }();
    m_network_preparser = vlc_preparser_New(VLC_OBJECT(libvlc), &cfg);

    m_threadRunner = new ThreadRunner();

    m_initialEffectiveMainInterfaceMode = getEffectiveMainInterfaceMode();
}

MainCtx::~MainCtx()
{
    /* Save states */

    m_threadRunner->destroy();

    if ( !p_intf->preferencesResetPending)
    {
        settings->beginGroup("MainWindow");
        settings->setValue( "ShowRemainingTime", m_showRemainingTime );
        settings->setValue( "interface-scale", QString::number( m_intfUserScaleFactor ) );

        /* Save playlist state */
        settings->setValue( "playqueue-panel-docked", m_playqueuePanel->m_docked );
        settings->setValue( "playqueue-panel-visible", m_playqueuePanel->m_visible );
        settings->setValue( "playqueue-panel-width", QString::number( m_playqueuePanel->m_width ) );

        settings->setValue( "navigation-panel-visible", m_navigationPanel->m_visible );
        settings->setValue( "navigation-panel-width", QString::number( m_navigationPanel->m_width ) );

        settings->setValue( "player-playlist-width", QString::number( m_playerPlaylistWidth ) );
        settings->setValue( "artist-albums-width", QString::number( m_artistAlbumsWidth ) );

        settings->setValue( "grid-view", m_gridView );

        settings->setValue( "color-scheme-index", m_colorScheme->currentIndex() );

        settings->setValue( "album-sections", m_albumSections );

        settings->setValue( "lyrics-mode", m_lyricsMode );

        settings->setValue( "show-waveform", m_showWaveform );

        settings->setValue( "show-statistics", m_showStatistics );

        settings->setValue( "show-subtitle-timeline", m_showSubtitleTimeline );

        /* Save the stackCentralW sizes */
        settings->endGroup();
    }
    else
    {
        settings->remove("MainWindow");
    }

    if( !p_intf->preferencesResetPending && var_InheritBool( p_intf, "save-recentplay" ) )
        getSettings()->setValue( "filedialog-path", m_dialogFilepath );
    else
        getSettings()->remove( "filedialog-path" );

    /* Unregister callbacks */
    libvlc_int_t* libvlc = vlc_object_instance(p_intf);
    var_DelCallback( libvlc, "intf-boss", IntfBossCB, p_intf );
    var_DelCallback( libvlc, "intf-show", IntfRaiseMainCB, p_intf );
    var_DelCallback( libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_DelCallback( libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    if (m_medialib)
        delete m_medialib;

    if (m_network_preparser)
        vlc_preparser_Delete(m_network_preparser);

    p_intf->p_mi = NULL;
}

bool MainCtx::hasVLM() const {
#ifdef ENABLE_VLM
    return true;
#else
    return false;
#endif
}

bool MainCtx::useClientSideDecoration() const
{
    //don't show CSD when interface is fullscreen
    return !m_windowTitlebar;
}

bool MainCtx::hasFirstrun() const {
    return var_InheritBool(p_intf,  "qt-privacy-ask" );
}

void MainCtx::setUseGlobalShortcuts( bool useShortcuts )
{
    if (m_useGlobalShortcuts == useShortcuts)
        return;
    m_useGlobalShortcuts = useShortcuts;
    emit useGlobalShortcutsChanged(m_useGlobalShortcuts);
}

void MainCtx::setWindowSuportExtendedFrame(bool support) {
    if (m_windowSuportExtendedFrame == support)
        return;
    m_windowSuportExtendedFrame = support;
    emit windowSuportExtendedFrameChanged();
}

void MainCtx::setWindowExtendedMargin(unsigned int margin) {
    if (margin == m_windowExtendedMargin)
        return;
    m_windowExtendedMargin = margin;
    emit windowExtendedMarginChanged(margin);
}

/*****************************
 *   Main UI handling        *
 *****************************/

void MainCtx::loadPrefs(const bool callSignals)
{
    const auto loadFromVLCOption = [this, callSignals](auto &variable, const char *name
            , const std::function<void(MainCtx *)> signal)
    {
        using variableType = std::remove_reference_t<decltype(variable)>;

        const auto value =  loadVLCOption<variableType>(VLC_OBJECT(p_intf), name);
        if (value == variable)
            return;

        variable = value;
        if (callSignals && signal)
            signal(this);
    };

    /* "Start in Hide UI". Only read at start: the option says how VLC starts,
     * changing it must not leave the mode. The minimal view is now
     * Picture-in-picture, which never starts by itself. */
    if (!callSignals)
    {
        bool hideUIMode = false;
        loadFromVLCOption(hideUIMode, "qt-minimal-view", nullptr);
        setHideUI(hideUIMode);
    }

    loadFromVLCOption(m_bgCone, "qt-bgcone", &MainCtx::bgConeToggled);

    /* Should the UI stays on top of other windows */
    loadFromVLCOption(b_interfaceOnTop, "video-on-top", [this](MainCtx *)
    {
        emit interfaceAlwaysOnTopChanged(b_interfaceOnTop);
    });

    loadFromVLCOption(m_windowTitlebar, "qt-titlebar" , &MainCtx::useClientSideDecorationChanged);

    loadFromVLCOption(m_smoothScroll, "qt-smooth-scrolling", &MainCtx::smoothScrollChanged);

    loadFromVLCOption(m_maxVolume, "qt-max-volume", &MainCtx::maxVolumeChanged);

    loadFromVLCOption(m_pinVideoControls, "qt-pin-controls", &MainCtx::pinVideoControlsChanged);

    THEMPL->setPlayAddsToQueue(var_InheritBool(p_intf, "qt-play-adds-to-queue"));

    loadFromVLCOption(m_pinOpacity, "qt-fs-opacity", &MainCtx::pinOpacityChanged);

    loadFromVLCOption(m_safeArea, "qt-safe-area", &MainCtx::safeAreaChanged);

    loadFromVLCOption(m_mouseHideTimeout, "mouse-hide-timeout", &MainCtx::mouseHideTimeoutChanged);

    loadFromVLCOption(m_accentColor, "qt-accent-color", &MainCtx::accentColorChanged);

#ifdef _WIN32
    loadFromVLCOption(m_videoBlurOption, "qt-video-blur", &MainCtx::videoBlurOptionChanged);
#endif

    const bool reduceMotionWas = reduceMotion();
#ifdef _WIN32
    /* Windows "Animation effects" (Settings > Accessibility > Visual effects) */
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        m_systemReducedMotion = !animations;
#endif
    loadFromVLCOption(m_reduceMotionOption, "qt-reduce-motion", nullptr);
    if (callSignals && reduceMotion() != reduceMotionWas)
        emit reduceMotionChanged();

    /* the Shortcut message replaces the video's own OSD, it follows its option */
    m_shortcutMessage->setEnabled(var_InheritBool(p_intf, "osd"));
    m_shortcutMessage->setShowKeys(var_InheritBool(p_intf, "qt-osd-shortcut-keys"));
}

void MainCtx::loadFromSettingsImpl(const bool callSignals)
{
    const auto loadFromSettings = [this, callSignals](auto &variable, const char *name
            , const auto defaultValue, auto obj,  auto signal)
    {
        using variableType = std::remove_reference_t<decltype(variable)>;

        const auto value = getSettings()->value(name, defaultValue).template value<variableType>();
        if (value == variable)
            return;

        variable = value;
        if (obj && callSignals && signal)
            (obj->*signal)(variable);
    };

    loadFromSettings(m_playqueuePanel->m_docked, "MainWindow/playqueue-panel-docked", true, m_playqueuePanel, &SidePanelCtx::dockedChanged);
    loadFromSettings(m_playqueuePanel->m_visible, "MainWindow/playqueue-panel-visible", false, m_playqueuePanel, &SidePanelCtx::visibleChanged);
    loadFromSettings(m_playqueuePanel->m_width, "MainWindow/playqueue-panel-width", 120, m_playqueuePanel, &SidePanelCtx::widthChanged);

    loadFromSettings(m_navigationPanel->m_visible, "MainWindow/navigation-panel-visible", true, m_navigationPanel, &SidePanelCtx::visibleChanged);
    loadFromSettings(m_navigationPanel->m_width, "MainWindow/navigation-panel-width", 120, m_navigationPanel, &SidePanelCtx::widthChanged);

    loadFromSettings(m_playerPlaylistWidth, "MainWindow/player-playlist-width", 120, this, &MainCtx::playerPlaylistWidthChanged);
    loadFromSettings(m_artistAlbumsWidth, "MainWindow/artist-albums-width", 120, this,  &MainCtx::artistAlbumsWidthChanged);

    loadFromSettings(m_gridView, "MainWindow/grid-view", true, this, &MainCtx::gridViewChanged);

    loadFromSettings(m_showRemainingTime, "MainWindow/ShowRemainingTime", false, this, &MainCtx::showRemainingTimeChanged);

    loadFromSettings(m_albumSections, "MainWindow/album-sections", true, this, &MainCtx::albumSectionsChanged);

    loadFromSettings(m_lyricsMode, "MainWindow/lyrics-mode", true, this, &MainCtx::lyricsModeChanged);

    loadFromSettings(m_showWaveform, "MainWindow/show-waveform", true, this, &MainCtx::showWaveformChanged);

    loadFromSettings(m_showStatistics, "MainWindow/show-statistics", false, this, &MainCtx::showStatisticsChanged);

    loadFromSettings(m_showSubtitleTimeline, "MainWindow/show-subtitle-timeline", false, this, &MainCtx::showSubtitleTimelineChanged);

    const auto colorSchemeIndex = getSettings()->value( "MainWindow/color-scheme-index", 0 ).toInt();
    m_colorScheme->setCurrentIndex(colorSchemeIndex);

    /* user interface scale factor */
    auto userIntfScaleFactor = var_InheritFloat(p_intf, "qt-interface-scale");
    if (userIntfScaleFactor == -1)
        userIntfScaleFactor = getSettings()->value( "MainWindow/interface-scale", 1.0).toDouble();
    if (m_intfUserScaleFactor != userIntfScaleFactor)
    {
        m_intfUserScaleFactor = userIntfScaleFactor;
        updateIntfScaleFactor();
    }
}

void MainCtx::reloadPrefs()
{
    loadPrefs(true);
}

void MainCtx::storeProfileSettings()
{
    settings->setValue( "MainWindow/interface-scale", QString::number( m_intfUserScaleFactor ) );
    settings->setValue( "MainWindow/color-scheme-index", m_colorScheme->currentIndex() );
    settings->setValue( "MainWindow/show-waveform", m_showWaveform );
}

void MainCtx::loadProfileSettings()
{
    m_colorScheme->setCurrentIndex( settings->value( "MainWindow/color-scheme-index", 0 ).toInt() );

    // a scale given on the command line wins
    if ( var_InheritFloat( p_intf, "qt-interface-scale" ) == -1 )
        setIntfUserScaleFactor( settings->value( "MainWindow/interface-scale", 1.0 ).toDouble() );

    setShowWaveform( settings->value( "MainWindow/show-waveform", true ).toBool() );
}

void MainCtx::onInputChanged( bool hasInput )
{
    if( hasInput == false )
        return;
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( autoRaise == MainCtx::RAISE_NEVER )
        return;
    if( THEMIM->hasVideoOutput() == true )
    {
        if( ( autoRaise & MainCtx::RAISE_VIDEO ) == 0 )
            return;
    }
    else if ( ( autoRaise & MainCtx::RAISE_AUDIO ) == 0 )
        return;
    emit askRaise();
}

void MainCtx::onVOutChanged( bool video )
{
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( video && ( autoRaise & MainCtx::RAISE_VIDEO ) )
        emit askRaise();
}

#ifdef KeyPress
#undef KeyPress
#endif
void MainCtx::sendHotkey(Qt::Key key , Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers );
    int vlckey = qtEventToVLCKey(&event);
    sendVLCHotkey(vlckey);
}

void MainCtx::sendVLCHotkey(int vlcHotkey)
{
    /* the Shortcut message names the key */
    ShortcutMessage::KeyScope scope(vlcHotkey);
    var_SetInteger(vlc_object_instance(p_intf), "key-pressed", vlcHotkey);
}

void MainCtx::updateIntfScaleFactor()
{
    auto newValue = m_intfUserScaleFactor;
    if (QWindow* window = p_intf->p_compositor ? p_intf->p_compositor->interfaceMainWindow() : nullptr)
    {
        QScreen* screen = window->screen();
        if (screen)
        {
            qreal dpi = screen->logicalDotsPerInch();
            newValue = m_intfUserScaleFactor * dpi / VLC_REFERENCE_SCALE_FACTOR;
        }
    }

    const int FACTOR_RESOLUTION = 100;
    if ( static_cast<int>(m_intfScaleFactor * FACTOR_RESOLUTION)
         == static_cast<int>(newValue * FACTOR_RESOLUTION) ) return;

    m_intfScaleFactor = newValue;
    emit intfScaleFactorChanged();
}

void MainCtx::onWindowVisibilityChanged(QWindow::Visibility visibility)
{
    m_windowVisibility = visibility;
}

void MainCtx::setHasAcrylicSurface(const bool v)
{
    if (m_hasAcrylicSurface == v)
        return;

    m_hasAcrylicSurface = v;
    emit hasAcrylicSurfaceChanged(v);
}

void MainCtx::incrementIntfUserScaleFactor(bool increment)
{
    if (increment)
        setIntfUserScaleFactor(m_intfUserScaleFactor + 0.1);
    else
        setIntfUserScaleFactor(m_intfUserScaleFactor - 0.1);
}

void MainCtx::setIntfUserScaleFactor(double newValue)
{
    const auto value = qBound(getMinIntfUserScaleFactor(), newValue, getMaxIntfUserScaleFactor());
    if (qFuzzyCompare(value, m_intfUserScaleFactor))
        return;
    m_intfUserScaleFactor = value;
    updateIntfScaleFactor();
}

void MainCtx::setPinVideoControls(bool pinVideoControls)
{
    if (m_pinVideoControls == pinVideoControls)
        return;

    m_pinVideoControls = pinVideoControls;
    emit pinVideoControlsChanged();
}

void MainCtx::setHideUI(bool hideUI)
{
    if (m_hideUI == hideUI)
        return;

    m_hideUI = hideUI;
    emit hideUIChanged(m_hideUI);
}

void MainCtx::setPictureInPicture(bool pictureInPicture)
{
    if (m_pictureInPicture == pictureInPicture)
        return;

    m_pictureInPicture = pictureInPicture;
    // the window handler resizes the window, the minimal view draws it
    emit pictureInPictureChanged(m_pictureInPicture);
    setMinimalView(m_pictureInPicture);
}

void MainCtx::setPinOpacity(float pinOpacity)
{
    if (m_pinOpacity == pinOpacity)
        return;

    m_pinOpacity = pinOpacity;

    emit pinOpacityChanged();
}

inline void MainCtx::initSystray()
{
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    bool b_systrayWanted = var_InheritBool( p_intf, "qt-system-tray" );

    if( var_InheritBool( p_intf, "qt-start-minimized") )
    {
        if( b_systrayAvailable )
        {
            b_systrayWanted = true;
            b_hideAfterCreation = true;
        }
        else
            msg_Err( p_intf, "cannot start minimized without system tray bar" );
    }

    if( b_systrayAvailable && b_systrayWanted )
        m_systray = std::make_unique<VLCSystray>(this);
}

ThreadRunner* MainCtx::threadRunner() const
{
    return m_threadRunner;
}

QUrl MainCtx::folderMRL(const QString &fileMRL) const
{
    return folderMRL(QUrl::fromUserInput(fileMRL));
}

QUrl MainCtx::folderMRL(const QUrl &fileMRL) const
{
    if (fileMRL.isLocalFile())
    {
        const QString f = fileMRL.toLocalFile();
        return QUrl::fromLocalFile(QFileInfo(f).absoluteDir().absolutePath());
    }

    return {};
}

QString MainCtx::actionShortcut(const QString &action) const
{
    return actionKeyText(VLC_OBJECT(vlc_object_instance(p_intf)), action);
}

QString MainCtx::actionSequence(const QString &action) const
{
    return appActionSequence(VLC_OBJECT(vlc_object_instance(p_intf)), action);
}

void MainCtx::notifyKeyBindingsChanged()
{
    ++m_keyBindingsRevision;
    emit keyBindingsRevisionChanged();
}

void MainCtx::addBookmark()
{
    if (!m_medialib)
        return;

    if (!m_bookmarks)
    {
        m_bookmarks = new MLBookmarkModel(this);
        m_bookmarks->setPlayer(p_intf->p_player);
        m_bookmarks->setMl(m_medialib);
        connect(m_bookmarks, &MLBookmarkModel::added, this, &MainCtx::bookmarkAdded);
    }
    m_bookmarks->add();
}

void MainCtx::toggleAdjustments()
{
    if (getEffectiveMainInterfaceMode() == MAININTERFACE_MODE_PLAYER)
    {
        emit adjustmentsToggled();
        return;
    }
    /* the player view opens with the sidebar */
    m_pendingAdjustments = AdjustmentsLastTab;
    setPlayerView(true);
}

void MainCtx::openAdjustments(int tab)
{
    if (getEffectiveMainInterfaceMode() == MAININTERFACE_MODE_PLAYER)
    {
        emit adjustmentsRequested(tab);
        return;
    }
    m_pendingAdjustments = tab;
    setPlayerView(true);
}

int MainCtx::takePendingAdjustments()
{
    return std::exchange(m_pendingAdjustments, static_cast<int>(AdjustmentsNone));
}

QString MainCtx::displayMRL(const QUrl &mrl) const
{
    return urlToDisplayString(mrl);
}

void MainCtx::setPlayerPlaylistWidth( double factor )
{
    if (factor > 0.0)
    {
        m_playerPlaylistWidth = factor;
        emit playerPlaylistWidthChanged(factor);
    }
}

void MainCtx::setbgCone(bool bgCone)
{
    if (m_bgCone == bgCone)
        return;

    m_bgCone = bgCone;

    emit bgConeToggled();
}

void MainCtx::setMinimalView(bool enable)
{
    if (m_mainInterfaceModes.testFlag(MAININTERFACE_MODE_MINIMAL) == enable)
        return;

    m_mainInterfaceModes.setFlag(MAININTERFACE_MODE_MINIMAL, enable);
    emit mainInterfaceModesChanged(m_mainInterfaceModes);
}

void MainCtx::setPlayerView(bool enable)
{
    if (m_mainInterfaceModes.testFlag(MAININTERFACE_MODE_PLAYER) == enable)
        return;

    m_mainInterfaceModes.setFlag(MAININTERFACE_MODE_PLAYER, enable);
    emit mainInterfaceModesChanged(m_mainInterfaceModes);
    // the library switches between the Library pane (a list) and the full window
    if (m_gridView)
        emit gridViewChanged(hasGridView());
}

void MainCtx::setShowRemainingTime( bool show )
{
    m_showRemainingTime = show;
    emit showRemainingTimeChanged(show);
}

void MainCtx::setGridView(bool asGrid)
{
    m_gridView = asGrid;
    emit gridViewChanged( hasGridView() );
}

void MainCtx::setAlbumSections(bool enabled)
{
    if (m_albumSections == enabled)
        return;

    m_albumSections = enabled;
    emit albumSectionsChanged(enabled);
}

void MainCtx::setLyricsMode(bool enabled)
{
    if (m_lyricsMode == enabled)
        return;

    m_lyricsMode = enabled;
    emit lyricsModeChanged(enabled);
}

void MainCtx::setShowWaveform(bool enabled)
{
    if (m_showWaveform == enabled)
        return;

    m_showWaveform = enabled;
    emit showWaveformChanged(enabled);
}

void MainCtx::setShowStatistics(bool enabled)
{
    if (m_showStatistics == enabled)
        return;

    m_showStatistics = enabled;
    emit showStatisticsChanged(enabled);
}

void MainCtx::setShowSubtitleTimeline(bool enabled)
{
    if (m_showSubtitleTimeline == enabled)
        return;

    m_showSubtitleTimeline = enabled;
    emit showSubtitleTimelineChanged(enabled);
}

void MainCtx::setInterfaceAlwaysOnTop( bool on_top )
{
    if (b_interfaceOnTop == on_top)
        return;

    b_interfaceOnTop = on_top;
    emit interfaceAlwaysOnTopChanged(on_top);
}

bool MainCtx::hasEmbededVideo() const
{
    return m_videoSurfaceProvider && m_videoSurfaceProvider->hasVideoEmbed();
}

void MainCtx::setVideoSurfaceProvider(VideoSurfaceProvider* videoSurfaceProvider)
{
    if (m_videoSurfaceProvider)
        disconnect(m_videoSurfaceProvider, &VideoSurfaceProvider::hasVideoEmbedChanged, this, &MainCtx::hasEmbededVideoChanged);
    m_videoSurfaceProvider = videoSurfaceProvider;
    if (m_videoSurfaceProvider)
        connect(m_videoSurfaceProvider, &VideoSurfaceProvider::hasVideoEmbedChanged,
                this, &MainCtx::hasEmbededVideoChanged,
                Qt::QueuedConnection);
    emit hasEmbededVideoChanged(m_videoSurfaceProvider && m_videoSurfaceProvider->hasVideoEmbed());
}

QJSValue MainCtx::urlListToMimeData(const QJSValue &array) {
    // NOTE: Due to a Qt regression since 17318c4
    //       (Nov 11, 2022), it is not possible to
    //       use RFC-2483 compliant string here.
    //       This regression was later corrected by
    //       c25f53b (Jul 31, 2024).
    // NOTE: Qt starts supporting string list since
    //       17318c4, so starting from 6.5.0 a string
    //       list can be used which is not affected
    //       by the said issue. For Qt versions below
    //       6.5.0, use byte array which is used as is
    //       by Qt.
    assert(array.property("length").toInt() > 0);

    QJSEngine* const engine = qjsEngine(this);
    assert(engine);

    QJSValue data;
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    QString string;
    for (int i = 0; i < array.property(QStringLiteral("length")).toInt(); ++i)
    {
        QString decodedUrl;
        const QJSValue element = array.property(i);
        if (element.isUrl())
            // QJSValue does not have `toUrl()`
            decodedUrl = QJSManagedValue(element, engine).toUrl().toString(QUrl::FullyEncoded);
        else if (element.isString())
            // If the element is string, we assume it is already encoded
            decodedUrl = element.toString();
        else if (element.isVariant())
        {
            const QVariant variant = element.toVariant();
            if (variant.typeId() == QMetaType::QUrl)
                decodedUrl = variant.toUrl().toString(QUrl::FullyEncoded);
            else if (variant.typeId() == QMetaType::QString)
                decodedUrl = variant.toString();
            else
                Q_UNREACHABLE();
        }
        else
            Q_UNREACHABLE(); // Assertion failure in debug builds
        string += decodedUrl + QStringLiteral("\r\n");
    }
    string.chop(2);
    data = engine->toScriptValue(string);
#else
    data = array;
#endif
    QJSValue ret = engine->newObject();
    ret.setProperty(QStringLiteral("text/uri-list"), data);
    return ret;
}

VideoSurfaceProvider* MainCtx::getVideoSurfaceProvider() const
{
    return m_videoSurfaceProvider;
}

void MainCtx::setVideoBlurProvider(VideoBlurProvider* videoBlurProvider)
{
    if (m_videoBlurProvider == videoBlurProvider)
        return;
    m_videoBlurProvider = videoBlurProvider;
    emit videoBlurProviderChanged();
}

/************************************************************************
 * Events stuff
 ************************************************************************/

void MainCtx::onWindowClose( QWindow* )
{
    PlaylistController* playlistController = p_intf->p_mainPlaylistController;
    PlayerController* playerController = p_intf->p_mainPlayerController;

    if (m_videoSurfaceProvider)
        m_videoSurfaceProvider->onWindowClosed();
    //We need to make sure that noting is playing anymore otherwise the vout will be closed
    //after the main interface, and it requires (at least with OpenGL) that the OpenGL context
    //from the main window is still valid.
    //vlc_window_ReportClose is currently stubbed
    if (playerController && playerController->hasVideoOutput()) {
        connect(playerController, &PlayerController::playingStateChanged, [this](PlayerController::PlayingState state){
            if (state == PlayerController::PLAYING_STATE_STOPPED) {
                emit askToQuit();
            }
        });
        playlistController->stop();
    }
    else
    {
        emit askToQuit(); /* ask THEDP to quit, so we have a unique method */
    }
}

void MainCtx::toggleInterfaceFullScreen()
{
    emit setInterfaceFullScreen( m_windowVisibility != QWindow::FullScreen );
}

void MainCtx::emitBoss()
{
    emit askBoss();
}

void MainCtx::emitShow()
{
    emit askShow();
}

void MainCtx::emitRaise()
{
    emit askRaise();
}

VLCVarChoiceModel* MainCtx::getExtraInterfaces()
{
    return m_extraInterfaces;
}

bool MainCtx::pasteFromClipboard()
{
    assert(qApp);
    const QClipboard *const clipboard = qApp->clipboard();
    const QMimeData *const mimeData = clipboard ? clipboard->mimeData(QClipboard::Clipboard)
                                                : nullptr;
    const QStringList mrls = mimeData ? vlc::playlist::mrlsFromMimeData(*mimeData)
                                      : QStringList {};
    if (mrls.isEmpty())
    {
        emit clipboardHasNothingToOpen();
        return false;
    }

    QVector<vlc::playlist::Media> medias;
    QStringList streams;
    for (const QString &mrl : mrls)
    {
        medias.push_back(vlc::playlist::Media {mrl, QString {}});
        if (vlc::playlist::isNetworkMrl(mrl))
            streams.push_back(mrl);
    }
    THEMPL->playMedia(medias);

    /* Network links become Saved streams, as with Open Network Stream */
    if (!streams.isEmpty())
        m_savedStreams->save(streams);
    return true;
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t new_val, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;

    if( p_intf->pf_show_dialog )
    {
        p_intf->pf_show_dialog( p_intf->intf, INTF_DIALOG_POPUPMENU,
                                new_val.b_bool, NULL );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfShowCB: callback triggered by the intf-toggle-fscontrol libvlc variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitShow();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfRaiseMainCB: callback triggered by the intf-show-main libvlc variable.
 *****************************************************************************/
static int IntfRaiseMainCB( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitRaise();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfBossCB: callback triggered by the intf-boss libvlc variable.
 *****************************************************************************/
static int IntfBossCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitBoss();

    return VLC_SUCCESS;
}

bool MainCtx::acrylicActive() const
{
    return m_acrylicActive;
}

void MainCtx::setAcrylicActive(bool newAcrylicActive)
{
    if (m_acrylicActive == newAcrylicActive)
        return;

    m_acrylicActive = newAcrylicActive;
    emit acrylicActiveChanged();
}

bool MainCtx::preferHotkeys() const
{
    return m_preferHotkeys;
}

void MainCtx::setPreferHotkeys(bool enable)
{
    if (m_preferHotkeys == enable)
        return;

    m_preferHotkeys = enable;

    emit preferHotkeysChanged();
}

QWindow *MainCtx::intfMainWindow() const
{
    if (p_intf->p_compositor)
        return p_intf->p_compositor->interfaceMainWindow();
    else
        return nullptr;
}

QVariant MainCtx::settingValue(const QString &key, const QVariant &defaultValue) const
{
    return settings->value(key, defaultValue);
}

void MainCtx::setSettingValue(const QString &key, const QVariant &value)
{
    settings->setValue(key, value);
}

void MainCtx::setAttachedToolTip(QObject *toolTip)
{
    // See QQuickToolTipAttachedPrivate::instance(bool create)
    assert(toolTip);

    // Prevent possible invalid down-casting:
    assert(toolTip->inherits("QQuickToolTip"));

    QQmlEngine* const engine = qmlEngine(toolTip);
    assert(engine);
    assert(engine->objectOwnership(toolTip) == QQmlEngine::ObjectOwnership::JavaScriptOwnership);

    // Dynamic internal property:
#if QT_VERSION < QT_VERSION_CHECK(6, 12, 0)
    static const char* const name = "_q_QQuickToolTip";
#else
    static const char* const name = "_q_shared_QQuickToolTip";
#endif

    if (const auto obj = engine->property(name).value<QObject *>())
    {
        if (engine->objectOwnership(obj) == QQmlEngine::ObjectOwnership::CppOwnership)
            obj->deleteLater();
    }

    // setProperty() will return false, so there is no
    // need to check the return value:
    engine->setProperty(name, QVariant::fromValue(toolTip));

    // Check if the attached tooltip is actually the
    // one that is set
#ifndef NDEBUG
    QMetaObject::invokeMethod(toolTip, [toolTip]() {
        const auto engine = qmlEngine(toolTip);
        if (Q_UNLIKELY(!engine)) // Very unlikely, if not impossible
            return;
        QQmlComponent component(engine);
        component.setData(QByteArrayLiteral("import QtQuick; import QtQuick.Templates as T; Item { }"), {});
        QObject* const obj = component.create();
        assert(obj);
        // Consider disabling setting of custom attached
        // tooltip if the following assertion fails:
        if (QQmlProperty::read(obj, QStringLiteral("T.ToolTip.toolTip"), qmlContext(obj)).value<QObject*>() != toolTip)
            qmlWarning(obj) << "Could not set self as custom ToolTip!";
        obj->deleteLater();
    }, Qt::QueuedConnection);
#endif
}

MainCtx::MainInterfaceMode MainCtx::getEffectiveMainInterfaceMode() const {
    //priority applies across modes
    if (m_mainInterfaceModes & MainCtx::MAININTERFACE_MODE_MINIMAL)
        return MAININTERFACE_MODE_MINIMAL;
    else if (m_mainInterfaceModes & MAININTERFACE_MODE_PLAYER)
        return MAININTERFACE_MODE_PLAYER;
    else if (m_mainInterfaceModes & MAININTERFACE_MODE_MAINDISPLAY)
        return MAININTERFACE_MODE_MAINDISPLAY;
    else
        return MAININTERFACE_MODE_INVALID;
}

double MainCtx::dp(const double px, const double scale)
{
    return std::round(px * scale);
}

double MainCtx::dp(const double px) const
{
    return dp(px, m_intfScaleFactor);
}

bool MainCtx::useXmasCone() const
{
    return (QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY)
            && var_InheritBool( p_intf, "qt-icon-change" );
}

bool WindowStateHolder::holdFullscreen(QWindow *window, Source source, bool hold)
{
    QVariant prop = window->property("__windowFullScreen");
    bool ok = false;
    unsigned fullscreenCounter = prop.toUInt(&ok);
    if (!ok)
        fullscreenCounter = 0;

    if (hold)
        fullscreenCounter |= source;
    else
        fullscreenCounter &= ~source;

    Qt::WindowStates oldflags = window->windowStates();
    Qt::WindowStates newflags;

    if( fullscreenCounter != 0 )
        newflags = oldflags | Qt::WindowFullScreen;
    else
        newflags = oldflags & ~Qt::WindowFullScreen;

    window->setProperty("__windowFullScreen", QVariant::fromValue(fullscreenCounter));

    if( newflags != oldflags )
    {
        window->setWindowStates( newflags );
    }

    return fullscreenCounter != 0;
}

bool WindowStateHolder::holdOnTop(QWindow *window, Source source, bool hold)
{
    QVariant prop = window->property("__windowOnTop");
    bool ok = false;
    unsigned onTopCounter = prop.toUInt(&ok);
    if (!ok)
        onTopCounter = 0;

    if (hold)
        onTopCounter |= source;
    else
        onTopCounter &= ~source;

#ifdef _WIN32
    if (!window->handle()) // do not call hold on top if there is no platform window
        return false;
#endif

    if( onTopCounter != 0 )
    {
#ifdef _WIN32
        // Qt::WindowStaysOnTopHint is broken on Windows: QTBUG-133034, QTBUG-132522.
        // Reported to be fixed by Qt 6.9, but needs additional checking if the solution does not interfere with `CSDWin32EventHandler`.
        SetWindowPos(reinterpret_cast<HWND>(window->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
        window->setFlag(Qt::WindowStaysOnTopHint, true);
#endif
    }
    else
    {
#ifdef _WIN32
        // Qt::WindowStaysOnTopHint is broken on Windows: QTBUG-133034, QTBUG-132522.
        // Reported to be fixed by Qt 6.9, but needs additional checking if the solution does not interfere with `CSDWin32EventHandler`.
        SetWindowPos(reinterpret_cast<HWND>(window->winId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
        window->setFlag(Qt::WindowStaysOnTopHint, false);
#endif
    }

    window->setProperty("__windowOnTop", QVariant::fromValue(onTopCounter));

    return onTopCounter != 0;
}

double MainCtx::artistAlbumsWidth() const
{
    return dp(m_artistAlbumsWidth);
}

void MainCtx::setArtistAlbumsWidth(double newArtistAlbumsWidth)
{
    double unscaledValue = newArtistAlbumsWidth / getIntfScaleFactor();
    if (qFuzzyCompare(m_artistAlbumsWidth, unscaledValue))
        return;

    m_artistAlbumsWidth = unscaledValue;
    emit artistAlbumsWidthChanged( dp(m_artistAlbumsWidth) );
}
