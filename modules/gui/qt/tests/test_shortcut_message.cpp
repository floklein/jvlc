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
#include <vlc_actions.h>
#include <vlc_player.h>
#include "../../../../lib/libvlc_internal.h"

#include <QTest>
#include <QSignalSpy>

#include "../player/shortcut_message.hpp"

// The Shortcut message: the result of an action, and the key that ran it
class TestShortcutMessage : public QObject
{
    Q_OBJECT

private:
    libvlc_instance_t* m_vlc = nullptr;
    vlc_player_t* m_player = nullptr;

    // what the player says after an action
    void playerSays(const char* text)
    {
        vlc_player_osd_Message(m_player, "%s", text);
    }

    // the player says it while the key is handled
    void pressKey(uint_fast32_t key, const char* result)
    {
        ShortcutMessage::KeyScope scope(key);
        playerSays(result);
    }

private slots:
    void initTestCase()
    {
        // keep paths set by the caller (cross-built Windows tests)
        setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 0);
        setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 0);

        static const char* args[] = {
            "--ignore-config", "--aout=dummy", "--vout=dummy",
        };
        m_vlc = libvlc_new(ARRAY_SIZE(args), args);
        QVERIFY(m_vlc);

        m_player = vlc_player_New(VLC_OBJECT(m_vlc->p_libvlc_int),
                                  VLC_PLAYER_LOCK_NORMAL);
        QVERIFY(m_player);
    }

    void cleanupTestCase()
    {
        if (m_player)
            vlc_player_Delete(m_player);
        if (m_vlc)
            libvlc_release(m_vlc);
    }

    // "Subtitle delay +50 ms · H"
    void testKeyFollowsTheResult()
    {
        ShortcutMessage message(m_player);

        pressKey('h', "Subtitle delay: 50 ms");

        QTRY_COMPARE(message.text(), QStringLiteral("Subtitle delay: 50 ms · H"));
        QVERIFY(message.isVisible());
    }

    void testKeyWithModifiers()
    {
        ShortcutMessage message(m_player);

        pressKey(KEY_MODIFIER_CTRL | KEY_UP, "Volume: 80%");
        QTRY_COMPARE(message.text(), QStringLiteral("Volume: 80% · Ctrl+↑"));

        pressKey(KEY_MODIFIER_SHIFT | 'r', "Recording");
        QTRY_COMPARE(message.text(), QStringLiteral("Recording · Shift+R"));

        pressKey(' ', "Pause");
        QTRY_COMPARE(message.text(), QStringLiteral("Pause · Space"));

        pressKey(KEY_LEFT, "00:05 / 00:10");
        QTRY_COMPARE(message.text(), QStringLiteral("00:05 / 00:10 · ←"));
    }

    // menus and buttons: the result alone
    void testActionWithoutKeyShowsTheResult()
    {
        ShortcutMessage message(m_player);

        playerSays("Subtitle delay: 50 ms");

        QTRY_COMPARE(message.text(), QStringLiteral("Subtitle delay: 50 ms"));
        QVERIFY(message.isVisible());
    }

    void testKeyIsForgottenAfterThePress()
    {
        ShortcutMessage message(m_player);

        pressKey('h', "Subtitle delay: 50 ms");
        playerSays("Subtitle delay: 100 ms");

        QTRY_COMPARE(message.text(), QStringLiteral("Subtitle delay: 100 ms"));
    }

    // the mouse wheel is not a keyboard shortcut
    void testWheelShowsNoKey()
    {
        ShortcutMessage message(m_player);

        pressKey(KEY_MOUSEWHEELUP, "Volume: 85%");

        QTRY_COMPARE(message.text(), QStringLiteral("Volume: 85%"));
    }

    // "Show shortcut keys in on-screen messages" off
    void testHiddenKeys()
    {
        ShortcutMessage message(m_player);
        message.setShowKeys(false);

        pressKey('h', "Subtitle delay: 50 ms");

        QTRY_COMPARE(message.text(), QStringLiteral("Subtitle delay: 50 ms"));
    }

    // one pill: a new press replaces it
    void testNewPressReplacesTheMessage()
    {
        ShortcutMessage message(m_player);
        QSignalSpy shown(&message, &ShortcutMessage::shown);

        pressKey('h', "Subtitle delay: 50 ms");
        pressKey('g', "Subtitle delay: 0 ms");

        QTRY_COMPARE(shown.count(), 2);
        QCOMPARE(message.text(), QStringLiteral("Subtitle delay: 0 ms · G"));
        QVERIFY(message.isVisible());
    }

    // shown for 1.5 s
    void testMessageHides()
    {
        ShortcutMessage message(m_player);
        QCOMPARE(message.duration(), 1500);

        playerSays("Pause");
        QTRY_VERIFY(message.isVisible());

        QTRY_VERIFY_WITH_TIMEOUT(!message.isVisible(), 10000);
    }

};

QTEST_GUILESS_MAIN(TestShortcutMessage)
#include "test_shortcut_message.moc"
