/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef PLAYERCONTROLBARMODEL_HPP
#define PLAYERCONTROLBARMODEL_HPP

#include <QObject>
#include <QJSValue>
#include <QMap>
#include <QVector>
#include <QVariantMap>

#include <array>

Q_MOC_INCLUDE( "player/control_list_model.hpp" )

class ControlListModel;

// The Layout of one player (video, music or mini player): the left, center
// and right zones of the Control bar and of the Top bar. In the Layouts that
// the user customizes, every item appears once (Spacer and Divider can
// repeat), the VLC menu always stays, and the seek bar sits either inline in
// the Control bar or on its own row above it (when no zone holds it).
class PlayerControlbarModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool dirty READ dirty WRITE setDirty NOTIFY dirtyChanged FINAL)
    Q_PROPERTY(bool empty READ empty NOTIFY emptyChanged FINAL)

    // the Control bar
    Q_PROPERTY(ControlListModel* left READ left CONSTANT FINAL)
    Q_PROPERTY(ControlListModel* center READ center CONSTANT FINAL)
    Q_PROPERTY(ControlListModel* right READ right CONSTANT FINAL)

    // the Top bar
    Q_PROPERTY(ControlListModel* topLeft READ topLeft CONSTANT FINAL)
    Q_PROPERTY(ControlListModel* topCenter READ topCenter CONSTANT FINAL)
    Q_PROPERTY(ControlListModel* topRight READ topRight CONSTANT FINAL)

    Q_PROPERTY(bool seekBarInline READ seekBarInline WRITE setSeekBarInline NOTIFY seekBarInlineChanged FINAL)

public:
    // the zones in their serialization order
    enum Zone {
        Left = 0,
        Center,
        Right,
        TopLeft,
        TopCenter,
        TopRight,
        ZoneCount
    };

    using Zones = std::array<QVector<int>, ZoneCount>;

    // When there is a need to add a new Player, just
    // add its identifier in this enum and set QML buttons layout
    // identifier to it. Such as `property int identifier =
    // PlayerControlbarModel.Videoplayer`.
    // To make it translatable, add a corresponding entry to
    // the static member playerIdentifierDictionary which is
    // initialized in the source file.
    enum PlayerIdentifier {
        Videoplayer = 0,
        Audioplayer
    };
    Q_ENUM(PlayerIdentifier)
    // This enum is iterated through QMetaEnum, and
    // a model out of this enum is generated
    // and used in the configuration editor.
    // Thanks to MOC, adding an entry to this enum
    // is enough for the editor to consider the
    // added entry without any other modification.
    // (except for the translation)

    static const QMap<PlayerIdentifier, const char*> playerIdentifierDictionary;

    static QJSValue getPlaylistIdentifierListModel(class QQmlEngine *engine,
                                                   class QJSEngine *scriptEngine);

    explicit PlayerControlbarModel(QObject *parent = nullptr);
    ~PlayerControlbarModel();

    bool dirty() const;
    bool empty() const;

    Zones serializeModels() const;
    void loadModels(const Zones& zones);

    // Makes the zones follow the rules of a customizable Layout: drops the
    // repeated items and a seek bar in the Top bar, and puts back the VLC
    // menu at the start of the Top bar when it is missing.
    static Zones sanitized(Zones zones);

    ControlListModel* left() const;
    ControlListModel* center() const;
    ControlListModel* right() const;

    ControlListModel* topLeft() const;
    ControlListModel* topCenter() const;
    ControlListModel* topRight() const;

    bool seekBarInline() const;
    // inline: the seek bar goes back to the Control bar center
    // own row: the seek bar leaves the zones
    void setSeekBarInline(bool seekBarInline);

    // Spacer, Expanding spacer and Divider can appear more than once
    Q_INVOKABLE bool isRepeatable(int control) const { return repeatable(control); }
    // the VLC menu can't be removed, so Settings stay reachable
    Q_INVOKABLE bool isRemovable(int control) const { return removable(control); }

    static bool repeatable(int control);
    static bool removable(int control);

    Q_INVOKABLE bool isPlaced(int control) const;
    // whether the palette offers the control
    Q_INVOKABLE bool canAdd(int control) const;
    // whether the control may go in this zone (the seek bar stays in the
    // Control bar)
    Q_INVOKABLE bool canPlace(int control, ControlListModel* zone) const;

    // Edits that keep the rules. They return false and change nothing when a
    // rule forbids them.
    Q_INVOKABLE bool addControl(ControlListModel* zone, int index, int control);
    Q_INVOKABLE bool removeControl(ControlListModel* zone, int index);
    Q_INVOKABLE bool moveControl(ControlListModel* from, int fromIndex,
                                 ControlListModel* to, int toIndex);
    // Moves an item one step left (-1) or right (+1) within its bar,
    // crossing into the next zone at the ends of a zone (Alt+Left/Right).
    // Returns where the item lands, as {zone, index}, or an empty map.
    Q_INVOKABLE QVariantMap shiftControl(ControlListModel* zone, int index, int step);

    Q_INVOKABLE bool isTopBarZone(const ControlListModel* zone) const;

    bool operator==(const PlayerControlbarModel& model) const;
    bool operator!=(const PlayerControlbarModel& model) const {
        return !(operator==(model));
    }

public slots:
    void setDirty(bool dirty);

signals:
    void dirtyChanged(bool dirty);
    void emptyChanged(bool);
    void controlListChanged();
    void seekBarInlineChanged();

private:
    bool m_dirty = false;
    bool m_empty = true;
    bool m_seekBarInline = false;

    ControlListModel* m_left = nullptr;
    ControlListModel* m_center = nullptr;
    ControlListModel* m_right = nullptr;

    ControlListModel* m_topLeft = nullptr;
    ControlListModel* m_topCenter = nullptr;
    ControlListModel* m_topRight = nullptr;

    std::array<ControlListModel*, ZoneCount> zones() const;
    // the zones of the same bar as zone, from left to right
    std::array<ControlListModel*, 3> barOf(const ControlListModel* zone) const;

private slots:
    void contentChanged();
};

#endif
