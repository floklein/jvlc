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
#ifndef VLC_QT_APP_ONBOARDING_MODEL_HPP
#define VLC_QT_APP_ONBOARDING_MODEL_HPP

#include "onboarding_model.hpp"
#include "maininterface/mainctx.hpp"

/**
 * Onboarding on the running interface: it opens with the system Videos and
 * Music folders and the current Theme and Accent color, previews them live,
 * and on Finish or Skip writes the choices, adds the folders to the media
 * library and turns off "qt-privacy-ask".
 */
class AppOnboardingModel : public OnboardingModel
{
    Q_OBJECT

    Q_PROPERTY(MainCtx* ctx READ ctx WRITE setCtx NOTIFY ctxChanged FINAL)

public:
    explicit AppOnboardingModel(QObject* parent = nullptr);

    MainCtx* ctx() const { return m_ctx; }
    void setCtx(MainCtx* ctx);

    // asks for a folder with the system picker and adds it
    Q_INVOKABLE void chooseFolder();

signals:
    void ctxChanged();

private:
    MainCtx* m_ctx = nullptr;
};

#endif // VLC_QT_APP_ONBOARDING_MODEL_HPP
