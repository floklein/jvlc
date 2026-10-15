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
#include "opensubtitles.hpp"
#include "http_request.hpp"

/* OPENSUBTITLES_API_KEY and OPENSUBTITLES_API_URL, from the build options */
#include "opensubtitles_config.h"

#include "qt.hpp"

#include <vlc_configuration.h>
#include <vlc_iso_lang.h>
#include <vlc_keystore.h>
#include <vlc_variables.h>

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSettings>
#include <QUrlQuery>

namespace opensubtitles {

namespace {

/* anonymous downloads a day, until a download tells otherwise */
constexpr int anonymousDownloads = 5;

/* the keystore entry of the sign-in */
const char keystoreProtocol[] = "https";
const char keystoreServer[] = "opensubtitles.com";

QString cleanTitle(QString title)
{
    static const QRegularExpression brackets(QStringLiteral("[\\[\\(\\{][^\\]\\)\\}]*[\\]\\)\\}]"));
    title.replace(brackets, QStringLiteral(" "));
    title.replace(QLatin1Char('.'), QLatin1Char(' '));
    title.replace(QLatin1Char('_'), QLatin1Char(' '));
    title = title.simplified();
    while (title.endsWith(QLatin1Char('-')) || title.endsWith(QLatin1Char(' ')))
        title.chop(1);
    return title.simplified();
}

} // anonymous namespace

QString ParsedName::query() const
{
    if (season >= 0 && episode >= 0)
        return QStringLiteral("%1 S%2E%3").arg(title)
            .arg(season, 2, 10, QLatin1Char('0'))
            .arg(episode, 2, 10, QLatin1Char('0'));
    if (episode >= 0)
        return QStringLiteral("%1 E%2").arg(title).arg(episode, 2, 10, QLatin1Char('0'));
    return title;
}

ParsedName parseName(const QString &fileName)
{
    static const QRegularExpression extension(QStringLiteral("\\.[A-Za-z0-9]{2,4}$"));
    static const QRegularExpression seasonEpisode(
        QStringLiteral("(?:^|[^A-Za-z0-9])[Ss](\\d{1,2})[ ._-]*[Ee](\\d{1,3})(?![0-9])"));
    static const QRegularExpression crossed(
        QStringLiteral("(?:^|[^A-Za-z0-9])(\\d{1,2})[xX](\\d{2,3})(?![0-9])"));
    static const QRegularExpression episodeOnly(
        QStringLiteral("(?:^|[^A-Za-z0-9])(?:[Ee][Pp]?|[Ee]pisode[ ._-]?)(\\d{1,3})(?![0-9])"));
    static const QRegularExpression tags(
        QStringLiteral("(?:^|[^A-Za-z0-9])((?:19|20)\\d\\d|480p|576p|720p|1080[pi]|2160p|4k|"
                       "x26[45]|h\\.?26[45]|hevc|bluray|blu-ray|brrip|bdrip|webrip|web-dl|"
                       "web|hdtv|dvdrip|proper|repack)(?![A-Za-z0-9])"),
        QRegularExpression::CaseInsensitiveOption);

    ParsedName parsed;
    QString name = fileName;
    name.remove(extension);

    qsizetype cut = name.size();
    QRegularExpressionMatch match = seasonEpisode.match(name);
    if (!match.hasMatch())
        match = crossed.match(name);
    if (match.hasMatch())
    {
        parsed.season = match.captured(1).toInt();
        parsed.episode = match.captured(2).toInt();
        cut = match.capturedStart(0);
    }
    else if ((match = episodeOnly.match(name)).hasMatch() && match.capturedStart(1) > 2)
    {
        parsed.episode = match.captured(1).toInt();
        cut = match.capturedStart(0);
    }

    /* a year or a release tag ends the title, unless it starts it */
    QRegularExpressionMatchIterator it = tags.globalMatch(name.left(cut));
    while (it.hasNext())
    {
        const QRegularExpressionMatch tag = it.next();
        if (tag.capturedStart(1) > 0)
        {
            cut = tag.capturedStart(1);
            break;
        }
    }

    parsed.title = cleanTitle(name.left(cut));
    if (parsed.title.isEmpty())
        parsed.title = cleanTitle(name);
    return parsed;
}

QString movieHash(const QString &path)
{
    constexpr qint64 chunk = 64 * 1024;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = file.size();
    if (size <= 0)
        return {};

    quint64 hash = static_cast<quint64>(size);
    auto sum = [&hash](const QByteArray &data) {
        const auto *bytes = reinterpret_cast<const uchar *>(data.constData());
        for (qsizetype i = 0; i + 8 <= data.size(); i += 8)
        {
            quint64 word = 0;
            for (int b = 7; b >= 0; --b)
                word = (word << 8) | bytes[i + b];
            hash += word;
        }
    };

    sum(file.read(chunk));
    if (!file.seek(std::max<qint64>(0, size - chunk)))
        return {};
    sum(file.read(chunk));

    return QStringLiteral("%1").arg(hash, 16, 16, QLatin1Char('0'));
}

QString preferredLanguage(vlc_object_t *obj)
{
    auto toCode = [](QString code) -> QString {
        code = code.trimmed().toLower();
        if (code.isEmpty() || code == QLatin1String("none") || code == QLatin1String("any"))
            return {};
        if (code == QLatin1String("pob") || code == QLatin1String("pt-br")
         || code == QLatin1String("pt_br"))
            return QStringLiteral("pt-br");
        const iso639_lang_t *lang = vlc_find_iso639(qtu(code.left(3)), true);
        if (lang == nullptr || lang->psz_iso639_1[0] == '\0')
            return {};
        const QString iso = QString::fromLatin1(lang->psz_iso639_1);
        if (iso == QLatin1String("pt"))
            return QStringLiteral("pt-pt");
        if (iso == QLatin1String("zh"))
            return QStringLiteral("zh-cn");
        return iso;
    };

    char *option = var_InheritString(obj, "sub-language");
    const QString languages = qfu(option);
    free(option);
    for (const QString &code : languages.split(QLatin1Char(','), Qt::SkipEmptyParts))
    {
        const QString language = toCode(code);
        if (!language.isEmpty())
            return language;
    }

    const QLocale locale = QLocale::system();
    if (locale.territory() == QLocale::Brazil && locale.language() == QLocale::Portuguese)
        return QStringLiteral("pt-br");
    const QString system = toCode(locale.name().section(QLatin1Char('_'), 0, 0));
    return system.isEmpty() ? QStringLiteral("en") : system;
}

QVariantList languages()
{
    static const char *const codes[] = {
        "ar", "bg", "ca", "cs", "da", "de", "el", "en", "es", "et", "eu", "fa",
        "fi", "fr", "he", "hi", "hr", "hu", "id", "it", "ja", "ko", "lt", "lv",
        "ms", "nl", "no", "pl", "pt-br", "pt-pt", "ro", "ru", "sk", "sl", "sr",
        "sv", "th", "tr", "uk", "vi", "zh-cn", "zh-tw",
    };

    QVariantList list;
    for (const char *code : codes)
    {
        const QString id = QString::fromLatin1(code);
        const iso639_lang_t *lang = vlc_find_iso639(qtu(id.left(2)), false);
        QString name = lang ? qfu(lang->psz_eng_name) : id;
        if (id == QLatin1String("pt-br"))
            name = qtr("Portuguese (Brazil)");
        else if (id == QLatin1String("pt-pt"))
            name = qtr("Portuguese (Portugal)");
        else if (id == QLatin1String("zh-cn"))
            name = qtr("Chinese (simplified)");
        else if (id == QLatin1String("zh-tw"))
            name = qtr("Chinese (traditional)");
        list.push_back(QVariantMap{ { QStringLiteral("code"), id },
                                    { QStringLiteral("name"), name } });
    }
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString()
             < b.toMap().value(QStringLiteral("name")).toString();
    });
    return list;
}

