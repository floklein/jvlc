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
#include "controlbar_profile_model.hpp"

#include <QSettings>
#include <QChar>

#include "qt.hpp"
#include "controlbar_profile.hpp"
#include "player/control_list_model.hpp"
#include "player/player_controlbar_model.hpp"

// NOTE: QSettings started to accept QAnyStringView with Qt 6.4,
//       so instead of a QString(Literal), use QLatin1String(View):
// The Layouts of the redesign are not migrated from the old control bar
// profiles ("ControlbarProfileModel" group): everyone starts on Default.
constexpr QLatin1String SETTINGS_GROUP {"PlayerLayouts"};
constexpr QLatin1String SETTINGS_KEY_SELECTEDPROFILE {"SelectedProfile"};
constexpr QLatin1String SETTINGS_ARRAYNAME_PROFILES {"Profiles"};
constexpr QLatin1String SETTINGS_KEY_NAME {"Name"};
constexpr QLatin1String SETTINGS_KEY_MODEL {"Model"};
constexpr QLatin1String SETTINGS_KEY_STYLE {"Style"};

constexpr QChar SETTINGS_CONTROL_SEPARATOR {','};
constexpr QChar SETTINGS_CONFIGURATION_SEPARATOR {'|'};
constexpr QChar SETTINGS_PROFILE_SEPARATOR {'$'};

using C = ControlListModel;

// The mini player is not customizable: every Layout uses this arrangement
static const PlayerControlbarModel::Zones miniPlayerZones {
    QVector<int> { C::ARTWORK_INFO },
    QVector<int> { C::RANDOM_BUTTON, C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON, C::LOOP_BUTTON },
    QVector<int> { C::VOLUME, C::PLAYER_SWITCH_BUTTON },
    QVector<int> {}, QVector<int> {}, QVector<int> {}
};

decltype (ControlbarProfileModel::m_defaults)
    ControlbarProfileModel::m_defaults =
        {
            {
                Style::EMPTY_STYLE,
                N_("Empty"),
                {
                    { PlayerControlbarModel::Videoplayer, {} },
                    { PlayerControlbarModel::Audioplayer, {} },
                    { PlayerControlbarModel::Miniplayer, miniPlayerZones }
                }
            },
            {
                // Figma 247:447 (Top bar) and 491:1779 (Control bar)
                Style::DEFAULT_STYLE,
                N_("Default"),
                {
                    {
                        PlayerControlbarModel::Videoplayer,
                        {
                            QVector<int> { C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON },
                            QVector<int> { C::SEEK_BAR },
                            QVector<int> { C::VOLUME, C::AUDIO_BUTTON, C::SUBTITLES_BUTTON,
                                           C::EXTENDED_BUTTON, C::PLAYLIST_BUTTON },
                            QVector<int> { C::VLC_MENU_BUTTON, C::LIBRARY_BUTTON, C::DIVIDER,
                                           C::FOLDER_BUTTON },
                            QVector<int> { C::TITLE_WIDGET },
                            QVector<int> {}
                        }
                    },
                    {
                        // The music player (Figma 4778:26181) adds the
                        // ±15 s skips, drops the subtitles button but keeps
                        // the adjustments (equalizer)
                        PlayerControlbarModel::Audioplayer,
                        {
                            QVector<int> { C::PREVIOUS_BUTTON, C::SKIP_BACK_BUTTON, C::PLAY_BUTTON,
                                           C::SKIP_FW_BUTTON, C::NEXT_BUTTON },
                            QVector<int> { C::SEEK_BAR },
                            QVector<int> { C::VOLUME, C::AUDIO_BUTTON, C::EXTENDED_BUTTON,
                                           C::PLAYLIST_BUTTON },
                            QVector<int> { C::VLC_MENU_BUTTON, C::LIBRARY_BUTTON, C::DIVIDER,
                                           C::FOLDER_BUTTON },
                            QVector<int> { C::TITLE_WIDGET },
                            QVector<int> {}
                        }
                    },
                    { PlayerControlbarModel::Miniplayer, miniPlayerZones }
                }
            },
            {
                // Figma 2160:7437: the seek bar on its own row, everything
                // else in the Control bar
                Style::TWO_ROW_STYLE,
                N_("Two-row"),
                {
                    {
                        PlayerControlbarModel::Videoplayer,
                        {
                            QVector<int> { C::VLC_MENU_BUTTON, C::LIBRARY_BUTTON, C::DIVIDER,
                                           C::FOLDER_BUTTON, C::TITLE_WIDGET },
                            QVector<int> { C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON },
                            QVector<int> { C::VOLUME, C::AUDIO_BUTTON, C::SUBTITLES_BUTTON,
                                           C::EXTENDED_BUTTON, C::PLAYLIST_BUTTON },
                            QVector<int> {}, QVector<int> {}, QVector<int> {}
                        }
                    },
                    {
                        PlayerControlbarModel::Audioplayer,
                        {
                            QVector<int> { C::VLC_MENU_BUTTON, C::LIBRARY_BUTTON, C::DIVIDER,
                                           C::FOLDER_BUTTON, C::TITLE_WIDGET },
                            QVector<int> { C::PREVIOUS_BUTTON, C::SKIP_BACK_BUTTON, C::PLAY_BUTTON,
                                           C::SKIP_FW_BUTTON, C::NEXT_BUTTON },
                            QVector<int> { C::VOLUME, C::AUDIO_BUTTON, C::EXTENDED_BUTTON,
                                           C::PLAYLIST_BUTTON },
                            QVector<int> {}, QVector<int> {}, QVector<int> {}
                        }
                    },
                    { PlayerControlbarModel::Miniplayer, miniPlayerZones }
                }
            }
        };


