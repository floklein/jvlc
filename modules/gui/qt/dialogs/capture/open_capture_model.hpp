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
#ifndef VLC_QT_OPEN_CAPTURE_MODEL_HPP
#define VLC_QT_OPEN_CAPTURE_MODEL_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QRect>
#include <QStringList>
#include <QVariant>

#include "qt.hpp"
#include "dialogs/capture/capture_preview.hpp"
#include "util/module_options_model.hpp"

/**
 * State of the Open Capture Device dialog.
 *
 * It lists the device types whose access module is installed, holds the
 * basic fields of each type, and turns them into the MRL and options of the
 * media to play. The capture options that have no basic field are in
 * advancedOptions, a form over the type's module options.
 */
class OpenCaptureModel : public QObject
{
    Q_OBJECT

public:
    enum DeviceType
    {
        Camera,
        Screen,
        TvTuner,
        CaptureCard,
    };
    Q_ENUM(DeviceType)

    enum TunerSystem
    {
        DvbT,
        DvbT2,
        DvbC,
        DvbS,
        DvbS2,
        Atsc,
        ClearQam,
    };
    Q_ENUM(TunerSystem)

    /* [{ type: DeviceType, text: string }] of the installed device types */
    Q_PROPERTY(QVariantList deviceTypes READ deviceTypes CONSTANT FINAL)
    Q_PROPERTY(DeviceType deviceType READ deviceType WRITE setDeviceType NOTIFY deviceTypeChanged FINAL)

    /* Camera: [{ value: string, text: string }] */
    Q_PROPERTY(QVariantList videoDevices READ videoDevices NOTIFY devicesChanged FINAL)
    Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY devicesChanged FINAL)
    Q_PROPERTY(QString videoDevice READ videoDevice WRITE setVideoDevice NOTIFY videoDeviceChanged FINAL)
    Q_PROPERTY(QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY audioDeviceChanged FINAL)

    /* Screen: an empty region captures the whole screen */
    Q_PROPERTY(double screenFps READ screenFps WRITE setScreenFps NOTIFY screenFpsChanged FINAL)
    Q_PROPERTY(QRect screenRegion READ screenRegion WRITE setScreenRegion NOTIFY screenRegionChanged FINAL)

    /* TV tuner */
    Q_PROPERTY(TunerSystem tunerSystem READ tunerSystem WRITE setTunerSystem NOTIFY tunerSystemChanged FINAL)
    Q_PROPERTY(int tunerAdapter READ tunerAdapter WRITE setTunerAdapter NOTIFY tunerAdapterChanged FINAL)
    Q_PROPERTY(int tunerFrequency READ tunerFrequency WRITE setTunerFrequency NOTIFY tunerFrequencyChanged FINAL) /* kHz */
    Q_PROPERTY(int tunerSymbolRate READ tunerSymbolRate WRITE setTunerSymbolRate NOTIFY tunerSymbolRateChanged FINAL)
    Q_PROPERTY(QString tunerModulation READ tunerModulation WRITE setTunerModulation NOTIFY tunerModulationChanged FINAL)
    Q_PROPERTY(int tunerBandwidth READ tunerBandwidth WRITE setTunerBandwidth NOTIFY tunerBandwidthChanged FINAL) /* MHz, 0 = auto */
    Q_PROPERTY(QVariantList tunerSystems READ tunerSystems CONSTANT FINAL)
    Q_PROPERTY(QVariantList tunerModulations READ tunerModulations NOTIFY tunerSystemChanged FINAL)
    Q_PROPERTY(QVariantList tunerBandwidths READ tunerBandwidths CONSTANT FINAL)
    Q_PROPERTY(bool tunerHasSymbolRate READ tunerHasSymbolRate NOTIFY tunerSystemChanged FINAL)
    Q_PROPERTY(bool tunerHasBandwidth READ tunerHasBandwidth NOTIFY tunerSystemChanged FINAL)

    /* Capture card (DeckLink) */
    Q_PROPERTY(int cardIndex READ cardIndex WRITE setCardIndex NOTIFY cardIndexChanged FINAL)
    Q_PROPERTY(QString cardVideoConnection READ cardVideoConnection WRITE setCardVideoConnection NOTIFY cardVideoConnectionChanged FINAL)
    Q_PROPERTY(QVariantList cardVideoConnections READ cardVideoConnections CONSTANT FINAL)

    Q_PROPERTY(QString mrl READ mrl NOTIFY mediaChanged FINAL)
    Q_PROPERTY(QStringList options READ options NOTIFY mediaChanged FINAL)
    Q_PROPERTY(ModuleOptionsModel* advancedOptions READ advancedOptions CONSTANT FINAL)
    Q_PROPERTY(CapturePreview* preview READ preview CONSTANT FINAL)

