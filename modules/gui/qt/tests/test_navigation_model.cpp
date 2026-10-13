/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include "../maininterface/navigationmodel.hpp"
#include "vlc_stub_modules.hpp"

#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>

class TestVLCNavigationModel : public QObject
{
    Q_OBJECT

private:
    enum NodeState {
        Leaf = 0,
        Closed = 2,
        Opened = 3
    };

    bool compareRow(int row,
                    const char* title, const char* title_str,
                    NodeState state, const char*  state_str,
                    int line )
    {
        if (!QTest::qCompare(
                m_model->data(m_model->index(row), NavigationModel::TITLE), title,
                "title", title_str,
                __FILE__, line))
            return false;

        if (!QTest::qCompare(
                m_model->data(m_model->index(row), NavigationModel::EXPANDABLE), !!(state & 2),
                "expandable", state_str,
                __FILE__, line))
            return false;

        if (!QTest::qCompare(
                m_model->data(m_model->index(row), NavigationModel::EXPANDED), !!(state & 1),
                "expanded", state_str,
                __FILE__, line))
            return false;

        return true;
    }
#define COMPARE_ROW(row, title, state) do {\
    if (!compareRow(row, title, #title, state, #state, __LINE__))\
        return;\
} while (false)

#define VERIFY_NOT_INSERTED QCOMPARE(m_insertedSpy->size(), 0)
#define VERIFY_NOT_REMOVED QCOMPARE(m_removedSpy->size(), 0)

    void initWithMedialib(bool hasMedialib) {
        m_model->classBegin();
        m_model->setHasMedialib(hasMedialib);
        m_model->componentComplete();
    }

private slots:
    void initTestCase() {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
    }

    void cleanupTestCase() {
        m_env.reset();
    }

    void init() {
        m_model = std::make_unique<NavigationModel>();
        new QAbstractItemModelTester(m_model.get(), m_model.get());

        m_insertedSpy = std::make_unique<QSignalSpy>(m_model.get(), &QAbstractItemModel::rowsAboutToBeInserted);
        m_removedSpy = std::make_unique<QSignalSpy>(m_model.get(), &QAbstractItemModel::rowsAboutToBeRemoved);
    }

    void cleanup() {
        m_model.reset();
    }

    void testInitialValues() {
        initWithMedialib(true);
        QCOMPARE(m_model->rowCount(), 6);
        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Videos", Leaf);
        COMPARE_ROW(i++, "Music", Closed);
        COMPARE_ROW(i++, "Network", Leaf);
        COMPARE_ROW(i++, "Playlists", Leaf);
        COMPARE_ROW(i++, "Recent", Leaf);
    }

    void testExpandMusic() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(2), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 10);
        QCOMPARE(m_insertedSpy->size(), 1);
        const QList<QVariant> inserted = m_insertedSpy->takeFirst();
        QCOMPARE(inserted[1].toInt(), 3);
        QCOMPARE(inserted[2].toInt(), 6);
        VERIFY_NOT_REMOVED;

        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Videos", Leaf);
        COMPARE_ROW(i++, "Music", Opened);
        {
            COMPARE_ROW(i++, "Songs", Leaf);
            COMPARE_ROW(i++, "Albums", Leaf);
            COMPARE_ROW(i++, "Artists", Leaf);
            COMPARE_ROW(i++, "Genres", Leaf);
        }
        COMPARE_ROW(i++, "Network", Leaf);
        COMPARE_ROW(i++, "Playlists", Leaf);
        COMPARE_ROW(i++, "Recent", Leaf);

        QCOMPARE(m_model->data(m_model->index(3), NavigationModel::URI), (QStringList{"music", "tracks"}));
        QCOMPARE(m_model->data(m_model->index(4), NavigationModel::URI), (QStringList{"music", "albums"}));
        QCOMPARE(m_model->data(m_model->index(5), NavigationModel::URI), (QStringList{"music", "artists"}));
        QCOMPARE(m_model->data(m_model->index(6), NavigationModel::URI), (QStringList{"music", "genres"}));

        QVERIFY(m_model->setData(m_model->index(2), false, NavigationModel::EXPANDED));
        QCOMPARE(m_model->rowCount(), 6);
    }

    void testInitialValuesNoML() {
        initWithMedialib(false);
        QCOMPARE(m_model->rowCount(), 2);
        int i = 0;
        COMPARE_ROW(i++, "Home", Leaf);
        COMPARE_ROW(i++, "Network", Leaf);
    }

    void testBrowseAndDiscoverAreGone() {
        initWithMedialib(true);
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            const QString title = m_model->data(m_model->index(row), NavigationModel::TITLE).toString();
            QVERIFY2(title != "Browse", "Browse is not a library section");
            QVERIFY2(title != "Discover", "Discover is not a library section");
        }
    }

    void testURI() {
        initWithMedialib(true);
        const QList<QStringList> expected = {
            {"home"}, {"video"}, {"music"}, {"network"}, {"playlists"}, {"recent"}
        };
        QCOMPARE(m_model->rowCount(), expected.size());
        for (int row = 0; row < expected.size(); ++row)
        {
            QCOMPARE(m_model->data(m_model->index(row), NavigationModel::URI), expected[row]);
            QCOMPARE(m_model->data(m_model->index(row), NavigationModel::DEPTH), 0);
        }
    }

    void testEverySectionHasIcons() {
        initWithMedialib(true);
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            QVERIFY(!m_model->data(m_model->index(row), NavigationModel::ICON).toString().isEmpty());
            QVERIFY(!m_model->data(m_model->index(row), NavigationModel::ICON_SVG).toString().isEmpty());
        }
    }

    void testExpandLeafDoesNothing() {
        initWithMedialib(true);
        QVERIFY(m_model->setData(m_model->index(1), true, NavigationModel::EXPANDED));

        QCOMPARE(m_model->rowCount(), 6);
        VERIFY_NOT_INSERTED;
        VERIFY_NOT_REMOVED;
        COMPARE_ROW(1, "Videos", Leaf);

        QVERIFY(m_model->setData(m_model->index(1), false, NavigationModel::EXPANDED));
        QCOMPARE(m_model->rowCount(), 6);
        VERIFY_NOT_INSERTED;
        VERIFY_NOT_REMOVED;
    }

    void testExpandOOB() {
        initWithMedialib(true);
        QVERIFY(!m_model->setData(m_model->index(6), true, NavigationModel::EXPANDED));
        QVERIFY(!m_model->setData(m_model->index(6), false, NavigationModel::EXPANDED));
        QCOMPARE(m_model->rowCount(), 6);
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<NavigationModel> m_model;
    std::unique_ptr<QSignalSpy> m_insertedSpy;
    std::unique_ptr<QSignalSpy> m_removedSpy;
};


QTEST_GUILESS_MAIN(TestVLCNavigationModel)
#include "test_navigation_model.moc"
