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
#include <vlc_plugin.h>

#include "dialogs/settings/settings_model.hpp"
#include "dialogs/settings/settings_registry.hpp"

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

    static SettingsOption option(const char* name, const char* module, const char* moduleLabel,
                                 int subcategory, int type = CONFIG_ITEM_BOOL)
    {
        SettingsOption option;
        option.name = name;
        option.module = QString::fromLatin1(module);
        option.moduleLabel = QString::fromLatin1(moduleLabel);
        option.subcategory = subcategory;
        option.type = type;
        option.label = QStringLiteral("%1 label").arg(QString::fromLatin1(name));
        return option;
    }

    static QStringList ids(const QAbstractItemModel& model)
    {
        QStringList list;
        for (int i = 0; i < model.rowCount(); ++i)
            list << model.index(i, 0).data(SettingsModel::IdRole).toString();
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

    /* Rows writing a filter chain */

    void testFilterRowEditsTheChain()
    {
        config_PutPsz("video-filter", "sharpen");
        Row row;
        row.id = "adjust";
        row.page = "video";
        m_model->bindFilter(row, "video-filter", "adjust");
        m_model->addRow(row);

        QCOMPARE(role("adjust", SettingsModel::ValueRole).toBool(), false);
        QCOMPARE(role("adjust", SettingsModel::IsDefaultRole).toBool(), true);

        // other filters of the chain are kept
        QVERIFY(m_model->setValue("adjust", true));
        QCOMPARE(configString("video-filter"), QStringLiteral("sharpen:adjust"));
        QCOMPARE(role("adjust", SettingsModel::ValueRole).toBool(), true);

        QVERIFY(m_model->setValue("adjust", false));
        QCOMPARE(configString("video-filter"), QStringLiteral("sharpen"));

        // a chain written by hand with commas is understood too
        config_PutPsz("video-filter", "adjust,sharpen");
        QCOMPARE(role("adjust", SettingsModel::ValueRole).toBool(), true);
        QVERIFY(m_model->setValue("adjust", false));
        QCOMPARE(configString("video-filter"), QStringLiteral("sharpen"));

        // the chain is shared by every filter: it keeps its own row
        QVERIFY(role("adjust", SettingsModel::OptionsRole).toStringList().isEmpty());
        config_PutPsz("video-filter", nullptr);
    }

    void testFilterPluginGroupStartsWithItsChainToggle()
    {
        config_PutPsz("video-filter", nullptr);
        // a core option stands for the plugin's, which the tests don't load
        SettingsOption grayscale = option("grayscale", "adjust", "Image properties filter", SUBCAT_VIDEO_VFILTER);
        grayscale.filterChain = "video-filter";
        m_model->addAdvancedRows({ grayscale });

        SettingsFilterModel filter;
        filter.setSourceModel(m_model.get());
        filter.setPage("video");
        m_model->setAdvancedExpanded("video", true);
        m_model->setGroupExpanded("video", "adjust", true);
        QCOMPARE(ids(filter), (QStringList { "advanced:video", "group:video:adjust", "filter:adjust", "grayscale" }));

        QVERIFY(m_model->setValue("filter:adjust", true));
        QCOMPARE(configString("video-filter"), QStringLiteral("adjust"));
        config_PutPsz("video-filter", nullptr);
    }

    /* Reset all settings in this Profile */

    void testResetAllResetsTheProfile()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));

        Row remember;
        remember.id = "remember";
        remember.page = "interface";
        SettingsModel::bindSettings(remember, m_settings.get(), "MainWindow/remember-geometry", true);
        m_model->addRow(remember);

        // shared by every Profile: kept
        QString language = "fr";
        Row lang;
        lang.id = "language";
        lang.page = "general";
        lang.allProfiles = true;
        lang.read = [&language]() { return QVariant(language); };
        lang.readDefault = []() { return QVariant(QStringLiteral("auto")); };
        lang.write = [&language](const QVariant& value) { language = value.toString(); return true; };
        m_model->addRow(lang);

        QVERIFY(m_model->setValue("hide", 3000));
        QVERIFY(m_model->setValue("remember", false));
        // an option without a row is reset too
        config_PutInt("fullscreen", 1);
        QSignalSpy changedSpy(m_model.get(), &QAbstractItemModel::dataChanged);

        m_model->resetAll();

        QCOMPARE(config_GetInt("mouse-hide-timeout"), 1000);
        QCOMPARE(config_GetInt("fullscreen"), 0);
        QCOMPARE(m_settings->value("MainWindow/remember-geometry", true).toBool(), true);
        QCOMPARE(language, QStringLiteral("fr"));
        QCOMPARE(role("hide", SettingsModel::IsDefaultRole).toBool(), true);
        QVERIFY(changedSpy.count() >= 1);
        QTRY_COMPARE(m_saveCount, 1);
    }

    void testActionRowTriggers()
    {
        int triggered = 0;
        Row row;
        row.id = "reset-all";
        row.page = "general";
        row.control = SettingsModel::Action;
        row.action = [&triggered]() { ++triggered; };
        m_model->addRow(row);

        QCOMPARE(role("reset-all", SettingsModel::AvailableRole).toBool(), true);
        m_model->trigger("reset-all");
        QCOMPARE(triggered, 1);
    }

    /* Advanced Options */

    void testAdvancedOptionsAreCollapsedByDefault()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        m_model->addAdvancedRows({
            option("video-on-top", "core", 0, SUBCAT_INTERFACE_GENERAL),
            option("embedded-video", "core", 0, SUBCAT_INTERFACE_GENERAL),
            option("fullscreen", "fake_plugin", "Fake plugin", SUBCAT_INTERFACE_MAIN),
        });

        SettingsFilterModel filter;
        filter.setSourceModel(m_model.get());
        filter.setPage("interface");

        // the basic row, then the Advanced Options header
        QCOMPARE(ids(filter), (QStringList { "hide", "advanced:interface" }));
        QCOMPARE(filter.index(1, 0).data(SettingsModel::KindRole).toInt(), int(SettingsModel::AdvancedHeader));
        QCOMPARE(filter.index(1, 0).data(SettingsModel::ExpandedRole).toBool(), false);

        // core and Qt options first, then one collapsed group per plugin
        m_model->setAdvancedExpanded("interface", true);
        QCOMPARE(ids(filter), (QStringList { "hide", "advanced:interface", "video-on-top", "embedded-video",
                                             "group:interface:fake_plugin" }));
        QCOMPARE(filter.index(4, 0).data(SettingsModel::GroupLabelRole).toString(), QStringLiteral("Fake plugin"));

        m_model->setGroupExpanded("interface", "fake_plugin", true);
        QCOMPARE(ids(filter).last(), QStringLiteral("fullscreen"));
        QCOMPARE(filter.index(5, 0).data(SettingsModel::AdvancedRole).toBool(), true);
        QCOMPARE(filter.index(5, 0).data(SettingsModel::GroupRole).toString(), QStringLiteral("fake_plugin"));

        // other pages are not affected
        filter.setPage("general");
        QCOMPARE(filter.rowCount(), 0);
    }

    void testAdvancedOptionsRememberTheirState()
    {
        m_model->setStateSettings(m_settings.get());
        m_model->addAdvancedRows({ option("video-on-top", "core", 0, SUBCAT_INTERFACE_GENERAL) });
        QCOMPARE(m_model->isAdvancedExpanded("interface"), false);

        m_model->setAdvancedExpanded("interface", true);

        SettingsModel other(m_env->intf);
        other.setStateSettings(m_settings.get());
        other.addAdvancedRows({ option("video-on-top", "core", 0, SUBCAT_INTERFACE_GENERAL) });
        QCOMPARE(other.isAdvancedExpanded("interface"), true);
        QCOMPARE(other.isAdvancedExpanded("video"), false);
    }

    void testRegistryPlacesOptionsByCategory()
    {
        m_model->addAdvancedRows({
            option("sout-x264-preset", "x264", "x264 encoder", SUBCAT_INPUT_VCODEC),
            option("avcodec-threads", "avcodec", "FFmpeg", SUBCAT_INPUT_VCODEC),
            option("sout-avcodec-codec", "avcodec", "FFmpeg", SUBCAT_INPUT_VCODEC),
            option("freetype-font", "freetype", "Text renderer", SUBCAT_VIDEO_SUBPIC),
            option("logo-file", "logo", "Logo overlay", SUBCAT_VIDEO_SUBPIC),
            option("normvol-buff-size", "normvol", "Volume normalizer", SUBCAT_AUDIO_AFILTER),
            option("network-caching", "core", 0, SUBCAT_INPUT_GENERAL),
            option("metadata-network-access", "core", 0, SUBCAT_PLAYLIST_GENERAL),
            option("random", "core", 0, SUBCAT_PLAYLIST_GENERAL),
            option("mkv-preload-local-dir", "mkv", "Matroska", SUBCAT_INPUT_DEMUX),
            option("logmode", "file_logger", "File logger", SUBCAT_ADVANCED_MISC),
            option("lua-intf", "lua", "Lua", SUBCAT_INTERFACE_MAIN),
            option("qt-max-volume", "qt", "Qt interface", SUBCAT_INTERFACE_MAIN),
            option("qt-embedded-open", "qt", "Qt interface", SUBCAT_INTERFACE_MAIN),
        });

        QCOMPARE(role("sout-x264-preset", SettingsModel::PageRole).toString(), QStringLiteral("recording"));
        QCOMPARE(role("sout-x264-preset", SettingsModel::GroupRole).toString(), QStringLiteral("x264"));
        // one plugin can have a group on two pages
        QCOMPARE(role("avcodec-threads", SettingsModel::PageRole).toString(), QStringLiteral("performance"));
        QCOMPARE(role("sout-avcodec-codec", SettingsModel::PageRole).toString(), QStringLiteral("recording"));
        QCOMPARE(role("freetype-font", SettingsModel::PageRole).toString(), QStringLiteral("subtitles"));
        QCOMPARE(role("logo-file", SettingsModel::PageRole).toString(), QStringLiteral("video"));
        QCOMPARE(role("normvol-buff-size", SettingsModel::PageRole).toString(), QStringLiteral("audio"));
        QCOMPARE(role("network-caching", SettingsModel::PageRole).toString(), QStringLiteral("network"));
        QCOMPARE(role("metadata-network-access", SettingsModel::PageRole).toString(), QStringLiteral("library"));
        QCOMPARE(role("random", SettingsModel::PageRole).toString(), QStringLiteral("playback"));
        QCOMPARE(role("mkv-preload-local-dir", SettingsModel::PageRole).toString(), QStringLiteral("performance"));
        QCOMPARE(role("logmode", SettingsModel::PageRole).toString(), QStringLiteral("general"));
        QCOMPARE(role("lua-intf", SettingsModel::PageRole).toString(), QStringLiteral("extensions"));
        QCOMPARE(role("qt-max-volume", SettingsModel::PageRole).toString(), QStringLiteral("audio"));

        // the core and Qt options are not grouped
        QCOMPARE(role("network-caching", SettingsModel::GroupRole).toString(), QString());
        QCOMPARE(role("qt-embedded-open", SettingsModel::PageRole).toString(), QStringLiteral("interface"));
        QCOMPARE(role("qt-embedded-open", SettingsModel::GroupRole).toString(), QString());
    }

    void testRegistryHidesCommandLineAndCaptureOptions()
    {
        SettingsOption hidden = option("bookmark1", "core", 0, SUBCAT_HIDDEN);
        m_model->addAdvancedRows({
            option("start-time", "core", 0, SUBCAT_INPUT_GENERAL),
            option("meta-title", "core", 0, SUBCAT_INPUT_GENERAL),
            option("input-slave", "core", 0, SUBCAT_INPUT_GENERAL),
            option("dshow-vdev", "dshow", "DirectShow", SUBCAT_INPUT_ACCESS),
            option("v4l2-dev", "v4l2", "Video4Linux", SUBCAT_INPUT_ACCESS),
            option("dvb-adapter", "dtv", "DTV", SUBCAT_INPUT_ACCESS),
            option("skins2-last", "skins2", "Skins", SUBCAT_INTERFACE_MAIN),
            hidden,
            option("stats", "core", 0, SUBCAT_ADVANCED_MISC),
        });

        QStringList settingIds;
        for (int i = 0; i < m_model->rowCount(); ++i)
            if (m_model->index(i).data(SettingsModel::KindRole).toInt() == SettingsModel::Setting)
                settingIds << m_model->index(i).data(SettingsModel::IdRole).toString();
        QCOMPARE(settingIds, QStringList { "stats" });
        QCOMPARE(role("stats", SettingsModel::PageRole).toString(), QStringLiteral("general"));
    }

    void testEachOptionAppearsOnce()
    {
        // already a basic row
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));

        m_model->addAdvancedRows({
            option("mouse-hide-timeout", "core", 0, SUBCAT_VIDEO_GENERAL),
            // the same option declared by several plugins
            option("contrast", "adjust", "Image properties filter", SUBCAT_VIDEO_VFILTER),
            option("contrast", "gladjust", "OpenGL adjust filter", SUBCAT_VIDEO_VFILTER),
            option("contrast", "d3d11_filters", "Direct3D11 adjust filter", SUBCAT_VIDEO_VFILTER),
            // one concept under several names
            option("effect-width", "visual", "Visualizer", SUBCAT_AUDIO_VISUAL),
            option("goom-width", "goom", "Goom", SUBCAT_AUDIO_VISUAL),
            option("glspectrum-width", "glspectrum", "3D OpenGL spectrum", SUBCAT_AUDIO_VISUAL),
        });

        QMap<QString, int> uses;
        for (int i = 0; i < m_model->rowCount(); ++i)
            for (const QString& name : m_model->index(i).data(SettingsModel::OptionsRole).toStringList())
                ++uses[name];
        for (auto it = uses.cbegin(); it != uses.cend(); ++it)
            QVERIFY2(it.value() == 1, qPrintable(it.key()));

        QCOMPARE(role("mouse-hide-timeout", SettingsModel::KindRole), QVariant());

        // one row, in the first plugin's group, naming the others
        QCOMPARE(role("contrast", SettingsModel::GroupRole).toString(), QStringLiteral("adjust"));
        const QString help = role("contrast", SettingsModel::HelpRole).toString();
        QVERIFY(help.contains("OpenGL adjust filter"));
        QVERIFY(help.contains("Direct3D11 adjust filter"));

        // one row writing every name, under Audio
        const QModelIndex width = indexOf("visualization-width");
        QVERIFY(width.isValid());
        QCOMPARE(width.data(SettingsModel::PageRole).toString(), QStringLiteral("audio"));
        QCOMPARE(width.data(SettingsModel::OptionsRole).toStringList(),
                 (QStringList { "effect-width", "goom-width", "glspectrum-width" }));
        QVERIFY(!indexOf("goom-width").isValid());
    }

    void testRegistryRowsReadAndWriteTheOption()
    {
        m_model->addAdvancedRows({
            option("stats", "core", 0, SUBCAT_ADVANCED_MISC, CONFIG_ITEM_BOOL),
            option("mouse-hide-timeout", "core", 0, SUBCAT_VIDEO_GENERAL, CONFIG_ITEM_INTEGER),
        });

        QCOMPARE(role("stats", SettingsModel::ControlRole).toInt(), int(SettingsModel::Toggle));
        QCOMPARE(role("mouse-hide-timeout", SettingsModel::ControlRole).toInt(), int(SettingsModel::Number));
        QCOMPARE(role("stats", SettingsModel::LabelRole).toString(), QStringLiteral("stats label"));

        QVERIFY(m_model->setValue("stats", false));
        QCOMPARE(config_GetInt("stats"), 0);
        QVERIFY(m_model->setValue("mouse-hide-timeout", 2000));
        QCOMPARE(config_GetInt("mouse-hide-timeout"), 2000);
    }

    /* Search */

    void testSearchCoversAdvancedOptions()
    {
        m_model->addRow(configRow("hide", "interface", {"mouse-hide-timeout"}));
        SettingsOption top = option("video-on-top", "core", 0, SUBCAT_VIDEO_GENERAL);
        top.label = "Always on top";
        SettingsOption fs = option("fullscreen", "fake_plugin", "Fake plugin", SUBCAT_VIDEO_VFILTER);
        fs.label = "Fullscreen video output";
        m_model->addAdvancedRows({ top, fs });

        SettingsFilterModel filter;
        filter.setSourceModel(m_model.get());
        filter.setPage("interface");

        // collapsed advanced rows and plugin rows are found, headers are not
        filter.setSearchPattern("always on top");
        QCOMPARE(ids(filter), QStringList { "video-on-top" });
        filter.setSearchPattern("fullscreen");
        QCOMPARE(ids(filter), QStringList { "fullscreen" });

        // by plugin name and by option name
        filter.setSearchPattern("fake plugin");
        QCOMPARE(ids(filter), QStringList { "fullscreen" });
        filter.setSearchPattern("video-on-top");
        QCOMPARE(ids(filter), QStringList { "video-on-top" });
    }

    void testLiveRegistryCoversEveryOptionAndShortcut()
    {
        m_model->addAdvancedRows(SettingsRegistry::options());

        // an ordinary core option
        QVERIFY(indexOf("stats").isValid());
        QCOMPARE(role("stats", SettingsModel::AdvancedRole).toBool(), true);
        QCOMPARE(role("stats", SettingsModel::AvailableRole).toBool(), true);
        // command-line only
        QVERIFY(!indexOf("start-time").isValid());
        QVERIFY(!indexOf("bookmark1").isValid());

        // each option has one row
        QMap<QString, int> uses;
        for (int i = 0; i < m_model->rowCount(); ++i)
            for (const QString& name : m_model->index(i).data(SettingsModel::OptionsRole).toStringList())
                ++uses[name];
        for (auto it = uses.cbegin(); it != uses.cend(); ++it)
            QVERIFY2(it.value() == 1, qPrintable(it.key()));

        // shortcut actions are found by search only
        const QModelIndex fullscreen = indexOf("key-toggle-fullscreen");
        QVERIFY(fullscreen.isValid());
        QCOMPARE(fullscreen.data(SettingsModel::ControlRole).toInt(), int(SettingsModel::Shortcut));
        QCOMPARE(fullscreen.data(SettingsModel::SearchOnlyRole).toBool(), true);
        QVERIFY(!indexOf("global-key-toggle-fullscreen").isValid());

        SettingsFilterModel filter;
        filter.setSourceModel(m_model.get());
        filter.setPage("controls");
        m_model->setAdvancedExpanded("controls", true);
        QVERIFY(!ids(filter).contains("key-toggle-fullscreen"));

        filter.setSearchPattern(fullscreen.data(SettingsModel::LabelRole).toString());
        QVERIFY(ids(filter).contains("key-toggle-fullscreen"));
    }
};

QTEST_GUILESS_MAIN(TestSettingsModel)
#include "test_settings_model.moc"
