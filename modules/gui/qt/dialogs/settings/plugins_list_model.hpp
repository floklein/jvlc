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

#ifndef VLC_QT_PLUGINS_LIST_MODEL_HPP
#define VLC_QT_PLUGINS_LIST_MODEL_HPP

#include <QAbstractListModel>
#include <QString>
#include <QVector>

/**
 * The plugins of this VLC, read-only, by name: Settings › Extensions ›
 * Advanced Options. The core is not listed.
 */
class PluginsListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern NOTIFY searchPatternChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        // the module name, as on the command line
        ObjectRole,
        CapabilityRole,
        DescriptionRole,
    };
    Q_ENUM(Roles)

    explicit PluginsListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString searchPattern() const { return m_searchPattern; }
    void setSearchPattern(const QString& pattern);

    int count() const { return m_shown.count(); }

signals:
    void searchPatternChanged();
    void countChanged();

private:
    struct Plugin
    {
        QString name;
        QString object;
        QString capability;
        QString description;
    };

    void filter();

    QVector<Plugin> m_plugins;
    // the indexes in m_plugins of the plugins shown
    QVector<int> m_shown;
    QString m_searchPattern;
};

#endif // VLC_QT_PLUGINS_LIST_MODEL_HPP
