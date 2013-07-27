/*
 * Various utilities for command line tools
 * Copyright (c) 2000-2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


/*
 * NOTE: this file is for compatibility with older versions of
 *       libavcodec, before AVFrame reference counting was introduced.
 *       It is not compiled if libavcodec is new enough.
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>


#include <libavcodec/avcodec.h>
#include <libavutil/avassert.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/common.h>

#include "config.h"

#include "lavc.h"

#if HAVE_PTHREADS
#include <pthread.h>
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
#define pool_lock() pthread_mutex_lock(&pool_mutex)
#define pool_unlock() pthread_mutex_unlock(&pool_mutex)
#else
#define pool_lock() 0
#define pool_unlock() 0
#endif

typedef struct FramePool {
    struct FrameBuffer *list;
    // used to deal with frames that live past the time the pool should live
    int dead;
    int refcount; // number of allocated buffers (not in list)
} FramePool;

typedef struct FrameBuffer {
    uint8_t *base[4];
    uint8_t *data[4];
    int  linesize[4];

    int h, w;
    int pix_fmt;

    int used_by_decoder, needed_by_decoder;
    int refcount;
    struct FramePool *pool;
    struct FrameBuffer *next;
} FrameBuffer;


static int alloc_buffer(FramePool *pool, AVCodecContext *s)
{
    const AVPixFmtDescriptor *desc = &av_pix_fmt_descriptors[s->pix_fmt];
    FrameBuffer *buf;
    int i, ret;
    int pixel_size;
    int h_chroma_shift, v_chroma_shift;
    int edge = 32; // XXX should be avcodec_get_edge_width(), but that fails on svq1
    int w = s->width, h = s->height;

    if (!desc)
        return AVERROR(EINVAL);
    pixel_size = desc->comp[0].step_minus1 + 1;

    buf = av_mallocz(sizeof(*buf));
    if (!buf)
        return AVERROR(ENOMEM);

    avcodec_align_dimensions(s, &w, &h);

    if (!(s->flags & CODEC_FLAG_EMU_EDGE)) {
        w += 2*edge;
        h += 2*edge;
    }

    if ((ret = av_image_alloc(buf->base, buf->linesize, w, h,
                              s->pix_fmt, 32)) < 0) {
        av_freep(&buf);
        av_log(s, AV_LOG_ERROR, "alloc_buffer: av_image_alloc() failed\n");
        return ret;
    }
    /* XXX this shouldn't be needed, but some tests break without this line
     * those decoders are buggy and need to be fixed.
     * the following tests fail:
     * cdgraphics, ansi, aasc, fraps-v1, qtrle-1bit
     */
    memset(buf->base[0], 128, ret);

    avcodec_get_chroma_sub_sample(s->pix_fmt, &h_chroma_shift, &v_chroma_shift);
    for (i = 0; i < FF_ARRAY_ELEMS(buf->data); i++) {
        const int h_shift = i==0 ? 0 : h_chroma_shift;
        const int v_shift = i==0 ? 0 : v_chroma_shift;
        if ((s->flags & CODEC_FLAG_EMU_EDGE) || !buf->linesize[i] || !buf->base[i])
            buf->data[i] = buf->base[i];
        else
            buf->data[i] = buf->base[i] +
                           FFALIGN((buf->linesize[i]*edge >> v_shift) +
                                   (pixel_size*edge >> h_shift), 32);
    }
    buf->w       = s->width;
    buf->h       = s->height;
    buf->pix_fmt = s->pix_fmt;
    buf->pool    = pool;

    buf->next    = pool->list;
    pool->list   = buf;
    return 0;
}

