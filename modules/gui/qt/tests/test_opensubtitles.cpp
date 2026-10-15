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

/* Add Subtitles from OpenSubtitles and From Link…, against a local mock of
 * the OpenSubtitles REST API: never the real service. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "vlc_stub_modules.hpp"

#include <vlc_interrupt.h>
#include <vlc_network.h>
#include <vlc_tls.h>
#include <vlc_url.h>
#include <vlc_variables.h>

#include "../dialogs/subtitles/add_subtitles_model.hpp"
#include "../dialogs/subtitles/opensubtitles.hpp"
#include "../dialogs/subtitles/opensubtitles_models.hpp"
#include "../medialibrary/medialib.hpp"
#include "../player/player_controller.hpp"
#include "../playlist/playlist_controller.hpp"

#include <functional>
#include <memory>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSettings>
#include <QSignalSpy>
#include <QStringList>
#include <mutex>
#include <thread>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

using vlc::playlist::PlaylistController;
using namespace opensubtitles;

static QString toMrl(const QString &path)
{
    char *uri = vlc_path2uri(qtu(QDir::toNativeSeparators(path)), "file");
    QString mrl = qfu(uri);
    free(uri);
    return mrl;
}

/* a .zip holding these files, stored without compression */
static QByteArray makeZip(const QList<QPair<QByteArray, QByteArray>> &files)
{
    auto crc32 = [](const QByteArray &data) {
        quint32 crc = 0xFFFFFFFFu;
        for (char c : data)
        {
            crc ^= static_cast<uchar>(c);
            for (int k = 0; k < 8; ++k)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return ~crc;
    };
    auto le16 = [](QByteArray &out, quint16 v) {
        out.append(char(v & 0xFF)).append(char(v >> 8));
    };
    auto le32 = [&le16](QByteArray &out, quint32 v) {
        le16(out, v & 0xFFFF);
        le16(out, v >> 16);
    };

    QByteArray zip, central;
    for (const auto &file : files)
    {
        const quint32 offset = zip.size();
        const quint32 crc = crc32(file.second);
        le32(zip, 0x04034b50);
        le16(zip, 10); le16(zip, 0); le16(zip, 0); le16(zip, 0); le16(zip, 0x21);
        le32(zip, crc); le32(zip, file.second.size()); le32(zip, file.second.size());
        le16(zip, file.first.size()); le16(zip, 0);
        zip += file.first;
        zip += file.second;

        le32(central, 0x02014b50);
        le16(central, 20); le16(central, 10); le16(central, 0); le16(central, 0);
        le16(central, 0); le16(central, 0x21);
        le32(central, crc); le32(central, file.second.size()); le32(central, file.second.size());
        le16(central, file.first.size()); le16(central, 0); le16(central, 0);
        le16(central, 0); le16(central, 0); le32(central, 0); le32(central, offset);
        central += file.first;
    }
    const quint32 centralOffset = zip.size();
    zip += central;
    le32(zip, 0x06054b50);
    le16(zip, 0); le16(zip, 0);
    le16(zip, files.size()); le16(zip, files.size());
    le32(zip, central.size()); le32(zip, centralOffset);
    le16(zip, 0);
    return zip;
}

static QByteArray srt(const QByteArray &text)
{
    return "1\n00:00:00,000 --> 00:10:00,000\n" + text + "\n\n";
}

/**
 * A local HTTP server playing the OpenSubtitles REST API (/api/v1/...), its
 * download links (/files/<id>.srt) and plain files for From Link…
 * (/links/<name>).
 */
class MockApi
{
public:
    struct Request
    {
        QByteArray method;
        QString path;
        QUrlQuery query;
        QMap<QByteArray, QByteArray> headers;
        QJsonObject json;
    };

    /* the results of GET /subtitles for a request */
    std::function<QJsonArray(const Request &)> subtitles;
    /* the downloads left; POST /download refuses at 0 */
    int remaining = 5;
    int requests = 0;
    /* the account of POST /login */
    QString user = QStringLiteral("jane");
    QString password = QStringLiteral("secret");
    int userAllowed = 20;
    int userRemaining = 19;
    /* plain files under /links/ */
    QMap<QString, QByteArray> links;

    explicit MockApi(vlc_object_t *obj)
        : m_obj(obj)
    {
        m_fds = net_ListenTCP(obj, "127.0.0.1", 0);
        if (m_fds == nullptr)
            return;
        struct sockaddr_storage addr;
        socklen_t len = sizeof (addr);
        if (getsockname(m_fds[0], reinterpret_cast<struct sockaddr *>(&addr), &len) == 0)
            m_port = ntohs(reinterpret_cast<struct sockaddr_in *>(&addr)->sin_port);
        m_interrupt = vlc_interrupt_create();
        m_thread = std::thread([this] { run(); });
    }

    ~MockApi()
    {
        if (m_thread.joinable())
        {
            vlc_interrupt_kill(m_interrupt);
            m_thread.join();
        }
        if (m_interrupt != nullptr)
            vlc_interrupt_destroy(m_interrupt);
        if (m_fds != nullptr)
            net_ListenClose(m_fds);
    }

    bool listening() const { return m_port != 0; }

    QString base() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_port);
    }
    QString apiUrl() const { return base() + QStringLiteral("/api/v1"); }

    QVector<Request> received() const
    {
        std::lock_guard<std::mutex> lock(m_lock);
        return m_received;
    }

    int count(const QByteArray &method, const QString &path) const
    {
        const QVector<Request> all = received();
        return std::count_if(all.cbegin(), all.cend(), [&](const Request &r) {
            return r.method == method && r.path == path;
        });
    }

    /* the last such request; an empty method if none */
    Request last(const QByteArray &method, const QString &path) const
    {
        const QVector<Request> all = received();
        for (auto it = all.crbegin(); it != all.crend(); ++it)
            if (it->method == method && it->path == path)
                return *it;
        return {};
    }

    static QJsonObject result(int fileId, const QString &release, const QString &language,
                              int downloads, bool exact, int season = -1, int episode = -1)
    {
        QJsonObject feature;
        if (season >= 0)
            feature.insert("season_number", season);
        if (episode >= 0)
            feature.insert("episode_number", episode);
        QJsonObject file{ { "file_id", fileId }, { "cd_number", 1 },
                          { "file_name", release + ".srt" } };
        QJsonObject attributes{
            { "subtitle_id", QString::number(fileId * 10) },
            { "language", language },
            { "download_count", downloads },
            { "hearing_impaired", false },
            { "release", release },
            { "moviehash_match", exact },
            { "files", QJsonArray{ file } },
            { "feature_details", feature },
        };
        return QJsonObject{ { "id", QString::number(fileId * 10) }, { "type", "subtitle" },
                            { "attributes", attributes } };
    }

