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

#include "conversion_profile_model.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_modules.h>

#include "dialogs/sout/profiles.hpp"
#include "util/soutchain.hpp"

#include <QSettings>
#include <QRegularExpression>
#include <QUrl>

namespace {

const char PROFILES_KEY[] = "codecs-profiles";
const char SELECTED_KEY[] = "codecs-profiles-selected";
const char OLD_FORMAT[] = "^\\w+;\\d+;\\d+;\\d+;";

using Properties = QHash<QString, QString>;

/* Profiles of VLC 1.0: "mux;video;audio;subs;vcodec;vb;scale;fps;width;
 * height;acodec;ab;channels;samplerate;scodec;soverlay" */
Properties parseOldFormat(const QString& value)
{
    Properties props;
    const QStringList options = value.split(';');
    if (options.count() < 16)
        return props;

    props["muxer_mux"] = options[0];
    if (options[4] != "none")
    {
        props["video_enable"] = "yes";
        if (!options[1].toInt())
            props["video_copy"] = "yes";
        else
        {
            props["video_codec"] = options[4];
            props["vcodec_bitrate"] = options[5];
            if (options[6].toDouble() > 0)
                props["vcodec_scale"] = options[6];
            props["vcodec_framerate"] = options[7];
            props["vcodec_width"] = options[8];
            props["vcodec_height"] = options[9];
        }
    }
    if (options[10] != "none")
    {
        props["audio_enable"] = "yes";
        if (!options[2].toInt())
            props["audio_copy"] = "yes";
        else
        {
            props["audio_codec"] = options[10];
            props["acodec_bitrate"] = options[11];
            props["acodec_channels"] = options[12];
            props["acodec_samplerate"] = options[13];
        }
    }
    if (options[3].toInt())
    {
        props["subtitles_enable"] = "yes";
        props["subtitles_codec"] = options[14];
        if (options[15].toInt())
            props["subtitles_overlay"] = "yes";
    }
    return props;
}

Properties parse(const QString& value)
{
    if (QRegularExpression(QString::fromLatin1(OLD_FORMAT)).match(value).hasMatch())
        return parseOldFormat(value);

    Properties props;
    const QStringList tuples = value.split(';');
    for (const QString& tuple : tuples)
    {
        const QStringList keyvalue = tuple.split('=');
        if (keyvalue.count() != 2 || keyvalue[0].split('_').count() != 2)
            continue;
        props.insert(keyvalue[0], keyvalue[1]);
    }
    return props;
}

QString percentDecoded(const QString& value)
{
    return QUrl::fromPercentEncoding(value.toLatin1());
}

QString percentEncoded(const QString& value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value, "", "_;"));
}

QString quoted(const QString& value)
{
    char* escaped = config_StringEscape(qtu(value));
    if (!escaped)
        return {};
    QString ret = QStringLiteral("'%1'").arg(qfu(escaped));
    free(escaped);
    return ret;
}

struct Muxer {
    const char* value;
    const char* text;
    const char* module;
    bool video, audio, menus, subtitles, streamable, chapters;
};

/* same capabilities as the profile editor of VLC 3 */
const Muxer muxerList[] = {
    { "mp4",    "MP4/MOV",  "mp4",      true,  true,  true,  true,  true,  false },
    { "mkv",    "MKV",      "avformat", true,  true,  true,  true,  true,  true  },
    { "webm",   "WebM",     "avformat", true,  true,  false, false, true,  false },
    { "ts",     "MPEG-TS",  "ts",       true,  true,  false, true,  true,  false },
    { "ps",     "MPEG-PS",  "ps",       true,  true,  false, true,  false, true  },
    { "mpeg1",  "MPEG 1",   "ps",       true,  true,  false, false, false, false },
    { "ogg",    "Ogg/Ogm",  "ogg",      true,  true,  false, false, true,  true  },
    { "asf",    "ASF/WMV",  "asf",      true,  true,  false, true,  true,  true  },
    { "avi",    "AVI",      "avi",      true,  true,  false, false, false, false },
    { "flv",    "FLV",      "avformat", true,  true,  false, false, true,  false },
    { "mpjpeg", "MJPEG",    "mpjpeg",   true,  false, false, false, false, false },
    { "raw",    "RAW",      "dummy",    true,  true,  false, false, false, false },
    { "mp3",    "MP3",      "dummy",    false, true,  false, false, false, false },
    { "flac",   "FLAC",     "dummy",    false, true,  false, false, false, false },
    { "wav",    "WAV",      "wav",      false, true,  false, false, false, false },
};

