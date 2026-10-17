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

#ifndef QVLC_MESSAGES_MODEL_HPP_
#define QVLC_MESSAGES_MODEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <atomic>
#include <deque>
#include <vector>

#include <QAbstractListModel>
#include <QMutex>

#include "qt.hpp"
#include "util/singleton.hpp"

/**
 * The Messages log.
 *
 * It listens to every message of the instance from its creation, in addition
 * to the configured log, and keeps the last ones in a ring buffer. The rows
 * are the messages that pass the verbosity and the search pattern.
 *
 * Errors, warnings and info are always kept, so an error notice's Details
 * show what came before. Debug messages are many, from busy threads: they are
 * only formatted and kept while the verbosity is Debug.
 */
class MessagesModel : public QAbstractListModel, public Singleton<MessagesModel>
{
    Q_OBJECT

    Q_PROPERTY(Verbosity verbosity READ verbosity WRITE setVerbosity NOTIFY verbosityChanged FINAL)
    Q_PROPERTY(QString searchPattern READ searchPattern WRITE setSearchPattern NOTIFY searchPatternChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Roles
    {
        TYPE = Qt::UserRole + 1, //< VLC_MSG_* value
        MODULE,
        TEXT,
        LINE //< "module type: text", as copied and saved
    };
    Q_ENUM(Roles)

    /// Most verbose level shown, each level includes the previous ones
    enum Verbosity
    {
        Errors,
        Warnings,
        Info,
        Debug
    };
    Q_ENUM(Verbosity)

    explicit MessagesModel(qt_intf_t *intf, size_t capacity = 10000, QObject *parent = nullptr);
    ~MessagesModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QHash<int, QByteArray> roleNames() const override;

    Verbosity verbosity() const { return m_verbosity; }
    void setVerbosity(Verbosity verbosity);

    QString searchPattern() const { return m_searchPattern; }
    void setSearchPattern(const QString &pattern);

    int count() const { return rowCount(); }

    /// The visible lines, one per message
    Q_INVOKABLE QString text() const;
    /// Copies the visible lines to the clipboard
    Q_INVOKABLE void copy() const;
    /// Writes the visible lines to a file
    Q_INVOKABLE bool save(const QString &path) const;
    /// Forgets every message received so far
    Q_INVOKABLE void clear();

signals:
    void verbosityChanged();
    void searchPatternChanged();
    void countChanged();

private:
    struct Message
    {
        int type;
        QString module;
        QString text;
        QString line;
    };

    static void onLog(void *data, int type, const vlc_log_t *item,
                      const char *format, va_list ap);

    void flush();
    bool accepts(const Message &message) const;
    const Message &messageAt(int row) const;
    void refilter();

    qt_intf_t *m_intf;
    const size_t m_capacity;

    std::deque<Message> m_messages;
    /// sequence number of m_messages.front()
    quint64 m_firstSeq = 0;
    /// sequence numbers of the visible messages, in order
    std::vector<quint64> m_visible;

    Verbosity m_verbosity = Info;
    /// whether debug messages are kept, read from the logging threads
    std::atomic<bool> m_keepDebug { false };
    QString m_searchPattern;

    /// messages received from any thread, not yet in the model
    QMutex m_pendingLock;
    std::deque<Message> m_pending;
    bool m_flushScheduled = false;
};

#endif
