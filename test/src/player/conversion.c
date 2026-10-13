// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * conversion.c: background conversion player test
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

#include <sys/stat.h>
#include <unistd.h>

/* The Qt interface converts media on a second, hidden player, created on a
 * private child object, while the main player keeps playing. */

struct conversion
{
    vlc_cond_t wait;
    bool stopped;
    bool error;
    size_t position_count;
};

static void
conversion_on_state_changed(vlc_player_t *player, enum vlc_player_state state,
                            void *data)
{
    (void) player;
    struct conversion *conv = data;
    if (state == VLC_PLAYER_STATE_STOPPED)
    {
        conv->stopped = true;
        vlc_cond_signal(&conv->wait);
    }
}

static void
conversion_on_error_changed(vlc_player_t *player, enum vlc_player_error error,
                            void *data)
{
    (void) player;
    struct conversion *conv = data;
    if (error != VLC_PLAYER_ERROR_NONE)
        conv->error = true;
}

static void
conversion_on_position_changed(vlc_player_t *player, vlc_tick_t time,
                               double pos, void *data)
{
    (void) player; (void) time; (void) pos;
    struct conversion *conv = data;
    conv->position_count++;
}

static void
convert(libvlc_int_t *libvlc, const char *mrl, const char *dst,
        struct conversion *conv)
{
    vlc_object_t *parent = vlc_object_create(libvlc, sizeof(*parent));
    assert(parent);

    vlc_player_t *converter = vlc_player_New(parent, VLC_PLAYER_LOCK_NORMAL);
    assert(converter);

    static const struct vlc_player_cbs cbs = {
        .on_state_changed = conversion_on_state_changed,
        .on_error_changed = conversion_on_error_changed,
        .on_position_changed = conversion_on_position_changed,
    };

    vlc_cond_init(&conv->wait);
    conv->stopped = conv->error = false;
    conv->position_count = 0;

    input_item_t *item = input_item_New(mrl, "conversion");
    assert(item);

    char *sout;
    int ret = asprintf(&sout, "sout=#transcode{vcodec=none,acodec=s16l,"
                       "scodec=none}:std{access=file,mux=wav,dst='%s'}", dst);
    assert(ret != -1);
    ret = input_item_AddOption(item, sout, VLC_INPUT_OPTION_TRUSTED);
    assert(ret == VLC_SUCCESS);
    free(sout);

    vlc_player_Lock(converter);
    vlc_player_listener_id *listener =
        vlc_player_AddListener(converter, &cbs, conv);
    assert(listener);

    ret = vlc_player_SetCurrentMedia(converter, item);
    assert(ret == VLC_SUCCESS);
    input_item_Release(item);
    ret = vlc_player_Start(converter);
    assert(ret == VLC_SUCCESS);

    while (!conv->stopped)
        vlc_player_CondWait(converter, &conv->wait);

    vlc_player_RemoveListener(converter, listener);
    vlc_player_Unlock(converter);
    vlc_player_Delete(converter);
    vlc_object_delete(parent);
}

static void
test_conversion_beside_playback(struct ctx *ctx)
{
    test_log("conversion beside playback\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    player_set_current_mock_media(ctx, "media1", &params, false);
    input_item_t *media = vlc_player_GetCurrentMedia(player);
    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    const size_t state_count = ctx->report.on_state_changed.size;

    char dst[] = "/tmp/vlc-test-conversion-XXXXXX";
    int fd = mkstemp(dst);
    assert(fd != -1);
    close(fd);

    /* Let the main player run while converting */
    vlc_player_Unlock(player);
    struct conversion conv;
    convert(ctx->vlc->p_libvlc_int,
            /* long enough to last more than a stats period (250 ms) */
            "mock://video_track_count=0;audio_track_count=1;length=1800000000;"
            "audio_format=u8;audio_channels=1;audio_rate=8000;audio_sinewave=0",
            dst, &conv);
    vlc_player_Lock(player);

    /* The conversion wrote a whole file */
    assert(!conv.error);
    assert(conv.position_count > 0);
    struct stat st;
    int ret = stat(dst, &st);
    assert(ret == 0);
    /* 30 min of 16-bit mono 8 kHz (less the last block) */
    assert(st.st_size > 1799 * 8000 * 2);
    unlink(dst);

    /* Playback is untouched: same media, still playing, and moving on */
    assert(vlc_player_GetCurrentMedia(player) == media);
    assert(vlc_player_GetState(player) == VLC_PLAYER_STATE_PLAYING);
    assert(ctx->report.on_state_changed.size == state_count);

    vec_on_position_changed *vec = &ctx->report.on_position_changed;
    const size_t position_count = vec->size;
    while (vec->size == position_count)
        vlc_player_CondWait(player, &ctx->wait);

    test_prestop(ctx);
    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_conversion_beside_playback(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