QVariantList choices(std::initializer_list<std::pair<const char*, const char*>> list)
{
    QVariantList ret;
    for (const auto& [text, value] : list)
        ret.append(QVariantMap{ { "text", qfu(text) }, { "value", qfu(value) } });
    return ret;
}

QVariantList filters(const char* capability)
{
    QVariantList ret;
    size_t count;
    module_t** modules = module_list_get(&count);
    for (size_t i = 0; i < count; i++)
    {
        module_t* module = modules[i];
        if (module_get_score(module) > 0 || strcmp(module_get_capability(module), capability))
            continue;
        ret.append(QVariantMap{
            { "text", qfu(module_GetLongName(module)) },
            { "value", qfu(module_get_object(module)) },
            { "help", qfu(module_get_help(module)) },
        });
    }
    module_list_free(modules);
    std::sort(ret.begin(), ret.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap().value("text").toString().localeAwareCompare(
                   b.toMap().value("text").toString()) < 0;
    });
    return ret;
}

const char* boolKeys[] = {
    "video_enable", "video_copy", "audio_enable", "audio_copy",
    "subtitles_enable", "subtitles_overlay",
};
const char* intKeys[] = {
    "vcodec_bitrate", "vcodec_qp", "vcodec_width", "vcodec_height",
    "acodec_bitrate", "acodec_channels", "acodec_samplerate",
};
const char* doubleKeys[] = { "vcodec_framerate", "acodec_gain" };
const char* stringKeys[] = {
    "muxer_mux", "video_codec", "vcodec_scale", "audio_codec", "subtitles_codec",
};

} // anonymous namespace

ConversionProfileModel::ConversionProfileModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_ownSettings(std::make_unique<QSettings>(
#ifdef _WIN32
          QSettings::IniFormat,
#else
          QSettings::NativeFormat,
#endif
          QSettings::UserScope, "vlc", "vlc-qt-interface"))
    , m_settings(m_ownSettings.get())
{
    load();
}

ConversionProfileModel::ConversionProfileModel(QSettings* settings, QObject* parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
{
    load();
}

ConversionProfileModel::~ConversionProfileModel() = default;

void ConversionProfileModel::load()
{
    const int size = m_settings->beginReadArray(PROFILES_KEY);
    for (int i = 0; i < size; i++)
    {
        m_settings->setArrayIndex(i);
        const QString name = m_settings->value("Profile-Name").toString();
        if (name.isEmpty())
            continue;
        m_profiles.append({ name, m_settings->value("Profile-Value").toString() });
    }
    m_settings->endArray();

    if (size == 0)
    {
        for (size_t i = 0; i < NB_PROFILE; i++)
            m_profiles.append({ qfu(video_profile_name_list[i]),
                                qfu(video_profile_value_list[i]) });
    }

    m_selected = std::max(indexOf(m_settings->value(SELECTED_KEY).toString()), 0);
}

void ConversionProfileModel::store() const
{
    m_settings->remove(PROFILES_KEY);
    m_settings->beginWriteArray(PROFILES_KEY);
    for (int i = 0; i < m_profiles.size(); i++)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue("Profile-Name", m_profiles[i].name);
        m_settings->setValue("Profile-Value", m_profiles[i].value);
    }
    m_settings->endArray();
    if (m_selected < m_profiles.size())
        m_settings->setValue(SELECTED_KEY, m_profiles[m_selected].name);
    m_settings->sync();
}

bool ConversionProfileModel::isBuiltIn(const Profile& profile) const
{
    for (size_t i = 0; i < NB_PROFILE; i++)
        if (profile.name == qfu(video_profile_name_list[i])
            && profile.value == qfu(video_profile_value_list[i]))
            return true;
    return false;
}

int ConversionProfileModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_profiles.size();
}

QVariant ConversionProfileModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Profile& profile = m_profiles[index.row()];
    switch (role)
    {
    case Qt::DisplayRole:
    case NAME:
        return profile.name;
    case VALUE:
        return profile.value;
    case BUILTIN:
        return isBuiltIn(profile);
    default:
        return {};
    }
}

