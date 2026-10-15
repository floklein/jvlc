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

#include "open_capture_model.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_input_item.h>
#include <vlc_modules.h>

#include "util/shared_input_item.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QPointer>
#include <QRegularExpression>
#include <QThreadPool>

namespace {

QVariantMap choice(const QVariant& value, const QString& text)
{
    return QVariantMap{ { "value", value }, { "text", text } };
}

/* the suggested values of a configuration option, as [{ value, text }] */
QVariantList stringChoices(const char* name)
{
    char** values = nullptr;
    char** texts = nullptr;
    const ssize_t count = config_GetPszChoices(name, &values, &texts);
    QVariantList list;
    for (ssize_t i = 0; i < count; ++i)
    {
        const QString value = qfu(values[i]);
        const QString text = texts[i] ? qfu(texts[i]) : value;
        list.append(choice(value, text.isEmpty() ? value : text));
        free(values[i]);
        free(texts[i]);
    }
    free(values);
    free(texts);
    return list;
}

QVariantList intChoices(const char* name)
{
    int64_t* values = nullptr;
    char** texts = nullptr;
    const ssize_t count = config_GetIntChoices(name, &values, &texts);
    QVariantList list;
    for (ssize_t i = 0; i < count; ++i)
    {
        list.append(choice(static_cast<int>(values[i]), qfu(texts[i])));
        free(texts[i]);
    }
    free(values);
    free(texts);
    return list;
}

bool containsValue(const QVariantList& choices, const QVariant& value)
{
    return std::any_of(choices.cbegin(), choices.cend(), [&value](const QVariant& entry) {
        return entry.toMap().value("value") == value;
    });
}

QString textOfValue(const QVariantList& choices, const QVariant& value)
{
    for (const QVariant& entry : choices)
    {
        const QVariantMap map = entry.toMap();
        if (map.value("value") == value)
            return map.value("text").toString();
    }
    return {};
}

#ifndef _WIN32
/* video4linux device nodes */
QVariantList v4l2VideoDevices()
{
    QVariantList list;
    const QStringList nodes = QDir(QStringLiteral("/dev")).entryList({ QStringLiteral("video*") },
                                                                    QDir::System, QDir::Name);
    for (const QString& node : nodes)
    {
        const QString device = QStringLiteral("/dev/") + node;
        list.append(choice(device, device));
    }
    return list;
}

/* ALSA capture devices, as hw:card,device */
QVariantList alsaCaptureDevices()
{
    QVariantList list{ choice(QString(), qtr("None")) };
    static const QRegularExpression pcm(QStringLiteral("^pcmC(\\d+)D(\\d+)c$"));
    const QStringList nodes = QDir(QStringLiteral("/dev/snd")).entryList({ QStringLiteral("pcmC*D*c") },
                                                                         QDir::System, QDir::Name);
    for (const QString& node : nodes)
    {
        const QRegularExpressionMatch match = pcm.match(node);
        if (!match.hasMatch())
            continue;
        const QString device = QStringLiteral("hw:%1,%2").arg(match.captured(1), match.captured(2));
        list.append(choice(device, device));
    }
    return list;
}
#endif

} // namespace

OpenCaptureModel::OpenCaptureModel(qt_intf_t* intf, QObject* parent)
    : QObject(parent)
    , m_intf(intf)
    , m_advancedOptions(new ModuleOptionsModel(this))
    , m_preview(new CapturePreview(intf, this))
{
    /* the first installed type */
    const QVariantList types = deviceTypes();
    if (!types.isEmpty())
        m_deviceType = static_cast<DeviceType>(types.first().toMap().value("type").toInt());

#ifdef _WIN32
    /* no audio unless asked for: the default audio device is a microphone */
    m_audioDevice = QStringLiteral("none");
#endif
    if (module_exists("dtv"))
        m_tunerAdapter = config_GetInt("dvb-adapter");

    connect(m_advancedOptions, &ModuleOptionsModel::optionsChanged,
            this, &OpenCaptureModel::mediaChanged);
    connect(this, &OpenCaptureModel::mediaChanged, this, [this]() {
        m_preview->setSource(mrl(), options());
    });

    updateAdvancedOptions();
    m_preview->setSource(mrl(), options());
}

OpenCaptureModel::~OpenCaptureModel() = default;

