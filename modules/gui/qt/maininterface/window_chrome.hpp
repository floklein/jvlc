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
#ifndef VLC_QT_WINDOW_CHROME_HPP
#define VLC_QT_WINDOW_CHROME_HPP

#include <functional>

#include <QRect>
#include <QSize>
#include <QString>

class QObject;

// The window chrome of the redesign: the window title, the side of the
// window buttons and the Picture-in-picture geometry.
namespace WindowChrome
{

// the taskbar and window switcher title: "Title · jVLC" while something
// plays, "jVLC" otherwise
QString windowTitle( const QString &mediaTitle );

// the side of the title bar holding the window buttons
enum class ButtonsSide
{
    Right,
    Left,
};

// from GNOME's button-layout setting, "appmenu:minimize,maximize,close"
ButtonsSide buttonsSideFromGnomeLayout( const QString &layout );

// from KDE's ButtonsOnLeft and ButtonsOnRight settings, "X" being close
ButtonsSide buttonsSideFromKdeButtons( const QString &onLeft, const QString &onRight );

// the side the desktop uses, read from its settings (Linux only, right
// elsewhere); GNOME asks gsettings, a process: the answer comes later, to
// <done> in the thread of <context>, never blocking it
void queryDesktopButtonsSide( QObject *context, std::function<void( ButtonsSide )> done );

// the Picture-in-picture window in the available area of a screen: the saved
// geometry when it lies on that screen, else about 480 px wide at the video's
// aspect ratio (16:9 without video) in the bottom-right corner
QRect pictureInPictureGeometry( const QRect &available, const QSize &videoSize,
                                const QRect &saved );

}

#endif // VLC_QT_WINDOW_CHROME_HPP