private:
    struct Reply
    {
        int status = 200;
        QByteArray body;
        bool chunked = false;
    };

    /* the server thread: one request per connection */
    void run()
    {
        vlc_interrupt_set(m_interrupt);
        for (;;)
        {
            /* interrupted by the destructor */
            const int fd = vlc_accept_i11e(m_fds[0], nullptr, nullptr, false);
            if (fd < 0)
            {
                if (vlc_killed())
                    break;
                continue;
            }
            vlc_tls_t *socket = vlc_tls_SocketOpen(fd);
            if (socket == nullptr)
            {
                net_Close(fd);
                continue;
            }
            serve(socket);
            vlc_tls_Close(socket);
        }
        vlc_interrupt_set(nullptr);
    }

    void serve(vlc_tls_t *socket)
    {
        auto readLine = [socket](bool &ok) {
            char *line = vlc_tls_GetLine(socket);
            ok = line != nullptr;
            QByteArray result(line);
            free(line);
            return result;
        };

        bool ok;
        const QList<QByteArray> first = readLine(ok).split(' ');
        if (!ok)
            return;
        Request request;
        request.method = first.value(0);
        const QUrl url(QString::fromLatin1(first.value(1)));
        request.path = url.path();
        request.query = QUrlQuery(url);
        qsizetype length = 0;
        for (;;)
        {
            const QByteArray line = readLine(ok);
            if (!ok)
                return;
            if (line.isEmpty())
                break;
            const qsizetype colon = line.indexOf(':');
            if (colon < 0)
                continue;
            const QByteArray name = line.left(colon).trimmed().toLower();
            request.headers.insert(name, line.mid(colon + 1).trimmed());
            if (name == "content-length")
                length = line.mid(colon + 1).trimmed().toLongLong();
        }
        QByteArray body(length, '\0');
        if (length > 0 && vlc_tls_Read(socket, body.data(), length, true) != length)
            return;
        request.json = QJsonDocument::fromJson(body).object();

        Reply reply;
        {
            std::lock_guard<std::mutex> lock(m_lock);
            m_received.push_back(request);
            reply = answer(request);
        }

        QByteArray out = "HTTP/1.1 " + QByteArray::number(reply.status) + " X\r\n"
                         "Content-Type: application/json\r\nConnection: close\r\n";
        if (reply.chunked)
        {
            out += "Transfer-Encoding: chunked\r\n\r\n";
            /* in two chunks */
            const QByteArray a = reply.body.left(reply.body.size() / 2);
            const QByteArray b = reply.body.mid(a.size());
            for (const QByteArray &chunk : { a, b })
                if (!chunk.isEmpty())
                    out += QByteArray::number(chunk.size(), 16) + "\r\n" + chunk + "\r\n";
            out += "0\r\n\r\n";
        }
        else
            out += "Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n\r\n"
                   + reply.body;
        vlc_tls_Write(socket, out.constData(), out.size());
        vlc_tls_Shutdown(socket, true);
    }

    static QByteArray json(const QJsonObject &object)
    {
        return QJsonDocument(object).toJson(QJsonDocument::Compact);
    }

    Reply answer(const Request &r)
    {
        const bool keyed = r.headers.value("api-key") == "test-key"
                        && !r.headers.value("user-agent").isEmpty();
        if (r.path.startsWith("/api/v1/") && !keyed)
            return { 403, json({ { "message", "bad key" } }) };

        if (r.method == "GET" && r.path == "/api/v1/subtitles")
        {
            const QJsonArray data = subtitles ? subtitles(r) : QJsonArray{};
            return { 200, json({ { "total_count", data.size() }, { "page", 1 }, { "data", data } }), true };
        }
        if (r.method == "POST" && r.path == "/api/v1/download")
        {
            if (remaining <= 0)
                return { 406, json({ { "requests", requests }, { "remaining", 0 },
                                     { "message", "You have downloaded your allowed subtitles for 24h." } }) };
            --remaining;
            ++requests;
            const int id = r.json.value("file_id").toInt();
            return { 200, json({
                { "link", base() + QStringLiteral("/files/%1.srt").arg(id) },
                { "file_name", QStringLiteral("subtitle-%1.srt").arg(id) },
                { "requests", requests }, { "remaining", remaining },
                { "reset_time_utc", QDateTime::currentDateTimeUtc().addDays(1).toString(Qt::ISODate) },
            }) };
        }
        if (r.method == "POST" && r.path == "/api/v1/login")
        {
            if (r.json.value("username").toString() != user
             || r.json.value("password").toString() != password)
                return { 401, json({ { "message", "Error, invalid username/password" } }) };
            return { 200, json({
                { "user", QJsonObject{ { "allowed_downloads", userAllowed }, { "vip", false },
                                       { "level", "Sub leecher" } } },
                { "base_url", "api.opensubtitles.com" },
                { "token", "token-of-jane" }, { "status", 200 },
            }) };
        }
        if (r.method == "GET" && r.path == "/api/v1/infos/user")
        {
            if (r.headers.value("authorization") != "Bearer token-of-jane")
                return { 401, json({ { "message", "not signed in" } }) };
            return { 200, json({ { "data", QJsonObject{
                { "allowed_downloads", userAllowed }, { "remaining_downloads", userRemaining },
                { "vip", false } } } }) };
        }
        if (r.method == "DELETE" && r.path == "/api/v1/logout")
            return { 200, json({ { "status", 200 } }) };
        if (r.method == "GET" && r.path.startsWith("/files/"))
            return { 200, srt("subtitle " + r.path.mid(7).toLatin1()) };
        if (r.method == "GET" && r.path.startsWith("/links/") && links.contains(r.path.mid(7)))
            return { 200, links.value(r.path.mid(7)) };
        return { 404, json({ { "message", "not found" } }) };
    }

    vlc_object_t *m_obj;
    int *m_fds = nullptr;
    unsigned m_port = 0;
    vlc_interrupt_t *m_interrupt = nullptr;
    mutable std::mutex m_lock;
    QVector<Request> m_received;
    std::thread m_thread;
};