int mp_codec_get_buffer(AVCodecContext *s, AVFrame *frame)
{
    sh_video_t *sh = s->opaque;
    struct lavc_ctx *ctx = sh->context;

    if (!ctx->dr1_buffer_pool) {
        ctx->dr1_buffer_pool = av_mallocz(sizeof(*ctx->dr1_buffer_pool));
        if (!ctx->dr1_buffer_pool)
            return AVERROR(ENOMEM);
    }

    FramePool *pool = ctx->dr1_buffer_pool;
    FrameBuffer *buf;
    int ret, i;

    if(av_image_check_size(s->width, s->height, 0, s) || s->pix_fmt<0) {
        av_log(s, AV_LOG_ERROR, "codec_get_buffer: image parameters invalid\n");
        return -1;
    }

    pool_lock();

    if (!pool->list && (ret = alloc_buffer(pool, s)) < 0) {
        pool_unlock();
        return ret;
    }

    buf = pool->list;
    if (buf->w != s->width || buf->h != s->height || buf->pix_fmt != s->pix_fmt) {
        pool->list = buf->next;
        av_freep(&buf->base[0]);
        av_free(buf);
        if ((ret = alloc_buffer(pool, s)) < 0) {
            pool_unlock();
            return ret;
        }
        buf = pool->list;
    }
    av_assert0(!buf->refcount);
    buf->refcount++;

    pool->list = buf->next;
    pool->refcount++;

    pool_unlock();

    frame->opaque        = buf;
    frame->type          = FF_BUFFER_TYPE_USER;
    frame->extended_data = frame->data;

    buf->used_by_decoder = buf->needed_by_decoder = 1;
    if (frame->buffer_hints & FF_BUFFER_HINTS_VALID) {
        buf->needed_by_decoder =
            (frame->buffer_hints & FF_BUFFER_HINTS_PRESERVE) ||
            (frame->buffer_hints & FF_BUFFER_HINTS_REUSABLE);
    } else {
        buf->needed_by_decoder = !!frame->reference;
    }

    for (i = 0; i < FF_ARRAY_ELEMS(buf->data); i++) {
        frame->base[i]     = buf->base[i];  // XXX h264.c uses base though it shouldn't
        frame->data[i]     = buf->data[i];
        frame->linesize[i] = buf->linesize[i];
    }

    return 0;
}

void mp_buffer_ref(struct FrameBuffer *buf)
{
    pool_lock();
    buf->refcount++;
    pool_unlock();
}

void mp_buffer_unref(struct FrameBuffer *buf)
{
    FramePool *pool = buf->pool;
    bool pool_dead;

    pool_lock();

    av_assert0(pool->refcount > 0);
    av_assert0(buf->refcount > 0);
    buf->refcount--;
    if (!buf->refcount) {
        FrameBuffer *tmp;
        for(tmp= pool->list; tmp; tmp= tmp->next)
            av_assert1(tmp != buf);

        buf->next = pool->list;
        pool->list = buf;
        pool->refcount--;
    }

    pool_dead = pool->dead && pool->refcount == 0;
    pool_unlock();

    if (pool_dead)
        mp_buffer_pool_free(&pool);
}

bool mp_buffer_is_unique(struct FrameBuffer *buf)
{
    int refcount;
    pool_lock();
    refcount = buf->refcount;
    // Decoder has a reference, but doesn't want to use it. (ffmpeg has no good
    // way of transferring frame ownership to the user.)
    if (buf->used_by_decoder && !buf->needed_by_decoder)
        refcount--;
    pool_unlock();
    return refcount == 1;
}

void mp_codec_release_buffer(AVCodecContext *s, AVFrame *frame)
{
    FrameBuffer *buf = frame->opaque;
    int i;

    if(frame->type!=FF_BUFFER_TYPE_USER) {
        avcodec_default_release_buffer(s, frame);
        return;
    }

    buf->used_by_decoder = buf->needed_by_decoder = 0;

    for (i = 0; i < FF_ARRAY_ELEMS(frame->data); i++)
        frame->data[i] = NULL;

    mp_buffer_unref(buf);
}

void mp_buffer_pool_free(struct FramePool **p_pool)
{
    struct FramePool *pool = *p_pool;
    if (!pool)
        return;

    pool_lock();

    while (pool->list) {
        FrameBuffer *buf = pool->list;
        pool->list = buf->next;
        av_assert0(buf->refcount == 0);
        av_freep(&buf->base[0]);
        av_free(buf);
    }
    pool->dead = 1;
    if (pool->refcount == 0)
        av_free(pool);

    pool_unlock();

    *p_pool = NULL;
}