ControlbarProfileModel::ControlbarProfileModel(QSettings* settings, QObject *parent)
    : QAbstractListModel(parent),
    m_settings(settings)
{
    assert(m_settings);

    connect(this, &QAbstractListModel::rowsInserted, this, &ControlbarProfileModel::countChanged);
    connect(this, &QAbstractListModel::rowsRemoved, this, &ControlbarProfileModel::countChanged);
    connect(this, &QAbstractListModel::modelReset, this, &ControlbarProfileModel::countChanged);

    // To make the QML player controlbars update when model is Reset
    connect(this, &QAbstractListModel::modelReset, this, &ControlbarProfileModel::selectedProfileChanged);

    connect(this, &ControlbarProfileModel::selectedProfileChanged, this, &ControlbarProfileModel::dirtyChanged);

    // When all profiles are removed, insert defaults:
    // Maybe add a dedicate button for this purpose and don't allow removing all profiles ?
    connect(this, &ControlbarProfileModel::countChanged, this, [this] () {
        if (rowCount() == 0)
            insertDefaults();
    });

    // defer initialization reload:
    QMetaObject::invokeMethod(this, [this] () {
        if (reload() == false)
        {
            // If initial reload fails, load the default profiles:
            insertDefaults();
        }
    }, Qt::QueuedConnection);
}

void ControlbarProfileModel::insertDefaults()
{
    // Add default profiles:
    for (const auto& i : m_defaults)
    {
        if (i.id == Style::EMPTY_STYLE)
            continue;

        const auto ptrNewProfile = newProfile(qfut(i.name));
        if (!ptrNewProfile)
            continue;

        ptrNewProfile->injectModel(i.modelData);
        ptrNewProfile->setStyle(static_cast<int>(i.id));
        ptrNewProfile->resetDirty(); // default profiles should not be dirty initially
    }

    setSelectedProfile(0);
    m_savedSelectedProfile = m_selectedProfile;
    emit dirtyChanged();
}

void ControlbarProfileModel::watchProfile(ControlbarProfile *profile)
{
    connect(profile, &ControlbarProfile::dirtyChanged, this, &ControlbarProfileModel::dirtyChanged);
}

