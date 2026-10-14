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

#include "vlc_stub_modules.hpp"

#include "../medialibrary/medialib.hpp"
#include "../medialibrary/mlplaylistmanager.hpp"
#include "../medialibrary/mlplaylistlistmodel.hpp"
#include "../medialibrary/mlplaylistmodel.hpp"
#include "../playlist/playlist_controller.hpp"

#include <algorithm>
#include <memory>

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using vlc::playlist::PlaylistController;
using vlc::playlist::vlc_playlist_locker;

static QVariant mediaId(int64_t id)
{
    return QVariant::fromValue(MLItemId{id, VLC_ML_PARENT_UNKNOWN});
}

static QVariant playlistId(int64_t id)
{
    return QVariant::fromValue(MLItemId{id, VLC_ML_PARENT_PLAYLIST});
}

class TestMLPlaylists : public QObject
{
    Q_OBJECT

private:
    vlc_playlist_t *queue() const
    {
        return m_env->intf->p_playlist;
    }

    /* The Queue as the core sees it: the names of its items */
    QStringList queueNames() const
    {
        QStringList names;
        vlc_playlist_locker locker(queue());
        for (size_t i = 0; i < vlc_playlist_Count(queue()); ++i)
            names.push_back(qfu(vlc_playlist_item_GetMedia(vlc_playlist_Get(queue(), i))->psz_name));
        return names;
    }

    void clearQueue()
    {
        vlc_playlist_locker locker(queue());
        vlc_playlist_Clear(queue());
    }

    /* The media ids of the stub Playlist named `name` */
    std::vector<int64_t> playlistMedia(const std::string &name) const
    {
        for (const auto &playlist : m_env->playlists())
            if (playlist.name == name)
                return playlist.media;
        return {};
    }

    bool hasPlaylist(const std::string &name) const
    {
        for (const auto &playlist : m_env->playlists())
            if (playlist.name == name)
                return true;
        return false;
    }

    QStringList recentNames(const MLPlaylistManager &manager) const
    {
        QStringList names;
        for (const QVariant &entry : manager.recentPlaylists())
            names.push_back(entry.toMap().value("name").toString());
        return names;
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
        m_env->clearMediaLibrary();
        m_controller = std::make_unique<PlaylistController>(queue());
        QTRY_VERIFY(m_controller->isInitialized());
        m_medialib = std::make_unique<MediaLib>(m_env->intf, m_controller.get());
        m_thumbnails = std::make_unique<QTemporaryDir>();
        QVERIFY(m_thumbnails->isValid());
        m_medialib->playlistManager()->setThumbnailDir(m_thumbnails->path());
    }

    void cleanup()
    {
        m_medialib.reset();
        m_thumbnails.reset();
        clearQueue();
        m_controller.reset();
    }

    // New Playlist

    void testCreateWithoutItems()
    {
        MLPlaylistManager *manager = m_medialib->playlistManager();
        QSignalSpy created(manager, &MLPlaylistManager::created);

        manager->create(QStringLiteral("Empty"));

        QTRY_COMPARE(created.count(), 1);
        QCOMPARE(created.at(0).at(1).toString(), QStringLiteral("Empty"));
        QVERIFY(hasPlaylist("Empty"));
        QVERIFY(playlistMedia("Empty").empty());
    }

    void testCreateWithItems()
    {
        MLPlaylistManager *manager = m_medialib->playlistManager();
        QSignalSpy created(manager, &MLPlaylistManager::created);

        // library media and a file from the picker, in this order
        manager->create(QStringLiteral("Mix"),
                        { mediaId(3), mediaId(5), QUrl(QStringLiteral("vlc://pause:601")) });

        QTRY_COMPARE(created.count(), 1);
        const std::vector<int64_t> media = playlistMedia("Mix");
        QCOMPARE(media.size(), size_t(3));
        QCOMPARE(media[0], int64_t(3));
        QCOMPARE(media[1], int64_t(5));
        QVERIFY(media[2] >= 1000); // a new external media
    }

