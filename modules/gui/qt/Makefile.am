guidir = $(pluginsdir)/gui
gui_LTLIBRARIES =
gui_PLUGINS =
gui_RPATH =

if HAVE_PARTIAL_LINKING
noinst_LTLIBRARIES = $(gui_PLUGINS)
else
gui_LTLIBRARIES += $(gui_PLUGINS)
gui_RPATH += -rpath '$(guidir)'
endif


## Howto
# For each Q_OBJECT:
#   - Add *.moc.cpp to nodist_..._SOURCES
#   - Add *.cpp to ..._SOURCES
#   - Add *.hpp to ..._SOURCES
# For each UI
#   - Add *.ui to ..._UI
#   - Add *.h to nodist_..._SOURCES
# For each resource (icon, png, ...)
#   - Add it to DEPS_res
# For each qml and js resource
#   - Add it to ..._QML

CLEANFILES =

include ../../common.am

# the OpenSubtitles consumer key and endpoint, from configure
opensubtitles_config.h: Makefile
	$(AM_V_GEN)printf '#define OPENSUBTITLES_API_KEY "%s"\n#define OPENSUBTITLES_API_URL "%s"\n' \
		'$(OPENSUBTITLES_API_KEY)' '$(OPENSUBTITLES_API_URL)' > $@.tmp && mv -f $@.tmp $@
BUILT_SOURCES += opensubtitles_config.h

# Generate vlc_modules_list for Qt plugins
include ../../modulelist.mk

SUFFIXES += .ui .h .hpp .moc.cpp .qml .js .mjs .moc
TEST_EXTENSIONS = .qml
EXTRA_DIST = qt6.pro qtest.pro quicktest.pro scripts
vlc_qt_libs = $(QT_LIBS)
RCC_EXTRA_PARAMS =

all-local: $(VLC_QT_CHECK_DEP) $(QML_MODULE_STAMPS)

if !HAVE_QT_ZSTD
RCC_EXTRA_PARAMS += --no-zstd
endif

vlc_qt_check_SOURCES = vlc-qt-check.cpp plugins.hpp
vlc_qt_check_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC $(CXXFLAGS_qt)
vlc_qt_check_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_qt)
vlc_qt_check_LDADD = $(vlc_qt_libs)
vlc_qt_check_LDFLAGS = $(AM_LDFLAGS) $(QT_LDFLAGS)

libqt_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_qt) \
	-DQT_STRICT_ITERATORS \
	-DQT_USE_QSTRINGBUILDER \
	-DQT_NO_CAST_TO_ASCII \
	-DQT_NO_CAST_FROM_BYTEARRAY \
	-I$(srcdir) -I$(builddir) \
	-I$(builddir)/dialogs/open \
	-I$(builddir)/dialogs/sout

libqt_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) $(CXXFLAGS_qt) -fPIC
libqt_plugin_la_LIBADD = $(vlc_qt_libs)
if HAVE_LIBATOMIC
libqt_plugin_la_CXXFLAGS += -DQT_HAS_LIBATOMIC
libqt_plugin_la_LIBADD += -latomic
endif
libqt_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(QT_LDFLAGS) $(GC_SECTIONS_LDFLAGS)
if HAVE_OSX
libqt_plugin_la_LDFLAGS += -Wl,-framework,Cocoa
endif

if HAVE_X
if HAVE_XCB
libqt_plugin_la_CXXFLAGS += $(XCB_CFLAGS) -DQT_HAS_X11
libqt_plugin_la_LIBADD += $(XCB_LIBS) $(X_LIBS) $(X_PRE_LIB) -lX11
vlc_qt_check_CXXFLAGS += $(XCB_CFLAGS) -DQT_HAS_X11
endif
endif

if HAVE_WIN32
libqt_plugin_la_CXXFLAGS += $(LIBCOMCXXFLAGS)
libqt_plugin_la_LIBADD += $(LIBCOM) -lcomctl32 -luuid -ldwmapi
endif
if UPDATE_CHECK
libqt_plugin_la_CPPFLAGS += -DUPDATE_CHECK
endif
if QT_QML_DEBUG
libqt_plugin_la_CPPFLAGS += -DQT_QML_DEBUG
endif

libqt_plugin_la_SOURCES = \
	qt.cpp qt.hpp plugins.hpp \
	dialogs/capture/capture_preview.cpp dialogs/capture/capture_preview.hpp \
	dialogs/capture/open_capture_model.cpp dialogs/capture/open_capture_model.hpp \
	dialogs/dialogs/dialogmodel.cpp \
	dialogs/dialogs/dialogmodel.hpp \
	dialogs/dialogs_provider.cpp dialogs/dialogs_provider.hpp \
	dialogs/extensions/extensions.cpp dialogs/extensions/extensions.hpp \
	dialogs/extensions/extensions_manager.cpp dialogs/extensions/extensions_manager.hpp \
	dialogs/fingerprint/chromaprint.cpp \
	dialogs/fingerprint/chromaprint.hpp \
	dialogs/help/aboutmodel.hpp \
	dialogs/epg/programguidemodel.cpp dialogs/epg/programguidemodel.hpp \
	dialogs/messages/messagesmodel.cpp dialogs/messages/messagesmodel.hpp \
	dialogs/mediainfo/media_properties_model.cpp dialogs/mediainfo/media_properties_model.hpp \
	dialogs/open/disc_model.cpp dialogs/open/disc_model.hpp \
	dialogs/convert/conversion_manager.cpp \
	dialogs/convert/conversion_manager.hpp \
	dialogs/convert/conversion_profile_model.cpp \
	dialogs/convert/conversion_profile_model.hpp \
	dialogs/settings/profiles_model.cpp \
	dialogs/settings/profiles_model.hpp \
	dialogs/onboarding/app_onboarding_model.cpp \
	dialogs/onboarding/app_onboarding_model.hpp \
	dialogs/onboarding/onboarding_model.cpp \
	dialogs/onboarding/onboarding_model.hpp \
	dialogs/settings/plugins_list_model.cpp \
	dialogs/settings/plugins_list_model.hpp \
	dialogs/settings/settings_basic_rows.cpp \
	dialogs/settings/settings_basic_rows.hpp \
	dialogs/settings/settings_model.cpp \
	dialogs/settings/settings_model.hpp \
	dialogs/settings/settings_pages.cpp \
	dialogs/settings/settings_pages.hpp \
	dialogs/settings/settings_registry.cpp \
	dialogs/settings/settings_registry.hpp \
	dialogs/shortcuts/app_shortcuts_model.cpp \
	dialogs/shortcuts/app_shortcuts_model.hpp \
	dialogs/shortcuts/shortcuts_model.cpp \
	dialogs/shortcuts/shortcuts_model.hpp \
	dialogs/sout/profiles.hpp \
	dialogs/sout/stream_output.cpp \
	dialogs/sout/stream_output.hpp \
	dialogs/subtitles/add_subtitles_model.cpp \
	dialogs/subtitles/add_subtitles_model.hpp \
	dialogs/subtitles/http_request.cpp \
	dialogs/subtitles/http_request.hpp \
	dialogs/subtitles/opensubtitles.cpp \
	dialogs/subtitles/opensubtitles.hpp \
	dialogs/subtitles/opensubtitles_models.cpp \
	dialogs/subtitles/opensubtitles_models.hpp \
	dialogs/subtitles/subtitle_match_model.cpp \
	dialogs/subtitles/subtitle_match_model.hpp \
	dialogs/subtitles/subtitle_matching.cpp \
	dialogs/subtitles/subtitle_matching.hpp \
	dialogs/systray/systray.cpp \
	dialogs/systray/systray.hpp \
	dialogs/toolbar/controlbar_profile.hpp \
	dialogs/toolbar/controlbar_profile.cpp \
	dialogs/toolbar/controlbar_profile_model.cpp \
	dialogs/toolbar/controlbar_profile_model.hpp \
	maininterface/compositor.hpp \
	maininterface/compositor.cpp \
	maininterface/compositor_common.hpp \
	maininterface/compositor_common.cpp \
	maininterface/compositor_accessibility.cpp \
	maininterface/compositor_accessibility.hpp \
	maininterface/compositor_dummy.hpp \
	maininterface/compositor_dummy.cpp \
	maininterface/compositor_platform.hpp \
	maininterface/compositor_platform.cpp \
	maininterface/interface_window_handler.cpp \
	maininterface/interface_window_handler.hpp \
	maininterface/mainctx.cpp \
	maininterface/mainctx.hpp \
	maininterface/mainctx_submodels.cpp maininterface/mainctx_submodels.hpp \
	maininterface/window_chrome.cpp maininterface/window_chrome.hpp \
	maininterface/mainui.cpp \
	maininterface/mainui.hpp \
	maininterface/navigationmodel.cpp maininterface/navigationmodel.hpp \
	maininterface/pinsmodel.cpp maininterface/pinsmodel.hpp \
	maininterface/videosurface.cpp \
	maininterface/videosurface.hpp \
	maininterface/videoblur.cpp \
	maininterface/videoblur.hpp \
	maininterface/video_window_handler.cpp \
	maininterface/video_window_handler.hpp \
	maininterface/windoweffects_module.hpp \
	medialibrary/medialib.cpp \
	medialibrary/medialib.hpp \
	medialibrary/mlmediastore.cpp \
	medialibrary/mlmediastore.hpp \
	medialibrary/mlalbum.cpp \
	medialibrary/mlalbum.hpp \
	medialibrary/mlalbummodel.cpp \
	medialibrary/mlalbummodel.hpp \
	medialibrary/mlaudio.cpp \
	medialibrary/mlaudio.hpp \
	medialibrary/mlaudiomodel.cpp \
	medialibrary/mlaudiomodel.hpp \
	medialibrary/mlartist.cpp \
	medialibrary/mlartist.hpp \
	medialibrary/mlartistmodel.cpp \
	medialibrary/mlartistmodel.hpp \
	medialibrary/mlbasemodel.cpp \
	medialibrary/mlbasemodel.hpp \
	medialibrary/mlbookmarkmodel.cpp \
	medialibrary/mlbookmarkmodel.hpp \
	medialibrary/mlevent.hpp \
	medialibrary/mlfolder.cpp \
	medialibrary/mlfolder.hpp \
	medialibrary/mlfoldersmodel.cpp \
	medialibrary/mlfoldersmodel.hpp \
	medialibrary/mlgenre.cpp \
	medialibrary/mlgenre.hpp \
	medialibrary/mlgenremodel.cpp \
	medialibrary/mlgenremodel.hpp \
	medialibrary/mlhelper.cpp \
	medialibrary/mlhelper.hpp \
	medialibrary/mlthreadpool.cpp \
	medialibrary/mlthreadpool.hpp \
	medialibrary/mlqmltypes.hpp \
	medialibrary/mlmedia.hpp \
	medialibrary/mlplaycountrank.cpp \
	medialibrary/mlplaycountrank.hpp \
	medialibrary/mlqueryparams.cpp \
	medialibrary/mlqueryparams.hpp \
	medialibrary/mlrecentaudiomodel.cpp \
	medialibrary/mlrecentaudiomodel.hpp \
	medialibrary/mlrecenthidelist.cpp \
	medialibrary/mlrecenthidelist.hpp \
	medialibrary/mlrecentmediamodel.cpp \
	medialibrary/mlrecentmediamodel.hpp \
	medialibrary/mlrecentvideomodel.cpp \
	medialibrary/mlrecentvideomodel.hpp \
	medialibrary/mlvideo.cpp \
	medialibrary/mlvideo.hpp \
	medialibrary/mlvideofoldersmodel.cpp \
	medialibrary/mlvideofoldersmodel.hpp \
	medialibrary/mlfoldertreemodel.cpp \
	medialibrary/mlfoldertreemodel.hpp \
	medialibrary/mlmediafolder.cpp \
	medialibrary/mlmediafolder.hpp \
	medialibrary/mlfolderactions.cpp \
	medialibrary/mlfolderactions.hpp \
	medialibrary/mlmusicfoldertreemodel.cpp \
	medialibrary/mlmusicfoldertreemodel.hpp \
	medialibrary/mlmediamodel.cpp \
	medialibrary/mlmediamodel.hpp \
	medialibrary/mlvideomodel.cpp \
	medialibrary/mlvideomodel.hpp \
	medialibrary/mlplaylist.cpp \
	medialibrary/mlplaylist.hpp \
	medialibrary/mlplaylistlistmodel.cpp \
	medialibrary/mlplaylistlistmodel.hpp \
	medialibrary/mlplaylistmanager.cpp \
	medialibrary/mlplaylistmanager.hpp \
	medialibrary/mlplaylistmedia.cpp \
	medialibrary/mlplaylistmedia.hpp \
	medialibrary/mlplaylistmodel.cpp \
	medialibrary/mlplaylistmodel.hpp \
	medialibrary/thumbnailcollector.hpp \
	medialibrary/thumbnailcollector.cpp \
	medialibrary/mlcustomcover.hpp \
	medialibrary/mlcustomcover.cpp \
	menus/custom_menus.cpp \
	menus/custom_menus.hpp \
	menus/qml_menu_wrapper.cpp \
	menus/qml_menu_wrapper.hpp \
	menus/menus.cpp menus/menus.hpp \
	network/addonsmodel.cpp \
	network/addonsmodel.hpp \
	network/mediatreelistener.cpp \
	network/mediatreelistener.hpp \
	network/devicesourceprovider.cpp \
	network/devicesourceprovider.hpp \
	network/networkbasemodel.cpp \
	network/networkbasemodel.hpp \
	network/networkdevicemodel.cpp \
	network/networkdevicemodel.hpp \
	network/networksourcesmodel.cpp \
	network/networksourcesmodel.hpp \
	network/networkmediamodel.cpp \
	network/networkmediamodel.hpp \
	network/standardpathmodel.cpp \
	network/standardpathmodel.hpp \
	network/vlcmediasourcewrapper.hpp \
	network/saved_streams_model.cpp \
	network/saved_streams_model.hpp \
	network/network_playlist_model.cpp \
	network/network_playlist_model.hpp \
	network/preparser_playlist_reader.cpp \
	network/preparser_playlist_reader.hpp \
	network/podcast_feeds.cpp \
	network/podcast_feeds.hpp \
	style/qtthemeprovider.hpp \
	style/accent_colors.hpp \
	style/colorcontext.cpp \
	style/colorcontext.hpp \
	style/systempalette.cpp \
	style/systempalette.hpp \
	style/themetokens.cpp \
	style/themetokens.hpp \
	style/vlcicons.cpp 	style/vlcicons.hpp \
	style/defaultthemeproviders.hpp \
	player/input_models.cpp player/input_models.hpp \
	player/delay_estimator.cpp \
	player/delay_estimator.hpp \
	player/audio_adjustments.cpp player/audio_adjustments.hpp \
	player/equalizer_curve.cpp player/equalizer_curve.hpp \
	player/seek_thumbnailer.cpp player/seek_thumbnailer.hpp \
	player/shortcut_message.cpp player/shortcut_message.hpp \
	player/player_controller.cpp player/player_controller.hpp player/player_controller_p.hpp \
	player/player_controlbar_model.cpp player/player_controlbar_model.hpp \
	player/control_list_model.cpp player/control_list_model.hpp \
	player/control_list_filter.cpp \
	player/control_list_filter.hpp \
	player/waveform.cpp \
	player/waveform.hpp \
	player/waveform_bars.cpp \
	player/waveform_bars.hpp \
	player/subtitle_timeline.cpp \
	player/subtitle_timeline.hpp \
	player/subtitle_timeline_blocks.cpp \
	player/subtitle_timeline_blocks.hpp \
	player/sleep_timer.cpp \
	player/sleep_timer.hpp \
	player/playback_statistics.cpp \
	player/playback_statistics.hpp \
	player/subtitle_appearance.cpp \
	player/subtitle_appearance.hpp \
	player/presets_model.cpp \
	player/presets_model.hpp \
	player/preset_controller.cpp \
	player/preset_controller.hpp \
	player/player_preset_target.cpp \
	player/player_preset_target.hpp \
	player/video_adjustments.cpp \
	player/video_adjustments.hpp \
	player/player_video_outputs.cpp \
	player/player_video_outputs.hpp \
	playlist/media.hpp \
	playlist/playlist_common.hpp \
	playlist/playlist_controller.cpp \
	playlist/playlist_controller.hpp \
	playlist/playlist_controller_p.hpp \
	playlist/playlist_item.cpp \
	playlist/playlist_item.hpp \
	playlist/playlist_model.cpp \
	playlist/playlist_model.hpp \
	playlist/playlist_model_p.hpp \
	playlist/pasted_media.cpp \
	playlist/pasted_media.hpp \
	playlist/queue_store.cpp \
	playlist/queue_store.hpp \
	util/asynctask.hpp \
	util/accessible_compat_attached.cpp util/accessible_compat_attached.hpp \
	util/audio_device_model.cpp  \
	util/audio_device_model.hpp \
	util/workerthreadset.hpp util/workerthreadset.cpp \
	util/base_model.hpp util/base_model_p.hpp util/base_model.cpp \
	util/color_scheme_model.cpp util/color_scheme_model.hpp \
	util/ui_notifier.cpp util/ui_notifier.hpp \
	util/color_svg_image_provider.cpp util/color_svg_image_provider.hpp \
	util/vlcaccess_image_provider.cpp util/vlcaccess_image_provider.hpp \
	util/covergenerator.cpp \
	util/covergenerator.hpp \
	util/imageluminanceextractor.cpp util/imageluminanceextractor.hpp \
	util/csdbuttonmodel.cpp util/csdbuttonmodel.hpp \
	util/csdmenu.cpp util/csdmenu.hpp \
	util/imagehelper.cpp util/imagehelper.hpp \
	util/keyhelper.cpp util/keyhelper.hpp \
	util/listcache.hxx util/listcache.hpp \
	util/listcacheloader.hpp \
	util/locallistcacheloader.hpp \
	util/locallistbasemodel.hpp \
	util/navigation_history.cpp util/navigation_history.hpp \
	util/item_key_event_filter.cpp \
	util/item_key_event_filter.hpp \
	util/flickable_scroll_handler.cpp \
	util/flickable_scroll_handler.hpp \
	util/qt_dirs.cpp util/qt_dirs.hpp \
	util/proxycolumnmodel.hpp \
	util/module_options_model.cpp util/module_options_model.hpp \
	util/registry.cpp util/registry.hpp \
	util/renderer_manager.cpp util/renderer_manager.hpp \
	util/singleton.hpp \
	util/soutchain.cpp util/soutchain.hpp \
	util/validators.cpp util/validators.hpp \
	util/varcommon_p.hpp \
	util/varchoicemodel.cpp  util/varchoicemodel.hpp \
	util/variables.cpp util/variables.hpp \
	util/vlchotkeyconverter.cpp util/vlchotkeyconverter.hpp \
	util/context_menu_key_filter.cpp util/context_menu_key_filter.hpp \
	util/glyph_icon.cpp util/glyph_icon.hpp \
	util/vlctick.cpp \
	util/vlctick.hpp \
	util/shared_input_item.hpp \
	util/fast_gaussian_blur_template.h \
	util/effects_image_provider.cpp \
	util/effects_image_provider.hpp \
	util/qsgtextureview.cpp \
	util/qsgtextureview.hpp \
	util/dismiss_popup_event_filter.cpp \
	util/dismiss_popup_event_filter.hpp \
	util/list_selection_model.cpp \
	util/list_selection_model.hpp \
	util/vlcqtmessagehandler.cpp \
	util/vlcqtmessagehandler.hpp \
	util/colorizedsvgicon.cpp \
	util/colorizedsvgicon.hpp \
	util/textureproviderobserver.cpp \
	util/textureproviderobserver.hpp \
	util/kirigamiwheelhandler.cpp \
	util/kirigamiwheelhandler.hpp \
	util/textureproviderindirection.cpp \
	util/textureproviderindirection.hpp \
	util/sgmanipulator.cpp \
	util/sgmanipulator.hpp \
	util/pointingtooltipattached.hpp \
	widgets/native/animators.cpp \
	widgets/native/animators.hpp \
	widgets/native/customwidgets.cpp widgets/native/customwidgets.hpp \
	widgets/native/csdthemeimage.cpp \
	widgets/native/csdthemeimage.hpp \
	widgets/native/navigation_attached.cpp \
	widgets/native/navigation_attached.hpp \
	widgets/native/mlfolderseditor.cpp \
	widgets/native/mlfolderseditor.hpp \
	widgets/native/qvlcframe.cpp \
	widgets/native/qvlcframe.hpp \
	widgets/native/searchlineedit.cpp widgets/native/searchlineedit.hpp \
	widgets/native/viewblockingrectangle.cpp widgets/native/viewblockingrectangle.hpp \
	widgets/native/doubleclickignoringitem.hpp

