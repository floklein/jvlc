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
#ifndef QVLC_PRESETS_MODEL_HPP
#define QVLC_PRESETS_MODEL_HPP

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

class QSettings;

/**
 * The Presets and the Shows (folders) they are attached to.
 *
 * A Preset is a named, sparse map of adjustments (option-like keys such as
 * "sub-language" or "rate"). A link attaches a Preset to a folder, identified
 * by its path; it also applies to the subfolders, unless a nearer folder has
 * a link of its own. A link without a Preset means "Default" (Profile values
 * only) and stops the Presets of the parent folders.
 *
 * Presets and links live in the Qt settings (group "Presets"), outside the
 * Profiles, so every Profile shares them.
 */
class PresetsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    // a deleted Preset can be brought back with undoRemove()
    Q_PROPERTY(bool canUndoRemove READ canUndoRemove NOTIFY canUndoRemoveChanged FINAL)
    Q_PROPERTY(QString removedName READ removedName NOTIFY canUndoRemoveChanged FINAL)

public:
    enum Roles
    {
        ID = Qt::UserRole + 1,
        NAME,
        // the folders the Preset is attached to
        FOLDERS
    };
    Q_ENUM(Roles)

    // the Preset that applies to a folder, and the folder holding its link
    struct Resolution
    {
        // empty for Default
        QString id;
        // empty when no folder up to the root has a link
        QString folder;
    };

    explicit PresetsModel(QSettings* settings, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_presets.size(); }
    bool canUndoRemove() const { return m_removed.has_value(); }
    QString removedName() const;

    /// a new Preset with these values, at the end; returns its id
    Q_INVOKABLE QString create(const QString& name, const QVariantMap& values = {});
    Q_INVOKABLE bool rename(const QString& id, const QString& name);
    /// a copy of the values, not of the links, named "<name> Copy"
    Q_INVOKABLE QString duplicate(const QString& id);
    /// deletes a Preset and detaches it everywhere, until undoRemove()
    Q_INVOKABLE bool remove(const QString& id);
    Q_INVOKABLE void undoRemove();
    /// forgets the deleted Preset for good
    Q_INVOKABLE void dropUndo();

    /// no other Preset than `except` has this name (case-insensitive)
    Q_INVOKABLE bool isNameAvailable(const QString& name, const QString& except = {}) const;
    Q_INVOKABLE QString name(const QString& id) const;
    bool contains(const QString& id) const;

    QVariantMap values(const QString& id) const;
    void setValue(const QString& id, const QString& key, const QVariant& value);
    void clearValue(const QString& id, const QString& key);

    /// attaches a Preset to a folder, or Default with an empty id
    Q_INVOKABLE void attach(const QString& folder, const QString& id);
    /// removes the folder's own link: it follows its parent folders again
    Q_INVOKABLE void detach(const QString& folder);
    /// the folder's own link: nullopt without one, an empty id for Default
    std::optional<QString> link(const QString& folder) const;
    /// the nearest link from the folder up to the root
    Resolution resolve(const QString& folder) const;
    /// the id of the Preset the files of a folder use, empty for Default
    Q_INVOKABLE QString presetOf(const QString& folder) const { return resolve(folder).id; }
    Q_INVOKABLE QStringList folders(const QString& id) const;

    /// a path as links store it: "/" separators, no trailing one
    static QString normalizeFolder(const QString& path);

signals:
    void countChanged();
    void canUndoRemoveChanged();
    void valuesChanged(const QString& id);
    void linksChanged();

private:
    struct Preset
    {
        QString id;
        QString name;
        QVariantMap values;
    };
    struct Link
    {
        QString folder;
        QString preset;
    };
    struct Removed
    {
        Preset preset;
        int row;
        QList<Link> links;
    };

    int rowOf(const QString& id) const;
    int linkOf(const QString& folder) const;
    void emitLinksChanged();
    void load();
    void save();

    QSettings* m_settings;
    QList<Preset> m_presets;
    QList<Link> m_links;
    std::optional<Removed> m_removed;
};

#endif // QVLC_PRESETS_MODEL_HPP
