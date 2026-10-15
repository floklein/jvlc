/*****************************************************************************
 * dialogs_provider.hpp : Dialogs provider
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#ifndef QVLC_DIALOGS_PROVIDER_H_
#define QVLC_DIALOGS_PROVIDER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <cassert>

#include "qt.hpp"

#include "widgets/native/qvlcframe.hpp"

#include "playlist/playlist_item.hpp"

#include "util/singleton.hpp"
#include "util/shared_input_item.hpp"

#include "medialibrary/mlqmltypes.hpp"

#include "dialogs/mediainfo/media_properties_model.hpp"

#include <QEvent>
#include <QFileDialog>
#include <QObject>
#include <QStringList>
#include <QMessageBox>
#include <QPointer>
#include <QUrl>

#include <vlc_es.h>

#define TITLE_EXTENSIONS_MEDIA qtr( "Media Files" )
#define TITLE_EXTENSIONS_VIDEO qtr( "Video Files" )
#define TITLE_EXTENSIONS_AUDIO qtr( "Audio Files" )
#define TITLE_EXTENSIONS_IMAGE qtr( "Image Files" )
#define TITLE_EXTENSIONS_PLAYLIST qtr( "Playlist Files" )
#define TITLE_EXTENSIONS_SUBTITLE qtr( "Subtitle Files" )
#define TITLE_EXTENSIONS_ALL qtr( "All Files" )
#define EXTENSIONS_ALL "*"
#define ADD_EXT_FILTER( string, type ) \
    string = string + QString("%1 ( %2 );;") \
            .arg( TITLE_##type ) \
            .arg( QString( type ) );

enum {
    EXT_FILTER_MEDIA     =  0x01,
    EXT_FILTER_VIDEO     =  0x02,
    EXT_FILTER_AUDIO     =  0x04,
    EXT_FILTER_PLAYLIST  =  0x08,
    EXT_FILTER_SUBTITLE  =  0x10,
};

class QEvent;
class QSignalMapper;
class VLCMenuBar;

class PluginDialog;
class AddSubtitlesModel;
class SubtitleMatchModel;
namespace opensubtitles { class Client; }

class DialogsProvider : public QObject, public Singleton<DialogsProvider>
{
    Q_OBJECT
    friend class VLCMenuBar;
    friend class Singleton<DialogsProvider>;

public:
    enum Mode
    {
        Show,
        Hide,
        Toggle
    };
    Q_ENUM(Mode)

    enum StreamAndCastTab
    {
        CAST_TAB,
        STREAM_TAB
    };
    Q_ENUM(StreamAndCastTab)

    Q_PROPERTY(bool pluginDialogVisible READ pluginDialogVisible NOTIFY pluginDialogVisibleChanged FINAL)
    Q_PROPERTY(bool prefsDialogVisible READ prefsDialogVisible NOTIFY prefsDialogVisibleChanged FINAL)

    static DialogsProvider *getInstance()
    {
        const auto instance = Singleton<DialogsProvider>::getInstance<false>();
        assert( instance );
        return instance;
    }
    static DialogsProvider *getInstance( qt_intf_t *p_intf )
    {
        return Singleton<DialogsProvider>::getInstance( p_intf );
    }
    QStringList showSimpleOpen( const QString& help = QString(),
                                int filters = EXT_FILTER_MEDIA |
                                EXT_FILTER_VIDEO | EXT_FILTER_AUDIO |
                                EXT_FILTER_PLAYLIST,
                                const QUrl& path = QUrl() );
    bool isDying() { return b_isDying; }
    static QString getDirectoryDialog( qt_intf_t *p_intf);

    static QString getSaveFileName(QWidget *parent = NULL,
                                    const QString &caption = QString(),
                                    const QUrl &dir = QUrl(),
                                    const QString &filter = QString(),
                                    QString *selectedFilter = NULL );

    Q_INVOKABLE static QVariant getTextDialog(QWidget *parent, const QString& title,
                                              const QString& label,
                                              const QString& placeholder,
                                              bool* ok = nullptr);

    Q_INVOKABLE QUrl getArtworkFileUrl();

    /* the OpenSubtitles session, created on first use */
    opensubtitles::Client *openSubtitles();
    /* gives the matching of a folder or Playlist its From OpenSubtitles */
    void addOpenSubtitlesToMatches( SubtitleMatchModel *matches );

