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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "preset_controller.hpp"

#include <vlc_common.h>
#include <vlc_actions.h>

#include <QUrl>

#include <utility>

PresetController::PresetController(PresetsModel* model, PresetTarget* target, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_target(target)
{
    connect(m_target, &PresetTarget::changedByUser, this, &PresetController::onChangedByUser);

    // a Preset attached, detached, deleted or brought back
    connect(m_model, &PresetsModel::linksChanged, this, [this]() { refresh(false); });
    connect(m_model, &PresetsModel::valuesChanged, this, [this](const QString& id) {
        if (id == m_activeId)
            emit overriddenKeysChanged();
    });
    connect(m_model, &PresetsModel::dataChanged, this,
            [this](const QModelIndex& from, const QModelIndex& to, const QList<int>& roles) {
        if (m_activeId.isEmpty() || !roles.contains(PresetsModel::NAME))
            return;
        for (int row = from.row(); row <= to.row(); ++row)
            if (m_model->data(m_model->index(row), PresetsModel::ID).toString() == m_activeId)
                emit activeChanged();
    });
}

bool PresetController::isPerFile(const QString& key)
{
    return key == QLatin1String("audio-delay") || key == QLatin1String("sub-delay");
}

QVariantMap PresetController::keysMatching(const QVariantMap& values, const QStringList& prefixes)
{
    static const QStringList ownKeys = {
        QStringLiteral("audio-delay"), QStringLiteral("sub-delay"),
        QStringLiteral("audio-language"), QStringLiteral("sub-language"),
        QStringLiteral("rate"),
    };
    QVariantMap matching;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        const bool own = ownKeys.contains(it.key());
        for (const QString& prefix : prefixes)
        {
            if (own ? it.key() == prefix : it.key().startsWith(prefix))
            {
                matching.insert(it.key(), it.value());
                break;
            }
        }
    }
    return matching;
}

QString PresetController::keyOfAction(int action)
{
    switch (action)
    {
    case ACTIONID_AUDIODELAY_DOWN:
    case ACTIONID_AUDIODELAY_UP:
        return QStringLiteral("audio-delay");
    case ACTIONID_SUBDELAY_DOWN:
    case ACTIONID_SUBDELAY_UP:
    case ACTIONID_SUBSYNC_APPLY:
    case ACTIONID_SUBSYNC_RESET:
        return QStringLiteral("sub-delay");
    case ACTIONID_RATE_NORMAL:
    case ACTIONID_RATE_SLOWER:
    case ACTIONID_RATE_FASTER:
    case ACTIONID_RATE_SLOWER_FINE:
    case ACTIONID_RATE_FASTER_FINE:
        return QStringLiteral("rate");
    default:
        return {};
    }
}

QVariantMap PresetController::layered(const QVariantMap& profile, const QVariantMap& preset,
                                      const QVariantMap& file)
{
    QVariantMap values = profile;
    for (const QVariantMap* layer : { &preset, &file })
        for (auto it = layer->cbegin(); it != layer->cend(); ++it)
            values.insert(it.key(), it.value());
    return values;
}

QString PresetController::folderName() const
{
    const QString name = m_folder.section('/', -1);
    return name.isEmpty() ? m_folder : name;
}

QString PresetController::activeName() const
{
    return m_model->name(m_activeId);
}

QStringList PresetController::overriddenKeys() const
{
    if (m_activeId.isEmpty())
        return {};
    return m_model->values(m_activeId).keys();
}

bool PresetController::isOverridden(const QStringList& keys) const
{
    if (m_activeId.isEmpty())
        return false;
    const QVariantMap values = m_model->values(m_activeId);
    for (const QString& key : keys)
        if (values.contains(key))
            return true;
    return false;
}

void PresetController::setMedia(const QString& mrl)
{
    QString folder;
    const QUrl url(mrl);
    if (url.isLocalFile())
    {
        const QString path = url.toLocalFile();
        folder = PresetsModel::normalizeFolder(path.left(path.lastIndexOf('/')));
    }
    if (folder != m_folder)
    {
        m_folder = folder;
        emit folderChanged();
    }
    refresh(true);
}

void PresetController::profileSwitched()
{
    if (m_activeId.isEmpty())
        return;
    show(layered({}, m_model->values(m_activeId), m_target->fileValues()), false);
}

