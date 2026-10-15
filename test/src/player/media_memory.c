// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * media_memory.c: per-file memory test
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

#include <vlc_media_library.h>

/* Each media remembers the subtitle and audio delays of its last playback in
 * the playback states of the media library, and gets them back when it is
 * opened again. */

static void
play(struct ctx *ctx, const char *name, const struct media_params *params)
{
    player_set_current_mock_media(ctx, name, params, false);
    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);
}

static void
set_delays(struct ctx *ctx, vlc_tick_t audio, vlc_tick_t subtitle)
{
    vlc_player_t *player = ctx->player;
    vlc_player_SetAudioDelay(player, audio, VLC_PLAYER_WHENCE_ABSOLUTE);
    vlc_player_SetSubtitleDelay(player, subtitle, VLC_PLAYER_WHENCE_ABSOLUTE);
    assert(vlc_player_GetAudioDelay(player) == audio);
    assert(vlc_player_GetSubtitleDelay(player) == subtitle);
}

/* what the media library keeps of a media */
static vlc_ml_playback_states_all
saved_states(struct ctx *ctx, const struct media_params *params)
{
    vlc_medialibrary_t *ml = vlc_ml_instance_get(ctx->vlc->p_libvlc_int);
    assert(ml != NULL);
    input_item_t *item = create_mock_media("saved", params);
    vlc_ml_media_t *media = vlc_ml_get_media_by_mrl(ml, item->psz_uri);
    input_item_Release(item);
    assert(media != NULL);

    vlc_ml_playback_states_all states;
    int ret = vlc_ml_media_get_all_playback_pref(ml, media->i_id, &states);
    assert(ret == VLC_SUCCESS);
    vlc_ml_release(media);
    return states;
}

static void
assert_no_memory(vlc_player_t *player)
{
    struct vlc_player_media_memory memory;
    int ret = vlc_player_GetMediaMemory(player, &memory);
    assert(ret == VLC_SUCCESS);
    assert(!memory.has_delays[AUDIO_ES]);
    assert(!memory.has_delays[SPU_ES]);
    assert(!memory.tracks[AUDIO_ES]);
    assert(!memory.tracks[SPU_ES]);
}

static void
test_delays_are_remembered_per_media(struct ctx *ctx)
{
    test_log("delays_are_remembered_per_media\n");
    vlc_player_t *player = ctx->player;

    struct media_params episode = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    struct media_params other = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(11));

    play(ctx, "episode", &episode);
    assert_no_memory(player);
    set_delays(ctx, VLC_TICK_FROM_MS(200), VLC_TICK_FROM_MS(-300));
    test_end(ctx);

    /* kept in the existing playback-state slots */
    vlc_ml_playback_states_all states = saved_states(ctx, &episode);
    assert(states.has_audio_delay);
    assert(states.audio_delay == 200000);
    assert(states.has_subtitle_delay);
    assert(states.subtitle_delay == -300000);
    vlc_ml_release(&states);

    /* back when the media is opened again, before it plays */
    player_set_current_mock_media(ctx, "episode", &episode, false);
    assert(vlc_player_GetAudioDelay(player) == VLC_TICK_FROM_MS(200));
    assert(vlc_player_GetSubtitleDelay(player) == VLC_TICK_FROM_MS(-300));

    /* the interface learns they come from this media */
    struct vlc_player_media_memory memory;
    int ret = vlc_player_GetMediaMemory(player, &memory);
    assert(ret == VLC_SUCCESS);
    assert(memory.has_delays[AUDIO_ES]);
    assert(memory.delays[AUDIO_ES] == VLC_TICK_FROM_MS(200));
    assert(memory.has_delays[SPU_ES]);
    assert(memory.delays[SPU_ES] == VLC_TICK_FROM_MS(-300));
    assert(!memory.has_delays[VIDEO_ES]);

    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);
    assert(vlc_player_GetAudioDelay(player) == VLC_TICK_FROM_MS(200));
    assert(vlc_player_GetSubtitleDelay(player) == VLC_TICK_FROM_MS(-300));

    /* a delay put back to zero is remembered too */
    set_delays(ctx, 0, VLC_TICK_FROM_MS(-300));
    test_end(ctx);

    /* another media keeps its own */
    play(ctx, "other", &other);
    assert(vlc_player_GetAudioDelay(player) == 0);
    assert(vlc_player_GetSubtitleDelay(player) == 0);
    assert_no_memory(player);
    test_end(ctx);

    play(ctx, "episode", &episode);
    assert(vlc_player_GetAudioDelay(player) == 0);
    assert(vlc_player_GetSubtitleDelay(player) == VLC_TICK_FROM_MS(-300));
    test_end(ctx);
}

