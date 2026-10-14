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

#include "dialogs/settings/settings_model.hpp"

/*
 * The rows are bound to core options, which exist whatever plugins are
 * built. The test environment runs with --ignore-config, and the model's
 * save function is replaced, so no vlcrc is written.
 */
class TestSettingsModel : public QObject
{
    Q_OBJECT

private:
    using Row = SettingsModel::Row;

    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<SettingsModel> m_model;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_settings;
    int m_saveCount = 0;

    QModelIndex indexOf(const QString& id) const
    {
        for (int i = 0; i < m_model->rowCount(); ++i)
        {
            const QModelIndex index = m_model->index(i);
            if (index.data(SettingsModel::IdRole).toString() == id)
                return index;
        }
        return {};
    }

    QVariant role(const QString& id, int role) const
    {
        return indexOf(id).data(role);
    }

    Row configRow(const QString& id, const QString& page, const QStringList& options)
    {
        Row row;
        row.id = id;
        row.page = page;
        row.section = QStringLiteral("Section");
        row.label = id;
        m_model->bindConfig(row, options);
        return row;
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
        config_PutInt("mouse-hide-timeout", 1000);
        config_PutInt("fullscreen", 0);
        config_PutInt("video-on-top", 0);
        config_PutInt("stats", 1);

        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<QSettings>(m_dir->filePath("vlc.conf"), QSettings::IniFormat);

        m_saveCount = 0;
        m_model = std::make_unique<SettingsModel>(m_env->intf);
        m_model->setSaveDelay(10);
        m_model->setSaveFunction([this]() { ++m_saveCount; });
    }

    void cleanup()
    {
        m_model.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void testModelIsConsistent()
    {
        QAbstractItemModelTester tester(m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        m_model->addRow(configRow("top", "interface", {"video-on-top"}));
        QCOMPARE(m_model->rowCount(), 2);
    }

    void testConfigRowShowsLiveValueAndDefault()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));

        QCOMPARE(role("hide", SettingsModel::ValueRole).toInt(), 1000);
        QCOMPARE(role("hide", SettingsModel::DefaultValueRole).toInt(), 1000);
        QCOMPARE(role("hide", SettingsModel::IsDefaultRole).toBool(), true);
        QCOMPARE(role("hide", SettingsModel::AvailableRole).toBool(), true);

