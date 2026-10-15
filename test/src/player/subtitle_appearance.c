// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * subtitle_appearance.c: live subtitle appearance test
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

/* The text renderer options are player variables, inherited by the text
 * renderer of every vout. vlc_player_vout_RedrawSubtitles() renders the
 * subtitles again, so that a change shows on the line already on screen. */

static void
wait_rendered(struct text_render_report *report)
{
    vlc_mutex_lock(&report->lock);
    while (report->count == 0)
        vlc_cond_wait(&report->wait, &report->lock);
    vlc_mutex_unlock(&report->lock);
}

static void
wait_rerendered_with_font(struct text_render_report *report, const char *font)
{
    vlc_mutex_lock(&report->lock);
    while (report->rerendered_font == NULL
        || strcmp(report->rerendered_font, font) != 0)
        vlc_cond_wait(&report->wait, &report->lock);
    vlc_mutex_unlock(&report->lock);
}

static void
test_redraw_paused_subtitle(struct ctx *ctx)
{
    test_log("redraw_paused_subtitle\n");
    vlc_player_t *player = ctx->player;

    struct text_render_report report = {
        .count = 0, .text_count = 0, .rerendered_font = NULL,
    };
    vlc_mutex_init(&report.lock);
    vlc_cond_init(&report.wait);

    /* the vouts inherit these from libvlc, through the player */
    libvlc_int_t *vlc = ctx->vlc->p_libvlc_int;
    int ret = var_Create(vlc, "test-text-report", VLC_VAR_ADDRESS);
    assert(ret == VLC_SUCCESS);
    var_SetAddress(vlc, "test-text-report", &report);
    ret = var_Create(vlc, "text-renderer", VLC_VAR_STRING);
    assert(ret == VLC_SUCCESS);
    var_SetString(vlc, "text-renderer", "test_src_player_text");

    /* the renderer options are player variables */
    ret = vlc_player_SetSubtitleTextOption(player, "freetype-font",
                                           (vlc_value_t) { .psz_string = (char *) "Before" });
    assert(ret == VLC_SUCCESS);
    vlc_value_t font;
    ret = vlc_player_GetSubtitleTextOption(player, "freetype-font", &font);
    assert(ret == VLC_SUCCESS);
    assert(strcmp(font.psz_string, "Before") == 0);
    free(font.psz_string);

    /* one subtitle line per video frame, each shown until the next one */
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(100));
    params.track_count[AUDIO_ES] = 0;
    player_set_current_mock_media(ctx, "media1", &params, false);
    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    /* subtitles are not shown by default */
    while (vlc_player_GetSubtitleTrackCount(player) == 0)
        vlc_player_CondWait(player, &ctx->wait);
    vlc_player_SelectTrack(player, vlc_player_GetSubtitleTrackAt(player, 0),
                           VLC_PLAYER_SELECT_EXCLUSIVE);

    vlc_player_Unlock(player);
    wait_rendered(&report);
    vlc_player_Lock(player);

    /* a few lines later, one is on screen */
    {
        vec_on_position_changed *vec = &ctx->report.on_position_changed;
        while (vec->size == 0 || VEC_LAST(vec).time < VLC_TICK_FROM_MS(1000))
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* the line on screen stays the same while paused */
    vlc_player_Pause(player);
    wait_state(ctx, VLC_PLAYER_STATE_PAUSED);

    ret = vlc_player_SetSubtitleTextOption(player, "freetype-font",
                                           (vlc_value_t) { .psz_string = (char *) "After" });
    assert(ret == VLC_SUCCESS);

    /* a line already rendered is rendered again, with the new font */
    vlc_player_Unlock(player);
    vlc_player_vout_RedrawSubtitles(player);
    wait_rerendered_with_font(&report, "After");
    vlc_player_Lock(player);

    test_prestop(ctx);
    test_end(ctx);

    var_SetAddress(vlc, "test-text-report", NULL);
    var_Destroy(vlc, "test-text-report");
    var_Destroy(vlc, "text-renderer");

    for (size_t i = 0; i < report.text_count; ++i)
        free(report.texts[i]);
    free(report.rerendered_font);
}

static void
test_redraw_without_vout(struct ctx *ctx)
{
    test_log("redraw_without_vout\n");

    /* nothing to redraw: no crash, no dummy vout use */
    vlc_player_vout_RedrawSubtitles(ctx->player);
}

static void
test_text_options(struct ctx *ctx)
{
    test_log("text_options\n");
    vlc_player_t *player = ctx->player;

    /* they start with the configuration values */
    vlc_value_t value;
    int ret = vlc_player_GetSubtitleTextOption(player, "freetype-keep-styling", &value);
    assert(ret == VLC_SUCCESS);
    assert(value.b_bool);

    ret = vlc_player_SetSubtitleTextOption(player, "freetype-color",
                                           (vlc_value_t) { .i_int = 0xFFCC00 });
    assert(ret == VLC_SUCCESS);
    ret = vlc_player_GetSubtitleTextOption(player, "freetype-color", &value);
    assert(ret == VLC_SUCCESS);
    assert(value.i_int == 0xFFCC00);

    /* only the text renderer options */
    ret = vlc_player_SetSubtitleTextOption(player, "rate",
                                           (vlc_value_t) { .f_float = 2.f });
    assert(ret == VLC_ENOENT);
    ret = vlc_player_SetSubtitleTextOption(player, "freetype-unknown",
                                           (vlc_value_t) { .i_int = 1 });
    assert(ret == VLC_ENOENT);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_redraw_without_vout(&ctx);
    test_text_options(&ctx);
    test_redraw_paused_subtitle(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