static void
test_music_remembers_too(struct ctx *ctx)
{
    test_log("music_remembers_too\n");
    vlc_player_t *player = ctx->player;

    struct media_params song = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(12));
    song.track_count[VIDEO_ES] = 0;

    play(ctx, "song", &song);
    set_delays(ctx, VLC_TICK_FROM_MS(-150), VLC_TICK_FROM_MS(400));
    test_end(ctx);

    play(ctx, "song", &song);
    assert(vlc_player_GetAudioDelay(player) == VLC_TICK_FROM_MS(-150));
    assert(vlc_player_GetSubtitleDelay(player) == VLC_TICK_FROM_MS(400));
    test_end(ctx);
}

static void
test_picked_tracks_are_remembered(struct ctx *ctx)
{
    test_log("picked_tracks_are_remembered\n");
    vlc_player_t *player = ctx->player;

    struct media_params movie = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(15));
    movie.track_count[SPU_ES] = 2;

    /* subtitles are not shown by default: the viewer picks the second one */
    play(ctx, "movie", &movie);
    while (vlc_player_GetSubtitleTrackCount(player) < 2)
        vlc_player_CondWait(player, &ctx->wait);
    const struct vlc_player_track *track = vlc_player_GetSubtitleTrackAt(player, 1);
    char *picked = strdup(vlc_es_id_GetStrId(track->es_id));
    assert(picked != NULL);
    vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
    while (!vlc_player_GetSubtitleTrackAt(player, 1)->selected)
        vlc_player_CondWait(player, &ctx->wait);
    test_end(ctx);

    vlc_ml_playback_states_all states = saved_states(ctx, &movie);
    assert(states.current_subtitle_track != NULL);
    assert(strcmp(states.current_subtitle_track, picked) == 0);
    /* the automatic choices are not the viewer's */
    assert(states.current_audio_track == NULL);
    vlc_ml_release(&states);

    /* the interface learns this media has its own subtitles: they win over
     * the preferred language */
    play(ctx, "movie", &movie);
    struct vlc_player_media_memory memory;
    int ret = vlc_player_GetMediaMemory(player, &memory);
    assert(ret == VLC_SUCCESS);
    assert(memory.tracks[SPU_ES]);
    assert(!memory.tracks[AUDIO_ES]);

    for (;;)
    {
        const struct vlc_player_track *selected =
            vlc_player_GetSelectedTrack(player, SPU_ES);
        if (selected != NULL)
        {
            assert(strcmp(vlc_es_id_GetStrId(selected->es_id), picked) == 0);
            break;
        }
        vlc_player_CondWait(player, &ctx->wait);
    }
    free(picked);
    test_end(ctx);
}

