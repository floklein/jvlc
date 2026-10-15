/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#include "qml_menu_wrapper.hpp"
#include "menus.hpp"
#include "medialibrary/medialib.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"
#include "medialibrary/mlplaylistmanager.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"
#include "network/networkdevicemodel.hpp"
#include "network/networkmediamodel.hpp"
#include "player/player_controller.hpp"
#include "playlist/playlist_controller.hpp"
#include "playlist/playlist_model.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "util/colorizedsvgicon.hpp"
#include "util/glyph_icon.hpp"
#include "style/vlcicons.hpp"
#include "maininterface/navigationmodel.hpp"
#include "maininterface/pinsmodel.hpp"

// Qt includes
#include <QPainter>
#include <QScreen>
#include <QActionGroup>
#include <QAbstractProxyModel>
#include <QStyle>
#include <QStyleOption>

#include <algorithm>

namespace
{
    QIcon sortIcon(QWidget *widget, int order)
    {
        assert(order == Qt::AscendingOrder || order == Qt::DescendingOrder);

        QStyleOptionHeader headerOption;
        headerOption.initFrom(widget);
        headerOption.sortIndicator = (order == Qt::AscendingOrder)
                ? QStyleOptionHeader::SortDown
                : QStyleOptionHeader::SortUp;

        QStyle *style = qApp->style();
        int arrowsize = style->pixelMetric(QStyle::PM_HeaderMarkSize, &headerOption, widget);
        if (arrowsize <= 0)
            arrowsize = 32;

        qreal dpr = widget ? widget->devicePixelRatioF() : 1.0;
        headerOption.rect = QRect(0, 0, arrowsize, arrowsize);

        QPixmap arrow(arrowsize * dpr, arrowsize * dpr);
        arrow.setDevicePixelRatio(dpr);
        arrow.fill(Qt::transparent);

        {
            QPainter arrowPainter(&arrow);
            style->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &headerOption, &arrowPainter, widget);
        }

        return QIcon(arrow);
    }
}

void StringListMenu::popup(const QPoint &point, const QVariantList &stringList)
{
    assert(m_ctx);
    QMenu *m = newMenu();

    // the two-tone icon of an entry {text, icon: [base, accent]}, either
    // glyph may be empty (Figma 2404:13340)
    const auto setEntryIcon = [](QAction *action, const QMenu *menu, const QVariantMap &entry)
    {
        const QVariantList icon = entry.value("icon").toList();
        if (icon.size() != 2)
            return;
        const auto glyph = [](const QVariant &value) {
            const QString text = value.toString();
            return text.isEmpty() ? QChar() : text.front();
        };
        action->setIcon(glyphIcon(glyph(icon[0]), glyph(icon[1]), menu));
    };

    for (int i = 0; i != stringList.size(); ++i)
    {
        // NOTE: An entry {text, children} is a submenu, an empty child a
        //       separator; a child {text, checked} has a check mark. An
        //       entry {text, icon} is an entry with an icon.
        const bool isMap = stringList[i].typeId() == QMetaType::QVariantMap;
        const QVariantMap entry = isMap ? stringList[i].toMap() : QVariantMap {};
        if (isMap && entry.contains("children"))
        {
            QMenu *submenu = m->addMenu(entry.value("text").toString());
            setEntryIcon(submenu->menuAction(), m, entry);
            const QVariantList children = entry.value("children").toList();
            for (int j = 0; j != children.size(); ++j)
            {
                const bool checkable = children[j].typeId() == QMetaType::QVariantMap;
                QString child = checkable ? children[j].toMap().value("text").toString()
                                          : children[j].toString();
                if (child.isEmpty())
                {
                    submenu->addSeparator();
                    continue;
                }
                // children may be user text (Playlist names): no mnemonic
                child.replace('&', QLatin1String("&&"));
                QAction *action = submenu->addAction(child, this, [this, i, j]()
                {
                    emit childSelected(i, j);
                });
                if (checkable)
                {
                    action->setCheckable(true);
                    action->setChecked(children[j].toMap().value("checked").toBool());
                }
            }
            continue;
        }

        const auto str = isMap ? entry.value("text").toString() : stringList[i].toString();
        // an empty entry is a separator
        if (str.isEmpty())
        {
            m->addSeparator();
            continue;
        }
        // entries are text ("Stream & Cast…", user names): no mnemonic
        QString label = str;
        label.replace('&', QLatin1String("&&"));
        QAction *action = m->addAction(label, this, [this, i, str]()
        {
            emit selected(i, str);
        });
        if (isMap)
            setEntryIcon(action, m, entry);
    }

    m->popup(point);
}