QString ControlbarProfileModel::generateUniqueName(const QString &name)
{
    static const auto targetNameGenerator = [](const auto& name, long count) {
        return QString("%1 (%2)").arg(name).arg(count);
    };

    // Actually, the profile model inherently does not allow two
    // profiles to have the same name so it could be sufficient
    // to check only for name existence but for cases when the
    // config file is edited explicitly, using count_if might
    // be helpful.
    // NOTE: not static, it captures this model
    const auto sameNameCount = [this](const auto& name) {
        return std::count_if(m_profiles.begin(),
                             m_profiles.end(),
                             [name](const ControlbarProfile* i) {
                                 return i->name() == name;
                                 });
        };

    auto count = sameNameCount(name);

    if (count > 0)
    {
        // suppose the existing profiles with these names:
        // Profile, Profile (1), Profile (2)
        // when the user adds a new profile with name 'Profile',
        // its name will be replaced with 'Profile (3)'.
        // However, the same will not happen if the user adds a
        // new profile with name 'Profile (1)' or 'Profile (2)'.
        // In that case, the new names will be 'Profile (1) (1)',
        // and 'Profile (2) (1)', respectively. This behavior is
        // intended because otherwise profile name assignment
        // would be restrictive.

        auto targetName = targetNameGenerator(name, count);

        // if targetName also exists, increase the count by one
        // and try again:
        while (sameNameCount(targetName) >= 1)
        {
            ++count;
            targetName = targetNameGenerator(name, count);
        }

        return targetName;
    }
    else
        return name;
}

int ControlbarProfileModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_profiles.size();
}

QVariant ControlbarProfileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto ptrProfile = m_profiles.at(index.row());

    if (!ptrProfile)
        return QVariant();

    switch (role)
    {
    case Qt::DisplayRole:
        return ptrProfile->name();
    case MODEL_ROLE:
        return QVariant::fromValue(ptrProfile);
    }

    return QVariant();
}

QHash<int, QByteArray> ControlbarProfileModel::roleNames() const
{
    return {
        {
            Qt::DisplayRole, QByteArrayLiteral("name")
        },
        {
            MODEL_ROLE, QByteArrayLiteral("model")
        }
    };
}

bool ControlbarProfileModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || row > m_profiles.size())
        return false;

    beginInsertRows(parent, row, row + count - 1);

    for (int i = 0; i < count; ++i)
    {
        const auto profile = generateProfileFromStyle(Style::EMPTY_STYLE);
        assert(profile);
        profile->setParent(this);
        profile->setName(tr("Profile %1").arg(m_profiles.size()));
        watchProfile(profile);

        m_profiles.insert(row, profile);
    }

    endInsertRows();

    return true;
}

bool ControlbarProfileModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (row < 0 || count < 1 || row + count > m_profiles.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);

    auto from = m_profiles.begin() + row;
    auto to = from + count - 1;
    std::for_each(from, to, [](auto* item) {
        assert(item);
        item->deleteLater();
    });
    m_profiles.erase(from, to);

    endRemoveRows();

    return true;
}

bool ControlbarProfileModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (data(index, role) != value)
    {
        auto ptrProfile = m_profiles.at(index.row());

        if (!ptrProfile)
            return false;

        switch (role)
        {
        case Qt::DisplayRole:
            if (value.canConvert<QString>())
                ptrProfile->setName(value.toString());
            else
                return false;
            break;
        case MODEL_ROLE:
            if (value.canConvert<ControlbarProfile*>())
                ptrProfile = qvariant_cast<ControlbarProfile*>(value);
            else
                return false;
            break;
        default:
            return false;
        }

        m_profiles.replace(index.row(), ptrProfile);

        emit dataChanged(index, index, { role });
        return true;
    }
    return false;
}

Qt::ItemFlags ControlbarProfileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return (Qt::ItemIsEditable | Qt::ItemNeverHasChildren);
}

int ControlbarProfileModel::selectedProfile() const
{
    return m_selectedProfile;
}

ControlbarProfile* ControlbarProfileModel::currentModel() const
{
    return getProfile(selectedProfile());
}

