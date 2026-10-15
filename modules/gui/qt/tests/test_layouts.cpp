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

#include <QTest>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "../dialogs/toolbar/controlbar_profile_model.hpp"
#include "../dialogs/toolbar/controlbar_profile.hpp"
#include "../player/player_controlbar_model.hpp"
#include "../player/control_list_model.hpp"

using C = ControlListModel;
using P = PlayerControlbarModel;

// Layouts: the arrangement of the Top bar and Control bar, edited by
// Customize Player (spec #30, ticket #44)
class TestLayouts : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    int m_file = 0;

    QString newSettingsPath()
    {
        return m_dir.filePath(QStringLiteral("layouts-%1.ini").arg(++m_file));
    }

    static std::unique_ptr<ControlbarProfile> builtIn(ControlbarProfileModel::Style style)
    {
        return std::unique_ptr<ControlbarProfile>(ControlbarProfileModel::generateProfileFromStyle(style));
    }

    static QVector<int> zone(const ControlbarProfile* profile, int identifier, int zone)
    {
        return profile->getModelData(identifier)[zone];
    }

    // a Default Layout, as a user would start editing it
    static std::unique_ptr<ControlbarProfile> defaultLayout()
    {
        return builtIn(ControlbarProfileModel::Style::DEFAULT_STYLE);
    }

    static void writeLayout(QSettings& settings, const QString& model)
    {
        settings.beginGroup(QStringLiteral("PlayerLayouts"));
        settings.setValue(QStringLiteral("SelectedProfile"), 0);
        settings.beginWriteArray(QStringLiteral("Profiles"));
        settings.setArrayIndex(0);
        settings.setValue(QStringLiteral("Name"), QStringLiteral("Mine"));
        settings.setValue(QStringLiteral("Model"), model);
        settings.endArray();
        settings.endGroup();
        settings.sync();
    }

