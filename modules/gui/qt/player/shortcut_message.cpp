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

#include "shortcut_message.hpp"

#include <vlc_variables.h>
#include <vlc_actions.h>
#include <vlc_vout.h>

#include "util/vlchotkeyconverter.hpp"

namespace {

/* the pill stays 1.5 s */
constexpr int messageDuration = 1500;

/* the key being handled by the core on this thread, KEY_UNSET if none */
thread_local uint_fast32_t currentKey = KEY_UNSET;

/* the messages are shown by the interface, not over the video */
void onVoutChanged(vlc_player_t *, enum vlc_player_vout_action action,
                   vout_thread_t *vout, enum vlc_vout_order, vlc_es_id_t *,
                   void *)
{
    if (action != VLC_PLAYER_VOUT_STARTED)
        return;
    var_Create(vout, "osd", VLC_VAR_BOOL);
    var_SetBool(vout, "osd", false);
}

}

ShortcutMessage::KeyScope::KeyScope(uint_fast32_t vlcKey)
    : m_previous(currentKey)
{
    currentKey = vlcKey;
}

ShortcutMessage::KeyScope::~KeyScope()
{
    currentKey = m_previous;
}

ShortcutMessage::ShortcutMessage(vlc_player_t *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    m_lifetime.setSingleShot(true);
    m_lifetime.setInterval(messageDuration);
    connect(&m_lifetime, &QTimer::timeout, this, [this]() { setVisible(false); });

    static const struct vlc_player_osd_cbs osd_cbs = [] {
        struct vlc_player_osd_cbs cbs {};
        cbs.on_message = onOsdMessage;
        return cbs;
    }();
    m_osdListener = vlc_player_osd_AddListener(m_player, &osd_cbs, this);

    static const struct vlc_player_cbs player_cbs = [] {
        struct vlc_player_cbs cbs {};
        cbs.on_vout_changed = onVoutChanged;
        return cbs;
    }();
    vlc_player_Lock(m_player);
    m_playerListener = vlc_player_AddListener(m_player, &player_cbs, this);
    vlc_player_Unlock(m_player);
}

ShortcutMessage::~ShortcutMessage()
{
    if (m_playerListener)
    {
        vlc_player_Lock(m_player);
        vlc_player_RemoveListener(m_player, m_playerListener);
        vlc_player_Unlock(m_player);
    }
    if (m_osdListener)
        vlc_player_osd_RemoveListener(m_player, m_osdListener);
}

int ShortcutMessage::duration() const
{
    return m_lifetime.interval();
}

void ShortcutMessage::onOsdMessage(vlc_player_t *, const char *text, void *data)
{
    auto that = static_cast<ShortcutMessage *>(data);

    /* called from the thread that changed the player: when it is handling a
     * key, the message is the result of that key */
    const QString result = QString::fromUtf8(text);
    const uint_fast32_t key = currentKey;

    QMetaObject::invokeMethod(that, [that, result, key]() {
        that->show(result, key);
    }, Qt::QueuedConnection);
}

void ShortcutMessage::show(const QString &result, uint_fast32_t vlcKey)
{
    if (!m_enabled || result.isEmpty())
        return;

    const QString key = (m_showKeys && vlcKey != KEY_UNSET) ? shortcutKeyText(vlcKey)
                                                            : QString();
    m_text = key.isEmpty() ? result
                           : QStringLiteral("%1 · %2").arg(result, key);

    m_lifetime.start();
    setVisible(true);
    emit shown(m_text);
}

void ShortcutMessage::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    emit visibleChanged();
}

QString shortcutKeyText(uint_fast32_t vlcKey)
{
    const uint_fast32_t base = vlcKey & ~KEY_MODIFIER;
    if (base == KEY_UNSET
     || (base >= KEY_MOUSEWHEELUP && base <= KEY_MOUSEWHEELRIGHT))
        return {};

    QString text = VLCKeyToString(vlcKey, true);
    const qsizetype last = text.lastIndexOf('+', -2) + 1;

    switch (base)
    {
        case KEY_UP:    text.replace(last, text.size() - last, QChar(0x2191)); break;
        case KEY_DOWN:  text.replace(last, text.size() - last, QChar(0x2193)); break;
        case KEY_LEFT:  text.replace(last, text.size() - last, QChar(0x2190)); break;
        case KEY_RIGHT: text.replace(last, text.size() - last, QChar(0x2192)); break;
        default:
            // letters as printed on the keyboard: "Shift+R"
            if (text.size() - last == 1)
                text[last] = text[last].toUpper();
            break;
    }
    return text;
}