protected:
    void customEvent( QEvent *) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    DialogsProvider( qt_intf_t * );
    virtual ~DialogsProvider() override;

    void loadMediaFile( es_format_category_e category, int filter, const QString& dialogTitle );

    qt_intf_t *p_intf;

    std::unique_ptr<QMenu> popupMenu;
    std::unique_ptr<QMenu> videoPopupMenu;
    std::unique_ptr<QMenu> audioPopupMenu;
    std::unique_ptr<QMenu> miscPopupMenu;

    // the Settings window is a QML window, see Dialogs.qml
    bool m_settingsDialogVisible = false;

    QPointer<MediaPropertiesModel> m_playerMediaProperties;
    /* the model of the Add Subtitles dialog */
    QPointer<AddSubtitlesModel> m_addSubtitlesModel;
    /* the model of Add Subtitles, To This Folder or Playlist */
    QPointer<SubtitleMatchModel> m_subtitleMatchModel;
    /* the OpenSubtitles session, shared by Add Subtitles and Settings */
    QPointer<opensubtitles::Client> m_openSubtitles;
    std::unique_ptr<PluginDialog> m_pluginDialog;
    std::unique_ptr<input_item_parser_id_t,
                    decltype(&input_item_parser_id_Release)> m_parser;

    QWidget* root;
    bool b_isDying;

    void playerMediaProperties( MediaPropertiesModel::Tab tab );

    template<typename T>
    inline void ensureDialog(std::unique_ptr<T>& dialog);
    template<typename T>
    void toggleDialogVisible(std::unique_ptr<T>& dialog);

public slots:
    void mediaInfoDialog( void );
    void mediaInfoDialog( const SharedInputItem& inputItem );
    void mediaInfoDialog( const PlaylistItem& pItem );
    void mediaInfoDialog( const MLItemId& itemId );
    void mediaCodecDialog();
    bool questionDialog(const QString& text, const QString& title = {}) const;
    QMessageBox::StandardButton messageDialog(const QString& text,
                                              const QString& title = {},
                                              const QMessageBox::StandardButtons buttons = QMessageBox::NoButton,
                                              const QMessageBox::StandardButton defaultButton = QMessageBox::NoButton,
                                              const QMessageBox::Icon icon = QMessageBox::NoIcon);
    // shows or hides the Settings window
    void prefsDialog();
    // opens Settings at a page ("general", "interface"...), Advanced
    // Options expanded if asked
    void showSettings(const QString& page = {}, bool advanced = false);
    void setSettingsDialogVisible(bool visible);
    // opens Keyboard Shortcuts, on the action @a action ("play-pause") if
    // given
    void keyboardShortcutsDialog(const QString& action = {});
    /* Onboarding, on a step (0 for the first one) */
    void onboarding(int step = 0);
    /// Opens Messages, showing only the errors if page is 1
    void messagesDialog( int page = 0 );
    /// Asks for a file and saves the visible Messages lines to it
    void saveMessages();
    void sendKey( int key );
    void aboutDialog();
    void gotoTimeDialog();
    void pluginDialog();
    void epgDialog();
    void setPopupMenu();
    void destroyPopupMenu();

    void openFileGenericDialog( intf_dialog_args_t * );

    /* System file picker: play the files by the Play rule, or enqueue them.
     * openDialog() and openFileDialog() do the same. */
    void simpleOpenDialog( bool start = true );

    void openDialog();
    void openDiscDialog();
    void openFileDialog();
    void openNetDialog();
    void openCaptureDialog();

    // Picks a folder and adds it to the media library
    void addLibraryFolderDialog();

    /* System folder picker: play the folder by the Play rule, or enqueue it */
    void PLOpenDir();
    void PLAppendDir();

    void streamingDialog( QWindow *parent, const QStringList& mrls, bool b_stream = true,
                          QStringList options = QStringList("") );
    void streamingDialog( const QList<QUrl>& urls, bool b_stream = true );
    void openAndStreamingDialogs();
    /* Stream & Cast, on a tab (StreamAndCastTab) or on the last one shown */
    void streamAndCastDialog( int tab = -1 );
    /* Stream & Cast, on a file of the library: it streams or casts that file */
    void streamAndCastMediaDialog( const QString& mrl, const QString& name );
    void openAndTranscodingDialogs();
    /* Convert, with these media */
    void convertDialog( const QStringList& mrls );

    void savePlayingToPlaylist();

    /* Add Subtitles, To This File: the media of this MRL, or the playing one */
    void addSubtitlesDialog( const QString& mediaMrl = {} );
    /* Add Subtitles, To This Folder: the folder of this MRL or path */
    void addFolderSubtitlesDialog( const QString& folder );
    /* Add Subtitles, To This Playlist */
    void addPlaylistSubtitlesDialog( const QVariant& playlistId, const QString& name );
    /* Add Audio Track: pick audio files and attach them to the media of this
     * MRL, now if it plays and whenever it plays */
    void addAudioTrackDialog( const QString& mediaMrl );
    void loadSubtitlesFile();
    void loadAudioFile();
    void loadVideoFile();

    void quit();
    // save the settings, quit, then start VLC again with the same arguments
    void restartVLC();

