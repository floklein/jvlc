/*****************************************************************************
 * Copyright (C) 2023 the VideoLAN team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <QWindow>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>

#if !defined(QT_NO_ACCESSIBILITY) && defined(QT_DECLARATIVE_PRIVATE)

#include <QAccessibleObject>

#include <private/qquickitem_p.h>

#include "compositor_accessibility.hpp"

namespace vlc {



static void unignoredChildren(QQuickItem *item, QList<QQuickItem *> *items)
{
    const QList<QQuickItem*> childItems = item->childItems();
    for (QQuickItem *child : childItems)
    {
        if (QQuickItemPrivate::get(child)->isAccessible)
            items->append(child);
        else
            unignoredChildren(child, items);
    }
}

static QList<QQuickItem *> accessibleUnignoredChildren(QQuickItem *item)
{
    QList<QQuickItem *> items;
    unignoredChildren(item, &items);
    return items;
}

// the window that shows the interface drawn in an offscreen window, if any
static QWindow* renderWindowOf(const QQuickWindow* offscreenWindow)
{
    if (!offscreenWindow)
        return nullptr;
    const QWindowList windows = QGuiApplication::allWindows();
    for (QWindow* window : windows)
    {
        const auto renderWindow = dynamic_cast<AccessibleRenderWindow*>(window);
        if (renderWindow && renderWindow->getOffscreenWindow() == offscreenWindow)
            return window;
    }
    return nullptr;
}

/*
 * Screen readers walk up from the focused item to its window, and down from
 * the windows of the application. Qt makes the offscreen window the parent of
 * the root items of the interface, so the offscreen window is a pane of the
 * render window, the window the user sees: the render window has the
 * offscreen window as its only child, and the offscreen window has the root
 * items.
 */
class QAccessibleRenderWindow: public QAccessibleObject
{
public:
    QAccessibleRenderWindow(QWindow* window, AccessibleRenderWindow* renderWindow)
        : QAccessibleObject(window)
        , m_renderWindow(renderWindow)
    {
    }

    QAccessibleInterface* parent() const override
    {
        // we assume to be a top level window...
        return QAccessible::queryAccessibleInterface(qApp);
    }

    QAccessibleInterface* offscreenInterface() const
    {
        if (QQuickWindow* offscreenWindow = m_renderWindow->getOffscreenWindow())
            return QAccessible::queryAccessibleInterface(offscreenWindow);
        return nullptr;
    }

    QAccessibleInterface* child(int index) const override
    {
        return (index == 0) ? offscreenInterface() : nullptr;
    }

    int childCount() const override
    {
        return offscreenInterface() ? 1 : 0;
    }

    int indexOfChild(const QAccessibleInterface *iface) const override
    {
        const QAccessibleInterface* offscreen = offscreenInterface();
        return (iface && offscreen && iface->object() == offscreen->object()) ? 0 : -1;
    }

    QAccessibleInterface *childAt(int x, int y) const override
    {
        if (QAccessibleInterface* offscreen = offscreenInterface())
            return offscreen->childAt(x, y);
        return nullptr;
    }

    QAccessibleInterface *focusChild() const override
    {
        if (QAccessibleInterface* offscreen = offscreenInterface())
            return offscreen->focusChild();
        return nullptr;
    }

    QString text(QAccessible::Text) const override
    {
        return window()->title();
    }

    QRect rect() const override
    {
        return QRect(window()->x(), window()->y(), window()->width(), window()->height());
    }

    QAccessible::Role role() const override
    {
        return QAccessible::Window;
    }

    QAccessible::State state() const override
    {

        QAccessible::State st;
        // the focus window is its interface surface, a child window
        if (window()->isActive())
            st.active = true;
        if (!window()->isVisible())
            st.invisible = true;
        return st;
    }

    QWindow* window() const override
    {
        return static_cast<QWindow*>(object());
    }

private:
    AccessibleRenderWindow* m_renderWindow;
};


class QAccessibleOffscreenWindow: public QAccessibleObject
{
public:
    QAccessibleOffscreenWindow(QQuickWindow *window)
        : QAccessibleObject(window)
    {
    }

