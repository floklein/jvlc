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
#include <memory>

#include <QtTest>
#include <QAccessible>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>
#include <QtPlugin>

#include <private/qquickitem_p.h>

#include "../maininterface/compositor_accessibility.hpp"

#ifdef QT_STATIC
// static Qt (Windows contribs): import the plugins the test needs
QT_BEGIN_NAMESPACE
#include "../plugins.hpp"
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
QT_END_NAMESPACE
#endif

// The X11 compositor draws the interface of an offscreen window into the
// window the user sees, the render window. A stand-in for it.
class RenderWindow : public QWindow, public vlc::AccessibleRenderWindow
{
public:
    QQuickWindow* getOffscreenWindow() const override { return m_offscreenWindow; }

    QQuickWindow* m_offscreenWindow = nullptr;
};

// the window the interface is drawn in
class OffscreenWindow : public QQuickWindow
{
    Q_OBJECT
};

// Screen readers (Orca) find the window of the focused item by walking up
// from the item, and only present the items of the active window (spec #30,
// WCAG 2.2 AA, Orca pass of the accessibility verification).
class TestCompositorAccessibility : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<RenderWindow> m_renderWindow;
    std::unique_ptr<QWindow> m_interfaceSurface;
    std::unique_ptr<OffscreenWindow> m_offscreenWindow;
    QQuickItem* m_button = nullptr;

private slots:
    void initTestCase()
    {
        QAccessible::installFactory(&vlc::compositionAccessibleFactory);
    }

    void init()
    {
        m_renderWindow = std::make_unique<RenderWindow>();
        m_renderWindow->setTitle(QStringLiteral("jVLC"));
        m_renderWindow->resize(640, 480);

        m_offscreenWindow = std::make_unique<OffscreenWindow>();
        m_renderWindow->m_offscreenWindow = m_offscreenWindow.get();

        // a root item of the interface, and a button in it
        auto root = new QQuickItem(m_offscreenWindow->contentItem());
        QQuickItemPrivate::get(root)->setAccessible();
        m_button = new QQuickItem(root);
        QQuickItemPrivate::get(m_button)->setAccessible();

        // the surface the keys go to, a child window of the render window
        m_interfaceSurface = std::make_unique<QWindow>(m_renderWindow.get());
    }

    void cleanup()
    {
        m_interfaceSurface.reset();
        m_offscreenWindow.reset();
        m_renderWindow.reset();
        m_button = nullptr;
    }

    // the window of every item of the interface is the render window
    void testItemsLeadUpToTheRenderWindow()
    {
        QAccessibleInterface* render = QAccessible::queryAccessibleInterface(m_renderWindow.get());
        QVERIFY(render);
        QCOMPARE(render->role(), QAccessible::Window);
        QCOMPARE(render->text(QAccessible::Name), QStringLiteral("jVLC"));

        QAccessibleInterface* child = QAccessible::queryAccessibleInterface(m_button);
        QVERIFY(child);
        QAccessibleInterface* parent = child->parent();
        int depth = 0;
        while (parent && parent->object() != m_renderWindow.get())
        {
            // each step is a child its parent knows
            QVERIFY2(parent->indexOfChild(child) >= 0, "an item is not a child of its parent");
            child = parent;
            parent = parent->parent();
            QVERIFY(++depth < 10);
        }
        QVERIFY2(parent, "the items do not lead up to the render window");
        QCOMPARE(parent->indexOfChild(child), 0);
        QCOMPARE(parent->parent()->object(), qApp);
    }

    // and the render window lists them, once
    void testRenderWindowHasTheInterface()
    {
        QAccessibleInterface* render = QAccessible::queryAccessibleInterface(m_renderWindow.get());
        QCOMPARE(render->childCount(), 1);
        QAccessibleInterface* pane = render->child(0);
        QVERIFY(pane);
        QCOMPARE(pane->object(), m_offscreenWindow.get());
        QVERIFY(pane->role() != QAccessible::Window);
        QCOMPARE(pane->childCount(), 1);
        QCOMPARE(pane->child(0)->child(0)->object(), m_button);
    }

    // the keys go to the interface surface: the render window is the active one
    void testRenderWindowIsActiveWithItsInterfaceSurface()
    {
        m_renderWindow->show();
        m_interfaceSurface->show();
        m_interfaceSurface->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), m_interfaceSurface.get());

        QAccessibleInterface* render = QAccessible::queryAccessibleInterface(m_renderWindow.get());
        QVERIFY(render->state().active);
        QVERIFY(!render->state().invisible);
    }
};

int main(int argc, char **argv)
{
    // run offscreen, as the CI has no desktop environment
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestCompositorAccessibility test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "test_compositor_accessibility.moc"
