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

#ifndef VLC_QT_PROFILES_MODEL_HPP
#define VLC_QT_PROFILES_MODEL_HPP

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <functional>

#include "qt.hpp"

class QSettings;

/**
 * The Profiles: named bundles of every preference (VLC options, hotkeys
 * included), Theme, Accent color, interface scale and Layouts.
 *
 * The active Profile is the live state: the VLC options and the Qt
 * settings the interface reads. The other Profiles are snapshots kept in
 * the Qt settings (group "Profiles"). Switching stores the live state in
 * the snapshot of the active Profile, then puts the other snapshot in its
 * place through the configuration API, since VLC cannot reload its
 * configuration file.
 *
 * Everything else stays shared: the media library and its history, the
 * Presets, the Subtitle matches, the interface language (kept outside the
 * VLC options) and the other Qt settings.
 *
 * A Default Profile always exists.
 */
class ProfilesModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString activeId READ activeId NOTIFY activeChanged FINAL)
    Q_PROPERTY(QString activeName READ activeName NOTIFY activeChanged FINAL)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ActiveRole,
    };
    Q_ENUM(Roles)

    static const QString DEFAULT_ID;

    ProfilesModel(qt_intf_t* intf, QSettings* settings, QObject* parent = nullptr);

    /* The Qt settings that belong to a Profile: keys, and groups ending
     * with '/' */
    static const QStringList& profileSettings();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString activeId() const { return m_activeId; }
    QString activeName() const;
    bool restartRequired() const { return m_restartRequired; }

    // a name is needed, and two Profiles can't share one
    Q_INVOKABLE bool isNameAvailable(const QString& name) const;

    /* A new Profile, copy of the active one, which it replaces as the
     * active Profile. Returns its id, empty if the name is not available. */
    Q_INVOKABLE QString create(const QString& name);

    // make a Profile the active one; its settings apply at once
    Q_INVOKABLE bool activate(const QString& id);

    // saving the configuration file, replaceable by tests
    void setSaveFunction(std::function<void()> save);
    /* whether a VLC option is only read when VLC starts; by default, the
     * Settings option registry tells */
    void setRestartCheck(std::function<bool(const QString& option)> check);

signals:
    void activeChanged();
    void restartRequiredChanged();
    void countChanged();

    // before a switch: the live state must be in the Qt settings
    void aboutToSwitch();
    // after a switch: the interface must read its settings again
    void switched();

private:
    struct Profile
    {
        QString id;
        QString name;
    };

    // name -> value of the VLC options that are not at their default
    using Options = QHash<QString, QVariant>;

    void load();
    void saveList();
    int indexOf(const QString& id) const;
    QString nameOf(const Profile& profile) const;

    QStringList optionNames() const;
    Options liveOptions() const;
    void storeLiveState(const QString& id);
    void applyState(const QString& id);
    void updateRestartRequired();
    bool needsRestart(const QString& option) const;

    qt_intf_t* m_intf = nullptr;
    QSettings* m_settings = nullptr;
    QVector<Profile> m_profiles;
    QString m_activeId;

    // the VLC options when VLC started
    Options m_startOptions;
    bool m_restartRequired = false;

    std::function<void()> m_save;
    std::function<bool(const QString&)> m_restartCheck;
    mutable bool m_restartOptionsRead = false;
    mutable QSet<QString> m_restartOptions;
    mutable QStringList m_optionNames;
};

#endif // VLC_QT_PROFILES_MODEL_HPP
