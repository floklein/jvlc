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
#include <vlc_actions.h>
#include <vlc_configuration.h>
#include "../../../../lib/libvlc_internal.h"

#include <QTest>
#include <QHash>

#include "../dialogs/shortcuts/shortcuts_model.hpp"
#include "../util/vlchotkeyconverter.hpp"

// The Keyboard Shortcuts dialog: its actions, conflicts, replace and reset
class TestShortcutsModel : public QObject
{
    Q_OBJECT

private:
    libvlc_instance_t* m_vlc = nullptr;
    ShortcutsModel* m_model = nullptr;

    // the active Profile's options; unset ones come from the defaults
    QHash<QString, QString> m_options;
    int m_commits = 0;

    vlc_object_t* obj() const { return VLC_OBJECT(m_vlc->p_libvlc_int); }

    static int key(const char* name)
    {
        return int(vlc_str2keycode(name));
    }

    ShortcutsModel* newModel()
    {
        auto model = new ShortcutsModel(this);
        model->setStore(
            [this](const QString& option) {
                if (m_options.contains(option))
                    return m_options.value(option);
                // the defaults of the core
                if (config_GetType(qtu(option)) == 0)
                    return QString();
                char* value = config_GetPsz(qtu(option));
                const QString result = qfu(value ? value : "");
                free(value);
                return result;
            },
            [this](const QString& option, const QString& value) {
                m_options.insert(option, value);
            });
        model->setCommitFunction([this]() { ++m_commits; });
        model->load(obj());
        return model;
    }

    QVariant data(const QString& name, int role) const
    {
        return m_model->index(m_model->rowOf(name)).data(role);
    }

    QStringList keys(const QString& name) const
    {
        return data(name, ShortcutsModel::KeysRole).toStringList();
    }

    QStringList globalKeys(const QString& name) const
    {
        return data(name, ShortcutsModel::GlobalKeysRole).toStringList();
    }

    QString label(const QString& name) const
    {
        return data(name, ShortcutsModel::LabelRole).toString();
    }

    int group(const QString& name) const
    {
        return data(name, ShortcutsModel::GroupRole).toInt();
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

    void init()
    {
        m_options.clear();
        m_commits = 0;
        m_model = newModel();
    }

    void cleanup()
    {
        delete m_model;
        m_model = nullptr;
    }

    // all 114 core actions, plus the interface's own
    void testListsEveryAction()
    {
        QStringList names;
        for (const char* const* name = vlc_actions_get_key_names(obj()); *name; ++name)
            names.append(qfu(*name));
        QCOMPARE(names.size(), 114);
        names.append(appActionNames());

        QCOMPARE(m_model->rowCount(), names.size());
        for (const QString& name : names)
            QVERIFY2(m_model->rowOf(name) >= 0, qtu(name));
    }

    // Basic first, then Playback, Navigation, Audio, Video, Subtitles,
    // Interface and Advanced
    void testGroupsStartWithBasic()
    {
        QCOMPARE(m_model->index(0).data(ShortcutsModel::GroupRole).toInt(), int(ShortcutsModel::Basic));
        int previous = ShortcutsModel::Basic;
        int basic = 0;
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const int current = m_model->index(row).data(ShortcutsModel::GroupRole).toInt();
            QVERIFY(current >= previous);
            previous = current;
            if (current == ShortcutsModel::Basic)
                ++basic;
        }
        QCOMPARE(previous, int(ShortcutsModel::Advanced));
        // about the keys Onboarding shows
        QVERIFY(basic >= 10 && basic <= 16);
    }

    void testBasicHoldsTheOnboardingKeys()
    {
        for (const char* name : { "play-pause", "jump-short", "jump+short", "prev", "next",
                                  "frame-previous", "frame-next", "subtitle-track", "audio-track",
                                  "toggle-fullscreen", "vol-up", "vol-down", "show-queue",
                                  "snapshot", "record" })
            QVERIFY2(group(name) == ShortcutsModel::Basic, name);
    }

