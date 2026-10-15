// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * modules.c: modules used for the player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

#include <vlc_subpicture.h>
#include <vlc_text_style.h>
#include <vlc_media_library.h>

#define MODULE_NAME test_src_player
#undef VLC_DYNAMIC_PLUGIN
#include <vlc_plugin.h>
/* Define a builtin module for mocked parts */
const char vlc_module_name[] = MODULE_STRING;

struct aout_sys
{
    vlc_tick_t first_pts;
    vlc_tick_t first_play_date;
    vlc_tick_t pos;

    struct ctx *ctx;
};

static void aout_Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    struct aout_sys *sys = aout->sys;

    if (sys->first_play_date == VLC_TICK_INVALID)
    {
        assert(sys->first_pts == VLC_TICK_INVALID);
        sys->first_play_date = date;
        sys->first_pts = block->i_pts;

        struct ctx *ctx = sys->ctx;
        vlc_player_Lock(ctx->player);
        VEC_PUSH(on_aout_first_pts, sys->first_pts);
        vlc_player_Unlock(ctx->player);
    }

    aout_TimingReport(aout, sys->first_play_date + sys->pos - VLC_TICK_0,
                      sys->first_pts + sys->pos);
    sys->pos += block->i_length;
    block_Release(block);
}

static void aout_Flush(audio_output_t *aout)
{
    struct aout_sys *sys = aout->sys;
    sys->pos = 0;
    sys->first_pts = sys->first_play_date = VLC_TICK_INVALID;
}

static void aout_Stop(audio_output_t *aout)
{
    struct aout_sys *sys = aout->sys;

    if (sys->first_play_date != VLC_TICK_INVALID)
    {
        /* Same dates as the timing reports from Play() */
        struct report_aout_stop report = {
            .date = vlc_tick_now(),
            .end_date = sys->first_play_date + sys->pos - VLC_TICK_0,
        };

        struct ctx *ctx = sys->ctx;
        vlc_player_Lock(ctx->player);
        VEC_PUSH(on_aout_stop, report);
        vlc_player_Unlock(ctx->player);
    }

    aout_Flush(aout);
}

static void aout_Pause(audio_output_t *aout, bool pause, vlc_tick_t date)
{
    (void) date;
    if (pause)
        aout_Flush(aout);
}

static void aout_InstantDrain(audio_output_t *aout)
{
    aout_DrainedReport(aout);
}

static int aout_Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    (void) aout;
    return AOUT_FMT_LINEAR(fmt) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
aout_Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    free(aout->sys);
}

static int aout_Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    aout->start = aout_Start;
    aout->play = aout_Play;
    aout->pause = aout_Pause;
    aout->flush = aout_Flush;
    aout->stop = aout_Stop;
    aout->volume_set = NULL;
    aout->mute_set = NULL;

    struct aout_sys *sys = aout->sys = malloc(sizeof(*sys));
    assert(sys != NULL);

    sys->ctx = var_InheritAddress(aout, "test-ctx");
    assert(sys->ctx != NULL);

    if (sys->ctx->flags & AUDIO_INSTANT_DRAIN)
        aout->drain = aout_InstantDrain;

    aout_Flush(aout);

    return VLC_SUCCESS;
}

static block_t *resampler_Resample(filter_t *filter, block_t *in)
{
    VLC_UNUSED(filter);
    return in;
}

static void resampler_Close(filter_t *filter)
{
    VLC_UNUSED(filter);
}

static int resampler_Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    static const struct vlc_filter_operations filter_ops =
    { .filter_audio = resampler_Resample, .close = resampler_Close, };
    filter->ops = &filter_ops;
    return VLC_SUCCESS;
}

static subpicture_region_t *text_Render(filter_t *filter,
                                        const subpicture_region_t *region_in,
                                        const vlc_fourcc_t *chroma_list)
{
    (void) chroma_list;
    struct text_render_report *report =
        var_InheritAddress(filter, "test-text-report");
    if (report == NULL || region_in->p_text == NULL
     || region_in->p_text->psz_text == NULL)
        return NULL;

    /* read on every render, like the freetype renderer */
    char *font = var_InheritString(filter, "freetype-font");
    const char *text = region_in->p_text->psz_text;

    vlc_mutex_lock(&report->lock);
    report->count++;
    bool seen = false;
    for (size_t i = 0; i < report->text_count && !seen; ++i)
        seen = strcmp(report->texts[i], text) == 0;
    if (seen)
    {
        free(report->rerendered_font);
        report->rerendered_font = font;
        font = NULL;
    }
    else if (report->text_count < ARRAY_SIZE(report->texts))
    {
        report->texts[report->text_count] = strdup(text);
        assert(report->texts[report->text_count] != NULL);
        report->text_count++;
    }
    vlc_cond_broadcast(&report->wait);
    vlc_mutex_unlock(&report->lock);

    free(font);
    /* nothing to draw */
    return NULL;
}

