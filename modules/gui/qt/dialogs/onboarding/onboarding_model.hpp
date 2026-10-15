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
#ifndef VLC_QT_ONBOARDING_MODEL_HPP
#define VLC_QT_ONBOARDING_MODEL_HPP

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include <functional>

#include "qt.hpp"

/**
 * Onboarding, shown on first start: the library folders, the Theme and
 * Accent color, the online metadata choice, then the basic shortcuts.
 *
 * Every step has Back, Next and Skip. Next on the last step finishes. Skip
 * finishes at once: the steps left with Next keep their choices, the current
 * and later steps get back the values Onboarding opened with.
 */
class OnboardingModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int step READ step NOTIFY stepChanged FINAL)
    Q_PROPERTY(int stepCount READ stepCount NOTIFY stepCountChanged FINAL)
    Q_PROPERTY(Page page READ page NOTIFY stepChanged FINAL)
    Q_PROPERTY(bool lastStep READ isLastStep NOTIFY stepChanged FINAL)
    Q_PROPERTY(bool done READ isDone NOTIFY finished FINAL)

    // library folders, as URLs
    Q_PROPERTY(QStringList folders READ folders NOTIFY foldersChanged FINAL)
    // a ColorSchemeModel::ColorScheme
    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY themeChanged FINAL)
    // a value of the qt-accent-color option
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged FINAL)
    Q_PROPERTY(QVariantList accentChoices READ accentChoices CONSTANT FINAL)
    Q_PROPERTY(bool fetchOnline READ fetchOnline WRITE setFetchOnline NOTIFY fetchOnlineChanged FINAL)
    // { label, keys: [{ text, caps }] }, the keys bound now
    Q_PROPERTY(QVariantList shortcuts READ shortcuts NOTIFY shortcutsChanged FINAL)

public:
    enum Page
    {
        Folders,
        Theme,
        Privacy,
        Shortcuts,
    };
    Q_ENUM(Page)

    struct Choices
    {
        QStringList folders;
        int theme = 0;
        QString accentColor;
        bool fetchOnline = true;
    };

    // writes the choices, on Finish and Skip
    using ApplyFunction = std::function<void(const Choices& choices)>;
    // shows a Theme and Accent color at once, before they apply
    using PreviewFunction = std::function<void(int theme, const QString& accentColor)>;

    explicit OnboardingModel(QObject* parent = nullptr);

    void setApplyFunction(ApplyFunction apply);
    void setPreviewFunction(PreviewFunction preview);

    /**
     * @param obj object the key bindings are inherited from
     * @param initial the choices Onboarding opens with
     * @param hasLibrary without a media library, the folders step is left out
     */
    void load(vlc_object_t* obj, const Choices& initial, bool hasLibrary);

    int step() const { return m_step; }
    int stepCount() const { return m_pages.size(); }
    Page page() const;
    bool isLastStep() const { return m_step == stepCount() - 1; }
    bool isDone() const { return m_done; }

    // goes to another step, without Next or Back (VLC_QT_ONBOARDING hook)
    Q_INVOKABLE void setStep(int step);

    Q_INVOKABLE void next();
    Q_INVOKABLE void back();
    Q_INVOKABLE void skip();

    QStringList folders() const { return m_choices.folders; }
    // adds a folder, unless it is in the list already
    Q_INVOKABLE bool addFolder(const QString& url);
    Q_INVOKABLE void removeFolder(int index);
    // a folder URL for display
    Q_INVOKABLE static QString folderPath(const QString& url);

    int theme() const { return m_choices.theme; }
    void setTheme(int theme);
    QString accentColor() const { return m_choices.accentColor; }
    void setAccentColor(const QString& accentColor);
    static QVariantList accentChoices();

    bool fetchOnline() const { return m_choices.fetchOnline; }
    void setFetchOnline(bool fetchOnline);

    QVariantList shortcuts() const { return m_shortcuts; }

signals:
    void stepChanged();
    void stepCountChanged();
    void foldersChanged();
    void themeChanged();
    void accentColorChanged();
    void fetchOnlineChanged();
    void shortcutsChanged();
    // Finish or Skip applied the choices
    void finished();

private:
    void finish();
    void preview();

    QVector<Page> m_pages;
    int m_step = 0;
    bool m_done = false;

    Choices m_initial;
    Choices m_choices;
    QVariantList m_shortcuts;

    ApplyFunction m_apply;
    PreviewFunction m_preview;
};

#endif // VLC_QT_ONBOARDING_MODEL_HPP