QHash<int, QByteArray> ConversionProfileModel::roleNames() const
{
    return {
        { NAME, "name" },
        { VALUE, "value" },
        { BUILTIN, "builtin" },
    };
}

int ConversionProfileModel::selected() const
{
    return m_selected;
}

void ConversionProfileModel::setSelected(int row)
{
    if (row < 0 || row >= m_profiles.size() || row == m_selected)
        return;
    m_selected = row;
    store();
    emit selectedChanged();
}

QString ConversionProfileModel::selectedName() const
{
    return m_selected < m_profiles.size() ? m_profiles[m_selected].name : QString();
}

QString ConversionProfileModel::selectedValue() const
{
    return m_selected < m_profiles.size() ? m_profiles[m_selected].value : QString();
}

bool ConversionProfileModel::selectedBuiltIn() const
{
    return m_selected < m_profiles.size() && isBuiltIn(m_profiles[m_selected]);
}

int ConversionProfileModel::indexOf(const QString& name) const
{
    for (int i = 0; i < m_profiles.size(); i++)
        if (m_profiles[i].name == name)
            return i;
    return -1;
}

int ConversionProfileModel::save(const QString& name, const QString& value)
{
    if (name.isEmpty())
        return -1;

    int row = indexOf(name);
    if (row >= 0)
    {
        m_profiles[row].value = value;
        emit dataChanged(index(row), index(row));
        if (row == m_selected)
            emit selectedChanged();
    }
    else
    {
        row = m_profiles.size();
        beginInsertRows({}, row, row);
        m_profiles.append({ name, value });
        endInsertRows();
        emit countChanged();
    }
    store();
    return row;
}

void ConversionProfileModel::remove(int row)
{
    if (row < 0 || row >= m_profiles.size())
        return;

    beginRemoveRows({}, row, row);
    m_profiles.removeAt(row);
    endRemoveRows();
    emit countChanged();

    const int selected = m_selected;
    if (m_selected > row)
        m_selected--;
    else if (m_selected >= m_profiles.size())
        m_selected = std::max(int(m_profiles.size()) - 1, 0);
    store();
    if (selected != m_selected || selected == row)
        emit selectedChanged();
}

QVariantMap ConversionProfileModel::fields(const QString& value) const
{
    const Properties props = parse(value);
    QVariantMap ret;

    for (const char* key : boolKeys)
        ret[key] = !props.value(key).isEmpty();
    for (const char* key : intKeys)
        ret[key] = props.value(key).toInt();
    for (const char* key : doubleKeys)
        ret[key] = props.value(key).toDouble();
    for (const char* key : stringKeys)
        ret[key] = props.value(key);

    ret["vcodec_custom"] = percentDecoded(props.value("vcodec_custom"));
    for (const char* key : { "video_filters", "audio_filters" })
    {
        const QString list = percentDecoded(props.value(key));
        ret[key] = list.isEmpty() ? QStringList{} : list.split(';');
    }

    /* defaults of a new profile */
    if (ret["muxer_mux"].toString().isEmpty())
        ret["muxer_mux"] = "mp4";
    if (!props.contains("acodec_bitrate"))
        ret["acodec_bitrate"] = 128;
    if (!props.contains("acodec_channels"))
        ret["acodec_channels"] = 2;
    if (!props.contains("acodec_samplerate"))
        ret["acodec_samplerate"] = 44100;
    return ret;
}