# Meta-object compilation

moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose__$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC     " $@;
moc_verbose__0 = $(moc_verbose_0)

.hpp.moc.cpp:
	$(moc_verbose)$(MOC) $(MOC_CPPFLAGS) -o $@ $<

.cpp.moc:
	$(moc_verbose)$(MOC) $(MOC_CPPFLAGS) -o $@ $<

# These MOC files depend on the configure settings:
dialogs/dialogs_provider.moc.cpp: $(top_builddir)/config.h

nodist_libqt_plugin_la_SOURCES = \
	dialogs/capture/capture_preview.moc.cpp \
	dialogs/capture/open_capture_model.moc.cpp \
	dialogs/dialogs/dialogmodel.moc.cpp \
	dialogs/dialogs_provider.moc.cpp \
	dialogs/extensions/extensions.moc.cpp \
	dialogs/extensions/extensions_manager.moc.cpp \
	dialogs/fingerprint/chromaprint.moc.cpp \
	dialogs/help/aboutmodel.moc.cpp \
	dialogs/epg/programguidemodel.moc.cpp \
	dialogs/messages/messagesmodel.moc.cpp \
	dialogs/mediainfo/media_properties_model.moc.cpp \
	dialogs/open/disc_model.moc.cpp \
	dialogs/convert/conversion_manager.moc.cpp \
	dialogs/convert/conversion_profile_model.moc.cpp \
	dialogs/settings/profiles_model.moc.cpp \
	dialogs/onboarding/app_onboarding_model.moc.cpp \
	dialogs/onboarding/onboarding_model.moc.cpp \
	dialogs/settings/plugins_list_model.moc.cpp \
	dialogs/settings/settings_model.moc.cpp \
	dialogs/settings/settings_pages.moc.cpp \
	dialogs/shortcuts/app_shortcuts_model.moc.cpp \
	dialogs/shortcuts/shortcuts_model.moc.cpp \
	dialogs/sout/stream_output.moc.cpp \
	dialogs/subtitles/add_subtitles_model.moc.cpp \
	dialogs/subtitles/opensubtitles.moc.cpp \
	dialogs/subtitles/opensubtitles_models.moc.cpp \
	dialogs/subtitles/subtitle_match_model.moc.cpp \
	dialogs/subtitles/subtitle_matching.moc.cpp \
	dialogs/systray/systray.moc.cpp \
	dialogs/toolbar/controlbar_profile.moc.cpp \
	dialogs/toolbar/controlbar_profile_model.moc.cpp \
	maininterface/compositor.moc.cpp \
	maininterface/compositor_common.moc.cpp \
	maininterface/compositor_dummy.moc.cpp \
	maininterface/compositor_platform.moc.cpp \
	maininterface/interface_window_handler.moc.cpp \
	maininterface/mainctx.moc.cpp \
	maininterface/mainctx_submodels.moc.cpp \
	maininterface/mainui.moc.cpp \
	maininterface/navigationmodel.moc.cpp \
	maininterface/pinsmodel.moc.cpp \
	maininterface/videosurface.moc.cpp \
	maininterface/videoblur.moc.cpp \
	maininterface/video_window_handler.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlmediastore.moc.cpp \
	medialibrary/mlalbummodel.moc.cpp \
	medialibrary/mlaudiomodel.moc.cpp \
	medialibrary/mlartistmodel.moc.cpp \
	medialibrary/mlbookmarkmodel.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlfoldersmodel.moc.cpp \
	medialibrary/mlgenremodel.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp \
	medialibrary/mlrecentaudiomodel.moc.cpp \
	medialibrary/mlrecenthidelist.moc.cpp \
	medialibrary/mlrecentmediamodel.moc.cpp \
	medialibrary/mlrecentvideomodel.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/mlmediamodel.moc.cpp \
	medialibrary/mlvideomodel.moc.cpp \
	medialibrary/mlplaylistlistmodel.moc.cpp \
	medialibrary/mlplaylistmanager.moc.cpp \
	medialibrary/mlplaylistmodel.moc.cpp \
	medialibrary/mlvideofoldersmodel.moc.cpp \
	medialibrary/mlfoldertreemodel.moc.cpp \
	medialibrary/mlmediafolder.moc.cpp \
	medialibrary/mlfolderactions.moc.cpp \
	medialibrary/mlmusicfoldertreemodel.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	menus/custom_menus.moc.cpp \
	menus/qml_menu_wrapper.moc.cpp \
	menus/menus.moc.cpp \
	network/addonsmodel.moc.cpp \
	network/devicesourceprovider.moc.cpp \
	network/networkdevicemodel.moc.cpp \
	network/networkbasemodel.moc.cpp \
	network/networksourcesmodel.moc.cpp \
	network/networkmediamodel.moc.cpp \
	network/standardpathmodel.moc.cpp \
	network/saved_streams_model.moc.cpp \
	network/network_playlist_model.moc.cpp \
	network/preparser_playlist_reader.moc.cpp \
	network/podcast_feeds.moc.cpp \
	style/colorcontext.moc.cpp \
	style/systempalette.moc.cpp \
	style/vlcicons.moc.cpp \
	player/delay_estimator.moc.cpp \
	player/audio_adjustments.moc.cpp \
	player/equalizer_curve.moc.cpp \
	player/input_models.moc.cpp \
	player/seek_thumbnailer.moc.cpp \
	player/shortcut_message.moc.cpp \
	player/player_controller.moc.cpp \
	player/player_controlbar_model.moc.cpp \
	player/control_list_model.moc.cpp \
	player/control_list_filter.moc.cpp \
	player/waveform.moc.cpp \
	player/waveform_bars.moc.cpp \
	player/subtitle_timeline.moc.cpp \
	player/subtitle_timeline_blocks.moc.cpp \
	player/sleep_timer.moc.cpp \
	player/playback_statistics.moc.cpp \
	player/subtitle_appearance.moc.cpp \
	player/presets_model.moc.cpp \
	player/preset_controller.moc.cpp \
	player/player_preset_target.moc.cpp \
	player/video_adjustments.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp \
	playlist/playlist_model.moc.cpp \
	playlist/queue_store.moc.cpp \
	util/asynctask.moc.cpp \
	util/accessible_compat_attached.moc.cpp \
	util/audio_device_model.moc.cpp \
	util/workerthreadset.moc.cpp \
	util/base_model.moc.cpp \
	util/color_scheme_model.moc.cpp \
	util/ui_notifier.moc.cpp \
	util/color_svg_image_provider.moc.cpp \
	util/vlcaccess_image_provider.moc.cpp \
	util/imageluminanceextractor.moc.cpp \
	util/csdbuttonmodel.moc.cpp \
	util/csdmenu.moc.cpp \
	util/keyhelper.moc.cpp \
	util/listcache.moc.cpp \
	util/locallistcacheloader.moc.cpp \
	util/navigation_history.moc.cpp \
	util/item_key_event_filter.moc.cpp \
	util/effects_image_provider.moc.cpp \
	util/flickable_scroll_handler.moc.cpp \
	util/module_options_model.moc.cpp \
	util/renderer_manager.moc.cpp \
	util/validators.moc.cpp \
	util/varchoicemodel.moc.cpp \
	util/variables.moc.cpp \
	util/vlctick.moc.cpp \
	util/dismiss_popup_event_filter.moc.cpp \
	util/list_selection_model.moc.cpp \
	util/vlchotkeyconverter.moc.cpp \
	util/context_menu_key_filter.moc.cpp \
	util/qsgtextureview.moc.cpp \
	util/textureproviderobserver.moc.cpp \
	util/kirigamiwheelhandler.moc.cpp \
	util/textureproviderindirection.moc.cpp \
	util/sgmanipulator.moc.cpp \
	util/pointingtooltipattached.moc.cpp \
	widgets/native/animators.moc.cpp \
	widgets/native/csdthemeimage.moc.cpp \
	widgets/native/customwidgets.moc.cpp \
	widgets/native/navigation_attached.moc.cpp \
	widgets/native/mlfolderseditor.moc.cpp \
	widgets/native/searchlineedit.moc.cpp \
	widgets/native/viewblockingrectangle.moc.cpp


# windows compositor

if HAVE_WIN32

libqt_plugin_la_SOURCES += \
	maininterface/mainctx_win32.cpp \
	maininterface/mainctx_win32.hpp \
	maininterface/compositor_win7.cpp \
	maininterface/compositor_win7.hpp \
	style/windowsthemeprovider.cpp \
	maininterface/win32windoweffects_module.cpp \
	maininterface/win32windoweffects_module.hpp \
	util/csdmenu_win32.c \
	util/csdmenu_win32.h

nodist_libqt_plugin_la_SOURCES += maininterface/mainctx_win32.moc.cpp \
	maininterface/compositor_win7.moc.cpp

libqt_plugin_la_SOURCES += \
	maininterface/compositor_dcomp.cpp \
	maininterface/compositor_dcomp.hpp \
	maininterface/compositor_dcomp_error.hpp \
	maininterface/compositor_dcomp_acrylicsurface.hpp \
	maininterface/compositor_dcomp_acrylicsurface.cpp \
	maininterface/compositor_dcomp_videoblur.hpp \
	maininterface/compositor_dcomp_videoblur.cpp

nodist_libqt_plugin_la_SOURCES += \
	maininterface/compositor_dcomp_acrylicsurface.moc.cpp \
	maininterface/compositor_dcomp_videoblur.moc.cpp \
	maininterface/compositor_dcomp.moc.cpp

endif

# X11 compositor

if HAVE_X
if HAVE_QT_XCB
if HAVE_XCB
if HAVE_XCB_DAMAGE
if HAVE_XCB_XFIXES

libqt_plugin_la_CXXFLAGS += -DQT_HAS_X11_COMPOSITOR \
	$(QT_X11_CFLAGS) $(XCB_RENDER_CFLAGS) $(XCB_COMPOSITE_CFLAGS) $(XCB_DAMAGE_CFLAGS) $(XCB_XFIXES_CFLAGS)

libqt_plugin_la_LIBADD += $(QT_X11_LIBS) $(XCB_RENDER_LIBS) $(XCB_COMPOSITE_LIBS) $(XCB_DAMAGE_LIBS) $(XCB_XFIXES_LIBS)

libqt_plugin_la_SOURCES += \
	maininterface/compositor_x11.cpp \
	maininterface/compositor_x11.hpp \
	maininterface/compositor_x11_renderclient.cpp \
	maininterface/compositor_x11_renderclient.hpp \
	maininterface/compositor_x11_renderwindow.cpp \
	maininterface/compositor_x11_renderwindow.hpp \
	maininterface/compositor_x11_uisurface.cpp \
	maininterface/compositor_x11_uisurface.hpp \
	maininterface/compositor_x11_utils.cpp \
	maininterface/compositor_x11_utils.hpp

nodist_libqt_plugin_la_SOURCES += \
	maininterface/compositor_x11.moc.cpp \
	maininterface/compositor_x11_renderclient.moc.cpp \
	maininterface/compositor_x11_renderwindow.moc.cpp \
	maininterface/compositor_x11_uisurface.moc.cpp

#extra X11 features

libqt_x11_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(XCB_CFLAGS) $(XCB_RENDER_CFLAGS) $(XCB_COMPOSITE_CFLAGS) $(XCB_DAMAGE_CFLAGS) $(XCB_XFIXES_CFLAGS)
libqt_x11_plugin_la_LIBADD = $(XCB_LIBS) $(XCB_RENDER_LIBS) $(XCB_COMPOSITE_LIBS) $(XCB_DAMAGE_LIBS) $(XCB_XFIXES_LIBS)
libqt_x11_plugin_la_SOURCES = \
	maininterface/qt_x11_plugin.c \
	maininterface/compositor_x11_utils.hpp \
	maininterface/compositor_x11_utils.cpp \
	util/csdmenu_module.h \
	util/csdmenu_x11.cpp \
	util/csdmenu_x11.h

if ENABLE_QT
gui_PLUGINS += libqt_x11_plugin.la
endif

endif
endif
endif
endif
endif

# wayland compositor

if HAVE_WAYLAND
if HAVE_QT_GUI_PRIVATE

libqt_plugin_la_CPPFLAGS += -DQT_HAS_WAYLAND_COMPOSITOR

libqt_plugin_la_SOURCES += \
	maininterface/compositor_wayland.cpp \
	maininterface/compositor_wayland.hpp


nodist_libqt_plugin_la_SOURCES += \
	maininterface/compositor_wayland.moc.cpp

libqt_wayland_plugin_la_CFLAGS = $(AM_CFLAGS) $(WAYLAND_CLIENT_CFLAGS)
libqt_wayland_plugin_la_LIBADD =  $(WAYLAND_CLIENT_LIBS) $(LIBM)
libqt_wayland_plugin_la_SOURCES = \
	maininterface/qt_wayland_plugin.c \
	maininterface/compositor_wayland_module.c \
	maininterface/compositor_wayland_module.h

#if WAYLAND_PROTOCOLS
#if WAYLAND_SCANNER
libqt_wayland_plugin_la_CFLAGS += -DQT_HAS_WAYLAND_PROTOCOLS -I$(builddir)/maininterface

libqt_wayland_plugin_la_SOURCES += \
	util/csdmenu_wayland.c \
	util/csdmenu_wayland.h \
	util/csdmenu_module.h

nodist_libqt_wayland_plugin_la_SOURCES = \
	maininterface/xdg-shell-client-protocol.h \
	maininterface/xdg-shell-protocol.c

if HAVE_QT65
libqt_wayland_plugin_la_CFLAGS += -DQT_HAS_WAYLAND_FRACTIONAL_SCALING
nodist_libqt_wayland_plugin_la_SOURCES += \
	maininterface/viewporter-client-protocol.h \
	maininterface/viewporter-protocol.c

maininterface/viewporter-client-protocol.h: \
		$(WAYLAND_PROTOCOLS)/stable/viewporter/viewporter.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header $< $@

maininterface/viewporter-protocol.c: \
		$(WAYLAND_PROTOCOLS)/stable/viewporter/viewporter.xml \
		maininterface/viewporter-client-protocol.h
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code $< $@
endif

BUILT_SOURCES += $(nodist_libqt_wayland_plugin_la_SOURCES)

maininterface/xdg-shell-client-protocol.h: $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header $< $@

maininterface/xdg-shell-protocol.c: $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code $< $@

#endif
#endif

if ENABLE_QT
gui_PLUGINS += libqt_wayland_plugin.la
endif

endif
endif

if !HAVE_WIN32
if HAVE_KF6WINDOWSYSTEM