class TestOpenSubtitles : public QObject
{
    Q_OBJECT

private:
    vlc_player_t *player() const
    {
        return m_env->intf->p_player;
    }

    /* a video of 128 KiB of zeros: its hash is its size */
    QString writeVideo(const QString &relativePath)
    {
        const QString path = m_dir->filePath(relativePath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(QByteArray(128 * 1024, '\0'));
        return path;
    }

    QStringList savedMatches(const QString &mediaMrl) const
    {
        QStringList list;
        for (const std::string &mrl : m_env->subtitleMatches(qtu(mediaMrl)))
            list.push_back(qfu(mrl.c_str()));
        return list;
    }

    QString fileContent(const QString &mrl) const
    {
        char *path = vlc_uri2path(qtu(mrl));
        QFile file(qfu(path));
        free(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(file.readAll());
    }

    std::unique_ptr<Client> makeClient(const QByteArray &key = "test-key")
    {
        return std::make_unique<Client>(m_object, m_api->apiUrl(), key);
    }

    QStringList names(const QAbstractItemModel &model, int role) const
    {
        QStringList list;
        for (int i = 0; i < model.rowCount(); ++i)
            list.push_back(model.data(model.index(i, 0), role).toString());
        return list;
    }

    int spuCount() const
    {
        vlc_player_locker locker{ player() };
        return vlc_player_GetTrackCount(player(), SPU_ES);
    }

private slots:
    void initTestCase()
    {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());

        /* the preferred subtitle language: French, then English */
        var_Create(m_env->intf, "sub-language", VLC_VAR_STRING);
        var_SetString(m_env->intf, "sub-language", "fre,eng");
    }

    void cleanupTestCase()
    {
        m_env.reset();
    }

    void init()
    {
        m_env->clearMediaLibrary();
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_api = std::make_unique<MockApi>(VLC_OBJECT(m_env->intf));
        QVERIFY(m_api->listening());

        /* the sign-in goes to a keystore file of the test */
        m_object = static_cast<vlc_object_t *>(vlc_object_create(m_env->intf, sizeof (vlc_object_t)));
        QVERIFY(m_object);
        var_Create(m_object, "keystore", VLC_VAR_STRING);
        var_SetString(m_object, "keystore", "file_plaintext");
        var_Create(m_object, "keystore-file", VLC_VAR_STRING);
        var_SetString(m_object, "keystore-file", qtu(m_dir->filePath("keystore")));

        m_controller = std::make_unique<PlaylistController>(m_env->intf->p_playlist);
        QTRY_VERIFY(m_controller->isInitialized());
        m_medialib = std::make_unique<MediaLib>(m_env->intf, m_controller.get());
    }

    void cleanup()
    {
        {
            vlc_player_locker locker{ player() };
            vlc_player_SetCurrentMedia(player(), nullptr);
        }
        QTRY_VERIFY([this] {
            vlc_player_locker locker{ player() };
            return vlc_player_GetCurrentMedia(player()) == nullptr && !vlc_player_IsStarted(player());
        }());
        m_medialib.reset();
        m_controller.reset();
        vlc_object_delete(m_object);
        m_object = nullptr;
        m_api.reset();
        m_dir.reset();
    }

    // Reading file names and hashes

    void testParsesTheNameOfAVideo_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("query");
        QTest::newRow("dots") << "The.Wand.S01E01.720p.WEB-DL.mkv" << "The Wand S01E01";
        QTest::newRow("crossed") << "The Wand - 1x02 - Pilot.mkv" << "The Wand S01E02";
        QTest::newRow("episode only") << "[Group] Show Name - E05 [1080p].mkv" << "Show Name E05";
        QTest::newRow("movie") << "Some.Movie.2010.1080p.BluRay.x264.mkv" << "Some Movie";
        QTest::newRow("year first") << "2001.A.Space.Odyssey.1968.mkv" << "2001 A Space Odyssey";
        QTest::newRow("plain") << "holidays.mp4" << "holidays";
    }