static void
test_defaults_are_not_remembered(struct ctx *ctx)
{
    test_log("defaults_are_not_remembered\n");
    vlc_player_t *player = ctx->player;

    struct media_params movie = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(16));
    movie.track_count[SPU_ES] = 2;

    /* the interface applies the preferences of the media's folder (its
     * Preset): they aren't the media's own */
    play(ctx, "movie", &movie);
    int ret = vlc_player_SetCategoryDefaultDelay(player, AUDIO_ES,
                                                 VLC_TICK_FROM_MS(250));
    assert(ret == VLC_SUCCESS);
    assert(vlc_player_GetAudioDelay(player) == VLC_TICK_FROM_MS(250));
    while (vlc_player_GetSubtitleTrackCount(player) < 2)
        vlc_player_CondWait(player, &ctx->wait);
    vlc_player_SelectDefaultTrack(player, vlc_player_GetSubtitleTrackAt(player, 1));
    while (!vlc_player_GetSubtitleTrackAt(player, 1)->selected)
        vlc_player_CondWait(player, &ctx->wait);
    test_end(ctx);

    vlc_ml_playback_states_all states = saved_states(ctx, &movie);
    assert(!states.has_audio_delay);
    assert(states.current_subtitle_track == NULL);
    vlc_ml_release(&states);

    /* the viewer's own change after them is */
    play(ctx, "movie", &movie);
    assert_no_memory(player);
    ret = vlc_player_SetCategoryDefaultDelay(player, SPU_ES, VLC_TICK_FROM_MS(100));
    assert(ret == VLC_SUCCESS);
    vlc_player_SetSubtitleDelay(player, VLC_TICK_FROM_MS(300),
                                VLC_PLAYER_WHENCE_ABSOLUTE);
    test_end(ctx);

    states = saved_states(ctx, &movie);
    assert(states.has_subtitle_delay);
    assert(states.subtitle_delay == 300000);
    assert(!states.has_audio_delay);
    vlc_ml_release(&states);
}

static void
test_speed_is_not_per_media(struct ctx *ctx)
{
    test_log("speed_is_not_per_media\n");
    vlc_player_t *player = ctx->player;

    struct media_params movie = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(13));

    /* the speed is the viewer's, for every media */
    play(ctx, "movie", &movie);
    vlc_player_ChangeRate(player, 2.f);
    while (vlc_player_GetRate(player) != 2.f)
        vlc_player_CondWait(player, &ctx->wait);
    test_end(ctx);
    assert(vlc_player_GetRate(player) == 1.f);

    play(ctx, "movie", &movie);
    assert(vlc_player_GetRate(player) == 1.f);
    /* nor for the next ones */
    float rate;
    vlc_vector_foreach(rate, &ctx->report.on_rate_changed)
        assert(rate == 1.f);
    test_end(ctx);
}

static void
test_restore_can_be_turned_off(struct ctx *ctx)
{
    test_log("restore_can_be_turned_off\n");
    vlc_player_t *player = ctx->player;
    libvlc_int_t *vlc = ctx->vlc->p_libvlc_int;

    struct media_params movie = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(14));

    play(ctx, "movie", &movie);
    set_delays(ctx, VLC_TICK_FROM_MS(100), VLC_TICK_FROM_MS(100));
    test_end(ctx);

    int ret = var_Create(vlc, "restore-playback-states", VLC_VAR_BOOL);
    assert(ret == VLC_SUCCESS);
    var_SetBool(vlc, "restore-playback-states", false);

    play(ctx, "movie", &movie);
    assert(vlc_player_GetAudioDelay(player) == 0);
    assert(vlc_player_GetSubtitleDelay(player) == 0);
    assert_no_memory(player);
    test_end(ctx);

    /* still kept for later */
    vlc_ml_playback_states_all states = saved_states(ctx, &movie);
    assert(states.has_audio_delay);
    assert(states.audio_delay == 100000);
    vlc_ml_release(&states);

    var_Destroy(vlc, "restore-playback-states");
}

int
main(void)
{
    struct ctx ctx;
    /* "restore-playback-states" is on by default */
    ctx_init(&ctx, MEDIA_LIBRARY);
    test_delays_are_remembered_per_media(&ctx);
    test_music_remembers_too(&ctx);
    test_picked_tracks_are_remembered(&ctx);
    test_defaults_are_not_remembered(&ctx);
    test_speed_is_not_per_media(&ctx);
    test_restore_can_be_turned_off(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
