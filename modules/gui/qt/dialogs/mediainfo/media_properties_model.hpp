/*****************************************************************************
 * media_properties_model.hpp : Properties & Metadata of one media
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
#ifndef VLC_QT_MEDIA_PROPERTIES_MODEL_HPP_
#define VLC_QT_MEDIA_PROPERTIES_MODEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "util/shared_input_item.hpp"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <memory>

class Chromaprint;
struct fingerprint_request_t;

/**
 * Backs the Properties & Metadata window: the editable tags of one media,
 * its codec details, its live statistics (when it follows the player) and
 * the AcoustID "Identify song" lookup.
 *
 * Edits stay in the model until save() writes them to the item and to the
 * file through a meta writer module.
 */
class MediaPropertiesModel : public QObject
{
    Q_OBJECT

public:
    enum Tab
    {
        GeneralTab,
        CodecTab,
        StatisticsTab
    };
    Q_ENUM(Tab)

    enum IdentifyState
    {
        IdentifyIdle,
        IdentifySearching,
        IdentifyFound,
        IdentifyNotFound
    };
    Q_ENUM(IdentifyState)

    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaChanged FINAL)
    Q_PROPERTY(bool live READ isLive CONSTANT FINAL)
    Q_PROPERTY(QString location READ location NOTIFY mediaChanged FINAL)
    Q_PROPERTY(bool canSave READ canSave NOTIFY mediaChanged FINAL)
    Q_PROPERTY(bool canIdentify READ canIdentify NOTIFY mediaChanged FINAL)

    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY metaChanged FINAL)
    Q_PROPERTY(QString artist READ artist WRITE setArtist NOTIFY metaChanged FINAL)
    Q_PROPERTY(QString album READ album WRITE setAlbum NOTIFY metaChanged FINAL)
    Q_PROPERTY(QString genre READ genre WRITE setGenre NOTIFY metaChanged FINAL)
    Q_PROPERTY(QString date READ date WRITE setDate NOTIFY metaChanged FINAL)
    Q_PROPERTY(QString trackNumber READ trackNumber WRITE setTrackNumber NOTIFY metaChanged FINAL)
    Q_PROPERTY(QString trackTotal READ trackTotal WRITE setTrackTotal NOTIFY metaChanged FINAL)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY metaChanged FINAL)
    Q_PROPERTY(QUrl artwork READ artwork WRITE setArtwork NOTIFY metaChanged FINAL)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged FINAL)

    Q_PROPERTY(QVariantList codecInfo READ codecInfo NOTIFY codecInfoChanged FINAL)
    Q_PROPERTY(QVariantList statistics READ statistics NOTIFY statisticsChanged FINAL)
    Q_PROPERTY(QVariantList bitrateHistory READ bitrateHistory NOTIFY statisticsChanged FINAL)

    Q_PROPERTY(IdentifyState identifyState READ identifyState NOTIFY identifyStateChanged FINAL)
    Q_PROPERTY(QVariantList identifyResults READ identifyResults NOTIFY identifyStateChanged FINAL)

    /**
     * \param live the model follows the player's current media and receives
     *             its statistics
     */
    explicit MediaPropertiesModel( qt_intf_t *p_intf, bool live = false,
                                   QObject *parent = nullptr );
    ~MediaPropertiesModel() override;

    void setMedia( const SharedInputItem& item );
    SharedInputItem media() const { return m_item; }

    bool hasMedia() const { return static_cast<bool>( m_item ); }
    bool isLive() const { return m_live; }
    QString location() const { return m_location; }
    bool canSave() const;
    bool canIdentify() const;

    QString title() const { return m_tags.title; }
    QString artist() const { return m_tags.artist; }
    QString album() const { return m_tags.album; }
    QString genre() const { return m_tags.genre; }
    QString date() const { return m_tags.date; }
    QString trackNumber() const { return m_tags.trackNumber; }
    QString trackTotal() const { return m_tags.trackTotal; }
    QString description() const { return m_tags.description; }
    QUrl artwork() const { return m_tags.artwork; }
    bool isDirty() const { return m_dirty; }

    void setTitle( const QString& );
    void setArtist( const QString& );
    void setAlbum( const QString& );
    void setGenre( const QString& );
    void setDate( const QString& );
    void setTrackNumber( const QString& );
    void setTrackTotal( const QString& );
    void setDescription( const QString& );
    void setArtwork( const QUrl& );

    QVariantList codecInfo() const { return m_codecInfo; }
    QVariantList statistics() const { return m_statistics; }
    QVariantList bitrateHistory() const { return m_bitrateHistory; }

    IdentifyState identifyState() const { return m_identifyState; }
    QVariantList identifyResults() const { return m_identifyResults; }

public slots:
    /* Writes the edited tags to the item and to the file */
    bool save();
    /* Drops the edits */
    void revert();

    /* Looks the song up on AcoustID */
    void identify();
    /* Copies the tags of one identify result into the edits */
    void applyIdentity( int index );
    void discardIdentities();

    /* Re-reads the tags from the item, unless they are being edited */
    void refreshMeta();
    void refreshCodecInfo();
    void updateStatistics( const input_stats_t& stats );

signals:
    void mediaChanged();
    void metaChanged();
    void dirtyChanged();
    void codecInfoChanged();
    void statisticsChanged();
    void identifyStateChanged();
    void saved();
    void saveFailed();

private:
    struct Tags
    {
        QString title;
        QString artist;
        QString album;
        QString genre;
        QString date;
        QString trackNumber;
        QString trackTotal;
        QString description;
        QUrl artwork;
    };

    void loadTags();
    void setTag( QString Tags::*tag, const QString& value );
    void setDirty( bool dirty );
    void setIdentifyState( IdentifyState state );
    void onIdentifyResults();
    void resetIdentify();

    qt_intf_t *p_intf;
    const bool m_live;

    SharedInputItem m_item;
    QString m_location;
    Tags m_tags;
    /* The item name shown as title when the media has no title tag */
    QString m_fallbackTitle;
    bool m_dirty = false;

    QVariantList m_codecInfo;
    QVariantList m_statistics;
    QVariantList m_bitrateHistory;
    vlc_tick_t m_lastHistoryTick = VLC_TICK_INVALID;

    std::unique_ptr<Chromaprint> m_chromaprint;
    std::unique_ptr<fingerprint_request_t, void (*)( fingerprint_request_t * )> m_identifyRequest;
    IdentifyState m_identifyState = IdentifyIdle;
    QVariantList m_identifyResults;
};

#endif
