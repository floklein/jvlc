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

#include "disc_model.hpp"

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_url.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>

#ifdef _WIN32
# include <windows.h>
#elif defined( __linux__ )
# include <fcntl.h>
# include <unistd.h>
# include <sys/ioctl.h>
# include <linux/cdrom.h>
#endif

#include <memory>

namespace {

bool hasEntry(const QDir &dir, const QString &name, QDir::Filters filters)
{
    // discs are read on case-sensitive file systems too: match any case
    return !dir.entryList({ name }, filters).isEmpty();
}

#ifdef _WIN32

QVector<DiscModel::Drive> listDrives()
{
    QVector<DiscModel::Drive> drives;

    wchar_t names[512];
    names[0] = L'\0';
    if (!GetLogicalDriveStringsW(ARRAY_SIZE(names) - 1, names))
        return drives;

    // don't ask the user to insert a disc
    DWORD oldMode;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, &oldMode);

    for (const wchar_t *name = names; *name; name += wcslen(name) + 1)
    {
        if (GetDriveTypeW(name) != DRIVE_CDROM)
            continue;

        DiscModel::Drive drive;
        drive.device = QString::fromWCharArray(name);

        wchar_t label[MAX_PATH + 1] = L"";
        if (GetVolumeInformationW(name, label, ARRAY_SIZE(label), nullptr, nullptr,
                                  nullptr, nullptr, 0))
        {
            drive.root = drive.device;
            drive.label = QString::fromWCharArray(label);
        }
        else
            drive.type = DiscModel::NO_DISC;

        drives.push_back(drive);
    }

    SetThreadErrorMode(oldMode, nullptr);
    return drives;
}

#elif !defined( __OS2__ )

#ifdef __linux__
// the mount point of each mounted device
QHash<QString, QString> mountPoints()
{
    QHash<QString, QString> mounts;

    QFile file(QStringLiteral("/proc/self/mounts"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return mounts;

    // "device mountpoint type options 0 0", with spaces escaped as \040
    static const auto unescape = [](QByteArray field) {
        QByteArray out;
        for (qsizetype i = 0; i < field.size(); ++i)
        {
            if (field[i] == '\\' && i + 3 < field.size())
            {
                bool ok;
                const int c = field.mid(i + 1, 3).toInt(&ok, 8);
                if (ok)
                {
                    out += char(c);
                    i += 3;
                    continue;
                }
            }
            out += field[i];
        }
        return QString::fromUtf8(out);
    };

    while (!file.atEnd())
    {
        const QList<QByteArray> fields = file.readLine().split(' ');
        if (fields.size() < 2)
            continue;
        const QString device = QFileInfo(unescape(fields[0])).canonicalFilePath();
        if (!device.isEmpty())
            mounts.insert(device, unescape(fields[1]));
    }
    return mounts;
}
#endif

QVector<DiscModel::Drive> listDrives()
{
    QVector<DiscModel::Drive> drives;

    const QStringList names = QDir(QStringLiteral("/dev"))
            .entryList({ QStringLiteral("sr*") }, QDir::System, QDir::Name);
#ifdef __linux__
    const QHash<QString, QString> mounts = mountPoints();
#endif

    for (const QString &name : names)
    {
        DiscModel::Drive drive;
        drive.device = QStringLiteral("/dev/") + name;

#ifdef __linux__
        const int fd = vlc_open(qtu(drive.device), O_RDONLY | O_NONBLOCK);
        if (fd != -1)
        {
            const int status = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
            if (status == CDS_NO_DISC || status == CDS_TRAY_OPEN)
                drive.type = DiscModel::NO_DISC;
            else if (status == CDS_DISC_OK)
            {
                const int disc = ioctl(fd, CDROM_DISC_STATUS, 0);
                if (disc == CDS_AUDIO || disc == CDS_MIXED)
                    drive.type = DiscModel::AUDIO_CD;
                else if (disc == CDS_NO_INFO || disc == CDS_NO_DISC)
                    drive.type = DiscModel::NO_DISC;
            }
            vlc_close(fd);
        }

        const auto mount = mounts.constFind(drive.device);
        if (mount != mounts.constEnd())
        {
            drive.root = *mount;
            drive.label = QFileInfo(*mount).fileName();
        }
#endif
        drives.push_back(drive);
    }
    return drives;
}

#else

QVector<DiscModel::Drive> listDrives()
{
    return {};
}

#endif

} // anonymous namespace

DiscModel::DiscModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_lister(&DiscModel::systemDrives)
{
}

void DiscModel::setDriveLister(DriveLister lister)
{
    m_lister = std::move(lister);
}

QVector<DiscModel::Drive> DiscModel::systemDrives()
{
    return listDrives();
}

DiscModel::Type DiscModel::detectType(const QString &root)
{
    const QDir dir(root);
    if (root.isEmpty() || !dir.exists())
        return NO_DISC;

    const QDir::Filters dirs = QDir::Dirs | QDir::NoDotAndDotDot;
    if (hasEntry(dir, QStringLiteral("BDMV"), dirs))
        return BLURAY;
    if (hasEntry(dir, QStringLiteral("VIDEO_TS"), dirs))
        return DVD;
    if (hasEntry(dir, QStringLiteral("MPEGAV"), dirs) || hasEntry(dir, QStringLiteral("VCD"), dirs)
        || hasEntry(dir, QStringLiteral("SVCD"), dirs))
        return VCD;
    // Windows shows the tracks of an Audio CD as .cda files
    if (hasEntry(dir, QStringLiteral("*.cda"), QDir::Files))
        return AUDIO_CD;
    return UNKNOWN;
}

QString DiscModel::typeName(Type type)
{
    switch (type)
    {
    case DVD:      return qtr("DVD");
    case BLURAY:   return qtr("Blu-ray");
    case AUDIO_CD: return qtr("Audio CD");
    case VCD:      return qtr("VCD");
    case UNKNOWN:  return qtr("Unrecognized disc");
    case NO_DISC:  return qtr("No disc");
    }
    return {};
}

vlc::playlist::Media DiscModel::media(const QString &device, Type type, bool useMenus,
                                      int title, int chapter,
                                      int audioTrack, int subtitleTrack,
                                      const QString &name)
{
    const char *scheme;
    switch (type)
    {
    case DVD:      scheme = useMenus ? "dvd" : "dvdsimple"; break;
    case BLURAY:   scheme = "bluray"; break;
    case AUDIO_CD: scheme = "cdda"; break;
    case VCD:      scheme = "vcd"; break;
    default:
        return {};
    }

    char *uri = vlc_path2uri(qtu(device), scheme);
    if (unlikely(uri == nullptr))
        return {};
    QString mrl = qfu(uri);
    free(uri);

    QStringList options;

    /* starting title and chapter, as an MRL fragment */
    if (title > 0 && type != AUDIO_CD)
    {
        mrl += QStringLiteral("#%1").arg(title);
        if (chapter > 0 && type != VCD)
            mrl += QStringLiteral(":%1").arg(chapter);
    }

    if (type == BLURAY)
        options << (useMenus ? QStringLiteral(":bluray-menu") : QStringLiteral(":no-bluray-menu"));

    if (type == AUDIO_CD)
    {
        if (title > 0)
            options << QStringLiteral(":cdda-track=%1").arg(title);
    }
    else
    {
        /* the track options count from 0 */
        if (audioTrack > 0)
            options << QStringLiteral(":audio-track=%1").arg(audioTrack - 1);
        if (subtitleTrack > 0)
            options << QStringLiteral(":sub-track=%1").arg(subtitleTrack - 1);
    }

    return vlc::playlist::Media(mrl, name.isEmpty() ? typeName(type) : name, options);
}

void DiscModel::refresh()
{
    if (m_scanning)
    {
        // list again once the current scan is done
        m_rescan = true;
        return;
    }

    m_scanning = true;
    emit scanningChanged();

    auto promise = std::make_shared<QPromise<QVector<Drive>>>();
    auto watcher = new QFutureWatcher<QVector<Drive>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        watcher->deleteLater();
        setDrives(watcher->future().resultCount() > 0 ? watcher->result() : QVector<Drive>{});

        if (m_rescan)
        {
            m_rescan = false;
            m_scanning = false;
            refresh();
            return;
        }
        m_scanning = false;
        emit scanningChanged();
    });
    watcher->setFuture(promise->future());

    // reading the drives may spin them up: keep it off the GUI thread
    QThreadPool::globalInstance()->start([promise, lister = m_lister]() {
        promise->start();
        QVector<Drive> drives = lister ? lister() : QVector<Drive>{};
        for (Drive &drive : drives)
        {
            if (drive.type == UNKNOWN && !drive.root.isEmpty())
            {
                const Type type = detectType(drive.root);
                // a readable root holds a disc
                drive.type = (type == NO_DISC) ? UNKNOWN : type;
            }
        }
        promise->addResult(std::move(drives));
        promise->finish();
    });
}