QString quotaText(const Quota &quota)
{
    if (quota.remaining < 0)
        return {};
    if (quota.allowed < 0)
        return quota.remaining == 1 ? qtr("1 download left today")
                                    : qtr("%1 downloads left today").arg(quota.remaining);
    return quota.allowed == 1 ? qtr("%1 of 1 download left today").arg(quota.remaining)
                              : qtr("%1 of %2 downloads left today").arg(quota.remaining).arg(quota.allowed);
}

QString quotaNote(const Quota &quota)
{
    if (quota.vip)
        return {};
    if (!quota.signedIn)
    {
        if (quota.remaining == 0)
            return qtr("Sign in to OpenSubtitles in Settings › Subtitles to get more downloads. "
                       "VIP members get even more, without ads.");
        return qtr("Free downloads may include ads. Signing in or VIP raises the quota; VIP removes ads.");
    }
    if (quota.remaining == 0)
        return qtr("OpenSubtitles VIP raises the quota and removes ads.");
    return qtr("Free downloads may include ads. OpenSubtitles VIP removes them.");
}

int bestResult(const QVector<Result> &results, int season, int episode)
{
    int best = -1;
    auto score = [season, episode](const Result &r) {
        int s = 0;
        if (r.exactMatch)
            s += 4;
        if (episode >= 0 && r.episode == episode && (season < 0 || r.season < 0 || r.season == season))
            s += 2;
        return s;
    };
    for (int i = 0; i < results.size(); ++i)
    {
        const Result &r = results[i];
        /* a result for another episode never fits */
        if (!r.exactMatch && episode >= 0 && r.episode >= 0
         && (r.episode != episode || (season >= 0 && r.season >= 0 && r.season != season)))
            continue;
        if (best < 0 || score(r) > score(results[best])
         || (score(r) == score(results[best]) && r.downloads > results[best].downloads))
            best = i;
    }
    return best;
}