    void testParsesTheNameOfAVideo()
    {
        QFETCH(QString, file);
        QFETCH(QString, query);
        QCOMPARE(parseName(file).query(), query);
    }

    void testMovieHash()
    {
        /* the size, plus the 64-bit little-endian words of the first and
         * last 64 KiB */
        QFile file(m_dir->filePath("zeros.bin"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(128 * 1024, '\0'));
        file.close();
        QCOMPARE(movieHash(file.fileName()), QStringLiteral("0000000000020000"));

        QByteArray data(128 * 1024, '\0');
        data[0] = 1;
        /* the last word is all ones: it wraps around */
        for (int i = 1; i <= 8; ++i)
            data[data.size() - i] = char(0xFF);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(data);
        file.close();
        QCOMPARE(movieHash(file.fileName()), QStringLiteral("0000000000020000"));

        data[8] = 2;
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(data);
        file.close();
        QCOMPARE(movieHash(file.fileName()), QStringLiteral("0000000000020002"));

        QCOMPARE(movieHash(m_dir->filePath("missing.mkv")), QString());
    }

    // To This File, From OpenSubtitles

    void testSearchIsPrefilledAndRanksExactMatchesFirst()
    {
        const QString video = writeVideo("The.Wand.S01E01.720p.mkv");
        m_api->subtitles = [](const MockApi::Request &) {
            return QJsonArray{
                MockApi::result(1, "The.Wand.S01E01.HDTV", "fr", 50, false, 1, 1),
                MockApi::result(2, "The.Wand.S01E01.WEB", "fr", 900, false, 1, 1),
                MockApi::result(3, "The.Wand.S01E01.720p", "fr", 3, true, 1, 1),
            };
        };

        auto client = makeClient();
        OpenSubtitlesModel model(m_env->intf, m_medialib.get(), client.get(), toMrl(video));
        QVERIFY(model.available());
        QCOMPARE(model.query(), QStringLiteral("The Wand S01E01"));
        QCOMPARE(model.language(), QStringLiteral("fr"));
        QCOMPARE(model.quotaText(), QStringLiteral("5 of 5 downloads left today"));

        model.search();
        QVERIFY(model.busy());
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.error(), QString());
        QVERIFY(model.searched());

        QCOMPARE(names(model, OpenSubtitlesModel::NameRole),
                 QStringList({ "The.Wand.S01E01.720p", "The.Wand.S01E01.WEB", "The.Wand.S01E01.HDTV" }));
        QVERIFY(model.data(model.index(0), OpenSubtitlesModel::ExactMatchRole).toBool());
        QVERIFY(!model.data(model.index(1), OpenSubtitlesModel::ExactMatchRole).toBool());

        /* the API's parameters: sorted, lower case, with the hash */
        const MockApi::Request request = m_api->last("GET", "/api/v1/subtitles");
        QCOMPARE(request.method, QByteArray("GET"));
        QCOMPARE(request.query.toString(QUrl::FullyDecoded),
                 QStringLiteral("episode_number=1&languages=fr&moviehash=0000000000020000"
                                "&query=the wand&season_number=1"));
        /* nothing was downloaded */
        QCOMPARE(m_api->count("POST", "/api/v1/download"), 0);
    }

