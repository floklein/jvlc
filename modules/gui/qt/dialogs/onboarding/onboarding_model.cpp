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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "onboarding_model.hpp"

#include <vlc_common.h>
#include <vlc_actions.h>

#include "style/accent_colors.hpp"
#include "util/vlchotkeyconverter.hpp"

#include <QDir>
#include <QUrl>
#include <QVariantMap>

namespace {

/* The basic shortcuts, as Onboarding shows them: one row per action, or per
 * pair of opposite actions. The keys come from the key bindings. */
const struct
{
    const char* label;
    const char* actions[2];
} shortcutTable[] = {
    { N_("Play / Pause"),               { "play-pause" } },
    { N_("Seek backward / forward"),    { "jump-short", "jump+short" } },
    { N_("Previous / next item"),       { "prev", "next" } },
    { N_("Previous / next frame"),      { "frame-previous", "frame-next" } },
    { N_("Cycle subtitle track"),       { "subtitle-track" } },
    { N_("Cycle audio track"),          { "audio-track" } },
    { N_("Toggle fullscreen"),          { "toggle-fullscreen" } },
    { N_("Increase / decrease volume"), { "vol-up", "vol-down" } },
    { N_("Show / hide Queue"),          { "show-queue" } },
    { N_("Show / hide the Library pane"), { "library-pane" } },
    { N_("Take a snapshot"),            { "snapshot" } },
    { N_("Record"),                     { "record" } },
};

/* The first key bound to an action, KEY_UNSET if none */
uint_fast32_t actionKey(vlc_object_t* obj, const QString& action)
{
    // an action of the interface itself
    if (!appActionDefaultSequence(action).isEmpty())
        return qtSequenceToVLCKey(appActionSequence(obj, action));

    const QByteArray option = "key-" + action.toUtf8();
    if (vlc_actions_get_id(option.constData()) == ACTIONID_NONE)
        return KEY_UNSET;

    uint_fast32_t* keycodes = nullptr;
    const size_t count = vlc_actions_get_keycodes(obj, option.constData() + 4, false, &keycodes);
    uint_fast32_t key = KEY_UNSET;
    for (size_t i = 0; i < count && key == KEY_UNSET; ++i)
        key = keycodes[i];
    free(keycodes);
    return key;
}

/* A key as key caps: the modifiers, then the key. Arrows are drawn as such. */
QStringList keyCaps(uint_fast32_t key)
{
    const QString text = vlcKeyText(key);
    const uint_fast32_t base = key & ~KEY_MODIFIER;
    const QString baseText = vlcKeyText(base);

    QStringList caps;
    if (text.endsWith(baseText))
    {
        const QString modifiers = text.left(text.size() - baseText.size());
        caps = modifiers.split('+', Qt::SkipEmptyParts);
    }

    switch (base)
    {
    case KEY_LEFT:  caps.append(QStringLiteral("←")); break;
    case KEY_RIGHT: caps.append(QStringLiteral("→")); break;
    case KEY_UP:    caps.append(QStringLiteral("↑")); break;
    case KEY_DOWN:  caps.append(QStringLiteral("↓")); break;
    default:        caps.append(baseText); break;
    }
    return caps;
}

QVariantList shortcutRows(vlc_object_t* obj)
{
    QVariantList rows;
    if (!obj)
        return rows;

    for (const auto& entry : shortcutTable)
    {
        QVariantList keys;
        for (const char* action : entry.actions)
        {
            if (!action)
                continue;
            const uint_fast32_t key = actionKey(obj, QString::fromLatin1(action));
            if (key == KEY_UNSET)
                continue;
            keys.append(QVariantMap{
                { QStringLiteral("text"), vlcKeyText(key) },
                { QStringLiteral("caps"), keyCaps(key) },
            });
        }
        // an action without a key is not worth learning
        if (keys.isEmpty())
            continue;
        rows.append(QVariantMap{
            { QStringLiteral("label"), qtr(entry.label) },
            { QStringLiteral("keys"), keys },
        });
    }
    return rows;
}

}

OnboardingModel::OnboardingModel(QObject* parent)
    : QObject(parent)
{
}

