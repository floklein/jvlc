/*****************************************************************************
 * main_interface.hpp : Main Interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_MAIN_INTERFACE_H_
#define QVLC_MAIN_INTERFACE_H_

#include "qt.hpp"

#include <QtQuick/QQuickView>
#include <QApplication>
#include <QQuickItem>
#include <QTimer>

Q_MOC_INCLUDE( "dialogs/toolbar/controlbar_profile_model.hpp" )
Q_MOC_INCLUDE( "dialogs/settings/profiles_model.hpp" )
Q_MOC_INCLUDE( "util/csdbuttonmodel.hpp" )
Q_MOC_INCLUDE( "playlist/playlist_controller.hpp" )
Q_MOC_INCLUDE( "maininterface/mainctx_submodels.hpp" )
Q_MOC_INCLUDE( "maininterface/videosurface.hpp" )
Q_MOC_INCLUDE( "maininterface/videoblur.hpp" )
Q_MOC_INCLUDE( "medialibrary/medialib.hpp" )
Q_MOC_INCLUDE( "player/player_controller.hpp" )
Q_MOC_INCLUDE( "util/color_scheme_model.hpp" )
class CSDButtonModel;
class QSettings;
class QCloseEvent;
class QKeyEvent;
class QLabel;
class QEvent;
class VideoWidget;
class VisualSelector;
class QVBoxLayout;
class QStackedLayout;
class QMenu;
class QSize;
class QTimer;
class QSystemTrayIcon;
class StandardPLPanel;
struct vlc_window;
class VideoSurfaceProvider;
class VideoBlurProvider;
class ControlbarProfileModel;
class ProfilesModel;
class SearchCtx;
class SortCtx;
class VLCSystray;
class MediaLib;
class SavedStreamsModel;
class MLBookmarkModel;
class ColorSchemeModel;
class PinsModel;
class PresetsModel;
class PresetController;
class VLCVarChoiceModel;
class SidePanelCtx;
class ShortcutMessage;
struct vlc_preparser_t;
class ThreadRunner;

namespace vlc {
namespace playlist {
}
}

class WindowStateHolder : public QObject
{
public:
    enum Source {
        INTERFACE = 1,
        VIDEO = 2,
        PICTURE_IN_PICTURE = 4,
    };

    static bool holdFullscreen( QWindow* window, Source source, bool hold );


    static bool holdOnTop( QWindow* window, Source source, bool hold );

};

class MainCtx : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double playerPlaylistWidth READ getPlayerPlaylistWidth WRITE setPlayerPlaylistWidth NOTIFY playerPlaylistWidthChanged FINAL)
    Q_PROPERTY(SidePanelCtx* playqueuePanel READ getPlayqueuePanel CONSTANT FINAL)
    Q_PROPERTY(SidePanelCtx* navigationPanel READ getNavigationPanel CONSTANT FINAL)
    Q_PROPERTY(double artistAlbumsWidth READ artistAlbumsWidth WRITE setArtistAlbumsWidth NOTIFY artistAlbumsWidthChanged FINAL)
    Q_PROPERTY(bool interfaceAlwaysOnTop READ isInterfaceAlwaysOnTop WRITE setInterfaceAlwaysOnTop NOTIFY interfaceAlwaysOnTopChanged FINAL)
    Q_PROPERTY(bool hasEmbededVideo READ hasEmbededVideo NOTIFY hasEmbededVideoChanged FINAL)
    Q_PROPERTY(bool showRemainingTime READ isShowRemainingTime WRITE setShowRemainingTime NOTIFY showRemainingTimeChanged FINAL)
    Q_PROPERTY(VLCVarChoiceModel* extraInterfaces READ getExtraInterfaces CONSTANT FINAL)
    Q_PROPERTY(double intfScaleFactor READ getIntfScaleFactor NOTIFY intfScaleFactorChanged FINAL)
    Q_PROPERTY(bool mediaLibraryAvailable READ hasMediaLibrary CONSTANT FINAL)
    Q_PROPERTY(MediaLib* mediaLibrary READ getMediaLibrary CONSTANT FINAL)
    Q_PROPERTY(bool gridView READ hasGridView WRITE setGridView NOTIFY gridViewChanged FINAL)
    Q_PROPERTY(bool hasGridListMode MEMBER m_hasGridListMode NOTIFY hasGridListModeChanged FINAL)
    Q_PROPERTY(ColorSchemeModel* colorScheme READ getColorScheme CONSTANT FINAL)
    Q_PROPERTY(bool hasVLM READ hasVLM CONSTANT FINAL)
    Q_PROPERTY(bool clientSideDecoration READ useClientSideDecoration NOTIFY useClientSideDecorationChanged FINAL)
    Q_PROPERTY(bool hasFirstrun READ hasFirstrun CONSTANT FINAL)
    Q_PROPERTY(int  csdBorderSize READ CSDBorderSize NOTIFY useClientSideDecorationChanged FINAL)
    Q_PROPERTY(bool canShowVideoPIP READ canShowVideoPIP CONSTANT FINAL)
    Q_PROPERTY(bool pinVideoControls READ pinVideoControls WRITE setPinVideoControls NOTIFY pinVideoControlsChanged FINAL)
    Q_PROPERTY(float pinOpacity READ pinOpacity WRITE setPinOpacity NOTIFY pinOpacityChanged FINAL)
    Q_PROPERTY(ControlbarProfileModel* controlbarProfileModel READ controlbarProfileModel CONSTANT FINAL)
    Q_PROPERTY(ProfilesModel* profiles READ profiles CONSTANT FINAL)
    Q_PROPERTY(bool hasAcrylicSurface READ hasAcrylicSurface NOTIFY hasAcrylicSurfaceChanged FINAL)
    Q_PROPERTY(bool smoothScroll READ smoothScroll NOTIFY smoothScrollChanged FINAL)
    Q_PROPERTY(QWindow* intfMainWindow READ intfMainWindow CONSTANT FINAL)
    Q_PROPERTY(bool useGlobalShortcuts READ getUseGlobalShortcuts WRITE setUseGlobalShortcuts NOTIFY useGlobalShortcutsChanged FINAL)
    Q_PROPERTY(int maxVolume READ maxVolume NOTIFY maxVolumeChanged FINAL)
    Q_PROPERTY(float safeArea READ safeArea NOTIFY safeAreaChanged FINAL)
    Q_PROPERTY(VideoSurfaceProvider* videoSurfaceProvider READ getVideoSurfaceProvider WRITE setVideoSurfaceProvider NOTIFY hasEmbededVideoChanged FINAL)
    Q_PROPERTY(VideoBlurProvider* videoBlurProvider READ videoBlurProvider NOTIFY videoBlurProviderChanged FINAL)
    Q_PROPERTY(int mouseHideTimeout READ mouseHideTimeout NOTIFY mouseHideTimeoutChanged FINAL)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged FINAL)
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY reduceMotionChanged FINAL)
    Q_PROPERTY(bool hideUI READ hideUI WRITE setHideUI NOTIFY hideUIChanged FINAL)
    // test hook for screenshots without input (VLC_QT_TEST_SHOW_BARS)
    Q_PROPERTY(QString testShowBars READ testShowBars CONSTANT FINAL)
    // the window shrunk into a small always-on-top video (minimal view)
    Q_PROPERTY(bool pictureInPicture READ pictureInPicture WRITE setPictureInPicture NOTIFY pictureInPictureChanged FINAL)
    // the desktop puts the window buttons on the left of title bars (Linux)
    Q_PROPERTY(bool windowButtonsOnLeft READ windowButtonsOnLeft NOTIFY windowButtonsOnLeftChanged FINAL)
    Q_PROPERTY(bool albumSections READ albumSections WRITE setAlbumSections NOTIFY albumSectionsChanged FINAL)
    Q_PROPERTY(bool lyricsMode READ lyricsMode WRITE setLyricsMode NOTIFY lyricsModeChanged FINAL)
    // Show waveform: the music player draws its seek bar as a waveform
    Q_PROPERTY(bool showWaveform READ showWaveform WRITE setShowWaveform NOTIFY showWaveformChanged FINAL)
    // Show Statistics: the player shows the statistics overlay
    Q_PROPERTY(bool showStatistics READ showStatistics WRITE setShowStatistics NOTIFY showStatisticsChanged FINAL)
    // Show subtitle timeline: the seek bar shows the subtitle lines as blocks
    Q_PROPERTY(bool showSubtitleTimeline READ showSubtitleTimeline WRITE setShowSubtitleTimeline NOTIFY showSubtitleTimelineChanged FINAL)
    // the player shows the Adjustments sidebar (set by the player view)
    Q_PROPERTY(bool adjustmentsOpened MEMBER m_adjustmentsOpened NOTIFY adjustmentsOpenedChanged FINAL)
    Q_PROPERTY(bool usingTouch READ usingTouch NOTIFY usingTouchChanged FINAL)
    Q_PROPERTY(CSDButtonModel *csdButtonModel READ csdButtonModel CONSTANT FINAL)
    Q_PROPERTY(MainInterfaceModes mainInterfaceModes READ getMainInterfaceModes NOTIFY mainInterfaceModesChanged FINAL)
    Q_PROPERTY(MainInterfaceMode effectiveMainInterfaceMode READ getEffectiveMainInterfaceMode NOTIFY mainInterfaceModesChanged FINAL)
    Q_PROPERTY(MainInterfaceMode initialEffectiveMainInterfaceMode READ getInitialEffectiveMainInterfaceMode CONSTANT FINAL)

    //Property to get Operating System info
    Q_PROPERTY(OsType osName READ getOSName CONSTANT)
    Q_PROPERTY(int osVersion READ getOSVersion CONSTANT)

    // Expose Property Minimal View for Player View
    Q_PROPERTY(bool minimalView READ isMinimalView WRITE setMinimalView NOTIFY mainInterfaceModesChanged FINAL)
    Q_PROPERTY(bool playerView READ isPlayerView WRITE setPlayerView NOTIFY mainInterfaceModesChanged FINAL)
    // The library shows as the Library pane beside the video in the player
    // view, as the full-window library otherwise. The Library pane is a list
    // only (gridView reads false there) with a compact header.
    Q_PROPERTY(bool libraryPane READ isPlayerView NOTIFY mainInterfaceModesChanged FINAL)

    // This Property only works if hasAcrylicSurface is set
    Q_PROPERTY(bool acrylicActive READ acrylicActive WRITE setAcrylicActive NOTIFY acrylicActiveChanged FINAL)

    // NOTE: This is useful when we want to prioritize player hotkeys over QML keyboard navigation.
    Q_PROPERTY(bool preferHotkeys READ preferHotkeys WRITE setPreferHotkeys NOTIFY preferHotkeysChanged FINAL)

    //Property for Activating bgCone in player view
    Q_PROPERTY(bool bgCone READ isbgCone WRITE setbgCone NOTIFY bgConeToggled FINAL)
    Q_PROPERTY(bool windowSuportExtendedFrame READ windowSuportExtendedFrame NOTIFY windowSuportExtendedFrameChanged)
    Q_PROPERTY(unsigned windowExtendedMargin READ windowExtendedMargin WRITE setWindowExtendedMargin NOTIFY windowExtendedMarginChanged)
    Q_PROPERTY(SearchCtx* search MEMBER m_search CONSTANT FINAL)
    Q_PROPERTY(SortCtx* sort MEMBER m_sort CONSTANT FINAL)
    /// changes when Keyboard Shortcuts saves new keys: read it in bindings
    /// that call actionSequence() or actionShortcut() to follow them
    Q_PROPERTY(int keyBindingsRevision MEMBER m_keyBindingsRevision NOTIFY keyBindingsRevisionChanged FINAL)


public:
    /* tors */
    MainCtx(qt_intf_t *);
    virtual ~MainCtx();

    static const QEvent::Type ToolbarsNeedRebuild;
    static constexpr double MIN_INTF_USER_SCALE_FACTOR = 0.3;
    static constexpr double MAX_INTF_USER_SCALE_FACTOR = 3.0;