    void testSaveDownloadsNextToTheVideoAndAttaches()
    {
        const QString video = writeVideo("The.Wand.S01E02.mkv");
        m_api->subtitles = [](const MockApi::Request &) {
            return QJsonArray{ MockApi::result(42, "The.Wand.S01E02", "fr", 10, true, 1, 2) };
        };

        auto client = makeClient();
        OpenSubtitlesModel model(m_env->intf, m_medialib.get(), client.get(), toMrl(video));
        model.search();
        QTRY_COMPARE(model.count(), 1);

        model.toggle(0);
        QCOMPARE(model.checkedCount(), 1);
        QSignalSpy saved(&model, &OpenSubtitlesModel::saved);
        model.save();
        QTRY_COMPARE(saved.count(), 1);

        const QString subtitle = toMrl(m_dir->filePath("The.Wand.S01E02.fr.srt"));
        QTRY_COMPARE(savedMatches(toMrl(video)), QStringList({ subtitle }));
        QVERIFY(fileContent(subtitle).contains("subtitle 42"));
        QCOMPARE(m_api->count("POST", "/api/v1/download"), 1);
        QCOMPARE(m_api->last("POST", "/api/v1/download").json.value("file_id").toInt(), 42);
        QCOMPARE(model.quotaText(), QStringLiteral("4 of 5 downloads left today"));
    }