    void testRareActionsAreAdvanced()
    {
        for (const char* name : { "crop-top", "uncrop-left", "zoom-quarter", "zoom-double",
                                  "nav-up", "nav-activate", "intf-boss", "set-bookmark1",
                                  "play-bookmark10" })
            QVERIFY2(group(name) == ShortcutsModel::Advanced, name);
    }

    void testGroupsOfOtherActions()
    {
        QCOMPARE(group("stop"), int(ShortcutsModel::Playback));
        QCOMPARE(group("chapter-next"), int(ShortcutsModel::Navigation));
        QCOMPARE(group("vol-mute"), int(ShortcutsModel::Audio));
        QCOMPARE(group("aspect-ratio"), int(ShortcutsModel::Video));
        QCOMPARE(group("subdelay-up"), int(ShortcutsModel::Subtitles));
        QCOMPARE(group("open-file"), int(ShortcutsModel::Interface));
        QCOMPARE(group("picture-in-picture"), int(ShortcutsModel::Interface));
    }

    // labels are rewritten in plain words
    void testLabelsArePlainWords()
    {
        QCOMPARE(label("play-pause"), QStringLiteral("Play or pause"));
        QCOMPARE(label("jump+short"), QStringLiteral("Skip forward"));
        QCOMPARE(label("open-folder"), QStringLiteral("Open folder…"));
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const QModelIndex index = m_model->index(row);
            const QString label = index.data(ShortcutsModel::LabelRole).toString();
            QVERIFY2(!label.isEmpty(), qtu(index.data(ShortcutsModel::NameRole).toString()));
        }
    }

    void testShowsTheCurrentKeys()
    {
        QCOMPARE(keys("stop").value(0), QStringLiteral("S"));
        QCOMPARE(keys("record"), QStringList { QStringLiteral("Shift+R") });
        QCOMPARE(keys("open-folder"), QStringList { QStringLiteral("Ctrl+Shift+O") });
        QCOMPARE(keys("intf-boss"), QStringList());
        QCOMPARE(globalKeys("stop"), QStringList());
        QVERIFY(data("stop", ShortcutsModel::IsDefaultRole).toBool());
    }

    // interface actions have no global key
    void testOnlyHotkeysHaveAGlobalKey()
    {
        QVERIFY(data("stop", ShortcutsModel::HasGlobalKeyRole).toBool());
        QVERIFY(!data("open-folder", ShortcutsModel::HasGlobalKeyRole).toBool());
    }

    // "Press a key…": modifiers alone and Tab are not keys
    void testKeyFromEvent()
    {
        QCOMPARE(m_model->keyFromEvent(Qt::Key_R, Qt::ShiftModifier), key("Shift+r"));
        QCOMPARE(m_model->keyFromEvent(Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier), key("Ctrl+Shift+o"));
        QCOMPARE(m_model->keyFromEvent(Qt::Key_Space, Qt::NoModifier), key("Space"));
        QCOMPARE(m_model->keyFromEvent(Qt::Key_Shift, Qt::ShiftModifier), 0);
        QCOMPARE(m_model->keyFromEvent(Qt::Key_Control, Qt::ControlModifier), 0);
        QCOMPARE(m_model->keyFromEvent(Qt::Key_Tab, Qt::NoModifier), 0);
        QCOMPARE(m_model->keyText(key("Ctrl+Shift+o")), QStringLiteral("Ctrl+Shift+O"));
    }

    // "Used by X: Replace?"
    void testConflictNamesTheOtherAction()
    {
        QCOMPARE(m_model->conflict("stop", false, key("Space")), QStringLiteral("Play or pause"));
        QCOMPARE(m_model->conflict("stop", false, key("Shift+r")), QStringLiteral("Record"));
    }

    void testKeysOfInterfaceActionsConflictWithHotkeys()
    {
        QCOMPARE(m_model->conflict("stop", false, key("Ctrl+o")), QStringLiteral("Open file…"));
        QCOMPARE(m_model->conflict("open-file", false, key("Space")), QStringLiteral("Play or pause"));
    }

    void testNoConflict()
    {
        // its own key
        QCOMPARE(m_model->conflict("play-pause", false, key("Space")), QString());
        // a free key
        QCOMPARE(m_model->conflict("stop", false, key("Ctrl+Alt+F12")), QString());
        // keys and global keys are separate
        QCOMPARE(m_model->conflict("stop", true, key("Space")), QString());
    }

    void testConflictBetweenGlobalKeys()
    {
        m_model->assign("play-pause", true, key("Ctrl+Alt+p"));
        QCOMPARE(m_model->conflict("stop", true, key("Ctrl+Alt+p")), QStringLiteral("Play or pause"));
    }

    // Replace takes the key from the action that used it
    void testReplace()
    {
        m_model->assign("stop", false, key("Shift+r"));

        QCOMPARE(keys("stop"), QStringList { QStringLiteral("Shift+R") });
        QCOMPARE(keys("record"), QStringList());
        QVERIFY(!data("stop", ShortcutsModel::IsDefaultRole).toBool());
        QVERIFY(!data("record", ShortcutsModel::IsDefaultRole).toBool());
        QCOMPARE(m_model->conflict("record", false, key("Shift+r")), QStringLiteral("Stop"));
        QVERIFY(m_model->modified());
    }

    // an action with several keys keeps the others
    void testReplaceKeepsTheOtherKeys()
    {
        QCOMPARE(keys("vol-up").mid(0, 2), (QStringList { QStringLiteral("Up"), QStringLiteral("Ctrl+Up") }));
        m_model->assign("next", false, key("Up"));
        QCOMPARE(keys("vol-up").value(0), QStringLiteral("Ctrl+Up"));
    }

    void testInterfaceActionsHaveNoGlobalKey()
    {
        m_model->assign("open-file", true, key("Ctrl+Alt+o"));
        QCOMPARE(globalKeys("open-file"), QStringList());
        QVERIFY(!m_model->modified());
    }

    // Backspace clears
    void testClear()
    {
        m_model->clear("stop", false);
        QCOMPARE(keys("stop"), QStringList());
        QVERIFY(!data("stop", ShortcutsModel::IsDefaultRole).toBool());
        QCOMPARE(m_model->conflict("next", false, key("s")), QString());
    }

    void testResetRestoresTheDefaultKeys()
    {
        m_model->assign("stop", false, key("Ctrl+Alt+s"));
        m_model->assign("stop", true, key("Ctrl+Alt+Shift+s"));

        QCOMPARE(m_model->resetConflict("stop"), QString());
        m_model->reset("stop");

        QCOMPARE(keys("stop").value(0), QStringLiteral("S"));
        QCOMPARE(globalKeys("stop"), QStringList());
        QVERIFY(data("stop", ShortcutsModel::IsDefaultRole).toBool());
        QVERIFY(!m_model->modified());
    }

    // a default key used by another action since: reset asks, then takes it
    void testResetTakesItsKeyBack()
    {
        m_model->assign("pause", false, key("s"));
        QCOMPARE(keys("stop").contains(QStringLiteral("S")), false);

        QCOMPARE(m_model->resetConflict("stop"), QStringLiteral("Pause"));
        m_model->reset("stop");

        QCOMPARE(keys("stop").value(0), QStringLiteral("S"));
        QCOMPARE(keys("pause"), QStringList());
    }

    void testResetAll()
    {
        m_model->assign("stop", false, key("Shift+r"));
        m_model->assign("open-folder", false, key("Ctrl+Alt+f"));
        m_model->clear("next", false);

        m_model->resetAll();

        for (int row = 0; row < m_model->rowCount(); ++row)
            QVERIFY(m_model->index(row).data(ShortcutsModel::IsDefaultRole).toBool());
        QCOMPARE(keys("record"), QStringList { QStringLiteral("Shift+R") });
        QVERIFY(!m_model->modified());
    }

    // Save writes the changed keys into the active Profile
    void testSaveWritesTheChangedKeys()
    {
        m_model->assign("stop", false, key("Ctrl+Alt+s"));
        m_model->assign("stop", true, key("Ctrl+Alt+Shift+s"));
        m_model->assign("open-folder", false, key("Ctrl+Alt+f"));
        m_model->clear("settings", false);

        m_model->save();

        QCOMPARE(m_options.value("key-stop"), QStringLiteral("Ctrl+Alt+s"));
        QCOMPARE(m_options.value("global-key-stop"), QStringLiteral("Ctrl+Alt+Shift+s"));
        QCOMPARE(m_options.value("qt-app-keys"), QStringLiteral("open-folder=Ctrl+Alt+f\tsettings="));
        // nothing else
        QCOMPARE(m_options.size(), 3);
        QCOMPARE(m_commits, 1);
        QVERIFY(!m_model->modified());
    }

    void testSavedKeysLoadAgain()
    {
        m_model->assign("stop", false, key("Ctrl+Alt+s"));
        m_model->assign("open-folder", false, key("Ctrl+Alt+f"));
        m_model->clear("settings", false);
        m_model->save();

        delete m_model;
        m_model = newModel();

        QCOMPARE(keys("stop"), QStringList { QStringLiteral("Ctrl+Alt+S") });
        QCOMPARE(keys("open-folder"), QStringList { QStringLiteral("Ctrl+Alt+F") });
        QCOMPARE(keys("settings"), QStringList());
        QVERIFY(!data("stop", ShortcutsModel::IsDefaultRole).toBool());
        QVERIFY(!m_model->modified());
    }

    // reset all, then save: the Profile goes back to the defaults
    void testSaveAfterResetAll()
    {
        m_options.insert("key-stop", "Ctrl+Alt+s");
        m_options.insert("qt-app-keys", "open-folder=Ctrl+Alt+f");
        delete m_model;
        m_model = newModel();

        m_model->resetAll();
        m_model->save();

        QCOMPARE(m_options.value("key-stop"), formatDefault("key-stop"));
        QCOMPARE(m_options.value("qt-app-keys"), QString());
        QCOMPARE(keys("open-folder"), QStringList { QStringLiteral("Ctrl+Shift+O") });
    }

    // Cancel
    void testRevertDropsTheChanges()
    {
        m_model->assign("stop", false, key("Shift+r"));
        m_model->revert();

        QCOMPARE(keys("stop").value(0), QStringLiteral("S"));
        QCOMPARE(keys("record"), QStringList { QStringLiteral("Shift+R") });
        QVERIFY(!m_model->modified());
        QCOMPARE(m_options.size(), 0);
        QCOMPARE(m_commits, 0);
    }

    // search plus All fields / Action / Key
    void testSearch()
    {
        ShortcutsFilterModel filter;
        filter.setSourceModel(m_model);

        filter.setSearchPattern("subtitle track");
        QVERIFY(filter.rowOf("subtitle-track") >= 0);
        QVERIFY(filter.rowOf("subtitle-revtrack") >= 0);
        QCOMPARE(filter.rowOf("stop"), -1);

        filter.setSearchPattern("shift+r");
        QCOMPARE(filter.field(), ShortcutsFilterModel::AllFields);
        QVERIFY(filter.rowOf("record") >= 0);

        filter.setField(ShortcutsFilterModel::Action);
        QCOMPARE(filter.rowOf("record"), -1);

        // keys starting with the search
        filter.setField(ShortcutsFilterModel::Key);
        QVERIFY(filter.rowOf("record") >= 0);
        QCOMPARE(filter.rowOf("uncrop-top"), -1); // Alt+Shift+R
        QCOMPARE(filter.rowOf("random"), -1);     // R

        filter.setSearchPattern("ctrl + shift +");
        QVERIFY(filter.rowOf("open-folder") >= 0);
        QCOMPARE(filter.rowOf("open-file"), -1);

        filter.setSearchPattern("record");
        QCOMPARE(filter.count(), 0);

        filter.setSearchPattern("");
        QCOMPARE(filter.count(), m_model->rowCount());
    }

private:
    static QString formatDefault(const char* option)
    {
        return qfu(config_FindConfig(option)->orig.psz);
    }
};

QTEST_GUILESS_MAIN(TestShortcutsModel)
#include "test_shortcuts_model.moc"
