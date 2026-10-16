// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * start_time.c: start-time player test
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"
#include "timers.h"

/* A media played from a start time (":start-time") is a part of the media:
 * the timer counts from the start time and the length is what is left. It
 * does so from its very first point, which the outputs can report before the
 * input has given the start time: the time display must not show the media's
 * own time first, then jump. */
static void
test_start_time(struct ctx *ctx, bool paused)
{
    test_log("start_time (paused: %d)\n", paused);
    vlc_player_t *player = ctx->player;

    const vlc_tick_t start = VLC_TICK_FROM_SEC(8);
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    input_item_t *media = player_create_mock_media(ctx, "media1", &params);
    input_item_AddOption(media, ":start-time=8", VLC_INPUT_OPTION_TRUSTED);
    int ret = vlc_player_SetCurrentMedia(player, media);
    assert(ret == VLC_SUCCESS);
    bool success = vlc_vector_push(&ctx->added_medias, media);
    assert(success);
    success = vlc_vector_push(&ctx->played_medias, media);
    assert(success);
    /* the length the player reports */
    ctx->params.length = params.length - start;

    struct timer_state timer;
    player_add_timer(player, &timer, false, VLC_TICK_INVALID);

    if (paused)
        vlc_player_SetStartPaused(player, true);
    player_start(ctx);

    {
        vec_on_length_changed *vec = &ctx->report.on_length_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
        assert(VEC_LAST(vec) == ctx->params.length);
    }
    /* what the media times (chapters) are counted against */
    assert(vlc_player_GetStartTime(player) == start);

    /* every point the timer gave until now is within the part played */
    size_t points = 0;
    player_lock_timer(player, &timer);
    while (points == 0)
    {
        while (timer.last_report_idx < timer.vec.size)
        {
            struct report_timer *report = timer_state_wait_next_report(&timer);
            if (report->type != REPORT_TIMER_POINT)
                continue;
            points++;
            assert(report->point.ts >= VLC_TICK_0);
            assert(report->point.ts <= VLC_TICK_0 + ctx->params.length);
            assert(report->point.position >= 0 && report->point.position <= 1);
        }
        if (points == 0)
            vlc_cond_wait(&timer.wait, &timer.lock);
    }
    player_unlock_timer(player, &timer);

    player_remove_timer(player, &timer);

    if (!paused)
    {
        vlc_player_SetPosition(player, 1.0f);
        test_prestop(ctx);
        wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    }

    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_start_time(&ctx, false);
    test_start_time(&ctx, true);
    ctx_destroy(&ctx);
    return 0;
}
