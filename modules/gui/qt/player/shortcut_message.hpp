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

#ifndef SHORTCUT_MESSAGE_HPP
#define SHORTCUT_MESSAGE_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_player.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>

/**
 * @brief The Shortcut message: what the last action did ("Subtitle delay:
 * 50 ms"), followed by the key that ran it ("· H"). Actions from menus and
 * buttons show their result alone.
 *
 * The results are the player's OSD messages (vlc_player_osd_AddListener()).
 * The video outputs started while this object exists don't draw them.
 */
class ShortcutMessage : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString text READ text NOTIFY shown FINAL)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged FINAL)

public:
    /**
     * @brief Marks a key press while the core handles it, so that the
     * messages it causes name the key. Lives on the stack of the thread
     * that sends the key to the core.
     */
    class KeyScope
    {
    public:
        explicit KeyScope(uint_fast32_t vlcKey);
        ~KeyScope();

        KeyScope(const KeyScope&) = delete;
        KeyScope& operator=(const KeyScope&) = delete;

    private:
        uint_fast32_t m_previous;
    };

    explicit ShortcutMessage(vlc_player_t *player, QObject *parent = nullptr);
    ~ShortcutMessage();

    QString text() const { return m_text; }
    bool isVisible() const { return m_visible; }

    /// how long a message stays, in milliseconds
    int duration() const;

    /// "Show shortcut keys in on-screen messages"
    void setShowKeys(bool show) { m_showKeys = show; }

    /// no message is shown while disabled ("osd" option off)
    void setEnabled(bool enabled) { m_enabled = enabled; }

signals:
    /// a new message, even when its text did not change
    void shown(const QString &text);
    void visibleChanged();

private:
    static void onOsdMessage(vlc_player_t *player, const char *text, void *data);

    void show(const QString &result, uint_fast32_t vlcKey);
    void setVisible(bool visible);

    vlc_player_t *m_player;
    vlc_player_osd_listener_id *m_osdListener = nullptr;
    vlc_player_listener_id *m_playerListener = nullptr;
    QTimer m_lifetime;
    QString m_text;
    bool m_visible = false;
    bool m_showKeys = true;
    bool m_enabled = true;
};

/**
 * @brief the key of a Shortcut message: "H", "Space", "Ctrl+↑", or an
 * empty string for what is not a keyboard key (mouse wheel)
 */
QString shortcutKeyText(uint_fast32_t vlcKey);

#endif // SHORTCUT_MESSAGE_HPP
