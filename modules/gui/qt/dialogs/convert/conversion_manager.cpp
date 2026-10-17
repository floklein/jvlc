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
#include "conversion_manager.hpp"

#include <vlc_common.h>
#include <vlc_input_item.h>
#include <vlc_interface.h>
#include <vlc_variables.h>

#include "conversion_profile_model.hpp"
#include "qt.hpp"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

const vlc_player_cbs* playerCallbacks(
        void (*onState)(vlc_player_t*, enum vlc_player_state, void*),
        void (*onError)(vlc_player_t*, enum vlc_player_error, void*),
        void (*onPosition)(vlc_player_t*, vlc_tick_t, double, void*))
{
    static vlc_player_cbs cbs = [&] {
        vlc_player_cbs cbs{};
        cbs.on_state_changed = onState;
        cbs.on_error_changed = onError;
        cbs.on_position_changed = onPosition;
        return cbs;
    }();
    return &cbs;
}

bool isActive(int state)
{
    return state == ConversionManager::Pending || state == ConversionManager::Running;
}

} // anonymous namespace

ConversionManager::ConversionManager(vlc_object_t* parent, QObject* qparent)
    : QAbstractListModel(qparent)
    , m_parent(parent)
{
}

ConversionManager::~ConversionManager()
{
    if (!m_player)
        return;

    vlc_player_Lock(m_player);
    vlc_player_RemoveListener(m_player, m_listener);
    vlc_player_Unlock(m_player);
    /* stops the running conversion */
    vlc_player_Delete(m_player);

    const int row = runningRow();
    if (row >= 0)
        removeOutput(m_conversions[row]);

    vlc_object_delete(m_object);
}

void ConversionManager::removeOutput(const Conversion& conversion)
{
    if (conversion.output != conversion.destination)
        QFile::remove(conversion.output);
    else if (conversion.overwrite || !conversion.existed)
        QFile::remove(conversion.destination);
}

bool ConversionManager::ensurePlayer()
{
    if (m_player)
        return true;

    m_object = static_cast<vlc_object_t*>(vlc_object_create(m_parent, sizeof(*m_object)));
    if (!m_object)
        return false;

    /* the conversion never shows anything */
    var_Create(m_object, "vout", VLC_VAR_STRING);
    var_SetString(m_object, "vout", "dummy");
    var_Create(m_object, "aout", VLC_VAR_STRING);
    var_SetString(m_object, "aout", "dummy");

    m_player = vlc_player_New(m_object, VLC_PLAYER_LOCK_NORMAL);
    if (!m_player)
    {
        vlc_object_delete(m_object);
        m_object = nullptr;
        return false;
    }

    vlc_player_Lock(m_player);
    m_listener = vlc_player_AddListener(m_player,
                                        playerCallbacks(&onStateChanged, &onErrorChanged,
                                                        &onPositionChanged),
                                        this);
    vlc_player_Unlock(m_player);
    if (!m_listener)
    {
        vlc_player_Delete(m_player);
        m_player = nullptr;
        vlc_object_delete(m_object);
        m_object = nullptr;
        return false;
    }
    return true;
}

void ConversionManager::onStateChanged(vlc_player_t*, enum vlc_player_state state, void* data)
{
    auto that = static_cast<ConversionManager*>(data);
    if (state == VLC_PLAYER_STATE_STARTED)
        that->m_postedProgress = 0.;
    else if (state == VLC_PLAYER_STATE_STOPPED)
        QMetaObject::invokeMethod(that, [that]() { that->finishRunning(); },
                                  Qt::QueuedConnection);
}

void ConversionManager::onErrorChanged(vlc_player_t*, enum vlc_player_error error, void* data)
{
    if (error == VLC_PLAYER_ERROR_NONE)
        return;
    auto that = static_cast<ConversionManager*>(data);
    QMetaObject::invokeMethod(that, [that]() { that->m_error = true; },
                              Qt::QueuedConnection);
}

void ConversionManager::onPositionChanged(vlc_player_t*, vlc_tick_t, double position, void* data)
{
    auto that = static_cast<ConversionManager*>(data);
    /* the conversion runs as fast as it can: don't flood the UI */
    if (position - that->m_postedProgress < 0.005)
        return;
    that->m_postedProgress = position;
    QMetaObject::invokeMethod(that, [that, position]() { that->setRunningProgress(position); },
                              Qt::QueuedConnection);
}

int ConversionManager::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_conversions.size();
}