QMenu *BasicMenuContainer::newMenu()
{
    QMenu *m = new VLCMenu(m_ctx->getIntf());
    m->setAttribute(Qt::WA_DeleteOnClose);
    if (m_prevMenu)
    {
        setVisible(false);
        m_prevMenu->disconnect(this);
    }

    m_prevMenu = m;
    connect(m, &QMenu::aboutToShow, this, [this](){ setVisible(true);});
    connect(m, &QMenu::aboutToHide, this, [this](){ setVisible(false);});
    return m;
}

void BasicMenuContainer::setVisible(bool visible)
{
    if (m_visible == visible) return;

    m_visible = visible;
    emit visibleChanged();
}

// SortMenu

void SortMenu::popup(const QPoint &point, const bool popupAbovePoint, const QVariantList &model)
{
    assert(m_ctx);
    m_menu = std::make_unique<VLCMenu>(m_ctx->getIntf());

    connect( m_menu.get(), &QMenu::aboutToShow, this, [this]() {
        m_shown = true;
        shownChanged();
    } );
    connect( m_menu.get(), &QMenu::aboutToHide, this, [this]() {
        m_shown = false;
        shownChanged();
    } );

    const auto actionGroup = new QActionGroup(m_menu.get());
    actionGroup->setExclusive(true);

    // model => [{text: "", checked: <bool>, order: <sort order> if checked else <invalid>}...]
    for (int i = 0; i != model.size(); ++i)
    {
        const auto obj = model[i].toMap();

        auto action = m_menu->addAction(obj.value("text").toString());
        action->setActionGroup(actionGroup);
        action->setCheckable(true);

        const bool checked = obj.value("checked").toBool();
        action->setChecked(checked);

        // an entry with its own order ("Title Z-A") shows no order arrow
        const QVariant order = obj.value("order");
        if (checked && order.isValid() && !order.isNull())
            action->setIcon(sortIcon(m_menu.get(), order.toInt()));

        connect(action, &QAction::triggered, this, [this, i]()
        {
            emit selected(i);
        });
    }

    onPopup(m_menu.get());

    if (popupAbovePoint)
        m_menu->popup(QPoint(point.x(), point.y() - m_menu->sizeHint().height()));
    else
        m_menu->popup(point);
}

void SortMenu::close()
{
    if (m_menu)
        m_menu->close();
}

// Protected functions

/* virtual */ void SortMenu::onPopup(QMenu *) {}

void SortMenuAlbums::onPopup(QMenu *menu)
{
    if (!m_sectionsVisible)
        return;

    assert(m_ctx);
    assert(menu);

    menu->addSeparator();

    QAction *action = menu->addAction(qtr("Album sections"));
    action->setCheckable(true);
    action->setChecked(m_ctx->albumSections());
    connect(action, &QAction::toggled, this, [this] (bool enabled) {
        if (Q_LIKELY(m_ctx))
            m_ctx->setAlbumSections(enabled);
    });
}

QmlGlobalMenu::QmlGlobalMenu(QObject *parent)
    : VLCMenuBar(parent)
{
}