public:
    /* Getters */
    inline qt_intf_t* getIntf() const { return p_intf; }
    inline vlc_preparser_t *getNetworkPreparser() const { return m_network_preparser; };
    bool smoothScroll() const { return m_smoothScroll; }

    VLCSystray* getSysTray() { return m_systray.get(); }

    enum
    {
        CONTROLS_VISIBLE  = 0x1,
        CONTROLS_HIDDEN   = 0x2,
        CONTROLS_ADVANCED = 0x4,
    };
    enum
    {
        RAISE_NEVER,
        RAISE_VIDEO,
        RAISE_AUDIO,
        RAISE_AUDIOVIDEO,
    };

    enum OsType
    {
        Windows,
        Unknown
    };
    Q_ENUM(OsType)

    //what should the main view display
    //multiple modes may be enabled, priority applies across modes
    //minimal > player > medialib
    enum MainInterfaceMode {
        MAININTERFACE_MODE_INVALID = 0,
        MAININTERFACE_MODE_MAINDISPLAY = 1,
        MAININTERFACE_MODE_PLAYER = 2,
        MAININTERFACE_MODE_MINIMAL = 4
    };
    Q_FLAG(MainInterfaceMode);
    Q_DECLARE_FLAGS(MainInterfaceModes, MainInterfaceMode)

    inline QWindow::Visibility interfaceVisibility() const { return m_windowVisibility; }
    inline SidePanelCtx* getPlayqueuePanel() const { return m_playqueuePanel; }
    inline SidePanelCtx* getNavigationPanel() const { return m_navigationPanel; }
    inline double getPlayerPlaylistWidth() const { return m_playerPlaylistWidth; }
    bool isInterfaceAlwaysOnTop() { return b_interfaceOnTop; }
    inline bool isHideAfterCreation() const { return b_hideAfterCreation; }
    inline bool isShowRemainingTime() const  { return m_showRemainingTime; }
    inline double getIntfScaleFactor() const { return m_intfScaleFactor; }
    Q_INVOKABLE inline double getIntfUserScaleFactor() const { return m_intfUserScaleFactor; }
    inline int CSDBorderSize() const { return 10; }
    inline double getMinIntfUserScaleFactor() const { return MIN_INTF_USER_SCALE_FACTOR; }
    inline double getMaxIntfUserScaleFactor() const { return MAX_INTF_USER_SCALE_FACTOR; }
    inline bool hasMediaLibrary() const { return b_hasMedialibrary; }
    inline MediaLib* getMediaLibrary() const { return m_medialib; }
    inline ShortcutMessage* shortcutMessage() const { return m_shortcutMessage; }
    inline SavedStreamsModel* getSavedStreams() const { return m_savedStreams; }
    // the Library pane is a list only
    inline bool hasGridView() const { return m_gridView && !isPlayerView(); }
    inline bool albumSections() const { return m_albumSections; }
    inline bool lyricsMode() const { return m_lyricsMode; }
    inline bool showWaveform() const { return m_showWaveform; }
    inline bool showStatistics() const { return m_showStatistics; }
    inline bool showSubtitleTimeline() const { return m_showSubtitleTimeline; }
    inline ColorSchemeModel* getColorScheme() const { return m_colorScheme; }
    inline PinsModel* getPins() const { return m_pins; }
    // the Preset of the playing file and the list of Presets
    inline PresetController* getPresets() const { return m_presetController; }
    bool hasVLM() const;
    bool useClientSideDecoration() const;
    bool hasFirstrun() const;
    inline bool canShowVideoPIP() const { return m_canShowVideoPIP; }
    inline void setCanShowVideoPIP(bool canShowVideoPIP) { m_canShowVideoPIP = canShowVideoPIP; }

    inline bool pinVideoControls() const { return m_pinVideoControls; }
    inline float pinOpacity() const { return m_pinOpacity; }

    inline ControlbarProfileModel* controlbarProfileModel() const { return m_controlbarProfileModel; }
    inline ProfilesModel* profiles() const { return m_profiles; }
    inline QUrl getDialogFilePath() const { return m_dialogFilepath; }
    inline void setDialogFilePath(const QUrl& filepath ){ m_dialogFilepath = filepath; }
    inline bool hasAcrylicSurface() const { return m_hasAcrylicSurface; }
    inline void reloadFromSettings() { loadFromSettingsImpl(true); }
    inline bool getUseGlobalShortcuts() const { return m_useGlobalShortcuts; }
    void setUseGlobalShortcuts(bool useGlobalShortcuts );
    inline int maxVolume() const { return m_maxVolume; }

    inline float safeArea() const { return m_safeArea; }

    inline OsType getOSName() const {return m_osName;}
    inline int getOSVersion() const {return m_osVersion;}

    inline bool isbgCone() const {return m_bgCone; }
    inline bool isMinimalView() const {return m_mainInterfaceModes & MAININTERFACE_MODE_MINIMAL; }
    inline bool isPlayerView() const {return m_mainInterfaceModes & MAININTERFACE_MODE_PLAYER; }


    inline bool windowSuportExtendedFrame() const { return m_windowSuportExtendedFrame; }
    inline unsigned windowExtendedMargin() const { return m_windowExtendedMargin; }
    void setWindowSuportExtendedFrame(bool support);
    void setWindowExtendedMargin(unsigned margin);

    bool hasEmbededVideo() const;
    VideoSurfaceProvider* getVideoSurfaceProvider() const;
    void setVideoSurfaceProvider(VideoSurfaceProvider* videoSurfaceProvider);

    // null when the compositor can not draw the Video blur, or while it is off
    // (the setting, Windows transparency effects, battery saver)
    VideoBlurProvider* videoBlurProvider() const { return m_videoBlurProvider; }
    void setVideoBlurProvider(VideoBlurProvider* videoBlurProvider);
    // the "Blur the video behind panels" setting (qt-video-blur)
    bool videoBlurOption() const { return m_videoBlurOption; }

    int mouseHideTimeout() const { return m_mouseHideTimeout; }

    // the qt-accent-color option: "default", "system" or a color
    inline QString accentColor() const { return m_accentColor; }

    // fades only, no slides: the Settings toggle or the system setting
    inline bool reduceMotion() const { return m_reduceMotionOption || m_systemReducedMotion; }

    // the bars stay hidden until the pointer nears an edge, as in fullscreen
    bool hideUI() const { return m_hideUI; }

    // The VLC_QT_TEST_SHOW_BARS environment variable: when set, the player
    // shows its bars and keeps them; "queue", "adjustments" or "library" also
    // opens that panel. Empty when unset.
    QString testShowBars() const { return qEnvironmentVariable("VLC_QT_TEST_SHOW_BARS"); }

    bool pictureInPicture() const { return m_pictureInPicture; }
    bool windowButtonsOnLeft() const { return m_windowButtonsOnLeft; }

    // the size of the last video the video output asked for, in pixels
    QSize videoSize() const { return m_videoSize; }
    void setVideoSize(const QSize& size) { m_videoSize = size; }

    bool usingTouch() const { return m_usingTouch; }

    Q_INVOKABLE bool backdropBlurRequested() const { return var_InheritBool(p_intf, "qt-backdrop-blur"); }

    Q_INVOKABLE static inline void setCursor(Qt::CursorShape cursor) { QApplication::setOverrideCursor(QCursor(cursor)); }
    Q_INVOKABLE static inline void restoreCursor(void) { QApplication::restoreOverrideCursor(); }

    Q_INVOKABLE static inline void setCursor(QQuickItem* item, Qt::CursorShape cursor) { assert(item); item->setCursor(cursor); }
    Q_INVOKABLE static inline void unsetCursor(QQuickItem* item) { assert(item); item->unsetCursor(); };

    Q_INVOKABLE static /*constexpr*/ inline unsigned int qtVersion() { return QT_VERSION; }
    Q_INVOKABLE static /*constexpr*/ inline unsigned int qtVersionCheck(unsigned char major,
                                                                        unsigned char minor,
                                                                        unsigned char patch)
                                                                       { return QT_VERSION_CHECK(major, minor, patch); }

    Q_INVOKABLE static /*constexpr*/ inline bool qtQuickControlRejectsHoverEvents() {
        // QTBUG-100543
        return (QT_VERSION < QT_VERSION_CHECK(6, 3, 0) && QT_VERSION >= QT_VERSION_CHECK(6, 2, 5)) ||
               (QT_VERSION < QT_VERSION_CHECK(6, 4, 0) && QT_VERSION >= QT_VERSION_CHECK(6, 3, 1)) ||
               (QT_VERSION >= QT_VERSION_CHECK(6, 4, 0));
    }

    Q_INVOKABLE QJSValue urlListToMimeData(const QJSValue& array);

    Q_INVOKABLE static void setFiltersChildMouseEvents(QQuickItem *item, bool enable)
    {
        assert(item);
        item->setFiltersChildMouseEvents(enable);
    }

    Q_INVOKABLE static void setItemFlag(QQuickItem *item, QQuickItem::Flag flag, bool enabled = true)
    {
        assert(item);
        item->setFlag(flag, enabled);
    }

    Q_INVOKABLE qreal effectiveDevicePixelRatio(const QQuickWindow* window) {
        if (window)
            return window->effectiveDevicePixelRatio();
        else
        {
            // This is useful when item no longer has window when getting removed from the scene,
            // which happens when the view changes. We should return the same value so that the
            // images are not loaded just before getting destroyed due to source size change.
            // Different windows have the same device pixel ratio, so this should not be a
            // problem. At the same time, don't use `QGuiApplication::devicePixelRatio()` as
            // it is not valid on Wayland and we prefer to return 0.0 rather than an intermediate
            // or invalid value here to intent not to load an image just to discard them afterwards.
            const auto window = intfMainWindow();
            const auto quickWindow = qobject_cast<QQuickWindow*>(window);
            if (quickWindow)
                return quickWindow->effectiveDevicePixelRatio();
            else if (window)
                return window->devicePixelRatio();
        }

        // Return 0.0 to indicate not to load the image, as we know that this is an intermediate
        // situation and we don't want to load image just to discard them after there is a window.
        // QQuickImage still loads image with (0, 0) source size, but at least we indicate our
        // intention and this can be handled manually in the future if necessary. Currently,
        // intfMainWindow() is always available, so in only rare cases this would return 0.0.
        return 0.0;
    }

    Q_INVOKABLE static bool windowHasDepthBuffer(const QWindow* window)
    {
        assert(window);
        // Make sure window has been initialized before (such as `::create()` was called):
        assert(window->handle());
        return window->format().depthBufferSize() > 0;
    }

    // TODO: This functions is akin to Window Web API's `setTimeout()`. Get rid of this once
    //       once Qt implements it.
    Q_INVOKABLE static void setTimeout(const QJSValue& func, const int delay, const QJSValueList& params = {}, const QObject* context = nullptr)
    {
        assert(func.isCallable());
        QTimer::singleShot(delay, context, [func, params]() {
            func.call(params);
        });
    }

    Q_INVOKABLE virtual bool platformHandlesResizeWithCSD() const { return false; };
    Q_INVOKABLE virtual bool platformHandlesTitleBarButtonsWithCSD() const { return false; };
    Q_INVOKABLE virtual bool platformHandlesShadowsWithCSD() const { return false; };

    void setUsingTouch(bool touch)
    {
        if (m_usingTouch == touch)
            return;
        m_usingTouch = touch;
        usingTouchChanged();
    }

    /**
     * @brief ask for the application to terminate
     */
    void onWindowClose(QWindow* );

    bool acrylicActive() const;
    void setAcrylicActive(bool newAcrylicActive);

    bool preferHotkeys() const;
    void setPreferHotkeys(bool enable);

    QWindow *intfMainWindow() const;

    Q_INVOKABLE QVariant settingValue(const QString &key, const QVariant &defaultValue) const;
    Q_INVOKABLE void setSettingValue(const QString &key, const QVariant &value);

    Q_INVOKABLE static void setAttachedToolTip(QObject* toolTip);

    CSDButtonModel *csdButtonModel() { return m_csdButtonModel.get(); }

    inline MainInterfaceModes getMainInterfaceModes() const { return m_mainInterfaceModes; };
    MainInterfaceMode getEffectiveMainInterfaceMode() const;

    MainInterfaceMode getInitialEffectiveMainInterfaceMode() const { return m_initialEffectiveMainInterfaceMode; }

    Q_INVOKABLE static double dp(const double px, const double scale);
    Q_INVOKABLE double dp(const double px) const;

    Q_INVOKABLE bool useXmasCone() const;

    ThreadRunner* threadRunner() const;

    Q_INVOKABLE QUrl folderMRL(const QString &fileMRL) const;
    Q_INVOKABLE QUrl folderMRL(const QUrl &fileMRL) const;

    Q_INVOKABLE QString displayMRL(const QUrl &mrl) const;

    /// the key currently bound to @a action, a core hotkey ("play-pause") or
    /// an interface action ("open-folder"), for menus, accessible names and
    /// tooltips; empty when the action has no key
    Q_INVOKABLE QString actionShortcut(const QString &action) const;

    /// the key sequence of the interface action @a action ("open-folder"),
    /// or of a core hotkey action ("quit"), for the interface's shortcuts
    Q_INVOKABLE QString actionSequence(const QString &action) const;

    /// Keyboard Shortcuts saved new keys
    void notifyKeyBindingsChanged();

    /// add a bookmark at the current time of the playing media;
    /// bookmarkAdded() confirms it
    Q_INVOKABLE void addBookmark();

    /// the Adjustments sidebar of the player, which shares the right-hand
    /// slot with the Queue panel. Tabs: Playback, Video, Audio, Subtitles
    /// (RightPanelFSM.Tab). From the library, the player view opens first.
    enum AdjustmentsRequest { AdjustmentsLastTab = -1, AdjustmentsNone = -2 };
    Q_ENUM(AdjustmentsRequest)
    /// the adjustments button, Ctrl+E and the menus: open on the last tab,
    /// or close
    Q_INVOKABLE void toggleAdjustments();
    /// "More settings" of the Track menu: open on @a tab
    Q_INVOKABLE void openAdjustments(int tab);
    /// the request left for a player view that was not open, or
    /// AdjustmentsNone
    Q_INVOKABLE int takePendingAdjustments();
    /// Jump to Time from the library: true when the player view shows, else
    /// it opens and takePendingJumpToTime() tells it to show Jump to Time
    bool requestJumpToTime();
    Q_INVOKABLE bool takePendingJumpToTime();

    double artistAlbumsWidth() const;
    void setArtistAlbumsWidth(double newArtistAlbumsWidth);


