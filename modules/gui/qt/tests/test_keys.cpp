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
#include <vlc_configuration.h>
#include "../../../../lib/libvlc_internal.h"

#include <QTest>
#include <QKeyEvent>

#include "../util/vlchotkeyconverter.hpp"
#include "../util/context_menu_key_filter.hpp"

// The default keys of the redesign and how menus show them
class TestKeys : public QObject
{
    Q_OBJECT

private:
    libvlc_instance_t* m_vlc = nullptr;

    vlc_object_t* obj() const { return VLC_OBJECT(m_vlc->p_libvlc_int); }

    // the action the core key map runs for a key, 0 for none
    int actionForKey(const char* key)
    {
        const uint_fast32_t keycode = vlc_str2keycode(key);
        if (keycode == KEY_UNSET)
            return -1;
        var_SetInteger(obj(), "key-action", 0);
        var_SetInteger(obj(), "key-pressed", keycode);
        return var_GetInteger(obj(), "key-action");
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

    // ←/→ seek 10 s and ↑/↓ change the volume
    void testPlainArrowsSeekAndChangeVolume()
    {
        QCOMPARE(actionForKey("Right"), int(ACTIONID_JUMP_FORWARD_SHORT));
        QCOMPARE(actionForKey("Left"), int(ACTIONID_JUMP_BACKWARD_SHORT));
        QCOMPARE(actionForKey("Up"), int(ACTIONID_VOL_UP));
        QCOMPARE(actionForKey("Down"), int(ACTIONID_VOL_DOWN));
    }

    void testModifierJumpsStay()
    {
        QCOMPARE(actionForKey("Shift+Right"), int(ACTIONID_JUMP_FORWARD_EXTRASHORT));
        QCOMPARE(actionForKey("Alt+Right"), int(ACTIONID_JUMP_FORWARD_SHORT));
        QCOMPARE(actionForKey("Alt+Left"), int(ACTIONID_JUMP_BACKWARD_SHORT));
        QCOMPARE(actionForKey("Ctrl+Right"), int(ACTIONID_JUMP_FORWARD_MEDIUM));
        QCOMPARE(actionForKey("Ctrl+Alt+Left"), int(ACTIONID_JUMP_BACKWARD_LONG));
        QCOMPARE(actionForKey("Ctrl+Up"), int(ACTIONID_VOL_UP));
        QCOMPARE(actionForKey("Ctrl+Down"), int(ACTIONID_VOL_DOWN));
    }

    void testRecordAndShuffle()
    {
        QCOMPARE(actionForKey("Shift+r"), int(ACTIONID_RECORD));
        QCOMPARE(actionForKey("r"), int(ACTIONID_RANDOM));
    }

    // F6 cycles the window areas
    void testF6PlaysNoBookmark()
    {
        QCOMPARE(actionForKey("F6"), 0);
        QCOMPARE(vlcActionKeyText(obj(), "play-bookmark6"), QString());
    }

    void testMenuKeyOpensTheContextMenu()
    {
        QCOMPARE(actionForKey("Menu"), int(ACTIONID_INTF_POPUP_MENU));
    }

    // menus show the first key of an action
    void testMenusShowThePlainArrows()
    {
        QCOMPARE(actionKeyText(obj(), "jump+short"), QStringLiteral("Right"));
        QCOMPARE(actionKeyText(obj(), "jump-short"), QStringLiteral("Left"));
        QCOMPARE(actionKeyText(obj(), "vol-up"), QStringLiteral("Up"));
        QCOMPARE(actionKeyText(obj(), "vol-down"), QStringLiteral("Down"));
    }

    void testMenusShowCoreKeys()
    {
        QCOMPARE(actionKeyText(obj(), "play-pause"), QStringLiteral("Space"));
        QCOMPARE(actionKeyText(obj(), "record"), QStringLiteral("Shift+R"));
        QCOMPARE(actionKeyText(obj(), "random"), QStringLiteral("R"));
        QCOMPARE(actionKeyText(obj(), "quit"), QStringLiteral("Ctrl+Q"));
    }

    // the interface's own actions (dialogs, views) are not core hotkeys
    void testMenusShowInterfaceKeys()
    {
        QCOMPARE(actionKeyText(obj(), "open-file"), QStringLiteral("Ctrl+O"));
        QCOMPARE(actionKeyText(obj(), "open-folder"), QStringLiteral("Ctrl+Shift+O"));
        QCOMPARE(actionKeyText(obj(), "open-clipboard"), QStringLiteral("Ctrl+V"));
        QCOMPARE(actionKeyText(obj(), "library-search"), QStringLiteral("Ctrl+F"));
        QCOMPARE(actionKeyText(obj(), "add-bookmark"), QStringLiteral("Ctrl+B"));
        QCOMPARE(actionKeyText(obj(), "hide-ui"), QStringLiteral("Ctrl+H"));
        QCOMPARE(actionKeyText(obj(), "settings"), QStringLiteral("Ctrl+P"));
        QCOMPARE(actionKeyText(obj(), "about"), QStringLiteral("Shift+F1"));
    }

    // Picture-in-picture is an interface action, no core hotkey takes its key
    void testPictureInPictureKey()
    {
        QCOMPARE(actionKeyText(obj(), "picture-in-picture"), QStringLiteral("Ctrl+Shift+P"));
        QCOMPARE(appActionSequence(obj(), "picture-in-picture"), QStringLiteral("Ctrl+Shift+P"));
        QCOMPARE(actionForKey("Ctrl+Shift+p"), 0);
    }

    void testUnknownActionHasNoKey()
    {
        QCOMPARE(actionKeyText(obj(), "no-such-action"), QString());
    }

    void testMenusFollowRebinding()
    {
        var_Create(obj(), "key-stop", VLC_VAR_STRING);
        var_SetString(obj(), "key-stop", "Ctrl+Alt+s");
        QCOMPARE(actionKeyText(obj(), "stop"), QStringLiteral("Ctrl+Alt+S"));
        var_Destroy(obj(), "key-stop");
    }

    // a rebinding saved by Keyboard Shortcuts works without a restart
    void testKeyMapFollowsRebindingAfterReload()
    {
        config_PutPsz("key-stop", "Ctrl+Alt+s");
        QCOMPARE(vlc_actions_reload(obj()), VLC_SUCCESS);

        QCOMPARE(actionForKey("Ctrl+Alt+s"), int(ACTIONID_STOP));
        QCOMPARE(actionForKey("s"), 0);

        config_PutPsz("key-stop", config_FindConfig("key-stop")->orig.psz);
        QCOMPARE(vlc_actions_reload(obj()), VLC_SUCCESS);
        QCOMPARE(actionForKey("Ctrl+Alt+s"), 0);
    }

    // shortcuts of the interface use the same key as the menus
    void testInterfaceShortcutSequence()
    {
        QCOMPARE(appActionSequence(obj(), "open-folder"), QStringLiteral("Ctrl+Shift+O"));
        QCOMPARE(appActionSequence(obj(), "library-search"), QStringLiteral("Ctrl+F"));
        QCOMPARE(appActionSequence(obj(), "play-pause"), QString());
    }

    // keys of interface actions changed in Keyboard Shortcuts
    void testInterfaceShortcutsFollowRebinding()
    {
        var_Create(obj(), "qt-app-keys", VLC_VAR_STRING);
        var_SetString(obj(), "qt-app-keys", "open-folder=Ctrl+Alt+f\tsettings=");

        QCOMPARE(appActionSequence(obj(), "open-folder"), QStringLiteral("Ctrl+Alt+F"));
        QCOMPARE(actionKeyText(obj(), "open-folder"), QStringLiteral("Ctrl+Alt+F"));
        // unbound
        QCOMPARE(appActionSequence(obj(), "settings"), QString());
        QCOMPARE(actionKeyText(obj(), "settings"), QString());
        // not overridden
        QCOMPARE(appActionSequence(obj(), "open-file"), QStringLiteral("Ctrl+O"));

        var_Destroy(obj(), "qt-app-keys");
    }

    // Shift+F10 opens context menus like the Menu key
    void testShiftF10ActsAsMenuKey()
    {
        struct Receiver : QObject
        {
            QList<QPair<int, int>> keys; // type, key
            Qt::KeyboardModifiers modifiers;
            bool event(QEvent* e) override
            {
                if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
                {
                    const auto ke = static_cast<QKeyEvent*>(e);
                    keys.append({ int(e->type()), ke->key() });
                    modifiers = ke->modifiers();
                    return true;
                }
                return QObject::event(e);
            }
        } receiver;

        ContextMenuKeyFilter filter;
        receiver.installEventFilter(&filter);

        QKeyEvent press(QEvent::KeyPress, Qt::Key_F10, Qt::ShiftModifier);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_F10, Qt::ShiftModifier);
        QCoreApplication::sendEvent(&receiver, &press);
        QCoreApplication::sendEvent(&receiver, &release);

        const QList<QPair<int, int>> expected {
            { int(QEvent::KeyPress), int(Qt::Key_Menu) },
            { int(QEvent::KeyRelease), int(Qt::Key_Menu) },
        };
        QCOMPARE(receiver.keys, expected);
        QCOMPARE(receiver.modifiers, Qt::NoModifier);
    }

    void testOtherKeysPassThrough()
    {
        struct Receiver : QObject
        {
            QList<int> keys;
            bool event(QEvent* e) override
            {
                if (e->type() == QEvent::KeyPress)
                {
                    keys.append(static_cast<QKeyEvent*>(e)->key());
                    return true;
                }
                return QObject::event(e);
            }
        } receiver;

        ContextMenuKeyFilter filter;
        receiver.installEventFilter(&filter);

        QKeyEvent f10(QEvent::KeyPress, Qt::Key_F10, Qt::NoModifier);
        QKeyEvent ctrlF10(QEvent::KeyPress, Qt::Key_F10, Qt::ControlModifier | Qt::ShiftModifier);
        QKeyEvent menu(QEvent::KeyPress, Qt::Key_Menu, Qt::NoModifier);
        QCoreApplication::sendEvent(&receiver, &f10);
        QCoreApplication::sendEvent(&receiver, &ctrlF10);
        QCoreApplication::sendEvent(&receiver, &menu);

        QCOMPARE(receiver.keys, (QList<int>{ Qt::Key_F10, Qt::Key_F10, Qt::Key_Menu }));
    }
};

QTEST_GUILESS_MAIN(TestKeys)
#include "test_keys.moc"