/* the state of the worker thread: the session and the keystore */
struct Client::Worker
{
    vlc_object_t *obj;
    QString apiUrl;
    QByteArray apiKey;

    // the URL of the session (VIP members get another host)
    QString sessionUrl;
    QByteArray token;
    QString user;
    vlc_keystore *keystore = nullptr;

    // last, so that it stops before the rest goes
    HttpWorker http;

    ~Worker()
    {
        /* no task runs after this */
        http.stop();
        if (keystore != nullptr)
            vlc_keystore_release(keystore);
    }

    vlc_keystore *openKeystore()
    {
        if (keystore == nullptr)
            keystore = vlc_keystore_create(obj);
        return keystore;
    }

    void initValues(const char *values[KEY_MAX], const char *user = nullptr)
    {
        VLC_KEYSTORE_VALUES_INIT(values);
        values[KEY_PROTOCOL] = keystoreProtocol;
        values[KEY_SERVER] = keystoreServer;
        values[KEY_USER] = user;
    }

    /* the stored user and password, false if none */
    bool loadCredentials(QString &userName, QString &password)
    {
        vlc_keystore *ks = openKeystore();
        if (ks == nullptr)
            return false;
        const char *values[KEY_MAX];
        initValues(values);
        vlc_keystore_entry *entries;
        const unsigned count = vlc_keystore_find(ks, values, &entries);
        if (count == 0)
            return false;
        userName = qfu(entries[0].ppsz_values[KEY_USER]);
        password = QString::fromUtf8(reinterpret_cast<const char *>(entries[0].p_secret),
                                     entries[0].i_secret_len);
        vlc_keystore_release_entries(entries, count);
        return !userName.isEmpty();
    }

    void forgetCredentials()
    {
        vlc_keystore *ks = openKeystore();
        if (ks == nullptr)
            return;
        const char *values[KEY_MAX];
        initValues(values);
        vlc_keystore_remove(ks, values);
    }

    bool storeCredentials(const QString &userName, const QString &password)
    {
        vlc_keystore *ks = openKeystore();
        if (ks == nullptr)
            return false;
        forgetCredentials();
        const QByteArray name = userName.toUtf8();
        const QByteArray secret = password.toUtf8();
        const char *values[KEY_MAX];
        initValues(values, name.constData());
        return vlc_keystore_store(ks, values, reinterpret_cast<const uint8_t *>(secret.constData()),
                                  secret.size(), "OpenSubtitles") == VLC_SUCCESS;
    }

