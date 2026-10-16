/*****************************************************************************
 * Thumbnailer.cpp: medialibrary thumbnailer implementation using libvlccore
 *****************************************************************************
 * Copyright © 2018 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "medialibrary.h"

#include <vlc_fs.h>
#include <vlc_block.h>
#include <vlc_url.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_preparser.h>
#include <vlc_image.h>
#include <vlc_picture.h>

#include <climits>
#include <stdexcept>

Thumbnailer::Thumbnailer( vlc_medialibrary_module_t* ml )
    : m_ml(ml)
    , m_currentContext(nullptr)
    , m_thumbnailer(VLC_OBJECT(ml), {
        .types = VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES,
        .max_parser_threads = 0,
        .max_thumbnailer_threads = 1,
        .timeout = VLC_TICK_FROM_SEC( 3 ),
#if !defined(HAVE_VLC_EXTERNAL_PREPARSER)
        .external_process = false,
#else
        .external_process = true,
#endif
    })
{
}

void Thumbnailer::onThumbnailToFilesComplete(vlc_preparser_req *req, int ,
                                             const bool *result_array,
                                             size_t result_count, void *data)
{
    ThumbnailerCtx* ctx = static_cast<ThumbnailerCtx*>( data );

    vlc::threads::mutex_locker lock( ctx->thumbnailer->m_mutex );
    ctx->done = true;
    if (result_count != 1) {
        ctx->error = true;
    } else {
        ctx->error = !result_array[0];
    }
    ctx->thumbnailer->m_currentContext = nullptr;
    ctx->thumbnailer->m_cond.broadcast();
    vlc_preparser_req_Release( req );
}

/* Many films open dark: titles, a fade in. A thumbnail taken there is all
 * black, so a dark one is taken again further in, a few times at most: the
 * check reads back the small picture just written. */
bool Thumbnailer::generate( const medialibrary::IMedia&, const std::string& mrl,
                            uint32_t desiredWidth, uint32_t desiredHeight,
                            float position, const std::string& dest )
{
    static const float step = .2f;
    static const int attempts = 3;

    bool generated = false;
    for ( int i = 0; i < attempts; ++i )
    {
        const float pos = position + i * step;
        if ( i > 0 && pos > .9f )
            break;
        if ( !generateAt( mrl, desiredWidth, desiredHeight, pos, i > 0, dest ) )
        {
            /* a dark thumbnail is still one */
            msg_Dbg( m_ml, "no thumbnail at %.2f for %s", pos, mrl.c_str() );
            continue;
        }
        generated = true;
        if ( !isDark( dest ) )
            break;
        msg_Dbg( m_ml, "dark thumbnail at %.2f for %s, trying further in",
                 pos, mrl.c_str() );
    }
    return generated;
}

/* Whether the picture of a file is almost black: its average luma is below
 * an eighth of the range */
bool Thumbnailer::isDark( const std::string& dest )
{
    auto url = vlc::wrap_cptr( vlc_path2uri( dest.c_str(), nullptr ), &free );
    if ( url == nullptr )
        return false;

    image_handler_t *handler = image_HandlerCreate( m_ml );
    if ( handler == nullptr )
        return false;

    video_format_t fmt;
    video_format_Init( &fmt, VLC_CODEC_I420 );
    picture_t *pic = image_ReadUrl( handler, url.get(), &fmt );
    image_HandlerDelete( handler );
    if ( pic == nullptr )
    {
        video_format_Clean( &fmt );
        return false;
    }

    bool dark = false;
    if ( pic->format.i_chroma == VLC_CODEC_I420 )
    {
        const plane_t &y = pic->p[0];
        uint64_t sum = 0;
        for ( int line = 0; line < y.i_visible_lines; ++line )
        {
            const uint8_t *p = y.p_pixels + line * y.i_pitch;
            for ( int col = 0; col < y.i_visible_pitch; ++col )
                sum += p[col];
        }
        const uint64_t count = (uint64_t)y.i_visible_lines * y.i_visible_pitch;
        dark = count > 0 && sum / count < 32;
    }
    picture_Release( pic );
    video_format_Clean( &fmt );
    return dark;
}

bool Thumbnailer::generateAt( const std::string& mrl,
                              uint32_t desiredWidth, uint32_t desiredHeight,
                              float position, bool precise,
                              const std::string& dest )
{
#if INT_MAX < UINT32_MAX
    assert(desiredWidth < (uint32_t)INT_MAX);
    assert(desiredHeight < (uint32_t)INT_MAX);
#endif

    ThumbnailerCtx ctx{};

    auto item = vlc::wrap_cptr( input_item_New( mrl.c_str(), nullptr ),
                                &input_item_Release );
    if ( unlikely( item == nullptr ) )
        return false;

    ctx.done = false;
    ctx.thumbnailer = this;
    ctx.error = true;
    {
        vlc::threads::mutex_locker lock( m_mutex );
        m_currentContext = &ctx;
        struct vlc_thumbnailer_arg thumb_arg = {
            .seek = {
                .type = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_POS,
                .pos = position,
                .speed = precise ? vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_PRECISE
                                 : vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_FAST,
            },
            .hw_dec = false,
        };

        struct vlc_thumbnailer_output thumb_out = {
            .format = VLC_THUMBNAILER_FORMAT_JPEG,
            .width = (int)desiredWidth,
            .height = (int)desiredHeight,
            .crop = true,
            .file_path = dest.c_str(),
            .creat_mode = 0600,
        };

        static const struct vlc_thumbnailer_to_files_cbs cbs = {
            .on_ended = onThumbnailToFilesComplete,
        };

        vlc_preparser_t *thumbnailer = m_thumbnailer.instance();
        if (thumbnailer == nullptr) {
            return false;
        }

        vlc_preparser_req *preparserReq =
            vlc_preparser_req_NewThumbnailToFiles(thumbnailer, item.get(),
                                                  &thumb_arg, &thumb_out, 1,
                                                  &cbs, &ctx);
        if (preparserReq == nullptr)
        {
            m_currentContext = nullptr;
            return false;
        }

        if (vlc_preparser_Submit(thumbnailer, preparserReq) != VLC_SUCCESS)
        {
            vlc_preparser_req_Release(preparserReq);
            m_currentContext = nullptr;
            return false;
        }

        while ( ctx.done == false )
            m_cond.wait( m_mutex );
        m_currentContext = nullptr;
    }

    return !ctx.error;
}

void Thumbnailer::stop()
{
    vlc_preparser_t *thumbnailer = m_thumbnailer.get();
    if (thumbnailer == nullptr) {
        return;
    }

    /* vlc_preparser_Cancel can call the callback from this thread so the mutex
     * must be unlock */
    vlc_preparser_Cancel(thumbnailer, nullptr);

    vlc::threads::mutex_locker lock(m_mutex);
    if ( m_currentContext != nullptr )
    {
        while (m_currentContext != nullptr && m_currentContext->done == false)
            m_cond.wait( m_mutex );
    }
}
