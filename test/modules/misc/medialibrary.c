/*****************************************************************************
 * medialibrary.c: test for the medialibrary code
 *****************************************************************************
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_misc_medialibrary
#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_image.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_filter.h>

#include <vlc_media_library.h>
#include <ftw.h>
#include <sys/stat.h>

#include "../lib/libvlc_internal.h"

const char vlc_module_name[] = MODULE_STRING;

static int exitcode = 0;

static void ValidateThumbnail(void *data, const vlc_ml_event_t *event)
{
    if (event->i_type != VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED)
        return;

    assert(event->media_thumbnail_generated.b_success);
    vlc_sem_t *sem = data;
    vlc_sem_post(sem);
}

static const char *tempdir;

static void WaitDiscovery(void *data, const vlc_ml_event_t *event)
{
    if (event->i_type != VLC_ML_EVENT_DISCOVERY_COMPLETED)
        return;

    vlc_sem_t *sem = data;
    vlc_sem_post(sem);
}

static void MakeDir(const char *path)
{
    char *dir;
    if (asprintf(&dir, "%s/%s", tempdir, path) < 0)
        abort();
    int ret = mkdir(dir, 0700);
    assert(ret == 0);
    (void) ret;
    free(dir);
}

static char *FolderMrl(const char *path)
{
    char *mrl;
    if (asprintf(&mrl, "file://%s/%s", tempdir, path) < 0)
        abort();
    return mrl;
}

static vlc_ml_folder_t *FolderByPath(vlc_medialibrary_t *ml, const char *path)
{
    char *mrl = FolderMrl(path);
    vlc_ml_folder_t *folder = vlc_ml_get_folder_by_mrl(ml, mrl);
    free(mrl);
    return folder;
}

/* The folder tree of the library: subfolders with sort and paging, the parent
 * of a folder and a folder found by its path */
static void TestFolderTree(vlc_medialibrary_t *ml)
{
    MakeDir("lib");
    MakeDir("lib/Shows");
    MakeDir("lib/Shows/S01");
    MakeDir("lib/Movies");

    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);
    vlc_ml_event_callback_t *listener =
        vlc_ml_event_register_callback(ml, WaitDiscovery, &sem);

    char *rootMrl = FolderMrl("lib/");
    vlc_ml_add_folder(ml, rootMrl);
    free(rootMrl);
    vlc_sem_wait(&sem);
    vlc_ml_event_unregister_callback(ml, listener);

    /* by path, with or without the trailing slash */
    vlc_ml_folder_t *root = FolderByPath(ml, "lib/");
    assert(root != NULL);
    assert(strcmp(root->psz_name, "lib") == 0);
    vlc_ml_folder_t *shows = FolderByPath(ml, "lib/Shows");
    assert(shows != NULL);
    assert(strcmp(shows->psz_name, "Shows") == 0);
    assert(FolderByPath(ml, "lib/Nothing") == NULL);

    /* subfolders, sorted and paged */
    vlc_ml_query_params_t params = vlc_ml_query_params_create();
    params.i_sort = VLC_ML_SORTING_ALPHA;
    params.b_desc = true;
    vlc_ml_folder_list_t *list = vlc_ml_list_subfolders(ml, &params, root->i_id);
    assert(list != NULL && list->i_nb_items == 2);
    assert(strcmp(list->p_items[0].psz_name, "Shows") == 0);
    assert(strcmp(list->p_items[1].psz_name, "Movies") == 0);
    vlc_ml_folder_list_release(list);

    params.b_desc = false;
    params.i_nbResults = 1;
    params.i_offset = 1;
    list = vlc_ml_list_subfolders(ml, &params, root->i_id);
    assert(list != NULL && list->i_nb_items == 1);
    assert(strcmp(list->p_items[0].psz_name, "Shows") == 0);
    vlc_ml_folder_list_release(list);
    assert(vlc_ml_count_subfolders(ml, NULL, root->i_id) == 2);

    /* parents, up to the folder the user added */
    vlc_ml_folder_t *s01 = FolderByPath(ml, "lib/Shows/S01/");
    assert(s01 != NULL);
    vlc_ml_folder_t *parent = vlc_ml_get_folder_parent(ml, s01->i_id);
    assert(parent != NULL && parent->i_id == shows->i_id);
    vlc_ml_folder_release(parent);
    parent = vlc_ml_get_folder_parent(ml, shows->i_id);
    assert(parent != NULL && parent->i_id == root->i_id);
    vlc_ml_folder_release(parent);
    assert(vlc_ml_get_folder_parent(ml, root->i_id) == NULL);

    vlc_ml_folder_release(s01);
    vlc_ml_folder_release(shows);
    vlc_ml_folder_release(root);
}

static int OpenIntf(vlc_object_t *root)
{
    vlc_medialibrary_t *ml = vlc_ml_instance_get(root);
    if (ml == NULL)
    {
        exitcode = 77;
        return VLC_SUCCESS;
    }

    TestFolderTree(ml);

    #define MOCK_URL "mock://video_track_count=1;length=100000000;" \
                     "video_width=800;video_height=600"
    vlc_ml_media_t *media = vlc_ml_new_external_media(ml, MOCK_URL);

    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);

    vlc_ml_event_callback_t *listener =
        vlc_ml_event_register_callback(ml, ValidateThumbnail, &sem);
    vlc_ml_media_generate_thumbnail(ml, media->i_id,
            VLC_ML_THUMBNAIL_SMALL, 800, 600, 0.f);

    vlc_sem_wait(&sem);
    vlc_ml_event_unregister_callback(ml, listener);

    vlc_ml_media_release(media);

    return VLC_SUCCESS;
}

/** Inject the mocked modules as a static plugin: **/
vlc_module_begin()
    set_callback(OpenIntf)
    set_capability("interface", 0)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

static int cleanup_tmpdir(const char *dirpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf)
{
    (void)sb; (void)typeflag; (void)ftwbuf;
    return remove(dirpath);
}

int main(void)
{
    char template[] = "/tmp/vlc.test." MODULE_STRING ".XXXXXX";
    tempdir = mkdtemp(template);
    if (tempdir == NULL)
    {
        assert(tempdir != NULL);
        return -1;
    }
    fprintf(stderr, "Using VLC_USERDATA_PATH directory %s\n", tempdir);
    setenv("VLC_USERDATA_PATH", tempdir, 1);

    test_init();

    const char * const args[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse", "--media-library"
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_InternalAddIntf(vlc->p_libvlc_int, MODULE_STRING);
    libvlc_InternalPlay(vlc->p_libvlc_int);

    libvlc_release(vlc);

    /* Remove temporary directory */
    nftw(tempdir, cleanup_tmpdir, FOPEN_MAX, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
    return exitcode;
}
