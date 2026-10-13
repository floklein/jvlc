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
#include <QAbstractItemModelTester>
#include <QTemporaryDir>
#include <QSettings>

#include <memory>

#include "../dialogs/convert/conversion_profile_model.hpp"

/* Built-in profiles, as shipped by the Convert dialog since VLC 3 */
static const char H264_MP3_MP4[] =
    "audio_enable=yes;audio_codec=mpga;acodec_bitrate=128;acodec_channels=2;"
    "acodec_samplerate=44100;video_enable=yes;video_codec=h264;vcodec_bitrate=0;"
    "vcodec_qp=0;vcodec_framerate=0;vcodec_width=0;vcodec_height=0;"
    "subtitles_enable=yes;subtitles_codec=tx3g;muxer_mux=mp4";
static const char AUDIO_CD[] =
    "audio_enable=yes;audio_codec=s16l;acodec_bitrate=128;acodec_channels=2;"
    "acodec_samplerate=44100;muxer_mux=wav";
static const char ANDROID_SD_LOW[] =
    "audio_enable=yes;audio_codec=mp3;acodec_bitrate=24;acodec_channels=1;"
    "acodec_samplerate=44100;video_enable=yes;video_codec=h264;vcodec_bitrate=56;"
    "vcodec_qp=0;vcodec_framerate=12;vcodec_width=176;vcodec_height=144;"
    "vcodec_custom=profile%3Dbaseline;subtitles_enable=yes;subtitles_codec=tx3g;"
    "muxer_mux=mp4";

