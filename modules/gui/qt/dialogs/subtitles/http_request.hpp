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
#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interrupt.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

/**
 * A small blocking HTTP/1.1 client over VLC's sockets and TLS, for the REST
 * calls of the interface (the Qt module doesn't link Qt Network).
 *
 * It blocks the calling thread: run it on a worker thread. The call is
 * interrupted by vlc_interrupt_kill() on the interrupt context of the thread.
 */
struct HttpRequest
{
    QByteArray method = "GET";
    QUrl url;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    // redirections followed, for GET only
    int maxRedirects = 5;
    // larger bodies fail
    qint64 maxSize = 16 * 1024 * 1024;
    // the whole request, redirections included, fails after this time
    vlc_tick_t timeout = VLC_TICK_FROM_SEC(60);
};

struct HttpResponse
{
    // 0 when no response came
    int status = 0;
    QByteArray body;
    // why no response came
    QString error;

    bool ok() const { return status >= 200 && status < 300; }
};

HttpResponse httpRequest(vlc_object_t *obj, const HttpRequest &request);

/**
 * A thread running blocking tasks (such as httpRequest()) one by one.
 * cancel() drops the pending tasks and interrupts the running one.
 */
class HttpWorker
{
public:
    HttpWorker();
    // cancels, then waits for the running task
    ~HttpWorker();

    HttpWorker(const HttpWorker &) = delete;
    HttpWorker &operator=(const HttpWorker &) = delete;

    void post(std::function<void()> task);
    void cancel();
    // cancel, then wait for the thread to end; nothing runs afterwards
    void stop();

    /* bumped by cancel(): a task delivers its result only if the
     * generation didn't change since it was posted */
    unsigned generation() const { return m_generation.load(); }

private:
    void run();

    std::mutex m_lock;
    std::condition_variable m_wait;
    std::deque<std::function<void()>> m_tasks;
    vlc_interrupt_t *m_current = nullptr;
    bool m_stop = false;
    std::atomic<unsigned> m_generation{ 0 };
    std::thread m_thread;
};

#endif // HTTP_REQUEST_HPP