static int text_Open(filter_t *filter)
{
    static const struct vlc_filter_operations filter_ops =
        { .render = text_Render, };
    filter->ops = &filter_ops;
    return VLC_SUCCESS;
}

/* An in-memory media library: what the player keeps of each media, by MRL */
struct ml_media
{
    char *mrl;
    vlc_ml_media_type_t type;
    double progress;
    vlc_ml_playback_states_all states;
};

struct ml_sys
{
    vlc_mutex_t lock;
    struct VLC_VECTOR(struct ml_media) medias;
};

static char *ml_strdup(const char *str)
{
    if (str == NULL)
        return NULL;
    char *dup = strdup(str);
    assert(dup != NULL);
    return dup;
}

static void ml_set_string(char **dst, const char *src)
{
    if (src == NULL)
        return;
    free(*dst);
    *dst = ml_strdup(src);
}

static struct ml_media *ml_find(struct ml_sys *sys, int64_t id)
{
    if (id <= 0 || (size_t) id > sys->medias.size)
        return NULL;
    return &sys->medias.data[id - 1];
}

static int64_t ml_find_mrl(struct ml_sys *sys, const char *mrl)
{
    for (size_t i = 0; i < sys->medias.size; ++i)
        if (strcmp(sys->medias.data[i].mrl, mrl) == 0)
            return i + 1;
    return 0;
}

static vlc_ml_media_t *ml_new_media(struct ml_sys *sys, int64_t id)
{
    vlc_ml_media_t *media = calloc(1, sizeof(*media));
    assert(media != NULL);
    media->i_id = id;
    media->i_type = ml_find(sys, id)->type;
    media->f_progress = ml_find(sys, id)->progress;
    return media;
}

/* Like the media library module: only the states that are set are written,
 * the others keep their value */
static void ml_set_states(vlc_ml_playback_states_all *dst,
                          const vlc_ml_playback_states_all *src)
{
    if (src->rate != .0f)
        dst->rate = src->rate;
    if (src->zoom != .0f)
        dst->zoom = src->zoom;
    if (src->current_title >= 0)
        dst->current_title = src->current_title;
    ml_set_string(&dst->current_video_track, src->current_video_track);
    ml_set_string(&dst->current_audio_track, src->current_audio_track);
    ml_set_string(&dst->current_subtitle_track, src->current_subtitle_track);
    ml_set_string(&dst->aspect_ratio, src->aspect_ratio);
    ml_set_string(&dst->crop, src->crop);
    ml_set_string(&dst->deinterlace, src->deinterlace);
    ml_set_string(&dst->video_filter, src->video_filter);
    if (src->has_audio_delay)
    {
        dst->has_audio_delay = true;
        dst->audio_delay = src->audio_delay;
    }
    if (src->has_subtitle_delay)
    {
        dst->has_subtitle_delay = true;
        dst->subtitle_delay = src->subtitle_delay;
    }
}

