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

#include <QTest>
#include <QSignalSpy>
#include <QColor>

#include <map>
#include <string>

#include "../player/subtitle_appearance.hpp"

// The Defaults and Appearance of the Subtitles tab of the Adjustments sidebar
// (spec #30, ticket #53): which options each row writes, and when the line on
// screen is redrawn
class TestSubtitleAppearance : public QObject
{
    Q_OBJECT

    // the options of the Profile, as the core would hold them
    class FakeOptions : public SubtitleAppearance::Backend
    {
    public:
        std::map<std::string, QVariant> values;
        std::map<std::string, int> writes;
        int redraws = 0;

        QVariant value(const char* option) const override
        {
            auto it = values.find(option);
            return it == values.end() ? QVariant{} : it->second;
        }

        void setValue(const char* option, const QVariant& value) override
        {
            values[option] = value;
            ++writes[option];
        }

        // shown on the player only
        std::map<std::string, QVariant> shown;
        void apply(const char* option, const QVariant& value) override
        {
            shown[option] = value;
        }

        void redraw() override
        {
            ++redraws;
        }

        int profileReloads = 0;
        void reloadProfile() override
        {
            ++profileReloads;
        }
    };

    FakeOptions* m_options = nullptr;
    std::unique_ptr<SubtitleAppearance> m_appearance;

    void create()
    {
        auto options = std::make_unique<FakeOptions>();
        m_options = options.get();
        m_options->values = {
            { "freetype-keep-styling", true },
            { "freetype-font", QString() },
            { "sub-text-scale", 100 },
            { "freetype-pixel-size", 0 },
            { "freetype-color", 0xFFFFFF },
            { "freetype-outline-color", 0x000000 },
            { "freetype-outline-thickness", 4 },
            { "freetype-background-opacity", 0 },
            { "sub-margin", 0 },
            { "sub-language", QString() },
        };
        m_appearance = std::make_unique<SubtitleAppearance>(std::move(options));
    }

    int writes(const char* option) const
    {
        auto it = m_options->writes.find(option);
        return it == m_options->writes.end() ? 0 : it->second;
    }

private slots:
    void init()
    {
        create();
    }

    void cleanup()
    {
        m_appearance.reset();
        m_options = nullptr;
    }

    void readsTheProfile()
    {
        m_appearance.reset();
        auto options = std::make_unique<FakeOptions>();
        options->values = {
            { "freetype-keep-styling", false },
            { "freetype-font", QStringLiteral("Inter") },
            { "sub-text-scale", 133 },
            { "freetype-pixel-size", 0 },
            { "freetype-color", 0xB8B8B8 },
            { "freetype-outline-color", 0x0C0C0C },
            { "freetype-outline-thickness", 6 },
            { "freetype-background-opacity", 128 },
            { "sub-margin", 24 },
            { "sub-language", QStringLiteral("fr,en") },
        };
        m_appearance = std::make_unique<SubtitleAppearance>(std::move(options));

        QCOMPARE(m_appearance->keepFileStyling(), false);
        QCOMPARE(m_appearance->font(), QStringLiteral("Inter"));
        QCOMPARE(m_appearance->sizeMode(), SubtitleAppearance::Relative);
        QCOMPARE(m_appearance->relativeSize(), SubtitleAppearance::Large);
        QCOMPARE(m_appearance->textColor(), QColor(QStringLiteral("#B8B8B8")));
        QCOMPARE(m_appearance->outlineColor(), QColor(QStringLiteral("#0C0C0C")));
        QCOMPARE(m_appearance->outlineThickness(), SubtitleAppearance::ThickOutline);
        QCOMPARE(m_appearance->backgroundOpacity(), 50);
        QCOMPARE(m_appearance->verticalOffset(), 24);
        // the first language of the list is the default one
        QCOMPARE(m_appearance->defaultLanguage(), QStringLiteral("fr"));
    }

    void aScaleBetweenTwoSizesShowsTheNearest()
    {
        m_options->values["sub-text-scale"] = 120;
        create_with(m_options->values);
        QCOMPARE(m_appearance->relativeSize(), SubtitleAppearance::Large);

        m_options->values["sub-text-scale"] = 10;
        create_with(m_options->values);
        QCOMPARE(m_appearance->relativeSize(), SubtitleAppearance::Smaller);
    }

