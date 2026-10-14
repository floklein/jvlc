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
#ifndef VLC_QT_MODULE_OPTIONS_MODEL_HPP
#define VLC_QT_MODULE_OPTIONS_MODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QAbstractListModel>
#include <QStringList>
#include <QVariant>

#include <vector>

/**
 * The configuration options of one module, as a form.
 *
 * Rows are the module's options in declaration order, minus the excluded
 * ones and the internal (command-line only) ones. Each row carries the
 * section it belongs to. Values start from the current configuration;
 * editing a row changes it for this model only, and the changed rows are
 * returned by options() as ":name=value" input item options.
 */
class ModuleOptionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        TextRole,
        DescriptionRole,
        SectionRole,
        TypeRole,
        ValueRole,
        DefaultValueRole,
        MinimumRole,
        MaximumRole,
        ChoicesRole,
        ModifiedRole,
    };
    Q_ENUM(Roles)

    enum OptionType
    {
        Bool,
        Integer,
        Float,
        String,
    };
    Q_ENUM(OptionType)

    Q_PROPERTY(QString module READ module WRITE setModule NOTIFY moduleChanged FINAL)
    Q_PROPERTY(QStringList excludedOptions READ excludedOptions WRITE setExcludedOptions NOTIFY excludedOptionsChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(bool modified READ modified NOTIFY optionsChanged FINAL)
    Q_PROPERTY(QStringList options READ options NOTIFY optionsChanged FINAL)

public:
    explicit ModuleOptionsModel(QObject* parent = nullptr);

    QString module() const { return m_module; }
    void setModule(const QString& module);
    QStringList excludedOptions() const { return m_excluded; }
    void setExcludedOptions(const QStringList& names);

    bool modified() const;
    QStringList options() const;

    /* row of the option, -1 if it is not listed */
    Q_INVOKABLE int indexOf(const QString& name) const;
    Q_INVOKABLE bool setValue(int row, const QVariant& value);
    /* back to the configuration values */
    Q_INVOKABLE void reset();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void moduleChanged();
    void excludedOptionsChanged();
    void countChanged();
    void optionsChanged();

private:
    struct Option
    {
        QString name;
        QString text;
        QString description;
        QString section;
        OptionType type;
        QVariant initial;
        QVariant value;
        QVariant defaultValue;
        QVariant minimum;
        QVariant maximum;
        QVariantList choices;
    };

    void load();
    QVariant normalized(const Option& option, const QVariant& value) const;

    QString m_module;
    QStringList m_excluded;
    std::vector<Option> m_options;
};

#endif // VLC_QT_MODULE_OPTIONS_MODEL_HPP
