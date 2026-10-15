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

#include <vlc_common.h>
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

/* the longest status, header or chunk-size line */
constexpr qsizetype maxLineLength = 8 * 1024;
/* the most bytes of headers in one response */
constexpr qsizetype maxHeaderBytes = 64 * 1024;

/* a line without its CRLF; fails past maxLineLength */
QByteArray readLine(vlc_tls_t *tls, bool &ok)
{
    QByteArray line;
    ok = false;
    for (;;)
    {
        char c;
        if (vlc_tls_Read(tls, &c, 1, false) != 1)
            return {};
        if (c == '\n')
            break;
        if (line.size() >= maxLineLength)
            return {};
        line.append(c);
    }
    if (line.endsWith('\r'))
        line.chop(1);
    ok = true;
    return line;
}

/* count must be within what the body may still grow by */
bool readBytes(vlc_tls_t *tls, qint64 count, QByteArray &body)
{
    const qsizetype start = body.size();
    body.resize(start + count);
    return vlc_tls_Read(tls, body.data() + start, count, true) == count;
}

bool sameOrigin(const QUrl &a, const QUrl &b)
{
    return a.scheme() == b.scheme()
        && a.host().compare(b.host(), Qt::CaseInsensitive) == 0
        && a.port(a.scheme() == QLatin1String("https") ? 443 : 80)
           == b.port(b.scheme() == QLatin1String("https") ? 443 : 80);
}

/* the headers that carry credentials: never sent to another origin */
bool isCredential(const QByteArray &name)
{
    const QByteArray lower = name.toLower();
    return lower == "authorization" || lower == "api-key" || lower == "cookie"
        || lower == "proxy-authorization";
}

/**
 * Interrupts the blocking calls of the thread after a timeout, or when the
 * thread's own interrupt context is interrupted.
 */
class Deadline
{
public:
    explicit Deadline(vlc_tick_t timeout)
        : m_context(vlc_interrupt_create())
    {
        if (m_context == nullptr)
            return;
        vlc_interrupt_forward_start(m_context, m_forward);
        m_previous = vlc_interrupt_set(m_context);
        m_timer = vlc_timer_create(&m_timerId, [](void *context) {
            vlc_interrupt_kill(static_cast<vlc_interrupt_t *>(context));
        }, m_context) == 0;
        if (m_timer)
            vlc_timer_schedule(m_timerId, false, timeout, VLC_TIMER_FIRE_ONCE);
    }

    ~Deadline()
    {
        if (m_context == nullptr)
            return;
        if (m_timer)
            vlc_timer_destroy(m_timerId);
        vlc_interrupt_set(m_previous);
        vlc_interrupt_forward_stop(m_forward);
        vlc_interrupt_destroy(m_context);
    }

    Deadline(const Deadline &) = delete;
    Deadline &operator=(const Deadline &) = delete;

private:
    vlc_interrupt_t *m_context;
    vlc_interrupt_t *m_previous = nullptr;
    void *m_forward[2] = { nullptr, nullptr };
    vlc_timer_t m_timerId;
    bool m_timer = false;
};

} // anonymous namespace

HttpResponse httpRequest(vlc_object_t *obj, const HttpRequest &request)
{
    HttpResponse response;
    QUrl url = request.url;
    QByteArray method = request.method;
    QByteArray body = request.body;
    /* the credentials go to the origin of the request only */
    bool credentials = true;

    const Deadline deadline(request.timeout);
    auto fail = [&response](const QString &error) {
        response.status = 0;
        response.error = vlc_killed() ? QStringLiteral("timed out or canceled") : error;
        return response;
    };

    for (int redirects = 0;; ++redirects)
    {
        Connection conn;
        if (!open(obj, url, conn, response.error))
            return fail(response.error);

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
            if (credentials || !isCredential(header.first))
                head += header.first + ": " + header.second + "\r\n";
        if (!body.isEmpty() || method == "POST" || method == "PUT")
            head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        head += "\r\n";

        if (!writeAll(conn.tls, head + body))
            return fail(QStringLiteral("cannot send the request"));

        /* the status line: "HTTP/1.1 200 OK" */
        bool ok;
        const QByteArray status = readLine(conn.tls, ok);
        const QList<QByteArray> parts = status.split(' ');
        if (!ok || parts.size() < 2 || !parts[0].startsWith("HTTP/"))
            return fail(QStringLiteral("no response"));
        response.status = parts[1].toInt();

        qint64 length = -1;
        bool chunked = false;
        QByteArray location;
        qsizetype headerBytes = 0;
        for (;;)
        {
            const QByteArray line = readLine(conn.tls, ok);
            if (!ok)
                return fail(QStringLiteral("truncated response"));
            headerBytes += line.size() + 2;
            if (headerBytes > maxHeaderBytes)
                return fail(QStringLiteral("response headers too large"));
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
            const QUrl next = url.resolved(QUrl::fromEncoded(location));
            /* never from https down to http */
            if (url.scheme() == QLatin1String("https") && next.scheme() != QLatin1String("https"))
                return fail(QStringLiteral("insecure redirection"));
            if (!sameOrigin(url, next))
                credentials = false;
            url = next;
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
                /* written so that it can't overflow */
                if (!ok || !hex || size < 0
                 || size > request.maxSize - response.body.size())
                    return fail(QStringLiteral("bad chunked response"));
                if (size == 0)
                    break;
                if (!readBytes(conn.tls, size, response.body))
                    return fail(QStringLiteral("truncated response"));
                readLine(conn.tls, ok); /* the CRLF after the chunk */
            }
        }
        else if (length >= 0)
        {
            if (length > request.maxSize || !readBytes(conn.tls, length, response.body))
                return fail(QStringLiteral("truncated response"));
        }
        else
        {
            /* until the server closes the connection */
            char buffer[4096];
            ssize_t read;
            while ((read = vlc_tls_Read(conn.tls, buffer, sizeof (buffer), false)) > 0)
            {
                if (read > request.maxSize - response.body.size())
                    return fail(QStringLiteral("response too large"));
                response.body.append(buffer, read);
            }
            if (vlc_killed())
                return fail(QString());
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
