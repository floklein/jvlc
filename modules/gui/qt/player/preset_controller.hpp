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
#ifndef QVLC_PRESET_CONTROLLER_HPP
#define QVLC_PRESET_CONTROLLER_HPP

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "presets_model.hpp"

/**
 * The adjustments a Preset can hold, by key ("sub-language", "rate",
 * "video-hue"…): the Adjustments sidebar's tabs and the player
 */
class PresetTarget : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// the value of every adjustment, as shown now
    virtual QVariantMap values() const = 0;
    /// the values of the Profile: the options for preferences, the defaults
    /// for the rest
    virtual QVariantMap profileValues() const = 0;
    /// the choices this file remembers (they win over a Preset)
    virtual QVariantMap fileValues() const { return {}; }
    /// shows these values; they never reach the Profile
    virtual void apply(const QVariantMap& values) = 0;
    /// whether the viewer's changes are kept in the Profile (no Preset)
    virtual void setProfileWrites(bool enabled) = 0;

signals:
    /// the viewer changed these adjustments (picking a track gives the
    /// preferred language of its category)
    void changedByUser(const QVariantMap& values);
};

/**
 * The Preset of the playing file: the one attached to its folder or the
 * nearest parent folder (its Show).
 *
 * Values are layered Profile → Preset → this file's remembered choices.
 * While a Preset is active, what the viewer changes goes into it, not into
 * the Profile, and only the values that differ are kept. Leaving the Show
 * puts the Profile values back.
 */
class PresetController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(PresetsModel* model READ model CONSTANT FINAL)
    // the playing file's folder, empty when it is not a local file
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged FINAL)
    Q_PROPERTY(QString folderName READ folderName NOTIFY folderChanged FINAL)
    // the Preset of the playing file, empty for Default
    Q_PROPERTY(QString activeId READ activeId NOTIFY activeChanged FINAL)
    Q_PROPERTY(QString activeName READ activeName NOTIFY activeChanged FINAL)
    // the folder the Preset is attached to: the file's or a parent one
    Q_PROPERTY(QString activeFolder READ activeFolder NOTIFY activeChanged FINAL)
    // the keys whose value comes from the Preset
    Q_PROPERTY(QStringList overriddenKeys READ overriddenKeys NOTIFY overriddenKeysChanged FINAL)

public:
    PresetController(PresetsModel* model, PresetTarget* target, QObject* parent = nullptr);

    /// the keys of the file being played, which the next file starts again
    static bool isPerFile(const QString& key);
    /// the values in effect: the Preset over the Profile, this file's
    /// choices over both
    static QVariantMap layered(const QVariantMap& profile, const QVariantMap& preset,
                               const QVariantMap& file);

    PresetsModel* model() const { return m_model; }
    QString folder() const { return m_folder; }
    QString folderName() const;
    QString activeId() const { return m_activeId; }
    QString activeName() const;
    QString activeFolder() const { return m_activeFolder; }
    QStringList overriddenKeys() const;

    /// the media being played, by MRL; empty when stopped
    void setMedia(const QString& mrl);
    /// the Profile changed: show the Preset over its values again
    void profileSwitched();

    /// attaches a Preset to the playing file's folder, or Default with an
    /// empty id
    Q_INVOKABLE void choose(const QString& id);
    /// a Preset of the adjustments that differ from the Profile, attached to
    /// the playing file's folder; returns its id
    Q_INVOKABLE QString createFromCurrent(const QString& name);
    /// takes these keys out of the active Preset and shows the Profile values
    Q_INVOKABLE void resetToProfile(const QStringList& keys);
    /// one of the keys comes from the Preset
    Q_INVOKABLE bool isOverridden(const QStringList& keys) const;

signals:
    void folderChanged();
    void activeChanged();
    void overriddenKeysChanged();
    /// a Show starts using a Preset: "Using <Preset> for <folder>"
    void attached(const QString& presetName, const QString& folderName);

private:
    void refresh(bool mediaChanged);
    void onChangedByUser(const QVariantMap& values);
    /// shows the values that differ from what is shown; per-file ones too
    /// when a new file starts
    void show(const QVariantMap& values, bool newFile);

    PresetsModel* m_model;
    PresetTarget* m_target;

    QString m_folder;
    QString m_activeId;
    QString m_activeFolder;
    // the keys the active Preset set, to put back when it goes
    QSet<QString> m_shown;
    bool m_applying = false;
};

#endif // QVLC_PRESET_CONTROLLER_HPP