void DiscModel::setDrives(QVector<Drive> drives)
{
    const int oldDefault = defaultIndex();

    beginResetModel();
    m_drives = std::move(drives);
    endResetModel();

    if (defaultIndex() != oldDefault)
        emit defaultIndexChanged();
}

bool DiscModel::play(int row, Type type, bool useMenus,
                     int title, int chapter, int audioTrack, int subtitleTrack)
{
    if (!m_playlistController || row < 0 || row >= m_drives.size())
        return false;

    const Drive &drive = m_drives[row];
    if (drive.type == NO_DISC)
        return false;

    vlc::playlist::Media disc = media(drive.device, type, useMenus, title, chapter,
                                      audioTrack, subtitleTrack, drive.label);
    if (!disc)
        return false;

    /* the Play rule */
    m_playlistController->playMedia(QVector<vlc::playlist::Media>{ disc });
    return true;
}

int DiscModel::defaultIndex() const
{
    for (int i = 0; i < m_drives.size(); ++i)
        if (m_drives[i].type != NO_DISC)
            return i;
    return m_drives.isEmpty() ? -1 : 0;
}

void DiscModel::setPlaylistController(vlc::playlist::PlaylistController *controller)
{
    if (m_playlistController == controller)
        return;
    m_playlistController = controller;
    emit playlistControllerChanged();
}

int DiscModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_drives.size();
}

QVariant DiscModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Drive &drive = m_drives[index.row()];
    switch (role)
    {
    case Qt::DisplayRole:
    case NAME:
    {
        QString device = drive.device;
#ifdef _WIN32
        // "D:\" reads better as "D:"
        if (device.endsWith('\\'))
            device.chop(1);
#endif
        if (drive.label.isEmpty())
            return device;
        return QStringLiteral("%1  %2").arg(device, drive.label);
    }
    case DEVICE:
        return drive.device;
    case LABEL:
        return drive.label;
    case TYPE:
        return static_cast<int>(drive.type);
    case TYPE_NAME:
        return typeName(drive.type);
    default:
        return {};
    }
}

QHash<int, QByteArray> DiscModel::roleNames() const
{
    return {
        { NAME, "name" },
        { DEVICE, "device" },
        { LABEL, "label" },
        { TYPE, "type" },
        { TYPE_NAME, "typeName" },
    };
}