    void testCreateWithThumbnail()
    {
        QTemporaryDir source;
        const QString image = source.filePath(QStringLiteral("cover-in.png"));
        QImage cover(64, 32, QImage::Format_RGB32);
        cover.fill(Qt::red);
        QVERIFY(cover.save(image, "PNG"));

        MLPlaylistManager *manager = m_medialib->playlistManager();
        QSignalSpy created(manager, &MLPlaylistManager::created);

        manager->create(QStringLiteral("Covered"), {}, QUrl::fromLocalFile(image));

        QTRY_COMPARE(created.count(), 1);
        const MLItemId id = created.at(0).at(0).value<MLItemId>();
        const QString thumbnail = manager->thumbnailPath(id.id);
        QTRY_VERIFY(QFile::exists(thumbnail));
        QVERIFY(!QImage(thumbnail).isNull());

        // the Playlist list shows it instead of the generated mosaic
        MLPlaylistListModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.componentComplete();
        QTRY_COMPARE(model.getCount(), 1u);
        QTRY_COMPARE(model.data(model.index(0), MLPlaylistListModel::PLAYLIST_THUMBNAIL).toString(),
                     QUrl::fromLocalFile(thumbnail).toString());
    }

    void testDeleteRemovesThumbnail()
    {
        QTemporaryDir source;
        const QString image = source.filePath(QStringLiteral("cover.png"));
        QImage cover(16, 16, QImage::Format_RGB32);
        cover.fill(Qt::blue);
        QVERIFY(cover.save(image, "PNG"));

        MLPlaylistManager *manager = m_medialib->playlistManager();
        QSignalSpy created(manager, &MLPlaylistManager::created);
        manager->create(QStringLiteral("Gone"), {}, QUrl::fromLocalFile(image));
        QTRY_COMPARE(created.count(), 1);
        const MLItemId id = created.at(0).at(0).value<MLItemId>();
        QTRY_VERIFY(QFile::exists(manager->thumbnailPath(id.id)));

        MLPlaylistListModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.componentComplete();
        QTRY_COMPARE(model.getCount(), 1u);
        model.deletePlaylists({ QVariant::fromValue(id) });

        QTRY_VERIFY(!hasPlaylist("Gone"));
        QTRY_VERIFY(!QFile::exists(manager->thumbnailPath(id.id)));
    }

    // Add to Playlist

    void testAppendAddsAtTheEndAndConfirms()
    {
        const int64_t id = m_env->addPlaylist("Road trip", {1, 2});
        MLPlaylistManager *manager = m_medialib->playlistManager();
        QSignalSpy appended(manager, &MLPlaylistManager::appended);

        manager->append(MLItemId{id, VLC_ML_PARENT_PLAYLIST}, { mediaId(7), mediaId(8) });

        QTRY_COMPARE(appended.count(), 1);
        QCOMPARE(appended.at(0).at(1).toString(), QStringLiteral("Road trip"));
        QCOMPARE(playlistMedia("Road trip"), std::vector<int64_t>({1, 2, 7, 8}));
    }

    void testRecentPlaylistsMostRecentlyChangedFirst()
    {
        MLPlaylistManager *manager = m_medialib->playlistManager();
        const QStringList all = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L"};
        int64_t idC = 0;
        for (const QString &name : all)
        {
            const int64_t id = m_env->addPlaylist(qtu(name), {});
            if (name == "C")
                idC = id;
        }

        QTRY_COMPARE(recentNames(*manager),
                     QStringList({"L", "K", "J", "I", "H", "G", "F", "E", "D", "C"}));

        manager->append(MLItemId{idC, VLC_ML_PARENT_PLAYLIST}, { mediaId(1) });

        QTRY_COMPARE(recentNames(*manager),
                     QStringList({"C", "L", "K", "J", "I", "H", "G", "F", "E", "D"}));
    }