void QmlGlobalMenu::popup(QPoint pos)
{
    if (!m_ctx)
        return;

    qt_intf_t* p_intf = m_ctx->getIntf();
    if (!p_intf)
        return;

    m_menu = std::make_unique<VLCMenu>(m_ctx->getIntf());

    connect( m_menu.get(), &QMenu::aboutToShow, this, [this]() {
        m_shown = true;
        shownChanged();
        aboutToShow();
    });
    connect( m_menu.get(), &QMenu::aboutToHide, this, [this]() {
        m_shown = false;
        shownChanged();
        aboutToHide();
    });

    FoldedEntries( p_intf, m_menu.get(), m_foldedControls,
                   [this]( int control ) { emit foldedControlTriggered( control ); } );

    VLCMenuEntries( p_intf, m_menu.get() );

    m_menu->popup(pos);
}

void QmlGlobalMenu::close()
{
    if (m_menu)
        m_menu->close();
}

// QmlMenuPositioner

/* explicit */ QmlMenuPositioner::QmlMenuPositioner(QObject * parent) : QObject(parent) {}

// Interface

void QmlMenuPositioner::popup(QMenu * menu, const QPoint & position, bool above)
{
    menu->removeEventFilter(this);

    if (above == false)
    {
        menu->popup(position);

        return;
    }

    m_position = position;

    menu->installEventFilter(this);

    // NOTE: QMenu::height() returns an invalid height until the initial popup call.
    menu->popup(position);
}

// Public events

bool QmlMenuPositioner::eventFilter(QObject * object, QEvent * event)
{
    if (event->type() == QEvent::Resize)
    {
        QScreen * screen = QGuiApplication::screenAt(m_position);

        if (screen == nullptr)
            return QObject::eventFilter(object, event);

        QMenu * menu = static_cast<QMenu *> (object);

        int width  = menu->width();
        int height = menu->height();

        QRect geometry = screen->availableGeometry();

        int x = geometry.x();
        int y = geometry.y();

        // NOTE: We want a position within the screen boundaries.

        x = qBound(x, m_position.x(), x + geometry.width() - width);

        y = qBound(y, m_position.y() - height, y + geometry.height() - height);

        menu->move(QPoint(x, y));
    }

    return QObject::eventFilter(object, event);
}

// QmlBookmarkMenu

/* explicit */ QmlBookmarkMenu::QmlBookmarkMenu(QObject * parent)
    : QObject(parent)
{}

// Interface

/* Q_INVOKABLE */ void QmlBookmarkMenu::popup(const QPoint & position, bool above)
{
    if (m_ctx == nullptr || m_player == nullptr)
        return;

    m_menu = std::make_unique<VLCMenu>(m_ctx->getIntf());

    connect(m_menu.get(), &QMenu::aboutToHide, this, &QmlBookmarkMenu::aboutToHide);
    connect(m_menu.get(), &QMenu::aboutToShow, this, &QmlBookmarkMenu::aboutToShow);

    QAction * sectionTitles    = m_menu->addSection(qtr("Titles"));
    QAction * sectionChapters  = m_menu->addSection(qtr("Chapters"));
    QAction * sectionBookmarks = nullptr;

    if (m_ctx->hasMediaLibrary())
        sectionBookmarks = m_menu->addSection(qtr("Bookmarks"));

    // Titles

    TitleListModel * titles = m_player->getTitles();

    sectionTitles->setVisible(titles->rowCount() != 0);

    ListMenuHelper * helper = new ListMenuHelper(m_menu.get(), titles, sectionChapters, m_menu.get());

    connect(helper, &ListMenuHelper::select, [titles](int index, bool)
    {
        titles->setData(titles->index(index), true, Qt::CheckStateRole);
    });

    connect(helper, &ListMenuHelper::countChanged, [sectionTitles](int count)
    {
        // NOTE: The section should only be visible when the model has content.
        sectionTitles->setVisible(count != 0);
    });

    // Chapters

    ChapterListModel * chapters = m_player->getChapters();

    sectionChapters->setVisible(chapters->rowCount() != 0);

    helper = new ListMenuHelper(m_menu.get(), chapters, sectionBookmarks, m_menu.get());

    connect(helper, &ListMenuHelper::select, [chapters](int index, bool)
    {
        chapters->setData(chapters->index(index), true, Qt::CheckStateRole);
    });

    connect(helper, &ListMenuHelper::countChanged, [sectionChapters](int count)
    {
        // NOTE: The section should only be visible when the model has content.
        sectionChapters->setVisible(count != 0);
    });

    // Bookmarks
    if (m_ctx->hasMediaLibrary())
    {
        MLBookmarkModel * bookmarks = new MLBookmarkModel(m_menu.get());
        bookmarks->setPlayer(m_player->getPlayer());
        bookmarks->setMl(m_ctx->getMediaLibrary());

        m_menu->addAction(qtr("&Add Bookmark"), bookmarks, &MLBookmarkModel::add);
        m_menu->addSeparator();

        helper = new ListMenuHelper(m_menu.get(), bookmarks, nullptr, m_menu.get());

        connect(helper, &ListMenuHelper::select, [bookmarks](int index, bool)
        {
            bookmarks->select(bookmarks->index(index, 0));
        });
    }

    m_positioner.popup(m_menu.get(), position, above);
}

