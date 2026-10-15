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
#include "http_request.hpp"

#include <vlc_tls.h>

#include <memory>

namespace {

struct Connection
{
    vlc_tls_client_t *creds = nullptr;
    vlc_tls_t *tls = nullptr;

    ~Connection()
    {
        if (tls != nullptr)
            vlc_tls_Close(tls);
        if (creds != nullptr)
            vlc_tls_ClientDelete(creds);
    }
};

bool open(vlc_object_t *obj, const QUrl &url, Connection &conn, QString &error)
{
    const QByteArray host = url.host(QUrl::FullyEncoded).toLatin1();
    if (url.scheme() == QLatin1String("http"))
    {
        conn.tls = vlc_tls_SocketOpenTCP(obj, host.constData(), url.port(80));
    }
    else if (url.scheme() == QLatin1String("https"))
    {
        conn.creds = vlc_tls_ClientCreate(obj);
        if (conn.creds == nullptr)
        {
            error = QStringLiteral("no TLS support");
            return false;
        }
        static const char *const alpn[] = { "http/1.1", nullptr };
        conn.tls = vlc_tls_SocketOpenTLS(conn.creds, host.constData(), url.port(443),
                                         "https", alpn, nullptr);
    }
    else
    {
        error = QStringLiteral("unsupported scheme");
        return false;
    }
    if (conn.tls == nullptr)
    {
        error = QStringLiteral("cannot connect to %1").arg(url.host());
        return false;
    }
    return true;
}

bool writeAll(vlc_tls_t *tls, const QByteArray &data)
{
    return vlc_tls_Write(tls, data.constData(), data.size()) == data.size();
}

QByteArray readLine(vlc_tls_t *tls, bool &ok)
{
    char *line = vlc_tls_GetLine(tls);
    ok = line != nullptr;
    QByteArray result(line);
    free(line);
    return result;
}

bool readBytes(vlc_tls_t *tls, qint64 count, QByteArray &body)
{
    const qsizetype start = body.size();
    body.resize(start + count);
    return vlc_tls_Read(tls, body.data() + start, count, true) == count;
}

} // anonymous namespace

HttpResponse httpRequest(vlc_object_t *obj, const HttpRequest &request)
{
    HttpResponse response;
    QUrl url = request.url;
    QByteArray method = request.method;
    QByteArray body = request.body;

    for (int redirects = 0;; ++redirects)
    {
        Connection conn;
        if (!open(obj, url, conn, response.error))
            return response;

        QByteArray path = url.path(QUrl::FullyEncoded).toLatin1();
        if (path.isEmpty())
            path = "/";
        if (url.hasQuery())
            path += '?' + url.query(QUrl::FullyEncoded).toLatin1();

        QByteArray host = url.host(QUrl::FullyEncoded).toLatin1();
        if (url.port() != -1)
            host += ':' + QByteArray::number(url.port());

        QByteArray head = method + ' ' + path + " HTTP/1.1\r\n"
                        + "Host: " + host + "\r\n"
                        + "Connection: close\r\n"
                        + "Accept-Encoding: identity\r\n";
        for (const auto &header : request.headers)
            head += header.first + ": " + header.second + "\r\n";
        if (!body.isEmpty() || method == "POST" || method == "PUT")
            head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        head += "\r\n";

        if (!writeAll(conn.tls, head + body))
        {
            response.error = QStringLiteral("cannot send the request");
            return response;
        }

        /* the status line: "HTTP/1.1 200 OK" */
        bool ok;
        const QByteArray status = readLine(conn.tls, ok);
        const QList<QByteArray> parts = status.split(' ');
        if (!ok || parts.size() < 2 || !parts[0].startsWith("HTTP/"))
        {
            response.error = QStringLiteral("no response");
            return response;
        }
        response.status = parts[1].toInt();

        qint64 length = -1;
        bool chunked = false;
        QByteArray location;
        for (;;)
        {
            const QByteArray line = readLine(conn.tls, ok);
            if (!ok)
            {
                response.status = 0;
                response.error = QStringLiteral("truncated response");
                return response;
            }
            if (line.isEmpty())
                break;
            const qsizetype colon = line.indexOf(':');
            if (colon < 0)
                continue;
            const QByteArray name = line.left(colon).trimmed().toLower();
            const QByteArray value = line.mid(colon + 1).trimmed();
            if (name == "content-length")
                length = value.toLongLong();
            else if (name == "transfer-encoding")
                chunked = value.toLower().contains("chunked");
            else if (name == "location")
                location = value;
        }

        const bool redirect = response.status == 301 || response.status == 302
                           || response.status == 303 || response.status == 307
                           || response.status == 308;
        if (redirect && !location.isEmpty() && redirects < request.maxRedirects
         && (method == "GET" || response.status == 303))
        {
            url = url.resolved(QUrl::fromEncoded(location));
            method = "GET";
            body.clear();
            continue;
        }

        response.body.clear();
        if (method == "HEAD" || response.status == 204 || response.status == 304)
            return response;

        if (chunked)
        {
            for (;;)
            {
                const QByteArray sizeLine = readLine(conn.tls, ok);
                bool hex;
                const qint64 size = sizeLine.split(';').first().trimmed().toLongLong(&hex, 16);
                if (!ok || !hex || size < 0
                 || response.body.size() + size > request.maxSize)
                {
                    response.status = 0;
                    response.error = QStringLiteral("bad chunked response");
                    return response;
                }
                if (size == 0)
                    break;
                if (!readBytes(conn.tls, size, response.body))
                {
                    response.status = 0;
                    response.error = QStringLiteral("truncated response");
                    return response;
                }
                readLine(conn.tls, ok); /* the CRLF after the chunk */
            }
        }
        else if (length >= 0)
        {
            if (length > request.maxSize || !readBytes(conn.tls, length, response.body))
            {
                response.status = 0;
                response.error = QStringLiteral("truncated response");
                return response;
            }
        }
        else
        {
            /* until the server closes the connection */
            char buffer[4096];
            ssize_t read;
            while ((read = vlc_tls_Read(conn.tls, buffer, sizeof (buffer), false)) > 0)
            {
                if (response.body.size() + read > request.maxSize)
                {
                    response.status = 0;
                    response.error = QStringLiteral("response too large");
                    return response;
                }
                response.body.append(buffer, read);
            }
        }
        return response;
    }
}

HttpWorker::HttpWorker()
    : m_thread([this] { run(); })
{
}

HttpWorker::~HttpWorker()
{
    stop();
}

void HttpWorker::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_stop = true;
    }
    cancel();
    m_wait.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void HttpWorker::post(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_tasks.push_back(std::move(task));
    }
    m_wait.notify_one();
}

void HttpWorker::cancel()
{
    std::lock_guard<std::mutex> lock(m_lock);
    ++m_generation;
    m_tasks.clear();
    if (m_current != nullptr)
        vlc_interrupt_kill(m_current);
}

void HttpWorker::run()
{
    for (;;)
    {
        std::function<void()> task;
        vlc_interrupt_t *ctx = vlc_interrupt_create();
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_wait.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
            if (m_stop || ctx == nullptr)
            {
                if (ctx != nullptr)
                    vlc_interrupt_destroy(ctx);
                return;
            }
            task = std::move(m_tasks.front());
            m_tasks.pop_front();
            m_current = ctx;
        }

        vlc_interrupt_t *previous = vlc_interrupt_set(ctx);
        task();
        vlc_interrupt_set(previous);

        {
            std::lock_guard<std::mutex> lock(m_lock);
            m_current = nullptr;
        }
        vlc_interrupt_destroy(ctx);
    }
}