    void testSaveForAStreamGoesToTheCacheAndShows()
    {
        const QString media = "stubtracks://1";
        {
            input_item_t *item = input_item_New(qtu(media), "media");
            vlc_player_locker locker{ player() };
            QCOMPARE(vlc_player_SetCurrentMedia(player(), item), VLC_SUCCESS);
            QCOMPARE(vlc_player_Start(player()), VLC_SUCCESS);
            input_item_Release(item);
        }
        QTRY_COMPARE(spuCount(), 1);

        m_api->subtitles = [](const MockApi::Request &) {
            return QJsonArray{ MockApi::result(7, "Some.Show", "fr", 10, false) };
        };
        auto client = makeClient();
        OpenSubtitlesModel model(m_env->intf, m_medialib.get(), client.get(), media);
        model.setCacheFolder(m_dir->filePath("cache"));
        model.setQuery("some show");
        model.search();
        QTRY_COMPARE(model.count(), 1);
        model.toggle(0);
        QSignalSpy saved(&model, &OpenSubtitlesModel::saved);
        model.save();
        QTRY_COMPARE(saved.count(), 1);

        QCOMPARE(QDir(m_dir->filePath("cache")).entryList(QDir::Files).size(), 1);
        QTRY_COMPARE(savedMatches(media).size(), 1);
        QTRY_COMPARE(spuCount(), 2);
    }

    void testOutOfDownloadsShowsWhy()
    {
        const QString video = writeVideo("movie.mkv");
        m_api->remaining = 0;
        m_api->requests = 5;
        m_api->subtitles = [](const MockApi::Request &) {
            return QJsonArray{ MockApi::result(9, "movie", "fr", 1, true) };
        };
        auto client = makeClient();
        OpenSubtitlesModel model(m_env->intf, m_medialib.get(), client.get(), toMrl(video));
        model.search();
        QTRY_COMPARE(model.count(), 1);
        model.toggle(0);
        model.save();
        QTRY_VERIFY(!model.error().isEmpty());
        QVERIFY(!model.busy());
        QCOMPARE(model.quotaText(), QStringLiteral("0 of 5 downloads left today"));
        QVERIFY(model.quotaNote().contains("Sign in"));
        QCOMPARE(savedMatches(toMrl(video)), QStringList());
    }

    void testWithoutAKeyOpenSubtitlesIsUnavailable()
    {
        const QString video = writeVideo("movie.mkv");
        auto client = makeClient("");
        OpenSubtitlesModel model(m_env->intf, m_medialib.get(), client.get(), toMrl(video));
        QVERIFY(!model.available());
        model.search();
        QVERIFY(!model.busy());
        QTest::qWait(50);
        QVERIFY(m_api->received().isEmpty());
    }

    // To This Folder, From OpenSubtitles

