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
#include <vlc_fourcc.h>

#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <stdexcept>

Thumbnailer::Thumbnailer( vlc_medialibrary_module_t* ml )
    : m_ml(ml)
    , m_currentContext(nullptr)
    , m_thumbnailer(VLC_OBJECT(ml), {
        .types = VLC_PREPARSER_TYPE_THUMBNAIL,
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

void Thumbnailer::onThumbnailComplete(vlc_preparser_req *req, int status,
                                      picture_t *thumbnail, void *data)
{
    ThumbnailerCtx* ctx = static_cast<ThumbnailerCtx*>( data );

    vlc::threads::mutex_locker lock( ctx->thumbnailer->m_mutex );
    ctx->done = true;
    if ( status == VLC_SUCCESS && thumbnail != nullptr )
        ctx->picture = picture_Hold( thumbnail );
    ctx->thumbnailer->m_currentContext = nullptr;
    ctx->thumbnailer->m_cond.broadcast();
    vlc_preparser_req_Release( req );
}

/* Many films open dark: titles, a fade in. A thumbnail taken there is all
 * black, so a dark one is taken again further in, a few times at most. The
 * check reads the decoded picture, and only the last one is written. */
bool Thumbnailer::generate( const medialibrary::IMedia&, const std::string& mrl,
                            uint32_t desiredWidth, uint32_t desiredHeight,
                            float position, const std::string& dest )
{
    static const float step = .2f;
    static const int attempts = 3;

    picture_t *thumbnail = nullptr;
    for ( int i = 0; i < attempts; ++i )
    {
        const float pos = position + i * step;
        if ( i > 0 && pos > .9f )
            break;
        picture_t *pic = takeAt( mrl, pos, i > 0 );
        if ( pic == nullptr )
        {
            /* a dark thumbnail is still one */
            msg_Dbg( m_ml, "no thumbnail at %.2f for %s", pos, mrl.c_str() );
            continue;
        }
        if ( thumbnail != nullptr )
            picture_Release( thumbnail );
        thumbnail = pic;
        if ( !isDark( pic ) )
            break;
        msg_Dbg( m_ml, "dark thumbnail at %.2f for %s, trying further in",
                 pos, mrl.c_str() );
    }
    if ( thumbnail == nullptr )
        return false;

    bool written = write( thumbnail, desiredWidth, desiredHeight, dest );
    picture_Release( thumbnail );
    return written;
}

/* The average luma of the luma plane of an 8-bit YUV picture (planar or
 * semi-planar), from 0 to 255, or -1 for any other picture */
static int meanLuma( const picture_t *pic )
{
    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription( pic->format.i_chroma );
    if ( desc == nullptr || !vlc_chroma_description_IsYUV( desc )
      || desc->plane_count < 2 || desc->pixel_size != 1 )
        return -1;

    const plane_t &y = pic->p[0];
    uint64_t sum = 0;
    for ( int line = 0; line < y.i_visible_lines; ++line )
    {
        const uint8_t *p = y.p_pixels + line * y.i_pitch;
        for ( int col = 0; col < y.i_visible_pitch; ++col )
            sum += p[col];
    }
    const uint64_t count = (uint64_t)y.i_visible_lines * y.i_visible_pitch;
    return count > 0 ? sum / count : -1;
}

/* Whether a picture is almost black: its average luma is below an eighth of
 * the range */
bool Thumbnailer::isDark( picture_t *pic )
{
    video_format_t fmt = pic->format;
    picture_t *converted = nullptr;
    int luma = meanLuma( pic );
    if ( luma < 0 )
    {
        /* any other chroma: a small 8-bit YUV copy tells */
        image_handler_t *handler = image_HandlerCreate( m_ml );
        if ( handler == nullptr )
            return false;
        video_format_Init( &fmt, VLC_CODEC_I420 );
        fmt.i_width = fmt.i_visible_width = 64;
        fmt.i_height = fmt.i_visible_height = 64;
        converted = image_Convert( handler, pic, &pic->format, &fmt );
        image_HandlerDelete( handler );
        if ( converted == nullptr )
            return false;
        fmt = converted->format;
        luma = meanLuma( converted );
        picture_Release( converted );
        if ( luma < 0 )
            return false;
    }

    /* black is 16 in the limited range, 0 in the full one */
    const bool full = fmt.color_range == COLOR_RANGE_FULL;
    const int black = full ? 0 : 16;
    const int range = full ? 255 : 219;
    return luma < black + 32 * range / 255;
}

/* Writes the thumbnail as a JPEG file, as the preparser does for the
 * thumbnails to files */
bool Thumbnailer::write( picture_t *pic, uint32_t width, uint32_t height,
                         const std::string& dest )
{
#if INT_MAX < UINT32_MAX
    assert(width < (uint32_t)INT_MAX);
    assert(height < (uint32_t)INT_MAX);
#endif

    block_t *block;
    if ( picture_Export( VLC_OBJECT( m_ml ), &block, nullptr, pic,
                         VLC_CODEC_JPEG, (int)width, (int)height, true ) != VLC_SUCCESS )
        return false;

    int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_BINARY
    flags |= O_BINARY;
#endif
    int fd = vlc_open( dest.c_str(), flags, 0600 );
    if ( fd == -1 )
    {
        block_Release( block );
        return false;
    }

    const uint8_t *data = block->p_buffer;
    size_t size = block->i_buffer;
    while ( size > 0 )
    {
        ssize_t len = vlc_write( fd, data, size );
        if ( len < 0 )
        {
            if ( errno == EINTR || errno == EAGAIN )
                continue;
            break;
        }
        size -= len;
        data += len;
    }
    vlc_close( fd );
    block_Release( block );
    if ( size > 0 )
    {
        vlc_unlink( dest.c_str() );
        return false;
    }
    return true;
}

picture_t* Thumbnailer::takeAt( const std::string& mrl, float position,
                                bool precise )
{
    ThumbnailerCtx ctx{};

    auto item = vlc::wrap_cptr( input_item_New( mrl.c_str(), nullptr ),
                                &input_item_Release );
    if ( unlikely( item == nullptr ) )
        return nullptr;

    ctx.done = false;
    ctx.thumbnailer = this;
    ctx.picture = nullptr;
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

        static const struct vlc_thumbnailer_cbs cbs = {
            .on_ended = onThumbnailComplete,
        };

        vlc_preparser_t *thumbnailer = m_thumbnailer.instance();
        if (thumbnailer == nullptr) {
            m_currentContext = nullptr;
            return nullptr;
        }

        vlc_preparser_req *preparserReq =
            vlc_preparser_req_NewThumbnail(thumbnailer, item.get(),
                                           &thumb_arg, &cbs, &ctx);
        if (preparserReq == nullptr)
        {
            m_currentContext = nullptr;
            return nullptr;
        }

        if (vlc_preparser_Submit(thumbnailer, preparserReq) != VLC_SUCCESS)
        {
            vlc_preparser_req_Release(preparserReq);
            m_currentContext = nullptr;
            return nullptr;
        }

        while ( ctx.done == false )
            m_cond.wait( m_mutex );
        m_currentContext = nullptr;
    }

    return ctx.picture;
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