    HttpResponse call(const QByteArray &method, const QString &path,
                      const QUrlQuery &query = {}, const QJsonObject &body = {})
    {
        HttpRequest request;
        request.method = method;
        request.url = QUrl((sessionUrl.isEmpty() ? apiUrl : sessionUrl) + path);
        if (!query.isEmpty())
            request.url.setQuery(query);
        request.headers = {
            { "Api-Key", apiKey },
            { "User-Agent", "VLC v" PACKAGE_VERSION },
            { "Accept", "application/json" },
        };
        if (!token.isEmpty())
            request.headers.push_back({ "Authorization", QByteArray("Bearer ").append(token) });
        if (method == "POST")
        {
            request.headers.push_back({ "Content-Type", "application/json" });
            request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
        }
        return httpRequest(obj, request);
    }

    /* the error to show for a failed call */
    static QString errorOf(const HttpResponse &response)
    {
        if (response.status == 0)
            return qtr("OpenSubtitles can't be reached. Check your connection.");
        const QJsonObject json = QJsonDocument::fromJson(response.body).object();
        QString message = json.value(QStringLiteral("message")).toString();
        if (message.isEmpty())
        {
            const QJsonArray errors = json.value(QStringLiteral("errors")).toArray();
            if (!errors.isEmpty())
                message = errors.first().toString();
        }
        if (response.status == 401)
            return message.isEmpty() ? qtr("OpenSubtitles refused the sign-in.") : message;
        if (response.status == 429)
            return qtr("OpenSubtitles is busy. Try again in a moment.");
        if (!message.isEmpty())
            return message.trimmed();
        return qtr("OpenSubtitles failed (error %1).").arg(response.status);
    }

    /* POST /login, then the quota; the error, empty on success */
    QString login(const QString &userName, const QString &password, Quota &quota)
    {
        token.clear();
        sessionUrl.clear();
        user.clear();

        QJsonObject body;
        body.insert(QStringLiteral("username"), userName);
        body.insert(QStringLiteral("password"), password);
        const HttpResponse response = call("POST", QStringLiteral("/login"), {}, body);
        if (!response.ok())
            return errorOf(response);

        const QJsonObject json = QJsonDocument::fromJson(response.body).object();
        const QByteArray newToken = json.value(QStringLiteral("token")).toString().toUtf8();
        if (newToken.isEmpty())
            return qtr("OpenSubtitles refused the sign-in.");
        token = newToken;
        user = userName;

        /* VIP members get their own host */
        const QString host = json.value(QStringLiteral("base_url")).toString();
        const QUrl api(apiUrl);
        if (!host.isEmpty() && api.scheme() == QLatin1String("https")
         && api.host().endsWith(QLatin1String("opensubtitles.com")) && host != api.host())
        {
            QUrl vip = api;
            vip.setHost(host);
            sessionUrl = vip.toString();
        }

        const QJsonObject account = json.value(QStringLiteral("user")).toObject();
        quota = Quota{};
        quota.signedIn = true;
        quota.user = userName;
        quota.vip = account.value(QStringLiteral("vip")).toBool();
        quota.allowed = account.value(QStringLiteral("allowed_downloads")).toInt(-1);
        quota.remaining = quota.allowed;
        readUserInfo(quota);
        return {};
    }

    /* GET /infos/user: the downloads left */
    void readUserInfo(Quota &quota)
    {
        const HttpResponse response = call("GET", QStringLiteral("/infos/user"));
        if (!response.ok())
            return;
        const QJsonObject data = QJsonDocument::fromJson(response.body).object()
                                     .value(QStringLiteral("data")).toObject();
        quota.allowed = data.value(QStringLiteral("allowed_downloads")).toInt(quota.allowed);
        quota.remaining = data.value(QStringLiteral("remaining_downloads")).toInt(quota.remaining);
        quota.vip = data.value(QStringLiteral("vip")).toBool(quota.vip);
    }
};

QByteArray Client::bundledKey()
{
    return QByteArray(OPENSUBTITLES_API_KEY);
}