QVariant ConversionManager::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Conversion& conversion = m_conversions[index.row()];
    switch (role)
    {
    case Qt::DisplayRole:
    case NAME:
    {
        const QString name = QUrl(conversion.mrl).fileName();
        return name.isEmpty() ? conversion.mrl : name;
    }
    case SOURCE:
        return conversion.mrl;
    case DESTINATION:
        return conversion.destination;
    case STATE:
        return QVariant::fromValue(conversion.state);
    case PROGRESS:
        return conversion.progress;
    default:
        return {};
    }
}

QHash<int, QByteArray> ConversionManager::roleNames() const
{
    return {
        { NAME, "name" },
        { SOURCE, "source" },
        { DESTINATION, "destination" },
        { STATE, "status" },
        { PROGRESS, "progress" },
    };
}

int ConversionManager::activeCount() const
{
    return std::count_if(m_conversions.cbegin(), m_conversions.cend(),
                         [](const Conversion& conversion) { return isActive(conversion.state); });
}

double ConversionManager::progress() const
{
    double sum = 0.;
    int count = 0;
    for (const Conversion& conversion : m_conversions)
    {
        if (conversion.state == Failed || conversion.state == Cancelled)
            continue;
        sum += conversion.progress;
        count++;
    }
    return count ? sum / count : 0.;
}

int ConversionManager::runningRow() const
{
    for (int i = 0; i < m_conversions.size(); i++)
        if (m_conversions[i].state == Running)
            return i;
    return -1;
}

QString ConversionManager::convertedPath(const QString& mrl, const QString& extension) const
{
    const QUrl url(mrl);
    QString dir;
    QString base;
    if (url.isLocalFile())
    {
        const QFileInfo info(url.toLocalFile());
        dir = info.absolutePath();
        base = info.completeBaseName();
    }
    else
    {
        dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        base = QFileInfo(url.path()).completeBaseName();
    }
    if (base.isEmpty())
        base = QStringLiteral("media");

    const auto taken = [this](const QString& path) {
        if (QFileInfo::exists(path))
            return true;
        return std::any_of(m_conversions.cbegin(), m_conversions.cend(), [&path](const Conversion& conversion) {
            return isActive(conversion.state) && conversion.destination == path;
        });
    };

    QString path = QDir(dir).filePath(QStringLiteral("%1-converted.%2").arg(base, extension));
    for (int i = 2; taken(path); i++)
        path = QDir(dir).filePath(QStringLiteral("%1-converted (%2).%3").arg(base).arg(i).arg(extension));
    return path;
}

QStringList ConversionManager::chooseMedia() const
{
    const QList<QUrl> urls = QFileDialog::getOpenFileUrls(
        nullptr, qtr("Add Files to Convert"),
        QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)),
        QStringLiteral("%1 (%2);;%3 (*)").arg(qtr("Media Files"), qfu(EXTENSIONS_MEDIA).replace(QChar(';'), QChar(' ')), qtr("All Files")));

    QStringList mrls;
    for (const QUrl& url : urls)
        mrls.append(url.toString(QUrl::FullyEncoded));
    return mrls;
}

QString ConversionManager::chooseDestination(const QString& path, const QString& extension) const
{
    return QDir::toNativeSeparators(QFileDialog::getSaveFileName(
        nullptr, qtr("Save Converted File As"), path,
        QStringLiteral("%1 (*.%2);;%3 (*)").arg(qtr("Containers"), extension, qtr("All Files"))));
}

void ConversionManager::convert(const QStringList& mrls, const QString& profileValue,
                                const QString& saveAs, bool deinterlace)
{
    const bool single = mrls.size() == 1 && !saveAs.isEmpty();
    const QString extension = ConversionProfileModel::fileExtension(profileValue);

    for (const QString& mrl : mrls)
    {
        if (single)
        {
            /* the user confirmed the replacement when saving as: the file is
             * written aside, and replaced once done, so that it survives a
             * failed or cancelled conversion */
            const QString output = partPath(saveAs);
            add(mrl, ConversionProfileModel::soutChain(profileValue, output, deinterlace, true),
                saveAs, true, output);
            continue;
        }
        const QString destination = convertedPath(mrl, extension);
        add(mrl, ConversionProfileModel::soutChain(profileValue, destination, deinterlace, false),
            destination, false);
    }
}

QString ConversionManager::partPath(const QString& destination) const
{
    const auto taken = [this](const QString& path) {
        if (QFileInfo::exists(path))
            return true;
        return std::any_of(m_conversions.cbegin(), m_conversions.cend(), [&path](const Conversion& conversion) {
            return isActive(conversion.state) && conversion.output == path;
        });
    };

    QString path = destination + QStringLiteral(".part");
    for (int i = 2; taken(path); i++)
        path = QStringLiteral("%1.%2.part").arg(destination).arg(i);
    return path;
}

