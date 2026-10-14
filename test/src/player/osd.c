// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * osd.c: player OSD message test
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

/* The interfaces get the OSD messages from vlc_player_osd_AddListener(), so
 * that they can show them even without a video output. */

struct osd_report
{
    vlc_mutex_t lock;
    char *last;
    size_t count;
};

static void
on_osd_message(vlc_player_t *player, const char *text, void *data)
{
    (void) player;
    struct osd_report *report = data;

    vlc_mutex_lock(&report->lock);
    free(report->last);
    report->last = strdup(text);
    assert(report->last != NULL);
    report->count++;
    vlc_mutex_unlock(&report->lock);
}

static void
assert_last_message(struct osd_report *report, const char *expected)
{
    vlc_mutex_lock(&report->lock);
    assert(report->count > 0);
    assert(report->last != NULL);
    if (strcmp(report->last, expected) != 0)
    {
        test_log("osd message: \"%s\", expected \"%s\"\n", report->last,
                 expected);
        assert(!"unexpected osd message");
    }
    vlc_mutex_unlock(&report->lock);
}

static void
test_osd_messages(struct ctx *ctx)
{
    test_log("osd_messages\n");
    vlc_player_t *player = ctx->player;

    struct osd_report report = { .last = NULL, .count = 0 };
    vlc_mutex_init(&report.lock);
    static const struct vlc_player_osd_cbs cbs = {
        .on_message = on_osd_message,
    };
    vlc_player_osd_listener_id *listener =
        vlc_player_osd_AddListener(player, &cbs, &report);
    assert(listener != NULL);

    /* no video: there is no video output to draw the messages */
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    params.track_count[VIDEO_ES] = 0;
    params.track_count[SPU_ES] = 1;
    player_set_current_mock_media(ctx, "media1", &params, false);
    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    /* the result of a delay change */
    vlc_player_SetCategoryDelay(player, SPU_ES, VLC_TICK_FROM_MS(50),
                                VLC_PLAYER_WHENCE_RELATIVE);
    assert_last_message(&report, "Subtitle delay: 50 ms");

    vlc_player_SetCategoryDelay(player, SPU_ES, VLC_TICK_FROM_MS(50),
                                VLC_PLAYER_WHENCE_RELATIVE);
    assert_last_message(&report, "Subtitle delay: 100 ms");

    /* the playback state */
    vlc_player_Pause(player);
    assert_last_message(&report, "Pause");
    wait_state(ctx, VLC_PLAYER_STATE_PAUSED);

    vlc_player_Resume(player);
    assert_last_message(&report, "Play");
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    /* the position after a seek, once the length is known */
    {
        vec_on_length_changed *vec = &ctx->report.on_length_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
    }
    vlc_player_SetTime(player, VLC_TICK_FROM_SEC(5));
    assert_last_message(&report, "00:05 / 00:10");

    test_prestop(ctx);
    test_end(ctx);

    vlc_player_osd_RemoveListener(player, listener);
    free(report.last);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_osd_messages(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
