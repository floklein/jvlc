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

#include "../util/item_order.hpp"

#include <QTest>

#include <memory>

namespace {

struct Item
{
    QVariant title;
    QVariant duration;
};

using Items = std::vector<std::unique_ptr<Item>>;

Items make(const QList<std::pair<QVariant, QVariant>>& values)
{
    Items items;
    for (const auto& v : values)
        items.push_back(std::make_unique<Item>(Item{ v.first, v.second }));
    return items;
}

QStringList titles(const Items& items)
{
    QStringList out;
    for (const auto& item : items)
        out << item->title.toString();
    return out;
}

const auto byTitle = [](const std::unique_ptr<Item>& item) { return item->title; };
const auto byDuration = [](const std::unique_ptr<Item>& item) { return item->duration; };

}

// the sort of the rows of Home: the items of a row, in another order
class TestItemOrder : public QObject
{
    Q_OBJECT

private slots:
    void testNameAscendingIsNaturalAndIgnoresCase()
    {
        Items items = make({ { "the Wand 10", 1 }, { "Big Buck", 2 }, { "The Wand 2", 3 }, { "apple", 4 } });
        ItemOrder::sort(items, byTitle, Qt::AscendingOrder);
        QCOMPARE(titles(items), QStringList({ "apple", "Big Buck", "The Wand 2", "the Wand 10" }));
    }

    void testNameDescending()
    {
        Items items = make({ { "b", 1 }, { "a", 2 }, { "c", 3 } });
        ItemOrder::sort(items, byTitle, Qt::DescendingOrder);
        QCOMPARE(titles(items), QStringList({ "c", "b", "a" }));
    }

    void testDurationByValue()
    {
        Items items = make({ { "ten", 10000 }, { "nine", 9000 }, { "hundred", 100000 } });
        ItemOrder::sort(items, byDuration, Qt::DescendingOrder);
        QCOMPARE(titles(items), QStringList({ "hundred", "ten", "nine" }));
        ItemOrder::sort(items, byDuration, Qt::AscendingOrder);
        QCOMPARE(titles(items), QStringList({ "nine", "ten", "hundred" }));
    }

    // a row keeps its items: the sort only moves them
    void testKeepsEveryItem()
    {
        Items items = make({ { "c", 3 }, { "a", 1 }, { "b", 2 }, { "a", 4 } });
        ItemOrder::sort(items, byTitle, Qt::AscendingOrder);
        QCOMPARE(items.size(), size_t(4));
        // equal keys keep the order they had
        QCOMPARE(items[0]->duration.toInt(), 1);
        QCOMPARE(items[1]->duration.toInt(), 4);
    }

    // a stream without duration stays after the media that have one
    void testItemsWithoutKeyStayLast()
    {
        Items items = make({ { "stream", QVariant() }, { "short", 1 }, { "long", 9 } });
        ItemOrder::sort(items, byDuration, Qt::DescendingOrder);
        QCOMPARE(titles(items), QStringList({ "long", "short", "stream" }));
        ItemOrder::sort(items, byDuration, Qt::AscendingOrder);
        QCOMPARE(titles(items), QStringList({ "short", "long", "stream" }));
    }
};

QTEST_GUILESS_MAIN(TestItemOrder)
#include "test_item_order.moc"