    void testRecentPlaylistsPersist()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);

        const int64_t idA = m_env->addPlaylist("A", {});
        m_env->addPlaylist("B", {});
        {
            MLPlaylistManager manager(m_medialib.get(), m_controller.get(), &settings);
            QTRY_COMPARE(recentNames(manager), QStringList({"B", "A"}));
            manager.append(MLItemId{idA, VLC_ML_PARENT_PLAYLIST}, { mediaId(1) });
            QTRY_COMPARE(recentNames(manager), QStringList({"A", "B"}));
        }

        // a new session remembers A was changed last
        MLPlaylistManager manager(m_medialib.get(), m_controller.get(), &settings);
        QTRY_COMPARE(recentNames(manager), QStringList({"A", "B"}));
    }

    // The Queue gets a copy

    void testPlayCopiesPlaylistIntoQueue()
    {
        const int64_t id = m_env->addPlaylist("Series", {1, 2, 3});
        m_controller->append(QVariantList{QStringLiteral("vlc://pause:600")});

        m_medialib->addAndPlay(QVariantList{playlistId(id)});

        QTRY_COMPARE(queueNames(), QStringList({"ml:1", "ml:2", "ml:3"}));
        QVERIFY(m_controller->canUndoReplace());

        // editing the Queue leaves the Playlist alone
        {
            vlc_playlist_locker locker(queue());
            vlc_playlist_Remove(queue(), 0, 1);
        }
        QCOMPARE(queueNames(), QStringList({"ml:2", "ml:3"}));
        QCOMPARE(playlistMedia("Series"), std::vector<int64_t>({1, 2, 3}));

        m_controller->undoReplace();
        QCOMPARE(queueNames().size(), 1);
    }

    void testShuffleCopiesPlaylistShuffled()
    {
        const std::vector<int64_t> ids = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        const int64_t id = m_env->addPlaylist("Mixtape", ids);
        m_controller->append(QVariantList{QStringLiteral("vlc://pause:600")});

        m_medialib->shuffleAndPlay(QVariantList{playlistId(id)});

        QTRY_COMPARE(queueNames().size(), 10);
        QStringList sorted = queueNames();
        std::sort(sorted.begin(), sorted.end());
        QStringList expected;
        for (int64_t i : ids)
            expected.push_back(QStringLiteral("ml:%1").arg(i));
        std::sort(expected.begin(), expected.end());
        QCOMPARE(sorted, expected);
        QVERIFY(m_controller->canUndoReplace());
        QCOMPARE(playlistMedia("Mixtape"), ids);
    }

    void testAddToQueueAppendsCopy()
    {
        const int64_t id = m_env->addPlaylist("Later", {4, 5});
        m_controller->append(QVariantList{QStringLiteral("vlc://pause:600")});
        const QString first = queueNames().at(0);

        m_medialib->addToPlaylist(QVariantList{playlistId(id)});

        QTRY_COMPARE(queueNames(), QStringList({first, "ml:4", "ml:5"}));
        QVERIFY(!m_controller->canUndoReplace());
        QCOMPARE(playlistMedia("Later"), std::vector<int64_t>({4, 5}));
    }

    void testSaveQueueAsPlaylistOffersQueueItems()
    {
        m_controller->append(QVariantList{QStringLiteral("vlc://pause:600"),
                                          QStringLiteral("vlc://pause:601")});
        MLPlaylistManager *manager = m_medialib->playlistManager();
        QSignalSpy requested(manager, &MLPlaylistManager::newPlaylistRequested);

        manager->saveQueueAsPlaylist();

        QCOMPARE(requested.count(), 1);
        const QVariantList items = requested.at(0).at(0).toList();
        QCOMPARE(items.size(), 2);
        QCOMPARE(items.at(0).toUrl(), QUrl(QStringLiteral("vlc://pause:600")));
        QCOMPARE(items.at(1).toUrl(), QUrl(QStringLiteral("vlc://pause:601")));
    }

    // Inside a Playlist

    void testRemoveInsidePlaylist()
    {
        const int64_t id = m_env->addPlaylist("Edit", {1, 2, 3, 4});
        MLPlaylistModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.setParentId(MLItemId{id, VLC_ML_PARENT_PLAYLIST});
        model.componentComplete();
        QTRY_COMPARE(model.getCount(), 4u);

        model.remove({ model.index(1), model.index(2) });

        QTRY_COMPARE(playlistMedia("Edit"), std::vector<int64_t>({1, 4}));
    }

    void testReorderInsidePlaylist()
    {
        const int64_t id = m_env->addPlaylist("Order", {1, 2, 3, 4});
        MLPlaylistModel model;
        model.classBegin();
        model.setMl(m_medialib.get());
        model.setParentId(MLItemId{id, VLC_ML_PARENT_PLAYLIST});
        model.componentComplete();
        QTRY_COMPARE(model.getCount(), 4u);

        // drag the first item below the third one
        model.move({ model.index(0) }, 3);

        QTRY_COMPARE(playlistMedia("Order"), std::vector<int64_t>({2, 3, 1, 4}));
    }

    // Export

    void testExportM3U8()
    {
        const int64_t id = m_env->addPlaylist("Shared", {});
        MLPlaylistManager *manager = m_medialib->playlistManager();
        QSignalSpy appended(manager, &MLPlaylistManager::appended);
        manager->append(MLItemId{id, VLC_ML_PARENT_PLAYLIST},
                        { QUrl(QStringLiteral("file:///music/a%20b.mp3")), mediaId(2) });
        QTRY_COMPARE(appended.count(), 1);

        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("shared.m3u8"));
        QSignalSpy exported(manager, &MLPlaylistManager::exported);

        manager->exportPlaylist(MLItemId{id, VLC_ML_PARENT_PLAYLIST}, QUrl::fromLocalFile(path));

        QTRY_COMPARE(exported.count(), 1);
        QVERIFY(exported.at(0).at(0).toBool());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.value(0), QStringLiteral("#EXTM3U"));
        QVERIFY(lines.value(1).startsWith(QStringLiteral("#EXTINF:")));
        QVERIFY(lines.value(1).endsWith(QStringLiteral(",ml:1000")));
        QCOMPARE(lines.value(2), QStringLiteral("file:///music/a%20b.mp3"));
        QVERIFY(lines.value(3).endsWith(QStringLiteral(",ml:2")));
        QCOMPARE(lines.value(4), QStringLiteral("vlc://pause:600"));
    }

    void testExportXSPF()
    {
        const int64_t id = m_env->addPlaylist("Shared <xml>", {1, 2});
        MLPlaylistManager *manager = m_medialib->playlistManager();

        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("shared.xspf"));
        QSignalSpy exported(manager, &MLPlaylistManager::exported);

        manager->exportPlaylist(MLItemId{id, VLC_ML_PARENT_PLAYLIST}, QUrl::fromLocalFile(path));

        QTRY_COMPARE(exported.count(), 1);
        QVERIFY(exported.at(0).at(0).toBool());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString xml = QString::fromUtf8(file.readAll());
        QVERIFY(xml.contains(QStringLiteral("<playlist xmlns=\"http://xspf.org/ns/0/\" version=\"1\">")));
        QVERIFY(xml.contains(QStringLiteral("<title>Shared &lt;xml&gt;</title>")));
        QCOMPARE(xml.count(QStringLiteral("<track>")), 2);
        QVERIFY(xml.indexOf(QStringLiteral("<title>ml:1</title>"))
                < xml.indexOf(QStringLiteral("<title>ml:2</title>")));
        QVERIFY(xml.contains(QStringLiteral("<location>vlc://pause:600</location>")));
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
    std::unique_ptr<PlaylistController> m_controller;
    std::unique_ptr<MediaLib> m_medialib;
    std::unique_ptr<QTemporaryDir> m_thumbnails;
};

QTEST_GUILESS_MAIN(TestMLPlaylists)
#include "test_ml_playlists.moc"
