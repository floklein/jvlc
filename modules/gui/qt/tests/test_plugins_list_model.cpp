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
#include <QAbstractItemModelTester>
#include <QCollator>

#include "dialogs/settings/plugins_list_model.hpp"

/*
 * The test environment's plugins are the core and the stub modules
 * (an interface, an audio output, an access...).
 */
class TestPluginsListModel : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<VLCTestingEnv> m_env;

    static QStringList column(const QAbstractItemModel& model, int role)
    {
        QStringList list;
        for (int i = 0; i < model.rowCount(); ++i)
            list << model.index(i, 0).data(role).toString();
        return list;
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

    void testModelIsConsistent()
    {
        PluginsListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setSearchPattern("audio");
        model.setSearchPattern("");
    }

    void testListsEveryPluginButTheCore()
    {
        PluginsListModel model;
        QVERIFY(model.rowCount() > 0);
        QCOMPARE(model.count(), model.rowCount());

        const QStringList capabilities = column(model, PluginsListModel::CapabilityRole);
        QVERIFY(capabilities.contains("audio output"));
        QVERIFY(capabilities.contains("interface"));
        QVERIFY(!capabilities.contains("core"));
        QVERIFY(!column(model, PluginsListModel::NameRole).contains(QString()));

        // by name
        const QStringList names = column(model, PluginsListModel::NameRole);
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        for (int i = 1; i < names.count(); ++i)
            QVERIFY2(collator.compare(names.at(i - 1), names.at(i)) <= 0, qPrintable(names.at(i)));
    }

    void testSearchFiltersByNameAndCapability()
    {
        PluginsListModel model;
        const int all = model.rowCount();

        model.setSearchPattern("AUDIO OUTPUT");
        QVERIFY(model.rowCount() >= 1);
        QVERIFY(model.rowCount() < all);
        QVERIFY(column(model, PluginsListModel::CapabilityRole).contains("audio output"));
        QCOMPARE(model.count(), model.rowCount());

        model.setSearchPattern("no such plugin anywhere");
        QCOMPARE(model.rowCount(), 0);

        model.setSearchPattern("");
        QCOMPARE(model.rowCount(), all);
    }
};

QTEST_GUILESS_MAIN(TestPluginsListModel)
#include "test_plugins_list_model.moc"