protected:
    /* Systray */
    void initSystray();

    qt_intf_t* p_intf = nullptr;
    vlc_preparser_t *m_network_preparser = nullptr;

    bool m_hasEmbededVideo = false;
    VideoSurfaceProvider* m_videoSurfaceProvider = nullptr;
    VideoBlurProvider* m_videoBlurProvider = nullptr;
    bool m_showRemainingTime = false;

    /* */
    QSettings           *settings = nullptr;

    std::unique_ptr<VLCSystray> m_systray;

    /* Flags */
    double               m_intfUserScaleFactor = 1.;
    double               m_intfScaleFactor = 1.;
    int                  i_notificationSetting = 0; /// Systray Notifications
    bool                 b_hideAfterCreation = false; /// --qt-start-minimized
    SidePanelCtx*   m_playqueuePanel = nullptr;
    SidePanelCtx*   m_navigationPanel = nullptr;
    QWindow::Visibility  m_windowVisibility = QWindow::Windowed;
    bool                 b_interfaceOnTop = false;      ///keep UI on top
    bool                 b_hasWayland = false;
    bool                 b_hasMedialibrary = false;
    MediaLib*            m_medialib = nullptr;
    SavedStreamsModel*   m_savedStreams = nullptr;
    MLBookmarkModel*     m_bookmarks = nullptr;
    bool                 m_gridView = false;
    bool                 m_hasGridListMode = false;
    ColorSchemeModel*    m_colorScheme = nullptr;
    PinsModel*           m_pins = nullptr;
    PresetsModel*        m_presets = nullptr;
    PresetController*    m_presetController = nullptr;
    bool                 m_windowTitlebar = true;
    // NOTE: Ideally this should be a QVLCBool.
    bool                 m_canShowVideoPIP = false;

    /* Pinned */
    bool                 m_pinVideoControls = false;
    float                m_pinOpacity       = 1.0f;

    bool                 m_useGlobalShortcuts = true;
    QUrl                 m_dialogFilepath; /* Last path used in dialogs */

    /* States */
    double               m_playerPlaylistWidth = 120;
    MainInterfaceModes   m_mainInterfaceModes = { MAININTERFACE_MODE_MAINDISPLAY };
    MainInterfaceMode    m_initialEffectiveMainInterfaceMode = MAININTERFACE_MODE_MAINDISPLAY;

    double               m_artistAlbumsWidth = 120;

    VLCVarChoiceModel* m_extraInterfaces = nullptr;

    ControlbarProfileModel* m_controlbarProfileModel = nullptr;
    ProfilesModel* m_profiles = nullptr;

    bool m_hasAcrylicSurface = false;
    bool m_acrylicActive = false;

    bool m_smoothScroll = true;

    bool m_preferHotkeys = false;

    int m_maxVolume = 125;

    float m_safeArea = 0.0;

    int m_mouseHideTimeout = 1000;

    QString m_accentColor = QStringLiteral("default");

    bool m_reduceMotionOption = false;
    bool m_systemReducedMotion = false;

    bool m_videoBlurOption = true;

    bool m_hideUI = false;
    bool m_pictureInPicture = false;
    bool m_windowButtonsOnLeft = false;
    QSize m_videoSize;

    bool m_albumSections = true;

    bool m_lyricsMode = true;
    bool m_showWaveform = true;
    bool m_showStatistics = false;
    bool m_showSubtitleTimeline = false;
    int m_pendingAdjustments = AdjustmentsNone;
    bool m_pendingJumpToTime = false;
    bool m_adjustmentsOpened = false;

    bool m_usingTouch = false;

    OsType m_osName;
    int m_osVersion;

    bool m_bgCone = true;
    int m_keyBindingsRevision = 0;
    bool m_windowSuportExtendedFrame = false;
    unsigned m_windowExtendedMargin = 0;

    std::unique_ptr<CSDButtonModel> m_csdButtonModel;

    SearchCtx* m_search = nullptr;
    SortCtx* m_sort = nullptr;
    ShortcutMessage* m_shortcutMessage = nullptr;


    ThreadRunner* m_threadRunner = nullptr;

