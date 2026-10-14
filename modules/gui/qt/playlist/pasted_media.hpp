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
#ifndef VLC_QT_PASTED_MEDIA_HPP_
#define VLC_QT_PASTED_MEDIA_HPP_

#include <QString>
#include <QStringList>

class QMimeData;

namespace vlc {
namespace playlist {

/**
 * The media MRLs of pasted text: one per line that is a link
 * ("scheme://…") or the absolute path of an existing file or folder.
 * Other lines are ignored.
 */
QStringList mrlsFromText(const QString &text);

/**
 * The media MRLs of pasted content: its URL list (copied files), or else the
 * links and paths of its text.
 */
QStringList mrlsFromMimeData(const QMimeData &data);

/**
 * Whether a MRL points to a network host; such links are kept as
 * Saved streams when opened.
 */
bool isNetworkMrl(const QString &mrl);

} // namespace playlist
} // namespace vlc

#endif // VLC_QT_PASTED_MEDIA_HPP_
