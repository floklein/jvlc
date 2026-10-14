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
#include <QTest>
#include <QSignalSpy>

#include "vlc_stub_modules.hpp"

#include <vlc_modules.h>

#include "../dialogs/capture/open_capture_model.hpp"
#include "../util/module_options_model.hpp"
#include "../util/shared_input_item.hpp"
#include "../dialogs/capture/capture_preview.hpp"

#define MOCK_VIDEO "mock://video_track_count=1;audio_track_count=0;length=10000000"

/* the mock demux is a test plugin: the Windows install has none */
#define SKIP_WITHOUT_MOCK() \
    do { if (!module_exists("demux_mock")) QSKIP("no mock demux"); } while (0)

class TestOpenCaptureModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
    }

    void cleanupTestCase()
    {
        m_env.reset();
    }

    void init()
    {
        m_model = std::make_unique<OpenCaptureModel>(m_env->intf);
    }

    void cleanup()
    {
        m_model.reset();
    }

    void testDeviceTypesFollowTheInstalledModules()
    {
        const QVariantList types = m_model->deviceTypes();
        for (const QVariant& entry : types)
        {
            const QVariantMap type = entry.toMap();
            QVERIFY(!type.value("text").toString().isEmpty());
            const auto id = static_cast<OpenCaptureModel::DeviceType>(type.value("type").toInt());
            const QString module = OpenCaptureModel::moduleForType(id);
            QVERIFY2(!module.isEmpty() && module_exists(qtu(module)), qtu(module));
        }

        if (module_exists("dtv"))
            QVERIFY(hasType(types, OpenCaptureModel::TvTuner));
        if (module_exists("decklink"))
            QVERIFY(hasType(types, OpenCaptureModel::CaptureCard));
        else
            QVERIFY(!hasType(types, OpenCaptureModel::CaptureCard));
    }

    void testScreenCapturesTheWholeScreenAtTheFrameRate()
    {
        m_model->setDeviceType(OpenCaptureModel::Screen);
        m_model->setScreenFps(12.5);

        QCOMPARE(m_model->mrl(), QStringLiteral("screen://"));
        QCOMPARE(m_model->options(), QStringList{ QStringLiteral(":screen-fps=12.5") });
    }

    void testScreenCapturesARegion()
    {
        m_model->setDeviceType(OpenCaptureModel::Screen);
        m_model->setScreenFps(5);
        m_model->setScreenRegion(QRect(10, 20, 640, 480));

        QCOMPARE(m_model->mrl(), QStringLiteral("screen://"));
        QCOMPARE(m_model->options(), (QStringList{
            QStringLiteral(":screen-fps=5"),
            QStringLiteral(":screen-left=10"),
            QStringLiteral(":screen-top=20"),
            QStringLiteral(":screen-width=640"),
            QStringLiteral(":screen-height=480"),
        }));
    }

    void testTunerTunesTheFrequency_data()
    {
        QTest::addColumn<int>("system");
        QTest::addColumn<int>("symbolRate");
        QTest::addColumn<QString>("modulation");
        QTest::addColumn<int>("bandwidth");
        QTest::addColumn<QString>("mrl");

        QTest::newRow("DVB-T") << int(OpenCaptureModel::DvbT) << 0 << QString() << 8
                               << "dvb-t://frequency=474000000:bandwidth=8";
        QTest::newRow("DVB-T2, automatic bandwidth") << int(OpenCaptureModel::DvbT2) << 0 << QString() << 0
                               << "dvb-t2://frequency=474000000";
        QTest::newRow("DVB-C") << int(OpenCaptureModel::DvbC) << 6900000 << "256QAM" << 8
                               << "dvb-c://frequency=474000000:modulation=256QAM:srate=6900000";
        QTest::newRow("DVB-S") << int(OpenCaptureModel::DvbS) << 27500000 << "8PSK" << 0
                               << "dvb-s://frequency=474000000:srate=27500000";
        QTest::newRow("DVB-S2") << int(OpenCaptureModel::DvbS2) << 27500000 << "8PSK" << 0
                               << "dvb-s2://frequency=474000000:modulation=8PSK:srate=27500000";
        QTest::newRow("ATSC") << int(OpenCaptureModel::Atsc) << 0 << QString() << 0
                               << "atsc://frequency=474000000";
        QTest::newRow("Clear QAM") << int(OpenCaptureModel::ClearQam) << 0 << "64QAM" << 0
                               << "cqam://frequency=474000000:modulation=64QAM";
    }

    void testTunerTunesTheFrequency()
    {
        QFETCH(int, system);
        QFETCH(int, symbolRate);
        QFETCH(QString, modulation);
        QFETCH(int, bandwidth);
        QFETCH(QString, mrl);

        m_model->setDeviceType(OpenCaptureModel::TvTuner);
        m_model->setTunerSystem(static_cast<OpenCaptureModel::TunerSystem>(system));
        m_model->setTunerFrequency(474000);
        m_model->setTunerSymbolRate(symbolRate);
        m_model->setTunerModulation(modulation);
        m_model->setTunerBandwidth(bandwidth);
        m_model->setTunerAdapter(1);

        QCOMPARE(m_model->mrl(), mrl);
        QCOMPARE(m_model->options(), QStringList{ QStringLiteral(":dvb-adapter=1") });
    }

    void testTunerFieldsFollowTheDeliverySystem()
    {
        m_model->setTunerSystem(OpenCaptureModel::DvbT);
        QVERIFY(m_model->tunerHasBandwidth());
        QVERIFY(!m_model->tunerHasSymbolRate());
        QVERIFY(m_model->tunerModulations().isEmpty());

        m_model->setTunerSystem(OpenCaptureModel::DvbC);
        QVERIFY(!m_model->tunerHasBandwidth());
        QVERIFY(m_model->tunerHasSymbolRate());
        QVERIFY(hasValue(m_model->tunerModulations(), "256QAM"));

        m_model->setTunerSystem(OpenCaptureModel::DvbS2);
        QVERIFY(m_model->tunerHasSymbolRate());
        QVERIFY(hasValue(m_model->tunerModulations(), "8PSK"));
        QVERIFY(!hasValue(m_model->tunerModulations(), "256QAM"));
    }

    void testCaptureCardOpensTheCardInput()
    {
        m_model->setDeviceType(OpenCaptureModel::CaptureCard);
        m_model->setCardIndex(1);
        m_model->setCardVideoConnection(QStringLiteral("sdi"));

        QCOMPARE(m_model->mrl(), QStringLiteral("decklink://"));
        QCOMPARE(m_model->options(), (QStringList{
            QStringLiteral(":decklink-card-index=1"),
            QStringLiteral(":decklink-video-connection=sdi"),
        }));

        m_model->setCardVideoConnection({});
        QCOMPARE(m_model->options(), QStringList{ QStringLiteral(":decklink-card-index=1") });
    }

    void testCameraOpensTheVideoAndAudioDevices()
    {
        m_model->setDeviceType(OpenCaptureModel::Camera);
        m_model->setVideoDevice(QStringLiteral("Cam"));
        m_model->setAudioDevice(QStringLiteral("Mic"));
#ifdef _WIN32
        QCOMPARE(m_model->mrl(), QStringLiteral("dshow://"));
        QCOMPARE(m_model->options(), (QStringList{
            QStringLiteral(":dshow-vdev=Cam"),
            QStringLiteral(":dshow-adev=Mic"),
        }));
#else
        QCOMPARE(m_model->mrl(), QStringLiteral("v4l2://Cam"));
        QCOMPARE(m_model->options(), QStringList{ QStringLiteral(":input-slave=alsa://Mic") });
#endif
    }

    void testCameraWithoutAudioDevice()
    {
        m_model->setDeviceType(OpenCaptureModel::Camera);
        m_model->setVideoDevice(QStringLiteral("Cam"));
#ifdef _WIN32
        m_model->setAudioDevice(QStringLiteral("none"));
        QCOMPARE(m_model->options(), (QStringList{
            QStringLiteral(":dshow-vdev=Cam"),
            QStringLiteral(":dshow-adev=none"),
        }));
#else
        m_model->setAudioDevice({});
        QCOMPARE(m_model->options(), QStringList{});
#endif
        if (module_exists(qtu(OpenCaptureModel::moduleForType(OpenCaptureModel::Camera))))
        {
            /* "no audio" is one of the listed audio devices */
            m_model->refreshDevices();
            QVERIFY(hasValue(m_model->audioDevices(), m_model->audioDevice()));
        }
    }

    void testAdvancedOptionsListTheTypeOptionsWithoutTheBasicOnes()
    {
        if (!module_exists("dtv"))
            QSKIP("no dtv module");

        m_model->setDeviceType(OpenCaptureModel::TvTuner);
        ModuleOptionsModel* advanced = m_model->advancedOptions();
        QCOMPARE(advanced->module(), QStringLiteral("dtv"));

        for (const char* basic : { "dvb-adapter", "dvb-frequency", "dvb-srate",
                                   "dvb-modulation", "dvb-bandwidth" })
            QCOMPARE(advanced->indexOf(basic), -1);

        const int fec = advanced->indexOf("dvb-fec");
        QVERIFY(fec >= 0);
        const QModelIndex index = advanced->index(fec);
        QCOMPARE(index.data(ModuleOptionsModel::SectionRole).toString(),
                 qtr("Cable and satellite reception parameters"));
        QCOMPARE(index.data(ModuleOptionsModel::TypeRole).toInt(), int(ModuleOptionsModel::String));
        QVERIFY(!index.data(ModuleOptionsModel::ChoicesRole).toList().isEmpty());
        QVERIFY(!index.data(ModuleOptionsModel::TextRole).toString().isEmpty());

        /* options of another type replace them */
        m_model->setDeviceType(OpenCaptureModel::Screen);
        QCOMPARE(advanced->indexOf("dvb-fec"), -1);
    }

    void testChangedAdvancedOptionsAreAddedToTheMedia()
    {
        if (!module_exists("dtv"))
            QSKIP("no dtv module");

        m_model->setDeviceType(OpenCaptureModel::TvTuner);
        m_model->setTunerFrequency(474000);
        m_model->setTunerAdapter(0);
        ModuleOptionsModel* advanced = m_model->advancedOptions();
        QVERIFY(!advanced->modified());

        QSignalSpy mediaChanged(m_model.get(), &OpenCaptureModel::mediaChanged);
        const int inversion = advanced->indexOf("dvb-inversion");
        const int fec = advanced->indexOf("dvb-fec");
        QVERIFY(inversion >= 0 && fec > inversion);
        QCOMPARE(advanced->index(inversion).data(ModuleOptionsModel::TypeRole).toInt(),
                 int(ModuleOptionsModel::Integer));

        QVERIFY(advanced->setValue(fec, QStringLiteral("3/4")));
        QVERIFY(advanced->setValue(inversion, 1));
        QVERIFY(advanced->modified());
        QCOMPARE(mediaChanged.count(), 2);
        QVERIFY(advanced->index(inversion).data(ModuleOptionsModel::ModifiedRole).toBool());

        /* in the order of the module options */
        QCOMPARE(m_model->options(), (QStringList{
            QStringLiteral(":dvb-adapter=0"),
            QStringLiteral(":dvb-inversion=1"),
            QStringLiteral(":dvb-fec=3/4"),
        }));

        /* back to the configuration value: not an option anymore */
        QVERIFY(advanced->setValue(fec, QString()));
        QCOMPARE(m_model->options(), (QStringList{
            QStringLiteral(":dvb-adapter=0"),
            QStringLiteral(":dvb-inversion=1"),
        }));

        advanced->reset();
        QVERIFY(!advanced->modified());
        QCOMPARE(m_model->options(), QStringList{ QStringLiteral(":dvb-adapter=0") });
    }

    void testChangedBoolOptionsAreFlags()
    {
        m_model->setDeviceType(OpenCaptureModel::Screen);
        m_model->setScreenFps(5);
        ModuleOptionsModel* advanced = m_model->advancedOptions();
        const int follow = advanced->indexOf("screen-follow-mouse");
        if (follow < 0)
            QSKIP("no screen module");
        QCOMPARE(advanced->index(follow).data(ModuleOptionsModel::TypeRole).toInt(),
                 int(ModuleOptionsModel::Bool));

        QVERIFY(advanced->setValue(follow, true));
        QCOMPARE(m_model->options(), (QStringList{
            QStringLiteral(":screen-fps=5"),
            QStringLiteral(":screen-follow-mouse"),
        }));
    }

    void testAdvancedOptionsClampTheValues()
    {
        if (!module_exists("dtv"))
            QSKIP("no dtv module");

        m_model->setDeviceType(OpenCaptureModel::TvTuner);
        ModuleOptionsModel* advanced = m_model->advancedOptions();
        const int lnb = advanced->indexOf("dvb-lnb-low");
        const QModelIndex index = advanced->index(lnb);
        const qlonglong maximum = index.data(ModuleOptionsModel::MaximumRole).toLongLong();
        QCOMPARE(maximum, 0x7fffffffLL);

        QVERIFY(advanced->setValue(lnb, maximum + 10));
        QCOMPARE(index.data(ModuleOptionsModel::ValueRole).toLongLong(), maximum);
    }

    void testInputItemCarriesTheMrlAndOptions()
    {
        m_model->setDeviceType(OpenCaptureModel::Screen);
        m_model->setScreenFps(5);

        const QVariant variant = m_model->inputItem();
        QVERIFY(variant.canConvert<SharedInputItem>());
        const SharedInputItem item = variant.value<SharedInputItem>();
        QVERIFY(item);

        QCOMPARE(qfu(item->psz_uri), QStringLiteral("screen://"));
        QStringList options;
        for (int i = 0; i < item->i_options; ++i)
            options << qfu(item->ppsz_options[i]);
        QCOMPARE(options, QStringList{ QStringLiteral(":screen-fps=5") });
        QVERIFY(!qfu(item->psz_name).isEmpty());
    }

    void testPreviewWaitsUntilActive()
    {
        CapturePreview preview(m_env->intf);
        preview.setSource(QStringLiteral(MOCK_VIDEO), {});
        QCOMPARE(preview.status(), CapturePreview::Idle);
        QVERIFY(preview.image().isEmpty());
    }

    void testPreviewShowsAFrameOfTheSource()
    {
        SKIP_WITHOUT_MOCK();
        CapturePreview preview(m_env->intf);
        preview.setActive(true);
        preview.setSource(QStringLiteral(MOCK_VIDEO), {});

        QTRY_COMPARE_WITH_TIMEOUT(preview.status(), CapturePreview::Ready, 10000);
        QVERIFY(preview.image().toString().startsWith(QStringLiteral("data:image/png;base64,")));
    }

    void testPreviewKeepsRefreshing()
    {
        SKIP_WITHOUT_MOCK();
        CapturePreview preview(m_env->intf);
        QSignalSpy frames(&preview, &CapturePreview::imageChanged);
        preview.setRefreshInterval(20);
        preview.setActive(true);
        preview.setSource(QStringLiteral(MOCK_VIDEO), {});

        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 3, 20000);

        /* inactive: the frames stop */
        preview.setActive(false);
        const qsizetype count = frames.count();
        QTest::qWait(500);
        QVERIFY(frames.count() <= count + 1);
    }

    void testPreviewOfABrokenSourceIsUnavailable()
    {
        CapturePreview preview(m_env->intf);
        preview.setActive(true);
        preview.setSource(QStringLiteral("mock://video_track_count=1;error=true"), {});

        QTRY_COMPARE_WITH_TIMEOUT(preview.status(), CapturePreview::Unavailable, 10000);
        QVERIFY(preview.image().isEmpty());
    }

    void testModelPreviewFollowsTheMedia()
    {
        QCOMPARE(m_model->preview()->status(), CapturePreview::Idle);
        m_model->preview()->setActive(true);
        m_model->setDeviceType(OpenCaptureModel::TvTuner);
        m_model->setTunerFrequency(1);
        /* a tuner that does not exist: no frame */
        QTRY_COMPARE_WITH_TIMEOUT(m_model->preview()->status(), CapturePreview::Unavailable, 15000);
    }

private:
    static bool hasValue(const QVariantList& choices, const QString& value)
    {
        return std::any_of(choices.cbegin(), choices.cend(), [&value](const QVariant& entry) {
            return entry.toMap().value("value").toString() == value;
        });
    }

    static bool hasType(const QVariantList& types, OpenCaptureModel::DeviceType id)
    {
        return std::any_of(types.cbegin(), types.cend(), [id](const QVariant& entry) {
            return entry.toMap().value("type").toInt() == id;
        });
    }

    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<OpenCaptureModel> m_model;
};

QTEST_GUILESS_MAIN(TestOpenCaptureModel)
#include "test_open_capture_model.moc"
