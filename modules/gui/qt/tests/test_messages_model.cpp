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
#include "vlc_stub_modules.hpp"

#include <QTest>
#include <QFile>
#include <QTemporaryDir>

#include <vlc_messages.h>

#include "dialogs/messages/messagesmodel.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"

class TestMessagesModel : public QObject
{
    Q_OBJECT

private:
    // Other threads of the instance log too: every test only looks at the
    // messages carrying its own token, through the search pattern.
    QStringList visibleTexts(const MessagesModel& model) const
    {
        QStringList texts;
        for (int i = 0; i < model.rowCount(); ++i)
            texts << model.data(model.index(i), MessagesModel::TEXT).toString();
        return texts;
    }

    vlc_object_t* obj() const { return VLC_OBJECT(m_env->intf); }

private slots:
    void initTestCase()
    {
        m_env = std::make_unique<VLCTestingEnv>();
        QVERIFY(m_env->init());
    }

    void cleanupTestCase()
    {
        m_env.reset();
    }

    void testCapturesEveryLevel()
    {
        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Debug);
        model.setSearchPattern("tst-levels");

        msg_Err(obj(), "tst-levels error");
        msg_Warn(obj(), "tst-levels warning");
        msg_Info(obj(), "tst-levels info");
        msg_Dbg(obj(), "tst-levels debug");

        QTRY_COMPARE(model.rowCount(), 4);
        QCOMPARE(visibleTexts(model), QStringList({ "tst-levels error", "tst-levels warning",
                                                    "tst-levels info", "tst-levels debug" }));
        QCOMPARE(model.data(model.index(0), MessagesModel::TYPE).toInt(), int(VLC_MSG_ERR));
        QCOMPARE(model.data(model.index(3), MessagesModel::TYPE).toInt(), int(VLC_MSG_DBG));
    }

    void testVerbosityFilter_data()
    {
        QTest::addColumn<int>("verbosity");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("errors") << int(MessagesModel::Errors)
                                << QStringList{ "tst-verbosity error" };
        QTest::newRow("warnings") << int(MessagesModel::Warnings)
                                  << QStringList{ "tst-verbosity error", "tst-verbosity warning" };
        QTest::newRow("info") << int(MessagesModel::Info)
                              << QStringList{ "tst-verbosity error", "tst-verbosity warning",
                                              "tst-verbosity info" };
    }

    void testVerbosityFilter()
    {
        QFETCH(int, verbosity);
        QFETCH(QStringList, expected);

        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Debug);
        model.setSearchPattern("tst-verbosity");

        msg_Err(obj(), "tst-verbosity error");
        msg_Warn(obj(), "tst-verbosity warning");
        msg_Info(obj(), "tst-verbosity info");
        msg_Dbg(obj(), "tst-verbosity debug");
        QTRY_COMPARE(model.rowCount(), 4);

        model.setVerbosity(static_cast<MessagesModel::Verbosity>(verbosity));
        QCOMPARE(visibleTexts(model), expected);
    }

    void testDebugIsOnlyKeptAtDebugVerbosity()
    {
        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Info);
        model.setSearchPattern("tst-debug-gate");

        /* debug messages are many: they are not even formatted unless shown */
        msg_Dbg(obj(), "tst-debug-gate dropped");
        msg_Info(obj(), "tst-debug-gate info");
        QTRY_COMPARE(model.rowCount(), 1);

        model.setVerbosity(MessagesModel::Debug);
        QCOMPARE(visibleTexts(model), QStringList{ "tst-debug-gate info" });

        msg_Dbg(obj(), "tst-debug-gate kept");
        QTRY_COMPARE(visibleTexts(model), QStringList({ "tst-debug-gate info",
                                                        "tst-debug-gate kept" }));
    }

    void testLessVerboseLevelsAreKeptAtAnyVerbosity()
    {
        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Errors);
        model.setSearchPattern("tst-kept");

        msg_Warn(obj(), "tst-kept warning");
        msg_Err(obj(), "tst-kept error");
        QTRY_COMPARE(model.rowCount(), 1);

        /* back from the errors of a notice's Details, the rest is there */
        model.setVerbosity(MessagesModel::Info);
        QCOMPARE(visibleTexts(model), QStringList({ "tst-kept warning", "tst-kept error" }));
    }

    void testSearchIgnoresCase()
    {
        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Debug);

        msg_Err(obj(), "tst-search Alpha");
        msg_Err(obj(), "tst-search beta");

        model.setSearchPattern("TST-SEARCH ALPHA");
        QTRY_COMPARE(visibleTexts(model), QStringList{ "tst-search Alpha" });

        model.setSearchPattern("tst-search");
        QCOMPARE(model.rowCount(), 2);
    }

    void testTextAndSaveHoldTheVisibleLines()
    {
        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Warnings);
        model.setSearchPattern("tst-save");

        msg_Err(obj(), "tst-save first");
        msg_Dbg(obj(), "tst-save hidden");
        msg_Warn(obj(), "tst-save second");
        QTRY_COMPARE(model.rowCount(), 2);

        const QStringList lines = model.text().split('\n');
        QCOMPARE(lines.size(), 2);
        QVERIFY2(lines[0].endsWith(" error: tst-save first"), qPrintable(lines[0]));
        QVERIFY2(lines[1].endsWith(" warning: tst-save second"), qPrintable(lines[1]));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("vlc.log");
        QVERIFY(model.save(path));

        QFile file(path);
        QVERIFY(file.open(QFile::ReadOnly | QFile::Text));
        QCOMPARE(QString::fromUtf8(file.readAll()), model.text() + '\n');
    }

    void testDropsOldestBeyondCapacity()
    {
        MessagesModel model(m_env->intf, 3);
        model.setVerbosity(MessagesModel::Debug);
        model.setSearchPattern("tst-capacity");

        for (int i = 0; i < 5; ++i)
            msg_Err(obj(), "tst-capacity %d", i);

        QTRY_VERIFY(visibleTexts(model).contains("tst-capacity 4"));
        const QStringList texts = visibleTexts(model);
        QVERIFY(texts.size() <= 3);
        QVERIFY(!texts.contains("tst-capacity 0"));
        QVERIFY(!texts.contains("tst-capacity 1"));
    }

    void testClear()
    {
        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Debug);
        model.setSearchPattern("tst-clear");

        msg_Err(obj(), "tst-clear before");
        QTRY_COMPARE(model.rowCount(), 1);

        model.clear();
        QCOMPARE(model.rowCount(), 0);

        msg_Err(obj(), "tst-clear after");
        QTRY_COMPARE(visibleTexts(model), QStringList{ "tst-clear after" });
    }

    void testErrorNoticesAreLogged()
    {
        MessagesModel model(m_env->intf);
        model.setVerbosity(MessagesModel::Errors);
        model.setSearchPattern("tst-notice");

        DialogErrorModel errors(m_env->intf);
        errors.pushError("tst-notice title", "tst-notice text");

        QTRY_COMPARE(visibleTexts(model), QStringList{ "tst-notice title: tst-notice text" });
    }

private:
    std::unique_ptr<VLCTestingEnv> m_env;
};

QTEST_GUILESS_MAIN(TestMessagesModel)
#include "test_messages_model.moc"
