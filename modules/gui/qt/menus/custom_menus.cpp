/*****************************************************************************
 * custom_menus.cpp : Qt custom menus classes
 *****************************************************************************
 * Copyright © 2006-2018 VideoLAN authors
 *                  2018 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "custom_menus.hpp"

// MediaLibrary includes
#include "medialibrary/mlbookmarkmodel.hpp"

// Dialogs includes
#include "dialogs/dialogs_provider.hpp"

// Menus includes
#include "menus/menus.hpp"
#include "player/player_controller.hpp"

// Qt includes
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMetaMethod>

#include "util/vlcaccess_image_provider.hpp"

/*   CheckableListMenu   */

CheckableListMenu::CheckableListMenu(QString title, QAbstractListModel* model , QActionGroup::ExclusionPolicy grouping,  QWidget *parent)
    : QMenu(parent)
    , m_model(model)
{
    this->setTitle(title);

    ListMenuHelper* helper = new ListMenuHelper(this, model, nullptr, this);
    helper->getActionGroup()->setExclusionPolicy(grouping);
    connect(helper, &ListMenuHelper::select, this, [this](int row, bool checked){
        m_model->setData(m_model->index(row), checked, Qt::CheckStateRole);
    });
}

// ListMenuHelper

ListMenuHelper::ListMenuHelper(QMenu * menu, QAbstractListModel * model, QAction * before,
                               QObject * parent)
    : QObject(parent), m_menu(menu), m_model(model), m_before(before)
{
    m_group = new QActionGroup(this);

    onModelReset();

    connect(m_model, &QAbstractListModel::rowsInserted, this, &ListMenuHelper::onRowsInserted);
    connect(m_model, &QAbstractListModel::rowsRemoved,  this, &ListMenuHelper::onRowsRemoved);

    connect(m_model, &QAbstractListModel::dataChanged, this, &ListMenuHelper::onDataChanged);

    connect(m_model, &QAbstractListModel::modelReset, this, &ListMenuHelper::onModelReset);
}


ListMenuHelper::~ListMenuHelper()
{}
// Interface

int ListMenuHelper::count() const
{
    return m_actions.count();
}

QActionGroup* ListMenuHelper::getActionGroup() const
{
    return m_group;
}


void ListMenuHelper::setIcon(QAction* action,  const QUrl& iconUrl)
{

    if (!iconUrl.isValid())
    {
        action->setIcon({});
        return;
    }

    if (m_iconLoader)
    {
        disconnect(m_iconLoader.get(), nullptr, this, nullptr);
        m_iconLoader->cancel();
    }
    m_iconLoader.reset(VLCAccessImageProvider::requestImageResponseUnWrapped(iconUrl, {64,64}));
    connect(m_iconLoader.get(), &QQuickImageResponse::finished, this, [this, action] {
        std::unique_ptr<QQuickTextureFactory> factory(m_iconLoader->textureFactory());
        if (!factory)
        {
            action->setIcon({});
            return;
        }
        QImage img = factory->image();
        action->setIcon(QIcon(QPixmap::fromImage(img)));
    });
}


// Private slots

void ListMenuHelper::onRowsInserted(const QModelIndex &, int first, int last)
{
    QAction * before;

    if (first < m_actions.count())
        before = m_actions.at(first);
    else
        before = m_before;

    for (int i = first; i <= last; i++)
    {
        QModelIndex index = m_model->index(i, 0);

        QString name = m_model->data(index, Qt::DisplayRole).toString();

        QAction * action = new QAction(name, this);

        QVariant checked = m_model->data(index, Qt::CheckStateRole);
        if (checked.isValid() && checked.canConvert<bool>())
        {
            action->setCheckable(true);
            action->setChecked(checked.toBool());
        }

        QVariant iconPath = m_model->data(index, Qt::DecorationRole);
        if (iconPath.isValid())
        {
            QUrl iconUrl;
            if (iconPath.canConvert<QUrl>())
                iconUrl = iconPath.toUrl();
            else if (iconPath.canConvert<QString>())
                iconUrl = QUrl::fromEncoded(iconPath.toString().toUtf8());

            setIcon(action, iconUrl);
        }

        // NOTE: We are adding sequentially *before* the next action in the list.
        m_menu->insertAction(before, action);

        m_group->addAction(action);

        m_actions.insert(i, action);

        connect(action, &QAction::triggered, this, &ListMenuHelper::onTriggered);
    }

    emit countChanged(m_actions.count());
}