void QmlBookmarkMenu::close()
{
    if (m_menu)
        m_menu->close();
}

// QmlProgramMenu

/* explicit */ QmlProgramMenu::QmlProgramMenu(QObject * parent)
    : QObject(parent)
{}

// Interface

/* Q_INVOKABLE */ void QmlProgramMenu::popup(const QPoint & position, bool above)
{
    assert(m_ctx);
    if (m_player == nullptr)
        return;

    m_menu = std::make_unique<VLCMenu>(m_ctx->getIntf());

    connect(m_menu.get(), &QMenu::aboutToHide, this, &QmlProgramMenu::aboutToHide);
    connect(m_menu.get(), &QMenu::aboutToShow, this, &QmlProgramMenu::aboutToShow);

    m_menu->addSection(qtr("Programs"));

    ProgramListModel * programs = m_player->getPrograms();

    ListMenuHelper * helper = new ListMenuHelper(m_menu.get(), programs, nullptr, m_menu.get());

    connect(helper, &ListMenuHelper::select, [programs](int index, bool)
    {
        programs->setData(programs->index(index), true, Qt::CheckStateRole);
    });

    m_positioner.popup(m_menu.get(), position, above);
}

void QmlProgramMenu::close()
{
    if (m_menu)
        m_menu->close();
}

QmlAudioContextMenu::QmlAudioContextMenu(QObject *parent)
    : VLCMenuBar(parent)
{
}

void QmlAudioContextMenu::popup(const QPoint & position)
{
    if (!m_ctx)
        return;

    qt_intf_t* p_intf = m_ctx->getIntf();
    if (!p_intf)
        return;

    m_menu.reset(PopupMenu(p_intf, false));
    if (m_menu)
        m_menu->popup(position);
}

//=================================================================================================
// PlaylistListContextMenu
//=================================================================================================

PlaylistListContextMenu::PlaylistListContextMenu(QObject * parent)
    : BasicMenuContainer(parent)
{}