libqt_plugin_la_CPPFLAGS += -DQT_HAS_KWINDOWSYSTEM_PLUGIN

libqt_kwindowsystem_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(KF6WINDOWSYSTEM_CFLAGS)
libqt_kwindowsystem_plugin_la_LIBADD =  $(KF6WINDOWSYSTEM_LIBS) $(QT_LIBS)
libqt_kwindowsystem_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(QT_LDFLAGS)
libqt_kwindowsystem_plugin_la_SOURCES = \
	maininterface/kwindowsystem_module.cpp

if ENABLE_QT
gui_PLUGINS += libqt_kwindowsystem_plugin.la
endif
endif
endif

# User interface compilation

uic_verbose = $(uic_verbose_$(V))
uic_verbose_ = $(uic_verbose__$(AM_DEFAULT_VERBOSITY))
uic_verbose_0 = @echo "  UIC     " $@;
uic_verbose__0 = $(uic_verbose_0)

MOC_CPPFLAGS = $(DEFS) \
	-I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_builddir) \
	$(QT_MOC_INCLUDE_DIRS) \
	$(QT_CFLAGS) \
	$(libqt_plugin_la_CPPFLAGS)
if HAVE_DARWIN
MOC_CPPFLAGS += -DQ_OS_MAC
endif
if HAVE_WIN32
MOC_CPPFLAGS += -D_WIN32
endif

libqt_plugin_la_UI =
EXTRA_DIST += $(libqt_plugin_la_UI)

ui_%.h: %.ui
	$(AM_V_at)$(MKDIR_P) "ui"
	$(AM_V_at)rm -f $@ $@.tmp
	$(AM_V_at)echo "#define Q_(a,b) QString::fromUtf8(_(a))" > $@.tmp
	$(uic_verbose)$(UIC) -tr "Q_" $< >> $@.tmp
	$(AM_V_at)sed -e 's/Q_(\"_(\\\"\(.*\)\\\")"/Q_("\1"/' $@.tmp >$@
	$(AM_V_at)rm -f $@.tmp

# Resources

libqt_plugin_la_RES = \
	pixmaps/VLCIcons.ttf \
	fonts/Inter-Regular.ttf \
	fonts/Inter-Medium.ttf \
	fonts/Inter-SemiBold.ttf \
	fonts/Inter-Bold.ttf \
	fonts/DMMono-Regular.ttf \
	pixmaps/addons/addon_default.svg \
	pixmaps/addons/addon_broken.svg \
	pixmaps/addons/addon_blue.svg \
	pixmaps/addons/addon_cyan.svg \
	pixmaps/addons/addon_green.svg \
	pixmaps/addons/addon_magenta.svg \
	pixmaps/addons/addon_red.svg \
	pixmaps/addons/addon_yellow.svg \
	pixmaps/addons/addon_score.svg \
	pixmaps/icons/active_indicator.svg \
	pixmaps/icons/add.svg \
	pixmaps/icons/ic_album_cover.svg \
	pixmaps/icons/ic_aspect_ratio.svg \
	pixmaps/icons/atob.svg \
	pixmaps/icons/ic_audiosub.svg \
	pixmaps/icons/back.svg \
	pixmaps/icons/ic_fluent_bookmark.svg \
	pixmaps/icons/breadcrumb_prev.svg \
	pixmaps/icons/breadcrumb_sep.svg \
	pixmaps/icons/check.svg \
	pixmaps/icons/clear.svg \
	pixmaps/icons/close.svg \
	pixmaps/icons/ic_fluent_full_screen_minimize.svg \
	pixmaps/icons/del.svg \
	pixmaps/icons/download.svg \
	pixmaps/icons/dropzone.svg \
	pixmaps/icons/dvd_menu.svg \
	pixmaps/icons/dvd_next.svg \
	pixmaps/icons/dvd_prev.svg \
	pixmaps/icons/ic_fluent_arrow_eject.svg \
	pixmaps/icons/ellipsis.svg \
	pixmaps/icons/ic_playlist_add.svg \
	pixmaps/icons/expand_inverted.svg \
	pixmaps/icons/expand.svg \
	pixmaps/icons/ic_fluent_options.svg \
	pixmaps/icons/ic_fluent_fast_forward.svg \
	pixmaps/icons/ic_fluent_next_frame.svg \
	pixmaps/icons/ic_fluent_previous_frame.svg \
	pixmaps/icons/ic_fluent_full_screen_maximize.svg \
	pixmaps/icons/ic_fluent_grid.svg \
	pixmaps/icons/history.svg \
	pixmaps/icons/ic_fluent_home.svg \
	pixmaps/icons/ic_fluent_info.svg \
	pixmaps/icons/ic_fluent_apps_list.svg \
	pixmaps/icons/ic_fluent_more_vertical.svg \
	pixmaps/icons/multiselect.svg \
	pixmaps/icons/ok.svg \
	pixmaps/icons/ic_fluent_next.svg \
	pixmaps/icons/ic_pause_filled.svg \
	pixmaps/icons/pip.svg \
	pixmaps/icons/ic_playlist_clear.svg \
	pixmaps/icons/ic_playlist.svg \
	pixmaps/icons/ic_fluent_play.svg \
	pixmaps/icons/ic_fluent_play_reverse_filled.svg \
	pixmaps/icons/ic_fluent_arrow_move.svg \
	pixmaps/icons/ic_fluent_chevron_down_24.svg \
	pixmaps/icons/ic_fluent_chevron_left_24.svg \
	pixmaps/icons/ic_fluent_chevron_right_24.svg \
	pixmaps/icons/ic_fluent_chevron_up_24.svg \
	pixmaps/icons/ic_fluent_play_filled.svg \
	pixmaps/icons/podcast.svg \
	pixmaps/icons/ic_fluent_previous.svg \
	pixmaps/icons/profile_new.svg \
	pixmaps/icons/ic_fluent_record.svg \
	pixmaps/icons/remove.svg \
	pixmaps/icons/ic_fluent_cast.svg \
	pixmaps/icons/ic_fluent_arrow_repeat_all.svg \
	pixmaps/icons/ic_fluent_arrow_repeat_1.svg \
	pixmaps/icons/ic_fluent_rewind.svg \
	pixmaps/icons/ic_fluent_search.svg \
	pixmaps/icons/ic_fluent_arrow_shuffle.svg \
	pixmaps/icons/ic_fluent_skip_back_10.svg \
	pixmaps/icons/ic_fluent_skip_forward_10.svg \
	pixmaps/icons/ic_fluent_camera.svg \
	pixmaps/icons/ic_fluent_spacebar.svg \
	pixmaps/icons/ic_fluent_stop.svg \
	pixmaps/icons/stream.svg \
	pixmaps/icons/sync.svg \
	pixmaps/icons/ic_fluent_clock.svg \
	pixmaps/icons/ic_fluent_globe.svg \
	pixmaps/icons/topbar_filter.svg \
	pixmaps/icons/ic_fluent_music_note_2.svg \
	pixmaps/icons/ic_fluent_wifi_1.svg \
	pixmaps/icons/ic_fluent_arrow_sort.svg \
	pixmaps/icons/ic_fluent_filmstrip.svg \
	pixmaps/icons/ic_fluent_chevron_up.svg \
	pixmaps/icons/ic_fluent_chevron_down.svg \
	pixmaps/icons/ic_fluent_tv.svg \
	pixmaps/icons/ic_tvtelx.svg \
	pixmaps/icons/visualization.svg \
	pixmaps/icons/volume_high.svg \
	pixmaps/icons/volume_low.svg \
	pixmaps/icons/volume_medium.svg \
	pixmaps/icons/volume_muted.svg \
	pixmaps/icons/volume_zero.svg \
	pixmaps/icons/window_close.svg \
	pixmaps/icons/window_maximize.svg \
	pixmaps/icons/window_minimize.svg \
	pixmaps/icons/window_restore.svg \
	pixmaps/icons/ic_fluent_arrow_download_24_regular.svg \
	pixmaps/icons/ic_fluent_arrow_sync_24_regular.svg \
	pixmaps/icons/regex.svg \
	pixmaps/icons/ic_fluent_text_change_case_24_regular.svg \
	pixmaps/menu/add.svg \
	pixmaps/menu/capture-card.svg \
	pixmaps/menu/clear.svg \
	pixmaps/menu/disc.svg \
	pixmaps/menu/download.svg \
	pixmaps/menu/ic_fluent_arrow_eject.svg \
	pixmaps/menu/exit.svg \
	pixmaps/menu/ic_fluent_fast_forward.svg \
	pixmaps/menu/file.svg \
	pixmaps/menu/folder.svg \
	pixmaps/menu/help.svg \
	pixmaps/menu/info.svg \
	pixmaps/menu/media_library.svg \
	pixmaps/menu/messages.svg \
	pixmaps/menu/movie.svg \
	pixmaps/menu/music.svg \
	pixmaps/menu/network.svg \
	pixmaps/menu/ic_playlist.svg \
	pixmaps/menu/ic_fluent_next.svg \
	pixmaps/menu/ic_pause_filled.svg \
	pixmaps/menu/ic_fluent_play_filled.svg \
	pixmaps/menu/preferences.svg \
	pixmaps/menu/ic_fluent_previous.svg \
	pixmaps/menu/profile_new.svg \
	pixmaps/menu/ic_fluent_record.svg \
	pixmaps/menu/remove.svg \
	pixmaps/menu/ic_fluent_arrow_repeat_all.svg \
	pixmaps/menu/repeat_off.svg \
	pixmaps/menu/ic_fluent_arrow_repeat_1.svg \
	pixmaps/menu/ic_fluent_rewind.svg \
	pixmaps/menu/search_clear.svg \
	pixmaps/menu/ic_fluent_options.svg \
	pixmaps/menu/ic_fluent_skip_back_10.svg \
	pixmaps/menu/ic_fluent_skip_forward_10.svg \
	pixmaps/menu/ic_fluent_arrow_shuffle.svg \
	pixmaps/menu/ic_fluent_stop.svg \
	pixmaps/menu/stream.svg \
	pixmaps/menu/sync.svg \
	pixmaps/menu/update.svg \
	pixmaps/menu/volume-high.svg \
	pixmaps/menu/volume-low.svg \
	pixmaps/menu/volume-muted.svg \
	pixmaps/misc/cone.svg \
	pixmaps/misc/new_indicator.svg \
	pixmaps/misc/theme_dark.svg \
	pixmaps/misc/theme_daynight.svg \
	pixmaps/misc/theme_light.svg \
	pixmaps/misc/volume_cone_level.svg \
	pixmaps/misc/volume_cone_track.svg \
	pixmaps/misc/wait1.svg \
	pixmaps/misc/wait2.svg \
	pixmaps/misc/wait3.svg \
	pixmaps/misc/wait4.svg \
	pixmaps/placeholder/add_service.svg \
	pixmaps/placeholder/noart.png \
	pixmaps/placeholder/noart_album.svg \
	pixmaps/placeholder/noart_albumCover.svg \
	pixmaps/placeholder/noart_artist.svg \
	pixmaps/placeholder/noart_artistCover.svg \
	pixmaps/placeholder/noart_artist_small.svg \
	pixmaps/placeholder/noart_videoCover.svg \
	pixmaps/prefs/advprefs_audio.svg \
	pixmaps/prefs/advprefs_codec.svg \
	pixmaps/prefs/advprefs_extended.svg \
	pixmaps/prefs/advprefs_intf.svg \
	pixmaps/prefs/advprefs_playlist.svg \
	pixmaps/prefs/advprefs_sout.svg \
	pixmaps/prefs/advprefs_video.svg \
	pixmaps/prefs/sample_complete.png \
	pixmaps/prefs/sample_minimal.png \
	pixmaps/prefs/sample_skins.png \
	pixmaps/prefs/spref_audio.png \
	pixmaps/prefs/spref_hotkeys.png \
	pixmaps/prefs/spref_input.png \
	pixmaps/prefs/spref_interface.png \
	pixmaps/prefs/spref_medialibrary.png \
	pixmaps/prefs/spref_subtitles.png \
	pixmaps/prefs/spref_video.png \
	pixmaps/sd/assembleenationale.svg \
	pixmaps/sd/capture-card.svg \
	pixmaps/sd/directory.svg \
	pixmaps/sd/disc.svg \
	pixmaps/sd/file.svg \
	pixmaps/sd/fmc.svg \
	pixmaps/sd/frenchtv.svg \
	pixmaps/sd/harddisk.svg \
	pixmaps/sd/icecast.svg \
	pixmaps/sd/jamendo.svg \
	pixmaps/sd/katsomo.svg \
	pixmaps/sd/lan.svg \
	pixmaps/sd/library.svg \
	pixmaps/sd/metachannels.svg \
	pixmaps/sd/mtp.svg \
	pixmaps/sd/network.svg \
	pixmaps/sd/node.svg \
	pixmaps/sd/pictures.svg \
	pixmaps/sd/playlist.svg \
	pixmaps/sd/podcast.svg \
	pixmaps/sd/radio.svg \
	pixmaps/sd/screen.svg \
	pixmaps/sd/stream.svg \
	pixmaps/sd/unknown.svg

if HAVE_WIN32
libqt_plugin_la_RES += \
	pixmaps/win7/pause.svg \
	pixmaps/win7/prev.svg \
	pixmaps/win7/next.svg \
	pixmaps/win7/play.svg
endif

# QML modules
#
# Each QML module has a small section below. To add a new module "foo", append
# a section following this template:
#
#   libqml_module_foo_a_Module     = VLC.Foo
#   libqml_module_foo_a_ModulePath = VLC/Foo
#   libqml_module_foo_a_CXXFLAGS   = $(libqt_plugin_la_CXXFLAGS)
#   libqml_module_foo_a_CPPFLAGS   = $(libqt_plugin_la_CPPFLAGS)
#   libqml_module_foo_a_SOURCES    = $(QML_MODULE_foo_SRC)
#   libqml_module_foo_a_QML 			 = foo/qml/Bar.qml # ...
#   libqml_module_foo_a_Singleton_QML  = foo/qml/SingletonBar.qml # only when there are Singletons
#   nodist_libqml_module_foo_a_SOURCES = foo/qmlassets.cpp $(QML_MODULE_foo_NODIST_EXTRA)
#   QML_MODULE_LIBS += libqml_module_foo.a
#
# When exposing singletons, list those QML files in the proper variable so that
# they are declared as singletons in the generated qmldir.
#
# Then:
#   1. Add "foo" to libqt_plugin_la_QML_MODULES.
#   2. Add the QML file lists to libqt_plugin_la_ALL_QML.
#   3. Add a QML_MODULE_foo_SRC / QML_MODULE_foo_NODIST_EXTRA pair to the
#      "if HAVE_QMLCACHEGEN" block below.
#
noinst_LIBRARIES =
QML_MODULE_LIBS =

libqml_module_dialogs_a_Module = VLC.Dialogs
libqml_module_dialogs_a_ModulePath = VLC/Dialogs
libqml_module_dialogs_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_dialogs_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_dialogs_a_QML = \
	dialogs/capture/qml/ModuleOptionsView.qml \
	dialogs/capture/qml/OpenCaptureDialog.qml \
	dialogs/convert/qml/ConversionProfileEditor.qml \
	dialogs/convert/qml/ConversionsIndicator.qml \
	dialogs/convert/qml/ConvertDialog.qml \
	dialogs/dialogs/qml/AboutWindow.qml \
	dialogs/dialogs/qml/CustomDialog.qml \
	dialogs/dialogs/qml/DialogWindows.qml \
	dialogs/dialogs/qml/Dialogs.qml \
	dialogs/dialogs/qml/ErrorNotice.qml \
	dialogs/dialogs/qml/MessagesWindow.qml \
	dialogs/dialogs/qml/ModalDialog.qml \
	dialogs/dialogs/qml/ProfileRestartNotice.qml \
	dialogs/dialogs/qml/ProgramGuideWindow.qml \
	dialogs/dialogs/qml/TextInputDialog.qml \
	dialogs/dialogs/qml/WindowDialog.qml \
	dialogs/settings/qml/SettingsDialog.qml \
	dialogs/settings/qml/SettingsPanel.qml \
	dialogs/settings/qml/SettingsRow.qml \
	dialogs/settings/qml/SettingsSegmented.qml \
	dialogs/settings/qml/SettingsSlider.qml \
	dialogs/settings/qml/SettingsToggle.qml \
	dialogs/shortcuts/qml/KeyboardShortcutsDialog.qml \
	dialogs/shortcuts/qml/KeyboardShortcutsLoader.qml \
	dialogs/shortcuts/qml/ShortcutKeyField.qml \
	dialogs/onboarding/qml/OnboardingWindow.qml \
	dialogs/mediainfo/qml/MediaPropertiesDialog.qml \
	dialogs/open/qml/OpenDiscDialog.qml \
	dialogs/toolbar/qml/EditorDashedBorder.qml \
	dialogs/open/qml/OpenNetworkDialog.qml \
	dialogs/sout/qml/StreamCastDialog.qml \
	dialogs/subtitles/qml/AddSubtitlesDialog.qml \
	dialogs/toolbar/qml/EditorDNDDelegate.qml \
	dialogs/toolbar/qml/EditorDNDView.qml\
	dialogs/toolbar/qml/EditorDummyButton.qml \
	dialogs/toolbar/qml/ToolbarEditor.qml \
	dialogs/toolbar/qml/ToolbarEditorDialog.qml \
	dialogs/toolbar/qml/ToolbarEditorButtonList.qml
