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

#ifndef PINSMODEL_HPP
#define PINSMODEL_HPP

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class QSettings;

/**
 * The Pins of the library sidebar: shortcuts to folders, Playlists and
 * streams, in the order the user gives them.
 *
 * Pins are kept in the Qt settings (array "Pins"), apart from the media
 * library Favorites. A pin is identified by its type and its key: the media
 * library id of a folder or a Playlist, the id of a Saved stream.
 */
class PinsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum PinType
    {
        FOLDER,
        PLAYLIST,
        STREAM
    };
    Q_ENUM(PinType)

    enum Roles
    {
        TYPE = Qt::UserRole + 1,
        KEY,
        LABEL,
        ML_ID // the MLItemId of a folder or a Playlist, whose key is MLItemId::toString()
    };
    Q_ENUM(Roles)

    explicit PinsModel(QSettings *settings, QObject *parent = nullptr);

    // append a pin; false if it is already pinned
    Q_INVOKABLE bool pin(PinType type, const QString &key, const QString &label);
    Q_INVOKABLE bool unpin(PinType type, const QString &key);
    Q_INVOKABLE bool isPinned(PinType type, const QString &key) const;

    // the row of a pin, -1 if not pinned
    Q_INVOKABLE int indexOf(PinType type, const QString &key) const;

    // move the pin at `from` to the row `to`
    Q_INVOKABLE bool move(int from, int to);
    Q_INVOKABLE bool remove(int row);

    // the label shown for a pin, after its item was renamed
    Q_INVOKABLE bool rename(PinType type, const QString &key, const QString &label);

    // "+ Add item": the window hosts the picker
    Q_INVOKABLE void requestPicker() { emit pickerRequested(); }

    int count() const { return m_pins.size(); }

public: // QAbstractItemModel
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();
    void pickerRequested();

private:
    struct Pin
    {
        PinType type;
        QString key;
        QString label;
    };

    void load();
    void save();

    QSettings *m_settings = nullptr;
    QVector<Pin> m_pins;
};

#endif // PINSMODEL_HPP