QString Client::bundledUrl()
{
    return QStringLiteral(OPENSUBTITLES_API_URL);
}

Client::Client(vlc_object_t *obj, QObject *parent)
    : Client(obj, bundledUrl(), bundledKey(), parent)
{
}

Client::Client(vlc_object_t *obj, const QString &apiUrl, const QByteArray &apiKey, QObject *parent)
    : QObject(parent)
    , m_apiUrl(apiUrl)
    , m_apiKey(apiKey)
    , m_worker(std::make_shared<Worker>())
{
    while (m_apiUrl.endsWith(QLatin1Char('/')))
        m_apiUrl.chop(1);
    m_worker->obj = obj;
    m_worker->apiUrl = m_apiUrl;
    m_worker->apiKey = m_apiKey;

    m_quota.remaining = anonymousDownloads;
    m_quota.allowed = anonymousDownloads;

    if (available())
        loadStoredUser();
}

Client::~Client()
{
    /* stops the thread first */
    m_worker.reset();
}

void Client::setQuotaStore(QSettings *settings)
{
    m_quotaStore = settings;
    loadStoredQuota();
}

void Client::loadStoredQuota()
{
    if (m_quotaStore == nullptr || m_quota.signedIn)
        return;
    const QDateTime reset = m_quotaStore->value(QStringLiteral("OpenSubtitles/reset")).toDateTime();
    if (!reset.isValid() || reset <= QDateTime::currentDateTimeUtc())
        return;
    Quota quota = m_quota;
    quota.remaining = m_quotaStore->value(QStringLiteral("OpenSubtitles/remaining"), quota.remaining).toInt();
    quota.allowed = m_quotaStore->value(QStringLiteral("OpenSubtitles/allowed"), quota.allowed).toInt();
    setQuota(quota);
}

void Client::setQuota(const Quota &quota)
{
    if (quota.remaining == m_quota.remaining && quota.allowed == m_quota.allowed
     && quota.signedIn == m_quota.signedIn && quota.vip == m_quota.vip && quota.user == m_quota.user)
        return;
    m_quota = quota;
    emit quotaChanged();
}

void Client::post(std::function<void()> task)
{
    m_worker->http.post(std::move(task));
}

void Client::cancel()
{
    m_worker->http.cancel();
}

void Client::search(const SearchQuery &query, SearchCallback callback)
{
    if (!available())
    {
        callback({}, qtr("OpenSubtitles isn't available in this build."));
        return;
    }

    Worker *w = m_worker.get();
    const unsigned gen = w->http.generation();
    post([this, w, gen, query, callback]() {
        const ParsedName parsed = parseName(query.query);
        QUrlQuery params;
        /* sorted and lower case, as the API asks to avoid redirections */
        const int episode = query.episode >= 0 ? query.episode : parsed.episode;
        const int season = query.season >= 0 ? query.season : parsed.season;
        if (episode >= 0)
            params.addQueryItem(QStringLiteral("episode_number"), QString::number(episode));
        if (!query.language.isEmpty())
            params.addQueryItem(QStringLiteral("languages"), query.language.toLower());
        if (!query.movieHash.isEmpty())
            params.addQueryItem(QStringLiteral("moviehash"), query.movieHash.toLower());
        const QString text = (episode >= 0 ? parsed.title : query.query).toLower().simplified();
        if (!text.isEmpty())
            params.addQueryItem(QStringLiteral("query"), text);
        if (season >= 0)
            params.addQueryItem(QStringLiteral("season_number"), QString::number(season));

        const HttpResponse response = w->call("GET", QStringLiteral("/subtitles"), params);
        QVector<Result> results;
        QString error;
        if (!response.ok())
            error = Worker::errorOf(response);
        else
        {
            const QJsonArray data = QJsonDocument::fromJson(response.body).object()
                                        .value(QStringLiteral("data")).toArray();
            for (const QJsonValue &value : data)
            {
                const QJsonObject attributes = value.toObject()
                                                   .value(QStringLiteral("attributes")).toObject();
                const QJsonArray files = attributes.value(QStringLiteral("files")).toArray();
                if (files.isEmpty())
                    continue;
                const QJsonObject file = files.first().toObject();
                const QJsonObject feature = attributes.value(QStringLiteral("feature_details")).toObject();

                Result result;
                result.fileId = QString::number(file.value(QStringLiteral("file_id")).toVariant().toLongLong());
                result.fileName = file.value(QStringLiteral("file_name")).toString();
                result.release = attributes.value(QStringLiteral("release")).toString();
                result.language = attributes.value(QStringLiteral("language")).toString();
                result.downloads = attributes.value(QStringLiteral("download_count")).toInt();
                result.exactMatch = attributes.value(QStringLiteral("moviehash_match")).toBool();
                result.hearingImpaired = attributes.value(QStringLiteral("hearing_impaired")).toBool();
                result.season = feature.value(QStringLiteral("season_number")).toInt(-1);
                result.episode = feature.value(QStringLiteral("episode_number")).toInt(-1);
                if (result.season <= 0 && result.episode <= 0)
                    result.season = result.episode = -1;
                results.push_back(result);
            }
            /* exact matches first, then the most downloaded */
            std::stable_sort(results.begin(), results.end(), [](const Result &a, const Result &b) {
                if (a.exactMatch != b.exactMatch)
                    return a.exactMatch;
                return a.downloads > b.downloads;
            });
        }

        QMetaObject::invokeMethod(this, [this, gen, results, error, callback]() {
            if (m_worker->http.generation() == gen)
                callback(results, error);
        }, Qt::QueuedConnection);
    });
}

