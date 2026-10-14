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

#include "glyph_icon.hpp"

#include "qt.hpp"
#include "style/vlcicons.hpp"
#include "style/themetokens.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

namespace {

class GlyphIconEngine : public QIconEngine
{
public:
    GlyphIconEngine( QChar base, QColor baseColor, QChar accent, QColor accentColor )
        : m_base( base ), m_baseColor( baseColor )
        , m_accent( accent ), m_accentColor( accentColor )
    {}

    void paint( QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State ) override
    {
        QFont font( VLCIcons::fontFamily );
        font.setPixelSize( std::min( rect.width(), rect.height() ) );

        painter->save();
        painter->setFont( font );
        painter->setRenderHint( QPainter::TextAntialiasing );
        if( mode == QIcon::Disabled )
            painter->setOpacity( 0.4 );

        const auto draw = [&]( QChar glyph, const QColor &color ) {
            if( glyph.isNull() )
                return;
            painter->setPen( color );
            painter->drawText( rect, Qt::AlignCenter, QString( glyph ) );
        };
        draw( m_base, m_baseColor );
        draw( m_accent, m_accentColor );

        painter->restore();
    }

    QPixmap pixmap( const QSize &size, QIcon::Mode mode, QIcon::State state ) override
    {
        return scaledPixmap( size, mode, state, 1.0 );
    }

    QPixmap scaledPixmap( const QSize &size, QIcon::Mode mode, QIcon::State state,
                          qreal scale ) override
    {
        QPixmap pixmap( size * scale );
        pixmap.fill( Qt::transparent );
        {
            QPainter painter( &pixmap );
            paint( &painter, QRect( QPoint(), pixmap.size() ), mode, state );
        }
        pixmap.setDevicePixelRatio( scale );
        return pixmap;
    }

    QIconEngine *clone() const override
    {
        return new GlyphIconEngine( m_base, m_baseColor, m_accent, m_accentColor );
    }

    QString key() const override
    {
        return QStringLiteral( "vlcglyph" );
    }

private:
    QChar m_base;
    QColor m_baseColor;
    QChar m_accent;
    QColor m_accentColor;
};

void registerIconFont()
{
    static const bool registered = []() {
        if( QFontDatabase::families().contains( VLCIcons::fontFamily ) )
            return true;
        return QFontDatabase::addApplicationFont( QStringLiteral( ":/VLCIcons.ttf" ) ) != -1;
    }();
    Q_UNUSED( registered );
}

}

QColor accentColorFor( const QColor &background )
{
    const bool dark = background.lightness() < 128;

    // the Accent color option, read as SystemPalette does
    QColor userAccent;
    char *option = config_GetPsz( "qt-accent-color" );
    if( option )
    {
        if( !strcmp( option, "system" ) )
            userAccent = qApp->palette().color( QPalette::Normal, QPalette::Accent );
        else if( strcmp( option, "default" ) )
            userAccent = QColor( option );
        free( option );
    }

    return makeThemeTokens( dark, userAccent ).accentMain;
}

QIcon glyphIcon( QChar base, QChar accent, const QWidget *widget )
{
    registerIconFont();

    const QPalette palette = widget ? widget->palette() : qApp->palette();
    const QColor background = palette.color( QPalette::Window );

    QColor baseColor = palette.color( QPalette::WindowText );
    baseColor.setAlphaF( 0.7f );

    return QIcon( new GlyphIconEngine( base, baseColor, accent, accentColorFor( background ) ) );
}
