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

#include "../playlist/pasted_media.hpp"

#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace vlc::playlist;

static QString fileMrl(const QString &path)
{
    return QString::fromUtf8(QUrl::fromLocalFile(path).toEncoded());
}

class TestPastedMedia : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_file;

    QString nativePath(const QString &path)
    {
        return QDir::toNativeSeparators(path);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_file = m_dir.filePath("clip.mkv");
        QFile file(m_file);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    void textWithALink()
    {
        QCOMPARE(mrlsFromText("https://example.com/live/stream.m3u8"),
                 QStringList { "https://example.com/live/stream.m3u8" });
        /* surrounding blanks are not part of the link */
        QCOMPARE(mrlsFromText("  rtsp://192.168.1.10:554/cam \n"),
                 QStringList { "rtsp://192.168.1.10:554/cam" });
    }

    void textWithAFilePath()
    {
        QCOMPARE(mrlsFromText(nativePath(m_file)), QStringList { fileMrl(m_file) });
        /* "Copy as path" on Windows quotes the path */
        QCOMPARE(mrlsFromText('"' + nativePath(m_file) + '"'), QStringList { fileMrl(m_file) });
    }

    void textWithAFolderPath()
    {
        const QStringList mrls = mrlsFromText(nativePath(m_dir.path()));
        QCOMPARE(mrls.size(), 1);
        QCOMPARE(QUrl(mrls[0]).toLocalFile(), QUrl::fromLocalFile(m_dir.path()).toLocalFile());
    }

    void textWithNothingToOpen()
    {
        QCOMPARE(mrlsFromText(""), QStringList {});
        QCOMPARE(mrlsFromText("   \n\n"), QStringList {});
        QCOMPARE(mrlsFromText("Hello, see you tomorrow"), QStringList {});
        /* a sentence that contains a link is not a link */
        QCOMPARE(mrlsFromText("watch https://example.com/a.mp4 now"), QStringList {});
        /* paths must exist and be absolute */
        QCOMPARE(mrlsFromText(nativePath(m_dir.filePath("missing.mkv"))), QStringList {});
        QCOMPARE(mrlsFromText("clip.mkv"), QStringList {});
    }

    void textWithSeveralLines()
    {
        const QString text = "https://example.com/a.mp4\r\n"
                             "not a link\n"
                             + nativePath(m_file) + "\n"
                             "\n"
                             "udp://@239.0.0.1:1234\n";
        QCOMPARE(mrlsFromText(text), (QStringList {
            "https://example.com/a.mp4",
            fileMrl(m_file),
            "udp://@239.0.0.1:1234",
        }));
    }

    void copiedFiles()
    {
        /* files copied in a file manager arrive as a URL list */
        QMimeData data;
        data.setUrls({ QUrl::fromLocalFile(m_file), QUrl::fromLocalFile(m_dir.path()) });
        QCOMPARE(mrlsFromMimeData(data), (QStringList {
            fileMrl(m_file), fileMrl(m_dir.path())
        }));
    }

    void copiedText()
    {
        QMimeData data;
        data.setText("https://example.com/a.mp4");
        QCOMPARE(mrlsFromMimeData(data), QStringList { "https://example.com/a.mp4" });

        QMimeData empty;
        QCOMPARE(mrlsFromMimeData(empty), QStringList {});
    }

    void networkLinks()
    {
        QVERIFY(isNetworkMrl("https://example.com/a.mp4"));
        QVERIFY(isNetworkMrl("rtsp://192.168.1.10:554/cam"));
        QVERIFY(isNetworkMrl("udp://@239.0.0.1:1234"));
        QVERIFY(isNetworkMrl("smb://nas/videos/a.mkv"));

        QVERIFY(!isNetworkMrl(fileMrl(m_file)));
        QVERIFY(!isNetworkMrl("dvd:///D:/"));
        QVERIFY(!isNetworkMrl("screen://"));
        QVERIFY(!isNetworkMrl("v4l2:///dev/video0"));
    }
};

QTEST_GUILESS_MAIN(TestPastedMedia)
#include "test_pasted_media.moc"