QString OpenCaptureModel::moduleForType(DeviceType type)
{
    switch (type)
    {
    case Camera:
#ifdef _WIN32
        return QStringLiteral("dshow");
#else
        return QStringLiteral("v4l2");
#endif
    case Screen:
        /* the screen access is a different plugin on each platform */
        for (const char* name : { "screen_win", "screen", "xcb_screen" })
            if (module_exists(name))
                return QString::fromLatin1(name);
        return {};
    case TvTuner:
        return QStringLiteral("dtv");
    case CaptureCard:
        return QStringLiteral("decklink");
    }
    return {};
}

QVariantList OpenCaptureModel::deviceTypes() const
{
    const std::pair<DeviceType, QString> types[] = {
        { Camera, qtr("Camera / video device") },
        { Screen, qtr("Screen") },
        { TvTuner, qtr("TV tuner (DVB)") },
        { CaptureCard, qtr("Capture card (DeckLink)") },
    };

    QVariantList list;
    for (const auto& [type, text] : types)
    {
        const QString module = moduleForType(type);
        if (module.isEmpty() || !module_exists(qtu(module)))
            continue;
        list.append(QVariantMap{ { "type", type }, { "text", text } });
    }
    return list;
}

void OpenCaptureModel::setDeviceType(DeviceType type)
{
    if (m_deviceType == type)
        return;
    m_deviceType = type;
    emit deviceTypeChanged();
    updateAdvancedOptions();
    emit mediaChanged();
}

