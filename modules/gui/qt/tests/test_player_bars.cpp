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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_common.h>
#include <vlc_variables.h>
#include "../../../../lib/libvlc_internal.h"

#include <QTest>
#include <QSettings>
#include <QTemporaryDir>

#include "../dialogs/toolbar/controlbar_profile_model.hpp"
#include "../dialogs/toolbar/controlbar_profile.hpp"
#include "../player/player_controlbar_model.hpp"
#include "../player/control_list_model.hpp"
#include "../util/vlchotkeyconverter.hpp"

using C = ControlListModel;

// The Top bar and Control bar of the redesign (Figma 247:447 and 491:1779)
class TestPlayerBars : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    libvlc_instance_t* m_vlc = nullptr;

    static QVector<int> controls(const ControlbarProfile* profile, int identifier, int zone)
    {
        PlayerControlbarModel* model = profile->getModel(identifier);
        if (!model)
            return {};
        switch (zone)
        {
        case 0: return model->left()->getControls();
        case 1: return model->center()->getControls();
        default: return model->right()->getControls();
        }
    }

private slots:
    void initTestCase()
    {
        // keep paths set by the caller (cross-built Windows tests)
        setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 0);
        setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 0);

        static const char* args[] = { "--ignore-config" };
        m_vlc = libvlc_new(ARRAY_SIZE(args), args);
        QVERIFY(m_vlc);
    }

    void cleanupTestCase()
    {
        if (m_vlc)
            libvlc_release(m_vlc);
    }

    // Accessible names carry the shortcut ("Play, Space"), read from the
    // current key binding of the action
    void testActionKeyTextReadsDefaultBindings()
    {
        vlc_object_t* obj = VLC_OBJECT(m_vlc->p_libvlc_int);

        QCOMPARE(vlcActionKeyText(obj, "play-pause"), QStringLiteral("Space"));
        QCOMPARE(vlcActionKeyText(obj, "next"), QStringLiteral("N"));
        QCOMPARE(vlcActionKeyText(obj, "prev"), QStringLiteral("P"));
        QCOMPARE(vlcActionKeyText(obj, "record"), QStringLiteral("Shift+R"));
        QCOMPARE(vlcActionKeyText(obj, "toggle-fullscreen"), QStringLiteral("F"));
    }

    void testActionKeyTextFollowsRebinding()
    {
        vlc_object_t* obj = VLC_OBJECT(m_vlc->p_libvlc_int);

        var_Create(obj, "key-subtitle-track", VLC_VAR_STRING);

        var_SetString(obj, "key-subtitle-track", "");
        QCOMPARE(vlcActionKeyText(obj, "subtitle-track"), QString());

        var_SetString(obj, "key-subtitle-track", "Ctrl+Alt+Right\tv");
        QCOMPARE(vlcActionKeyText(obj, "subtitle-track"), QStringLiteral("Ctrl+Alt+Right"));

        var_Destroy(obj, "key-subtitle-track");
    }

    void testNewControlsUseReservedSlots()
    {
        // saved Layouts store the numeric values: they must never move
        QCOMPARE(int(C::SEEK_BAR), 0x2B);
        QCOMPARE(int(C::AUDIO_BUTTON), 0x2C);
        QCOMPARE(int(C::SUBTITLES_BUTTON), 0x2D);
        QCOMPARE(int(C::PICTURE_IN_PICTURE_BUTTON), 0x33);
        QCOMPARE(int(C::VOLUME), 0x21);
        QCOMPARE(int(C::WIDGET_SPACER), 0x40);
    }

    void testDefaultLayoutIsSelectedOnFirstStart()
    {
        QSettings settings(m_dir.filePath("first.ini"), QSettings::IniFormat);
        ControlbarProfileModel model(&settings);

        QTRY_VERIFY(model.rowCount() > 0);
        QVERIFY(model.currentModel());
        QCOMPARE(controls(model.currentModel(), PlayerControlbarModel::Videoplayer, 0),
                 (QVector<int>{ C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON }));
    }

    void testDefaultVideoLayout()
    {
        std::unique_ptr<ControlbarProfile> profile {
            ControlbarProfileModel::generateProfileFromStyle(ControlbarProfileModel::Style::DEFAULT_STYLE) };
        QVERIFY(profile);

        const int video = PlayerControlbarModel::Videoplayer;
        QCOMPARE(controls(profile.get(), video, 0),
                 (QVector<int>{ C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON }));
        QCOMPARE(controls(profile.get(), video, 1),
                 (QVector<int>{ C::SEEK_BAR }));
        QCOMPARE(controls(profile.get(), video, 2),
                 (QVector<int>{ C::VOLUME, C::AUDIO_BUTTON, C::SUBTITLES_BUTTON,
                                C::EXTENDED_BUTTON, C::PLAYLIST_BUTTON }));
    }

    void testDefaultMusicLayout()
    {
        std::unique_ptr<ControlbarProfile> profile {
            ControlbarProfileModel::generateProfileFromStyle(ControlbarProfileModel::Style::DEFAULT_STYLE) };
        QVERIFY(profile);

        // the music player adds the ±15 s skips around play and has no
        // Adjustments key (Figma 4778:26181): the waveform keeps its length
        const int audio = PlayerControlbarModel::Audioplayer;
        QCOMPARE(controls(profile.get(), audio, 0),
                 (QVector<int>{ C::PREVIOUS_BUTTON, C::SKIP_BACK_BUTTON, C::PLAY_BUTTON,
                                C::SKIP_FW_BUTTON, C::NEXT_BUTTON }));
        QCOMPARE(controls(profile.get(), audio, 1),
                 (QVector<int>{ C::SEEK_BAR }));
        QCOMPARE(controls(profile.get(), audio, 2),
                 (QVector<int>{ C::VOLUME, C::AUDIO_BUTTON, C::PLAYLIST_BUTTON }));
    }

    void testDefaultLayoutsHaveNoStopOrRecord()
    {
        std::unique_ptr<ControlbarProfile> profile {
            ControlbarProfileModel::generateProfileFromStyle(ControlbarProfileModel::Style::DEFAULT_STYLE) };
        QVERIFY(profile);

        for (int identifier : { int(PlayerControlbarModel::Videoplayer), int(PlayerControlbarModel::Audioplayer) })
        {
            for (int zone = 0; zone < 3; ++zone)
            {
                const QVector<int> list = controls(profile.get(), identifier, zone);
                QVERIFY(!list.contains(C::STOP_BUTTON));
                QVERIFY(!list.contains(C::RECORD_BUTTON));
            }
        }
    }

    // The Control bar draws its own seek bar row only when the Layout does
    // not place the seek bar inline
    void testZoneTellsWhetherItHoldsAControl()
    {
        ControlListModel zone;
        QVERIFY(!zone.contains(C::SEEK_BAR));

        zone.setControls({ C::PREVIOUS_BUTTON, C::SEEK_BAR });
        QVERIFY(zone.contains(C::SEEK_BAR));
        QVERIFY(!zone.contains(C::PLAY_BUTTON));

        zone.remove(1);
        QVERIFY(!zone.contains(C::SEEK_BAR));
    }

    void testNewControlsSurviveSaveAndReload()
    {
        const QString path = m_dir.filePath("roundtrip.ini");
        {
            QSettings settings(path, QSettings::IniFormat);
            ControlbarProfileModel model(&settings);
            QTRY_VERIFY(model.rowCount() > 0);
            model.save();
        }

        QSettings settings(path, QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_VERIFY(model.rowCount() > 0);
        QVERIFY(model.currentModel());

        const int video = PlayerControlbarModel::Videoplayer;
        QCOMPARE(controls(model.currentModel(), video, 1), (QVector<int>{ C::SEEK_BAR }));
        QVERIFY(controls(model.currentModel(), video, 2).contains(C::SUBTITLES_BUTTON));
    }
};

QTEST_GUILESS_MAIN(TestPlayerBars)
#include "test_player_bars.moc"
