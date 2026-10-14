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

#ifndef MLMUSICFOLDERTREEMODEL_HPP
#define MLMUSICFOLDERTREEMODEL_HPP

#include "mlaudiomodel.hpp"

#include <QVariantList>

// One level of the Songs folder tree, the music of the library as it is on
// disk.
//
// Without a parentId, it lists the folders the user added to the library.
// With a folder as parentId, it lists the subfolders of that folder, then its
// tracks. Folders without any music, at any depth, are left out (except the
// added folders that hold no media yet, being scanned or empty).
//
// Sort criteria, for the tracks:
// - "title": by title, numbers in natural order;
// - "main_artist": by artist, then album, disc, track and title;
// - "album_title": by album, then disc, track and title;
// - "track_number": by track number, then title;
// - "disc_number": by disc, then track and title;
// - "insertion": by the date they were added to the library.
// Tracks without the artist or album sorted on come last; descending reverses
// the whole order. Folders always come first, by title (reversed only by a
// descending "title").
// The search pattern keeps the folders whose title contains it, and the
// tracks whose title, artist or album contains it.
class MLMusicFolderTreeModel : public MLAudioModel
{
    Q_OBJECT

    // The folders from the top of the tree to parentId, as a list of
    // { id: MLItemId, title: string }; empty at the top
    Q_PROPERTY(QVariantList path READ path NOTIFY pathChanged FINAL)

public:
    enum Roles
    {
        FOLDER_IS_FOLDER = MLAudioModel::AUDIO_ROLES_COUNT,
    };

    explicit MLMusicFolderTreeModel(QObject *parent = nullptr);

    QVariantList path() const { return m_path; }

signals:
    void pathChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

    QVariant itemRoleData(const MLItem *item, int role) const override;

    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

    void onVlcMlEvent(const MLEvent &event) override;

private:
    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    void updatePath();

    struct Loader : public MLListCacheLoader::MLOp
    {
        Loader(const MLMusicFolderTreeModel &model);

        size_t count(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *queryParams) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *queryParams) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t *ml, MLItemId itemId) const override;

    private:
        // every row of the level, filtered and sorted
        std::vector<std::unique_ptr<MLItem>> rows(vlc_medialibrary_t *ml) const;

        QString m_criteria;
    };

    QVariantList m_path;
    quint64 m_pathTask = 0;
};

#endif // MLMUSICFOLDERTREEMODEL_HPP
