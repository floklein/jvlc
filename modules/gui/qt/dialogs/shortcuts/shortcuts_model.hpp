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

#ifndef VLC_QT_SHORTCUTS_MODEL_HPP
#define VLC_QT_SHORTCUTS_MODEL_HPP

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVector>

#include <functional>

#include "qt.hpp"

/**
 * The rows of the Keyboard Shortcuts dialog: every core hotkey action
 * ("key-*" and "global-key-*" options) and every action of the interface
 * itself ("qt-app-keys"), in plain-word groups.
 *
 * Changes stay in the model until save() writes them into the options of the
 * active Profile; revert() drops them.
 */
class ShortcutsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged FINAL)

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        GroupRole,
        GroupLabelRole,
        LabelRole,
        KeysRole,           // the keys, for display
        GlobalKeysRole,
        HasGlobalKeyRole,   // interface actions work only in VLC's windows
        IsDefaultRole,
    };
    Q_ENUM(Roles)

    enum Group
    {
        Basic,
        Playback,
        Navigation,
        Audio,
        Video,
        Subtitles,
        Interface,
        Advanced,
    };
    Q_ENUM(Group)

    // reads an option, a null string when it is unset
    using ReadFunction = std::function<QString(const QString& option)>;
    using WriteFunction = std::function<void(const QString& option, const QString& value)>;

    explicit ShortcutsModel(QObject* parent = nullptr);

    // replaces the configuration, for tests; call before load()
    void setStore(ReadFunction read, WriteFunction write);
    // run after save() wrote the options
    void setCommitFunction(std::function<void()> commit);

    // reads the actions and their keys
    void load(vlc_object_t* obj);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int rowOf(const QString& name) const;

    // the VLC key of a key press, 0 for keys that cannot be bound alone
    // (modifiers, Tab)
    Q_INVOKABLE int keyFromEvent(int key, int modifiers) const;
    Q_INVOKABLE QString keyText(int key) const;

    // the label of the other action that uses @a key in the same column,
    // empty if none
    Q_INVOKABLE QString conflict(const QString& name, bool global, int key) const;
    // binds @a key to the action, and takes it from the action that used it
    Q_INVOKABLE void assign(const QString& name, bool global, int key);
    Q_INVOKABLE void clear(const QString& name, bool global);

    // the label of another action that uses a default key of the action
    Q_INVOKABLE QString resetConflict(const QString& name) const;
    // restores the default keys of the action, taking them from other actions
    Q_INVOKABLE void reset(const QString& name);
    Q_INVOKABLE void resetAll();

    // writes the changed keys into the active Profile
    Q_INVOKABLE void save();
    // drops the changes since the last save()
    Q_INVOKABLE void revert();

    bool modified() const;

signals:
    void modifiedChanged();

protected:
    vlc_object_t* object() const { return m_obj; }

private:
    using Keys = QVector<uint32_t>;

    struct Entry
    {
        QString name;
        QString label;
        Group group = Advanced;
        bool app = false;   // an interface action

        Keys keys;
        Keys globalKeys;
        Keys defaultKeys;
        Keys defaultGlobalKeys;
        Keys savedKeys;
        Keys savedGlobalKeys;
    };

    static Keys parseKeys(const QString& value);
    static QString formatKeys(const Keys& keys);
    QStringList keyTexts(const Keys& keys) const;

    QString readOption(const QString& option) const;
    void writeOption(const QString& option, const QString& value);

    int otherRowWith(int row, bool global, uint32_t key) const;
    void setKeys(int row, bool global, const Keys& keys);
    void takeKey(int row, bool global, uint32_t key);
    void updateModified();

    vlc_object_t* m_obj = nullptr;
    QVector<Entry> m_entries;
    bool m_modified = false;

    ReadFunction m_read;
    WriteFunction m_write;
    std::function<void()> m_commit;
};

/**
 * The rows of Keyboard Shortcuts that match the search.
 */
class ShortcutsFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern NOTIFY searchPatternChanged FINAL)
    Q_PROPERTY(Field field READ field WRITE setField NOTIFY fieldChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    // what the search looks at
    enum Field
    {
        AllFields,
        Action,
        Key,
    };
    Q_ENUM(Field)

    explicit ShortcutsFilterModel(QObject* parent = nullptr);

    QString searchPattern() const { return m_searchPattern; }
    void setSearchPattern(const QString& pattern);

    Field field() const { return m_field; }
    void setField(Field field);

    int count() const;

    // the row of the action @a name, -1 if hidden
    Q_INVOKABLE int rowOf(const QString& name) const;

signals:
    void searchPatternChanged();
    void fieldChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_searchPattern;
    QStringList m_searchWords;
    Field m_field = AllFields;
};

#endif // VLC_QT_SHORTCUTS_MODEL_HPP
