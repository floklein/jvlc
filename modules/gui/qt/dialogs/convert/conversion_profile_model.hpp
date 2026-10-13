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
#ifndef VLC_QT_CONVERSION_PROFILE_MODEL_HPP
#define VLC_QT_CONVERSION_PROFILE_MODEL_HPP

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

#include <memory>

class QSettings;
class SoutChain;

/**
 * The Conversion profiles: the built-in ones and the user's custom ones.
 *
 * A Conversion profile is stored as a "category_property=value;..." string,
 * e.g. "video_enable=yes;video_codec=h264;muxer_mux=mp4", in the
 * "codecs-profiles" array of the "vlc-qt-interface" settings, the format
 * used by every VLC since 1.1.
 */
class ConversionProfileModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged FINAL)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectedChanged FINAL)
    Q_PROPERTY(QString selectedValue READ selectedValue NOTIFY selectedChanged FINAL)
    Q_PROPERTY(bool selectedBuiltIn READ selectedBuiltIn NOTIFY selectedChanged FINAL)

public:
    enum Roles {
        NAME = Qt::UserRole + 1,
        VALUE,
        BUILTIN,
    };
    Q_ENUM(Roles)

    /* uses the "vlc-qt-interface" settings of the user */
    explicit ConversionProfileModel(QObject* parent = nullptr);
    /* settings is not owned and must outlive the model */
    explicit ConversionProfileModel(QSettings* settings, QObject* parent = nullptr);
    ~ConversionProfileModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int selected() const;
    void setSelected(int row);
    QString selectedName() const;
    QString selectedValue() const;
    bool selectedBuiltIn() const;

    /** Saves a profile under name, replacing the one with that name if any.
     *  Returns its row. */
    Q_INVOKABLE int save(const QString& name, const QString& value);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE int indexOf(const QString& name) const;

    /* Profile editor: the profile's settings, by "category_property" key */
    Q_INVOKABLE QVariantMap fields(const QString& value) const;
    /* Profile editor: the profile string of the settings from fields() */
    Q_INVOKABLE QString value(const QVariantMap& fields) const;

    /* Profile editor choices, lists of { value, text, ... } */
    Q_INVOKABLE QVariantList muxers() const;
    Q_INVOKABLE QVariantList videoCodecs() const;
    Q_INVOKABLE QVariantList audioCodecs() const;
    Q_INVOKABLE QVariantList subtitleCodecs() const;
    Q_INVOKABLE QVariantList sampleRates() const;
    Q_INVOKABLE QVariantList videoFilters() const;
    Q_INVOKABLE QVariantList audioFilters() const;

    /** The "#transcode{...}" part of a profile, and its muxer */
    static SoutChain transcodeChain(const QString& value, QString* mux = nullptr);

    /** The full stream output chain writing to the destination file */
    static QString soutChain(const QString& value, const QString& destination,
                             bool deinterlace, bool overwrite);

    /** The extension of the files the profile writes, without the dot */
    Q_INVOKABLE static QString fileExtension(const QString& value);

signals:
    void countChanged();
    void selectedChanged();

private:
    struct Profile {
        QString name;
        QString value;
    };

    void load();
    void store() const;
    bool isBuiltIn(const Profile& profile) const;

    std::unique_ptr<QSettings> m_ownSettings;
    QSettings* m_settings = nullptr;
    QVector<Profile> m_profiles;
    int m_selected = 0;
};

#endif // VLC_QT_CONVERSION_PROFILE_MODEL_HPP
