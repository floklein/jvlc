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
#ifndef ITEM_ORDER_HPP
#define ITEM_ORDER_HPP

#include <QVariant>
#include <QStringView>
#include <QString>

#include <algorithm>
#include <utility>
#include <vector>

/*
 * The order of the items a list already holds, by a key of each item: the
 * rows of Home are sorted inside the row, the row keeps its items.
 */
namespace ItemOrder
{

// natural order: "Episode 2" before "Episode 10", ignoring case; the text
// between the numbers follows the locale
inline int compareText(const QString& a, const QString& b)
{
    qsizetype i = 0, j = 0;
    while (i < a.size() && j < b.size())
    {
        if (a[i].isDigit() && b[j].isDigit())
        {
            qsizetype ie = i, je = j;
            while (ie < a.size() && a[ie].isDigit()) ++ie;
            while (je < b.size() && b[je].isDigit()) ++je;
            // compare the numbers by value: without leading zeros, the
            // longer one is the larger
            QStringView na = QStringView(a).mid(i, ie - i);
            QStringView nb = QStringView(b).mid(j, je - j);
            while (na.size() > 1 && na.front() == u'0') na = na.mid(1);
            while (nb.size() > 1 && nb.front() == u'0') nb = nb.mid(1);
            if (na.size() != nb.size())
                return na.size() < nb.size() ? -1 : 1;
            if (const int c = na.compare(nb); c != 0)
                return c < 0 ? -1 : 1;
            i = ie;
            j = je;
            continue;
        }

        qsizetype ie = i, je = j;
        while (ie < a.size() && !a[ie].isDigit()) ++ie;
        while (je < b.size() && !b[je].isDigit()) ++je;
        const QString ta = a.mid(i, ie - i).toCaseFolded();
        const QString tb = b.mid(j, je - j).toCaseFolded();
        if (const int c = QString::localeAwareCompare(ta, tb); c != 0)
            return c < 0 ? -1 : 1;
        i = ie;
        j = je;
    }
    if (i < a.size())
        return 1;
    if (j < b.size())
        return -1;
    return 0;
}

// < 0 when a comes first: text in natural order, ignoring case; numbers by
// value; an item without key last
inline int compare(const QVariant& a, const QVariant& b)
{
    const bool aValid = a.isValid() && !a.isNull();
    const bool bValid = b.isValid() && !b.isNull();
    if (!aValid || !bValid)
        return int(!aValid) - int(!bValid);

    if (a.typeId() == QMetaType::QString || b.typeId() == QMetaType::QString)
        return compareText(a.toString(), b.toString());

    bool aOk = false, bOk = false;
    const double av = a.toDouble(&aOk);
    const double bv = b.toDouble(&bOk);
    if (aOk && bOk)
        return (av < bv) ? -1 : (av > bv) ? 1 : 0;
    return 0;
}

// sorts items by key(item), stably: equal keys keep their order, and so does
// an order without key
template<typename T, typename Key>
void sort(std::vector<T>& items, Key key, Qt::SortOrder order)
{
    std::vector<std::pair<QVariant, size_t>> keys;
    keys.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i)
        keys.emplace_back(key(items[i]), i);

    std::stable_sort(keys.begin(), keys.end(), [order](const auto& x, const auto& y) {
        const int c = compare(x.first, y.first);
        // an item without key stays last in both orders
        const bool xValid = x.first.isValid() && !x.first.isNull();
        const bool yValid = y.first.isValid() && !y.first.isNull();
        if (!xValid || !yValid)
            return c < 0;
        return (order == Qt::AscendingOrder) ? (c < 0) : (c > 0);
    });

    std::vector<T> sorted;
    sorted.reserve(items.size());
    for (const auto& k : keys)
        sorted.push_back(std::move(items[k.second]));
    items = std::move(sorted);
}

}

#endif // ITEM_ORDER_HPP