QString ConversionProfileModel::value(const QVariantMap& fields) const
{
    QStringList ret;
    const auto add = [&](const char* key, const QString& value) {
        if (!value.isEmpty())
            ret << QStringLiteral("%1=%2").arg(key, value);
    };
    const auto addNumber = [&](const char* key) {
        add(key, QString::number(fields.value(key).toDouble()));
    };
    const auto addBool = [&](const char* key) {
        add(key, fields.value(key).toBool() ? QStringLiteral("yes") : QString());
    };

    if (fields.value("video_enable").toBool())
    {
        addBool("video_enable");
        addBool("video_copy");
        if (!fields.value("video_copy").toBool())
        {
            add("video_codec", fields.value("video_codec").toString());
            addNumber("vcodec_bitrate");
            addNumber("vcodec_qp");
            addNumber("vcodec_framerate");
            add("vcodec_custom", percentEncoded(fields.value("vcodec_custom").toString()));
            add("vcodec_scale", fields.value("vcodec_scale").toString());
            addNumber("vcodec_width");
            addNumber("vcodec_height");
            add("video_filters", percentEncoded(fields.value("video_filters").toStringList().join(';')));
        }
    }
    if (fields.value("audio_enable").toBool())
    {
        addBool("audio_enable");
        addBool("audio_copy");
        if (!fields.value("audio_copy").toBool())
        {
            add("audio_codec", fields.value("audio_codec").toString());
            addNumber("acodec_bitrate");
            addNumber("acodec_channels");
            addNumber("acodec_samplerate");
            addNumber("acodec_gain");
            add("audio_filters", percentEncoded(fields.value("audio_filters").toStringList().join(';')));
        }
    }
    if (fields.value("subtitles_enable").toBool())
    {
        addBool("subtitles_enable");
        if (fields.value("subtitles_overlay").toBool())
            addBool("subtitles_overlay");
        else
            add("subtitles_codec", fields.value("subtitles_codec").toString());
    }
    add("muxer_mux", fields.value("muxer_mux").toString());
    return ret.join(';');
}

SoutChain ConversionProfileModel::transcodeChain(const QString& value, QString* mux)
{
    Properties props = parse(value);
    SoutChain transcode;

    const QString muxer = props.value("muxer_mux");
    if (mux)
        *mux = muxer;
    if (muxer.isEmpty())
        return transcode;

    transcode.begin("transcode");

    if (!props.value("video_enable").isEmpty())
    {
        const QString codec = props.value("video_codec");
        if (!codec.isEmpty() && props.value("video_copy").isEmpty())
        {
            transcode.option("vcodec", codec);

            if (props.value("vcodec_bitrate").toInt() > 0)
                transcode.option("vb", props.value("vcodec_bitrate").toInt());

            const QString filters = props.value("video_filters");
            if (!filters.isEmpty())
                transcode.option("vfilter", percentDecoded(filters).split(';').join(':'));

            QStringList codecOptions;
            if (props.value("vcodec_qp").toInt() > 0)
                codecOptions << QStringLiteral("qp=%1").arg(props.value("vcodec_qp"));
            if (!props.value("vcodec_custom").isEmpty())
                codecOptions << percentDecoded(props.value("vcodec_custom"));
            if (!codecOptions.isEmpty())
                transcode.option("venc", QStringLiteral("x264{%1}").arg(codecOptions.join(',')));

            if (props.value("vcodec_framerate").toDouble() > 0)
                transcode.option("fps", props.value("vcodec_framerate"));
            if (!props.value("vcodec_scale").isEmpty())
                transcode.option("scale", props.value("vcodec_scale"));
            if (props.value("vcodec_width").toInt() > 0)
                transcode.option("width", props.value("vcodec_width"));
            if (props.value("vcodec_height").toInt() > 0)
                transcode.option("height", props.value("vcodec_height"));
        }
    }
    else
        transcode.option("vcodec", "none");

    if (!props.value("audio_enable").isEmpty())
    {
        const QString codec = props.value("audio_codec");
        if (!codec.isEmpty() && props.value("audio_copy").isEmpty())
        {
            transcode.option("acodec", codec);
            transcode.option("ab", props.value("acodec_bitrate").toInt());
            transcode.option("channels", props.value("acodec_channels").toInt());
            transcode.option("samplerate", props.value("acodec_samplerate").toInt());

            QStringList audioFilters;
            if (!props.value("audio_filters").isEmpty())
                audioFilters << percentDecoded(props.value("audio_filters")).split(';');

            /* normvol peak-level multiplier: 100% = 1.0 (no change),
             * 200% = 2.0 (double volume), 50% = 0.5 (half volume). */
            const double gain = props.value("acodec_gain").toDouble();
            if (gain > 0.0)
                audioFilters.prepend(QStringLiteral("normvol{max-level=%1}")
                                         .arg(gain / 100.0, 0, 'f', 2));

            if (!audioFilters.isEmpty())
                transcode.option("afilter", audioFilters.join(':'));
        }
    }
    else
        transcode.option("acodec", "none");

    if (!props.value("subtitles_enable").isEmpty())
    {
        if (props.value("subtitles_overlay").isEmpty())
            transcode.option("scodec", props.value("subtitles_codec"));
        else
            transcode.option("soverlay");
    }
    else
        transcode.option("scodec", "none");

    return transcode;
}

