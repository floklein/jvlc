// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * stream_output.c: player stream output test
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

#include <vlc_renderer_discovery.h>

#include <sys/stat.h>
#include <unistd.h>

/* Stream & Cast streams the current item or the Queue: the player opens its
 * next medias with the stream output it is given, until it is unset. */

/* 10 s of audio, read as fast as possible when it is streamed */
#define AUDIO_MEDIA "mock://video_track_count=0;audio_track_count=1;" \
    "length=10000000;audio_format=u8;audio_channels=1;audio_rate=8000;" \
    "audio_sinewave=0"

/* played here, at its own pace */
#define SHORT_AUDIO_MEDIA "mock://video_track_count=0;audio_track_count=1;" \
    "length=200000;audio_format=u8;audio_channels=1;audio_rate=8000;" \
    "audio_sinewave=0"

static void
play_to_end(struct ctx *ctx, const char *mrl, const char *name)
{
    vlc_player_t *player = ctx->player;

    input_item_t *media = input_item_New(mrl, name);
    assert(media);
    int ret = vlc_player_SetCurrentMedia(player, media);
    assert(ret == VLC_SUCCESS);
    input_item_Release(media);

    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    assert(!vlc_player_IsStarted(player));
    vec_on_error_changed *errors = &ctx->report.on_error_changed;
    for (size_t i = 0; i < errors->size; ++i)
        assert(errors->data[i] == VLC_PLAYER_ERROR_NONE);
    ctx_reset(ctx);
}

static void
test_stream_output(struct ctx *ctx)
{
    test_log("stream output\n");
    vlc_player_t *player = ctx->player;

    char dst[] = "/tmp/vlc-test-stream-output-XXXXXX";
    int fd = mkstemp(dst);
    assert(fd != -1);
    close(fd);
    unlink(dst);

    /* No stream output by default */
    assert(vlc_player_GetStreamOutput(player) == NULL);

    char *sout;
    int ret = asprintf(&sout, "#transcode{vcodec=none,acodec=s16l,scodec=none}"
                       ":std{access=file,mux=wav,dst='%s'}", dst);
    assert(ret != -1);
    ret = vlc_player_SetStreamOutput(player, sout);
    assert(ret == VLC_SUCCESS);

    char *current = vlc_player_GetStreamOutput(player);
    assert(current && strcmp(current, sout) == 0);
    free(current);
    free(sout);

    /* The media goes to the stream output */
    play_to_end(ctx, AUDIO_MEDIA, "streamed");
    struct stat st;
    ret = stat(dst, &st);
    assert(ret == 0);
    /* 10 s of 16-bit mono 8 kHz (less the last block) */
    assert(st.st_size > 9 * 8000 * 2);
    unlink(dst);

    /* Unset, the next media plays here again */
    ret = vlc_player_SetStreamOutput(player, NULL);
    assert(ret == VLC_SUCCESS);
    assert(vlc_player_GetStreamOutput(player) == NULL);

    play_to_end(ctx, SHORT_AUDIO_MEDIA, "played");
    ret = stat(dst, &st);
    assert(ret != 0);
}

/* Casting restarts the decoders of the selected tracks: a media without any
 * track must survive it */
static void
test_cast_without_tracks(struct ctx *ctx)
{
    test_log("cast without tracks\n");
    vlc_player_t *player = ctx->player;

    input_item_t *media = input_item_New("mock://video_track_count=0;"
                                         "audio_track_count=0;length=200000",
                                         "no tracks");
    assert(media);
    int ret = vlc_player_SetCurrentMedia(player, media);
    assert(ret == VLC_SUCCESS);
    input_item_Release(media);

    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    vlc_renderer_item_t *device =
        vlc_renderer_item_new("type", "device", "dummy://127.0.0.1:1",
                              NULL, NULL, NULL, 0);
    assert(device);
    vlc_player_SetRenderer(player, device);
    vlc_player_SetRenderer(player, NULL);
    vlc_renderer_item_release(device);

    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    ctx_reset(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_stream_output(&ctx);
    test_cast_without_tracks(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
