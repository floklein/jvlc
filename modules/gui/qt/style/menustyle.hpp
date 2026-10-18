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
#ifndef VLC_QT_MENUSTYLE_HPP
#define VLC_QT_MENUSTYLE_HPP

#include <QProxyStyle>
#include <QString>

/**
 * Draws the native menus (QMenu) as Figma's menus (6062:13354, 6065:13616,
 * 856:2215, 6648:20666, 858:3290): the rows, the icon column, the
 * right-aligned shortcut column, the submenu chevron and the check, on every
 * platform, whatever the native style would draw.
 *
 * It is the application style, over the platform style: everything that is
 * not a menu goes to the platform style. The Theme's style sheet
 * (menuStyleSheet()) only gives the menus their colors, font and width, so
 * that the style sheet code hands the drawing over to this style. Without
 * that style sheet (High Contrast), the menus are the platform's.
 */
class MenuStyle : public QProxyStyle
{
public:
    explicit MenuStyle(QStyle *base = nullptr);

    /**
     * Where a check mark goes. Figma puts it in the icon column of a menu
     * with icons, in place of the missing icon (Profile ▸, 6648:20666), and
     * at the end of the row of a menu without icons (sorting, 856:2215) or
     * of an entry that has its own icon (View Options, 858:3290).
     */
    enum class CheckPlace { None, Leading, Trailing };
    static CheckPlace checkPlace(bool checkable, bool hasIcon, bool menuHasIcons);

    /**
     * A shortcut as Figma writes it: "Ctrl + Shift + O" for "Ctrl+Shift+O"
     * (a "+" key stays: "Ctrl + +")
     */
    static QString keyText(const QString &shortcut);

    /**
     * An entry's label as Figma writes it: no "..." or "…" at the end
     * ("Open File(s)..." is "Open File(s)")
     */
    static QString labelText(const QString &label);

    /** Figma's sizes of a menu (in device independent pixels at scale 1) */
    struct Metrics
    {
        int padding;      // around the rows, inside the menu
        int rowHeight;
        int rowPadding;   // left and right of a row's content
        int iconSize;     // icons, chevron and check
        int gap;          // between the icon, the text, the shortcut and the chevron
        int rowRadius;    // hovered and checked rows
        int radius;       // the menu
        int separator;    // line height
        int submenuGap;   // from a row's right edge to its submenu
    };
    static Metrics metrics(double scale);

    /**
     * Figma's width of a menu, at scale 1: 375 for the VLC menu (object
     * name "vlcMenu", 6062:13354), 292 for the player's right-click menu
     * ("playerMenu", 6065:13616), 265 for the others (item, sorting and
     * submenus: 2404:13340, 856:2215, 6648:20666). Longer entries widen it.
     */
    static int width(const QWidget *menu);

    /** whether the menu widget is drawn by this style */
    static bool draws(const QWidget *widget);

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option,
                           const QSize &size, const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget = nullptr) const override;

private:
    void drawMenuItem(const class QStyleOptionMenuItem *item, QPainter *painter,
                      const QWidget *widget) const;
};

#endif // VLC_QT_MENUSTYLE_HPP