void OpenCaptureModel::setVideoDevice(const QString& device)
{
    if (m_videoDevice == device)
        return;
    m_videoDevice = device;
    emit videoDeviceChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setAudioDevice(const QString& device)
{
    if (m_audioDevice == device)
        return;
    m_audioDevice = device;
    emit audioDeviceChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setScreenFps(double fps)
{
    if (qFuzzyCompare(m_screenFps, fps))
        return;
    m_screenFps = fps;
    emit screenFpsChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setScreenRegion(const QRect& region)
{
    if (m_screenRegion == region)
        return;
    m_screenRegion = region;
    emit screenRegionChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setTunerSystem(TunerSystem system)
{
    if (m_tunerSystem == system)
        return;
    m_tunerSystem = system;
    emit tunerSystemChanged();

    /* keep a modulation the new system knows */
    const QVariantList modulations = tunerModulations();
    if (!containsValue(modulations, m_tunerModulation))
    {
        m_tunerModulation = modulations.isEmpty()
            ? QString() : modulations.first().toMap().value("value").toString();
        emit tunerModulationChanged();
    }
    emit mediaChanged();
}

void OpenCaptureModel::setTunerAdapter(int adapter)
{
    if (m_tunerAdapter == adapter)
        return;
    m_tunerAdapter = adapter;
    emit tunerAdapterChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setTunerFrequency(int frequency)
{
    if (m_tunerFrequency == frequency)
        return;
    m_tunerFrequency = frequency;
    emit tunerFrequencyChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setTunerSymbolRate(int rate)
{
    if (m_tunerSymbolRate == rate)
        return;
    m_tunerSymbolRate = rate;
    emit tunerSymbolRateChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setTunerModulation(const QString& modulation)
{
    if (m_tunerModulation == modulation)
        return;
    m_tunerModulation = modulation;
    emit tunerModulationChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setTunerBandwidth(int bandwidth)
{
    if (m_tunerBandwidth == bandwidth)
        return;
    m_tunerBandwidth = bandwidth;
    emit tunerBandwidthChanged();
    emit mediaChanged();
}

QVariantList OpenCaptureModel::tunerSystems() const
{
    return {
        choice(DvbT, QStringLiteral("DVB-T")),
        choice(DvbT2, QStringLiteral("DVB-T2")),
        choice(DvbC, QStringLiteral("DVB-C")),
        choice(DvbS, QStringLiteral("DVB-S")),
        choice(DvbS2, QStringLiteral("DVB-S2")),
        choice(Atsc, QStringLiteral("ATSC")),
        choice(ClearQam, qtr("Clear QAM")),
    };
}

QVariantList OpenCaptureModel::tunerModulations() const
{
    switch (m_tunerSystem)
    {
    case DvbC:
    case ClearQam:
        return {
            choice(QStringLiteral("QAM"), qtr("Automatic")),
            choice(QStringLiteral("256QAM"), QStringLiteral("256-QAM")),
            choice(QStringLiteral("128QAM"), QStringLiteral("128-QAM")),
            choice(QStringLiteral("64QAM"), QStringLiteral("64-QAM")),
            choice(QStringLiteral("32QAM"), QStringLiteral("32-QAM")),
            choice(QStringLiteral("16QAM"), QStringLiteral("16-QAM")),
        };
    case DvbS2:
        return {
            choice(QStringLiteral("QPSK"), QStringLiteral("QPSK")),
            choice(QStringLiteral("8PSK"), QStringLiteral("8-PSK")),
            choice(QStringLiteral("16APSK"), QStringLiteral("16-APSK")),
            choice(QStringLiteral("32APSK"), QStringLiteral("32-APSK")),
        };
    default:
        return {};
    }
}

QVariantList OpenCaptureModel::tunerBandwidths() const
{
    QVariantList list = intChoices("dvb-bandwidth");
    if (list.isEmpty())
        list.append(choice(0, qtr("Automatic")));
    return list;
}

bool OpenCaptureModel::tunerHasSymbolRate() const
{
    return m_tunerSystem == DvbC || m_tunerSystem == DvbS || m_tunerSystem == DvbS2;
}

bool OpenCaptureModel::tunerHasBandwidth() const
{
    return m_tunerSystem == DvbT || m_tunerSystem == DvbT2;
}

void OpenCaptureModel::setCardIndex(int index)
{
    if (m_cardIndex == index)
        return;
    m_cardIndex = index;
    emit cardIndexChanged();
    emit mediaChanged();
}

void OpenCaptureModel::setCardVideoConnection(const QString& connection)
{
    if (m_cardVideoConnection == connection)
        return;
    m_cardVideoConnection = connection;
    emit cardVideoConnectionChanged();
    emit mediaChanged();
}

QVariantList OpenCaptureModel::cardVideoConnections() const
{
    QVariantList list{ choice(QString(), qtr("Automatic")) };
    if (module_exists("decklink"))
        list.append(stringChoices("decklink-video-connection"));
    return list;
}

QString OpenCaptureModel::mrl() const
{
    switch (m_deviceType)
    {
    case Camera:
#ifdef _WIN32
        return QStringLiteral("dshow://");
#else
        return QStringLiteral("v4l2://") + m_videoDevice;
#endif
    case Screen:
        return QStringLiteral("screen://");
    case TvTuner:
    {
        static const char* const schemes[] = {
            "dvb-t", "dvb-t2", "dvb-c", "dvb-s", "dvb-s2", "atsc", "cqam",
        };
        /* the tuner takes Hz */
        QString mrl = QStringLiteral("%1://frequency=%2")
            .arg(QLatin1String(schemes[m_tunerSystem]))
            .arg(qint64(m_tunerFrequency) * 1000);
        if (!tunerModulations().isEmpty() && !m_tunerModulation.isEmpty())
            mrl += QStringLiteral(":modulation=") + m_tunerModulation;
        if (tunerHasSymbolRate())
            mrl += QStringLiteral(":srate=%1").arg(m_tunerSymbolRate);
        if (tunerHasBandwidth() && m_tunerBandwidth > 0)
            mrl += QStringLiteral(":bandwidth=%1").arg(m_tunerBandwidth);
        return mrl;
    }
    case CaptureCard:
        return QStringLiteral("decklink://");
    }
    return {};
}

QStringList OpenCaptureModel::basicOptions() const
{
    QStringList options;
    switch (m_deviceType)
    {
    case Camera:
#ifdef _WIN32
        options << QStringLiteral(":dshow-vdev=") + m_videoDevice
                << QStringLiteral(":dshow-adev=") + m_audioDevice;
#else
        if (!m_audioDevice.isEmpty())
            options << QStringLiteral(":input-slave=alsa://") + m_audioDevice;
#endif
        break;
    case Screen:
        options << QStringLiteral(":screen-fps=%1").arg(m_screenFps);
        if (!m_screenRegion.isEmpty())
            options << QStringLiteral(":screen-left=%1").arg(m_screenRegion.x())
                    << QStringLiteral(":screen-top=%1").arg(m_screenRegion.y())
                    << QStringLiteral(":screen-width=%1").arg(m_screenRegion.width())
                    << QStringLiteral(":screen-height=%1").arg(m_screenRegion.height());
        break;
    case TvTuner:
        options << QStringLiteral(":dvb-adapter=%1").arg(m_tunerAdapter);
        break;
    case CaptureCard:
        options << QStringLiteral(":decklink-card-index=%1").arg(m_cardIndex);
        if (!m_cardVideoConnection.isEmpty())
            options << QStringLiteral(":decklink-video-connection=") + m_cardVideoConnection;
        break;
    }
    return options;
}

QStringList OpenCaptureModel::options() const
{
    return basicOptions() + m_advancedOptions->options();
}

QString OpenCaptureModel::itemName() const
{
    switch (m_deviceType)
    {
    case Camera:
    {
        const QString text = textOfValue(m_videoDevices, m_videoDevice);
        if (!text.isEmpty() && !m_videoDevice.isEmpty())
            return text;
        return qtr("Camera");
    }
    case Screen:
        return qtr("Screen");
    case TvTuner:
        return qtr("TV tuner");
    case CaptureCard:
        return qtr("Capture card");
    }
    return {};
}

void OpenCaptureModel::updateAdvancedOptions()
{
    /* the options that have a basic field */
    static const QStringList basicNames[] = {
        /* Camera */
        { QStringLiteral("dshow-vdev"), QStringLiteral("dshow-adev"), QStringLiteral("v4l2-dev") },
        /* Screen */
        { QStringLiteral("screen-fps"), QStringLiteral("screen-left"), QStringLiteral("screen-top"),
          QStringLiteral("screen-width"), QStringLiteral("screen-height") },
        /* TvTuner */
        { QStringLiteral("dvb-adapter"), QStringLiteral("dvb-frequency"), QStringLiteral("dvb-srate"),
          QStringLiteral("dvb-modulation"), QStringLiteral("dvb-bandwidth") },
        /* CaptureCard */
        { QStringLiteral("decklink-card-index"), QStringLiteral("decklink-video-connection") },
    };

    m_advancedOptions->setExcludedOptions(basicNames[m_deviceType]);
    m_advancedOptions->setModule(moduleForType(m_deviceType));
}

void OpenCaptureModel::refreshDevices()
{
    /* DirectShow takes a while to enumerate the devices: not on the UI
     * thread */
    QPointer<OpenCaptureModel> self(this);
    QThreadPool::globalInstance()->start([self]() {
        QVariantList video;
        QVariantList audio;
#ifdef _WIN32
        if (module_exists("dshow"))
        {
            /* DirectShow lists "" (the default device) and "none" first,
             * unless its list callback cannot be reached */
            const auto withDefaults = [](QVariantList devices) {
                if (!containsValue(devices, QStringLiteral("none")))
                    devices.prepend(choice(QStringLiteral("none"), qtr("None")));
                if (!containsValue(devices, QString()))
                    devices.prepend(choice(QString(), qtr("Default")));
                return devices;
            };
            video = withDefaults(stringChoices("dshow-vdev"));
            audio = withDefaults(stringChoices("dshow-adev"));
        }
#else
        video = v4l2VideoDevices();
        audio = alsaCaptureDevices();
#endif
        QMetaObject::invokeMethod(qApp, [self, video, audio]() {
            if (self)
                self->setDevices(video, audio);
        }, Qt::QueuedConnection);
    });
}

void OpenCaptureModel::setDevices(const QVariantList& video, const QVariantList& audio)
{
    m_videoDevices = video;
    m_audioDevices = audio;
    emit devicesChanged();

    /* keep the selection if the device is still there */
    if (!m_videoDevices.isEmpty() && !containsValue(m_videoDevices, m_videoDevice))
        setVideoDevice(m_videoDevices.first().toMap().value("value").toString());
    if (!m_audioDevices.isEmpty() && !containsValue(m_audioDevices, m_audioDevice))
        setAudioDevice(m_audioDevices.first().toMap().value("value").toString());
}

QVariant OpenCaptureModel::inputItem() const
{
    const QString name = itemName();
    SharedInputItem item(input_item_New(qtu(mrl()), qtu(name)), false);
    if (!item)
        return {};

    for (const QString& option : options())
        input_item_AddOption(item.get(), qtu(option), VLC_INPUT_OPTION_TRUSTED);
    return QVariant::fromValue(item);
}