public:
    explicit OpenCaptureModel(qt_intf_t* intf, QObject* parent = nullptr);
    ~OpenCaptureModel() override;

    /* the access module that captures from this type of device */
    static QString moduleForType(DeviceType type);

    QVariantList deviceTypes() const;
    DeviceType deviceType() const { return m_deviceType; }
    void setDeviceType(DeviceType type);

    QVariantList videoDevices() const { return m_videoDevices; }
    QVariantList audioDevices() const { return m_audioDevices; }
    QString videoDevice() const { return m_videoDevice; }
    void setVideoDevice(const QString& device);
    QString audioDevice() const { return m_audioDevice; }
    void setAudioDevice(const QString& device);

    double screenFps() const { return m_screenFps; }
    void setScreenFps(double fps);
    QRect screenRegion() const { return m_screenRegion; }
    void setScreenRegion(const QRect& region);

    TunerSystem tunerSystem() const { return m_tunerSystem; }
    void setTunerSystem(TunerSystem system);
    int tunerAdapter() const { return m_tunerAdapter; }
    void setTunerAdapter(int adapter);
    int tunerFrequency() const { return m_tunerFrequency; }
    void setTunerFrequency(int frequency);
    int tunerSymbolRate() const { return m_tunerSymbolRate; }
    void setTunerSymbolRate(int rate);
    QString tunerModulation() const { return m_tunerModulation; }
    void setTunerModulation(const QString& modulation);
    int tunerBandwidth() const { return m_tunerBandwidth; }
    void setTunerBandwidth(int bandwidth);
    QVariantList tunerSystems() const;
    QVariantList tunerModulations() const;
    QVariantList tunerBandwidths() const;
    bool tunerHasSymbolRate() const;
    bool tunerHasBandwidth() const;

    int cardIndex() const { return m_cardIndex; }
    void setCardIndex(int index);
    QString cardVideoConnection() const { return m_cardVideoConnection; }
    void setCardVideoConnection(const QString& connection);
    QVariantList cardVideoConnections() const;

    QString mrl() const;
    /* basic fields, then the changed advanced options */
    QStringList options() const;

    ModuleOptionsModel* advancedOptions() const { return m_advancedOptions; }
    CapturePreview* preview() const { return m_preview; }

    /* re-list the cameras and microphones */
    Q_INVOKABLE void refreshDevices();
    /* a SharedInputItem of the capture, to hand to the Queue */
    Q_INVOKABLE QVariant inputItem() const;

signals:
    void deviceTypeChanged();
    void devicesChanged();
    void videoDeviceChanged();
    void audioDeviceChanged();
    void screenFpsChanged();
    void screenRegionChanged();
    void tunerSystemChanged();
    void tunerAdapterChanged();
    void tunerFrequencyChanged();
    void tunerSymbolRateChanged();
    void tunerModulationChanged();
    void tunerBandwidthChanged();
    void cardIndexChanged();
    void cardVideoConnectionChanged();
    void mediaChanged();

private:
    QStringList basicOptions() const;
    QString itemName() const;
    void updateAdvancedOptions();

    qt_intf_t* m_intf = nullptr;

    DeviceType m_deviceType = Camera;

    QVariantList m_videoDevices;
    QVariantList m_audioDevices;
    QString m_videoDevice;
    QString m_audioDevice;

    double m_screenFps = 10.;
    QRect m_screenRegion;

    TunerSystem m_tunerSystem = DvbT;
    int m_tunerAdapter = 0;
    int m_tunerFrequency = 0;
    int m_tunerSymbolRate = 0;
    QString m_tunerModulation;
    int m_tunerBandwidth = 0;

    int m_cardIndex = 0;
    QString m_cardVideoConnection;

    ModuleOptionsModel* m_advancedOptions = nullptr;
    CapturePreview* m_preview = nullptr;
};

#endif // VLC_QT_OPEN_CAPTURE_MODEL_HPP