QString ConversionProfileModel::soutChain(const QString& value, const QString& destination,
                                          bool deinterlace, bool overwrite)
{
    QString mux;
    SoutChain chain = transcodeChain(value, &mux);
    if (mux.isEmpty() || destination.isEmpty())
        return {};

    if (deinterlace)
        chain.option("deinterlace");
    chain.header("#");

    SoutModule access("file");
    if (!overwrite)
        access.option("no-overwrite");

    SoutModule std("std");
    std.option("access", access);
    std.option("mux", mux);
    std.option("dst", quoted(destination), true);
    chain.module(std);

    return chain.to_string();
}

QString ConversionProfileModel::fileExtension(const QString& value)
{
    const QString mux = parse(value).value("muxer_mux");
    if (mux == "ps" || mux == "mpeg1")
        return QStringLiteral("mpg");
    if (mux == "mpjpeg")
        return QStringLiteral("mjpg");
    return mux;
}

QVariantList ConversionProfileModel::muxers() const
{
    QSet<QString> available;
    size_t count;
    module_t** modules = module_list_get(&count);
    for (size_t i = 0; i < count; i++)
        if (module_provides(modules[i], "sout mux"))
            available.insert(qfu(module_get_object(modules[i])));
    module_list_free(modules);

    QVariantList ret;
    for (const Muxer& muxer : muxerList)
    {
        const QString module = qfu(muxer.module);
        ret.append(QVariantMap{
            { "value", qfu(muxer.value) },
            { "text", qfu(muxer.text) },
            /* avformat muxers are not provided by VLC itself */
            { "external", module == "avformat" },
            { "available", available.contains(module) || available.contains("mux_" + module) },
            { "video", muxer.video },
            { "audio", muxer.audio },
            { "menus", muxer.menus },
            { "subtitles", muxer.subtitles },
            { "streamable", muxer.streamable },
            { "chapters", muxer.chapters },
        });
    }
    return ret;
}

QVariantList ConversionProfileModel::videoCodecs() const
{
    return choices({
        { "H-264 (AVC)", "h264" }, { "H-265 (HEVC)", "hevc" }, { "AV1", "av01" },
        { "VP8", "VP80" }, { "MPEG-1", "mp1v" }, { "MPEG-2", "mp2v" },
        { "MPEG-4", "mp4v" }, { "DIVX 1", "DIV1" }, { "DIVX 2", "DIV2" },
        { "DIVX 3", "DIV3" }, { "H-263", "H263" }, { "WMV1", "WMV1" },
        { "WMV2", "WMV2" }, { "M-JPEG", "MJPG" }, { "Theora", "theo" },
    });
}

QVariantList ConversionProfileModel::audioCodecs() const
{
    return choices({
        { "MPEG 4 Audio (AAC)", "mp4a" }, { "MP3", "mp3" }, { "MPEG Audio", "mpga" },
        { "A52/AC-3", "a52" }, { "Vorbis", "vorb" }, { "FLAC", "flac" },
        { "Opus", "opus" }, { "Speex", "spx" }, { "PCM 16-bit", "s16l" },
        { "WMA2", "wma2" },
    });
}

QVariantList ConversionProfileModel::subtitleCodecs() const
{
    return choices({
        { "DVBS (DVB subtitles)", "dvbs" },
        { "tx3g (MPEG-4 timed text)", "tx3g" },
        { "T-REC 140 (for RTP)", "t140" },
    });
}

QVariantList ConversionProfileModel::sampleRates() const
{
    QVariantList ret;
    for (int rate : { 8000, 11025, 22050, 44100, 48000 })
        ret.append(QVariantMap{ { "text", QStringLiteral("%1 Hz").arg(rate) }, { "value", rate } });
    return ret;
}

QVariantList ConversionProfileModel::videoFilters() const
{
    return filters("video filter");
}

QVariantList ConversionProfileModel::audioFilters() const
{
    return filters("audio filter");
}