void PlaylistListContextMenu::popup(const QModelIndexList & selected, QPoint pos, QVariantMap)
{
    assert(m_ctx);

    if (!m_model)
        return;

    QVariantList ids;

    for (const QModelIndex & modelIndex : selected)
        ids.push_back(m_model->data(modelIndex, MLPlaylistListModel::PLAYLIST_ID));

    auto menu = newMenu();

    MediaLib * ml = m_model->ml();

    // the two-tone icons of the entries (Figma 2459:9389)
    const auto icon = [menu](QChar base, QChar accent) {
        return glyphIcon(base, accent, menu);
    };

    // NOTE: The Queue gets a copy of the Playlist, Play and Shuffle replace it (with Undo).
    QAction * action = menu->addAction(icon({}, VLCIcons::playback_tt_accent), qtr("Play"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addAndPlay(ids);
    });

    if (ids.count() == 1)
    {
        const QPersistentModelIndex index = selected.first();
        const bool isFavorite = m_model->data(index, MLPlaylistListModel::PLAYLIST_IS_FAVORITE).toBool();

        action = menu->addAction(icon({}, VLCIcons::favorite_tt_accent),
                                 isFavorite ? qtr("Remove from Favorites") : qtr("Favorite"));

        connect(action, &QAction::triggered, [model = m_model, index, isFavorite]() {
            if (index.isValid())
                model->setPlaylistIsFavorite(index, !isFavorite);
        });
    }

    action = menu->addAction(icon(VLCIcons::shuffle_tt_base, VLCIcons::shuffle_tt_accent),
                             qtr("Shuffle"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->shuffleAndPlay(ids);
    });

    if (ids.count() == 1)
    {
        const MLItemId id = ids.first().value<MLItemId>();
        const QString name = m_model->data(selected.first(), MLPlaylistListModel::PLAYLIST_NAME).toString();

        action = menu->addAction(icon(VLCIcons::save_tt_base, VLCIcons::save_tt_accent),
                                 qtr("Export Playlist..."));

        connect(action, &QAction::triggered, [ml, id, name]() {
            ml->playlistManager()->exportPlaylistAs(id, name);
        });
    }

    action = menu->addAction(icon(VLCIcons::queue_tt_base, VLCIcons::queue_tt_accent),
                             qtr("Add to Queue"));

    connect(action, &QAction::triggered, [ml, ids]() {
        ml->addToPlaylist(ids);
    });

    PinsModel * pins = m_ctx->getPins();

    if (ids.count() == 1 && pins)
    {
        const QString key = ids.first().value<MLItemId>().toString();
        const QString name = m_model->data(selected.first(), MLPlaylistListModel::PLAYLIST_NAME).toString();

        if (pins->isPinned(PinsModel::PLAYLIST, key))
        {
            action = menu->addAction(qtr("Unpin from Sidebar"));

            connect(action, &QAction::triggered, pins, [pins, key]() {
                pins->unpin(PinsModel::PLAYLIST, key);
            });
        }
        else
        {
            action = menu->addAction(qtr("Pin to Sidebar"));

            connect(action, &QAction::triggered, pins, [pins, key, name]() {
                pins->pin(PinsModel::PLAYLIST, key, name);
            });
        }
    }

    if (ids.count() == 1)
    {
        // Add Subtitles, To This Playlist
        const QString name = m_model->data(selected.first(), MLPlaylistListModel::PLAYLIST_NAME).toString();

        action = menu->addAction(icon(VLCIcons::subtitles_tt_base, VLCIcons::subtitles_tt_accent),
                                 qtr("Add Subtitle Tracks..."));

        connect(action, &QAction::triggered, [id = ids.first(), name]() {
            DialogsProvider::getInstance()->addPlaylistSubtitlesDialog(id, name);
        });
    }

    if (ids.count() == 1)
    {
        menu->addSeparator();

        action = menu->addAction(qtr("Rename"));

        QModelIndex index = selected.first();

        connect(action, &QAction::triggered, [this, index, pins]() {
            if (!m_model->showDialogRename(index) || !pins)
                return;
            // the Pin of the Playlist shows its new name
            const QString key = m_model->data(index, MLPlaylistListModel::PLAYLIST_ID).value<MLItemId>().toString();
            pins->rename(PinsModel::PLAYLIST, key,
                         m_model->data(index, MLPlaylistListModel::PLAYLIST_NAME).toString());
        });
    }

    action = menu->addAction(qtr("Delete"));

    connect(action, &QAction::triggered, this, &PlaylistListContextMenu::requestDeleteSelectedPlaylists);

    menu->popup(pos);
}

