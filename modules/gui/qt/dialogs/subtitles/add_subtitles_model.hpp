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
#ifndef ADD_SUBTITLES_MODEL_HPP
#define ADD_SUBTITLES_MODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QUrl>
#include <QVector>

class MediaLib;

/**
 * Add Subtitles, To This File, From Computer: the subtitle files next to the
 * video and in its "Subs" folder, plus the files picked with Browse…, to
 * check. Save attaches the checked ones to the file as Subtitle matches, and
 * to the player when the file is playing.
 */
class AddSubtitlesModel : public QAbstractListModel
{
    Q_OBJECT

    // the MRL of the file the subtitles are added to
    Q_PROPERTY(QString media READ media CONSTANT FINAL)
    // the folder of the file, as " / " separated names
    Q_PROPERTY(QString folder READ folder CONSTANT FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedCountChanged FINAL)

public:
    enum Roles {
        NameRole = Qt::UserRole,
        UrlRole,
        CheckedRole,
    };
    Q_ENUM(Roles)

    // ml may be null: the subtitles are then only attached to the player
    AddSubtitlesModel(qt_intf_t *intf, MediaLib *ml, const QString &mediaMrl,
                      QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString media() const { return m_media; }
    QString folder() const { return m_folder; }
    int count() const { return m_rows.size(); }
    int checkedCount() const;

    Q_INVOKABLE void toggle(int row);
    // Browse…: list the files and check them
    Q_INVOKABLE void addFiles(const QList<QUrl> &files);
    // pick files with the system picker, then addFiles()
    Q_INVOKABLE void browse();
    // attach the checked files and remember them
    Q_INVOKABLE void save();

signals:
    void countChanged();
    void checkedCountChanged();

private:
    struct Row {
        QString name;
        QString mrl;
        bool checked = false;
    };

    void listFolder(const QString &path);

    qt_intf_t *m_intf;
    MediaLib *m_ml;
    QString m_media;
    QString m_folder;
    // the folder of the file on disk, empty if it isn't a local file
    QString m_folderPath;
    QVector<Row> m_rows;
};

#endif // ADD_SUBTITLES_MODEL_HPP