void Client::download(const QString &fileId, const QString &destination, DownloadCallback callback)
{
    if (!available())
    {
        callback({}, qtr("OpenSubtitles isn't available in this build."));
        return;
    }

    Worker *w = m_worker.get();
    const unsigned gen = w->http.generation();
    const Quota before = m_quota;
    post([this, w, gen, fileId, destination, callback, before]() {
        QJsonObject body;
        body.insert(QStringLiteral("file_id"), fileId.toLongLong());
        const HttpResponse response = w->call("POST", QStringLiteral("/download"), {}, body);
        const QJsonObject json = QJsonDocument::fromJson(response.body).object();

        /* the quota comes with the answer, refused or not */
        Quota quota = before;
        bool quotaKnown = false;
        if (json.contains(QStringLiteral("remaining")))
        {
            quota.remaining = std::max(0, json.value(QStringLiteral("remaining")).toInt());
            const int requests = json.value(QStringLiteral("requests")).toInt(-1);
            if (requests >= 0)
                quota.allowed = quota.remaining + requests;
            quotaKnown = true;
        }
        const QDateTime reset = QDateTime::fromString(
            json.value(QStringLiteral("reset_time_utc")).toString(), Qt::ISODate);

        QString path;
        QString error;
        const QString link = json.value(QStringLiteral("link")).toString();
        if (!response.ok() || link.isEmpty())
        {
            error = Worker::errorOf(response);
            if (response.status == 406)
            {
                quota.remaining = 0;
                quotaKnown = true;
            }
        }
        else
        {
            HttpRequest get;
            get.url = QUrl(link);
            get.headers = { { "User-Agent", "VLC v" PACKAGE_VERSION } };
            const HttpResponse file = httpRequest(w->obj, get);
            if (!file.ok() || file.body.isEmpty())
                error = qtr("The subtitle couldn't be downloaded.");
            else
            {
                /* the extension of the subtitle, a name not taken yet */
                const QFileInfo target(destination);
                QString suffix = QFileInfo(json.value(QStringLiteral("file_name")).toString()).suffix();
                if (suffix.isEmpty())
                    suffix = QStringLiteral("srt");
                const QString base = target.dir().filePath(target.completeBaseName());
                path = base + QLatin1Char('.') + suffix;
                for (int i = 2; QFileInfo::exists(path); ++i)
                    path = QStringLiteral("%1 (%2).%3").arg(base).arg(i).arg(suffix);

                QDir().mkpath(target.absolutePath());
                QFile out(path);
                if (!out.open(QIODevice::WriteOnly) || out.write(file.body) != file.body.size())
                {
                    error = qtr("The subtitle couldn't be saved in %1.")
                                .arg(QDir::toNativeSeparators(target.absolutePath()));
                    out.close();
                    out.remove();
                    path.clear();
                }
            }
        }

        QMetaObject::invokeMethod(this, [this, gen, quota, quotaKnown, reset, path, error, callback]() {
            if (quotaKnown)
            {
                Quota updated = m_quota;
                updated.remaining = quota.remaining;
                updated.allowed = quota.allowed;
                setQuota(updated);
                if (m_quotaStore != nullptr && !m_quota.signedIn && reset.isValid())
                {
                    m_quotaStore->setValue(QStringLiteral("OpenSubtitles/remaining"), m_quota.remaining);
                    m_quotaStore->setValue(QStringLiteral("OpenSubtitles/allowed"), m_quota.allowed);
                    m_quotaStore->setValue(QStringLiteral("OpenSubtitles/reset"), reset);
                }
            }
            if (m_worker->http.generation() == gen)
                callback(path, error);
        }, Qt::QueuedConnection);
    });
}