    void anAppearanceChangeRedrawsTheLineOnScreen()
    {
        QSignalSpy changed(m_appearance.get(), &SubtitleAppearance::textColorChanged);

        m_appearance->setTextColor(QColor(QStringLiteral("#FFCC00")));
        QCOMPARE(m_options->values["freetype-color"].toInt(), 0xFFCC00);
        QCOMPARE(m_options->redraws, 1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(m_appearance->textColor(), QColor(QStringLiteral("#FFCC00")));

        m_appearance->setOutlineColor(QColor(QStringLiteral("#102030")));
        QCOMPARE(m_options->values["freetype-outline-color"].toInt(), 0x102030);

        m_appearance->setFont(QStringLiteral("Georgia"));
        QCOMPARE(m_options->values["freetype-font"].toString(), QStringLiteral("Georgia"));

        m_appearance->setKeepFileStyling(false);
        QCOMPARE(m_options->values["freetype-keep-styling"].toBool(), false);

        QCOMPARE(m_options->redraws, 4);
    }

    void theSameValueWritesNothing()
    {
        m_appearance->setTextColor(QColor(Qt::white));
        m_appearance->setOutlineThickness(SubtitleAppearance::NormalOutline);
        m_appearance->setVerticalOffset(0);
        QCOMPARE(writes("freetype-color"), 0);
        QCOMPARE(writes("freetype-outline-thickness"), 0);
        QCOMPARE(writes("sub-margin"), 0);
        QCOMPARE(m_options->redraws, 0);
    }

    void outlineThicknessHasFourSteps()
    {
        m_appearance->setOutlineThickness(SubtitleAppearance::NoOutline);
        QCOMPARE(m_options->values["freetype-outline-thickness"].toInt(), 0);
        m_appearance->setOutlineThickness(SubtitleAppearance::ThinOutline);
        QCOMPARE(m_options->values["freetype-outline-thickness"].toInt(), 2);
        m_appearance->setOutlineThickness(SubtitleAppearance::ThickOutline);
        QCOMPARE(m_options->values["freetype-outline-thickness"].toInt(), 6);
    }

    void backgroundOpacityIsAPercentage()
    {
        m_appearance->setBackgroundOpacity(100);
        QCOMPARE(m_options->values["freetype-background-opacity"].toInt(), 255);
        m_appearance->setBackgroundOpacity(40);
        QCOMPARE(m_options->values["freetype-background-opacity"].toInt(), 102);
        QCOMPARE(m_appearance->backgroundOpacity(), 40);
        m_appearance->setBackgroundOpacity(150);
        QCOMPARE(m_appearance->backgroundOpacity(), 100);
    }

    void relativeSizesMatchTheTextScale()
    {
        m_appearance->setRelativeSize(SubtitleAppearance::Smaller);
        QCOMPARE(m_options->values["sub-text-scale"].toInt(), 80);
        m_appearance->setRelativeSize(SubtitleAppearance::Small);
        QCOMPARE(m_options->values["sub-text-scale"].toInt(), 89);
        m_appearance->setRelativeSize(SubtitleAppearance::Larger);
        QCOMPARE(m_options->values["sub-text-scale"].toInt(), 267);
        m_appearance->setRelativeSize(SubtitleAppearance::Normal);
        QCOMPARE(m_options->values["sub-text-scale"].toInt(), 100);
        QCOMPARE(m_options->values["freetype-pixel-size"].toInt(), 0);
    }

    void pixelsGiveAnExactSize()
    {
        QSignalSpy mode(m_appearance.get(), &SubtitleAppearance::sizeModeChanged);

        m_appearance->setSizeMode(SubtitleAppearance::Pixels);
        QCOMPARE(mode.count(), 1);
        QCOMPARE(m_options->values["freetype-pixel-size"].toInt(), m_appearance->pixelSize());
        QVERIFY(m_appearance->pixelSize() > 0);

        m_appearance->setPixelSize(56);
        QCOMPARE(m_options->values["freetype-pixel-size"].toInt(), 56);

        // back to relative, the pixel size is kept for later
        m_appearance->setSizeMode(SubtitleAppearance::Relative);
        QCOMPARE(m_options->values["freetype-pixel-size"].toInt(), 0);
        QCOMPARE(m_appearance->pixelSize(), 56);

        // a relative size doesn't leave the pixels mode by itself
        m_appearance->setSizeMode(SubtitleAppearance::Pixels);
        QCOMPARE(m_options->values["freetype-pixel-size"].toInt(), 56);
        m_appearance->setRelativeSize(SubtitleAppearance::Large);
        QCOMPARE(m_options->values["sub-text-scale"].toInt(), 133);
        QCOMPARE(m_options->values["freetype-pixel-size"].toInt(), 0);
        QCOMPARE(m_appearance->sizeMode(), SubtitleAppearance::Relative);
    }

    void aPixelSizeIsReadBack()
    {
        m_options->values["freetype-pixel-size"] = 48;
        create_with(m_options->values);
        QCOMPARE(m_appearance->sizeMode(), SubtitleAppearance::Pixels);
        QCOMPARE(m_appearance->pixelSize(), 48);
    }

    void verticalOffsetMovesTheLineWithoutARedraw()
    {
        m_appearance->setVerticalOffset(40);
        QCOMPARE(m_options->values["sub-margin"].toInt(), 40);
        // the video output moves the subtitles by itself
        QCOMPARE(m_options->redraws, 0);
    }

    void defaultLanguageIsOneLanguageOrNone()
    {
        m_appearance->setDefaultLanguage(QStringLiteral("de"));
        QCOMPARE(m_options->values["sub-language"].toString(), QStringLiteral("de"));

        m_appearance->setDefaultLanguage(QStringLiteral("none"));
        QCOMPARE(m_options->values["sub-language"].toString(), QStringLiteral("none"));
        QCOMPARE(m_appearance->defaultLanguage(), QStringLiteral("none"));

        // a default language picks the track of the next media
        QCOMPARE(m_options->redraws, 0);
    }

    // switching Profile (#45) replaces the options under the tab
    void anotherProfileIsShownAndDrawn()
    {
        QSignalSpy color(m_appearance.get(), &SubtitleAppearance::textColorChanged);
        QSignalSpy mode(m_appearance.get(), &SubtitleAppearance::sizeModeChanged);

        m_options->values["freetype-color"] = 0x00FF00;
        m_options->values["freetype-pixel-size"] = 30;
        m_appearance->reload();

        QCOMPARE(m_options->profileReloads, 1);
        QCOMPARE(m_appearance->textColor(), QColor(Qt::green));
        QCOMPARE(m_appearance->sizeMode(), SubtitleAppearance::Pixels);
        QCOMPARE(m_appearance->pixelSize(), 30);
        QCOMPARE(color.count(), 1);
        QCOMPARE(mode.count(), 1);
        QCOMPARE(m_options->redraws, 1);
    }

    // a Preset is active (#56): changes are shown, the Profile keeps its
    // values
    void withoutProfileWritesTheChangesAreOnlyShown()
    {
        QSignalSpy adjusted(m_appearance.get(), &SubtitleAppearance::adjusted);
        m_appearance->setProfileWrites(false);

        m_appearance->setTextColor(Qt::yellow);
        m_appearance->setRelativeSize(SubtitleAppearance::Large);
        m_appearance->setDefaultLanguage(QStringLiteral("ja"));

        QCOMPARE(writes("freetype-color"), 0);
        QCOMPARE(writes("sub-text-scale"), 0);
        QCOMPARE(writes("sub-language"), 0);
        QCOMPARE(m_options->shown["freetype-color"].toInt(), 0xFFFF00);
        QCOMPARE(m_options->shown["sub-text-scale"].toInt(), 133);
        QCOMPARE(m_options->shown["sub-language"].toString(), QStringLiteral("ja"));
        QCOMPARE(m_options->redraws, 2);
        QCOMPARE(adjusted.count(), 3);

        m_appearance->setProfileWrites(true);
        m_appearance->setTextColor(Qt::white);
        QCOMPARE(writes("freetype-color"), 1);
    }

    void reloadingTheProfileIsNotAnAdjustment()
    {
        QSignalSpy adjusted(m_appearance.get(), &SubtitleAppearance::adjusted);
        m_options->values["freetype-color"] = 0x00FF00;
        m_appearance->reload();
        QCOMPARE(adjusted.count(), 0);
    }

    void languagesStartWithAutomaticAndNone()
    {
        const QVariantList languages = m_appearance->languages();
        QVERIFY(languages.size() > 50);

        const QVariantMap automatic = languages.at(0).toMap();
        QCOMPARE(automatic.value("code").toString(), QString());
        const QVariantMap none = languages.at(1).toMap();
        QCOMPARE(none.value("code").toString(), QStringLiteral("none"));

        bool french = false;
        QString previous;
        for (int i = 2; i < languages.size(); ++i)
        {
            const QVariantMap language = languages.at(i).toMap();
            const QString name = language.value("name").toString();
            QVERIFY(!name.isEmpty());
            // sorted by name
            QVERIFY(QString::localeAwareCompare(previous, name) <= 0);
            previous = name;
            if (language.value("code").toString() == QLatin1String("fr"))
                french = name == QLatin1String("French");
        }
        QVERIFY(french);
    }

private:
    void create_with(std::map<std::string, QVariant> values)
    {
        m_appearance.reset();
        auto options = std::make_unique<FakeOptions>();
        options->values = std::move(values);
        m_options = options.get();
        m_appearance = std::make_unique<SubtitleAppearance>(std::move(options));
    }
};

QTEST_GUILESS_MAIN(TestSubtitleAppearance)
#include "test_subtitle_appearance.moc"
