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

#include "app_onboarding_model.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_media_library.h>
#include <vlc_url.h>
#include <vlc_cxx_helpers.hpp>

#include "medialibrary/medialib.hpp"
#include "util/color_scheme_model.hpp"

#include <QDir>
#include <QFileDialog>
#include <QSettings>
#include <QUrl>

namespace {

// set once Onboarding finished: shown again, it keeps the online metadata choice
const char onboardingDoneKey[] = "Onboarding/done";

QStringList defaultFolders()
{
    QStringList folders;
    for (const vlc_user_dir dir : { VLC_VIDEOS_DIR, VLC_MUSIC_DIR })
    {
        auto path = vlc::wrap_cptr(config_GetUserDir(dir));
        if (!path)
            continue;
        auto mrl = vlc::wrap_cptr(vlc_path2uri(path.get(), nullptr));
        if (!mrl)
            continue;
        const QString folder = qfu(mrl.get());
        if (!folders.contains(folder))
            folders.append(folder);
    }
    return folders;
}

void applyTheme(MainCtx* ctx, int theme)
{
    ColorSchemeModel* schemes = ctx->getColorScheme();
    // the rows of ColorSchemeModel are in the order of its enum
    if (theme >= 0 && theme < schemes->rowCount({}) && theme != schemes->currentIndex())
        schemes->setCurrentIndex(theme);
}

void applyAccentColor(MainCtx* ctx, const QString& accentColor)
{
    if (accentColor == ctx->accentColor())
        return;
    config_PutPsz("qt-accent-color", qtu(accentColor));
    // MainCtx reads the Accent color again, and the palette follows
    ctx->reloadPrefs();
}

}

AppOnboardingModel::AppOnboardingModel(QObject* parent)
    : OnboardingModel(parent)
{
}

void AppOnboardingModel::setCtx(MainCtx* ctx)
{
    if (m_ctx || !ctx)
        return;
    m_ctx = ctx;

    qt_intf_t* p_intf = ctx->getIntf();
    MediaLib* mediaLib = ctx->getMediaLibrary();

    setPreviewFunction([ctx](int theme, const QString& accentColor) {
        applyTheme(ctx, theme);
        applyAccentColor(ctx, accentColor);
    });

    setApplyFunction([ctx, p_intf, mediaLib](const Choices& choices) {
        config_PutInt("metadata-network-access", choices.fetchOnline);
        config_PutInt("qt-privacy-ask", 0);

        applyTheme(ctx, choices.theme);
        getSettings()->setValue("MainWindow/color-scheme-index", ctx->getColorScheme()->currentIndex());
        applyAccentColor(ctx, choices.accentColor);
        getSettings()->setValue(onboardingDoneKey, true);

        if (mediaLib)
        {
            const QStringList folders = choices.folders;
            mediaLib->runOnMLThread(mediaLib, [folders](vlc_medialibrary_t* ml) {
                for (const QString& folder : folders)
                    vlc_ml_add_folder(ml, qtu(folder));
            });
            // while "qt-privacy-ask" was set, the library did not start
            mediaLib->reload();
        }

        ctx->reloadPrefs();
        config_SaveConfigFile(p_intf);
    });

    Choices initial;
    initial.folders = defaultFolders();
    initial.theme = ctx->getColorScheme()->currentIndex();
    initial.accentColor = ctx->accentColor();
    // on by default; shown again, Onboarding keeps the user's choice
    initial.fetchOnline = !getSettings()->value(onboardingDoneKey, false).toBool()
                       || var_InheritBool(p_intf, "metadata-network-access");

    load(VLC_OBJECT(vlc_object_instance(p_intf)), initial, mediaLib != nullptr);

    emit ctxChanged();
}

void AppOnboardingModel::chooseFolder()
{
    const QUrl folder = QFileDialog::getExistingDirectoryUrl(
        nullptr, qtr("Add a folder to the library"), QUrl::fromLocalFile(QDir::homePath()));
    if (folder.isEmpty())
        return;
    addFolder(folder.toString(QUrl::FullyEncoded));
}