        // a change made elsewhere shows on the next read
        config_PutInt("video-on-top", 1);
        m_model->addRow(configRow("top", "interface", {"video-on-top"}));
        QCOMPARE(role("top", SettingsModel::ValueRole).toBool(), true);
        QCOMPARE(role("top", SettingsModel::IsDefaultRole).toBool(), false);
    }

    void testSetValueAppliesImmediatelyAndSaves()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        QSignalSpy dataSpy(m_model.get(), &QAbstractItemModel::dataChanged);
        QSignalSpy changedSpy(m_model.get(), &SettingsModel::settingChanged);

        QVERIFY(m_model->setValue("hide", 3000));

        QCOMPARE(config_GetInt("mouse-hide-timeout"), 3000);
        QCOMPARE(role("hide", SettingsModel::ValueRole).toInt(), 3000);
        QCOMPARE(role("hide", SettingsModel::IsDefaultRole).toBool(), false);
        QCOMPARE(dataSpy.count(), 1);
        QCOMPARE(changedSpy.count(), 1);
        QCOMPARE(changedSpy.at(0).at(0).toString(), QStringLiteral("hide"));

        // several quick changes are saved once
        QVERIFY(m_model->setValue("hide", 4000));
        QTRY_COMPARE(m_saveCount, 1);
        QTest::qWait(50);
        QCOMPARE(m_saveCount, 1);
    }

    void testSetDataOnValueRoleWrites()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));

        QVERIFY(m_model->setData(indexOf("hide"), 2500, SettingsModel::ValueRole));
        QCOMPARE(config_GetInt("mouse-hide-timeout"), 2500);

        QVERIFY(!m_model->setData(indexOf("hide"), 2500, SettingsModel::LabelRole));
    }

    void testSettingTheSameValueDoesNothing()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        QSignalSpy changedSpy(m_model.get(), &SettingsModel::settingChanged);

        QVERIFY(m_model->setValue("hide", 1000));
        QCOMPARE(changedSpy.count(), 0);
        QTest::qWait(50);
        QCOMPARE(m_saveCount, 0);
    }

    void testResetRestoresTheDefault()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        QVERIFY(m_model->setValue("hide", 3000));

        m_model->reset("hide");

        QCOMPARE(config_GetInt("mouse-hide-timeout"), 1000);
        QCOMPARE(role("hide", SettingsModel::IsDefaultRole).toBool(), true);
    }

    void testOneRowWritesEveryOption()
    {
        m_model->addRow(configRow("both", "interface", {"fullscreen", "video-on-top"}));

        QVERIFY(m_model->setValue("both", true));

        QCOMPARE(config_GetInt("fullscreen"), 1);
        QCOMPARE(config_GetInt("video-on-top"), 1);
    }

    void testMissingOptionIsUnavailable()
    {
        m_model->addRow(configRow("missing", "interface", {"no-such-option-in-vlc"}));
        m_model->addRow(configRow("partly", "interface", {"no-such-option-in-vlc", "video-on-top"}));

        QCOMPARE(role("missing", SettingsModel::AvailableRole).toBool(), false);
        QVERIFY(!m_model->setValue("missing", true));

        // a row is usable as long as one of its options exists
        QCOMPARE(role("partly", SettingsModel::AvailableRole).toBool(), true);
        QVERIFY(m_model->setValue("partly", true));
        QCOMPARE(config_GetInt("video-on-top"), 1);
    }

    void testRestartRowsAskForARestart()
    {
        Row row = configRow("stats", "general", {"stats"});
        row.effect = SettingsModel::Restart;
        m_model->addRow(row);
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        QSignalSpy restartSpy(m_model.get(), &SettingsModel::restartRequiredChanged);

        QVERIFY(!m_model->restartRequired());

        // rows that apply immediately never ask for a restart
        QVERIFY(m_model->setValue("hide", 3000));
        QVERIFY(!m_model->restartRequired());

        QVERIFY(m_model->setValue("stats", false));
        QVERIFY(m_model->restartRequired());
        QCOMPARE(role("stats", SettingsModel::PendingRestartRole).toBool(), true);
        QCOMPARE(restartSpy.count(), 1);

        // going back to the value VLC runs with cancels it
        QVERIFY(m_model->setValue("stats", true));
        QVERIFY(!m_model->restartRequired());
        QCOMPARE(role("stats", SettingsModel::PendingRestartRole).toBool(), false);
        QCOMPARE(restartSpy.count(), 2);
    }

    void testSettingsStoreRow()
    {
        Row row;
        row.id = "remember";
        row.page = "interface";
        row.label = "Remember window size and position";
        SettingsModel::bindSettings(row, m_settings.get(), "MainWindow/remember-geometry", true);
        m_model->addRow(row);

        QCOMPARE(role("remember", SettingsModel::ValueRole).toBool(), true);
        QCOMPARE(role("remember", SettingsModel::IsDefaultRole).toBool(), true);

        QVERIFY(m_model->setValue("remember", false));
        QCOMPARE(m_settings->value("MainWindow/remember-geometry").toBool(), false);
        QCOMPARE(role("remember", SettingsModel::IsDefaultRole).toBool(), false);

        m_model->reset("remember");
        QCOMPARE(m_settings->value("MainWindow/remember-geometry", true).toBool(), true);
        QCOMPARE(role("remember", SettingsModel::IsDefaultRole).toBool(), true);
    }

    void testCustomStoreRow()
    {
        double scale = 1.0;
        Row row;
        row.id = "scale";
        row.page = "general";
        row.control = SettingsModel::Slider;
        row.read = [&scale]() { return QVariant(scale); };
        row.readDefault = []() { return QVariant(1.0); };
        row.write = [&scale](const QVariant& value) { scale = value.toDouble(); return true; };
        m_model->addRow(row);

        QVERIFY(m_model->setValue("scale", 1.5));
        QCOMPARE(scale, 1.5);
        QCOMPARE(role("scale", SettingsModel::IsDefaultRole).toBool(), false);

        // values that differ by a rounding error are the default
        scale = 1.0 + 1e-9;
        QCOMPARE(role("scale", SettingsModel::IsDefaultRole).toBool(), true);
    }

    void testFilterShowsOnePage()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        m_model->addRow(configRow("top", "interface", {"video-on-top"}));
        m_model->addRow(configRow("stats", "general", {"stats"}));
        m_model->addRow(configRow("missing", "general", {"no-such-option-in-vlc"}));

        SettingsFilterModel filter;
        filter.setSourceModel(m_model.get());

        filter.setPage("interface");
        QCOMPARE(filter.rowCount(), 2);

        // unavailable rows are never shown
        filter.setPage("general");
        QCOMPARE(filter.rowCount(), 1);
        QCOMPARE(filter.index(0, 0).data(SettingsModel::IdRole).toString(), QStringLiteral("stats"));
    }

    void testSearchCoversLabelsHelpAndChoices()
    {
        Row hide = configRow("hide", "interface", {"mouse-hide-timeout"});
        hide.label = "Hide the pointer after";
        hide.help = "Time without moving the mouse";
        m_model->addRow(hide);

        Row top = configRow("top", "interface", {"video-on-top"});
        top.label = "Keep window above other windows";
        top.choices = { SettingsModel::choice(true, "Always on top"), SettingsModel::choice(false, "Normal") };
        m_model->addRow(top);

        Row stats = configRow("stats", "general", {"stats"});
        stats.label = "Collect playback statistics";
        stats.section = "Diagnostics";
        m_model->addRow(stats);

        SettingsFilterModel filter;
        filter.setSourceModel(m_model.get());
        filter.setPage("general");

        // search ignores the page and the case
        filter.setSearchPattern("POINTER");
        QCOMPARE(filter.rowCount(), 1);
        QCOMPARE(filter.index(0, 0).data(SettingsModel::IdRole).toString(), QStringLiteral("hide"));

        filter.setSearchPattern("mouse");
        QCOMPARE(filter.rowCount(), 1);

        filter.setSearchPattern("always on");
        QCOMPARE(filter.rowCount(), 1);
        QCOMPARE(filter.index(0, 0).data(SettingsModel::IdRole).toString(), QStringLiteral("top"));

        filter.setSearchPattern("diagnostics");
        QCOMPARE(filter.rowCount(), 1);

        // every word must match
        filter.setSearchPattern("window pointer");
        QCOMPARE(filter.rowCount(), 0);

        filter.setSearchPattern("");
        QCOMPARE(filter.rowCount(), 1);
    }

    void testDeveloperRowsShowInDeveloperMode()
    {
        Row verbose = configRow("verbose", "general", {"stats"});
        verbose.developer = true;
        m_model->addRow(verbose);

        SettingsFilterModel filter;
        filter.setSourceModel(m_model.get());
        filter.setPage("general");
        QCOMPARE(filter.rowCount(), 0);

        filter.setShowDeveloper(true);
        QCOMPARE(filter.rowCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestSettingsModel)
#include "test_settings_model.moc"