public:
    bool pluginDialogVisible() const;
    bool prefsDialogVisible() const;

signals:
    void releaseMouseEvents();
    void showToolbarEditorDialog();
    // asks the name of a new Profile, copy of the active one
    void showNewProfileDialog();
    void showSettingsDialog(const QString& page, bool advanced);
    void hideSettingsDialog();
    void showKeyboardShortcutsDialog(const QString& action);
    void showOnboarding(int step);
    void showOpenCaptureDialog();
    /* opens Convert with these media, if any */
    void showConvertDialog(const QStringList& mrls);
    /* opens Stream & Cast on a tab (StreamAndCastTab), -1 for the last one */
    void showStreamAndCastDialog(int tab);
    /* opens Stream & Cast on a file, named name */
    void showStreamAndCastMedia(const QString& mrl, const QString& name);
    /* Shows the Properties & Metadata window of a media. The live window,
     * which follows the player, toggles when shown again on the same tab */
    void showMediaProperties( MediaPropertiesModel *model, int tab );
    void showOpenDiscDialog();
    void showOpenNetworkDialog();
    /* opens Add Subtitles over this model; the next one replaces it */
    void showAddSubtitles( AddSubtitlesModel *model );
    /* opens Add Subtitles, To This Folder or Playlist, over this model */
    void showSubtitleMatches( SubtitleMatchModel *model );

    // requests to the QML dialogs
    void showMessages();
    void showAbout();
    void showProgramGuide();
    void showJumpToTime();

    //visiblity signal
    void pluginDialogVisibleChanged();
    void prefsDialogVisibleChanged();
};

class DialogEvent : public QEvent
{
public:
    static const QEvent::Type DialogEvent_Type;
    DialogEvent( int _i_dialog, int _i_arg, intf_dialog_args_t *_p_arg ) :
                 QEvent( DialogEvent_Type )
    {
        i_dialog = _i_dialog;
        i_arg = _i_arg;
        p_arg = _p_arg;
    }

    int i_arg, i_dialog;
    intf_dialog_args_t *p_arg;
};


#endif