bool ControlbarProfileModel::dirty() const
{
    if (m_selectedProfile != m_savedSelectedProfile)
        return true;

    return std::any_of(m_profiles.begin(), m_profiles.end(), [](const ControlbarProfile* i) {
        return i->dirty();
    });
}

bool ControlbarProfileModel::testOpenEditor() const
{
    return !qEnvironmentVariableIsEmpty("VLC_QT_TEST_CUSTOMIZE_PLAYER");
}

void ControlbarProfileModel::resetToDefault(int identifier)
{
    const auto ptrProfile = currentModel();
    if (!ptrProfile || identifier == PlayerControlbarModel::Miniplayer)
        return;

    Style style = Style::DEFAULT_STYLE;
    for (const auto& i : m_defaults)
    {
        if (static_cast<int>(i.id) == ptrProfile->style())
            style = i.id;
    }

    for (const auto& i : m_defaults)
    {
        if (i.id != style)
            continue;

        for (const auto& configuration : i.modelData)
        {
            if (configuration.identifier == identifier)
                ptrProfile->setModelData(identifier, configuration.data);
        }
    }
}

void ControlbarProfileModel::save(bool clearDirty) const
{
    m_settings->beginGroup(SETTINGS_GROUP);
    m_settings->remove(""); // clear the group before save

    m_settings->setValue(SETTINGS_KEY_SELECTEDPROFILE, selectedProfile());

    m_settings->beginWriteArray(SETTINGS_ARRAYNAME_PROFILES);

    for (int i = 0; i < m_profiles.size(); ++i)
    {
        m_settings->setArrayIndex(i);

        const auto& ptrModelMap = m_profiles.at(i)->m_models;

        QString val;
        for (auto it = ptrModelMap.constBegin(); it != ptrModelMap.end(); ++it)
        {
            const int identifier = it.key();

            // the mini player is fixed
            if (identifier == PlayerControlbarModel::Miniplayer)
                continue;

            const auto serializedModels = m_profiles.at(i)->getModelData(identifier);

            static const auto join = [](const QVector<int>& list) {
                QString ret;
                for (auto i : list)
                {
                    ret += QString::number(i) + SETTINGS_CONTROL_SEPARATOR;
                }
                if (!ret.isEmpty())
                    ret.chop(1);
                return ret;
            };

            val += SETTINGS_PROFILE_SEPARATOR % QString::number(identifier);
            for (const auto& zone : serializedModels)
                val += SETTINGS_CONFIGURATION_SEPARATOR % join(zone);
        }

        if (clearDirty)
            m_profiles.at(i)->resetDirty();

        m_settings->setValue(SETTINGS_KEY_NAME, m_profiles.at(i)->name());
        m_settings->setValue(SETTINGS_KEY_MODEL, val);
        m_settings->setValue(SETTINGS_KEY_STYLE, m_profiles.at(i)->style());
    }

    m_settings->endArray();
    m_settings->endGroup();

    if (clearDirty && m_savedSelectedProfile != m_selectedProfile)
    {
        m_savedSelectedProfile = m_selectedProfile;
        emit const_cast<ControlbarProfileModel*>(this)->dirtyChanged();
    }
}

