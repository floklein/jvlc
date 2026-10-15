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
#include "../../../../lib/libvlc_internal.h"

#include <QTest>
#include <QDir>
#include <QSignalSpy>

#include "../dialogs/onboarding/onboarding_model.hpp"

// Onboarding: its steps, Back, Next, Skip and Finish, and what it applies
class TestOnboardingModel : public QObject
{
    Q_OBJECT

private:
    using Choices = OnboardingModel::Choices;

    libvlc_instance_t* m_vlc = nullptr;
    OnboardingModel* m_model = nullptr;

    // what the model applied, once per Finish or Skip
    QList<Choices> m_applied;
    // the live preview of the Theme and Accent color
    QList<QPair<int, QString>> m_previews;

    static constexpr int System = 0;
    static constexpr int Light = 1;
    static constexpr int Dark = 2;

    vlc_object_t* obj() const { return VLC_OBJECT(m_vlc->p_libvlc_int); }

    // what onboarding opens with
    static Choices initialChoices()
    {
        Choices choices;
        choices.folders = { QStringLiteral("file:///home/user/Videos"),
                            QStringLiteral("file:///home/user/Music") };
        choices.theme = System;
        choices.accentColor = QStringLiteral("default");
        choices.fetchOnline = true;
        return choices;
    }

    OnboardingModel* newModel(bool hasLibrary = true)
    {
        auto model = new OnboardingModel(this);
        model->setApplyFunction([this](const Choices& choices) {
            m_applied.append(choices);
        });
        model->setPreviewFunction([this](int theme, const QString& accent) {
            m_previews.append({ theme, accent });
        });
        model->load(obj(), initialChoices(), hasLibrary);
        return model;
    }

    // the keys of the actions of a row of the shortcuts step, as key caps
    QList<QStringList> caps(const QString& label) const
    {
        for (const QVariant& rowValue : m_model->shortcuts())
        {
            const QVariantMap row = rowValue.toMap();
            if (row.value("label").toString() != label)
                continue;
            QList<QStringList> result;
            for (const QVariant& key : row.value("keys").toList())
                result.append(key.toMap().value("caps").toStringList());
            return result;
        }
        return {};
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
        m_applied.clear();
        m_previews.clear();
        m_model = newModel();
    }

    void cleanup()
    {
        delete m_model;
        m_model = nullptr;
    }

    // library folders, Theme and Accent color, Privacy, then the shortcuts
    void testStepsInOrder()
    {
        QCOMPARE(m_model->stepCount(), 4);
        QCOMPARE(m_model->step(), 0);
        QCOMPARE(m_model->page(), OnboardingModel::Folders);

        m_model->next();
        QCOMPARE(m_model->page(), OnboardingModel::Theme);
        m_model->next();
        QCOMPARE(m_model->page(), OnboardingModel::Privacy);
        m_model->next();
        QCOMPARE(m_model->page(), OnboardingModel::Shortcuts);
        QVERIFY(m_model->isLastStep());
        QVERIFY(m_applied.isEmpty());
    }

    void testBackReturnsToThePreviousStep()
    {
        m_model->back();
        QCOMPARE(m_model->step(), 0);

        m_model->next();
        m_model->next();
        m_model->back();
        QCOMPARE(m_model->page(), OnboardingModel::Theme);
    }

    // Next on the last step is Finish: the choices apply once
    void testFinishAppliesTheChoicesOnce()
    {
        QSignalSpy finished(m_model, &OnboardingModel::finished);

        for (int i = 0; i < m_model->stepCount(); ++i)
            m_model->next();

        QCOMPARE(finished.count(), 1);
        QCOMPARE(m_applied.size(), 1);
        QCOMPARE(m_applied[0].folders, initialChoices().folders);
        QCOMPARE(m_applied[0].theme, System);
        QCOMPARE(m_applied[0].accentColor, QStringLiteral("default"));
        QVERIFY(m_applied[0].fetchOnline);

        // finished: nothing more applies
        m_model->next();
        m_model->skip();
        QCOMPARE(m_applied.size(), 1);
        QCOMPARE(finished.count(), 1);
    }

    // Skip on the first step applies the defaults
    void testSkipAppliesTheDefaults()
    {
        QSignalSpy finished(m_model, &OnboardingModel::finished);
        m_model->removeFolder(0);
        m_model->setFetchOnline(false);

        m_model->skip();

        QCOMPARE(finished.count(), 1);
        QVERIFY(m_model->isDone());
        QCOMPARE(m_applied.size(), 1);
        QCOMPARE(m_applied[0].folders, initialChoices().folders);
        QCOMPARE(m_applied[0].theme, System);
        QCOMPARE(m_applied[0].accentColor, QStringLiteral("default"));
        QVERIFY(m_applied[0].fetchOnline);
    }

    // the steps left with Next keep their choices, the others get the defaults
    void testSkipKeepsTheStepsLeftWithNext()
    {
        m_model->removeFolder(1);
        m_model->next();
        m_model->setTheme(Light);
        m_model->setAccentColor(QStringLiteral("#19a234"));
        m_model->next();
        m_model->setFetchOnline(false);

        m_model->skip();

        QCOMPARE(m_applied.size(), 1);
        QCOMPARE(m_applied[0].folders, QStringList{ QStringLiteral("file:///home/user/Videos") });
        QCOMPARE(m_applied[0].theme, Light);
        QCOMPARE(m_applied[0].accentColor, QStringLiteral("#19a234"));
        QVERIFY(m_applied[0].fetchOnline);
    }

