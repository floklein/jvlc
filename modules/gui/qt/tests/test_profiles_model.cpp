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
#include "vlc_stub_modules.hpp"

#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QAbstractItemModelTester>

#include <vlc_configuration.h>

#include "dialogs/settings/profiles_model.hpp"

/*
 * Profiles over core options, which exist whatever plugins are built, and
 * a temporary Qt settings file. The test environment runs with
 * --ignore-config and the model's save function is replaced, so no vlcrc
 * is written.
 */
class TestProfilesModel : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<ProfilesModel> m_model;
    int m_saveCount = 0;

    std::unique_ptr<ProfilesModel> makeModel()
    {
        auto model = std::make_unique<ProfilesModel>(m_env->intf, m_settings.get());
        model->setSaveFunction([this]() { ++m_saveCount; });
        return model;
    }

    static QStringList column(const QAbstractItemModel& model, int role)
    {
        QStringList list;
        for (int i = 0; i < model.rowCount(); ++i)
            list << model.index(i, 0).data(role).toString();
        return list;
    }

    static QString configString(const char* name)
    {
        char* value = config_GetPsz(name);
        const QString result = QString::fromUtf8(value);
        free(value);
        return result;
    }

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
        config_ResetAll();

        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<QSettings>(m_dir->filePath("vlc.conf"), QSettings::IniFormat);

        m_saveCount = 0;
        m_model = makeModel();
    }

    void cleanup()
    {
        m_model.reset();
        m_settings.reset();
        m_dir.reset();
        config_ResetAll();
    }

    void testModelIsConsistent()
    {
        QAbstractItemModelTester tester(m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(!m_model->create("Movies").isEmpty());
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
    }

    void testDefaultProfileAlwaysExists()
    {
        QCOMPARE(m_model->rowCount(), 1);
        QCOMPARE(m_model->activeId(), ProfilesModel::DEFAULT_ID);
        QCOMPARE(m_model->activeName(), QStringLiteral("Default"));
        QCOMPARE(column(*m_model, ProfilesModel::NameRole), QStringList { "Default" });
        QCOMPARE(m_model->index(0).data(ProfilesModel::ActiveRole).toBool(), true);

        // a stored list without it gets it back, first
        m_model.reset();
        m_settings->beginGroup("Profiles");
        m_settings->beginWriteArray("List");
        m_settings->setArrayIndex(0);
        m_settings->setValue("Id", "work");
        m_settings->setValue("Name", "Work");
        m_settings->endArray();
        m_settings->setValue("Active", "work");
        m_settings->endGroup();

        m_model = makeModel();
        QCOMPARE(column(*m_model, ProfilesModel::NameRole), (QStringList { "Default", "Work" }));
        QCOMPARE(m_model->activeName(), QStringLiteral("Work"));

        // an unknown active Profile falls back to Default
        m_model.reset();
        m_settings->setValue("Profiles/Active", "gone");
        m_model = makeModel();
        QCOMPARE(m_model->activeId(), ProfilesModel::DEFAULT_ID);
    }

    void testCreateCopiesTheActiveProfile()
    {
        config_PutInt("mouse-hide-timeout", 2500);
        config_PutPsz("key-play-pause", "Ctrl+p");
        m_settings->setValue("MainWindow/color-scheme-index", 1);
        m_settings->setValue("PlayerLayouts/SelectedProfile", 1);

        QSignalSpy activeSpy(m_model.get(), &ProfilesModel::activeChanged);
        QSignalSpy switchedSpy(m_model.get(), &ProfilesModel::switched);
        const QString id = m_model->create("  Movies ");
        QVERIFY(!id.isEmpty());

        // the new Profile becomes the active one, with the same settings
        QCOMPARE(column(*m_model, ProfilesModel::NameRole), (QStringList { "Default", "Movies" }));
        QCOMPARE(m_model->activeId(), id);
        QCOMPARE(m_model->activeName(), QStringLiteral("Movies"));
        QCOMPARE(m_model->index(1).data(ProfilesModel::ActiveRole).toBool(), true);
        QCOMPARE(m_model->index(0).data(ProfilesModel::ActiveRole).toBool(), false);
        QCOMPARE(activeSpy.count(), 1);
        QCOMPARE(switchedSpy.count(), 0);
        QCOMPARE(config_GetInt("mouse-hide-timeout"), 2500);
        QCOMPARE(configString("key-play-pause"), QStringLiteral("Ctrl+p"));
        QCOMPARE(m_settings->value("MainWindow/color-scheme-index").toInt(), 1);
        QVERIFY(!m_model->restartRequired());

        // Default kept its settings
        config_PutInt("mouse-hide-timeout", 4000);
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QCOMPARE(config_GetInt("mouse-hide-timeout"), 2500);
        QCOMPARE(configString("key-play-pause"), QStringLiteral("Ctrl+p"));

        // the list and the active Profile are kept
        m_model = makeModel();
        QCOMPARE(column(*m_model, ProfilesModel::NameRole), (QStringList { "Default", "Movies" }));
        QCOMPARE(m_model->activeId(), ProfilesModel::DEFAULT_ID);
    }

    void testNamesAreNeededAndUnique()
    {
        QVERIFY(!m_model->isNameAvailable(""));
        QVERIFY(!m_model->isNameAvailable("   "));
        QVERIFY(!m_model->isNameAvailable("default"));
        QVERIFY(m_model->isNameAvailable("Work"));

        QVERIFY(!m_model->create("Work").isEmpty());
        QVERIFY(!m_model->isNameAvailable(" work "));
        QVERIFY(m_model->create("WORK").isEmpty());
        QVERIFY(m_model->create("").isEmpty());
        QCOMPARE(m_model->rowCount(), 2);
    }

    void testSwitchAppliesTheProfileOptions()
    {
        const QString defaultKey = configString("key-play-pause");
        config_PutInt("mouse-hide-timeout", 2500);
        const QString movies = m_model->create("Movies");

        // Movies changes one option and puts another back to its default
        config_PutInt("mouse-hide-timeout", 1000);
        config_PutInt("video-on-top", 1);
        config_PutPsz("key-play-pause", "Ctrl+p");

        QSignalSpy aboutSpy(m_model.get(), &ProfilesModel::aboutToSwitch);
        QSignalSpy switchedSpy(m_model.get(), &ProfilesModel::switched);
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QCOMPARE(config_GetInt("mouse-hide-timeout"), 2500);
        QCOMPARE(config_GetInt("video-on-top"), 0);
        QCOMPARE(configString("key-play-pause"), defaultKey);
        QCOMPARE(aboutSpy.count(), 1);
        QCOMPARE(switchedSpy.count(), 1);
        QCOMPARE(m_saveCount, 1);

        QVERIFY(m_model->activate(movies));
        QCOMPARE(config_GetInt("mouse-hide-timeout"), 1000);
        QCOMPARE(config_GetInt("video-on-top"), 1);
        QCOMPARE(configString("key-play-pause"), QStringLiteral("Ctrl+p"));
        QCOMPARE(m_saveCount, 2);

        // the active Profile, or an unknown one, is no switch
        QVERIFY(m_model->activate(movies));
        QVERIFY(!m_model->activate("gone"));
        QCOMPARE(switchedSpy.count(), 2);
        QCOMPARE(m_model->activeId(), movies);
    }

    void testThemeScaleAndLayoutsArePerProfile()
    {
        QVERIFY(ProfilesModel::profileSettings().contains("MainWindow/color-scheme-index"));
        QVERIFY(ProfilesModel::profileSettings().contains("MainWindow/interface-scale"));
        QVERIFY(ProfilesModel::profileSettings().contains("PlayerLayouts/"));

        m_settings->setValue("MainWindow/color-scheme-index", 1);
        m_settings->setValue("MainWindow/interface-scale", "1.25");
        m_settings->setValue("PlayerLayouts/SelectedProfile", 1);
        m_settings->setValue("PlayerLayouts/Profiles/1/Name", "Two-row");
        m_settings->setValue("PlayerLayouts/Profiles/size", 1);
        const QString work = m_model->create("Work");

        m_settings->setValue("MainWindow/color-scheme-index", 2);
        m_settings->remove("MainWindow/interface-scale");
        m_settings->remove("PlayerLayouts");
        m_settings->setValue("PlayerLayouts/SelectedProfile", 0);

        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QCOMPARE(m_settings->value("MainWindow/color-scheme-index").toInt(), 1);
        QCOMPARE(m_settings->value("MainWindow/interface-scale").toString(), QStringLiteral("1.25"));
        QCOMPARE(m_settings->value("PlayerLayouts/SelectedProfile").toInt(), 1);
        QCOMPARE(m_settings->value("PlayerLayouts/Profiles/1/Name").toString(), QStringLiteral("Two-row"));

        QVERIFY(m_model->activate(work));
        QCOMPARE(m_settings->value("MainWindow/color-scheme-index").toInt(), 2);
        QVERIFY(!m_settings->contains("MainWindow/interface-scale"));
        QCOMPARE(m_settings->value("PlayerLayouts/SelectedProfile").toInt(), 0);
        QVERIFY(!m_settings->contains("PlayerLayouts/Profiles/1/Name"));
    }

    void testLibraryHistoryPresetsAndMatchesStayShared()
    {
        // stores outside the Profiles: Presets, Subtitle matches, the Queue,
        // the interface state; the library and its history are in the media
        // library, the interface language outside the VLC options
        m_settings->setValue("Presets/List/1/Name", "Anime");
        m_settings->setValue("SubtitleMatches/abc", "/shows/a.srt");
        m_settings->setValue("MainWindow/playqueue-panel-width", 300);
        const QString work = m_model->create("Work");

        m_settings->setValue("Presets/List/1/Name", "Anime (dubbed)");
        m_settings->setValue("SubtitleMatches/def", "/shows/b.srt");

        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QCOMPARE(m_settings->value("Presets/List/1/Name").toString(), QStringLiteral("Anime (dubbed)"));
        QCOMPARE(m_settings->value("SubtitleMatches/abc").toString(), QStringLiteral("/shows/a.srt"));
        QCOMPARE(m_settings->value("SubtitleMatches/def").toString(), QStringLiteral("/shows/b.srt"));
        QCOMPARE(m_settings->value("MainWindow/playqueue-panel-width").toInt(), 300);

        QVERIFY(m_model->activate(work));
        QCOMPARE(m_settings->value("Presets/List/1/Name").toString(), QStringLiteral("Anime (dubbed)"));
        QCOMPARE(m_settings->value("SubtitleMatches/def").toString(), QStringLiteral("/shows/b.srt"));
    }

    void testRestartIsRequiredForOptionsReadAtStartup()
    {
        m_model->setRestartCheck([](const QString& option) {
            return option == QLatin1String("key-play-pause");
        });
        QSignalSpy spy(m_model.get(), &ProfilesModel::restartRequiredChanged);

        const QString movies = m_model->create("Movies");
        config_PutInt("mouse-hide-timeout", 3000);

        // an option applied at once needs no restart
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QVERIFY(!m_model->restartRequired());

        QVERIFY(m_model->activate(movies));
        config_PutPsz("key-play-pause", "Ctrl+p");
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QVERIFY(!m_model->restartRequired());

        // what VLC started with differs
        QVERIFY(m_model->activate(movies));
        QVERIFY(m_model->restartRequired());
        QCOMPARE(spy.count(), 1);

        // and no longer
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QVERIFY(!m_model->restartRequired());
        QCOMPARE(spy.count(), 2);
    }

    void testTheRegistryTellsWhichOptionsNeedARestart()
    {
        // by default, the Settings option registry: the global keys are
        // read once, the pointer timeout at once
        const QString movies = m_model->create("Movies");
        config_PutInt("mouse-hide-timeout", 3000);
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QVERIFY(!m_model->restartRequired());

        QVERIFY(m_model->activate(movies));
        config_PutPsz("global-key-play-pause", "Ctrl+Alt+p");
        QVERIFY(m_model->activate(ProfilesModel::DEFAULT_ID));
        QVERIFY(m_model->activate(movies));
        QVERIFY(m_model->restartRequired());
    }
};

QTEST_GUILESS_MAIN(TestProfilesModel)
#include "test_profiles_model.moc"