class TestConversionProfileModel : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    std::unique_ptr<QSettings> settings()
    {
        return std::make_unique<QSettings>(m_dir->filePath("profiles.ini"),
                                           QSettings::IniFormat);
    }

    static QString nameAt(const ConversionProfileModel& model, int row)
    {
        return model.data(model.index(row), ConversionProfileModel::NAME).toString();
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup()
    {
        m_dir.reset();
    }

    void testBuiltInProfilesWhenNothingIsSaved()
    {
        auto s = settings();
        ConversionProfileModel model(s.get());
        QAbstractItemModelTester tester(&model);

        QCOMPARE(model.rowCount(), 25);
        QCOMPARE(nameAt(model, 0), "Video - H.264 + MP3 (MP4)");
        QCOMPARE(model.data(model.index(0), ConversionProfileModel::VALUE).toString(),
                 H264_MP3_MP4);
        QCOMPARE(model.data(model.index(0), ConversionProfileModel::BUILTIN).toBool(), true);
        QCOMPARE(nameAt(model, 14), "Audio - CD (uncompressed)");
    }

    void testSoutChainOfVideoProfile()
    {
        QCOMPARE(ConversionProfileModel::soutChain(H264_MP3_MP4, "/tmp/out.mp4", false, false),
                 "#transcode{vcodec=h264,acodec=mpga,ab=128,channels=2,samplerate=44100,"
                 "scodec=tx3g}:std{access=file{no-overwrite},mux=mp4,dst='/tmp/out.mp4'}");
    }

    void testSoutChainOfAudioProfileDropsVideoAndSubtitles()
    {
        QCOMPARE(ConversionProfileModel::soutChain(AUDIO_CD, "/tmp/out.wav", false, false),
                 "#transcode{vcodec=none,acodec=s16l,ab=128,channels=2,samplerate=44100,"
                 "scodec=none}:std{access=file{no-overwrite},mux=wav,dst='/tmp/out.wav'}");
    }

    void testSoutChainWithEncoderOptionsDeinterlaceAndOverwrite()
    {
        QCOMPARE(ConversionProfileModel::soutChain(ANDROID_SD_LOW, "C:\\My Videos\\a,b.mp4", true, true),
                 "#transcode{vcodec=h264,vb=56,venc='x264{profile=baseline}',fps=12,width=176,"
                 "height=144,acodec=mp3,ab=24,channels=1,samplerate=44100,scodec=tx3g,"
                 "deinterlace}:std{access=file,mux=mp4,dst='C:\\\\My Videos\\\\a,b.mp4'}");
    }

    void testFileExtensionOfMuxer()
    {
        QCOMPARE(ConversionProfileModel::fileExtension(H264_MP3_MP4), "mp4");
        QCOMPARE(ConversionProfileModel::fileExtension(AUDIO_CD), "wav");
        QCOMPARE(ConversionProfileModel::fileExtension("video_enable=yes;muxer_mux=ps"), "mpg");
        QCOMPARE(ConversionProfileModel::fileExtension("video_enable=yes;muxer_mux=mpeg1"), "mpg");
    }

    void testEditorFieldsRoundTrip()
    {
        auto s = settings();
        ConversionProfileModel model(s.get());

        QVariantMap fields = model.fields(ANDROID_SD_LOW);
        QCOMPARE(fields.value("muxer_mux").toString(), "mp4");
        QCOMPARE(fields.value("video_codec").toString(), "h264");
        QCOMPARE(fields.value("vcodec_width").toInt(), 176);
        QCOMPARE(fields.value("vcodec_custom").toString(), "profile=baseline");
        QCOMPARE(fields.value("video_enable").toBool(), true);
        QCOMPARE(fields.value("video_copy").toBool(), false);

        const QString value = model.value(fields);
        QCOMPARE(ConversionProfileModel::soutChain(value, "/o.mp4", false, false),
                 ConversionProfileModel::soutChain(ANDROID_SD_LOW, "/o.mp4", false, false));

        /* keeping the original video track writes no video codec */
        fields["video_copy"] = true;
        QCOMPARE(ConversionProfileModel::soutChain(model.value(fields), "/o.mp4", false, false),
                 "#transcode{acodec=mp3,ab=24,channels=1,samplerate=44100,scodec=tx3g}"
                 ":std{access=file{no-overwrite},mux=mp4,dst='/o.mp4'}");
    }

    void testCustomProfileIsSavedAndRestored()
    {
        {
            auto s = settings();
            ConversionProfileModel model(s.get());
            QAbstractItemModelTester tester(&model);
            const int row = model.save("My phone", AUDIO_CD);
            QCOMPARE(row, 25);
            QCOMPARE(model.rowCount(), 26);
            QCOMPARE(model.data(model.index(row), ConversionProfileModel::BUILTIN).toBool(), false);
            model.setSelected(row);
        }
        {
            auto s = settings();
            ConversionProfileModel model(s.get());
            QCOMPARE(model.rowCount(), 26);
            QCOMPARE(nameAt(model, 25), "My phone");
            QCOMPARE(model.selected(), 25);
            QCOMPARE(model.selectedName(), "My phone");
            QCOMPARE(model.selectedValue(), AUDIO_CD);
            QCOMPARE(model.selectedBuiltIn(), false);
            QCOMPARE(model.data(model.index(25), ConversionProfileModel::VALUE).toString(), AUDIO_CD);
        }
    }

    void testSavingAnExistingNameUpdatesIt()
    {
        auto s = settings();
        ConversionProfileModel model(s.get());
        QAbstractItemModelTester tester(&model);
        model.save("Mine", AUDIO_CD);
        const int row = model.save("Mine", H264_MP3_MP4);
        QCOMPARE(row, 25);
        QCOMPARE(model.rowCount(), 26);
        QCOMPARE(model.data(model.index(row), ConversionProfileModel::VALUE).toString(), H264_MP3_MP4);
    }

    void testRemoveProfile()
    {
        {
            auto s = settings();
            ConversionProfileModel model(s.get());
            QAbstractItemModelTester tester(&model);
            model.save("Mine", AUDIO_CD);
            model.remove(25);
            model.remove(0);
            QCOMPARE(model.rowCount(), 24);
        }
        auto s = settings();
        ConversionProfileModel model(s.get());
        QCOMPARE(model.rowCount(), 24);
        QCOMPARE(nameAt(model, 0), "Video - H.264 + AAC (MP4)");
    }

    void testProfilesSavedByOlderVersionsAreRead()
    {
        {
            auto s = settings();
            s->beginWriteArray("codecs-profiles");
            s->setArrayIndex(0);
            s->setValue("Profile-Name", "Old one");
            s->setValue("Profile-Value", AUDIO_CD);
            s->endArray();
            s->setValue("codecs-profiles-selected", "Old one");
        }
        auto s = settings();
        ConversionProfileModel model(s.get());
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(nameAt(model, 0), "Old one");
        QCOMPARE(model.selected(), 0);
    }
};

QTEST_GUILESS_MAIN(TestConversionProfileModel)
#include "test_conversion_profile_model.moc"
