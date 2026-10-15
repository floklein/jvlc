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
#include "window_chrome.hpp"

#include <algorithm>

#include <QDir>
#include <QProcess>
#include <QSettings>

namespace WindowChrome
{

QString windowTitle( const QString &mediaTitle )
{
    static const QString app = QStringLiteral( "VLC" );
    if( mediaTitle.isEmpty() )
        return app;
    return mediaTitle + QStringLiteral( " · " ) + app;
}

ButtonsSide buttonsSideFromGnomeLayout( const QString &layout )
{
    // "'appmenu:minimize,maximize,close'": the buttons before the colon are
    // on the left, close tells the side, else the other window buttons
    QString value = layout.trimmed();
    value.remove( QLatin1Char( '\'' ) );

    const QStringList sides = value.split( QLatin1Char( ':' ) );
    const QString left = sides.value( 0 );
    const QString right = sides.value( 1 );

    const auto hasButton = []( const QString &side, const char *name ) {
        return side.split( QLatin1Char( ',' ) ).contains( QLatin1String( name ) );
    };

    for( const char *button : { "close", "maximize", "minimize" } )
    {
        if( hasButton( left, button ) )
            return ButtonsSide::Left;
        if( hasButton( right, button ) )
            return ButtonsSide::Right;
    }
    return ButtonsSide::Right;
}

ButtonsSide buttonsSideFromKdeButtons( const QString &onLeft, const QString &onRight )
{
    // X close, A maximize, I minimize
    for( const QChar button : { QLatin1Char( 'X' ), QLatin1Char( 'A' ), QLatin1Char( 'I' ) } )
    {
        if( onLeft.contains( button ) )
            return ButtonsSide::Left;
        if( onRight.contains( button ) )
            return ButtonsSide::Right;
    }
    return ButtonsSide::Right;
}

ButtonsSide desktopButtonsSide()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
    const QString desktop = qEnvironmentVariable( "XDG_CURRENT_DESKTOP" );

    if( desktop.contains( QLatin1String( "KDE" ), Qt::CaseInsensitive ) )
    {
        QString dir = qEnvironmentVariable( "XDG_CONFIG_HOME" );
        if( dir.isEmpty() )
            dir = QDir::homePath() + QLatin1String( "/.config" );
        QSettings kwin( dir + QLatin1String( "/kwinrc" ), QSettings::IniFormat );
        kwin.beginGroup( QStringLiteral( "org.kde.kdecoration2" ) );
        return buttonsSideFromKdeButtons(
                    kwin.value( QStringLiteral( "ButtonsOnLeft" ), QStringLiteral( "MS" ) ).toString(),
                    kwin.value( QStringLiteral( "ButtonsOnRight" ), QStringLiteral( "HIAX" ) ).toString() );
    }

    static const char *const gnomeLike[] = { "GNOME", "Unity", "Budgie", "Pantheon", "Pop" };
    for( const char *name : gnomeLike )
    {
        if( !desktop.contains( QLatin1String( name ), Qt::CaseInsensitive ) )
            continue;

        QProcess gsettings;
        gsettings.start( QStringLiteral( "gsettings" ),
                         { QStringLiteral( "get" ),
                           QStringLiteral( "org.gnome.desktop.wm.preferences" ),
                           QStringLiteral( "button-layout" ) } );
        if( !gsettings.waitForFinished( 1000 ) || gsettings.exitCode() != 0 )
        {
            gsettings.kill();
            gsettings.waitForFinished( 100 );
            return ButtonsSide::Right;
        }
        return buttonsSideFromGnomeLayout( QString::fromUtf8( gsettings.readAllStandardOutput() ) );
    }
#endif
    return ButtonsSide::Right;
}

QRect pictureInPictureGeometry( const QRect &available, const QSize &videoSize,
                                const QRect &saved )
{
    if( saved.isValid() && available.intersects( saved ) )
    {
        // keep it whole on the screen
        QRect rect = saved;
        rect.setSize( rect.size().boundedTo( available.size() ) );
        rect.moveLeft( std::clamp( rect.left(), available.left(), available.right() - rect.width() + 1 ) );
        rect.moveTop( std::clamp( rect.top(), available.top(), available.bottom() - rect.height() + 1 ) );
        return rect;
    }

    constexpr int width = 480;
    constexpr int margin = 16;

    const double aspect = ( videoSize.width() > 0 && videoSize.height() > 0 )
                          ? double( videoSize.width() ) / videoSize.height()
                          : 16.0 / 9.0;

    QSize size( width, qRound( width / aspect ) );
    // a tall video stays within half the screen height
    const int maxHeight = available.height() / 2;
    if( size.height() > maxHeight )
        size = QSize( qRound( maxHeight * aspect ), maxHeight );

    return QRect( QPoint( available.right() + 1 - margin - size.width(),
                          available.bottom() + 1 - margin - size.height() ),
                  size );
}

}
