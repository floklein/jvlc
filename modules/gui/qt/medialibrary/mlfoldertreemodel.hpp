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

#ifndef MLFOLDERTREEMODEL_HPP
#define MLFOLDERTREEMODEL_HPP

#include "mlvideomodel.hpp"

#include <QVariantList>

class QCollator;
class MLFolder;

// The library folder tree, shared by Videos (MLFolderTreeModel) and Songs
// (MLMusicFolderTreeModel). Run on the ML thread.
namespace mlfoldertree
{
    bool isFolder(const MLItem *item);

    // whether the event changes the folders of the tree
    bool changesFolders(int eventType);

    // The folders of one level of the tree that hold media of the type at any
    // depth: the folders the user added when parent is unset (with those that
    // hold no media yet, being scanned or empty), the subfolders of parent
    // otherwise. Only the folders whose title contains pattern are kept.
    // They come by title, in natural order, reversed when titleDesc.
    std::vector<std::unique_ptr<MLFolder>> folders(vlc_medialibrary_t *ml, MLItemId parent,
                                                   vlc_ml_media_type_t type,
                                                   const QString &pattern, bool titleDesc);

    // the folders from the top of the tree to parent, as a list of
    // { id: MLItemId, title: string }
    QVariantList path(vlc_medialibrary_t *ml, MLItemId parent);

    // Compares titles with their numbers in natural order (S2 before S10)
    int naturalCompare(const QCollator &collator, const QString &a, const QString &b);

    // the requested page of the rows of a level
    std::vector<std::unique_ptr<MLItem>> page(std::vector<std::unique_ptr<MLItem>> rows,
                                              const vlc_ml_query_params_t *queryParams);
}

// One level of the Videos folder tree.
//
// Without a parentId, it lists the folders the user added to the library.
// With a folder as parentId, it lists the subfolders of that folder, then its
// videos. Folders without any video, at any depth, are left out (except the
// added folders that hold no media yet, being scanned or empty).
//
// Sort criteria:
// - "title": by title, numbers in natural order (S2 before S10);
// - "insertion": the videos by the date they were added to the library;
// - "unwatched": the videos in progress, then the unwatched ones, then the
//   watched ones, each by title; descending puts the watched ones first.
// Folders always come first, by title (reversed only by a descending "title").
// The search pattern keeps the rows whose title contains it.
class MLFolderTreeModel : public MLVideoModel
{
    Q_OBJECT

    // The folders from the top of the tree to parentId, as a list of
    // { id: MLItemId, title: string }; empty at the top
    Q_PROPERTY(QVariantList path READ path NOTIFY pathChanged FINAL)

public:
    enum Roles
    {
        FOLDER_IS_FOLDER = MLVideoModel::VIDEO_ROLES_COUNT,
        FOLDER_NB_VIDEOS,
    };

    explicit MLFolderTreeModel(QObject *parent = nullptr);

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
        Loader(const MLFolderTreeModel &model);

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

#endif // MLFOLDERTREEMODEL_HPP
