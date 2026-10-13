/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
#ifndef VLC_QT_DISC_MODEL_HPP
#define VLC_QT_DISC_MODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <functional>

#include <QAbstractListModel>
#include <QPointer>
#include <QVector>

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

/**
 * The optical drives of the system, with the type of the disc each one holds.
 * Backs the Open Disc dialog, which plays the selected drive.
 */
class DiscModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged FINAL)
    Q_PROPERTY(int defaultIndex READ defaultIndex NOTIFY defaultIndexChanged FINAL)
    Q_PROPERTY(vlc::playlist::PlaylistController* playlistController READ playlistController
               WRITE setPlaylistController NOTIFY playlistControllerChanged FINAL)

public:
    enum Type {
        NO_DISC,  // the drive is empty
        UNKNOWN,  // a disc is in the drive but its type was not recognized
        DVD,
        BLURAY,
        AUDIO_CD,
        VCD,
    };
    Q_ENUM(Type)

    enum Role {
        NAME = Qt::UserRole,
        DEVICE,
        LABEL,
        TYPE,
        TYPE_NAME,
    };
    Q_ENUM(Role)

    struct Drive
    {
        // what the access module opens: "D:\" on Windows, "/dev/sr0" elsewhere
        QString device;
        // where the disc's file system can be read, empty if unknown
        QString root;
        // volume label of the disc, if any
        QString label;
        // set by the drive listing when it can tell without reading the root
        Type type = UNKNOWN;
    };

    using DriveLister = std::function<QVector<Drive>()>;

    explicit DiscModel(QObject *parent = nullptr);

    // replaces the system drive listing, for tests
    void setDriveLister(DriveLister lister);

    static Type detectType(const QString &root);
    static QString typeName(Type type);

    /**
     * The playlist media that plays a disc.
     * title, chapter, audioTrack and subtitleTrack count from 1; 0 keeps the
     * disc's default. For an Audio CD, title is the track to start from.
     */
    static vlc::playlist::Media media(const QString &device, Type type, bool useMenus,
                                      int title, int chapter,
                                      int audioTrack, int subtitleTrack,
                                      const QString &name = {});

    // lists the drives again and detects their discs, off the GUI thread
    Q_INVOKABLE void refresh();

    // plays the disc of the drive at row, following the Play rule
    Q_INVOKABLE bool play(int row, DiscModel::Type type, bool useMenus,
                          int title, int chapter, int audioTrack, int subtitleTrack);

    bool isScanning() const { return m_scanning; }
    // the first drive holding a disc, else the first drive, else -1
    int defaultIndex() const;

    vlc::playlist::PlaylistController *playlistController() const { return m_playlistController; }
    void setPlaylistController(vlc::playlist::PlaylistController *controller);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    static QVector<Drive> systemDrives();

signals:
    void scanningChanged();
    void defaultIndexChanged();
    void playlistControllerChanged();

private:
    void setDrives(QVector<Drive> drives);

    DriveLister m_lister;
    QVector<Drive> m_drives;
    bool m_scanning = false;
    bool m_rescan = false;
    QPointer<vlc::playlist::PlaylistController> m_playlistController;
};

#endif // VLC_QT_DISC_MODEL_HPP