void Client::restoreSession(DoneCallback callback)
{
    if (!available())
    {
        if (callback)
            callback({});
        return;
    }

    Worker *w = m_worker.get();
    const unsigned gen = w->http.generation();
    post([this, w, gen, callback]() {
        Quota quota;
        QString error;
        bool signedIn = !w->token.isEmpty();
        if (signedIn)
        {
            quota.signedIn = true;
            quota.user = w->user;
            w->readUserInfo(quota);
        }
        else
        {
            QString user, password;
            if (w->loadCredentials(user, password))
            {
                error = w->login(user, password, quota);
                signedIn = error.isEmpty();
            }
        }

        QMetaObject::invokeMethod(this, [this, gen, signedIn, quota, error, callback]() {
            if (signedIn)
                setQuota(quota);
            if (callback && m_worker->http.generation() == gen)
                callback(error);
        }, Qt::QueuedConnection);
    });
}

void Client::signIn(const QString &user, const QString &password, DoneCallback callback)
{
    if (!available())
    {
        if (callback)
            callback(qtr("OpenSubtitles isn't available in this build."));
        return;
    }

    Worker *w = m_worker.get();
    const unsigned gen = w->http.generation();
    post([this, w, gen, user, password, callback]() {
        Quota quota;
        QString error = w->login(user, password, quota);
        if (error.isEmpty() && !w->storeCredentials(user, password))
            msg_Warn(w->obj, "unable to store the OpenSubtitles credentials");

        QMetaObject::invokeMethod(this, [this, gen, user, quota, error, callback]() {
            if (error.isEmpty())
            {
                setQuota(quota);
                setStoredUser(user);
            }
            if (callback && m_worker->http.generation() == gen)
                callback(error);
        }, Qt::QueuedConnection);
    });
}

void Client::signOut(DoneCallback callback)
{
    Worker *w = m_worker.get();
    const unsigned gen = w->http.generation();
    post([this, w, gen, callback]() {
        if (!w->token.isEmpty())
            w->call("DELETE", QStringLiteral("/logout"));
        w->token.clear();
        w->sessionUrl.clear();
        w->user.clear();
        w->forgetCredentials();

        QMetaObject::invokeMethod(this, [this, gen, callback]() {
            Quota quota;
            quota.remaining = anonymousDownloads;
            quota.allowed = anonymousDownloads;
            setQuota(quota);
            setStoredUser({});
            loadStoredQuota();
            if (callback && m_worker->http.generation() == gen)
                callback({});
        }, Qt::QueuedConnection);
    });
}

void Client::loadStoredUser()
{
    Worker *w = m_worker.get();
    post([this, w]() {
        QString user, password;
        w->loadCredentials(user, password);
        QMetaObject::invokeMethod(this, [this, user]() {
            setStoredUser(user);
        }, Qt::QueuedConnection);
    });
}

void Client::setStoredUser(const QString &user)
{
    if (user == m_storedUser)
        return;
    m_storedUser = user;
    emit storedUserChanged();
}

} // namespace opensubtitles