private slots:
    // saved Layouts store the numeric values: new items take the free
    // slots, and the existing values never move
    void testNewControlsAreAppended()
    {
        QCOMPARE(int(C::PLAY_BUTTON), 0x00);
        QCOMPARE(int(C::VOLUME), 0x21);
        QCOMPARE(int(C::SUBTITLES_BUTTON), 0x2D);
        QCOMPARE(int(C::VLC_MENU_BUTTON), 0x2E);
        QCOMPARE(int(C::LIBRARY_BUTTON), 0x2F);
        QCOMPARE(int(C::FOLDER_BUTTON), 0x30);
        QCOMPARE(int(C::TITLE_WIDGET), 0x31);
        QCOMPARE(int(C::DIVIDER), 0x32);
        QCOMPARE(int(C::WIDGET_SPACER), 0x40);
    }

    // Built-in Layouts

    // the Default and Two-row Layouts replace the five old presets
    void testFirstStartOffersDefaultAndTwoRow()
    {
        QSettings settings(newSettingsPath(), QSettings::IniFormat);
        ControlbarProfileModel model(&settings);

        QTRY_COMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(), QStringLiteral("Default"));
        QCOMPARE(model.data(model.index(1), Qt::DisplayRole).toString(), QStringLiteral("Two-row"));
        QCOMPARE(model.selectedProfile(), 0);
        QVERIFY(!model.dirty());
    }

    // Figma 247:447: the Top bar holds the VLC menu, library toggle, a
    // divider and the folder button on the left, the title in the center
    void testDefaultTopBar()
    {
        const auto layout = defaultLayout();

        for (int identifier : { int(P::Videoplayer), int(P::Audioplayer) })
        {
            QCOMPARE(zone(layout.get(), identifier, P::TopLeft),
                     (QVector<int>{ C::VLC_MENU_BUTTON, C::LIBRARY_BUTTON, C::DIVIDER, C::FOLDER_BUTTON }));
            QCOMPARE(zone(layout.get(), identifier, P::TopCenter), (QVector<int>{ C::TITLE_WIDGET }));
            QCOMPARE(zone(layout.get(), identifier, P::TopRight), QVector<int>{});
            QVERIFY(layout->getModel(identifier)->seekBarInline());
        }
    }

    // Figma 2160:7437: the seek bar on its own row, the Top bar empty and
    // its items at the left of the Control bar
    void testTwoRowLayout()
    {
        const auto layout = builtIn(ControlbarProfileModel::Style::TWO_ROW_STYLE);
        QVERIFY(layout);

        const int video = P::Videoplayer;
        QCOMPARE(zone(layout.get(), video, P::Left),
                 (QVector<int>{ C::VLC_MENU_BUTTON, C::LIBRARY_BUTTON, C::DIVIDER,
                                C::FOLDER_BUTTON, C::TITLE_WIDGET }));
        QCOMPARE(zone(layout.get(), video, P::Center),
                 (QVector<int>{ C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON }));
        QCOMPARE(zone(layout.get(), video, P::Right),
                 (QVector<int>{ C::VOLUME, C::AUDIO_BUTTON, C::SUBTITLES_BUTTON,
                                C::EXTENDED_BUTTON, C::PLAYLIST_BUTTON }));
        for (int z : { int(P::TopLeft), int(P::TopCenter), int(P::TopRight) })
            QCOMPARE(zone(layout.get(), video, z), QVector<int>{});
        QVERIFY(!layout->getModel(video)->seekBarInline());

        // music has no subtitles button
        QVERIFY(!zone(layout.get(), P::Audioplayer, P::Right).contains(C::SUBTITLES_BUTTON));
        QVERIFY(!layout->getModel(P::Audioplayer)->seekBarInline());
    }

    // Uniqueness

    void testPlacedItemCannotBeAddedAgain()
    {
        const auto layout = defaultLayout();
        PlayerControlbarModel* bars = layout->getModel(P::Videoplayer);

        QVERIFY(bars->isPlaced(C::PLAY_BUTTON));
        QVERIFY(!bars->canAdd(C::PLAY_BUTTON));
        QVERIFY(!bars->addControl(bars->topRight(), 0, C::PLAY_BUTTON));
        QCOMPARE(bars->topRight()->rowCount(), 0);

        // removing it puts it back in the palette
        QVERIFY(bars->removeControl(bars->left(), 1));
        QVERIFY(!bars->isPlaced(C::PLAY_BUTTON));
        QVERIFY(bars->canAdd(C::PLAY_BUTTON));
        QVERIFY(bars->addControl(bars->topRight(), 0, C::PLAY_BUTTON));
        QCOMPARE(bars->topRight()->getControls(), QVector<int>{ C::PLAY_BUTTON });
    }

    void testSpacerAndDividerRepeat()
    {
        const auto layout = defaultLayout();
        PlayerControlbarModel* bars = layout->getModel(P::Videoplayer);

        for (int control : { int(C::WIDGET_SPACER), int(C::WIDGET_SPACER_EXTEND), int(C::DIVIDER) })
        {
            QVERIFY(bars->addControl(bars->right(), 0, control));
            QVERIFY(bars->canAdd(control));
            QVERIFY(bars->addControl(bars->right(), 0, control));
        }
        QCOMPARE(bars->right()->getControls().count(C::DIVIDER), 2);

        // an expanding spacer would stretch the center zones
        QVERIFY(!bars->addControl(bars->center(), 0, C::WIDGET_SPACER_EXTEND));
        QVERIFY(!bars->addControl(bars->topCenter(), 0, C::WIDGET_SPACER_EXTEND));
    }

    void testVlcMenuCannotBeRemovedButMoves()
    {
        const auto layout = defaultLayout();
        PlayerControlbarModel* bars = layout->getModel(P::Videoplayer);

        QVERIFY(!bars->isRemovable(C::VLC_MENU_BUTTON));
        QVERIFY(bars->isRemovable(C::PLAY_BUTTON));
        QVERIFY(!bars->removeControl(bars->topLeft(), 0));
        QCOMPARE(bars->topLeft()->getControls().first(), int(C::VLC_MENU_BUTTON));

        QVERIFY(bars->moveControl(bars->topLeft(), 0, bars->left(), 0));
        QCOMPARE(bars->left()->getControls().first(), int(C::VLC_MENU_BUTTON));
        QVERIFY(!bars->topLeft()->contains(C::VLC_MENU_BUTTON));
    }

    void testMovingKeepsItemsUnique()
    {
        const auto layout = defaultLayout();
        PlayerControlbarModel* bars = layout->getModel(P::Videoplayer);

        // from the Top bar to the Control bar and back
        QVERIFY(bars->moveControl(bars->topCenter(), 0, bars->right(), 2));
        QCOMPARE(bars->right()->getControls().at(2), int(C::TITLE_WIDGET));
        QVERIFY(bars->topCenter()->getControls().isEmpty());

        // within a zone
        QVERIFY(bars->moveControl(bars->left(), 0, bars->left(), 2));
        QCOMPARE(bars->left()->getControls(),
                 (QVector<int>{ C::PLAY_BUTTON, C::NEXT_BUTTON, C::PREVIOUS_BUTTON }));
    }

    // Seek bar position

    void testSeekBarStaysInControlBar()
    {
        const auto layout = defaultLayout();
        PlayerControlbarModel* bars = layout->getModel(P::Videoplayer);

        QVERIFY(!bars->canPlace(C::SEEK_BAR, bars->topCenter()));
        QVERIFY(!bars->moveControl(bars->center(), 0, bars->topCenter(), 0));
        QVERIFY(bars->center()->contains(C::SEEK_BAR));
    }

    void testSeekBarOnItsOwnRow()
    {
        const auto layout = defaultLayout();
        PlayerControlbarModel* bars = layout->getModel(P::Videoplayer);
        QSignalSpy spy(bars, &PlayerControlbarModel::seekBarInlineChanged);

        bars->setSeekBarInline(false);
        QVERIFY(!bars->seekBarInline());
        QVERIFY(!bars->isPlaced(C::SEEK_BAR));
        QCOMPARE(spy.count(), 1);

        bars->setSeekBarInline(true);
        QVERIFY(bars->seekBarInline());
        QCOMPARE(bars->center()->getControls(), QVector<int>{ C::SEEK_BAR });
        QCOMPARE(spy.count(), 2);

        // dragging the seek bar out also puts it on its own row
        QVERIFY(bars->removeControl(bars->center(), 0));
        QVERIFY(!bars->seekBarInline());
    }

    // Keyboard: Alt+Left/Right

    void testShiftMovesWithinAndAcrossZones()
    {
        const auto layout = defaultLayout();
        PlayerControlbarModel* bars = layout->getModel(P::Videoplayer);

        // within the zone
        QVariantMap at = bars->shiftControl(bars->left(), 0, +1);
        QCOMPARE(at.value("zone").value<ControlListModel*>(), bars->left());
        QCOMPARE(at.value("index").toInt(), 1);
        QCOMPARE(bars->left()->getControls(),
                 (QVector<int>{ C::PLAY_BUTTON, C::PREVIOUS_BUTTON, C::NEXT_BUTTON }));

        // past the end of the zone: the start of the next zone
        at = bars->shiftControl(bars->left(), 2, +1);
        QCOMPARE(at.value("zone").value<ControlListModel*>(), bars->center());
        QCOMPARE(at.value("index").toInt(), 0);
        QCOMPARE(bars->center()->getControls(), (QVector<int>{ C::NEXT_BUTTON, C::SEEK_BAR }));

        // back: the end of the previous zone
        at = bars->shiftControl(bars->center(), 0, -1);
        QCOMPARE(at.value("zone").value<ControlListModel*>(), bars->left());
        QCOMPARE(at.value("index").toInt(), 2);

        // the ends of the bar stop it, and the bars stay apart
        QVERIFY(bars->shiftControl(bars->left(), 0, -1).isEmpty());
        QVERIFY(bars->shiftControl(bars->right(), bars->right()->rowCount() - 1, +1).isEmpty());
        QVERIFY(bars->shiftControl(bars->topCenter(), 0, +1).value("zone").value<ControlListModel*>() == bars->topRight());
    }

    // Save, Cancel and Reset to default

    void testResetToDefaultRestoresOnePlayer()
    {
        QSettings settings(newSettingsPath(), QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_COMPARE(model.rowCount(), 2);

        PlayerControlbarModel* video = model.currentModel()->getModel(P::Videoplayer);
        PlayerControlbarModel* music = model.currentModel()->getModel(P::Audioplayer);
        QVERIFY(video->removeControl(video->left(), 0));
        QVERIFY(music->removeControl(music->left(), 0));

        model.resetToDefault(P::Videoplayer);

        QCOMPARE(video->left()->getControls(),
                 (QVector<int>{ C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON }));
        QCOMPARE(video->topLeft()->getControls(),
                 (QVector<int>{ C::VLC_MENU_BUTTON, C::LIBRARY_BUTTON, C::DIVIDER, C::FOLDER_BUTTON }));
        // the other player keeps its changes
        QCOMPARE(music->left()->getControls(),
                 (QVector<int>{ C::SKIP_BACK_BUTTON, C::PLAY_BUTTON, C::SKIP_FW_BUTTON, C::NEXT_BUTTON }));
    }

    void testResetToDefaultKeepsTheBuiltInItCameFrom()
    {
        QSettings settings(newSettingsPath(), QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_COMPARE(model.rowCount(), 2);

        model.setSelectedProfile(1); // Two-row
        PlayerControlbarModel* video = model.currentModel()->getModel(P::Videoplayer);
        video->setSeekBarInline(true);

        model.resetToDefault(P::Videoplayer);
        QVERIFY(!video->seekBarInline());
        QCOMPARE(video->center()->getControls(),
                 (QVector<int>{ C::PREVIOUS_BUTTON, C::PLAY_BUTTON, C::NEXT_BUTTON }));
    }

    // Save is enabled only after a change; Cancel reloads the saved state
    void testDirtyUntilSavedOrCancelled()
    {
        QSettings settings(newSettingsPath(), QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_COMPARE(model.rowCount(), 2);
        model.save();
        QVERIFY(!model.dirty());

        QSignalSpy spy(&model, &ControlbarProfileModel::dirtyChanged);
        PlayerControlbarModel* video = model.currentModel()->getModel(P::Videoplayer);
        QVERIFY(video->removeControl(video->right(), 0));
        QVERIFY(model.dirty());
        QVERIFY(spy.count() > 0);

        // Cancel
        QVERIFY(model.reload());
        QVERIFY(!model.dirty());
        video = model.currentModel()->getModel(P::Videoplayer);
        QVERIFY(video->right()->contains(C::VOLUME));

        // picking the other Layout is a change too
        model.setSelectedProfile(1);
        QVERIFY(model.dirty());

        // Save
        model.save();
        QVERIFY(!model.dirty());
    }

    void testSaveAndReloadKeepBothBarsAndSelection()
    {
        const QString path = newSettingsPath();
        {
            QSettings settings(path, QSettings::IniFormat);
            ControlbarProfileModel model(&settings);
            QTRY_COMPARE(model.rowCount(), 2);

            model.setSelectedProfile(1);
            PlayerControlbarModel* video = model.currentModel()->getModel(P::Videoplayer);
            QVERIFY(video->moveControl(video->left(), 4, video->topCenter(), 0)); // the title
            QVERIFY(video->addControl(video->topRight(), 0, C::FULLSCREEN_BUTTON));
            model.save();
        }

        QSettings settings(path, QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_COMPARE(model.rowCount(), 2);
        QCOMPARE(model.selectedProfile(), 1);
        QVERIFY(!model.dirty());

        PlayerControlbarModel* video = model.currentModel()->getModel(P::Videoplayer);
        QCOMPARE(video->topCenter()->getControls(), QVector<int>{ C::TITLE_WIDGET });
        QCOMPARE(video->topRight()->getControls(), QVector<int>{ C::FULLSCREEN_BUTTON });
        QVERIFY(!video->left()->contains(C::TITLE_WIDGET));
        QVERIFY(!video->seekBarInline());

        // Reset to default still knows it is the Two-row Layout
        model.resetToDefault(P::Videoplayer);
        QVERIFY(video->left()->contains(C::TITLE_WIDGET));
    }

    // a switch of Profile puts other saved Layouts in place, or none
    void testReloadOrDefaultsFollowsTheSettings()
    {
        QSettings settings(newSettingsPath(), QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_COMPARE(model.rowCount(), 2);

        model.setSelectedProfile(1);
        model.save();
        model.setSelectedProfile(0);

        // saved Layouts are read again
        model.reloadOrDefaults();
        QCOMPARE(model.selectedProfile(), 1);
        QVERIFY(!model.dirty());

        // without any, the built-in ones come back
        PlayerControlbarModel* video = model.currentModel()->getModel(P::Videoplayer);
        QVERIFY(video->removeControl(video->right(), 0));
        settings.remove(QStringLiteral("PlayerLayouts"));
        model.reloadOrDefaults();
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(1), Qt::DisplayRole).toString(), QStringLiteral("Two-row"));
        QCOMPARE(model.selectedProfile(), 0);
        QVERIFY(model.currentModel()->getModel(P::Videoplayer)->right()->contains(C::VOLUME));
        QVERIFY(!model.dirty());
    }

    // a hand-edited or broken configuration still follows the rules
    void testLoadingRepairsTheRules()
    {
        const QString path = newSettingsPath();
        {
            QSettings settings(path, QSettings::IniFormat);
            // video: play twice, a seek bar in the Top bar, no VLC menu
            writeLayout(settings, QStringLiteral("$0|0,0,50|43||||43"));
        }

        QSettings settings(path, QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_COMPARE(model.rowCount(), 1);

        PlayerControlbarModel* video = model.currentModel()->getModel(P::Videoplayer);
        QVERIFY(video);
        QCOMPARE(video->left()->getControls(), (QVector<int>{ C::PLAY_BUTTON, C::DIVIDER }));
        QCOMPARE(video->center()->getControls(), QVector<int>{ C::SEEK_BAR });
        QCOMPARE(video->topLeft()->getControls(), QVector<int>{ C::VLC_MENU_BUTTON });
        QCOMPARE(video->topRight()->getControls(), QVector<int>{});
    }

    // the mini player is not customizable
    void testMiniPlayerIsFixed()
    {
        const QString path = newSettingsPath();
        {
            QSettings settings(path, QSettings::IniFormat);
            writeLayout(settings, QStringLiteral("$0|0|43||46|||$2|1|||||"));
        }

        QSettings settings(path, QSettings::IniFormat);
        ControlbarProfileModel model(&settings);
        QTRY_COMPARE(model.rowCount(), 1);

        const auto reference = defaultLayout();
        PlayerControlbarModel* mini = model.currentModel()->getModel(P::Miniplayer);
        QVERIFY(mini);
        QVERIFY(*mini == *reference->getModel(P::Miniplayer));
        QVERIFY(!mini->isPlaced(C::STOP_BUTTON));
        QVERIFY(!mini->isPlaced(C::VLC_MENU_BUTTON));
    }
};

QTEST_GUILESS_MAIN(TestLayouts)
#include "test_layouts.moc"