libqml_module_dialogs_a_Singleton_QML =
libqml_module_dialogs_a_SOURCES = $(QML_MODULE_dialogs_SRC)
nodist_libqml_module_dialogs_a_SOURCES = dialogs/qmlassets.cpp $(QML_MODULE_dialogs_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_dialogs.a

libqml_module_maininterface_a_Module = VLC.MainInterface
libqml_module_maininterface_a_ModulePath = VLC/MainInterface
libqml_module_maininterface_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_maininterface_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_maininterface_a_QML = \
	maininterface/qml/LocalTopbar.qml \
	maininterface/qml/MainInterface.qml \
	maininterface/qml/MainViewLoader.qml \
	maininterface/qml/MainDisplay.qml \
	maininterface/qml/SideNavigationPane.qml \
	maininterface/qml/SideNavigationDelegate.qml \
	maininterface/qml/LibraryPane.qml \
	maininterface/qml/NoMedialibHome.qml


libqml_module_maininterface_a_Singleton_QML = \
	maininterface/qml/LibraryPages.qml
libqml_module_maininterface_a_SOURCES = $(QML_MODULE_maininterface_SRC)
nodist_libqml_module_maininterface_a_SOURCES = maininterface/qmlassets.cpp $(QML_MODULE_maininterface_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_maininterface.a

libqml_module_medialibrary_a_Module = VLC.MediaLibrary
libqml_module_medialibrary_a_ModulePath = VLC/MediaLibrary
libqml_module_medialibrary_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_medialibrary_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_medialibrary_a_QML = \
	medialibrary/qml/ArtistTopBanner.qml \
	medialibrary/qml/AudioGridItem.qml \
	medialibrary/qml/FrequentFoldersView.qml \
	medialibrary/qml/HomeDisplay.qml \
	medialibrary/qml/HomePage.qml \
	medialibrary/qml/MediaView.qml \
	medialibrary/qml/MusicAlbums.qml \
	medialibrary/qml/MusicAlbumsDisplay.qml \
	medialibrary/qml/MusicAlbumsGridExpandDelegate.qml \
	medialibrary/qml/MusicArtist.qml \
	medialibrary/qml/MusicArtistDelegate.qml \
	medialibrary/qml/MusicArtistsAlbums.qml \
	medialibrary/qml/MusicAllArtists.qml \
	medialibrary/qml/MusicArtistsDisplay.qml \
	medialibrary/qml/MusicDisplay.qml \
	medialibrary/qml/MusicGenres.qml \
	medialibrary/qml/MusicGenresDisplay.qml \
	medialibrary/qml/MusicSongsDisplay.qml \
	medialibrary/qml/MusicTrackListDisplay.qml \
	medialibrary/qml/VideoDisplay.qml \
	medialibrary/qml/VideoFoldersDisplay.qml \
	medialibrary/qml/VideoAll.qml \
	medialibrary/qml/PlaylistMediaList.qml \
	medialibrary/qml/NewPlaylistDialog.qml \
	medialibrary/qml/PlaylistPicker.qml \
	medialibrary/qml/PlaylistDialogs.qml \
	medialibrary/qml/AddPinDialog.qml \
	medialibrary/qml/FolderDialogs.qml \
	medialibrary/qml/FolderPageMenu.qml \
	medialibrary/qml/PlaylistMedia.qml \
	medialibrary/qml/PlaylistMediaDisplay.qml \
	medialibrary/qml/PlaylistsDisplay.qml \
	medialibrary/qml/RecentDisplay.qml \
	medialibrary/qml/VideoGridItem.qml \
	medialibrary/qml/VideoInfoExpandPanel.qml \
	medialibrary/qml/VideoListDisplay.qml \
	medialibrary/qml/VideoGridDisplay.qml \
	medialibrary/qml/MusicAlbumSectionDelegate.qml \
	medialibrary/qml/MLGlobalShortcuts.qml \
	medialibrary/qml/Bookmarks.qml \
	medialibrary/qml/BookmarksPane.qml \
	medialibrary/qml/MLContextMenu.qml \
	medialibrary/qml/MLDragItem.qml \
	medialibrary/qml/MLTableColumns.qml \
	medialibrary/qml/ScanProgressBar.qml
libqml_module_medialibrary_a_Singleton_QML =
libqml_module_medialibrary_a_SOURCES = $(QML_MODULE_medialibrary_SRC)
nodist_libqml_module_medialibrary_a_SOURCES = medialibrary/qmlassets.cpp $(QML_MODULE_medialibrary_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_medialibrary.a

libqml_module_menus_a_Module = VLC.Menus
libqml_module_menus_a_ModulePath = VLC/Menus
libqml_module_menus_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_menus_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_menus_a_QML = \
	menus/qml/GlobalShortcuts.qml \
	menus/qml/ShortcutExt.qml
libqml_module_menus_a_Singleton_QML =
libqml_module_menus_a_SOURCES = $(QML_MODULE_menus_SRC)
nodist_libqml_module_menus_a_SOURCES = menus/qmlassets.cpp $(QML_MODULE_menus_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_menus.a

libqml_module_network_a_Module = VLC.Network
libqml_module_network_a_ModulePath = VLC/Network
libqml_module_network_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_network_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_network_a_QML = \
	network/qml/AddressbarButton.qml \
	network/qml/BrowseTreeDisplay.qml \
	network/qml/BrowseTreeHeader.qml \
	network/qml/NetworkAddressbar.qml \
	network/qml/NetworkDisplay.qml \
	network/qml/NetworkGridItem.qml \
	network/qml/NetworkHomeDisplay.qml \
	network/qml/NetworkPlaylistDisplay.qml \
	network/qml/NetworkRow.qml \
	network/qml/NetworkThumbnailItem.qml
libqml_module_network_a_Singleton_QML =
libqml_module_network_a_SOURCES = $(QML_MODULE_network_SRC)
nodist_libqml_module_network_a_SOURCES = network/qmlassets.cpp $(QML_MODULE_network_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_network.a

libqml_module_player_a_Module = VLC.Player
libqml_module_player_a_ModulePath = VLC/Player
libqml_module_player_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_player_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_player_a_QML = \
	player/qml/ControlBar.qml \
	player/qml/JumpToTimePopup.qml \
	player/qml/PlaybackSpeed.qml \
	player/qml/LyricsFlickable.qml \
	player/qml/MiniPlayer.qml \
	player/qml/Player.qml \
	player/qml/PlayerControlLayout.qml \
	player/qml/PlayerPlaylistVisibilityFSM.qml \
	player/qml/PictureInPictureView.qml \
	player/qml/ContinueWatchingPill.qml \
	player/qml/ShortcutMessagePill.qml \
	player/qml/SliderBar.qml \
	player/qml/SubtitleTimelineBar.qml \
	player/qml/TopBar.qml \
	player/qml/TrackMenu.qml \
	player/qml/AdjustmentsSidebar.qml \
	player/qml/AdjustmentsAudioTab.qml \
	player/qml/AdjustmentsPlaybackTab.qml \
	player/qml/AdjustmentsVideoTab.qml \
	player/qml/StatisticsOverlay.qml \
	player/qml/AdjustmentsSubtitlesTab.qml \
	player/qml/PresetSelector.qml \
	player/qml/PresetMark.qml \
	player/qml/PresetNotice.qml \
	player/qml/ControlLayout.qml \
	player/qml/ControlRepeater.qml \
	player/qml/NavigationBox.qml

libqml_module_player_a_Singleton_QML = \
	player/qml/PlayerControlbarControls.qml
libqml_module_player_a_SOURCES = $(QML_MODULE_player_SRC)
nodist_libqml_module_player_a_SOURCES = player/qmlassets.cpp $(QML_MODULE_player_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_player.a

libqml_module_playercontrols_a_Module = VLC.PlayerControls
libqml_module_playercontrols_a_ModulePath = VLC/PlayerControls
libqml_module_playercontrols_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_playercontrols_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_playercontrols_a_QML = \
	player/qml/controlbarcontrols/HighResolutionTimeWidget.qml \
	player/qml/controlbarcontrols/ArtworkInfoWidget.qml \
	player/qml/controlbarcontrols/AspectRatioWidget.qml \
	player/qml/controlbarcontrols/AtoBButton.qml \
	player/qml/controlbarcontrols/AudioButton.qml \
	player/qml/controlbarcontrols/SeekBarWidget.qml \
	player/qml/controlbarcontrols/SubtitlesButton.qml \
	player/qml/controlbarcontrols/VlcMenuButton.qml \
	player/qml/controlbarcontrols/LibraryButton.qml \
	player/qml/controlbarcontrols/FolderButton.qml \
	player/qml/controlbarcontrols/TitleWidget.qml \
	player/qml/controlbarcontrols/DividerWidget.qml \
	player/qml/controlbarcontrols/TrackMenuButton.qml \
	player/qml/controlbarcontrols/BackButton.qml \
	player/qml/controlbarcontrols/BookmarkButton.qml \
	player/qml/controlbarcontrols/ChapterNextButton.qml \
	player/qml/controlbarcontrols/ChapterPreviousButton.qml \
	player/qml/controlbarcontrols/DvdMenuButton.qml \
	player/qml/controlbarcontrols/ExpandingSpacerWidget.qml \
	player/qml/controlbarcontrols/ExtendedSettingsButton.qml \
	player/qml/controlbarcontrols/FasterButton.qml \
	player/qml/controlbarcontrols/FrameNextButton.qml \
	player/qml/controlbarcontrols/FramePrevButton.qml \
	player/qml/controlbarcontrols/FullscreenButton.qml \
	player/qml/controlbarcontrols/PictureInPictureButton.qml \
	player/qml/controlbarcontrols/InfoButton.qml \
	player/qml/controlbarcontrols/LoopButton.qml \
	player/qml/controlbarcontrols/MenuButton.qml \
	player/qml/controlbarcontrols/NavigationBoxButton.qml \
	player/qml/controlbarcontrols/NavigationWidget.qml \
	player/qml/controlbarcontrols/NextButton.qml \
	player/qml/controlbarcontrols/OpenButton.qml \
	player/qml/controlbarcontrols/PlaybackSpeedButton.qml \
	player/qml/controlbarcontrols/PlayButton.qml \
	player/qml/controlbarcontrols/PlayerSwitchButton.qml \
	player/qml/controlbarcontrols/PlaylistButton.qml \
	player/qml/controlbarcontrols/PreviousButton.qml \
	player/qml/controlbarcontrols/ProgramButton.qml \
	player/qml/controlbarcontrols/QuitButton.qml \
	player/qml/controlbarcontrols/RandomButton.qml \
	player/qml/controlbarcontrols/CastButton.qml \
	player/qml/controlbarcontrols/RecordButton.qml \
	player/qml/controlbarcontrols/ReverseButton.qml \
	player/qml/controlbarcontrols/SkipBackButton.qml \
	player/qml/controlbarcontrols/SkipForwardButton.qml \
	player/qml/controlbarcontrols/SlowerButton.qml \
	player/qml/controlbarcontrols/SnapshotButton.qml \
	player/qml/controlbarcontrols/SpacerWidget.qml \
	player/qml/controlbarcontrols/StopButton.qml \
	player/qml/controlbarcontrols/TeletextButton.qml \
	player/qml/controlbarcontrols/TeletextColorButton.qml \
	player/qml/controlbarcontrols/TeletextWidget.qml \
	player/qml/controlbarcontrols/VolumeWidget.qml \
	player/qml/controlbarcontrols/Fallback.qml
libqml_module_playercontrols_a_Singleton_QML =
libqml_module_playercontrols_a_SOURCES = $(QML_MODULE_playercontrols_SRC)
nodist_libqml_module_playercontrols_a_SOURCES = playercontrols/qmlassets.cpp $(QML_MODULE_playercontrols_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_playercontrols.a

libqml_module_playlist_a_Module = VLC.Playlist
libqml_module_playlist_a_ModulePath = VLC/Playlist
libqml_module_playlist_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_playlist_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_playlist_a_QML = \
	playlist/qml/PlaylistDelegate.qml \
	playlist/qml/PlaylistPane.qml \
	playlist/qml/PlaylistDetachedWindow.qml \
	playlist/qml/QueueReplacedNotice.qml \
	playlist/qml/QueuePanel.qml
libqml_module_playlist_a_Singleton_QML =
libqml_module_playlist_a_SOURCES = $(QML_MODULE_playlist_SRC)
nodist_libqml_module_playlist_a_SOURCES = playlist/qmlassets.cpp $(QML_MODULE_playlist_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_playlist.a

libqml_module_style_a_Module = VLC.Style
libqml_module_style_a_ModulePath = VLC/Style
libqml_module_style_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_style_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_style_a_QML =
libqml_module_style_a_Singleton_QML = \
	style/AcrylicController.qml \
	style/VLCStyle.qml
libqml_module_style_a_SOURCES = $(QML_MODULE_style_SRC)
nodist_libqml_module_style_a_SOURCES = style/qmlassets.cpp $(QML_MODULE_style_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_style.a

libqml_module_util_a_Module = VLC.Util
libqml_module_util_a_ModulePath = VLC/Util
libqml_module_util_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_util_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_util_a_QML = \
	util/qml/ModelSortSettingHandler.qml \
	util/qml/DefaultFlickableScrollHandler.qml \
	util/qml/ViewDragAutoScrollHandler.qml \
	util/qml/FSM.qml \
	util/qml/FSMState.qml \
	util/qml/TrackMenuFSM.qml \
	util/qml/ControlBarFolding.qml \
	util/qml/RightPanelFSM.qml \
	util/qml/ChoicePreview.qml \
	util/qml/NativeMenu.qml \
	util/qml/FadeControllerStateGroup.qml \
	util/qml/BarRevealFSM.qml \
	util/qml/BarRevealStateGroup.qml \
	util/qml/GridSizeHelper.qml \
	util/qml/DelayedBehavior.qml \
	util/qml/DelegateTouchTapHandler.qml \
	util/qml/ClickGesture.qml \
	util/qml/TimedPrompt.qml \
	util/qml/LineKeyNavigation.qml
libqml_module_util_a_Singleton_QML = \
	util/qml/Helpers.qml
libqml_module_util_a_SOURCES = $(QML_MODULE_util_SRC)
nodist_libqml_module_util_a_SOURCES = util/qmlassets.cpp $(QML_MODULE_util_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_util.a

libqml_module_widgets_a_Module = VLC.Widgets
libqml_module_widgets_a_ModulePath = VLC/Widgets
libqml_module_widgets_a_CXXFLAGS = $(libqt_plugin_la_CXXFLAGS)
libqml_module_widgets_a_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
libqml_module_widgets_a_QML = \
	widgets/qml/LabelExt.qml \
	widgets/qml/ActionButtonOverlay.qml \
	widgets/qml/ActionButtonPrimary.qml \
	widgets/qml/BannerTabButton.qml \
	widgets/qml/BannerCone.qml \
	widgets/qml/BusyIndicatorExt.qml \
	widgets/qml/CaptionLabel.qml \
	widgets/qml/CheckButton.qml \
	widgets/qml/CheckedDelegate.qml \
	widgets/qml/ComboBoxExt.qml \
	widgets/qml/AcrylicBackground.qml \
	widgets/qml/TintBackground.qml \
	widgets/qml/FloatingNotification.qml \
	widgets/qml/AnimatedBackground.qml \
	widgets/qml/CSDThemeButton.qml \
	widgets/qml/CSDThemeButtonSet.qml \
	widgets/qml/CSDWindowButton.qml \
	widgets/qml/CSDWindowButtonSet.qml \
	widgets/qml/CSDTitlebarTapNDrapHandler.qml \
	widgets/qml/CSDMouseStealer.qml \
	widgets/qml/CurrentIndicator.qml \
	widgets/qml/DragItem.qml \
	widgets/qml/DrawerExt.qml \
	widgets/qml/ExpandGridView.qml \
	widgets/qml/ExpandGridItemView.qml \
	widgets/qml/EmptyLabel.qml \
	widgets/qml/EmptyLabelButton.qml \
	widgets/qml/EmptyLabelHint.qml \
	widgets/qml/GridItem.qml \
	widgets/qml/GridSortFilterControls.qml \
	widgets/qml/HorizontalResizeHandle.qml \
	widgets/qml/IconLabel.qml \
	widgets/qml/IconButton.qml \
	widgets/qml/IconToolButton.qml \
	widgets/qml/IconTrackButton.qml \
	widgets/qml/TwoToneIcon.qml \
	widgets/qml/PanelTab.qml \
	widgets/qml/ImageToolButton.qml \
	widgets/qml/ListViewExt.qml \
	widgets/qml/TableViewExt.qml \
	widgets/qml/TableViewDelegateExt.qml \
	widgets/qml/TableRowDelegate.qml \
	widgets/qml/TableHeaderDelegate.qml \
	widgets/qml/DefaultShadow.qml \
	widgets/qml/ListLabel.qml \
	widgets/qml/MediaCover.qml \
	widgets/qml/SubtitleLabel.qml \
	widgets/qml/MenuCaption.qml \
	widgets/qml/MenuLabel.qml \
	widgets/qml/NavigableCol.qml \
	widgets/qml/NavigableRow.qml \
	widgets/qml/PlayCover.qml \
	widgets/qml/RoundButtonExt.qml \
	widgets/qml/ScaledImage.qml \
	widgets/qml/TextAutoScroller.qml \
	widgets/qml/SearchBox.qml \
	widgets/qml/SliderExt.qml \
	widgets/qml/SortControl.qml \
	widgets/qml/SpinBoxExt.qml \
	widgets/qml/StackViewExt.qml \
	widgets/qml/PageLoader.qml \
	widgets/qml/PageExt.qml \
	widgets/qml/ButtonExt.qml \
	widgets/qml/TableColumns.qml \
	widgets/qml/TextFieldExt.qml \
	widgets/qml/TextToolButton.qml \
	widgets/qml/TrackDelayButton.qml \
	widgets/qml/VideoProgressBar.qml \
	widgets/qml/VideoQualityLabels.qml \
	widgets/qml/ListSubtitleLabel.qml \
	widgets/qml/PointingTooltip.qml \
	widgets/qml/FrostedGlassEffect.qml \
	widgets/qml/ToolTipExt.qml \
	widgets/qml/DropShadowImage.qml \
	widgets/qml/DoubleShadow.qml \
	widgets/qml/FadingEdge.qml \
	widgets/qml/FadingEdgeForItemView.qml \
	widgets/qml/PopupIconToolButton.qml \
	widgets/qml/PartialEffect.qml \
	widgets/qml/ViewHeader.qml \
	widgets/qml/ProgressIndicator.qml \
	widgets/qml/ImageExt.qml \
	widgets/qml/EnhancedImageExt.qml \
	widgets/qml/ScrollBarExt.qml \
	widgets/qml/FastBlend.qml \
	widgets/qml/RadioButtonExt.qml \
	widgets/qml/RoundedRectangleShadow.qml \
	widgets/qml/VoronoiSnow.qml \
	widgets/qml/DualKawaseBlur.qml \
	widgets/qml/CheckBoxExt.qml \
	widgets/qml/ProgressBarExt.qml
libqml_module_widgets_a_Singleton_QML =
libqml_module_widgets_a_SOURCES = $(QML_MODULE_widgets_SRC)
nodist_libqml_module_widgets_a_SOURCES = widgets/qmlassets.cpp $(QML_MODULE_widgets_NODIST_EXTRA)
QML_MODULE_LIBS += libqml_module_widgets.a

if ENABLE_QT
libqt_plugin_la_LIBADD += $(QML_MODULE_LIBS)
noinst_LIBRARIES += $(QML_MODULE_LIBS)
endif

if HAVE_QMLCACHEGEN
QML_MODULE_dialogs_SRC = $(libqml_module_dialogs_a_QML) $(libqml_module_dialogs_a_Singleton_QML)
QML_MODULE_dialogs_NODIST_EXTRA = dialogs/qmlcache_loader.cpp
QML_MODULE_maininterface_SRC = $(libqml_module_maininterface_a_QML) $(libqml_module_maininterface_a_Singleton_QML)
QML_MODULE_maininterface_NODIST_EXTRA = maininterface/qmlcache_loader.cpp
QML_MODULE_medialibrary_SRC = $(libqml_module_medialibrary_a_QML) $(libqml_module_medialibrary_a_Singleton_QML)
QML_MODULE_medialibrary_NODIST_EXTRA = medialibrary/qmlcache_loader.cpp
QML_MODULE_menus_SRC = $(libqml_module_menus_a_QML) $(libqml_module_menus_a_Singleton_QML)
QML_MODULE_menus_NODIST_EXTRA = menus/qmlcache_loader.cpp
QML_MODULE_network_SRC = $(libqml_module_network_a_QML) $(libqml_module_network_a_Singleton_QML)
QML_MODULE_network_NODIST_EXTRA = network/qmlcache_loader.cpp
QML_MODULE_player_SRC = $(libqml_module_player_a_QML) $(libqml_module_player_a_Singleton_QML)
QML_MODULE_player_NODIST_EXTRA = player/qmlcache_loader.cpp
QML_MODULE_playercontrols_SRC = $(libqml_module_playercontrols_a_QML) $(libqml_module_playercontrols_a_Singleton_QML)
QML_MODULE_playercontrols_NODIST_EXTRA = playercontrols/qmlcache_loader.cpp
QML_MODULE_playlist_SRC = $(libqml_module_playlist_a_QML) $(libqml_module_playlist_a_Singleton_QML)
QML_MODULE_playlist_NODIST_EXTRA = playlist/qmlcache_loader.cpp
QML_MODULE_style_SRC = $(libqml_module_style_a_QML) $(libqml_module_style_a_Singleton_QML)
QML_MODULE_style_NODIST_EXTRA = style/qmlcache_loader.cpp
QML_MODULE_util_SRC = $(libqml_module_util_a_QML) $(libqml_module_util_a_Singleton_QML)
QML_MODULE_util_NODIST_EXTRA = util/qmlcache_loader.cpp
QML_MODULE_widgets_SRC = $(libqml_module_widgets_a_QML) $(libqml_module_widgets_a_Singleton_QML)
QML_MODULE_widgets_NODIST_EXTRA = widgets/qmlcache_loader.cpp
endif


libqt_plugin_la_QML_MODULES = \
	dialogs \
	maininterface \
	medialibrary \
	menus \
	network \
	player \
	playercontrols \
	playlist \
	style \
	util \
	widgets

libqt_plugin_la_ALL_QML = \
	$(libqml_module_dialogs_a_QML) \
	$(libqml_module_maininterface_a_QML) \
	$(libqml_module_medialibrary_a_QML) \
	$(libqml_module_menus_a_QML) \
	$(libqml_module_network_a_QML) \
	$(libqml_module_player_a_QML) \
	$(libqml_module_player_a_Singleton_QML) \
	$(libqml_module_playercontrols_a_QML) \
	$(libqml_module_playlist_a_QML) \
	$(libqml_module_style_a_QML) \
	$(libqml_module_style_a_Singleton_QML) \
	$(libqml_module_util_a_QML) \
	$(libqml_module_util_a_Singleton_QML) \
	$(libqml_module_widgets_a_QML)

# Shaders:
libqt_plugin_la_SHADER_FRAG = shaders/FadingEdge.frag \
                              shaders/Noise.frag \
                              shaders/RectFilter.frag \
                              shaders/SDFAARoundedTexture.frag \
                              shaders/SDFAARoundedTexture_cropsupport_bordersupport.frag \
                              shaders/DitheredTexture.frag \
                              shaders/FastBlend.frag \
                              shaders/FastBlend_additive.frag \
                              shaders/FastBlend_multiply.frag \
                              shaders/FastBlend_screen.frag \
                              shaders/RoundedRectangleShadow.frag \
                              shaders/RoundedRectangleShadow_hollow.frag \
                              shaders/VoronoiSnow.frag \
                              shaders/DualKawaseBlur_upsample.frag \
                              shaders/DualKawaseBlur_downsample.frag \
                              shaders/DualKawaseBlur_upsample_postprocess.frag

libqt_plugin_la_SHADER_VERT = shaders/FadingEdgeX.vert \
                              shaders/FadingEdgeY.vert \
                              shaders/SubTexture.vert

libqt_plugin_la_SHADER = $(libqt_plugin_la_SHADER_FRAG) $(libqt_plugin_la_SHADER_VERT)

if ENABLE_QT

BUILT_SOURCES += \
    $(libqt_plugin_la_SHADER_FRAG:.frag=.frag.qsb) \
    $(libqt_plugin_la_SHADER_VERT:.vert=.vert.qsb)

QSB_PARAMS = --glsl="300 es,130,150" --batchable -O
if HAVE_WIN32
QSB_PARAMS += --hlsl=50 -c
endif
if HAVE_DARWIN
QSB_PARAMS += --msl=12
endif

QSB_EXTRA_PATH =
if HAVE_FXC
QSB_EXTRA_PATH += $(FXC_PATH):
endif

qsb_verbose = $(qsb_verbose_$(V))
qsb_verbose_ = $(qsb_verbose__$(AM_DEFAULT_VERBOSITY))
qsb_verbose_0 = @echo "  QSB     " $@;
qsb_verbose__0 = $(qsb_verbose_0)

QSB_PARAMS_VERBOSE = $(QSB_PARAMS_VERBOSE_$(V))
QSB_PARAMS_VERBOSE_ = $(QSB_PARAMS_VERBOSE__$(AM_DEFAULT_VERBOSITY))
QSB_PARAMS_VERBOSE_0 = -s
QSB_PARAMS_VERBOSE__0 = $(QSB_PARAMS_VERBOSE_0)

EXTRA_DIST += shaders/Common.glsl

$(libqt_plugin_la_SHADER_FRAG:.frag=.frag.qsb) $(libqt_plugin_la_SHADER_VERT:.vert=.vert.qsb): shaders/Common.glsl

$(libqt_plugin_la_SHADER_FRAG:.frag=.frag.qsb):
	$(qsb_verbose)PATH="$(QSB_EXTRA_PATH)$$PATH" $(QSB) $(QSB_PARAMS) $(QSB_PARAMS_VERBOSE) -o $@ $(srcdir)/$(@:.qsb=)

$(libqt_plugin_la_SHADER_VERT:.vert=.vert.qsb):
	$(qsb_verbose)PATH="$(QSB_EXTRA_PATH)$$PATH" $(QSB) $(QSB_PARAMS) $(QSB_PARAMS_VERBOSE) -o $@ $(srcdir)/$(@:.qsb=)

shaders/shaders.qrc: $(libqt_plugin_la_SHADER) Makefile
	$(AM_V_GEN)$(PYTHON3) $(srcdir)/scripts/genqrc.py -o $@ --prefix shaders --sources $(libqt_plugin_la_SHADER_FRAG:.frag=.frag.qsb) $(libqt_plugin_la_SHADER_VERT:.vert=.vert.qsb)

shaders/shaders.cpp: shaders/shaders.qrc $(libqt_plugin_la_SHADER_FRAG:.frag=.frag.qsb) $(libqt_plugin_la_SHADER_VERT:.vert=.vert.qsb)
	$(AM_V_GEN) $(RCC) $(RCC_EXTRA_PARAMS) --name shaders -o $@ $<

libqt_plugin_la_SOURCES += $(libqt_plugin_la_SHADER)
CLEANFILES += shaders/shaders.qrc

lib_qt_plugin_la_QRC = assets.qrc

if HAVE_WIN32
lib_qt_plugin_la_QRC += windows.qrc

windows.cpp: $(libqt_plugin_la_RES)
windows.cpp: windows.qrc
	$(AM_V_GEN)$(RCC) $(RCC_EXTRA_PARAMS) --name windows -o $@ $<

nodist_libqt_plugin_la_SOURCES += windows.cpp
endif

EXTRA_DIST += $(lib_qt_plugin_la_QRC) $(libqt_plugin_la_RES) $(libqt_plugin_la_SHADER)
EXTRA_DIST += fonts/Inter-OFL.txt fonts/DMMono-OFL.txt

nodist_libqt_plugin_la_SOURCES += assets.cpp  shaders/shaders.cpp
endif

# Emit shader source dependencies so that the built shaders are properly
# registered with the sources as prerequisites
shaders/shaders.deps: Makefile
	$(AM_V_at)$(MKDIR_P) $(@D)
	$(AM_V_GEN){ \
	  for s in $(libqt_plugin_la_SHADER_FRAG) $(libqt_plugin_la_SHADER_VERT); do \
	    echo "$$s.qsb: $(srcdir)/$$s"; \
	  done; \
	} > $@.tmp && \
	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

BUILT_SOURCES += shaders/shaders.deps
CLEANFILES    += shaders/shaders.deps

-include shaders/shaders.deps


# Dynamically generate the rules dependencies so that rebuilds are working
# properly. Since this is not GNU Make, we cannot generate the rules using
# text functions.
QML_RULES_DEPS = $(libqt_plugin_la_QML_MODULES:=/qml.deps)

# Since every QML modules is built into a given directory, we can use $(@D)
# to get the module name.
$(QML_RULES_DEPS): Makefile
	$(AM_V_at)$(MKDIR_P) $(@D)
	$(AM_V_GEN){ \
	  echo '$(@D)/qmldir.qrc: $(@D)/qmldir'; \
	  echo '$(@D)/res.qrc $(@D)/qmldir: $(@D)/qml_modules.stamp'; \
	  echo '$(@D)/qmlcachegen.rsp: $$(builddir)/$(@D)/res.qrc'; \
	  echo '$(@D)/qmlcache_loader.cpp: $(@D)/qmlcachegen.rsp'; \
	  echo '$(@D)/qmlassets.cpp: $$(builddir)/$(@D)/qmldir.qrc $$(builddir)/$(@D)/res.qrc'; \
	  echo '$(@D)/qmlassets.cpp: $$(libqml_module_$(@D)_a_QML) $$(libqml_module_$(@D)_a_Singleton_QML)'; \
	  echo '$$(libqml_module_$(@D)_a_QML:.qml=.cpp) $$(libqml_module_$(@D)_a_Singleton_QML:.qml=.cpp): $$(builddir)/$(@D)/res.qrc'; \
	} > $@.tmp && \
	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

BUILT_SOURCES += $(QML_RULES_DEPS)
CLEANFILES += $(QML_RULES_DEPS)

-include $(QML_RULES_DEPS)

# QRC/qmldir generation depends on the list of files (defined here) but
# not the file contents. Use per-module stamp files so that changes to
# one module's file list don't trigger rebuilds in other modules.
QML_MODULE_STAMPS =
if ENABLE_QT
QML_MODULE_STAMPS += $(libqt_plugin_la_QML_MODULES:=/qml_modules.stamp)
endif

# Pre-expand target lists into named variables so that automake does not
# see the same '$(libqt_plugin_la_QML_MODULES' prefix repeated across
# conditionals and emit spurious "was already defined" warnings.
QML_MODULE_RES_QRC = $(libqt_plugin_la_QML_MODULES:=/res.qrc)
QML_MODULE_QMLDIR = $(libqt_plugin_la_QML_MODULES:=/qmldir)
QML_MODULE_QMLDIR_QRC = $(libqt_plugin_la_QML_MODULES:=/qmldir.qrc)

$(QML_MODULE_STAMPS): Makefile
	$(AM_V_at)$(MKDIR_P) $(@D); \
	echo '$(libqml_module_$(@D)_a_Module) $(libqml_module_$(@D)_a_ModulePath) $(libqml_module_$(@D)_a_QML) $(libqml_module_$(@D)_a_Singleton_QML)' > $@.tmp; \
	if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

$(QML_MODULE_RES_QRC):
	$(AM_V_GEN)$(PYTHON3) $(srcdir)/scripts/genqrc.py \
	-o $@ \
	-s $(srcdir) \
	--prefix qt/qml/$(libqml_module_$(@D)_a_ModulePath) \
	--sources $(libqml_module_$(@D)_a_QML) $(libqml_module_$(@D)_a_Singleton_QML)

$(QML_MODULE_QMLDIR):
	$(AM_V_GEN)$(PYTHON3) $(srcdir)/scripts/genqmldir.py \
	-o $@ \
	--prefix qt/qml \
	--module $(libqml_module_$(@D)_a_Module) \
	--sources $(libqml_module_$(@D)_a_QML) \
	--singletons $(libqml_module_$(@D)_a_Singleton_QML)

$(QML_MODULE_QMLDIR_QRC):
	$(AM_V_GEN)$(PYTHON3) $(srcdir)/scripts/genqrc.py -o $@ \
	--prefix qt/qml/$(libqml_module_$(@D)_a_ModulePath) --sources $<

CLEANFILES += $(QML_MODULE_STAMPS) $(QML_MODULE_RES_QRC) $(QML_MODULE_QMLDIR) $(QML_MODULE_QMLDIR_QRC) $(libqml_module_util_a_QML:.qml=.cpp)

if HAVE_QMLCACHEGEN

libqt_plugin_la_CPPFLAGS += -DQT_USE_QMLCACHEGEN

cachegen_verbose = $(cachegen_verbose_$(V))
cachegen_verbose_ = $(cachegen_verbose__$(AM_DEFAULT_VERBOSITY))
cachegen_verbose_0 = @echo " CACHEGEN " $@;
cachegen_verbose__0 = $(cachegen_verbose_0)

if HAVE_QT67
QML_CACHEGEN_EXTRA_ARGS=--validate-basic-blocks
endif

# Each .qml/.js/.mjs is compiled with its module's res.qrc as prerequisite. The
# per-module qrc isn't derivable from the target path, so pick it out of $^.
qml_resource = for f in $^; do case $$f in *.qrc) echo --resource=$$f;; esac; done

.qml.cpp:
	$(cachegen_verbose)$(QMLCACHEGEN) $(QML_CACHEGEN_EXTRA_ARGS) `$(qml_resource)` -o $@ $<

.js.cpp:
	$(cachegen_verbose)$(QMLCACHEGEN) $(QML_CACHEGEN_EXTRA_ARGS) `$(qml_resource)` -o $@ $<

.mjs.cpp:
	$(cachegen_verbose)$(QMLCACHEGEN) $(QML_CACHEGEN_EXTRA_ARGS) `$(qml_resource)` -o $@ $<

# Pre-expand target lists into named variables so that automake does not
# see the same $(libqt_plugin_la_QML_MODULES prefix repeated across
# conditionals and emit spurious "was already defined" warnings.
QML_MODULE_CACHEGEN_RSP = $(libqt_plugin_la_QML_MODULES:=/qmlcachegen.rsp)
QML_MODULE_CACHELOADER = $(libqt_plugin_la_QML_MODULES:=/qmlcache_loader.cpp)
QML_MODULE_CACHEGEN_ASSETS = $(libqt_plugin_la_QML_MODULES:=/qmlassets.cpp)

$(QML_MODULE_CACHEGEN_RSP):
	$(AM_V_GEN)$(PYTHON3) $(srcdir)/scripts/qrc2rsp.py --output $@ $^

$(QML_MODULE_CACHELOADER):
	$(cachegen_verbose)$(QMLCACHEGEN) --resource-name $(@D)_cachegen --resource=$(builddir)/$(@D)/res.qrc -o $@ @$<

$(QML_MODULE_CACHEGEN_ASSETS):
	$(AM_V_GEN)$(RCC) $(RCC_EXTRA_PARAMS) --name $(@D)_assets -o $@ $<


CLEANFILES += \
	$(QML_MODULE_CACHELOADER) \
	$(QML_MODULE_CACHEGEN_RSP) \
	$(QML_MODULE_CACHEGEN_ASSETS) \
	$(libqt_plugin_la_ALL_QML:.qml=.cpp)

else

#package all qml from the genreated QRC files

# See comment above QML_MODULE_RES_QRC for why this is a named variable.
QML_MODULE_ASSETS = $(libqt_plugin_la_QML_MODULES:=/qmlassets.cpp)

$(QML_MODULE_ASSETS):
	$(AM_V_GEN)$(RCC) $(RCC_EXTRA_PARAMS) --name $(@D)_assets \
	-o $@ $(builddir)/$(@D)/qmldir.qrc $(builddir)/$(@D)/res.qrc

nodist_libqt_plugin_la_SOURCES += $(QML_MODULE_ASSETS)
if ENABLE_QT
BUILT_SOURCES += $(QML_MODULE_RES_QRC) $(QML_MODULE_QMLDIR)
endif

endif

EXTRA_DIST += $(libqt_plugin_la_QML)

assets.cpp: $(libqt_plugin_la_RES)
assets.cpp: assets.qrc
	$(AM_V_GEN)$(RCC) $(RCC_EXTRA_PARAMS) --name assets -o $@ $<



if ENABLE_QT
gui_PLUGINS += libqt_plugin.la
BUILT_SOURCES += $(nodist_libqt_plugin_la_SOURCES)
if !HAVE_WIN32
if !HAVE_OS2
if !HAVE_IOS_OR_TVOS
if !HAVE_XROS
pkglibexec_PROGRAMS = vlc-qt-check
VLC_QT_CHECK_DEP = $(builddir)/../../vlc-qt-check

$(builddir)/../../vlc-qt-check: vlc-qt-check
	$(AM_V_GEN)cd ../../ && $(LN_S) -f gui/qt/vlc-qt-check . && chmod +x vlc-qt-check
endif
endif
endif
endif

base_model_test_SOURCES = tests/test_base_model.cpp \
        util/base_model.cpp util/base_model.hpp util/base_model_p.hpp

nodist_base_model_test_SOURCES = util/base_model.moc.cpp \
        util/listcache.moc.cpp \
        util/locallistcacheloader.moc.cpp

base_model_test_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC $(CXXFLAGS_qt)
base_model_test_LDADD = $(QT_LIBS) $(LIBS_qt)
base_model_test_LDFLAGS = $(AM_LDFLAGS) $(QT_LDFLAGS)
check_PROGRAMS = base_model_test
TESTS = base_model_test

if HAVE_QT_QTEST

LIBVLC = ../../../lib/libvlc.la
QT_QTEST_COMMON_cppflags = $(libqt_plugin_la_CPPFLAGS) -I$(builddir)/tests -DTOP_BUILDDIR=\"$(abs_top_builddir)\"
QT_QTEST_COMMON_cxxflags = $(AM_CXXFLAGS) $(QT_CFLAGS) $(QT_QTEST_CFLAGS) -fPIC $(CXXFLAGS_qt)
QT_QTEST_COMMON_ldadd = $(QT_LIBS) $(LIBS_qt) $(QT_QTEST_LIBS) $(LIBVLCCORE) $(LIBVLC)
QT_QTEST_COMMON_ldflags = $(AM_LDFLAGS) $(QT_LDFLAGS) $(QT_QTEST_LDFLAGS)


# test_queue

test_queue_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_queue.cpp \
	util/vlctick.cpp util/vlctick.hpp \
	playlist/playlist_controller.hpp playlist/playlist_controller.cpp \
	playlist/playlist_item.hpp playlist/playlist_item.cpp \
	playlist/queue_store.hpp playlist/queue_store.cpp

nodist_test_queue_SOURCES = \
	tests/test_queue.moc \
	util/vlctick.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp \
	playlist/queue_store.moc.cpp

BUILT_SOURCES += tests/test_queue.moc
CLEANFILES += tests/test_queue.moc
test_queue_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_queue_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_queue_LDADD = $(QT_QTEST_COMMON_ldadd)
test_queue_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_queue
TESTS += test_queue

# test_pasted_media

test_pasted_media_SOURCES = \
	tests/test_pasted_media.cpp \
	playlist/pasted_media.hpp playlist/pasted_media.cpp

nodist_test_pasted_media_SOURCES = tests/test_pasted_media.moc

BUILT_SOURCES += tests/test_pasted_media.moc
CLEANFILES += tests/test_pasted_media.moc
test_pasted_media_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_pasted_media_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_pasted_media_LDADD = $(QT_QTEST_COMMON_ldadd)
test_pasted_media_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_pasted_media
TESTS += test_pasted_media

# test_track_models

test_track_models_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_track_models.cpp \
	player/input_models.hpp player/input_models.cpp \
	util/audio_device_model.hpp util/audio_device_model.cpp \
	util/vlctick.hpp util/vlctick.cpp

nodist_test_track_models_SOURCES = \
	tests/test_track_models.moc \
	player/input_models.moc.cpp \
	util/audio_device_model.moc.cpp \
	util/vlctick.moc.cpp

BUILT_SOURCES += tests/test_track_models.moc
CLEANFILES += tests/test_track_models.moc
test_track_models_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_track_models_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_track_models_LDADD = $(QT_QTEST_COMMON_ldadd)
test_track_models_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_track_models
TESTS += test_track_models

# test_queue_panel

test_queue_panel_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_queue_panel.cpp \
	util/vlctick.cpp util/vlctick.hpp \
	playlist/playlist_controller.hpp playlist/playlist_controller.cpp \
	playlist/playlist_item.hpp playlist/playlist_item.cpp \
	playlist/playlist_model.hpp playlist/playlist_model.cpp

nodist_test_queue_panel_SOURCES = \
	tests/test_queue_panel.moc \
	util/vlctick.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp \
	playlist/playlist_model.moc.cpp

BUILT_SOURCES += tests/test_queue_panel.moc
CLEANFILES += tests/test_queue_panel.moc
test_queue_panel_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_queue_panel_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_queue_panel_LDADD = $(QT_QTEST_COMMON_ldadd)
test_queue_panel_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_queue_panel
TESTS += test_queue_panel

# test_ml_model

test_ml_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_ml_model.cpp \
	util/base_model.hpp util/base_model_p.hpp  util/base_model.cpp \
	util/listcache.hpp \
	util/vlctick.cpp util/vlctick.hpp \
	util/locallistcacheloader.hpp \
	medialibrary/mlqmltypes.hpp \
	medialibrary/mlbasemodel.hpp medialibrary/mlbasemodel.cpp \
	medialibrary/mlplaycountrank.hpp medialibrary/mlplaycountrank.cpp \
	medialibrary/medialib.hpp medialibrary/medialib.cpp \
	medialibrary/mlplaylistmanager.hpp medialibrary/mlplaylistmanager.cpp \
	medialibrary/mlrecenthidelist.hpp medialibrary/mlrecenthidelist.cpp \
	medialibrary/mlthreadpool.hpp medialibrary/mlthreadpool.cpp \
	medialibrary/mlfoldertreemodel.hpp medialibrary/mlfoldertreemodel.cpp \
	medialibrary/mlmediafolder.hpp medialibrary/mlmediafolder.cpp \
	medialibrary/mlfolderactions.hpp medialibrary/mlfolderactions.cpp \
	medialibrary/mlmusicfoldertreemodel.hpp medialibrary/mlmusicfoldertreemodel.cpp \
	medialibrary/mlaudiomodel.hpp medialibrary/mlaudiomodel.cpp \
	medialibrary/mlvideomodel.hpp medialibrary/mlvideomodel.cpp \
	medialibrary/mlvideofoldersmodel.hpp medialibrary/mlvideofoldersmodel.cpp \
	medialibrary/mlmediamodel.hpp medialibrary/mlmediamodel.cpp \
	medialibrary/mlvideo.hpp medialibrary/mlvideo.cpp \
	medialibrary/mlaudio.hpp medialibrary/mlaudio.cpp \
	medialibrary/mlfolder.hpp medialibrary/mlfolder.cpp \
	medialibrary/mlhelper.hpp medialibrary/mlhelper.cpp \
	medialibrary/mlcustomcover.hpp medialibrary/mlcustomcover.cpp \
	medialibrary/thumbnailcollector.hpp medialibrary/thumbnailcollector.cpp \
	util/covergenerator.hpp util/covergenerator.cpp \
	util/asynctask.hpp \
	playlist/playlist_controller.hpp playlist/playlist_controller.cpp \
	playlist/playlist_item.hpp playlist/playlist_item.cpp

nodist_test_ml_model_SOURCES = \
	tests/test_ml_model.moc \
	util/base_model.moc.cpp \
	util/listcache.moc.cpp \
	util/locallistcacheloader.moc.cpp \
	util/vlctick.moc.cpp \
	util/asynctask.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlfoldertreemodel.moc.cpp \
	medialibrary/mlmediafolder.moc.cpp \
	medialibrary/mlfolderactions.moc.cpp \
	medialibrary/mlmusicfoldertreemodel.moc.cpp \
	medialibrary/mlaudiomodel.moc.cpp \
	medialibrary/mlvideomodel.moc.cpp \
	medialibrary/mlvideofoldersmodel.moc.cpp \
	medialibrary/mlmediamodel.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlplaylistmanager.moc.cpp \
	medialibrary/mlrecenthidelist.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp

BUILT_SOURCES += tests/test_ml_model.moc
CLEANFILES += tests/test_ml_model.moc
test_ml_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_ml_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_ml_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_ml_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_ml_model
TESTS += test_ml_model

# test_ml_playlists

test_ml_playlists_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_ml_playlists.cpp \
	util/asynctask.hpp \
	util/base_model.hpp util/base_model_p.hpp util/base_model.cpp \
	util/covergenerator.hpp util/covergenerator.cpp \
	util/listcache.hpp \
	util/vlctick.cpp util/vlctick.hpp \
	util/locallistcacheloader.hpp \
	medialibrary/medialib.hpp medialibrary/medialib.cpp \
	medialibrary/mlbasemodel.hpp medialibrary/mlbasemodel.cpp \
	medialibrary/mlcustomcover.hpp medialibrary/mlcustomcover.cpp \
	medialibrary/mlplaycountrank.hpp medialibrary/mlplaycountrank.cpp \
	medialibrary/mlhelper.hpp medialibrary/mlhelper.cpp \
	medialibrary/mlplaylist.hpp medialibrary/mlplaylist.cpp \
	medialibrary/mlplaylistlistmodel.hpp medialibrary/mlplaylistlistmodel.cpp \
	medialibrary/mlplaylistmanager.hpp medialibrary/mlplaylistmanager.cpp \
	medialibrary/mlplaylistmedia.hpp medialibrary/mlplaylistmedia.cpp \
	medialibrary/mlplaylistmodel.hpp medialibrary/mlplaylistmodel.cpp \
	medialibrary/mlqmltypes.hpp \
	medialibrary/mlrecenthidelist.hpp medialibrary/mlrecenthidelist.cpp \
	medialibrary/mlthreadpool.hpp medialibrary/mlthreadpool.cpp \
	medialibrary/mlvideo.hpp medialibrary/mlvideo.cpp \
	medialibrary/thumbnailcollector.hpp medialibrary/thumbnailcollector.cpp \
	playlist/playlist_controller.hpp playlist/playlist_controller.cpp \
	playlist/playlist_item.hpp playlist/playlist_item.cpp

nodist_test_ml_playlists_SOURCES = \
	tests/test_ml_playlists.moc \
	util/asynctask.moc.cpp \
	util/base_model.moc.cpp \
	util/listcache.moc.cpp \
	util/locallistcacheloader.moc.cpp \
	util/vlctick.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlplaylistlistmodel.moc.cpp \
	medialibrary/mlplaylistmanager.moc.cpp \
	medialibrary/mlplaylistmodel.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp \
	medialibrary/mlrecenthidelist.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp

BUILT_SOURCES += tests/test_ml_playlists.moc
CLEANFILES += tests/test_ml_playlists.moc
test_ml_playlists_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_ml_playlists_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_ml_playlists_LDADD = $(QT_QTEST_COMMON_ldadd)
test_ml_playlists_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_ml_playlists
TESTS += test_ml_playlists

# test_ml_recent

test_ml_recent_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_ml_recent.cpp \
	util/asynctask.hpp \
	util/base_model.hpp util/base_model_p.hpp util/base_model.cpp \
	util/covergenerator.hpp util/covergenerator.cpp \
	util/listcache.hpp \
	util/vlctick.cpp util/vlctick.hpp \
	util/locallistcacheloader.hpp \
	medialibrary/medialib.hpp medialibrary/medialib.cpp \
	medialibrary/mlaudio.hpp medialibrary/mlaudio.cpp \
	medialibrary/mlbasemodel.hpp medialibrary/mlbasemodel.cpp \
	medialibrary/mlcustomcover.hpp medialibrary/mlcustomcover.cpp \
	medialibrary/mlhelper.hpp medialibrary/mlhelper.cpp \
	medialibrary/mlmediamodel.hpp medialibrary/mlmediamodel.cpp \
	medialibrary/mlplaycountrank.hpp medialibrary/mlplaycountrank.cpp \
	medialibrary/mlplaylist.hpp medialibrary/mlplaylist.cpp \
	medialibrary/mlplaylistmanager.hpp medialibrary/mlplaylistmanager.cpp \
	medialibrary/mlqmltypes.hpp \
	medialibrary/mlrecenthidelist.hpp medialibrary/mlrecenthidelist.cpp \
	medialibrary/mlrecentmediamodel.hpp medialibrary/mlrecentmediamodel.cpp \
	medialibrary/mlrecentvideomodel.hpp medialibrary/mlrecentvideomodel.cpp \
	medialibrary/mlthreadpool.hpp medialibrary/mlthreadpool.cpp \
	medialibrary/mlvideo.hpp medialibrary/mlvideo.cpp \
	medialibrary/mlvideomodel.hpp medialibrary/mlvideomodel.cpp \
	medialibrary/thumbnailcollector.hpp medialibrary/thumbnailcollector.cpp \
	playlist/playlist_controller.hpp playlist/playlist_controller.cpp \
	playlist/playlist_item.hpp playlist/playlist_item.cpp

nodist_test_ml_recent_SOURCES = \
	tests/test_ml_recent.moc \
	util/asynctask.moc.cpp \
	util/base_model.moc.cpp \
	util/listcache.moc.cpp \
	util/locallistcacheloader.moc.cpp \
	util/vlctick.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlmediamodel.moc.cpp \
	medialibrary/mlplaylistmanager.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp \
	medialibrary/mlrecenthidelist.moc.cpp \
	medialibrary/mlrecentmediamodel.moc.cpp \
	medialibrary/mlrecentvideomodel.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/mlvideomodel.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp

BUILT_SOURCES += tests/test_ml_recent.moc
CLEANFILES += tests/test_ml_recent.moc
test_ml_recent_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_ml_recent_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_ml_recent_LDADD = $(QT_QTEST_COMMON_ldadd)
test_ml_recent_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_ml_recent
TESTS += test_ml_recent

# test_renderer_manager_model

test_renderer_manager_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_renderer_manager_model.cpp \
    util/renderer_manager.hpp util/renderer_manager.cpp

nodist_test_renderer_manager_model_SOURCES = \
	tests/test_renderer_manager_model.moc \
    util/renderer_manager.moc.cpp

BUILT_SOURCES += tests/test_renderer_manager_model.moc
CLEANFILES += tests/test_renderer_manager_model.moc
test_renderer_manager_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_renderer_manager_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_renderer_manager_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_renderer_manager_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_renderer_manager_model
TESTS += test_renderer_manager_model

# test_subtitle_matches
test_subtitle_matches_SOURCES = \
	tests/test_subtitle_matches.cpp \
	tests/vlc_stub_modules.cpp \
	tests/vlc_stub_modules.hpp \
	dialogs/subtitles/add_subtitles_model.cpp \
	dialogs/subtitles/add_subtitles_model.hpp \
	dialogs/subtitles/http_request.cpp \
	dialogs/subtitles/http_request.hpp \
	dialogs/subtitles/subtitle_matching.cpp \
	dialogs/subtitles/subtitle_matching.hpp \
	player/input_models.cpp \
	player/input_models.hpp \
	util/base_model.cpp \
	util/base_model.hpp \
	util/covergenerator.cpp \
	util/vlctick.cpp \
	medialibrary/medialib.cpp \
	medialibrary/mlbasemodel.cpp \
	medialibrary/mlcustomcover.cpp \
	medialibrary/mlplaycountrank.cpp \
	medialibrary/mlhelper.cpp \
	medialibrary/mlplaylist.cpp \
	medialibrary/mlplaylistlistmodel.cpp \
	medialibrary/mlplaylistmanager.cpp \
	medialibrary/mlplaylistmedia.cpp \
	medialibrary/mlplaylistmodel.cpp \
	medialibrary/mlrecenthidelist.cpp \
	medialibrary/mlthreadpool.cpp \
	medialibrary/mlvideo.cpp \
	medialibrary/thumbnailcollector.cpp \
	playlist/playlist_controller.cpp \
	playlist/playlist_item.cpp
nodist_test_subtitle_matches_SOURCES = \
	tests/test_subtitle_matches.moc \
	dialogs/subtitles/add_subtitles_model.moc.cpp \
	dialogs/subtitles/subtitle_matching.moc.cpp \
	player/input_models.moc.cpp \
	util/base_model.moc.cpp \
	util/vlctick.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlplaylistlistmodel.moc.cpp \
	medialibrary/mlplaylistmanager.moc.cpp \
	medialibrary/mlplaylistmodel.moc.cpp \
	medialibrary/mlrecenthidelist.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp
BUILT_SOURCES += tests/test_subtitle_matches.moc
CLEANFILES += tests/test_subtitle_matches.moc
test_subtitle_matches_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_subtitle_matches_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_subtitle_matches_LDADD = $(QT_QTEST_COMMON_ldadd)
test_subtitle_matches_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_subtitle_matches
TESTS += test_subtitle_matches

# test_opensubtitles
test_opensubtitles_SOURCES = \
	tests/test_opensubtitles.cpp \
	tests/vlc_stub_modules.cpp \
	tests/vlc_stub_modules.hpp \
	dialogs/subtitles/add_subtitles_model.cpp \
	dialogs/subtitles/add_subtitles_model.hpp \
	dialogs/subtitles/http_request.cpp \
	dialogs/subtitles/http_request.hpp \
	dialogs/subtitles/opensubtitles.cpp \
	dialogs/subtitles/opensubtitles.hpp \
	dialogs/subtitles/opensubtitles_models.cpp \
	dialogs/subtitles/opensubtitles_models.hpp \
	dialogs/subtitles/subtitle_matching.cpp \
	dialogs/subtitles/subtitle_matching.hpp \
	player/input_models.cpp \
	player/input_models.hpp \
	util/base_model.cpp \
	util/base_model.hpp \
	util/covergenerator.cpp \
	util/vlctick.cpp \
	medialibrary/medialib.cpp \
	medialibrary/mlbasemodel.cpp \
	medialibrary/mlcustomcover.cpp \
	medialibrary/mlplaycountrank.cpp \
	medialibrary/mlhelper.cpp \
	medialibrary/mlplaylist.cpp \
	medialibrary/mlplaylistlistmodel.cpp \
	medialibrary/mlplaylistmanager.cpp \
	medialibrary/mlplaylistmedia.cpp \
	medialibrary/mlplaylistmodel.cpp \
	medialibrary/mlrecenthidelist.cpp \
	medialibrary/mlthreadpool.cpp \
	medialibrary/mlvideo.cpp \
	medialibrary/thumbnailcollector.cpp \
	playlist/playlist_controller.cpp \
	playlist/playlist_item.cpp
nodist_test_opensubtitles_SOURCES = \
	tests/test_opensubtitles.moc \
	dialogs/subtitles/add_subtitles_model.moc.cpp \
	dialogs/subtitles/opensubtitles.moc.cpp \
	dialogs/subtitles/opensubtitles_models.moc.cpp \
	dialogs/subtitles/subtitle_matching.moc.cpp \
	player/input_models.moc.cpp \
	util/base_model.moc.cpp \
	util/vlctick.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlplaylistlistmodel.moc.cpp \
	medialibrary/mlplaylistmanager.moc.cpp \
	medialibrary/mlplaylistmodel.moc.cpp \
	medialibrary/mlrecenthidelist.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp
BUILT_SOURCES += tests/test_opensubtitles.moc
CLEANFILES += tests/test_opensubtitles.moc
test_opensubtitles_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_opensubtitles_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_opensubtitles_LDADD = $(QT_QTEST_COMMON_ldadd)
test_opensubtitles_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_opensubtitles
TESTS += test_opensubtitles

# test_subtitle_folder_matches
test_subtitle_folder_matches_SOURCES = \
	tests/test_subtitle_folder_matches.cpp \
	tests/vlc_stub_modules.cpp \
	tests/vlc_stub_modules.hpp \
	dialogs/subtitles/http_request.cpp \
	dialogs/subtitles/http_request.hpp \
	dialogs/subtitles/subtitle_match_model.cpp \
	dialogs/subtitles/subtitle_match_model.hpp \
	dialogs/subtitles/subtitle_matching.cpp \
	dialogs/subtitles/subtitle_matching.hpp \
	util/base_model.cpp \
	util/base_model.hpp \
	util/covergenerator.cpp \
	util/vlctick.cpp \
	medialibrary/medialib.cpp \
	medialibrary/mlbasemodel.cpp \
	medialibrary/mlcustomcover.cpp \
	medialibrary/mlplaycountrank.cpp \
	medialibrary/mlhelper.cpp \
	medialibrary/mlplaylist.cpp \
	medialibrary/mlplaylistlistmodel.cpp \
	medialibrary/mlplaylistmanager.cpp \
	medialibrary/mlplaylistmedia.cpp \
	medialibrary/mlplaylistmodel.cpp \
	medialibrary/mlrecenthidelist.cpp \
	medialibrary/mlthreadpool.cpp \
	medialibrary/mlvideo.cpp \
	medialibrary/thumbnailcollector.cpp \
	playlist/playlist_controller.cpp \
	playlist/playlist_item.cpp
nodist_test_subtitle_folder_matches_SOURCES = \
	tests/test_subtitle_folder_matches.moc \
	dialogs/subtitles/subtitle_match_model.moc.cpp \
	dialogs/subtitles/subtitle_matching.moc.cpp \
	util/base_model.moc.cpp \
	util/vlctick.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlplaylistlistmodel.moc.cpp \
	medialibrary/mlplaylistmanager.moc.cpp \
	medialibrary/mlplaylistmodel.moc.cpp \
	medialibrary/mlrecenthidelist.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp
BUILT_SOURCES += tests/test_subtitle_folder_matches.moc
CLEANFILES += tests/test_subtitle_folder_matches.moc
test_subtitle_folder_matches_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_subtitle_folder_matches_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_subtitle_folder_matches_LDADD = $(QT_QTEST_COMMON_ldadd)
test_subtitle_folder_matches_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_subtitle_folder_matches
TESTS += test_subtitle_folder_matches

# test_conversion_profile_model

test_conversion_profile_model_SOURCES = \
	tests/test_conversion_profile_model.cpp \
	dialogs/convert/conversion_profile_model.hpp \
	dialogs/convert/conversion_profile_model.cpp \
	util/soutchain.hpp util/soutchain.cpp

nodist_test_conversion_profile_model_SOURCES = \
	tests/test_conversion_profile_model.moc \
	dialogs/convert/conversion_profile_model.moc.cpp

BUILT_SOURCES += tests/test_conversion_profile_model.moc
CLEANFILES += tests/test_conversion_profile_model.moc
test_conversion_profile_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_conversion_profile_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_conversion_profile_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_conversion_profile_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_conversion_profile_model
TESTS += test_conversion_profile_model

# test_conversion_manager

test_conversion_manager_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_conversion_manager.cpp \
	dialogs/convert/conversion_manager.hpp \
	dialogs/convert/conversion_manager.cpp \
	dialogs/convert/conversion_profile_model.hpp \
	dialogs/convert/conversion_profile_model.cpp \
	util/soutchain.hpp util/soutchain.cpp

nodist_test_conversion_manager_SOURCES = \
	tests/test_conversion_manager.moc \
	dialogs/convert/conversion_manager.moc.cpp \
	dialogs/convert/conversion_profile_model.moc.cpp

BUILT_SOURCES += tests/test_conversion_manager.moc
CLEANFILES += tests/test_conversion_manager.moc
test_conversion_manager_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_conversion_manager_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_conversion_manager_LDADD = $(QT_QTEST_COMMON_ldadd)
test_conversion_manager_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_conversion_manager
TESTS += test_conversion_manager

# test_stream_output

test_stream_output_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_stream_output.cpp \
	dialogs/sout/stream_output.hpp \
	dialogs/sout/stream_output.cpp \
	dialogs/convert/conversion_profile_model.hpp \
	dialogs/convert/conversion_profile_model.cpp \
	util/soutchain.hpp util/soutchain.cpp

nodist_test_stream_output_SOURCES = \
	tests/test_stream_output.moc \
	dialogs/sout/stream_output.moc.cpp \
	dialogs/convert/conversion_profile_model.moc.cpp

BUILT_SOURCES += tests/test_stream_output.moc
CLEANFILES += tests/test_stream_output.moc
test_stream_output_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_stream_output_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_stream_output_LDADD = $(QT_QTEST_COMMON_ldadd)
test_stream_output_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_stream_output
TESTS += test_stream_output

# test_thread_runner

test_thread_runner_SOURCES = \
	tests/test_thread_runner.cpp \
    medialibrary/mlthreadpool.cpp medialibrary/mlthreadpool.hpp

nodist_test_thread_runner_SOURCES = \
	tests/test_thread_runner.moc \
    medialibrary/mlthreadpool.moc.cpp

BUILT_SOURCES += tests/test_thread_runner.moc
CLEANFILES += tests/test_thread_runner.moc
test_thread_runner_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_thread_runner_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_thread_runner_LDADD = $(QT_QTEST_COMMON_ldadd)
test_thread_runner_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_thread_runner
TESTS += test_thread_runner

# test_pins_model

test_pins_model_SOURCES = \
	tests/test_pins_model.cpp \
	maininterface/pinsmodel.cpp maininterface/pinsmodel.hpp

nodist_test_pins_model_SOURCES = \
	tests/test_pins_model.moc \
	maininterface/pinsmodel.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp

BUILT_SOURCES += tests/test_pins_model.moc
CLEANFILES += tests/test_pins_model.moc
test_pins_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_pins_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_pins_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_pins_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_pins_model
TESTS += test_pins_model

# test_navigation_history

test_navigation_history_SOURCES = \
	tests/test_navigation_history.cpp \
	util/navigation_history.cpp util/navigation_history.hpp

nodist_test_navigation_history_SOURCES = \
	tests/test_navigation_history.moc \
	util/navigation_history.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp

BUILT_SOURCES += tests/test_navigation_history.moc
CLEANFILES += tests/test_navigation_history.moc
test_navigation_history_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_navigation_history_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_navigation_history_LDADD = $(QT_QTEST_COMMON_ldadd)
test_navigation_history_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_navigation_history
TESTS += test_navigation_history

# test_presets

test_presets_SOURCES = \
	tests/test_presets.cpp \
	player/presets_model.cpp player/presets_model.hpp \
	player/preset_controller.cpp player/preset_controller.hpp

nodist_test_presets_SOURCES = \
	tests/test_presets.moc \
	player/presets_model.moc.cpp \
	player/preset_controller.moc.cpp

BUILT_SOURCES += tests/test_presets.moc
CLEANFILES += tests/test_presets.moc
test_presets_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_presets_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_presets_LDADD = $(QT_QTEST_COMMON_ldadd)
test_presets_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_presets
TESTS += test_presets

# test_videoblur

test_videoblur_SOURCES = \
	tests/test_videoblur.cpp \
	maininterface/videoblur.cpp maininterface/videoblur.hpp

nodist_test_videoblur_SOURCES = \
	tests/test_videoblur.moc \
	maininterface/videoblur.moc.cpp

BUILT_SOURCES += tests/test_videoblur.moc
CLEANFILES += tests/test_videoblur.moc
test_videoblur_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_videoblur_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_videoblur_LDADD = $(QT_QTEST_COMMON_ldadd)
test_videoblur_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_videoblur
TESTS += test_videoblur

# test_compositor_accessibility

test_compositor_accessibility_SOURCES = \
	tests/test_compositor_accessibility.cpp \
	maininterface/compositor_accessibility.cpp maininterface/compositor_accessibility.hpp

nodist_test_compositor_accessibility_SOURCES = \
	tests/test_compositor_accessibility.moc

BUILT_SOURCES += tests/test_compositor_accessibility.moc
CLEANFILES += tests/test_compositor_accessibility.moc
test_compositor_accessibility_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_compositor_accessibility_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_compositor_accessibility_LDADD = $(QT_QTEST_COMMON_ldadd)
test_compositor_accessibility_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_compositor_accessibility
TESTS += test_compositor_accessibility

# test_subtitle_timeline

test_subtitle_timeline_SOURCES = \
	tests/test_subtitle_timeline.cpp \
	player/subtitle_timeline.cpp player/subtitle_timeline.hpp

nodist_test_subtitle_timeline_SOURCES = \
	tests/test_subtitle_timeline.moc \
	player/subtitle_timeline.moc.cpp

BUILT_SOURCES += tests/test_subtitle_timeline.moc
CLEANFILES += tests/test_subtitle_timeline.moc
test_subtitle_timeline_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_subtitle_timeline_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_subtitle_timeline_LDADD = $(QT_QTEST_COMMON_ldadd)
test_subtitle_timeline_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_subtitle_timeline
TESTS += test_subtitle_timeline

# test_window_chrome

test_window_chrome_SOURCES = \
	tests/test_window_chrome.cpp \
	maininterface/window_chrome.cpp maininterface/window_chrome.hpp

nodist_test_window_chrome_SOURCES = \
	tests/test_window_chrome.moc

BUILT_SOURCES += tests/test_window_chrome.moc
CLEANFILES += tests/test_window_chrome.moc
test_window_chrome_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_window_chrome_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_window_chrome_LDADD = $(QT_QTEST_COMMON_ldadd)
test_window_chrome_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_window_chrome
TESTS += test_window_chrome

# test_theme

test_theme_SOURCES = \
	tests/test_theme.cpp \
	style/themetokens.cpp style/themetokens.hpp \
	util/color_scheme_model.cpp util/color_scheme_model.hpp

nodist_test_theme_SOURCES = \
	tests/test_theme.moc \
	util/color_scheme_model.moc.cpp

BUILT_SOURCES += tests/test_theme.moc
CLEANFILES += tests/test_theme.moc
test_theme_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_theme_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_theme_LDADD = $(QT_QTEST_COMMON_ldadd)
test_theme_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_theme
TESTS += test_theme

# test_player_bars

test_player_bars_SOURCES = \
	tests/test_player_bars.cpp \
	dialogs/toolbar/controlbar_profile.cpp dialogs/toolbar/controlbar_profile.hpp \
	dialogs/toolbar/controlbar_profile_model.cpp dialogs/toolbar/controlbar_profile_model.hpp \
	player/player_controlbar_model.cpp player/player_controlbar_model.hpp \
	player/control_list_model.cpp player/control_list_model.hpp \
	util/vlchotkeyconverter.cpp util/vlchotkeyconverter.hpp

nodist_test_player_bars_SOURCES = \
	tests/test_player_bars.moc \
	dialogs/toolbar/controlbar_profile.moc.cpp \
	dialogs/toolbar/controlbar_profile_model.moc.cpp \
	player/player_controlbar_model.moc.cpp \
	player/control_list_model.moc.cpp \
	util/vlchotkeyconverter.moc.cpp

BUILT_SOURCES += tests/test_player_bars.moc
CLEANFILES += tests/test_player_bars.moc
test_player_bars_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_player_bars_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_player_bars_LDADD = $(QT_QTEST_COMMON_ldadd)
test_player_bars_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_player_bars
TESTS += test_player_bars

# test_layouts

test_layouts_SOURCES = \
	tests/test_layouts.cpp \
	dialogs/toolbar/controlbar_profile.cpp dialogs/toolbar/controlbar_profile.hpp \
	dialogs/toolbar/controlbar_profile_model.cpp dialogs/toolbar/controlbar_profile_model.hpp \
	player/player_controlbar_model.cpp player/player_controlbar_model.hpp \
	player/control_list_model.cpp player/control_list_model.hpp

nodist_test_layouts_SOURCES = \
	tests/test_layouts.moc \
	dialogs/toolbar/controlbar_profile.moc.cpp \
	dialogs/toolbar/controlbar_profile_model.moc.cpp \
	player/player_controlbar_model.moc.cpp \
	player/control_list_model.moc.cpp

BUILT_SOURCES += tests/test_layouts.moc
CLEANFILES += tests/test_layouts.moc
test_layouts_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_layouts_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_layouts_LDADD = $(QT_QTEST_COMMON_ldadd)
test_layouts_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_layouts
TESTS += test_layouts

# test_waveform

test_waveform_SOURCES = \
	tests/test_waveform.cpp \
	player/waveform.cpp player/waveform.hpp

nodist_test_waveform_SOURCES = \
	tests/test_waveform.moc \
	player/waveform.moc.cpp

BUILT_SOURCES += tests/test_waveform.moc
CLEANFILES += tests/test_waveform.moc
test_waveform_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_waveform_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_waveform_LDADD = $(QT_QTEST_COMMON_ldadd)
test_waveform_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_waveform
TESTS += test_waveform

# test_seek_thumbnailer

test_seek_thumbnailer_SOURCES = \
	tests/test_seek_thumbnailer.cpp \
	player/seek_thumbnailer.cpp player/seek_thumbnailer.hpp \
	util/vlctick.cpp util/vlctick.hpp

nodist_test_seek_thumbnailer_SOURCES = \
	tests/test_seek_thumbnailer.moc \
	player/seek_thumbnailer.moc.cpp \
	util/vlctick.moc.cpp

BUILT_SOURCES += tests/test_seek_thumbnailer.moc
CLEANFILES += tests/test_seek_thumbnailer.moc
test_seek_thumbnailer_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_seek_thumbnailer_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_seek_thumbnailer_LDADD = $(QT_QTEST_COMMON_ldadd)
test_seek_thumbnailer_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_seek_thumbnailer
TESTS += test_seek_thumbnailer

# test_playback_tab

test_playback_tab_SOURCES = \
	tests/test_playback_tab.cpp \
	player/sleep_timer.cpp player/sleep_timer.hpp \
	player/playback_statistics.cpp player/playback_statistics.hpp

nodist_test_playback_tab_SOURCES = \
	tests/test_playback_tab.moc \
	player/sleep_timer.moc.cpp \
	player/playback_statistics.moc.cpp

BUILT_SOURCES += tests/test_playback_tab.moc
CLEANFILES += tests/test_playback_tab.moc
test_playback_tab_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_playback_tab_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_playback_tab_LDADD = $(QT_QTEST_COMMON_ldadd)
test_playback_tab_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_playback_tab
TESTS += test_playback_tab

# test_subtitle_appearance

test_subtitle_appearance_SOURCES = \
	tests/test_subtitle_appearance.cpp \
	player/subtitle_appearance.cpp player/subtitle_appearance.hpp

nodist_test_subtitle_appearance_SOURCES = \
	tests/test_subtitle_appearance.moc \
	player/subtitle_appearance.moc.cpp

BUILT_SOURCES += tests/test_subtitle_appearance.moc
CLEANFILES += tests/test_subtitle_appearance.moc
test_subtitle_appearance_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_subtitle_appearance_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_subtitle_appearance_LDADD = $(QT_QTEST_COMMON_ldadd)
test_subtitle_appearance_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_subtitle_appearance
TESTS += test_subtitle_appearance

# test_video_tab

test_video_tab_SOURCES = \
	tests/test_video_tab.cpp \
	player/video_adjustments.cpp player/video_adjustments.hpp

nodist_test_video_tab_SOURCES = \
	tests/test_video_tab.moc \
	player/video_adjustments.moc.cpp

BUILT_SOURCES += tests/test_video_tab.moc
CLEANFILES += tests/test_video_tab.moc
test_video_tab_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_video_tab_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_video_tab_LDADD = $(QT_QTEST_COMMON_ldadd)
test_video_tab_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_video_tab
TESTS += test_video_tab

# test_audio_tab

test_audio_tab_SOURCES = \
	tests/test_audio_tab.cpp \
	player/audio_adjustments.cpp player/audio_adjustments.hpp

nodist_test_audio_tab_SOURCES = \
	tests/test_audio_tab.moc \
	player/audio_adjustments.moc.cpp

BUILT_SOURCES += tests/test_audio_tab.moc
CLEANFILES += tests/test_audio_tab.moc
test_audio_tab_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_audio_tab_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_audio_tab_LDADD = $(QT_QTEST_COMMON_ldadd)
test_audio_tab_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_audio_tab
TESTS += test_audio_tab

# test_shortcut_message

test_shortcut_message_SOURCES = \
	tests/test_shortcut_message.cpp \
	player/shortcut_message.cpp player/shortcut_message.hpp \
	util/vlchotkeyconverter.cpp util/vlchotkeyconverter.hpp

nodist_test_shortcut_message_SOURCES = \
	tests/test_shortcut_message.moc \
	player/shortcut_message.moc.cpp \
	util/vlchotkeyconverter.moc.cpp

BUILT_SOURCES += tests/test_shortcut_message.moc
CLEANFILES += tests/test_shortcut_message.moc
test_shortcut_message_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_shortcut_message_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_shortcut_message_LDADD = $(QT_QTEST_COMMON_ldadd)
test_shortcut_message_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_shortcut_message
TESTS += test_shortcut_message

# test_keys

test_keys_SOURCES = \
	tests/test_keys.cpp \
	util/vlchotkeyconverter.cpp util/vlchotkeyconverter.hpp \
	util/context_menu_key_filter.cpp util/context_menu_key_filter.hpp

nodist_test_keys_SOURCES = \
	tests/test_keys.moc \
	util/vlchotkeyconverter.moc.cpp \
	util/context_menu_key_filter.moc.cpp

BUILT_SOURCES += tests/test_keys.moc
CLEANFILES += tests/test_keys.moc
test_keys_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_keys_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_keys_LDADD = $(QT_QTEST_COMMON_ldadd)
test_keys_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_keys
TESTS += test_keys

# test_shortcuts_model

test_shortcuts_model_SOURCES = \
	tests/test_shortcuts_model.cpp \
	dialogs/shortcuts/shortcuts_model.cpp dialogs/shortcuts/shortcuts_model.hpp \
	util/vlchotkeyconverter.cpp util/vlchotkeyconverter.hpp

nodist_test_shortcuts_model_SOURCES = \
	tests/test_shortcuts_model.moc \
	dialogs/shortcuts/shortcuts_model.moc.cpp \
	util/vlchotkeyconverter.moc.cpp

BUILT_SOURCES += tests/test_shortcuts_model.moc
CLEANFILES += tests/test_shortcuts_model.moc
test_shortcuts_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_shortcuts_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_shortcuts_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_shortcuts_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_shortcuts_model
TESTS += test_shortcuts_model

# test_onboarding_model

test_onboarding_model_SOURCES = \
	tests/test_onboarding_model.cpp \
	dialogs/onboarding/onboarding_model.cpp dialogs/onboarding/onboarding_model.hpp \
	util/vlchotkeyconverter.cpp util/vlchotkeyconverter.hpp

nodist_test_onboarding_model_SOURCES = \
	tests/test_onboarding_model.moc \
	dialogs/onboarding/onboarding_model.moc.cpp \
	util/vlchotkeyconverter.moc.cpp

BUILT_SOURCES += tests/test_onboarding_model.moc
CLEANFILES += tests/test_onboarding_model.moc
test_onboarding_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_onboarding_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_onboarding_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_onboarding_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_onboarding_model
TESTS += test_onboarding_model

# test_vlc_dialog_model

test_vlc_dialog_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_vlc_dialog_model.cpp \
	dialogs/dialogs/dialogmodel.cpp

nodist_test_vlc_dialog_model_SOURCES = \
	tests/test_vlc_dialog_model.moc \
	dialogs/dialogs/dialogmodel.moc.cpp

BUILT_SOURCES += tests/test_vlc_dialog_model.moc
CLEANFILES += tests/test_vlc_dialog_model.moc
test_vlc_dialog_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_vlc_dialog_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_vlc_dialog_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_vlc_dialog_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_vlc_dialog_model
TESTS += test_vlc_dialog_model

# test_open_capture_model

test_open_capture_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_open_capture_model.cpp \
	dialogs/capture/open_capture_model.cpp dialogs/capture/open_capture_model.hpp \
	dialogs/capture/capture_preview.cpp dialogs/capture/capture_preview.hpp \
	util/module_options_model.cpp util/module_options_model.hpp

nodist_test_open_capture_model_SOURCES = \
	tests/test_open_capture_model.moc \
	dialogs/capture/open_capture_model.moc.cpp \
	dialogs/capture/capture_preview.moc.cpp \
	util/module_options_model.moc.cpp

BUILT_SOURCES += tests/test_open_capture_model.moc
CLEANFILES += tests/test_open_capture_model.moc
test_open_capture_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_open_capture_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_open_capture_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_open_capture_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_open_capture_model
TESTS += test_open_capture_model

# test_messages_model

test_messages_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_messages_model.cpp \
	dialogs/messages/messagesmodel.cpp \
	dialogs/dialogs/dialogmodel.cpp

nodist_test_messages_model_SOURCES = \
	tests/test_messages_model.moc \
	dialogs/messages/messagesmodel.moc.cpp \
	dialogs/dialogs/dialogmodel.moc.cpp

BUILT_SOURCES += tests/test_messages_model.moc
CLEANFILES += tests/test_messages_model.moc
test_messages_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_messages_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_messages_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_messages_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_messages_model
TESTS += test_messages_model

# test_program_guide_model

test_program_guide_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_program_guide_model.cpp \
	dialogs/epg/programguidemodel.cpp

nodist_test_program_guide_model_SOURCES = \
	tests/test_program_guide_model.moc \
	dialogs/epg/programguidemodel.moc.cpp

BUILT_SOURCES += tests/test_program_guide_model.moc
CLEANFILES += tests/test_program_guide_model.moc
test_program_guide_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_program_guide_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_program_guide_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_program_guide_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_program_guide_model
TESTS += test_program_guide_model

# test_settings_model

test_settings_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_settings_model.cpp \
	dialogs/settings/settings_model.cpp dialogs/settings/settings_model.hpp \
	dialogs/settings/settings_registry.cpp dialogs/settings/settings_registry.hpp \
	dialogs/settings/settings_basic_rows.cpp dialogs/settings/settings_basic_rows.hpp

nodist_test_settings_model_SOURCES = \
	tests/test_settings_model.moc \
	dialogs/settings/settings_model.moc.cpp

BUILT_SOURCES += tests/test_settings_model.moc
CLEANFILES += tests/test_settings_model.moc
test_settings_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_settings_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_settings_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_settings_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_settings_model
TESTS += test_settings_model

# test_profiles_model

test_profiles_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_profiles_model.cpp \
	dialogs/settings/profiles_model.cpp dialogs/settings/profiles_model.hpp \
	dialogs/settings/settings_registry.cpp dialogs/settings/settings_registry.hpp

nodist_test_profiles_model_SOURCES = \
	tests/test_profiles_model.moc \
	dialogs/settings/profiles_model.moc.cpp

BUILT_SOURCES += tests/test_profiles_model.moc
CLEANFILES += tests/test_profiles_model.moc
test_profiles_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_profiles_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_profiles_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_profiles_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_profiles_model
TESTS += test_profiles_model

# test_plugins_list_model

test_plugins_list_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_plugins_list_model.cpp \
	dialogs/settings/plugins_list_model.cpp dialogs/settings/plugins_list_model.hpp

nodist_test_plugins_list_model_SOURCES = \
	tests/test_plugins_list_model.moc \
	dialogs/settings/plugins_list_model.moc.cpp

BUILT_SOURCES += tests/test_plugins_list_model.moc
CLEANFILES += tests/test_plugins_list_model.moc
test_plugins_list_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_plugins_list_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_plugins_list_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_plugins_list_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_plugins_list_model
TESTS += test_plugins_list_model

# test_media_properties_model

test_media_properties_model_SOURCES = \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	tests/test_media_properties_model.cpp \
	dialogs/mediainfo/media_properties_model.cpp \
	dialogs/fingerprint/chromaprint.cpp

nodist_test_media_properties_model_SOURCES = \
	tests/test_media_properties_model.moc \
	dialogs/mediainfo/media_properties_model.moc.cpp \
	dialogs/fingerprint/chromaprint.moc.cpp

BUILT_SOURCES += tests/test_media_properties_model.moc
CLEANFILES += tests/test_media_properties_model.moc
test_media_properties_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_media_properties_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_media_properties_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_media_properties_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_media_properties_model
TESTS += test_media_properties_model

# test_disc_model

test_disc_model_SOURCES = \
	tests/test_disc_model.cpp \
	dialogs/open/disc_model.hpp dialogs/open/disc_model.cpp \
	playlist/playlist_controller.hpp playlist/playlist_controller.cpp \
	playlist/playlist_item.hpp playlist/playlist_item.cpp \
	util/vlctick.cpp util/vlctick.hpp

nodist_test_disc_model_SOURCES = \
	tests/test_disc_model.moc \
	dialogs/open/disc_model.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp \
	util/vlctick.moc.cpp

BUILT_SOURCES += tests/test_disc_model.moc
CLEANFILES += tests/test_disc_model.moc
test_disc_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_disc_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_disc_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_disc_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_disc_model
TESTS += test_disc_model

# test_saved_streams

test_saved_streams_SOURCES = \
	tests/test_saved_streams.cpp \
	tests/vlc_stub_modules.cpp tests/vlc_stub_modules.hpp \
	network/saved_streams_model.hpp network/saved_streams_model.cpp \
	network/network_playlist_model.hpp network/network_playlist_model.cpp \
	network/preparser_playlist_reader.hpp network/preparser_playlist_reader.cpp \
	network/podcast_feeds.hpp network/podcast_feeds.cpp

nodist_test_saved_streams_SOURCES = \
	tests/test_saved_streams.moc \
	network/saved_streams_model.moc.cpp \
	network/network_playlist_model.moc.cpp \
	network/preparser_playlist_reader.moc.cpp \
	network/podcast_feeds.moc.cpp

BUILT_SOURCES += tests/test_saved_streams.moc
CLEANFILES += tests/test_saved_streams.moc
test_saved_streams_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_saved_streams_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_saved_streams_LDADD = $(QT_QTEST_COMMON_ldadd)
test_saved_streams_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_saved_streams
TESTS += test_saved_streams

# test_vlcicons

test_vlcicons_SOURCES = \
	tests/test_vlcicons.cpp \
	style/vlcicons.cpp style/vlcicons.hpp

nodist_test_vlcicons_SOURCES = \
	tests/test_vlcicons.moc \
	tests/test_vlcicons.cpp.qrc.cpp \
	style/vlcicons.moc.cpp

tests/test_vlcicons.cpp.qrc.cpp: tests/test_vlcicons.qrc pixmaps/VLCIcons.ttf
	$(AM_V_GEN)$(RCC) $(RCC_EXTRA_PARAMS) --name test_vlcicons -o $@ $<

EXTRA_DIST += tests/test_vlcicons.qrc
BUILT_SOURCES += tests/test_vlcicons.moc
CLEANFILES += tests/test_vlcicons.moc tests/test_vlcicons.cpp.qrc.cpp
test_vlcicons_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_vlcicons_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_vlcicons_LDADD = $(QT_QTEST_COMMON_ldadd)
test_vlcicons_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_vlcicons
TESTS += test_vlcicons


test_qt_navigation_model_SOURCES = \
	tests/vlc_stub_modules.cpp \
	tests/test_navigation_model.cpp \
	maininterface/navigationmodel.hpp \
	maininterface/navigationmodel.cpp

nodist_test_qt_navigation_model_SOURCES = \
	tests/test_navigation_model.moc \
	maininterface/navigationmodel.moc.cpp

BUILT_SOURCES += tests/test_navigation_model.moc
CLEANFILES += tests/test_navigation_model.moc
test_qt_navigation_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_qt_navigation_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_qt_navigation_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_qt_navigation_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_qt_navigation_model
TESTS += test_qt_navigation_model

test_qt_pins_model_SOURCES = \
	tests/test_pins_model.cpp \
	maininterface/pinsmodel.hpp \
	maininterface/pinsmodel.cpp \
	medialibrary/mlqmltypes.hpp

nodist_test_qt_pins_model_SOURCES = \
	tests/test_pins_model.moc \
	maininterface/pinsmodel.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp

BUILT_SOURCES += tests/test_pins_model.moc
CLEANFILES += tests/test_pins_model.moc
test_qt_pins_model_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_qt_pins_model_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_qt_pins_model_LDADD = $(QT_QTEST_COMMON_ldadd)
test_qt_pins_model_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_qt_pins_model
TESTS += test_qt_pins_model

test_qt_navigation_history_SOURCES = \
	tests/test_navigation_history.cpp \
	util/navigation_history.hpp \
	util/navigation_history.cpp \
	medialibrary/mlqmltypes.hpp

nodist_test_qt_navigation_history_SOURCES = \
	tests/test_navigation_history.moc \
	util/navigation_history.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp

BUILT_SOURCES += tests/test_navigation_history.moc
CLEANFILES += tests/test_navigation_history.moc
test_qt_navigation_history_CPPFLAGS = $(QT_QTEST_COMMON_cppflags)
test_qt_navigation_history_CXXFLAGS = $(QT_QTEST_COMMON_cxxflags)
test_qt_navigation_history_LDADD = $(QT_QTEST_COMMON_ldadd)
test_qt_navigation_history_LDFLAGS = $(QT_QTEST_COMMON_ldflags)
check_PROGRAMS += test_qt_navigation_history
TESTS += test_qt_navigation_history


endif

QML_LOG_COMPILER = $(builddir)/qml_test -input

if HAVE_QT_QUICK_TEST
qml_test_SOURCES = tests/qml_test.cpp tests/qml_test.hpp
nodist_qml_test_SOURCES = tests/qml_test.moc.cpp
CLEANFILES += $(nodist_qml_test_SOURCES)
qml_test_CPPFLAGS = $(libqt_plugin_la_CPPFLAGS)
qml_test_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC $(CXXFLAGS_qt) ${QT_QUICK_TEST_CFLAGS} -DQUICK_TEST_SOURCE_DIR="\"${srcdir}/tests\"" -I"${builddir}/tests"
qml_test_LDADD = $(QT_INDIRECT_LIBS) ${QT_QUICK_TEST_LIBS} $(QT_LIBS) $(LIBS_qt) libqml_module_util.a
qml_test_LDFLAGS = $(AM_LDFLAGS) $(QT_QUICK_TEST_LDFLAGS)
check_PROGRAMS += qml_test
EXTRA_DIST += tests/tst_FSM.qml tests/tst_JumpToTime.qml
TESTS += tests/tst_FSM.qml tests/tst_JumpToTime.qml
EXTRA_DIST += tests/tst_Helpers.qml
TESTS += tests/tst_Helpers.qml
EXTRA_DIST += tests/tst_TrackMenuFSM.qml
TESTS += tests/tst_TrackMenuFSM.qml
EXTRA_DIST += tests/tst_ClickGesture.qml
TESTS += tests/tst_ClickGesture.qml
EXTRA_DIST += tests/tst_TimedPrompt.qml
TESTS += tests/tst_TimedPrompt.qml
EXTRA_DIST += tests/tst_BarRevealFSM.qml
TESTS += tests/tst_BarRevealFSM.qml
EXTRA_DIST += tests/tst_ControlBarFolding.qml
TESTS += tests/tst_ControlBarFolding.qml
EXTRA_DIST += tests/tst_RightPanelFSM.qml
TESTS += tests/tst_RightPanelFSM.qml
EXTRA_DIST += tests/tst_ChoicePreview.qml
TESTS += tests/tst_ChoicePreview.qml
EXTRA_DIST += tests/tst_LineKeyNavigation.qml
TESTS += tests/tst_LineKeyNavigation.qml
endif
endif
