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

#include "pasted_media.hpp"

#include <vlc_common.h>
#include <vlc_url.h>

#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QRegularExpression>
#include <QUrl>

namespace vlc {
namespace playlist {

namespace {

QString takeMrl(char *mrl)
{
    if (!mrl)
        return {};
    QString result = QString::fromUtf8(mrl);
    free(mrl);
    return result;
}

/* The MRL of one line of text, or an empty string */
QString mrlFromLine(QString line)
{
    line = line.trimmed();

    /* "Copy as path" quotes paths */
    if (line.size() >= 2
        && ((line.startsWith('"') && line.endsWith('"'))
            || (line.startsWith('\'') && line.endsWith('\''))))
        line = line.mid(1, line.size() - 2).trimmed();

    if (line.isEmpty())
        return {};

    /* A link: "scheme://…", as a single word */
    static const QRegularExpression link(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*://\\S*$"));
    if (link.match(line).hasMatch())
        return takeMrl(vlc_uri_fixup(line.toUtf8().constData()));

    /* A path: absolute, to a file or folder that exists */
    const QFileInfo file(line);
    if (file.isAbsolute() && file.exists())
    {
        const QString path = QDir::toNativeSeparators(file.absoluteFilePath());
        return takeMrl(vlc_path2uri(path.toUtf8().constData(), nullptr));
    }

    return {};
}

} // anonymous namespace

QStringList mrlsFromText(const QString &text)
{
    static const QRegularExpression lineBreaks(QStringLiteral("[\r\n]+"));

    QStringList mrls;
    for (const QString &line : text.split(lineBreaks, Qt::SkipEmptyParts))
    {
        QString mrl = mrlFromLine(line);
        if (!mrl.isEmpty())
            mrls.push_back(std::move(mrl));
    }
    return mrls;
}

QStringList mrlsFromMimeData(const QMimeData &data)
{
    QStringList mrls;
    if (data.hasUrls())
    {
        for (const QUrl &url : data.urls())
        {
            if (url.isValid() && !url.isRelative())
                mrls.push_back(QString::fromUtf8(url.toEncoded()));
        }
    }

    if (mrls.isEmpty() && data.hasText())
        mrls = mrlsFromText(data.text());

    return mrls;
}

bool isNetworkMrl(const QString &mrl)
{
    const QUrl url(mrl);
    return url.isValid() && !url.isLocalFile() && !url.host().isEmpty();
}

} // namespace playlist
} // namespace vlc