static int ml_Control(vlc_medialibrary_module_t *ml, int query, va_list args)
{
    struct ml_sys *sys = ml->p_sys;
    int ret = VLC_SUCCESS;

    vlc_mutex_lock(&sys->lock);
    switch (query)
    {
        case VLC_ML_NEW_EXTERNAL_MEDIA:
        {
            const char *mrl = va_arg(args, const char *);
            vlc_ml_media_t **media = va_arg(args, vlc_ml_media_t **);
            struct ml_media entry = {
                .mrl = ml_strdup(mrl),
                .type = VLC_ML_MEDIA_TYPE_UNKNOWN,
                .states = { .rate = .0f, .zoom = -1.f, .current_title = -1 },
            };
            bool success = vlc_vector_push(&sys->medias, entry);
            assert(success);
            *media = ml_new_media(sys, sys->medias.size);
            break;
        }
        case VLC_ML_MEDIA_SET_TYPE:
        {
            struct ml_media *entry = ml_find(sys, va_arg(args, int64_t));
            if (entry == NULL)
                ret = VLC_EGENERIC;
            else
                entry->type = va_arg(args, int);
            break;
        }
        case VLC_ML_MEDIA_UPDATE_PROGRESS:
        {
            struct ml_media *entry = ml_find(sys, va_arg(args, int64_t));
            if (entry == NULL)
                ret = VLC_EGENERIC;
            else
                entry->progress = va_arg(args, double);
            break;
        }
        case VLC_ML_MEDIA_GET_ALL_MEDIA_PLAYBACK_STATES:
        {
            struct ml_media *entry = ml_find(sys, va_arg(args, int64_t));
            vlc_ml_playback_states_all *states =
                va_arg(args, vlc_ml_playback_states_all *);
            if (entry == NULL)
            {
                ret = VLC_EGENERIC;
                break;
            }
            *states = entry->states;
            states->current_video_track = ml_strdup(entry->states.current_video_track);
            states->current_audio_track = ml_strdup(entry->states.current_audio_track);
            states->current_subtitle_track = ml_strdup(entry->states.current_subtitle_track);
            states->aspect_ratio = ml_strdup(entry->states.aspect_ratio);
            states->crop = ml_strdup(entry->states.crop);
            states->deinterlace = ml_strdup(entry->states.deinterlace);
            states->video_filter = ml_strdup(entry->states.video_filter);
            break;
        }
        case VLC_ML_MEDIA_SET_ALL_MEDIA_PLAYBACK_STATES:
        {
            struct ml_media *entry = ml_find(sys, va_arg(args, int64_t));
            if (entry == NULL)
                ret = VLC_EGENERIC;
            else
                ml_set_states(&entry->states,
                              va_arg(args, const vlc_ml_playback_states_all *));
            break;
        }
        default:
            ret = VLC_EGENERIC;
            break;
    }
    vlc_mutex_unlock(&sys->lock);
    return ret;
}

static int ml_List(vlc_medialibrary_module_t *ml, int query,
                   const vlc_ml_query_params_t *params, va_list args)
{
    (void) ml; (void) query; (void) params; (void) args;
    return VLC_EGENERIC;
}

static void *ml_Get(vlc_medialibrary_module_t *ml, int query, va_list args)
{
    struct ml_sys *sys = ml->p_sys;
    vlc_ml_media_t *media = NULL;

    vlc_mutex_lock(&sys->lock);
    if (query == VLC_ML_GET_MEDIA_BY_MRL)
    {
        int64_t id = ml_find_mrl(sys, va_arg(args, const char *));
        if (id != 0)
            media = ml_new_media(sys, id);
    }
    else if (query == VLC_ML_GET_MEDIA)
    {
        int64_t id = va_arg(args, int64_t);
        if (ml_find(sys, id) != NULL)
            media = ml_new_media(sys, id);
    }
    vlc_mutex_unlock(&sys->lock);
    return media;
}

static int ml_Open(vlc_object_t *obj)
{
    vlc_medialibrary_module_t *ml = (vlc_medialibrary_module_t *) obj;
    struct ml_sys *sys = malloc(sizeof(*sys));
    if (sys == NULL)
        return VLC_ENOMEM;
    vlc_mutex_init(&sys->lock);
    vlc_vector_init(&sys->medias);
    ml->p_sys = sys;
    ml->pf_control = ml_Control;
    ml->pf_list = ml_List;
    ml->pf_get = ml_Get;
    return VLC_SUCCESS;
}

static void ml_Close(vlc_object_t *obj)
{
    vlc_medialibrary_module_t *ml = (vlc_medialibrary_module_t *) obj;
    struct ml_sys *sys = ml->p_sys;
    struct ml_media *entry;
    vlc_vector_foreach_ref(entry, &sys->medias)
    {
        free(entry->mrl);
        vlc_ml_playback_states_all_release(&entry->states);
    }
    vlc_vector_destroy(&sys->medias);
    free(sys);
}

vlc_module_begin()
    /* This aout module will report audio timings perfectly, but without any
     * delay, in order to be usable for player tests. Indeed, this aout will
     * report timings immediately from Play(), but points will be in the
     * future (like when aout->time_get() is used). */
    set_capability("audio output", 0)
    set_callbacks(aout_Open, aout_Close)
    add_submodule ()
    /* aout will insert a resampler that can have samples delay, even for 1:1
     * Insert our own resampler that keeps blocks and pts untouched. */
    set_capability ("audio resampler", 9999)
    set_callback (resampler_Open)
    add_submodule ()
    /* A text renderer reporting what it renders, selected by name only */
    add_shortcut ("test_src_player_text")
    set_callback_text_renderer (text_Open, 0)
    add_submodule ()
    /* The media library of the tests run with --media-library */
    set_capability ("medialibrary", 10000)
    set_callbacks (ml_Open, ml_Close)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};