    // the render window may show it after it is first queried
    QWindow* renderWindow() const
    {
        return renderWindowOf(quickWindow());
    }

    QAccessibleInterface* parent() const override
    {
        if (QWindow* renderWindow = this->renderWindow())
            return QAccessible::queryAccessibleInterface(renderWindow);
        // we assume to be a top level window...
        return QAccessible::queryAccessibleInterface(qApp);
    }

    QList<QQuickItem *> rootItems() const
    {
        if (!renderWindow())
            return {};
        if (QQuickItem *ci = quickWindow()->contentItem())
            return accessibleUnignoredChildren(ci);
        return {};
    }

    QAccessibleInterface* child(int index) const override
    {
        const QList<QQuickItem*> &kids = rootItems();
        if (index >= 0 && index < kids.count())
            return QAccessible::queryAccessibleInterface(kids.at(index));
        return nullptr;
    }

    int childCount() const override
    {
        return rootItems().count();
    }

    int indexOfChild(const QAccessibleInterface *iface) const override
    {
        int i = -1;
        if (iface)
        {
            const QList<QQuickItem *> &roots = rootItems();
            i = roots.count() - 1;
            while (i >= 0)
            {
                if (iface->object() == roots.at(i))
                    break;
                --i;
            }
        }
        return i;
    }

    QAccessibleInterface *childAt(int x, int y) const override
    {
        for (int i = childCount() - 1; i >= 0; --i)
        {
            QAccessibleInterface *childIface = child(i);
            if (childIface && !childIface->state().invisible)
            {
                if (QAccessibleInterface *iface = childIface->childAt(x, y))
                    return iface;
                if (childIface->rect().contains(x, y))
                    return childIface;
            }
        }
        return nullptr;
    }

    QAccessibleInterface *focusChild() const override
    {
        if (!renderWindow())
            return nullptr;
        QQuickItem* item = quickWindow()->activeFocusItem();
        if (!item || item == quickWindow()->contentItem())
            return nullptr;
        return QAccessible::queryAccessibleInterface(item);
    }

    QString text(QAccessible::Text) const override
    {
        // the render window bears the title
        if (renderWindow())
            return {};
        return window()->title();
    }

    QRect rect() const override
    {
        const QWindow* shown = renderWindow();
        if (!shown)
            shown = window();
        return QRect(shown->x(), shown->y(), shown->width(), shown->height());
    }

    QAccessible::Role role() const override
    {
        return renderWindow() ? QAccessible::Pane : QAccessible::Window;
    }

    QAccessible::State state() const override
    {

        QAccessible::State st;
        if (const QWindow* renderWindow = this->renderWindow())
        {
            if (!renderWindow->isVisible())
                st.invisible = true;
            return st;
        }
        if (window() == QGuiApplication::focusWindow())
            st.active = true;
        if (!window()->isVisible())
            st.invisible = true;
        return st;
    }

private:
    QQuickWindow* quickWindow() const
    {
        return static_cast<QQuickWindow*>(object());
    }

    QWindow* window() const override
    {
        return static_cast<QWindow*>(object());
    }
};

QAccessibleInterface* compositionAccessibleFactory(const QString &classname, QObject *object)
{
    if (const auto window = qobject_cast<QWindow *>(object))
    {
        if (const auto renderWindow = dynamic_cast<AccessibleRenderWindow *>(window))
            return new QAccessibleRenderWindow(window, renderWindow);
    }

    // the offscreen window may be queried before its render window shows it
    if (classname == QLatin1String("vlc::CompositorOffscreenWindow")
        || (classname == QLatin1String("vlc::DCompOffscreenWindow")) )
    {
        return new QAccessibleOffscreenWindow(qobject_cast<QQuickWindow *>(object));
    }

    if (const auto quickWindow = qobject_cast<QQuickWindow *>(object))
    {
        if (renderWindowOf(quickWindow))
            return new QAccessibleOffscreenWindow(quickWindow);
    }

    return nullptr;
}

}

 #endif