void PresetController::refresh(bool mediaChanged)
{
    const PresetsModel::Resolution resolution = m_folder.isEmpty()
        ? PresetsModel::Resolution{} : m_model->resolve(m_folder);
    const QString id = m_model->contains(resolution.id) ? resolution.id : QString();

    const bool changed = id != m_activeId;
    if (!changed && !mediaChanged)
    {
        if (resolution.folder != m_activeFolder && !id.isEmpty())
        {
            m_activeFolder = resolution.folder;
            emit activeChanged();
        }
        return;
    }

    QVariantMap values;

    // the old Preset's values go back to the Profile's; a new file starts
    // with its own per-file ones
    if (changed && !m_activeId.isEmpty())
    {
        const QVariantMap profile = m_target->profileValues();
        for (const QString& key : std::as_const(m_shown))
            if (!(mediaChanged && isPerFile(key)) && profile.contains(key))
                values.insert(key, profile.value(key));
    }
    m_shown.clear();

    // the Preset over the Profile, the file's own choices over both
    const QVariantMap preset = m_model->values(id);
    for (auto it = preset.cbegin(); it != preset.cend(); ++it)
        m_shown.insert(it.key());
    const QVariantMap file = m_target->fileValues();
    values = layered(values, preset, file);

    m_activeId = id;
    m_activeFolder = id.isEmpty() ? QString() : resolution.folder;

    m_target->setProfileWrites(id.isEmpty());
    show(values, mediaChanged);

    if (changed)
    {
        emit activeChanged();
        emit overriddenKeysChanged();
    }
}

void PresetController::show(const QVariantMap& values, bool newFile)
{
    const QVariantMap live = m_target->values();
    QVariantMap changes;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        if ((newFile && isPerFile(it.key())) || live.value(it.key()) != it.value())
            changes.insert(it.key(), it.value());
    if (changes.isEmpty())
        return;

    m_applying = true;
    m_target->apply(changes);
    m_applying = false;
}

void PresetController::onChangedByUser(const QVariantMap& values)
{
    if (m_applying || m_activeId.isEmpty())
        return;

    const QVariantMap profile = m_target->profileValues();
    const QVariantMap preset = m_model->values(m_activeId);
    QVariantMap kept;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        // only what differs from what the Preset or the Profile gives
        const QVariant expected = preset.contains(it.key()) ? preset.value(it.key())
                                                            : profile.value(it.key());
        if (it.value() == expected)
            continue;
        m_model->setValue(m_activeId, it.key(), it.value());
        m_shown.insert(it.key());
        kept.insert(it.key(), it.value());
    }

    // a picked track only gives its language: show it as the preference
    show(kept, false);
}

void PresetController::choose(const QString& id)
{
    chooseForFolder(m_folder, id);
}

void PresetController::chooseForFolder(const QString& path, const QString& id)
{
    const QString folder = PresetsModel::normalizeFolder(path);
    if (folder.isEmpty() || (!id.isEmpty() && !m_model->contains(id)))
        return;
    if (m_model->resolve(folder).id == id)
        return;

    if (id.isEmpty())
    {
        // Default: drop the folder's own Preset, and stop a parent's
        m_model->detach(folder);
        if (!m_model->resolve(folder).id.isEmpty())
            m_model->attach(folder, QString());
        return;
    }

    m_model->attach(folder, id);
    const QString name = folder.section('/', -1);
    emit attached(m_model->name(id), name.isEmpty() ? folder : name);
}

QString PresetController::createFromCurrent(const QString& name)
{
    const QVariantMap live = m_target->values();
    const QVariantMap profile = m_target->profileValues();
    QVariantMap values;
    for (auto it = live.cbegin(); it != live.cend(); ++it)
        if (profile.contains(it.key()) && profile.value(it.key()) != it.value())
            values.insert(it.key(), it.value());

    const QString id = m_model->create(name, values);
    choose(id);
    return id;
}

void PresetController::resetToProfile(const QStringList& keys)
{
    if (m_activeId.isEmpty())
        return;
    const QVariantMap profile = m_target->profileValues();
    QVariantMap values;
    for (const QString& key : keys)
    {
        m_model->clearValue(m_activeId, key);
        m_shown.remove(key);
        if (profile.contains(key))
            values.insert(key, profile.value(key));
    }
    show(values, false);
}
