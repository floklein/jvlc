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

#include "../player/presets_model.hpp"
#include "../player/preset_controller.hpp"

#include <memory>

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

// The sidebar and the player as the Presets see them: live values over
// Profile values
class FakeTarget : public PresetTarget
{
    Q_OBJECT

public:
    QVariantMap values() const override { return live; }
    QVariantMap profileValues() const override { return profile; }
    QVariantMap fileValues() const override { return file; }

    void apply(const QVariantMap& values) override
    {
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            live.insert(it.key(), it.value());
        applied.append(values);
    }

    void setProfileWrites(bool enabled) override { profileWrites = enabled; }

    // the viewer changes something in the sidebar
    void userSets(const QString& key, const QVariant& value)
    {
        live.insert(key, value);
        if (profileWrites)
            profile.insert(key, value);
        emit changedByUser({ { key, value } });
    }

    // a new file starts: its own per-file values
    void newFile()
    {
        live.insert("sub-delay", 0);
        live.insert("audio-delay", 0);
    }

    QVariantMap live;
    QVariantMap profile;
    QVariantMap file;
    QList<QVariantMap> applied;
    bool profileWrites = true;
};

namespace {

QString episode(const QString& path)
{
    return QUrl::fromLocalFile(path).toString();
}

const QVariantMap PROFILE = {
    { "sub-language", "en" },
    { "sub-relative-size", 2 },
    { "video-hue", 0. },
    { "rate", 1. },
    { "sub-delay", 0 },
    { "audio-delay", 0 },
};

} // namespace

