/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#include <QtTest>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSGSimpleRectNode>
#include <QtPlugin>

#include "../maininterface/videoblur.hpp"

#ifdef QT_STATIC
// static Qt (Windows contribs): import the plugins the test needs
QT_BEGIN_NAMESPACE
#include "../plugins.hpp"
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
QT_END_NAMESPACE
#endif

// A panel over the video, painted with its Tint
class Panel : public QQuickItem
{
public:
    explicit Panel(QQuickItem *parent) : QQuickItem(parent)
    {
        setFlag(ItemHasContents);
    }

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *) override
    {
        auto rect = static_cast<QSGSimpleRectNode *>(node);
        if (!rect)
            rect = new QSGSimpleRectNode(boundingRect(), QColor(0, 0, 0, 190));
        rect->setRect(boundingRect());
        return rect;
    }
};

// A panel over the video reports where the Video blur shows, in device
// pixels of the scene, and withdraws the region when it is not shown.
class TestVideoBlur : public QObject
{
    Q_OBJECT

private:
    struct Regions
    {
        // key -> last reported region; a removed key is absent
        QMap<quintptr, VideoBlurProvider::Region> shown;
    };

    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<VideoBlurProvider> m_provider;
    Regions m_regions;
    QQuickItem *m_panel = nullptr;
    VideoBlur *m_blur = nullptr;

private slots:
    void init()
    {
        m_window = std::make_unique<QQuickWindow>();
        m_window->resize(640, 480);
        m_provider = std::make_unique<VideoBlurProvider>();
        m_regions = {};

        connect(m_provider.get(), &VideoBlurProvider::regionChanged, this,
                [this](quintptr key, const VideoBlurProvider::Region &region) { m_regions.shown[key] = region; });
        connect(m_provider.get(), &VideoBlurProvider::regionRemoved, this,
                [this](quintptr key) { m_regions.shown.remove(key); });

        m_panel = new Panel(m_window->contentItem());
        m_panel->setPosition({10., 400.});
        m_panel->setSize({620., 70.});

        m_blur = new VideoBlur(m_panel);
        m_blur->setPosition({5., 5.});
        m_blur->setSize({100., 40.});
        m_blur->setRadius(8.);

        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window.get()));
    }

    void cleanup()
    {
        m_window.reset();
        m_provider.reset();
    }

    void reportsItsSceneRectangleOnceItHasAProvider()
    {
        QVERIFY(m_regions.shown.isEmpty());

        m_blur->setProvider(m_provider.get());

        QCOMPARE(m_regions.shown.size(), 1);
        const qreal dpr = m_window->effectiveDevicePixelRatio();
        const VideoBlurProvider::Region region = m_regions.shown.first();
        QCOMPARE(region.rect, QRectF(15. * dpr, 405. * dpr, 100. * dpr, 40. * dpr));
        QCOMPARE(region.radius, 8. * dpr);
    }

    void followsThePanelWhenItMoves()
    {
        m_blur->setProvider(m_provider.get());

        m_panel->setY(300.);

        const qreal dpr = m_window->effectiveDevicePixelRatio();
        QTRY_COMPARE(m_regions.shown.first().rect.top(), 305. * dpr);
    }

    void withdrawsWhenThePanelHides()
    {
        m_blur->setProvider(m_provider.get());
        QCOMPARE(m_regions.shown.size(), 1);

        m_panel->setVisible(false);
        QTRY_VERIFY(m_regions.shown.isEmpty());

        m_panel->setVisible(true);
        QTRY_COMPARE(m_regions.shown.size(), 1);
    }

    void withdrawsWhenThePanelIsFullyTransparent()
    {
        m_blur->setProvider(m_provider.get());

        m_panel->setOpacity(0.);
        QTRY_VERIFY(m_regions.shown.isEmpty());

        m_panel->setOpacity(.5);
        QTRY_COMPARE(m_regions.shown.size(), 1);
    }

    void withdrawsWhenDestroyed()
    {
        m_blur->setProvider(m_provider.get());
        QCOMPARE(m_regions.shown.size(), 1);

        delete m_blur;
        m_blur = nullptr;
        QVERIFY(m_regions.shown.isEmpty());
    }
};

int main(int argc, char **argv)
{
    // run offscreen, as the CI has no desktop environment
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestVideoBlur test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "test_videoblur.moc"