void ConversionManager::add(const QString& mrl, const QString& soutChain,
                            const QString& destination, bool overwrite,
                            const QString& output)
{
    Conversion conversion;
    conversion.mrl = mrl;
    conversion.sout = soutChain;
    conversion.destination = destination;
    conversion.output = output.isEmpty() ? destination : output;
    conversion.overwrite = overwrite;

    const QUrl source(mrl);
    if (soutChain.isEmpty() || destination.isEmpty()
        || (source.isLocalFile() && QFileInfo(source.toLocalFile()) == QFileInfo(destination)))
        conversion.state = Failed;

    const int row = m_conversions.size();
    beginInsertRows({}, row, row);
    m_conversions.append(conversion);
    endInsertRows();
    emit countChanged();
    if (conversion.state == Pending)
        emit activeCountChanged();
    emit progressChanged();

    startNext();
}

void ConversionManager::setState(int row, State state)
{
    Conversion& conversion = m_conversions[row];
    if (conversion.state == state)
        return;
    const bool wasActive = isActive(conversion.state);
    conversion.state = state;
    if (state == Done)
        conversion.progress = 1.;
    emit dataChanged(index(row), index(row), { STATE, PROGRESS });
    if (wasActive != isActive(state))
        emit activeCountChanged();
    emit progressChanged();
}

void ConversionManager::setRunningProgress(double progress)
{
    const int row = runningRow();
    if (row < 0)
        return;
    m_conversions[row].progress = progress;
    emit dataChanged(index(row), index(row), { PROGRESS });
    emit progressChanged();
}

void ConversionManager::startNext()
{
    if (runningRow() >= 0)
        return;

    const auto it = std::find_if(m_conversions.begin(), m_conversions.end(),
                                 [](const Conversion& conversion) { return conversion.state == Pending; });
    if (it == m_conversions.end())
        return;
    const int row = std::distance(m_conversions.begin(), it);
    Conversion& conversion = *it;

    conversion.existed = QFileInfo::exists(conversion.destination);
    if ((conversion.existed && !conversion.overwrite) || !ensurePlayer())
    {
        setState(row, Failed);
        startNext();
        return;
    }

    input_item_t* item = input_item_New(qtu(conversion.mrl), nullptr);
    if (!item)
    {
        setState(row, Failed);
        startNext();
        return;
    }
    input_item_AddOption(item, qtu(QStringLiteral("sout=") + conversion.sout),
                         VLC_INPUT_OPTION_TRUSTED);

    m_error = false;
    m_cancelRequested = false;

    vlc_player_Lock(m_player);
    int ret = vlc_player_SetCurrentMedia(m_player, item);
    if (ret == VLC_SUCCESS)
        ret = vlc_player_Start(m_player);
    vlc_player_Unlock(m_player);
    input_item_Release(item);

    msg_Dbg(m_parent, "converting %s with %s", qtu(conversion.mrl), qtu(conversion.sout));

    if (ret != VLC_SUCCESS)
    {
        setState(row, Failed);
        startNext();
        return;
    }
    setState(row, Running);
}

void ConversionManager::finishRunning()
{
    const int row = runningRow();
    if (row < 0)
        return;

    const Conversion& conversion = m_conversions[row];
    State state = Done;
    if (m_cancelRequested)
        state = Cancelled;
    else if (m_error || QFileInfo(conversion.output).size() <= 0)
        state = Failed;
    else if (conversion.output != conversion.destination)
    {
        /* done: the written file replaces the destination */
        if (QFileInfo::exists(conversion.destination)
            && !QFile::remove(conversion.destination))
            state = Failed;
        else if (!QFile::rename(conversion.output, conversion.destination))
            state = Failed;
    }

    if (state != Done)
        removeOutput(conversion);

    setState(row, state);
    startNext();
}

void ConversionManager::cancel(int row)
{
    if (row < 0 || row >= m_conversions.size())
        return;

    switch (m_conversions[row].state)
    {
    case Pending:
        setState(row, Cancelled);
        break;
    case Running:
        m_cancelRequested = true;
        vlc_player_Lock(m_player);
        vlc_player_Stop(m_player);
        vlc_player_Unlock(m_player);
        break;
    default:
        break;
    }
}

void ConversionManager::cancelAll()
{
    for (int i = m_conversions.size() - 1; i >= 0; i--)
        cancel(i);
}

void ConversionManager::clearFinished()
{
    for (int i = m_conversions.size() - 1; i >= 0; i--)
    {
        if (isActive(m_conversions[i].state))
            continue;
        beginRemoveRows({}, i, i);
        m_conversions.removeAt(i);
        endRemoveRows();
    }
    emit countChanged();
    emit progressChanged();
}