// Presets and Shows: named sets of adjustments attached to folders, kept in
// the Qt settings outside the Profiles
class TestPresets : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<PresetsModel> reopen()
    {
        m_model.reset();
        m_settings = std::make_unique<QSettings>(m_dir->filePath(QStringLiteral("settings.ini")),
                                                 QSettings::IniFormat);
        return std::make_unique<PresetsModel>(m_settings.get());
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<QSettings>(m_dir->filePath(QStringLiteral("settings.ini")),
                                                 QSettings::IniFormat);
        m_model = std::make_unique<PresetsModel>(m_settings.get());
    }

    void cleanup()
    {
        m_controller.reset();
        m_target.reset();
        m_model.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void testPresetsAndTheirFoldersAreKeptInTheSettings()
    {
        const QString id = m_model->create(QStringLiteral("Anime"),
                                           { { "sub-language", "en" }, { "rate", 1.25 } });
        m_model->attach(QStringLiteral("/shows/The Wand"), id);

        m_model = reopen();

        QCOMPARE(m_model->count(), 1);
        QCOMPARE(m_model->name(id), QStringLiteral("Anime"));
        QCOMPARE(m_model->values(id).value("sub-language").toString(), QStringLiteral("en"));
        QCOMPARE(m_model->values(id).value("rate").toDouble(), 1.25);
        QCOMPARE(m_model->folders(id), QStringList{ QStringLiteral("/shows/The Wand") });
        // outside the Profiles' groups
        QVERIFY(m_settings->childGroups().contains(QStringLiteral("Presets")));
    }

    void testTheNearestFolderWithAPresetWins()
    {
        const QString show = m_model->create(QStringLiteral("Show"));
        const QString extras = m_model->create(QStringLiteral("Extras"));
        m_model->attach(QStringLiteral("/shows/The Wand"), show);
        m_model->attach(QStringLiteral("/shows/The Wand/S02"), extras);

        // a season inherits from its Show
        PresetsModel::Resolution season = m_model->resolve(QStringLiteral("/shows/The Wand/S01"));
        QCOMPARE(season.id, show);
        QCOMPARE(season.folder, QStringLiteral("/shows/The Wand"));

        // unless it has its own, which its subfolders inherit
        PresetsModel::Resolution bonus = m_model->resolve(QStringLiteral("/shows/The Wand/S02/Bonus"));
        QCOMPARE(bonus.id, extras);
        QCOMPARE(bonus.folder, QStringLiteral("/shows/The Wand/S02"));

        // a sibling folder whose name starts the same is not a subfolder
        QVERIFY(m_model->resolve(QStringLiteral("/shows/The Wanderer")).id.isEmpty());
        QVERIFY(m_model->resolve(QStringLiteral("/movies")).folder.isEmpty());
    }

    void testDefaultOnASubfolderStopsTheShowsPreset()
    {
        const QString show = m_model->create(QStringLiteral("Show"));
        m_model->attach(QStringLiteral("/shows/The Wand"), show);
        m_model->attach(QStringLiteral("/shows/The Wand/Specials"), QString());

        PresetsModel::Resolution specials = m_model->resolve(QStringLiteral("/shows/The Wand/Specials/Xmas"));
        QVERIFY(specials.id.isEmpty());
        QCOMPARE(specials.folder, QStringLiteral("/shows/The Wand/Specials"));

        // removing its own link makes it follow the Show again
        m_model->detach(QStringLiteral("/shows/The Wand/Specials"));
        QCOMPARE(m_model->resolve(QStringLiteral("/shows/The Wand/Specials")).id, show);
    }

    void testFolderPathsAreComparedAsTheFileSystemDoes()
    {
        const QString show = m_model->create(QStringLiteral("Show"));
        m_model->attach(QStringLiteral("/shows/The Wand/"), show);

        QCOMPARE(m_model->folders(show), QStringList{ QStringLiteral("/shows/The Wand") });
        QCOMPARE(m_model->resolve(QStringLiteral("/shows/./The Wand/S01")).id, show);
#ifdef _WIN32
        m_model->attach(QStringLiteral("C:\\Videos\\Anime"), show);
        QCOMPARE(m_model->resolve(QStringLiteral("c:/videos/anime/S01")).id, show);
        QCOMPARE(PresetsModel::normalizeFolder(QStringLiteral("C:\\")), QStringLiteral("C:/"));
#endif
    }

    void testOnePresetServesSeveralShows()
    {
        const QString anime = m_model->create(QStringLiteral("Anime"));
        m_model->attach(QStringLiteral("/shows/The Wand"), anime);
        m_model->attach(QStringLiteral("/shows/Moon Garden"), anime);

        QCOMPARE(m_model->folders(anime).size(), 2);
        QCOMPARE(m_model->data(m_model->index(0), PresetsModel::FOLDERS).toStringList().size(), 2);
        QCOMPARE(m_model->resolve(QStringLiteral("/shows/Moon Garden/S03")).id, anime);
    }

    void testDeletingAPresetDetachesItEverywhereUntilUndo()
    {
        const QString kept = m_model->create(QStringLiteral("Kept"));
        const QString anime = m_model->create(QStringLiteral("Anime"), { { "rate", 1.5 } });
        m_model->attach(QStringLiteral("/shows/The Wand"), anime);
        m_model->attach(QStringLiteral("/shows/Moon Garden"), anime);
        m_model->attach(QStringLiteral("/shows/Docs"), kept);

        QSignalSpy links(m_model.get(), &PresetsModel::linksChanged);
        QVERIFY(m_model->remove(anime));

        QCOMPARE(m_model->count(), 1);
        QVERIFY(m_model->resolve(QStringLiteral("/shows/The Wand/S01")).id.isEmpty());
        QCOMPARE(m_model->resolve(QStringLiteral("/shows/Docs")).id, kept);
        QVERIFY(m_model->canUndoRemove());
        QCOMPARE(m_model->removedName(), QStringLiteral("Anime"));
        QVERIFY(links.count() >= 1);

        m_model->undoRemove();

        QVERIFY(!m_model->canUndoRemove());
        QCOMPARE(m_model->count(), 2);
        QCOMPARE(m_model->data(m_model->index(1), PresetsModel::NAME).toString(), QStringLiteral("Anime"));
        QCOMPARE(m_model->values(anime).value("rate").toDouble(), 1.5);
        QCOMPARE(m_model->resolve(QStringLiteral("/shows/The Wand/S01")).id, anime);
        QCOMPARE(m_model->folders(anime).size(), 2);
    }

    void testRenameAndDuplicateKeepNamesUnique()
    {
        const QString anime = m_model->create(QStringLiteral("Anime"), { { "sub-language", "ja" } });
        m_model->attach(QStringLiteral("/shows/The Wand"), anime);
        const QString docs = m_model->create(QStringLiteral("Docs"));

        QVERIFY(!m_model->isNameAvailable(QStringLiteral("anime")));
        QVERIFY(m_model->isNameAvailable(QStringLiteral("anime"), anime));
        QVERIFY(!m_model->rename(docs, QStringLiteral(" ANIME ")));
        QVERIFY(m_model->rename(docs, QStringLiteral("Documentaries")));
        QCOMPARE(m_model->name(docs), QStringLiteral("Documentaries"));

        const QString copy = m_model->duplicate(anime);
        const QString secondCopy = m_model->duplicate(anime);
        QCOMPARE(m_model->name(copy), QStringLiteral("Anime Copy"));
        QCOMPARE(m_model->name(secondCopy), QStringLiteral("Anime Copy 2"));
        // the values, not the Shows
        QCOMPARE(m_model->values(copy), m_model->values(anime));
        QVERIFY(m_model->folders(copy).isEmpty());
    }

    void testValuesAreSparse()
    {
        const QString id = m_model->create(QStringLiteral("Anime"));
        m_model->setValue(id, QStringLiteral("sub-delay"), 300);
        m_model->setValue(id, QStringLiteral("rate"), 1.25);
        m_model->clearValue(id, QStringLiteral("rate"));

        m_model = reopen();
        QCOMPARE(m_model->values(id).keys(), QStringList{ QStringLiteral("sub-delay") });
        QCOMPARE(m_model->values(id).value("sub-delay").toInt(), 300);
    }

    // the Preset of the playing file

    void testPrecedenceIsProfileThenPresetThenTheFile()
    {
        const QVariantMap effective = PresetController::layered(
            { { "sub-language", "en" }, { "rate", 1. }, { "video-hue", 0. } },
            { { "sub-language", "ja" }, { "rate", 1.5 } },
            { { "sub-language", "fr" } });

        QCOMPARE(effective.value("video-hue").toDouble(), 0.);
        QCOMPARE(effective.value("rate").toDouble(), 1.5);
        QCOMPARE(effective.value("sub-language").toString(), QStringLiteral("fr"));
    }

    void testWithoutAPresetTheProfileIsShownAndWritten()
    {
        startController();
        m_controller->setMedia(episode("/movies/Heat.mkv"));

        QVERIFY(m_controller->activeId().isEmpty());
        QCOMPARE(m_controller->folder(), QStringLiteral("/movies"));
        QVERIFY(m_target->profileWrites);
        QCOMPARE(m_target->live, PROFILE);

        // changes go to the Profile, no Preset records them
        m_target->userSets("sub-relative-size", 3);
        QCOMPARE(m_target->profile.value("sub-relative-size").toInt(), 3);
    }

    void testAPresetOverridesOnlyItsOwnValues()
    {
        const QString anime = m_model->create("Anime", { { "sub-language", "ja" }, { "rate", 1.25 } });
        m_model->attach("/shows/The Wand", anime);
        startController();

        m_controller->setMedia(episode("/shows/The Wand/S01/e01.mkv"));

        QCOMPARE(m_controller->activeId(), anime);
        QCOMPARE(m_controller->activeName(), QStringLiteral("Anime"));
        QCOMPARE(m_controller->activeFolder(), QStringLiteral("/shows/The Wand"));
        QVERIFY(!m_target->profileWrites);
        QCOMPARE(m_target->live.value("sub-language").toString(), QStringLiteral("ja"));
        QCOMPARE(m_target->live.value("rate").toDouble(), 1.25);
        // the rest keeps the Profile's values
        QCOMPARE(m_target->live.value("sub-relative-size").toInt(), 2);
        QCOMPARE(m_target->live.value("video-hue").toDouble(), 0.);
        QCOMPARE(m_controller->overriddenKeys().size(), 2);
        QVERIFY(m_controller->isOverridden({ "video-hue", "rate" }));
        QVERIFY(!m_controller->isOverridden({ "video-hue" }));
    }

    void testTheFilesOwnChoicesWinOverThePreset()
    {
        const QString anime = m_model->create("Anime", { { "sub-language", "ja" }, { "rate", 1.25 } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_target->file = { { "sub-language", "fr" } };

        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));

        QCOMPARE(m_target->live.value("sub-language").toString(), QStringLiteral("fr"));
        QCOMPARE(m_target->live.value("rate").toDouble(), 1.25);
    }

    void testChangesWhileAPresetIsActiveGoIntoItSparsely()
    {
        const QString anime = m_model->create("Anime", { { "rate", 1.25 } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));
        QSignalSpy overridden(m_controller.get(), &PresetController::overriddenKeysChanged);

        m_target->userSets("video-hue", 120.);
        m_target->userSets("rate", 1.5);
        // the same as the Profile: nothing to keep
        m_target->userSets("sub-relative-size", 2);

        QCOMPARE(m_model->values(anime), QVariantMap({ { "rate", 1.5 }, { "video-hue", 120. } }));
        // the Profile is untouched
        QCOMPARE(m_target->profile, PROFILE);
        QVERIFY(overridden.count() >= 1);
        QVERIFY(m_controller->isOverridden({ "video-hue" }));
    }

    void testLeavingTheShowPutsTheProfileBack()
    {
        const QString anime = m_model->create("Anime", { { "sub-language", "ja" }, { "sub-delay", 300 } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));
        m_target->userSets("video-hue", 120.);
        QCOMPARE(m_target->live.value("sub-delay").toInt(), 300);

        m_target->newFile();
        m_controller->setMedia(episode("/movies/Heat.mkv"));

        QVERIFY(m_controller->activeId().isEmpty());
        QVERIFY(m_target->profileWrites);
        QCOMPARE(m_target->live, PROFILE);
    }

    void testEveryEpisodeOfTheShowStartsWithThePreset()
    {
        const QString anime = m_model->create("Anime", { { "sub-delay", 300 }, { "rate", 1.25 } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/S01/e01.mkv"));

        // the next file starts with its own delay, though the shown one
        // may not be updated yet
        m_target->applied.clear();
        m_controller->setMedia(episode("/shows/The Wand/S02/e01.mkv"));
        QCOMPARE(m_target->applied.size(), 1);
        QCOMPARE(m_target->applied.first(), QVariantMap({ { "sub-delay", 300 } }));

        // a Preset change on the way is kept for the next episodes
        m_target->userSets("sub-delay", 500);
        m_target->newFile();
        m_controller->setMedia(episode("/shows/The Wand/S02/e02.mkv"));
        QCOMPARE(m_target->live.value("sub-delay").toInt(), 500);
    }

    void testChoosingAPresetAttachesItToThePlayingFilesFolder()
    {
        const QString anime = m_model->create("Anime", { { "rate", 1.25 } });
        startController();
        m_controller->setMedia(episode("/shows/The Wand/S01/e01.mkv"));
        QSignalSpy notice(m_controller.get(), &PresetController::attached);

        m_controller->choose(anime);

        QCOMPARE(m_model->folders(anime), QStringList{ "/shows/The Wand/S01" });
        QCOMPARE(m_controller->activeId(), anime);
        QCOMPARE(m_target->live.value("rate").toDouble(), 1.25);
        QCOMPARE(notice.count(), 1);
        QCOMPARE(notice.first().at(0).toString(), QStringLiteral("Anime"));
        QCOMPARE(notice.first().at(1).toString(), QStringLiteral("S01"));

        // choosing it again is no news
        m_controller->choose(anime);
        QCOMPARE(notice.count(), 1);
    }

    void testChoosingDefaultLeavesOnlyTheProfile()
    {
        const QString anime = m_model->create("Anime", { { "rate", 1.25 } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/S01/e01.mkv"));

        m_controller->choose(QString());

        QVERIFY(m_controller->activeId().isEmpty());
        QCOMPARE(m_target->live.value("rate").toDouble(), 1.);
        // the other seasons keep the Show's Preset
        QCOMPARE(m_model->resolve("/shows/The Wand/S02").id, anime);

        // on the Show's own folder, its link goes
        m_controller->setMedia(episode("/shows/The Wand/pilot.mkv"));
        m_controller->choose(QString());
        QVERIFY(m_model->folders(anime).isEmpty());
        QVERIFY(!m_model->link("/shows/The Wand").has_value());
    }

    void testANewPresetStartsFromWhatDiffersFromTheProfile()
    {
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));
        m_target->live.insert("video-hue", 120.);
        m_target->live.insert("sub-delay", 300);

        const QString id = m_controller->createFromCurrent("Anime");

        QCOMPARE(m_model->name(id), QStringLiteral("Anime"));
        QCOMPARE(m_model->values(id), QVariantMap({ { "video-hue", 120. }, { "sub-delay", 300 } }));
        QCOMPARE(m_model->folders(id), QStringList{ "/shows/The Wand" });
        QCOMPARE(m_controller->activeId(), id);
        QVERIFY(!m_target->profileWrites);
    }

    void testResetToProfileValueTakesTheKeyOutOfThePreset()
    {
        const QString anime = m_model->create("Anime", { { "rate", 1.25 }, { "video-hue", 120. } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));

        m_controller->resetToProfile({ "video-hue" });

        QCOMPARE(m_model->values(anime), QVariantMap({ { "rate", 1.25 } }));
        QCOMPARE(m_target->live.value("video-hue").toDouble(), 0.);
        QCOMPARE(m_controller->overriddenKeys(), QStringList{ "rate" });
    }

    void testPickingATrackSetsThePresetsLanguage()
    {
        const QString anime = m_model->create("Anime");
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));

        // the track menu gives the language of the picked track
        emit m_target->changedByUser({ { "sub-language", "ja" } });

        QCOMPARE(m_model->values(anime).value("sub-language").toString(), QStringLiteral("ja"));
        // so that the next episode follows it
        QCOMPARE(m_target->live.value("sub-language").toString(), QStringLiteral("ja"));
        QCOMPARE(m_target->profile.value("sub-language").toString(), QStringLiteral("en"));
    }

    void testDeletingTheActivePresetUntilUndo()
    {
        const QString anime = m_model->create("Anime", { { "rate", 1.25 } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));

        m_model->remove(anime);
        QVERIFY(m_controller->activeId().isEmpty());
        QCOMPARE(m_target->live.value("rate").toDouble(), 1.);
        QVERIFY(m_target->profileWrites);

        m_model->undoRemove();
        QCOMPARE(m_controller->activeId(), anime);
        QCOMPARE(m_target->live.value("rate").toDouble(), 1.25);
    }

    void testRenamingTheActivePresetShowsItsNewName()
    {
        const QString anime = m_model->create("Anime");
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));
        QSignalSpy active(m_controller.get(), &PresetController::activeChanged);

        QVERIFY(m_model->rename(anime, "Cartoons"));

        QCOMPARE(m_controller->activeName(), QStringLiteral("Cartoons"));
        QCOMPARE(active.count(), 1);
    }

    void testASwitchOfProfileKeepsThePresetOnTop()
    {
        const QString anime = m_model->create("Anime", { { "sub-language", "ja" } });
        m_model->attach("/shows/The Wand", anime);
        startController();
        m_controller->setMedia(episode("/shows/The Wand/e01.mkv"));

        // the other Profile's options reach the player
        m_target->profile.insert("sub-language", "de");
        m_target->profile.insert("sub-relative-size", 4);
        m_target->live = m_target->profile;
        m_controller->profileSwitched();

        QCOMPARE(m_target->live.value("sub-language").toString(), QStringLiteral("ja"));
        QCOMPARE(m_target->live.value("sub-relative-size").toInt(), 4);
    }

    void testStreamsHaveNoShow()
    {
        const QString anime = m_model->create("Anime", { { "rate", 1.25 } });
        m_model->attach("/", anime);
        startController();

        m_controller->setMedia(QStringLiteral("https://example.org/live.m3u8"));

        QVERIFY(m_controller->folder().isEmpty());
        QVERIFY(m_controller->activeId().isEmpty());
        m_controller->choose(anime);
        QVERIFY(m_controller->activeId().isEmpty());
    }

private:
    void startController()
    {
        m_target = std::make_unique<FakeTarget>();
        m_target->profile = PROFILE;
        m_target->live = PROFILE;
        m_controller = std::make_unique<PresetController>(m_model.get(), m_target.get());
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_settings;
    std::unique_ptr<PresetsModel> m_model;
    std::unique_ptr<FakeTarget> m_target;
    std::unique_ptr<PresetController> m_controller;
};

QTEST_GUILESS_MAIN(TestPresets)
#include "test_presets.moc"
