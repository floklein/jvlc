/*****************************************************************************
 * medialib.c: Player/Media Library interractions
 *****************************************************************************
 * Copyright © 2018-2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_arrays.h>
#include "player.h"
#include "misc/variables.h"

/* Subtitle matches: attach the external subtitle files saved for the media.
 * The input is not started yet, so they load like the slaves of the item. */
static void
vlc_player_input_AttachMlSubtitles(struct vlc_player_input *input,
                                   const vlc_ml_media_t *media)
{
    if (media->p_files == NULL)
        return;
    input_item_t *item = input_GetItem(input->thread);
    bool first = true;

    vlc_mutex_lock(&item->lock);
    for (size_t i = 0; i < media->p_files->i_nb_items; ++i)
    {
        const vlc_ml_file_t *file = &media->p_files->p_items[i];
        if (file->i_type != VLC_ML_FILE_TYPE_SUBTITLE || !file->b_external
         || file->psz_mrl == NULL)
            continue;

        bool attached = false;
        for (int j = 0; j < item->i_slaves && !attached; ++j)
            attached = !strcmp(item->pp_slaves[j]->psz_uri, file->psz_mrl);
        if (attached)
        {
            first = false;
            continue;
        }

        /* the first one shows, like a subtitle file named after the video,
         * the others are only listed; a missing file is skipped */
        input_item_slave_t *slave =
            input_item_slave_New(file->psz_mrl, SLAVE_TYPE_SPU,
                                 first ? SLAVE_PRIORITY_MATCH_ALL
                                       : SLAVE_PRIORITY_MATCH_LEFT);
        if (slave == NULL)
            break;
        first = false;
        TAB_APPEND(item->i_slaves, item->pp_slaves, slave);
    }
    vlc_mutex_unlock(&item->lock);
}

void
vlc_player_input_RestoreMlStates(struct vlc_player_input* input, bool force_pos)
{
    vlc_player_t* player = input->player;
    vlc_player_assert_locked(player);

    int restore_pos;
    if (force_pos)
        restore_pos = VLC_PLAYER_RESTORE_PLAYBACK_POS_ALWAYS;
    else
        restore_pos = var_InheritInteger(input->thread, "restore-playback-pos");

    const bool restore_states = var_InheritBool(input->thread, "restore-playback-states");

    vlc_medialibrary_t* ml = vlc_ml_instance_get(input->player);
    if (!ml)
        return;
    input_item_t* item = input_GetItem(input->thread);
    vlc_ml_media_t* media = vlc_ml_get_media_by_mrl( ml, item->psz_uri);
    if (!media)
        return;
    vlc_player_input_AttachMlSubtitles(input, media);
    /* Video and music remember their playback, other media don't */
    const bool video = media->i_type == VLC_ML_MEDIA_TYPE_VIDEO;
    if (!video && media->i_type != VLC_ML_MEDIA_TYPE_AUDIO)
    {
        vlc_ml_release(media);
        return;
    }
    /* Asked again by vlc_player_RestorePlaybackPos() */
    vlc_ml_release(&input->ml.states);
    input->ml.states.current_video_track = input->ml.states.current_audio_track =
        input->ml.states.current_subtitle_track = NULL;
    input->ml.states.aspect_ratio = input->ml.states.crop =
        input->ml.states.deinterlace = input->ml.states.video_filter = NULL;
    if (vlc_ml_media_get_all_playback_pref(ml, media->i_id,
                                           &input->ml.states) != VLC_SUCCESS)
    {
        vlc_ml_release(media);
        return;
    }

    input->ml.restore = (restore_pos == VLC_PLAYER_RESTORE_PLAYBACK_POS_ALWAYS) ?
                            VLC_RESTOREPOINT_TITLE : VLC_RESTOREPOINT_NONE;
    input->ml.restore_states = restore_states;
    /* If we are aiming at a specific title, wait for it to be added, and
     * only then select it & set the position.
     * If we're not aiming at a specific title, just set the position now.
     */
    if (restore_pos == VLC_PLAYER_RESTORE_PLAYBACK_POS_ALWAYS &&
            input->ml.states.current_title == -1 &&
            media->f_progress > .0f)
        input_SetPosition(input->thread, media->f_progress, false);
    else if (restore_pos == VLC_PLAYER_RESTORE_PLAYBACK_POS_ASK &&
             media->f_progress > .0f)
    {
        input->ml.delay_restore = true;
        input->ml.pos = media->f_progress;
    }

    vlc_ml_release(media);

    /* The speed is the viewer's, for every media: it is not restored. The
     * delays are set with the initial ones, see
     * vlc_player_input_GetMlDelays() */
    if (!restore_states)
        return;

    const char *video_track_ids = input->ml.states.current_video_track;
    const char *audio_track_ids = input->ml.states.current_audio_track;
    const char *subtitle_track_ids = input->ml.states.current_subtitle_track;

    if (video_track_ids)
        vlc_player_input_SelectTracksByStringIds(input, VIDEO_ES,
                                                 video_track_ids);
    if (audio_track_ids)
        vlc_player_input_SelectTracksByStringIds(input, AUDIO_ES,
                                                 audio_track_ids);
    if (subtitle_track_ids)
        vlc_player_input_SelectTracksByStringIds(input, SPU_ES,
                                                 subtitle_track_ids);

    /* The video filters belong to the interface (its adjustments and
     * presets): they are neither restored nor reset here */
    vout_thread_t* vout = video ? vlc_player_vout_Hold(player) : NULL;
    if (vout != NULL)
    {
        if (input->ml.states.zoom >= .0f)
            var_SetFloat(vout, "zoom", input->ml.states.zoom);
        else
            var_SetFloat(vout, "zoom", 1.f);
        if (input->ml.states.aspect_ratio)
            var_SetString(vout, "aspect-ratio", input->ml.states.aspect_ratio);
        else
            var_SetString(vout, "aspect-ratio", NULL);
        if (input->ml.states.deinterlace)
        {
            var_SetString(vout, "deinterlace-mode", input->ml.states.deinterlace);
            var_SetInteger(vout, "deinterlace", 1);
        }
        else
        {
            /* back to the preferences */
            char *mode = var_InheritString(player, "deinterlace-mode");
            var_SetString(vout, "deinterlace-mode", mode);
            free(mode);
            var_SetInteger(vout, "deinterlace",
                           var_InheritInteger(player, "deinterlace"));
        }
        vout_Release(vout);
    }
}