    // a Theme picked, then skipped, goes back to the one shown before
    void testSkipOnTheThemeStepUndoesThePreview()
    {
        m_model->next();
        m_model->setTheme(Dark);
        m_model->skip();

        QCOMPARE(m_applied[0].theme, System);
        QCOMPARE(m_previews.last(), qMakePair(System, QStringLiteral("default")));
    }

    // Back does not undo the steps already seen
    void testBackKeepsTheChoices()
    {
        m_model->next();
        m_model->setTheme(Light);
        m_model->back();
        m_model->next();
        m_model->next();
        m_model->skip();

        QCOMPARE(m_applied[0].theme, Light);
    }

    void testFoldersStartWithVideosAndMusic()
    {
        QCOMPARE(m_model->folders(), initialChoices().folders);
    }

    void testAddFolder()
    {
        QSignalSpy changed(m_model, &OnboardingModel::foldersChanged);

        QVERIFY(m_model->addFolder(QStringLiteral("file:///mnt/films")));

        QCOMPARE(changed.count(), 1);
        QCOMPARE(m_model->folders().size(), 3);
        QCOMPARE(m_model->folders().last(), QStringLiteral("file:///mnt/films"));
    }

    void testAddFolderTwiceKeepsOne()
    {
        QVERIFY(!m_model->addFolder(QStringLiteral("file:///home/user/Music")));
        QVERIFY(!m_model->addFolder(QString()));
        QCOMPARE(m_model->folders(), initialChoices().folders);
    }

    // removing both default folders is allowed
    void testRemoveEveryFolder()
    {
        m_model->removeFolder(0);
        m_model->removeFolder(0);
        m_model->removeFolder(0);
        QVERIFY(m_model->folders().isEmpty());

        for (int i = 0; i < m_model->stepCount(); ++i)
            m_model->next();
        QVERIFY(m_applied[0].folders.isEmpty());
    }

    void testFolderPathIsTheLocalPath()
    {
        QCOMPARE(m_model->folderPath(QStringLiteral("file:///home/user/My%20Videos")),
                 QDir::toNativeSeparators(QStringLiteral("/home/user/My Videos")));
    }

    // without a media library, there is no folders step and no folder applies
    void testWithoutLibraryThereIsNoFoldersStep()
    {
        delete m_model;
        m_model = newModel(false);

        QCOMPARE(m_model->stepCount(), 3);
        QCOMPARE(m_model->page(), OnboardingModel::Theme);

        m_model->skip();
        QVERIFY(m_applied[0].folders.isEmpty());
    }

    // the Theme and Accent color show at once, before Finish
    void testThemeAndAccentPreviewLive()
    {
        m_model->next();
        m_model->setTheme(Light);
        QCOMPARE(m_model->theme(), Light);
        QCOMPARE(m_previews.last(), qMakePair(Light, QStringLiteral("default")));

        m_model->setAccentColor(QStringLiteral("#8e42ff"));
        QCOMPARE(m_model->accentColor(), QStringLiteral("#8e42ff"));
        QCOMPARE(m_previews.last(), qMakePair(Light, QStringLiteral("#8e42ff")));
        QVERIFY(m_applied.isEmpty());
    }

    // the swatches of Settings, the default one first
    void testAccentChoices()
    {
        const QVariantList choices = m_model->accentChoices();
        QCOMPARE(choices.size(), 9);
        QCOMPARE(choices[0].toMap().value("value").toString(), QStringLiteral("default"));
        QCOMPARE(choices[0].toMap().value("color").toString(), QStringLiteral("#d37918"));
        QCOMPARE(choices[1].toMap().value("value").toString(), QStringLiteral("#e15032"));
    }

    void testFetchOnlineIsOnByDefault()
    {
        QVERIFY(m_model->fetchOnline());
        m_model->setFetchOnline(false);
        QVERIFY(!m_model->fetchOnline());
    }

    // the keys shown are the ones bound in the core key map
    void testShortcutsShowTheDefaultKeys()
    {
        QCOMPARE(caps("Play / Pause"), (QList<QStringList>{ { "Space" } }));
        QCOMPARE(caps("Seek backward / forward"), (QList<QStringList>{ { "←" }, { "→" } }));
        QCOMPARE(caps("Previous / next item"), (QList<QStringList>{ { "P" }, { "N" } }));
        QCOMPARE(caps("Previous / next frame"), (QList<QStringList>{ { "Shift", "E" }, { "E" } }));
        QCOMPARE(caps("Cycle subtitle track"), (QList<QStringList>{ { "V" } }));
        QCOMPARE(caps("Cycle audio track"), (QList<QStringList>{ { "B" } }));
        QCOMPARE(caps("Toggle fullscreen"), (QList<QStringList>{ { "F" } }));
        QCOMPARE(caps("Increase / decrease volume"), (QList<QStringList>{ { "↑" }, { "↓" } }));
        QCOMPARE(caps("Show / hide Queue"), (QList<QStringList>{ { "Ctrl", "L" } }));
        QCOMPARE(caps("Take a snapshot"), (QList<QStringList>{ { "Shift", "S" } }));
        QCOMPARE(caps("Record"), (QList<QStringList>{ { "Shift", "R" } }));
    }

    // each key also has its text, for screen readers
    void testShortcutKeysHaveText()
    {
        for (const QVariant& rowValue : m_model->shortcuts())
        {
            const QVariantMap row = rowValue.toMap();
            if (row.value("label").toString() != QLatin1String("Take a snapshot"))
                continue;
            QCOMPARE(row.value("keys").toList().value(0).toMap().value("text").toString(),
                     QStringLiteral("Shift+S"));
            return;
        }
        QFAIL("no snapshot row");
    }
};

QTEST_GUILESS_MAIN(TestOnboardingModel)
#include "test_onboarding_model.moc"