//=================================================================================================

void NetworkMediaContextMenu::popup(const QModelIndexList& selected, QPoint pos)
{
    assert(m_ctx);

    if (!m_model)
        return;

    auto menu = newMenu();
    QAction* action;

    action = menu->addAction( qtr("Add and play") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addAndPlay(selected);
    });

    action = menu->addAction( qtr("Enqueue") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addToPlaylist(selected);
    });

    action = menu->addAction( qtr("Information") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        if (selected.isEmpty()) return;

        m_model->getItemsForIndexes(selected, [](const QVariantList& items) {
            if (items.isEmpty()) return;

            QVariant firstItem = items.first();

            if (firstItem.canConvert<SharedInputItem>()) {
                 SharedInputItem sii = firstItem.value<SharedInputItem>();
                 DialogsProvider::getInstance()->mediaInfoDialog(sii);
            }
        });
    });

    bool canBeIndexed = false;
    unsigned countIndexed = 0;
    for (const QModelIndex& idx : selected)
    {
        QVariant canIndex = m_model->data(m_model->index(idx.row()), NetworkMediaModel::NETWORK_CANINDEX );
        if (canIndex.isValid() && canIndex.toBool())
            canBeIndexed = true;
        else
            continue;
        QVariant isIndexed = m_model->data(m_model->index(idx.row()), NetworkMediaModel::NETWORK_INDEXED );
        if (!isIndexed.isValid())
            continue;
        if (isIndexed.toBool())
            ++countIndexed;
    }

    if (canBeIndexed)
    {
        bool removeFromML = countIndexed > 0;
        action = menu->addAction(removeFromML
            ? qtr("Remove from Media Library")
            : qtr("Add to Media Library"));

        connect(action, &QAction::triggered, [this, selected, removeFromML]( ) {
            for (const QModelIndex& idx : selected) {
                m_model->setData(m_model->index(idx.row()), !removeFromML, NetworkMediaModel::NETWORK_INDEXED);
            }
        });
    }

    menu->popup(pos);
}

void NetworkDeviceContextMenu::popup(const QModelIndexList& selected, QPoint pos)
{
    assert(m_ctx);

    if (!m_model)
        return;

    auto menu = newMenu();
    QAction* action;

    action = menu->addAction( qtr("Add and play") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addAndPlay(selected);
    });

    action = menu->addAction( qtr("Enqueue") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        m_model->addToPlaylist(selected);
    });

    action = menu->addAction( qtr("Information") );
    connect(action, &QAction::triggered, [this, selected]( ) {
        if (selected.isEmpty()) return;

        m_model->getItemsForIndexes(selected, [](const QVariantList& items) {
            if (items.isEmpty()) return;

            QVariant firstItem = items.first();

            if (firstItem.canConvert<SharedInputItem>()) {
                 SharedInputItem sii = firstItem.value<SharedInputItem>();
                 DialogsProvider::getInstance()->mediaInfoDialog(sii);
            }
        });
    });

    menu->popup(pos);
}

PlaylistContextMenu::PlaylistContextMenu(QObject* parent)
    : QObject(parent)
{}