bool ControlbarProfileModel::reload()
{
    m_settings->beginGroup(SETTINGS_GROUP);

    const int size = m_settings->beginReadArray(SETTINGS_ARRAYNAME_PROFILES);

    if (size <= 0)
    {
        m_settings->endArray();
        m_settings->endGroup();

        return false;
    }

    beginResetModel();

    // what a saved Layout lacks comes from Default, and the mini player
    // always does
    QVector<ControlbarProfile::Configuration> defaults;
    for (const auto& i : m_defaults)
    {
        if (i.id == Style::DEFAULT_STYLE)
            defaults = i.modelData;
    }

    decltype (m_profiles) profiles;
    for (int i = 0; i < size; ++i)
    {
        m_settings->setArrayIndex(i);

        const QString modelValue = m_settings->value(SETTINGS_KEY_MODEL).toString();
        if (modelValue.isEmpty())
            continue;

        QStringView modelValueStringView(modelValue);
        const auto val = modelValueStringView.split(SETTINGS_PROFILE_SEPARATOR);
        if (val.isEmpty())
            continue;

        const auto ptrNewProfile = new ControlbarProfile(this);
        ptrNewProfile->setName(m_settings->value(SETTINGS_KEY_NAME).toString());
        ptrNewProfile->setStyle(m_settings->value(SETTINGS_KEY_STYLE, -1).toInt());
        ptrNewProfile->injectModel(defaults);

        for (auto j : val)
        {
            if (j.isEmpty())
                continue;

            const auto alignments = j.split(SETTINGS_CONFIGURATION_SEPARATOR);

            if (alignments.length() != 1 + PlayerControlbarModel::ZoneCount)
                continue;

            bool ok = false;
            int identifier = alignments[0].toInt(&ok);
            if (!ok || identifier < 0 || identifier == PlayerControlbarModel::Miniplayer)
                continue;

            static const auto split = [](auto ref) {
                QVector<int> list;

                if (ref.isEmpty())
                    return list;

                for (auto i : ref.split(SETTINGS_CONTROL_SEPARATOR))
                {
                    bool ok = false;
                    int k = i.toInt(&ok);

                    if (ok)
                        list.append(k);
                }
                return list;
            };

            PlayerControlbarModel::Zones data;
            for (int zone = 0; zone < PlayerControlbarModel::ZoneCount; ++zone)
                data[zone] = split(alignments[zone + 1]);

            ptrNewProfile->setModelData(identifier, data);
        }

        ptrNewProfile->resetDirty(); // Newly loaded model can not be dirty
        watchProfile(ptrNewProfile);

        profiles.append(ptrNewProfile);
    }

    m_settings->endArray();

    m_selectedProfile = -1;
    std::for_each(m_profiles.begin(), m_profiles.end(), [](auto i) { delete i; });

    m_profiles = std::move(profiles);

    endResetModel();

    bool ok = false;
    int index = m_settings->value(SETTINGS_KEY_SELECTEDPROFILE).toInt(&ok);

    if (ok && getProfile(index))
        setSelectedProfile(index);
    else
        setSelectedProfile(0);

    m_savedSelectedProfile = m_selectedProfile;
    emit dirtyChanged();

    m_settings->endGroup();

    return true;
}

bool ControlbarProfileModel::setSelectedProfile(int selectedProfile)
{
    if (m_selectedProfile == selectedProfile)
        return false;

    const auto ptrProfileNew = getProfile(selectedProfile);
    const auto ptrProfileOld = getProfile(m_selectedProfile);

    assert(ptrProfileNew);

    if (!ptrProfileNew)
        return false;

    connect(ptrProfileNew, &ControlbarProfile::controlListChanged, this, &ControlbarProfileModel::selectedProfileControlListChanged);
    connect(this, &QAbstractListModel::modelReset, ptrProfileNew, &ControlbarProfile::generateLinearControlList);
    connect(this, &ControlbarProfileModel::selectedProfileChanged, ptrProfileNew, &ControlbarProfile::generateLinearControlList);

    if (ptrProfileOld && (ptrProfileNew != ptrProfileOld))
    {
        disconnect(ptrProfileOld, &ControlbarProfile::controlListChanged, this, &ControlbarProfileModel::selectedProfileControlListChanged);
        disconnect(this, &QAbstractListModel::modelReset, ptrProfileOld, &ControlbarProfile::generateLinearControlList);
        disconnect(this, &ControlbarProfileModel::selectedProfileChanged, ptrProfileOld, &ControlbarProfile::generateLinearControlList);
    }

    m_selectedProfile = selectedProfile;

    emit selectedProfileChanged();

    return true;
}

std::optional<int> ControlbarProfileModel::findModel(const ControlbarProfile *profile) const
{
    assert(profile);
    for (int i = 0; i < m_profiles.count(); ++i)
    {
        if (m_profiles[i] == profile)
            return i;
        else
        {
            assert(m_profiles[i]);
            if (*m_profiles[i] == *profile)
                return i;
        }
    }
    return std::nullopt;
}