    void testFolderPrePicksTheBestResultsWithinTheQuota()
    {
        const QString e1 = writeVideo("Show/The.Wand.S01E01.mkv");
        const QString e2 = writeVideo("Show/The.Wand.S01E02.mkv");
        const QString e3 = writeVideo("Show/The.Wand.S01E03.mkv");
        QFile(m_dir->filePath("Show/notes.txt")).open(QIODevice::WriteOnly);

        /* every search answers with the three episodes */
        m_api->subtitles = [](const MockApi::Request &r) {
            const int episode = r.query.queryItemValue("episode_number").toInt();
            return QJsonArray{
                MockApi::result(100 + episode, "wrong episode", "fr", 5000, false, 1, episode % 3 + 1),
                MockApi::result(10 * episode, QStringLiteral("The.Wand.S01E0%1").arg(episode), "fr",
                                40, false, 1, episode),
                MockApi::result(10 * episode + 1, QStringLiteral("The.Wand.S01E0%1.popular").arg(episode),
                                "fr", 400, false, 1, episode),
            };
        };

        /* two downloads left today, remembered from an earlier run */
        QSettings settings(m_dir->filePath("quota.ini"), QSettings::IniFormat);
        settings.setValue("OpenSubtitles/remaining", 2);
        settings.setValue("OpenSubtitles/allowed", 5);
        settings.setValue("OpenSubtitles/reset", QDateTime::currentDateTimeUtc().addSecs(3600));
        auto client = makeClient();
        client->setQuotaStore(&settings);

        OpenSubtitlesFolderModel model(m_env->intf, m_medialib.get(), client.get(),
                                       m_dir->filePath("Show"));
        QCOMPARE(model.quotaText(), QStringLiteral("2 of 5 downloads left today"));
        QCOMPARE(names(model, OpenSubtitlesFolderModel::EpisodeRole),
                 QStringList({ "The.Wand.S01E01.mkv", "The.Wand.S01E02.mkv", "The.Wand.S01E03.mkv" }));

        model.search();
        QTRY_VERIFY(!model.busy());
        QCOMPARE(m_api->count("GET", "/api/v1/subtitles"), 3);
        QCOMPARE(names(model, OpenSubtitlesFolderModel::PickNameRole),
                 QStringList({ "The.Wand.S01E01.popular", "The.Wand.S01E02.popular", "" }));
        auto state = [&model](int row) {
            return model.data(model.index(row), OpenSubtitlesFolderModel::StateRole).toInt();
        };
        QCOMPARE(state(0), int(OpenSubtitlesFolderModel::Picked));
        QCOMPARE(state(1), int(OpenSubtitlesFolderModel::Picked));
        /* no download left for the third one */
        QCOMPARE(state(2), int(OpenSubtitlesFolderModel::NotPicked));
        QCOMPARE(model.pickedCount(), 2);
        /* nothing downloads until Save */
        QCOMPARE(m_api->count("POST", "/api/v1/download"), 0);

        /* ⇆: the candidates, best first, and None */
        const QVariantList candidates = model.candidates(1);
        QCOMPARE(candidates.size(), 3);
        QCOMPARE(candidates.first().toMap().value("name").toString(), "The.Wand.S01E02.popular");
        model.pick(1, -1);
        QCOMPARE(state(1), int(OpenSubtitlesFolderModel::None));
        QCOMPARE(model.pickedCount(), 1);

        QSignalSpy saved(&model, &OpenSubtitlesFolderModel::saved);
        model.save();
        QTRY_COMPARE(saved.count(), 1);
        QCOMPARE(m_api->count("POST", "/api/v1/download"), 1);
        QTRY_COMPARE(savedMatches(toMrl(e1)).size(), 1);
        QVERIFY(fileContent(savedMatches(toMrl(e1)).first()).contains("subtitle 11"));
        QCOMPARE(savedMatches(toMrl(e2)), QStringList());
        QCOMPARE(savedMatches(toMrl(e3)), QStringList());
        QCOMPARE(state(0), int(OpenSubtitlesFolderModel::Saved));
    }

    // Signing in

    void testSignInStoresTheCredentialsInTheKeystore()
    {
        auto client = makeClient();
        QSignalSpy quota(client.get(), &Client::quotaChanged);

        QString error = "pending";
        client->signIn("jane", "nope", [&error](const QString &e) { error = e; });
        QTRY_VERIFY(error != "pending");
        QVERIFY(!error.isEmpty());
        QVERIFY(!client->quota().signedIn);

        error = "pending";
        client->signIn("jane", "secret", [&error](const QString &e) { error = e; });
        QTRY_COMPARE(error, QString());
        QVERIFY(client->quota().signedIn);
        QCOMPARE(client->storedUser(), QStringLiteral("jane"));
        QCOMPARE(quotaText(client->quota()), QStringLiteral("19 of 20 downloads left today"));
        QVERIFY(quota.count() >= 1);
        /* the password isn't in the settings: only in the keystore */
        QFile keystore(m_dir->filePath("keystore"));
        QVERIFY(keystore.exists());

        /* the next run signs in silently */
        auto next = makeClient();
        QTRY_COMPARE(next->storedUser(), QStringLiteral("jane"));
        const int logins = m_api->count("POST", "/api/v1/login");
        error = "pending";
        next->restoreSession([&error](const QString &e) { error = e; });
        QTRY_COMPARE(error, QString());
        QCOMPARE(m_api->count("POST", "/api/v1/login"), logins + 1);
        QCOMPARE(m_api->last("POST", "/api/v1/login").json.value("password").toString(), "secret");
        QVERIFY(next->quota().signedIn);

        /* signed in, the searches carry the token */
        next->search({ "the wand", "fr", {}, -1, -1 }, [](const QVector<Result> &, const QString &) {});
        QTRY_COMPARE(m_api->count("GET", "/api/v1/subtitles"), 1);
        QCOMPARE(m_api->last("GET", "/api/v1/subtitles").headers.value("authorization"),
                 QByteArray("Bearer token-of-jane"));

        /* signing out forgets them */
        error = "pending";
        next->signOut([&error](const QString &e) { error = e; });
        QTRY_COMPARE(error, QString());
        QVERIFY(!next->quota().signedIn);
        QCOMPARE(next->storedUser(), QString());

        auto after = makeClient();
        error = "pending";
        after->restoreSession([&error](const QString &e) { error = e; });
        QTRY_COMPARE(error, QString());
        QCOMPARE(m_api->count("POST", "/api/v1/login"), logins + 1);
        QVERIFY(!after->quota().signedIn);
    }