void PlaylistContextMenu::popup(int selectedIndex, QPoint pos )
{
    assert(m_ctx);

    if (!m_controler || !m_model || !m_selectionModel)
        return;

    m_menu = std::make_unique<VLCMenu>(m_ctx->getIntf());
    QAction* action;

    MediaLib * ml = m_ctx->getMediaLibrary();

    PlaylistItem selectedItem;
    if (selectedIndex >= 0 && selectedIndex < m_model->rowCount())
        selectedItem = m_model->itemAt(selectedIndex);

    if (selectedItem)
    {
        // Right-click on a row: the selection, or else the row
        QVector<int> rows = m_selectionModel->selectedIndexesFlat();
        if (!rows.contains(selectedIndex))
            rows = { selectedIndex };
        std::sort(rows.begin(), rows.end());

        action = m_menu->addAction( qtr("Play") );
        connect(action, &QAction::triggered, [this, selectedIndex]( ) {
            m_controler->goTo(selectedIndex, true);
        });

        action = m_menu->addAction( qtr("Play Next") );
        action->setEnabled(rows != QVector<int>{ m_model->getCurrentIndex() });
        connect(action, &QAction::triggered, [this, rows]( ) {
            m_model->moveItemsAfterCurrent(rows);
        });

        // Move Up / Move Down, as Alt+Up / Alt+Down do: the rows move together,
        // and not past the ends
        const int rowsCount = static_cast<int>(rows.size());
        const bool together = (rows.last() - rows.first() == rowsCount - 1);
        const int lastFirstRow = m_model->rowCount() - rowsCount;
        const auto canShift = [&rows, together, lastFirstRow](int offset) {
            return !together
                   || std::clamp(rows.first() + offset, 0, lastFirstRow) != rows.first();
        };

        action = m_menu->addAction( qtr("Move Up") );
        action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
        action->setEnabled(canShift(-1));
        connect(action, &QAction::triggered, [this, rows]( ) {
            m_model->shiftItems(rows, -1);
        });

        action = m_menu->addAction( qtr("Move Down") );
        action->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
        action->setEnabled(canShift(1));
        connect(action, &QAction::triggered, [this, rows]( ) {
            m_model->shiftItems(rows, 1);
        });

        action = m_menu->addAction( qtr("Remove") );
        action->setShortcut(QKeySequence::Delete);
        connect(action, &QAction::triggered, [this, rows]( ) {
            m_model->removeItems(rows);
        });

        m_menu->addSeparator();

        if (ml)
        {
            QVariantList rowItems;
            for (const int row : rows)
                rowItems.push_back(m_model->itemAt(row).getUrl());
            ml->playlistManager()->addToPlaylistMenu(m_menu.get(), rowItems);

            m_menu->addSeparator();
        }

        // the rows, in the background (Conversion)
        action = m_menu->addAction( qtr("Convert...") );
        connect(action, &QAction::triggered, [this, rows]( ) {
            QStringList mrls;
            for (const int row : rows)
                mrls.push_back(m_model->itemAt(row).getUrl().toString(QUrl::FullyEncoded));
            DialogsProvider::getInstance()->convertDialog(mrls);
        });

#ifdef _WIN32
        action = m_menu->addAction( qtr("Show in File Explorer") );
#else
        action = m_menu->addAction( qtr("Show in File Manager") );
#endif
        connect(action, &QAction::triggered, [this, selectedItem]( ) {
            m_controler->explore(selectedItem);
        });

        action = m_menu->addAction( qtr("Properties") );
        connect(action, &QAction::triggered, [selectedItem]( ) {
            DialogsProvider::getInstance()->mediaInfoDialog(selectedItem);
        });
    }
    else
    {
        // Right-click on empty space
        action = m_menu->addAction( qtr("Clear Queue") );
        action->setEnabled(m_model->rowCount() > 0);
        connect(action, &QAction::triggered, [this]( ) {
            m_controler->clear();
        });

        if (ml)
        {
            action = m_menu->addAction( qtr("Save Queue as Playlist...") );
            action->setEnabled(m_model->rowCount() > 0);
            connect(action, &QAction::triggered, [ml]( ) {
                ml->playlistManager()->saveQueueAsPlaylist();
            });
        }

        // streams the current item or the whole Queue, or casts them
        action = m_menu->addAction( qtr("Stream && Cast...") );
        action->setEnabled(m_model->rowCount() > 0);
        connect(action, &QAction::triggered, []( ) {
            DialogsProvider::getInstance()->streamAndCastDialog();
        });
    }

    m_menu->popup(pos);
}

