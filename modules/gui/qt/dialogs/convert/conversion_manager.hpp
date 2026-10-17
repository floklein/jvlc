/*****************************************************************************
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
#ifndef VLC_QT_CONVERSION_MANAGER_HPP
#define VLC_QT_CONVERSION_MANAGER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>

#include <vlc_common.h>
#include <vlc_player.h>

/**
 * The Conversions: files being converted in the background.
 *
 * Conversions run one after the other on a second, hidden player, so that
 * playback and the Queue are untouched. The hidden player is a child of
 * the given object, not of libvlc, so it writes no media library state.
 */
class ConversionManager : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    /* conversions waiting or running */
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged FINAL)
    /* overall progress of the conversions, from 0 to 1 */
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged FINAL)

public:
    enum State {
        Pending,
        Running,
        Done,
        Failed,
        Cancelled,
    };
    Q_ENUM(State)

    enum Roles {
        NAME = Qt::UserRole + 1,
        SOURCE,
        DESTINATION,
        STATE,
        PROGRESS,
    };
    Q_ENUM(Roles)

    explicit ConversionManager(vlc_object_t* parent, QObject* qparent = nullptr);
    ~ConversionManager() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeCount() const;
    double progress() const;

    /**
     * Converts the media with a Conversion profile. A single media is
     * written to saveAs, if given, replacing it; otherwise each output goes
     * next to its original with a "-converted" suffix. A replaced file is
     * replaced only once its conversion is done.
     */
    Q_INVOKABLE void convert(const QStringList& mrls, const QString& profileValue,
                             const QString& saveAs, bool deinterlace);

    /** Queues a conversion of mrl with a stream output chain writing to
     *  destination. An existing destination is replaced only if overwrite.
     *  If output is given, the chain writes it instead, and it becomes the
     *  destination once the conversion is done. */
    void add(const QString& mrl, const QString& soutChain,
             const QString& destination, bool overwrite = false,
             const QString& output = {});

    Q_INVOKABLE void cancel(int row);
    Q_INVOKABLE void cancelAll();
    Q_INVOKABLE void clearFinished();

    /** Where convert() writes mrl when it is not saved as another file */
    Q_INVOKABLE QString convertedPath(const QString& mrl, const QString& extension) const;

    /* System file pickers of the Convert dialog */
    Q_INVOKABLE QStringList chooseMedia() const;
    /* the chosen local file, or an empty string */
    Q_INVOKABLE QString chooseDestination(const QString& path, const QString& extension) const;

signals:
    void countChanged();
    void activeCountChanged();
    void progressChanged();

private:
    struct Conversion {
        QString mrl;
        QString sout;
        QString destination;
        /* the file the chain writes: destination, or a file beside it */
        QString output;
        bool overwrite = false;
        bool existed = false;
        State state = Pending;
        double progress = 0.;
    };

    bool ensurePlayer();
    void startNext();
    void finishRunning();
    /* removes what a conversion that did not end wrote */
    static void removeOutput(const Conversion& conversion);
    /* a free file beside destination, to write it until it is done */
    QString partPath(const QString& destination) const;
    void setState(int row, State state);
    void setRunningProgress(double progress);
    int runningRow() const;

    static void onStateChanged(vlc_player_t*, enum vlc_player_state, void*);
    static void onErrorChanged(vlc_player_t*, enum vlc_player_error, void*);
    static void onPositionChanged(vlc_player_t*, vlc_tick_t, double, void*);

    vlc_object_t* m_parent = nullptr;
    vlc_object_t* m_object = nullptr;
    vlc_player_t* m_player = nullptr;
    vlc_player_listener_id* m_listener = nullptr;

    QVector<Conversion> m_conversions;
    bool m_cancelRequested = false;
    bool m_error = false;
    /* only accessed from the player callbacks */
    double m_postedProgress = 0.;
};

#endif // VLC_QT_CONVERSION_MANAGER_HPP
