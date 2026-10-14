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

#include "../util/navigation_history.hpp"
#include "../network/networkmediamodel.hpp"

#include <QTest>
#include <QSignalSpy>

// The history checks the network nodes it keeps (NetworkTreeItem, a gadget of
// the network models); this test doesn't link these models, so the meta object
// of the gadget is a stub. No network node is pushed here.
const QMetaObject NetworkTreeItem::staticMetaObject = {};

// Back and forward through the library pages, as the header buttons do
class TestNavigationHistory : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_history = std::make_unique<NavigationHistory>();
        m_history->update({"home"});
    }

    void cleanup()
    {
        m_history.reset();
    }

    void testNothingForwardAtFirst()
    {
        m_history->push({"video"});

        QVERIFY(m_history->isNextEmpty());
        QVERIFY(!m_history->isPreviousEmpty());
    }

    void testBackThenForward()
    {
        m_history->push({"video"});
        m_history->push({"music"});

        m_history->previous();
        QCOMPARE(m_history->viewPath(), QStringList({"video"}));
        QVERIFY(!m_history->isNextEmpty());

        m_history->next();
        QCOMPARE(m_history->viewPath(), QStringList({"music"}));
        QVERIFY(m_history->isNextEmpty());
        QVERIFY(!m_history->isPreviousEmpty());
    }

    void testForwardSeveralSteps()
    {
        m_history->push({"video"});
        m_history->push({"music"});

        m_history->previous();
        m_history->previous();
        QCOMPARE(m_history->viewPath(), QStringList({"home"}));
        QVERIFY(m_history->isPreviousEmpty());

        m_history->next();
        QCOMPARE(m_history->viewPath(), QStringList({"video"}));
        m_history->next();
        QCOMPARE(m_history->viewPath(), QStringList({"music"}));
    }

    void testForwardRestoresViewProperties()
    {
        m_history->push({"video", "all"}, {{"title", "Shows"}});
        m_history->push({"video", "all"}, {{"title", "S01"}});
        m_history->viewProp()->insert("initialIndex", 3);

        m_history->previous();
        QCOMPARE(m_history->viewProp()->value("title").toString(), QStringLiteral("Shows"));

        m_history->next();
        QCOMPARE(m_history->viewProp()->value("title").toString(), QStringLiteral("S01"));
        QCOMPARE(m_history->viewProp()->value("initialIndex").toInt(), 3);
    }

    void testPushDropsForward()
    {
        m_history->push({"video"});
        m_history->push({"music"});
        m_history->previous();

        m_history->push({"recent"});

        QVERIFY(m_history->isNextEmpty());
        m_history->next();
        QCOMPARE(m_history->viewPath(), QStringList({"recent"}));
    }

    void testNextNavigates()
    {
        m_history->push({"video"});
        m_history->previous();

        QSignalSpy navigate(m_history.get(), &NavigationHistory::navigate);
        QSignalSpy nextEmpty(m_history.get(), &NavigationHistory::nextEmptyChanged);

        m_history->next();

        QCOMPARE(navigate.count(), 1);
        QCOMPARE(nextEmpty.count(), 1);
        QCOMPARE(nextEmpty.first().first().toBool(), true);
    }

    void testNextWithNothingForwardStays()
    {
        m_history->push({"video"});
        QSignalSpy navigate(m_history.get(), &NavigationHistory::navigate);

        m_history->next();

        QCOMPARE(navigate.count(), 0);
        QCOMPARE(m_history->viewPath(), QStringList({"video"}));
    }

    void testPreviousOffersForward()
    {
        m_history->push({"video"});
        QSignalSpy nextEmpty(m_history.get(), &NavigationHistory::nextEmptyChanged);

        m_history->previous();

        QCOMPARE(nextEmpty.count(), 1);
        QCOMPARE(nextEmpty.first().first().toBool(), false);
    }

private:
    std::unique_ptr<NavigationHistory> m_history;
};

QTEST_GUILESS_MAIN(TestNavigationHistory)
#include "test_navigation_history.moc"