void
vlc_player_input_GetMlDelays(struct vlc_player_input *input,
                             vlc_tick_t delays[DATA_ES])
{
    if (!input->ml.restore_states)
        return;
    if (input->ml.states.has_audio_delay)
        delays[AUDIO_ES] = VLC_TICK_FROM_US(input->ml.states.audio_delay);
    if (input->ml.states.has_subtitle_delay)
        delays[SPU_ES] = VLC_TICK_FROM_US(input->ml.states.subtitle_delay);
}

static bool
vlc_player_UpdateMediaType(const struct vlc_player_input* input,
                           vlc_medialibrary_t* ml, vlc_ml_media_t* media)
{
    assert(media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN);
    vlc_ml_media_type_t media_type;
    if (input->ml.has_video_tracks)
        media_type = VLC_ML_MEDIA_TYPE_VIDEO;
    else if (input->ml.has_audio_tracks)
        media_type = VLC_ML_MEDIA_TYPE_AUDIO;
    else
        return false;
    if (vlc_ml_media_set_type(ml, media->i_id, media_type) != VLC_SUCCESS)
        return false;
    media->i_type = media_type;
    return true;
}

static void
vlc_player_CompareAssignState(char **target_ptr, char **input_ptr)
{
    /* We only want to save these states if they are different, and not the
     * default values (NULL), so this means that either one is NULL and the
     * other isn't, or they are both non null and differ lexicographically */

    char *target = *target_ptr;
    char *input = *input_ptr;
    if ( ( input != NULL && target != NULL &&
           strcmp(input, target ) ) ||
         ( input == NULL && target != NULL ) ||
         ( input != NULL && target == NULL ) )
    {
        free(target);
        *target_ptr = input;
        *input_ptr = NULL;
    }
    else
    {
        free(target);
        *target_ptr = NULL;
    }
}