void ListMenuHelper::onRowsRemoved(const QModelIndex &, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        QAction * action = m_actions.at(i);

        m_group->removeAction(action);

        delete action;
    }

    QList<QAction *>::iterator begin = m_actions.begin();

    m_actions.erase(begin + first, begin + last + 1);

    emit countChanged(m_actions.count());
}

void ListMenuHelper::onDataChanged(const QModelIndex & topLeft,
                                   const QModelIndex & bottomRight, const QVector<int> & roles)
{
    const bool updateDisplay = roles.contains(Qt::DisplayRole);
    const bool updateChecked = roles.contains(Qt::CheckStateRole);
    const bool udpateIcon = roles.contains(Qt::DecorationRole);

    for (int i = topLeft.row(); i <= bottomRight.row(); i++)
    {
        QAction * action = m_actions.at(i);
        QModelIndex index = m_model->index(i, 0);

        if (updateDisplay)
        {
            QString name = m_model->data(index, Qt::DisplayRole).toString();
            action->setText(name);
        }

        if (updateChecked)
        {
            QVariant checked = m_model->data(index, Qt::CheckStateRole);
            if (checked.isValid() && checked.canConvert<bool>())
                action->setChecked(checked.toBool());
        }

        if (udpateIcon)
        {
            QVariant iconPath = m_model->data(index, Qt::DecorationRole);
            if (iconPath.isValid())
            {
                QUrl iconUrl;
                if (iconPath.canConvert<QUrl>())
                    iconUrl = iconPath.toUrl();
                else if (iconPath.canConvert<QString>())
                    iconUrl = QUrl::fromEncoded(iconPath.toString().toUtf8());

                setIcon(action, iconUrl);
            }
        }
    }
}

void ListMenuHelper::onModelReset()
{
    for (QAction * action : m_actions)
    {
        m_group->removeAction(action);

        delete action;
    }

    m_actions.clear();

    int count = m_model->rowCount();

    if (count)
        onRowsInserted(QModelIndex(), 0, count - 1);
}

void ListMenuHelper::onTriggered(bool checked)
{
    QAction * action = static_cast<QAction *> (sender());

    emit select(m_actions.indexOf(action), checked);
}

/*     BooleanPropertyAction    */

BooleanPropertyAction::BooleanPropertyAction(QString title, QObject *model, QString propertyName, QWidget *parent)
    : QAction(parent)
    , m_model(model)
    , m_propertyName(propertyName)
{
    setText(title);
    assert(model);
    const QMetaObject* meta = model->metaObject();
    int propertyId = meta->indexOfProperty(qtu(propertyName));
    assert(propertyId != -1);
    QMetaProperty property = meta->property(propertyId);
    assert(property.typeId() ==  QMetaType::Bool);
    const QMetaObject* selfMeta = this->metaObject();

    assert(property.hasNotifySignal());
    QMetaMethod checkedSlot = selfMeta->method(selfMeta->indexOfSlot( "setChecked(bool)" ));
    connect( model, property.notifySignal(), this, checkedSlot );
    connect( this, &BooleanPropertyAction::triggered, this, &BooleanPropertyAction::setModelChecked );

    setCheckable(true);
    setChecked(property.read(model).toBool());
}

void BooleanPropertyAction::setModelChecked(bool checked)
{
    m_model->setProperty(qtu(m_propertyName), QVariant::fromValue<bool>(checked) );
}


RecentMenu::RecentMenu(MLRecentMediaModel* model, MediaLib* ml,  QWidget* parent)
    : QMenu(parent)
    , m_model(model)
    , m_ml(ml)
{
    QAction* separator = addSeparator();

    ListMenuHelper* helper = new ListMenuHelper(this, model, separator, this);
    connect(helper, &ListMenuHelper::select, this, [this](int row, bool){
        QModelIndex index = m_model->index(row);

        MLItemId id = m_model->data(index, MLRecentMediaModel::MEDIA_ID).value<MLItemId>();
        m_ml->addAndPlay(id);
    });

    addAction( qtr("&Clear"), model, &MLRecentMediaModel::clearHistory );
}

// BookmarkMenu

BookmarkMenu::BookmarkMenu(MediaLib * mediaLib, vlc_player_t * player, QWidget * parent)
    : QMenu(parent)
{
    MLBookmarkModel * model = new MLBookmarkModel(this);
    model->setPlayer(player);
    model->setMl(mediaLib);

    addAction(qtr("&Add Bookmark"), model, &MLBookmarkModel::add);

    addSeparator();

    ListMenuHelper * helper = new ListMenuHelper(this, model, nullptr, this);

    connect(helper, &ListMenuHelper::select, [model](int index, bool )
    {
        model->select(model->index(index, 0));
    });

    setTearOffEnabled(true);
}