void ControlbarProfileModel::insertProfile(std::unique_ptr<ControlbarProfile> profile, bool select)
{
    assert(profile);
    const auto ptrProfile = profile.release();

    ptrProfile->setParent(this);
    watchProfile(ptrProfile);

    beginInsertRows(QModelIndex(), m_profiles.size(), m_profiles.size());
    m_profiles.append(ptrProfile);
    endInsertRows();

    if (select)
        setSelectedProfile(m_profiles.count() - 1);
}

ControlbarProfile *ControlbarProfileModel::getProfile(int index) const
{
    if (index < 0 || index >= m_profiles.size())
        return nullptr;

    return m_profiles.at(index);
}

ControlbarProfile *ControlbarProfileModel::newProfile(const QString &name)
{
    if (name.isEmpty())
        return nullptr;

    const auto ptrProfile = newProfile();

    ptrProfile->setName(generateUniqueName(name));

    return ptrProfile;
}

ControlbarProfile *ControlbarProfileModel::newProfile()
{
    const auto ptrNewProfile = generateProfileFromStyle(Style::EMPTY_STYLE);
    assert(ptrNewProfile);
    ptrNewProfile->setParent(this);
    watchProfile(ptrNewProfile);

    beginInsertRows(QModelIndex(), m_profiles.size(), m_profiles.size());

    m_profiles.append(ptrNewProfile);

    endInsertRows();

    return ptrNewProfile;
}

ControlbarProfile *ControlbarProfileModel::cloneProfile(const ControlbarProfile *profile)
{
    const auto ptrNewProfile = newProfile(profile->name());

    if (!ptrNewProfile)
        return nullptr;

    for (auto it = profile->m_models.constBegin(); it != profile->m_models.constEnd(); ++it)
    {
        ptrNewProfile->setModelData(it.key(), profile->getModelData(it.key()));
        ptrNewProfile->resetDirty();
    }
    ptrNewProfile->setStyle(profile->style());

    return ptrNewProfile;
}

void ControlbarProfileModel::cloneSelectedProfile(const QString &newProfileName)
{
    const auto ptrModel = currentModel();

    assert(ptrModel);
    if (!ptrModel)
        return;

    const auto ptrNewModel = cloneProfile(ptrModel);

    assert(ptrNewModel);
    if (!ptrNewModel)
        return;

    // if the newProfileName is empty or equal to the cloned profile's name
    // don't bother changing its name since cloneProfile() will first
    // set the new profile's name unique by assigning its name as
    // 'clonedProfileName (n)'
    if (!newProfileName.isEmpty() && newProfileName != ptrModel->name())
        ptrNewModel->setName(generateUniqueName(newProfileName));
}

void ControlbarProfileModel::deleteSelectedProfile()
{
    const auto ptrSelectedProfile = getProfile(m_selectedProfile);

    if (!ptrSelectedProfile)
        return;

    const auto _selectedProfile = m_selectedProfile;
    beginRemoveRows(QModelIndex(), _selectedProfile, _selectedProfile);

    m_selectedProfile = -1;

    delete ptrSelectedProfile;
    m_profiles.removeAt(_selectedProfile);

    endRemoveRows();

    if (getProfile(_selectedProfile - 1))
        setSelectedProfile(_selectedProfile - 1);
    else
        setSelectedProfile(_selectedProfile);
}

ControlbarProfile *ControlbarProfileModel::generateProfileFromStyle(const Style style)
{
    for (const auto& i : m_defaults)
    {
        if (i.id == style)
        {
            const auto ptrNewProfile = new ControlbarProfile();
            ptrNewProfile->injectModel(i.modelData);
            ptrNewProfile->setName(qfut(i.name));
            ptrNewProfile->setStyle(static_cast<int>(i.id));
            ptrNewProfile->resetDirty(); // default profiles should not be dirty initially
            return ptrNewProfile;
        }
    }

    // Style does not exist
    return nullptr;
}
