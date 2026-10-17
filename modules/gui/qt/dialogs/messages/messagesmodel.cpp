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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "messagesmodel.hpp"

#include <vlc_common.h>
#include <vlc_interface.h>

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QTextStream>

#include <algorithm>

namespace {

int verbosityOf(int type)
{
    switch (type)
    {
    case VLC_MSG_ERR:
        return MessagesModel::Errors;
    case VLC_MSG_WARN:
        return MessagesModel::Warnings;
    case VLC_MSG_INFO:
        return MessagesModel::Info;
    case VLC_MSG_DBG:
    default:
        return MessagesModel::Debug;
    }
}

QString typeName(int type)
{
    switch (type)
    {
    case VLC_MSG_ERR:
        return QStringLiteral("error");
    case VLC_MSG_WARN:
        return QStringLiteral("warning");
    case VLC_MSG_INFO:
        return QStringLiteral("info");
    case VLC_MSG_DBG:
    default:
        return QStringLiteral("debug");
    }
}

}

MessagesModel::MessagesModel(qt_intf_t *intf, size_t capacity, QObject *parent)
    : QAbstractListModel(parent)
    , m_intf(intf)
    , m_capacity(std::max<size_t>(capacity, 1))
{
    static const struct vlc_logger_operations ops = {
        MessagesModel::onLog,
        nullptr
    };
    vlc_LogSetListener(vlc_object_instance(m_intf), &ops, this);
}

MessagesModel::~MessagesModel()
{
    /* No callback runs once this returns */
    vlc_LogSetListener(vlc_object_instance(m_intf), nullptr, nullptr);
}

void MessagesModel::onLog(void *data, int type, const vlc_log_t *item,
                          const char *format, va_list ap)
{
    auto self = static_cast<MessagesModel *>(data);

    if (verbosityOf(type) == Debug
     && !self->m_keepDebug.load(std::memory_order_relaxed))
        return;

    char *str;
    if (unlikely(vasprintf(&str, format, ap) == -1))
        return;

    int canc = vlc_savecancel();

    Message message;
    message.type = type;
    message.module = qfu(item->psz_module);
    message.text = QString::fromUtf8(str).trimmed();
    message.line = QStringLiteral("%1 %2: %3").arg(message.module, typeName(type), message.text);
    free(str);

    bool schedule = false;
    {
        QMutexLocker locker(&self->m_pendingLock);
        self->m_pending.push_back(std::move(message));
        if (self->m_pending.size() > self->m_capacity)
            self->m_pending.pop_front();
        if (!self->m_flushScheduled)
        {
            self->m_flushScheduled = true;
            schedule = true;
        }
    }

    if (schedule)
        QMetaObject::invokeMethod(self, &MessagesModel::flush, Qt::QueuedConnection);

    vlc_restorecancel(canc);
}

void MessagesModel::flush()
{
    std::deque<Message> pending;
    {
        QMutexLocker locker(&m_pendingLock);
        pending.swap(m_pending);
        m_flushScheduled = false;
    }

    if (pending.empty())
        return;

    /* Append, then show the new visible messages */
    std::vector<quint64> newVisible;
    for (Message &message : pending)
    {
        const quint64 seq = m_firstSeq + m_messages.size();
        if (accepts(message))
            newVisible.push_back(seq);
        m_messages.push_back(std::move(message));
    }

    if (!newVisible.empty())
    {
        const int first = static_cast<int>(m_visible.size());
        beginInsertRows({}, first, first + static_cast<int>(newVisible.size()) - 1);
        m_visible.insert(m_visible.end(), newVisible.begin(), newVisible.end());
        endInsertRows();
    }

    /* Drop the oldest messages beyond the capacity */
    if (m_messages.size() > m_capacity)
    {
        const size_t dropped = m_messages.size() - m_capacity;
        m_messages.erase(m_messages.begin(), m_messages.begin() + dropped);
        m_firstSeq += dropped;

        const auto firstKept = std::lower_bound(m_visible.begin(), m_visible.end(), m_firstSeq);
        const int removed = static_cast<int>(std::distance(m_visible.begin(), firstKept));
        if (removed > 0)
        {
            beginRemoveRows({}, 0, removed - 1);
            m_visible.erase(m_visible.begin(), firstKept);
            endRemoveRows();
        }
    }

    emit countChanged();
}

bool MessagesModel::accepts(const Message &message) const
{
    if (verbosityOf(message.type) > m_verbosity)
        return false;
    return m_searchPattern.isEmpty()
        || message.line.contains(m_searchPattern, Qt::CaseInsensitive);
}

const MessagesModel::Message &MessagesModel::messageAt(int row) const
{
    return m_messages[m_visible[row] - m_firstSeq];
}

void MessagesModel::refilter()
{
    beginResetModel();
    m_visible.clear();
    for (size_t i = 0; i < m_messages.size(); ++i)
    {
        if (accepts(m_messages[i]))
            m_visible.push_back(m_firstSeq + i);
    }
    endResetModel();
    emit countChanged();
}

QVariant MessagesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const Message &message = messageAt(index.row());
    switch (role)
    {
    case TYPE:
        return message.type;
    case MODULE:
        return message.module;
    case TEXT:
        return message.text;
    case Qt::DisplayRole:
    case LINE:
        return message.line;
    default:
        return {};
    }
}

int MessagesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_visible.size());
}

QHash<int, QByteArray> MessagesModel::roleNames() const
{
    return {
        { TYPE, "type" },
        { MODULE, "module" },
        { TEXT, "text" },
        { LINE, "line" },
    };
}

void MessagesModel::setVerbosity(Verbosity verbosity)
{
    if (m_verbosity == verbosity)
        return;
    m_verbosity = verbosity;
    m_keepDebug.store(verbosity == Debug, std::memory_order_relaxed);
    refilter();
    emit verbosityChanged();
}

void MessagesModel::setSearchPattern(const QString &pattern)
{
    if (m_searchPattern == pattern)
        return;
    m_searchPattern = pattern;
    refilter();
    emit searchPatternChanged();
}

QString MessagesModel::text() const
{
    QStringList lines;
    lines.reserve(rowCount());
    for (int row = 0; row < rowCount(); ++row)
        lines << messageAt(row).line;
    return lines.join('\n');
}

void MessagesModel::copy() const
{
    if (auto app = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        app->clipboard()->setText(text());
}

bool MessagesModel::save(const QString &path) const
{
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
        return false;

    QTextStream out(&file);
    out << text() << '\n';
    out.flush();
    return out.status() == QTextStream::Ok;
}

void MessagesModel::clear()
{
    beginResetModel();
    m_firstSeq += m_messages.size();
    m_messages.clear();
    m_visible.clear();
    endResetModel();
    emit countChanged();
}