void OnboardingModel::setApplyFunction(ApplyFunction apply)
{
    m_apply = std::move(apply);
}

void OnboardingModel::setPreviewFunction(PreviewFunction preview)
{
    m_preview = std::move(preview);
}

void OnboardingModel::load(vlc_object_t* obj, const Choices& initial, bool hasLibrary)
{
    m_initial = initial;
    if (!hasLibrary)
        m_initial.folders.clear();
    m_choices = m_initial;

    m_pages.clear();
    if (hasLibrary)
        m_pages.append(Folders);
    m_pages.append({ Theme, Privacy, Shortcuts });
    m_step = 0;
    m_done = false;

    m_shortcuts = shortcutRows(obj);

    emit stepCountChanged();
    emit stepChanged();
    emit foldersChanged();
    emit themeChanged();
    emit accentColorChanged();
    emit fetchOnlineChanged();
    emit shortcutsChanged();
}

OnboardingModel::Page OnboardingModel::page() const
{
    return m_pages.value(m_step, Shortcuts);
}

void OnboardingModel::setStep(int step)
{
    if (m_done || step < 0 || step >= stepCount() || step == m_step)
        return;
    m_step = step;
    emit stepChanged();
}

void OnboardingModel::next()
{
    if (m_done)
        return;
    if (isLastStep())
        finish();
    else
        setStep(m_step + 1);
}

void OnboardingModel::back()
{
    setStep(m_step - 1);
}

void OnboardingModel::skip()
{
    if (m_done)
        return;

    // the current and later steps get back their defaults
    bool themeReset = false;
    for (int i = m_step; i < stepCount(); ++i)
    {
        switch (m_pages[i])
        {
        case Folders:
            m_choices.folders = m_initial.folders;
            break;
        case Theme:
            themeReset = m_choices.theme != m_initial.theme
                      || m_choices.accentColor != m_initial.accentColor;
            m_choices.theme = m_initial.theme;
            m_choices.accentColor = m_initial.accentColor;
            break;
        case Privacy:
            m_choices.fetchOnline = m_initial.fetchOnline;
            break;
        case Shortcuts:
            break;
        }
    }
    emit foldersChanged();
    emit themeChanged();
    emit accentColorChanged();
    emit fetchOnlineChanged();
    if (themeReset)
        preview();

    finish();
}

bool OnboardingModel::addFolder(const QString& url)
{
    if (m_done || url.isEmpty() || m_choices.folders.contains(url))
        return false;
    m_choices.folders.append(url);
    emit foldersChanged();
    return true;
}

void OnboardingModel::removeFolder(int index)
{
    if (m_done || index < 0 || index >= m_choices.folders.size())
        return;
    m_choices.folders.removeAt(index);
    emit foldersChanged();
}

QString OnboardingModel::folderPath(const QString& url)
{
    const QUrl folder(url);
    if (folder.isLocalFile())
        return QDir::toNativeSeparators(folder.toLocalFile());
    return folder.toDisplayString(QUrl::PreferLocalFile);
}

void OnboardingModel::setTheme(int theme)
{
    if (m_done || theme == m_choices.theme)
        return;
    m_choices.theme = theme;
    emit themeChanged();
    preview();
}

void OnboardingModel::setAccentColor(const QString& accentColor)
{
    if (m_done || accentColor == m_choices.accentColor)
        return;
    m_choices.accentColor = accentColor;
    emit accentColorChanged();
    preview();
}

QVariantList OnboardingModel::accentChoices()
{
    return accentColorChoices();
}

void OnboardingModel::setFetchOnline(bool fetchOnline)
{
    if (m_done || fetchOnline == m_choices.fetchOnline)
        return;
    m_choices.fetchOnline = fetchOnline;
    emit fetchOnlineChanged();
}

void OnboardingModel::finish()
{
    if (m_done)
        return;
    m_done = true;
    if (m_apply)
        m_apply(m_choices);
    emit finished();
}

void OnboardingModel::preview()
{
    if (m_preview)
        m_preview(m_choices.theme, m_choices.accentColor);
}
