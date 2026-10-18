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
#include "menustyle.hpp"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QStyleOption>

#include <algorithm>
#include <cmath>

namespace {

// Figma's menu (6062:13354): 4 px around 48 px rows, 14 px in the rows,
// 24 px icons 10 px from the text, rows rounded 6, the menu 10
constexpr int PADDING = 4;
constexpr int ROW_HEIGHT = 48;
constexpr int ROW_PADDING = 14;
constexpr int ICON_SIZE = 24;
constexpr int GAP = 10;
constexpr int ROW_RADIUS = 6;
constexpr int RADIUS = 10;
constexpr int SEPARATOR = 1;
// a submenu starts 11 px right of its menu, its first row level with the
// row that opened it (6648:20666): 16 px from the row, which ends 4 px
// (the padding) before the menu, plus 1 px of Qt's QRect::right()
constexpr int SUBMENU_GAP = 16;
// the text size of the Theme's style sheet: the menu's font size over it
// is the interface scale
constexpr int FONT_SIZE = 14;

int scaled(int value, double scale)
{
    return std::max(1, static_cast<int>(std::lround(value * scale)));
}

double scaleOf(const QWidget *widget)
{
    const int pixelSize = widget ? widget->font().pixelSize() : -1;
    return pixelSize > 0 ? pixelSize / static_cast<double>(FONT_SIZE) : 1.;
}

// a 24 px Figma icon drawn as its vector: the points are in its 24 px box
void drawStroke(QPainter *painter, const QRectF &box, std::initializer_list<QPointF> points,
                const QColor &color, double width, bool mirrored)
{
    const double unit = box.width() / ICON_SIZE;
    QPainterPath path;
    bool first = true;
    for (QPointF point : points)
    {
        if (mirrored)
            point.setX(ICON_SIZE - point.x());
        const QPointF p = box.topLeft() + point * unit;
        if (first)
            path.moveTo(p);
        else
            path.lineTo(p);
        first = false;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(color, width * unit, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawPath(path);
    painter->restore();
}

// "chevron", direction right: 1.5 px, FG/A2
void drawChevron(QPainter *painter, const QRectF &box, const QColor &color, bool rightToLeft)
{
    drawStroke(painter, box, { { 10, 7 }, { 15, 12 }, { 10, 17 } }, color, 1.5, rightToLeft);
}

// "check": 2 px, Accent/Main
void drawCheck(QPainter *painter, const QRectF &box, const QColor &color)
{
    drawStroke(painter, box, { { 5, 11.5 }, { 9.66667, 17 }, { 19, 6 } }, color, 2., false);
}

QRegion roundedRegion(const QRect &rect, int radius)
{
    radius = std::min({ radius, rect.width() / 2, rect.height() / 2 });
    if (radius <= 0)
        return QRegion(rect);

    const int d = 2 * radius;
    QRegion region(rect.adjusted(radius, 0, -radius, 0));
    region += QRegion(rect.adjusted(0, radius, 0, -radius));
    region += QRegion(QRect(rect.left(), rect.top(), d, d), QRegion::Ellipse);
    region += QRegion(QRect(rect.right() - d + 1, rect.top(), d, d), QRegion::Ellipse);
    region += QRegion(QRect(rect.left(), rect.bottom() - d + 1, d, d), QRegion::Ellipse);
    region += QRegion(QRect(rect.right() - d + 1, rect.bottom() - d + 1, d, d), QRegion::Ellipse);
    return region;
}

}

MenuStyle::MenuStyle(QStyle *base)
    : QProxyStyle(base)
{}

MenuStyle::CheckPlace MenuStyle::checkPlace(bool checkable, bool hasIcon, bool menuHasIcons)
{
    if (!checkable)
        return CheckPlace::None;
    if (menuHasIcons && !hasIcon)
        return CheckPlace::Leading;
    return CheckPlace::Trailing;
}

QString MenuStyle::keyText(const QString &shortcut)
{
    // "Ctrl++": the last "+" is the key
    QStringList keys;
    QString key;
    for (qsizetype i = 0; i < shortcut.size(); ++i)
    {
        const QChar c = shortcut.at(i);
        if (c == QLatin1Char('+') && !key.isEmpty())
        {
            keys.append(key);
            key.clear();
            continue;
        }
        key.append(c);
    }
    if (!key.isEmpty())
        keys.append(key);
    return keys.join(QLatin1String(" + "));
}

QString MenuStyle::labelText(const QString &label)
{
    QString text = label;
    if (text.endsWith(QLatin1String("...")))
        text.chop(3);
    else if (text.endsWith(QChar(0x2026)))
        text.chop(1);
    return text;
}

MenuStyle::Metrics MenuStyle::metrics(double scale)
{
    return {
        scaled(PADDING, scale),
        scaled(ROW_HEIGHT, scale),
        scaled(ROW_PADDING, scale),
        scaled(ICON_SIZE, scale),
        scaled(GAP, scale),
        scaled(ROW_RADIUS, scale),
        scaled(RADIUS, scale),
        scaled(SEPARATOR, scale),
        scaled(SUBMENU_GAP, scale),
    };
}

int MenuStyle::width(const QWidget *menu)
{
    const QString name = menu ? menu->objectName() : QString();
    if (name == QLatin1String("vlcMenu"))
        return 375;
    if (name == QLatin1String("playerMenu"))
        return 292;
    return 265;
}

bool MenuStyle::draws(const QWidget *widget)
{
    // the Theme's style sheet gives the menus their colors: without it (High
    // Contrast) they are the platform's
    return qobject_cast<const QMenu *>(widget) && qApp && !qApp->styleSheet().isEmpty();
}

int MenuStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
                           const QWidget *widget) const
{
    if (!draws(widget))
        return QProxyStyle::pixelMetric(metric, option, widget);

    const Metrics m = metrics(scaleOf(widget));
    switch (metric)
    {
    case PM_MenuPanelWidth:
        return 0;
    case PM_MenuHMargin:
    case PM_MenuVMargin:
        return m.padding;
    case PM_SmallIconSize:
        return m.iconSize;
    case PM_SubMenuOverlap:
        return m.submenuGap;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int MenuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                         QStyleHintReturn *returnData) const
{
    if (!draws(widget))
        return QProxyStyle::styleHint(hint, option, widget, returnData);

    switch (hint)
    {
    case SH_Menu_Mask:
        // the rounded corners: the menu window is cut to them
        if (auto *mask = qstyleoption_cast<QStyleHintReturnMask *>(returnData))
        {
            const QRect rect = option ? option->rect : widget->rect();
            mask->region = roundedRegion(rect, metrics(scaleOf(widget)).radius);
            return 1;
        }
        return 0;
    case SH_UnderlineShortcut:
        // Figma shows no mnemonic: the keys still work
        return 0;
    case SH_Menu_SupportsSections:
        return 1;
    default:
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
}

QSize MenuStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                  const QSize &size, const QWidget *widget) const
{
    const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (type != CT_MenuItem || !item || !draws(widget))
        return QProxyStyle::sizeFromContents(type, option, size, widget);

    const Metrics m = metrics(scaleOf(widget));

    if (item->menuItemType == QStyleOptionMenuItem::Separator && item->text.isEmpty())
        return QSize(size.width(), m.separator);
    if (item->menuItemType == QStyleOptionMenuItem::Separator)
        return QSize(2 * m.rowPadding + size.width(), m.rowHeight);

    const bool menuHasIcons = item->maxIconWidth > 0;
    int width = 2 * m.rowPadding + size.width();
    if (menuHasIcons)
        width += m.iconSize + m.gap;

    // the shortcut: QMenu adds the widest one as it measures it; Figma's
    // spaced "+" make it wider
    const qsizetype tab = item->text.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
    {
        const QString key = item->text.mid(tab + 1);
        width += m.gap + item->fontMetrics.horizontalAdvance(keyText(key))
                 - item->fontMetrics.horizontalAdvance(key);
    }

    // the chevron, or a check at the end
    const bool checkable = item->checkType != QStyleOptionMenuItem::NotCheckable;
    if (item->menuItemType == QStyleOptionMenuItem::SubMenu
        || checkPlace(checkable, !item->icon.isNull(), menuHasIcons) == CheckPlace::Trailing)
        width += m.gap + m.iconSize;

    // Figma's width: QMenu makes its column as wide as the widest entry,
    // plus the widest shortcut as it measures it
    int widestKey = 0;
    if (const auto *menu = qobject_cast<const QMenu *>(widget))
    {
        const QFontMetrics fm = menu->fontMetrics();
        for (const QAction *action : menu->actions())
        {
            if (!action->isVisible() || action->isSeparator())
                continue;
            const QString text = action->text();
            const qsizetype t = text.indexOf(QLatin1Char('\t'));
            if (t >= 0)
                widestKey = std::max(widestKey, fm.horizontalAdvance(text.mid(t + 1)));
            else if (!action->shortcut().isEmpty())
                widestKey = std::max(widestKey, fm.horizontalAdvance(
                    action->shortcut().toString(QKeySequence::NativeText)));
        }
    }
    const double scale = scaleOf(widget);
    width = std::max(width, scaled(MenuStyle::width(widget), scale) - 2 * m.padding - widestKey);

    return QSize(width, m.rowHeight);
}

void MenuStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                              QPainter *painter, const QWidget *widget) const
{
    if (!draws(widget))
        return QProxyStyle::drawPrimitive(element, option, painter, widget);

    switch (element)
    {
    case PE_PanelMenu:
        // BG/A3 Transparent over the window (the style sheet's
        // alternate-background-color); the mask rounds the corners
        painter->fillRect(option->rect, option->palette.color(QPalette::AlternateBase));
        return;
    case PE_FrameMenu:
        // no border in Figma
        return;
    default:
        return QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
}

void MenuStyle::drawControl(ControlElement element, const QStyleOption *option,
                            QPainter *painter, const QWidget *widget) const
{
    if (!draws(widget))
        return QProxyStyle::drawControl(element, option, painter, widget);

    switch (element)
    {
    case CE_MenuItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option))
            drawMenuItem(item, painter, widget);
        return;
    case CE_MenuEmptyArea:
        // PE_PanelMenu filled it
        return;
    default:
        return QProxyStyle::drawControl(element, option, painter, widget);
    }
}