void
vlc_player_UpdateMLStates(vlc_player_t *player, struct vlc_player_input* input)
{
    /* Do not save states for any secondary player. If this player's parent is
       the main vlc object, then it's the main player */
    if (player->obj.priv->parent != (vlc_object_t*)vlc_object_instance(player))
        return;

    vlc_medialibrary_t* ml = vlc_ml_instance_get(player);
    if (!ml)
        return;
    input_item_t* item = input_GetItem(input->thread);
    if (!item)
        return;
    vlc_ml_media_t* media = vlc_ml_get_media_by_mrl(ml, item->psz_uri);
    if (!media)
    {
        /* We don't know this media yet, let's add it as an external media so
         * we can still store its playback preferences
         */
        media = vlc_ml_new_external_media(ml, item->psz_uri);
        if (media == NULL)
            return;
    }

    if (media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN)
    {
        if (!vlc_player_UpdateMediaType(input, ml, media))
        {
            vlc_ml_release( media );
            return;
        }
    }
    assert(media->i_type != VLC_ML_MEDIA_TYPE_UNKNOWN);

    if ( var_GetBool( input->thread, "save-recentplay" ) )
        vlc_ml_media_update_progress( ml, media->i_id, input->position );

    /* If the value changed during the playback, update it in the medialibrary.
     * If not, set each state to their "unset" values, so that they aren't saved
     * in database */
    if ((input->ml.states.current_title == -1 && input->title_selected != 0) ||
         (input->ml.states.current_title != -1 &&
          input->ml.states.current_title != (int)input->title_selected))
    {
        input->ml.states.current_title = input->title_selected;
    }
    else
        input->ml.states.current_title = -1;

    /* The speed is the viewer's, for every media: .0f leaves it unsaved */
    input->ml.states.rate = .0f;

    /* The delays the media played with, when they changed during the
     * playback, a delay put back to zero too */
    input->ml.states.has_audio_delay =
        input->cat_delays[AUDIO_ES] != input->ml.opened_delays[AUDIO_ES];
    input->ml.states.audio_delay = US_FROM_VLC_TICK(input->cat_delays[AUDIO_ES]);
    input->ml.states.has_subtitle_delay =
        input->cat_delays[SPU_ES] != input->ml.opened_delays[SPU_ES];
    input->ml.states.subtitle_delay = US_FROM_VLC_TICK(input->cat_delays[SPU_ES]);

    /* The video filters belong to the interface */
    free(input->ml.states.video_filter);
    input->ml.states.video_filter = NULL;

    struct vlc_player_track_priv* t;
    vout_thread_t* vout = NULL;

    vlc_vector_foreach(t, &input->video_track_vector)
    {
        if (!t->t.selected)
            continue;
        enum vlc_vout_order order;
        vout = vlc_player_GetEsIdVout(player, t->t.es_id, &order);
        if (vout != NULL && order == VLC_VOUT_ORDER_PRIMARY)
            break;
        vout = NULL;
    }
    if (vout != NULL)
    {
        char* aspect_ratio = var_GetNonEmptyString(vout, "aspect-ratio");
        char* crop = var_GetNonEmptyString(vout, "crop");
        char* deinterlace = var_GetNonEmptyString(vout, "deinterlace-mode");

        vlc_player_CompareAssignState(&input->ml.states.aspect_ratio, &aspect_ratio);
        vlc_player_CompareAssignState(&input->ml.states.crop, &crop);
        vlc_player_CompareAssignState(&input->ml.states.deinterlace, &deinterlace);

        free(deinterlace);
        free(crop);
        free(aspect_ratio);

        if (input->ml.states.deinterlace != NULL &&
            !strcmp(input->ml.states.deinterlace, "auto"))
        {
            free(input->ml.states.deinterlace);
            input->ml.states.deinterlace = NULL;
        }

        float zoom = var_GetFloat(vout, "zoom");
        if (zoom != input->ml.states.zoom &&
            (zoom != 1.f && input->ml.states.zoom >= .0f))
            input->ml.states.zoom = zoom;
        else
            input->ml.states.zoom = -1.f;
    }

    /* The tracks are gone: the ones picked are kept as they are selected */
    char *video_track_ids = input->ml.picked_tracks[VIDEO_ES];
    char *audio_track_ids = input->ml.picked_tracks[AUDIO_ES];
    char *subtitle_track_ids = input->ml.picked_tracks[SPU_ES];
    input->ml.picked_tracks[VIDEO_ES] = input->ml.picked_tracks[AUDIO_ES] =
        input->ml.picked_tracks[SPU_ES] = NULL;

    vlc_player_CompareAssignState(&input->ml.states.current_video_track, &video_track_ids);
    vlc_player_CompareAssignState(&input->ml.states.current_audio_track, &audio_track_ids);
    vlc_player_CompareAssignState(&input->ml.states.current_subtitle_track, &subtitle_track_ids);

    free(video_track_ids);
    free(audio_track_ids);
    free(subtitle_track_ids);

    vlc_ml_media_set_all_playback_states(ml, media->i_id, &input->ml.states);

    vlc_ml_release(&input->ml.states);
    vlc_ml_release(media);
}

void
vlc_player_RestorePlaybackPos(vlc_player_t *player)
{
    vlc_player_input_RestoreMlStates(player->input, true);
}

int
vlc_player_GetMediaMemory(vlc_player_t *player,
                          struct vlc_player_media_memory *memory)
{
    vlc_player_assert_locked(player);
    memset(memory, 0, sizeof(*memory));
    memory->position = -1.f;

    struct vlc_player_input *input = player->input;
    if (input == NULL)
        return VLC_EGENERIC;

    memory->position = input->ml.pos;
    if (!input->ml.restore_states)
        return VLC_SUCCESS;

    const vlc_ml_playback_states_all *states = &input->ml.states;
    memory->tracks[VIDEO_ES] = states->current_video_track != NULL;
    memory->tracks[AUDIO_ES] = states->current_audio_track != NULL;
    memory->tracks[SPU_ES] = states->current_subtitle_track != NULL;
    memory->has_delays[AUDIO_ES] = states->has_audio_delay;
    if (states->has_audio_delay)
        memory->delays[AUDIO_ES] = VLC_TICK_FROM_US(states->audio_delay);
    memory->has_delays[SPU_ES] = states->has_subtitle_delay;
    if (states->has_subtitle_delay)
        memory->delays[SPU_ES] = VLC_TICK_FROM_US(states->subtitle_delay);
    return VLC_SUCCESS;
}