public slots:
    void toggleInterfaceFullScreen();
    void setPlayerPlaylistWidth( double factor );
    void setInterfaceAlwaysOnTop( bool );
    void setShowRemainingTime( bool );
    void setGridView( bool );
    void setAlbumSections( bool );
    void setLyricsMode( bool );
    void setShowWaveform( bool );
    void setShowStatistics( bool );
    void setShowSubtitleTimeline( bool );
    void incrementIntfUserScaleFactor( bool increment);
    void setIntfUserScaleFactor( double );

    void setPinVideoControls( bool );
    void setPinOpacity( float );

    void setHideUI( bool );
    void setPictureInPicture( bool );

    void setbgCone(bool);
    void updateIntfScaleFactor();
    void onWindowVisibilityChanged(QWindow::Visibility);
    void setHasAcrylicSurface(bool);

    void setMinimalView(bool);
    void setPlayerView(bool);

    void sendHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers );
    void sendVLCHotkey(int vlcHotkey);

    void emitBoss();
    void emitRaise();
    void emitShow();

    virtual void reloadPrefs();
    VLCVarChoiceModel* getExtraInterfaces();

    /* Open from Clipboard: play its links and file paths by the Play rule */
    bool pasteFromClipboard();

protected slots:
    void onInputChanged( bool );
    void onVOutChanged( bool );