    // From Link…

    void testFromLinkAddsASubtitleFile()
    {
        const QString video = writeVideo("movie.mkv");
        m_api->links.insert("movie.en.srt", srt("from a link"));

        AddSubtitlesModel model(m_env->intf, m_medialib.get(), toMrl(video));
        QCOMPARE(model.rowCount(), 0);

        model.addLink("ftp://example.org/movie.srt");
        QVERIFY(!model.linkError().isEmpty());
        model.addLink(m_api->base() + "/links/readme.txt");
        QVERIFY(!model.linkError().isEmpty());
        QCOMPARE(model.rowCount(), 0);

        model.addLink(m_api->base() + "/links/movie.en.srt");
        QVERIFY(model.fetching());
        QTRY_VERIFY(!model.fetching());
        QCOMPARE(model.linkError(), QString());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), AddSubtitlesModel::NameRole).toString(), "movie.en.srt");
        QCOMPARE(model.checkedCount(), 1);

        /* nothing lands next to the video before Save */
        QVERIFY(!QFile::exists(m_dir->filePath("movie.en.srt")));
        model.save();
        const QString copied = toMrl(m_dir->filePath("movie.en.srt"));
        QTRY_COMPARE(savedMatches(toMrl(video)), QStringList({ copied }));
        QVERIFY(fileContent(copied).contains("from a link"));

        /* a link that fails */
        model.addLink(m_api->base() + "/links/missing.srt");
        QTRY_VERIFY(!model.fetching());
        QVERIFY(!model.linkError().isEmpty());
    }

    void testFromLinkListsTheSubtitlesOfAZip()
    {
        const QString video = writeVideo("Show/Show S01E01.mkv");
        m_api->links.insert("subs.zip", makeZip({
            { "Show S01E02.srt", srt("second") },
            { "readme.txt", "not a subtitle" },
            { "Show S01E01.srt", srt("first") },
        }));

        AddSubtitlesModel model(m_env->intf, m_medialib.get(), toMrl(video));
        model.addLink(m_api->base() + "/links/subs.zip");
        QTRY_VERIFY(!model.fetching());
        QCOMPARE(model.linkError(), QString());
        QCOMPARE(names(model, AddSubtitlesModel::NameRole),
                 QStringList({ "Show S01E01.srt", "Show S01E02.srt" }));
        /* several to choose from: none is checked */
        QCOMPARE(model.checkedCount(), 0);

        model.toggle(0);
        model.save();
        const QString copied = toMrl(m_dir->filePath("Show/Show S01E01.srt"));
        QTRY_COMPARE(savedMatches(toMrl(video)), QStringList({ copied }));
        QVERIFY(fileContent(copied).contains("first"));
        QVERIFY(!QFile::exists(m_dir->filePath("Show/Show S01E02.srt")));
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<MockApi> m_api;
    vlc_object_t *m_object = nullptr;
    std::unique_ptr<PlaylistController> m_controller;
    std::unique_ptr<MediaLib> m_medialib;
};

QTEST_GUILESS_MAIN(TestOpenSubtitles)
#include "test_opensubtitles.moc"
