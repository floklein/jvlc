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

#ifndef VLC_QT_SETTINGS_MODEL_HPP
#define VLC_QT_SETTINGS_MODEL_HPP

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVector>

#include <functional>

#include "qt.hpp"

class QSettings;

/**
 * The rows of the Settings window.
 *
 * Each row is one setting as the user sees it: a label, a control, help
 * text and a value. The value lives in a store: VLC options (one row may
 * write several), a QSettings key, or custom accessors for settings kept
 * elsewhere (Theme, interface scale, language).
 *
 * Changes apply immediately. VLC options are saved to the configuration
 * file shortly after the last change.
 */
class SettingsModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged FINAL)
    Q_PROPERTY(QString profileName READ profileName NOTIFY profileNameChanged FINAL)

public:
    enum Roles
    {
        IdRole = Qt::UserRole + 1,
        PageRole,
        SectionRole,
        LabelRole,
        HelpRole,
        ControlRole,
        ValueRole,
        DefaultValueRole,
        IsDefaultRole,
        ChoicesRole,
        MinimumRole,
        MaximumRole,
        StepRole,
        ScaleRole,
        UnitRole,
        EffectRole,
        AllProfilesRole,
        DeveloperRole,
        AvailableRole,
        PendingRestartRole,
    };
    Q_ENUM(Roles)

    enum Control
    {
        Toggle,
        Choice,     // drop-down list
        Segmented,  // a few choices side by side
        Swatches,   // colors
        Slider,
        Text,
        SaveFile,   // text field plus Browse
    };
    Q_ENUM(Control)

    // when a change takes effect
    enum Effect
    {
        Immediate,
        NextFile,
        Restart,
    };
    Q_ENUM(Effect)

    struct Row
    {
        QString id;
        QString page;
        QString section;
        QString label;
        QString help;

        Control control = Toggle;
        Effect effect = Immediate;

        // shared by every Profile
        bool allProfiles = false;
        // shown only in developer mode
        bool developer = false;

        // { value, label } maps, see choice()
        QVariantList choices;
        std::function<QVariantList()> choicesProvider;

        // sliders; the value is multiplied by scale for display in unit
        double minimum = 0.;
        double maximum = 0.;
        double step = 1.;
        double scale = 1.;
        QString unit;

        // the store
        std::function<bool()> isAvailable;
        std::function<QVariant()> read;
        std::function<QVariant()> readDefault;
        std::function<bool(const QVariant&)> write;
        bool savesConfig = false;
    };

    explicit SettingsModel(qt_intf_t* intf = nullptr, QObject* parent = nullptr);
    ~SettingsModel() override;

    static QVariantMap choice(const QVariant& value, const QString& label);

    // store the row in VLC options; the first existing option gives the
    // value, writes go to every existing option
    void bindConfig(Row& row, const QStringList& options);
    // store the row in a QSettings key
    static void bindSettings(Row& row, QSettings* settings, const QString& key, const QVariant& defaultValue);

    void addRow(Row row);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariant value(const QString& id) const;
    Q_INVOKABLE bool setValue(const QString& id, const QVariant& value);
    Q_INVOKABLE void reset(const QString& id);

    bool restartRequired() const;
    QString profileName() const;

    // saving the configuration file, replaceable by tests
    void setSaveFunction(std::function<void()> save);
    void setSaveDelay(int ms);

signals:
    void settingChanged(const QString& id, const QVariant& value);
    void restartRequiredChanged();
    void profileNameChanged();

protected:
    void setIntf(qt_intf_t* intf);
    qt_intf_t* intf() const { return m_intf; }

private:
    struct Entry
    {
        Row row;
        QVariant startValue;
    };

    int rowOf(const QString& id) const;
    bool isAvailable(const Entry& entry) const;
    QVariant readValue(const Entry& entry) const;
    QVariant readDefault(const Entry& entry) const;
    bool isPendingRestart(const Entry& entry) const;
    bool writeRow(int row, const QVariant& value);
    void updateRestartRequired();
    void save();

    qt_intf_t* m_intf = nullptr;
    QVector<Entry> m_entries;
    bool m_restartRequired = false;
    QString m_profileName;
    QTimer m_saveTimer;
    std::function<void()> m_save;
};

/**
 * The rows of one Settings page, or the search results across pages.
 */
class SettingsFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

    Q_PROPERTY(QString page READ page WRITE setPage NOTIFY pageChanged FINAL)
    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern NOTIFY searchPatternChanged FINAL)
    Q_PROPERTY(bool showDeveloper READ showDeveloper WRITE setShowDeveloper NOTIFY showDeveloperChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    explicit SettingsFilterModel(QObject* parent = nullptr);

    QString page() const { return m_page; }
    void setPage(const QString& page);

    QString searchPattern() const { return m_searchPattern; }
    void setSearchPattern(const QString& pattern);

    bool showDeveloper() const { return m_showDeveloper; }
    void setShowDeveloper(bool show);

    int count() const;

signals:
    void pageChanged();
    void searchPatternChanged();
    void showDeveloperChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    bool matchesSearch(const QModelIndex& index) const;

    QString m_page;
    QString m_searchPattern;
    QStringList m_searchWords;
    bool m_showDeveloper = false;
};

#endif // VLC_QT_SETTINGS_MODEL_HPP