signals:
    void fullscreenInterfaceToggled( bool );
    void setInterfaceVisibible(bool, bool requestActivate = false);
    void setInterfaceFullScreen( bool );
    void toggleWindowVisibility(bool requestActivate = false);
    void askToQuit();
    /// addBookmark() added the bookmark @a name
    void bookmarkAdded(const QString &name);
    void keyBindingsRevisionChanged();
    void windowButtonsOnLeftChanged();
    void askShow();
    void askBoss();
    void askRaise();
    void kc_pressed(); /* easter eggs */
    void clipboardHasNothingToOpen();

    void playerPlaylistWidthChanged(double);
    void interfaceAlwaysOnTopChanged(bool);
    void hasEmbededVideoChanged(bool);
    void videoBlurProviderChanged();
    void videoBlurOptionChanged();
    void showRemainingTimeChanged(bool);
    void gridViewChanged( bool );
    void hasGridListModeChanged( bool );
    void albumSectionsChanged( bool );
    void lyricsModeChanged( bool );
    void showWaveformChanged( bool );
    void showStatisticsChanged( bool );
    void showSubtitleTimelineChanged( bool );
    /// the player view opens or closes the Adjustments sidebar
    void adjustmentsToggled();
    void adjustmentsRequested( int tab );
    void adjustmentsOpenedChanged();
    void colorSchemeChanged( QString );
    void useClientSideDecorationChanged();

    /// forward window maximise query to the actual window or widget
    void requestInterfaceMaximized();
    /// forward window normal query to the actual window or widget
    void requestInterfaceNormal();
    /// forward window normal query to the actual window or widget
    void requestInterfaceMinimized();

    void intfScaleFactorChanged();
    void intfDevicePixelRatioChanged();

    void pinVideoControlsChanged();
    void pinOpacityChanged();

    void hasAcrylicSurfaceChanged(bool);

    void acrylicActiveChanged();

    void smoothScrollChanged();

    void preferHotkeysChanged();

    void useGlobalShortcutsChanged( bool );

    void maxVolumeChanged();

    void safeAreaChanged();

    void mouseHideTimeoutChanged();

    void accentColorChanged();

    void reduceMotionChanged();

    void hideUIChanged(bool hideUI);
    void pictureInPictureChanged(bool pictureInPicture);

    void navBoxToggled();

    void bgConeToggled();
    void windowSuportExtendedFrameChanged();
    void windowExtendedMarginChanged(unsigned margin);

    void artistAlbumsWidthChanged( double );

    void mainInterfaceModesChanged(MainInterfaceModes);

    void usingTouchChanged();

private:
    void loadPrefs(bool callSignals);
    void loadFromSettingsImpl(bool callSignals);
    // the Theme, scale and Show waveform of the active Profile, to and from the Qt settings
    void storeProfileSettings();
    void loadProfileSettings();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MainCtx::MainInterfaceModes)

#endif
