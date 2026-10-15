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

#include "../maininterface/pinsmodel.hpp"
#include "../medialibrary/mlqmltypes.hpp"

#include <memory>

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

// The Pins of the library sidebar: an ordered list of folders, Playlists and
// streams, kept in the Qt settings apart from the media library Favorites
class TestPinsModel : public QObject
{
    Q_OBJECT

private:
    QStringList labels(const PinsModel &model) const
    {
        QStringList out;
        for (int row = 0; row < model.rowCount(); ++row)
            out.push_back(model.data(model.index(row), PinsModel::LABEL).toString());
        return out;
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<QSettings>(m_dir->filePath(QStringLiteral("settings.ini")),
                                                 QSettings::IniFormat);
        m_model = std::make_unique<PinsModel>(m_settings.get());
    }

    void cleanup()
    {
        m_model.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void testPinsKeepTheOrderTheyWereAddedIn()
    {
        QVERIFY(m_model->pin(PinsModel::FOLDER, QStringLiteral("12"), QStringLiteral("Shows")));
        QVERIFY(m_model->pin(PinsModel::PLAYLIST, QStringLiteral("3"), QStringLiteral("Road trip")));
        QVERIFY(m_model->pin(PinsModel::STREAM, QStringLiteral("{a1}"), QStringLiteral("Radio")));

        QCOMPARE(labels(*m_model), QStringList({"Shows", "Road trip", "Radio"}));
        QCOMPARE(m_model->data(m_model->index(1), PinsModel::TYPE).toInt(), int(PinsModel::PLAYLIST));
        QCOMPARE(m_model->data(m_model->index(1), PinsModel::KEY).toString(), QStringLiteral("3"));
    }

    void testPinningTwiceKeepsOnePin()
    {
        QVERIFY(m_model->pin(PinsModel::FOLDER, QStringLiteral("12"), QStringLiteral("Shows")));
        QVERIFY(!m_model->pin(PinsModel::FOLDER, QStringLiteral("12"), QStringLiteral("Shows")));

        QCOMPARE(m_model->rowCount(), 1);
    }

    void testSameKeyOfAnotherTypeIsAnotherPin()
    {
        QVERIFY(m_model->pin(PinsModel::FOLDER, QStringLiteral("4"), QStringLiteral("Movies")));
        QVERIFY(m_model->pin(PinsModel::PLAYLIST, QStringLiteral("4"), QStringLiteral("Party")));

        QVERIFY(m_model->isPinned(PinsModel::FOLDER, QStringLiteral("4")));
        QVERIFY(m_model->isPinned(PinsModel::PLAYLIST, QStringLiteral("4")));
        QVERIFY(!m_model->isPinned(PinsModel::STREAM, QStringLiteral("4")));
    }

    // drag, or Alt+Up / Alt+Down
    void testMoveReordersPins()
    {
        m_model->pin(PinsModel::FOLDER, QStringLiteral("1"), QStringLiteral("A"));
        m_model->pin(PinsModel::FOLDER, QStringLiteral("2"), QStringLiteral("B"));
        m_model->pin(PinsModel::FOLDER, QStringLiteral("3"), QStringLiteral("C"));

        QVERIFY(m_model->move(0, 2));
        QCOMPARE(labels(*m_model), QStringList({"B", "C", "A"}));

        QVERIFY(m_model->move(2, 1));
        QCOMPARE(labels(*m_model), QStringList({"B", "A", "C"}));
    }

    void testMoveOutOfTheListIsRefused()
    {
        m_model->pin(PinsModel::FOLDER, QStringLiteral("1"), QStringLiteral("A"));
        m_model->pin(PinsModel::FOLDER, QStringLiteral("2"), QStringLiteral("B"));

        // Alt+Up on the first pin, Alt+Down on the last one
        QVERIFY(!m_model->move(0, -1));
        QVERIFY(!m_model->move(1, 2));
        QVERIFY(!m_model->move(1, 1));
        QCOMPARE(labels(*m_model), QStringList({"A", "B"}));
    }

    void testUnpinRemovesThePin()
    {
        m_model->pin(PinsModel::FOLDER, QStringLiteral("1"), QStringLiteral("A"));
        m_model->pin(PinsModel::STREAM, QStringLiteral("{s}"), QStringLiteral("B"));
        m_model->pin(PinsModel::PLAYLIST, QStringLiteral("1"), QStringLiteral("C"));

        QVERIFY(m_model->unpin(PinsModel::STREAM, QStringLiteral("{s}")));
        QVERIFY(!m_model->unpin(PinsModel::STREAM, QStringLiteral("{s}")));
        QVERIFY(!m_model->isPinned(PinsModel::STREAM, QStringLiteral("{s}")));
        QCOMPARE(labels(*m_model), QStringList({"A", "C"}));

        QVERIFY(m_model->remove(0));
        QCOMPARE(labels(*m_model), QStringList({"C"}));
    }

    void testRenameChangesTheLabel()
    {
        m_model->pin(PinsModel::PLAYLIST, QStringLiteral("7"), QStringLiteral("Old"));

        QVERIFY(m_model->rename(PinsModel::PLAYLIST, QStringLiteral("7"), QStringLiteral("New")));
        QVERIFY(!m_model->rename(PinsModel::FOLDER, QStringLiteral("7"), QStringLiteral("Other")));

        QCOMPARE(labels(*m_model), QStringList({"New"}));
    }

    // what opens a folder or a Playlist page
    void testLibraryPinsGiveTheirMediaLibraryId()
    {
        const MLItemId folder{42, VLC_ML_PARENT_FOLDER};
        const MLItemId playlist{7, VLC_ML_PARENT_PLAYLIST};
        m_model->pin(PinsModel::FOLDER, folder.toString(), QStringLiteral("Shows"));
        m_model->pin(PinsModel::PLAYLIST, playlist.toString(), QStringLiteral("Mix"));
        m_model->pin(PinsModel::STREAM, QStringLiteral("{s}"), QStringLiteral("Radio"));

        QCOMPARE(m_model->data(m_model->index(0), PinsModel::ML_ID).value<MLItemId>(), folder);
        QCOMPARE(m_model->data(m_model->index(1), PinsModel::ML_ID).value<MLItemId>(), playlist);
        QVERIFY(!m_model->data(m_model->index(2), PinsModel::ML_ID).isValid());
    }

    void testPinsAreRememberedInTheirOrder()
    {
        m_model->pin(PinsModel::FOLDER, QStringLiteral("1"), QStringLiteral("A"));
        m_model->pin(PinsModel::STREAM, QStringLiteral("{s}"), QStringLiteral("B"));
        m_model->pin(PinsModel::PLAYLIST, QStringLiteral("2"), QStringLiteral("C"));
        m_model->move(2, 0);
        m_model->unpin(PinsModel::FOLDER, QStringLiteral("1"));

        // the next session
        m_model.reset();
        m_model = std::make_unique<PinsModel>(m_settings.get());

        QCOMPARE(labels(*m_model), QStringList({"C", "B"}));
        QVERIFY(m_model->isPinned(PinsModel::PLAYLIST, QStringLiteral("2")));
        QVERIFY(m_model->isPinned(PinsModel::STREAM, QStringLiteral("{s}")));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<PinsModel> m_model;
};

QTEST_GUILESS_MAIN(TestPinsModel)
#include "test_pins_model.moc"