void MenuStyle::drawMenuItem(const QStyleOptionMenuItem *item, QPainter *painter,
                             const QWidget *widget) const
{
    const Metrics m = metrics(scaleOf(widget));
    const QRect rect = item->rect;
    const bool rightToLeft = item->direction == Qt::RightToLeft;

    // the style sheet's colors: QMenu::item and its :disabled state set the
    // text (ButtonText), QMenu the rest
    const QPalette &palette = item->palette;
    const QColor text = palette.color(QPalette::ButtonText);
    const QColor shortcut = (item->state & State_Enabled)
        ? palette.color(QPalette::PlaceholderText) : text;
    // Figma centres the line box, rounding half a pixel down the row (Qt
    // would round it up): the text sits in a line box of its own
    const int textFlags = Qt::AlignTop | Qt::TextSingleLine | Qt::TextHideMnemonic;
    const int lineHeight = item->fontMetrics.height();
    const int textTop = rect.top() + static_cast<int>(std::lround((rect.height() - lineHeight) / 2.));

    painter->save();
    painter->setFont(item->font);

    if (item->menuItemType == QStyleOptionMenuItem::Separator)
    {
        if (item->text.isEmpty())
        {
            // the style sheet's QMenu::separator draws it, else FG/A2 faintly
            QColor line = text;
            line.setAlphaF(0.25f);
            painter->fillRect(QRect(rect.left(), rect.top(), rect.width(), m.separator), line);
        }
        else
        {
            // a section title: a label (FG/A3) at the text's place
            QRect textRect(rect.left() + m.rowPadding, textTop, rect.width() - 2 * m.rowPadding, lineHeight);
            if (item->maxIconWidth > 0)
                textRect.adjust(m.iconSize + m.gap, 0, 0, 0);
            painter->setPen(palette.color(QPalette::PlaceholderText));
            painter->drawText(visualRect(item->direction, rect, textRect),
                              textFlags | Qt::AlignLeft, item->text);
        }
        painter->restore();
        return;
    }

    const bool enabled = item->state & State_Enabled;
    const bool hovered = enabled && (item->state & State_Selected);
    const bool checkable = item->checkType != QStyleOptionMenuItem::NotCheckable;
    const bool checked = checkable && item->checked;
    const bool menuHasIcons = item->maxIconWidth > 0;
    const CheckPlace place = checkPlace(checkable, !item->icon.isNull(), menuHasIcons);

    // BG/A2 under the hovered row and the checked one
    if (hovered || checked)
    {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.color(QPalette::Highlight));
        painter->drawRoundedRect(QRectF(rect), m.rowRadius, m.rowRadius);
        painter->setRenderHint(QPainter::Antialiasing, false);
    }

    const QColor checkColor = enabled ? palette.color(QPalette::Accent) : text;
    const int iconTop = rect.top() + (rect.height() - m.iconSize) / 2;
    int left = rect.left() + m.rowPadding;
    int right = rect.right() + 1 - m.rowPadding;

    // the icon column
    if (menuHasIcons)
    {
        const QRect iconRect = visualRect(item->direction, rect,
                                          QRect(left, iconTop, m.iconSize, m.iconSize));
        if (!item->icon.isNull())
        {
            const QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
            item->icon.paint(painter, iconRect, Qt::AlignCenter, mode,
                             checked ? QIcon::On : QIcon::Off);
        }
        else if (checked && place == CheckPlace::Leading)
        {
            drawCheck(painter, iconRect, checkColor);
        }
        left += m.iconSize + m.gap;
    }

    // the end of the row: the chevron of a submenu, or the check
    const QRect endRect = visualRect(item->direction, rect,
                                     QRect(right - m.iconSize, iconTop, m.iconSize, m.iconSize));
    if (item->menuItemType == QStyleOptionMenuItem::SubMenu)
    {
        drawChevron(painter, endRect, text, rightToLeft);
        right -= m.iconSize + m.gap;
    }
    else if (checked && place == CheckPlace::Trailing)
    {
        drawCheck(painter, endRect, checkColor);
        right -= m.iconSize + m.gap;
    }

    // the label, then the shortcut, right-aligned, in FG/A3
    QString label = item->text;
    const qsizetype tab = label.indexOf(QLatin1Char('\t'));
    if (tab >= 0)
    {
        const QString key = keyText(label.mid(tab + 1));
        label.truncate(tab);

        const QRect keyRect = visualRect(item->direction, rect,
                                         QRect(left, textTop, right - left, lineHeight));
        painter->setPen(shortcut);
        painter->drawText(keyRect, textFlags | Qt::AlignRight, key);
        right -= item->fontMetrics.horizontalAdvance(key) + m.gap;
    }

    const QRect labelRect = visualRect(item->direction, rect,
                                       QRect(left, textTop, std::max(0, right - left), lineHeight));
    painter->setPen(text);
    painter->drawText(labelRect, textFlags | Qt::AlignLeft,
                      item->fontMetrics.elidedText(labelText(label), Qt::ElideRight, labelRect.width(),
                                                   Qt::TextShowMnemonic));

    painter->restore();
}