NavigationBarContextMenu::NavigationBarContextMenu(QObject *parent)
    : BasicMenuContainer(parent)
{

}

void NavigationBarContextMenu::popup(int index, const QPoint& pos, bool setFirstActionAsActive)
{
    assert(m_ctx);
    assert(m_model);

    QMenu *menu = newMenu();
    assert(menu);

    const auto modelIndex = m_model->index(index, 0);
    assert(modelIndex.isValid());

    const auto children = m_model->data(modelIndex, NavigationModel::CHILDREN).value<QList<NavigationModelItem>>();

    if (children.count() <= 0)
    {
        emit pageRequested(m_model->data(modelIndex, NavigationModel::URI).toStringList());
        return;
    }

    populateMenuForChildren(menu, children, setFirstActionAsActive);

    menu->popup(pos);
}

void NavigationBarContextMenu::popup(const QPoint &pos, bool setFirstActionAsActive)
{
    assert(m_ctx);
    assert(m_model);

    QMenu *menu = newMenu();
    assert(menu);

    bool activeActionSet = !setFirstActionAsActive;
    bool menuAdded = false;

    for (int i = 0; i < m_model->rowCount(); ++i)
    {
        const auto modelIndex = m_model->index(i, 0);
        assert(modelIndex.isValid());

        const auto menuTitle = m_model->data(modelIndex, NavigationModel::TITLE).toString();
        const auto menuUri = m_model->data(modelIndex, NavigationModel::URI).toStringList();
        const auto menuIcon = m_model->data(modelIndex, NavigationModel::ICON_SVG).toString();
        const auto menuExpandable = m_model->data(modelIndex, NavigationModel::EXPANDABLE).toBool();
        const auto menuChildren = m_model->data(modelIndex, NavigationModel::CHILDREN).value<QList<NavigationModelItem>>();
        const auto menuDepth = m_model->data(modelIndex, NavigationModel::DEPTH).toInt();

        // We currently do not care about deeper levels:
        if (menuDepth != 0)
            continue;

        if (menuExpandable && menuChildren.size() > 0)
        {
            const auto subMenu = menu->addMenu(menuTitle);

            if (menuIcon.length() > 0)
                subMenu->setIcon(ColorizedSvgIcon::colorizedIconForWidget(menuIcon, subMenu));

            populateMenuForChildren(subMenu, menuChildren, setFirstActionAsActive);

            menuAdded = true;
        }
        else
        {
            const auto action = menu->addAction(menuTitle, [this, menuUri]() {
                emit pageRequested(menuUri);
            });

            if (menuIcon.length() > 0)
                action->setIcon(ColorizedSvgIcon::colorizedIconForWidget(menuIcon, qobject_cast<QWidget*>(menu)));

            if (!activeActionSet && !menuAdded)
            {
                menu->setActiveAction(action);
                activeActionSet = true;
            }
        }
    }

    menu->popup(pos);
}

void NavigationBarContextMenu::populateMenuForChildren(QMenu *menu, const QList<NavigationModelItem> &children, bool setFirstActionAsActive)
{
    assert(m_ctx);
    assert(m_model);
    assert(menu);

    bool activeActionSet = !setFirstActionAsActive;

    for (const auto& child : children)
    {
        const auto childTitle = child.name();
        assert(childTitle.length() > 0);
        const auto childUri = child.uri();
        assert(childUri.length() > 0);
        const auto childIcon = child.icon_svg();

        const auto action = menu->addAction(childTitle, [this, uri = childUri]() {
            emit pageRequested(uri);
        });

        if (childIcon.length() > 0)
            action->setIcon(ColorizedSvgIcon::colorizedIconForWidget(childIcon, qobject_cast<QWidget*>(menu)));

        if (!activeActionSet)
        {
            menu->setActiveAction(action);
            activeActionSet = true;
        }
    }
}
