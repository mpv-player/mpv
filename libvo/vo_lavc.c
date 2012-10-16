/*
 * video encoding using libavformat
 * Copyright (C) 2010 Nicolas George <george@nsup.org>
 * Copyright (C) 2011 Rudolf Polzer <divVerent@xonotic.org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include "mpcommon.h"
#include "options.h"
#include "fmt-conversion.h"
#include "libmpcodecs/mp_image.h"
#include "libmpcodecs/vfcap.h"
#include "subopt-helper.h"
#include "talloc.h"
#include "video_out.h"

#include "encode_lavc.h"

#include "sub/sub.h"
#include "libvo/osd.h"

struct priv {
    uint8_t *buffer;
    size_t buffer_size;
    AVStream *stream;
    int have_first_packet;

    int harddup;

    double lastpts;
    int64_t lastipts;
    int64_t lastframeipts;
    double expected_next_pts;
    mp_image_t *lastimg;
    int lastdisplaycount;

    AVRational worst_time_base;
    int worst_time_base_is_stream;

    struct osd_state *osd;

    struct mp_csp_details colorspace;
};

static int preinit(struct vo *vo, const char *arg)
{
    struct priv *vc;
    if (!encode_lavc_available(vo->encode_lavc_ctx)) {
        mp_msg(MSGT_ENCODE, MSGL_ERR,
               "vo-lavc: the option -o (output file) must be specified\n");
        return -1;
    }
    vo->priv = talloc_zero(vo, struct priv);
    vc = vo->priv;
    vc->harddup = vo->encode_lavc_ctx->options->harddup;
    vc->colorspace = (struct mp_csp_details) MP_CSP_DETAILS_DEFAULTS;
    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi, double pts);
static void uninit(struct vo *vo)
{
    struct priv *vc = vo->priv;
    if (!vc)
        return;

    if (vc->lastipts >= 0 && vc->stream)
        draw_image(vo, NULL, MP_NOPTS_VALUE);

    if (vc->lastimg) {
        // palette hack
        if (vc->lastimg->imgfmt == IMGFMT_RGB8
                || vc->lastimg->imgfmt == IMGFMT_BGR8)
            vc->lastimg->planes[1] = NULL;
        free_mp_image(vc->lastimg);
        vc->lastimg = NULL;
    }

    vo->priv = NULL;
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *vc = vo->priv;
    enum PixelFormat pix_fmt = imgfmt2pixfmt(format);
    AVRational display_aspect_ratio, image_aspect_ratio;
    AVRational aspect;

    if (!vc)
        return -1;

    display_aspect_ratio.num = d_width;
    display_aspect_ratio.den = d_height;
    image_aspect_ratio.num = width;
    image_aspect_ratio.den = height;
    aspect = av_div_q(display_aspect_ratio, image_aspect_ratio);

    if (vc->stream) {
        /* NOTE:
         * in debug builds we get a "comparison between signed and unsigned"
         * warning here. We choose to ignore that; just because ffmpeg currently
         * uses a plain 'int' for these struct fields, it doesn't mean it always
         * will */
        if (width == vc->stream->codec->width &&
                height == vc->stream->codec->height) {
            if (aspect.num != vc->stream->codec->sample_aspect_ratio.num ||
                    aspect.den != vc->stream->codec->sample_aspect_ratio.den) {
                /* aspect-only changes are not critical */
                mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: unsupported pixel aspect "
                       "ratio change from %d:%d to %d:%d\n",
                       vc->stream->codec->sample_aspect_ratio.num,
                       vc->stream->codec->sample_aspect_ratio.den,
                       aspect.num, aspect.den);
            }
            return 0;
        }

        /* FIXME Is it possible with raw video? */
        mp_msg(MSGT_ENCODE, MSGL_ERR,
               "vo-lavc: resolution changes not supported.\n");
        goto error;
    }

    vc->lastipts = MP_NOPTS_VALUE;
    vc->lastframeipts = MP_NOPTS_VALUE;

    if (pix_fmt == PIX_FMT_NONE)
        goto error;  /* imgfmt2pixfmt already prints something */

    vc->stream = encode_lavc_alloc_stream(vo->encode_lavc_ctx,
                                          AVMEDIA_TYPE_VIDEO);
    vc->stream->sample_aspect_ratio = vc->stream->codec->sample_aspect_ratio =
            aspect;
    vc->stream->codec->width = width;
    vc->stream->codec->height = height;
    vc->stream->codec->pix_fmt = pix_fmt;

    encode_lavc_set_csp(vo->encode_lavc_ctx, vc->stream, vc->colorspace.format);
    encode_lavc_set_csp_levels(vo->encode_lavc_ctx, vc->stream, vc->colorspace.levels_out);
    vc->colorspace.format = encode_lavc_get_csp(vo->encode_lavc_ctx, vc->stream);
    vc->colorspace.levels_out = encode_lavc_get_csp_levels(vo->encode_lavc_ctx, vc->stream);

    if (encode_lavc_open_codec(vo->encode_lavc_ctx, vc->stream) < 0)
        goto error;

    vc->buffer_size = 6 * width * height + 200;
    if (vc->buffer_size < FF_MIN_BUFFER_SIZE)
        vc->buffer_size = FF_MIN_BUFFER_SIZE;
    if (vc->buffer_size < sizeof(AVPicture))
        vc->buffer_size = sizeof(AVPicture);

    vc->buffer = talloc_size(vc, vc->buffer_size);

    vc->lastimg = alloc_mpi(width, height, format);

    // palette hack
    if (vc->lastimg->imgfmt == IMGFMT_RGB8 ||
            vc->lastimg->imgfmt == IMGFMT_BGR8)
        vc->lastimg->planes[1] = talloc_zero_size(vc, 1024);

    return 0;

error:
    uninit(vo);
    return -1;
}

static int query_format(struct vo *vo, uint32_t format)
{
    enum PixelFormat pix_fmt = imgfmt2pixfmt(format);

    if (!vo->encode_lavc_ctx)
        return 0;

    return encode_lavc_supports_pixfmt(vo->encode_lavc_ctx, pix_fmt) ?
           VFCAP_CSP_SUPPORTED : 0;
}

static void write_packet(struct vo *vo, int size, AVPacket *packet)
{
    struct priv *vc = vo->priv;

    if (size < 0) {
        mp_msg(MSGT_ENCODE, MSGL_ERR, "vo-lavc: error encoding\n");
        return;
    }

    if (size > 0) {
        packet->stream_index = vc->stream->index;
        if (packet->pts != AV_NOPTS_VALUE) {
            packet->pts = av_rescale_q(packet->pts,
                                       vc->stream->codec->time_base,
                                       vc->stream->time_base);
        } else {
            mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: codec did not provide pts\n");
            packet->pts = av_rescale_q(vc->lastipts, vc->worst_time_base,
                                       vc->stream->time_base);
        }
        if (packet->dts != AV_NOPTS_VALUE) {
            packet->dts = av_rescale_q(packet->dts,
                                       vc->stream->codec->time_base,
                                       vc->stream->time_base);
        }
        if (packet->duration > 0) {
            packet->duration = av_rescale_q(packet->duration,
                                       vc->stream->codec->time_base,
                                       vc->stream->time_base);
        } else {
            // HACK: libavformat calculates dts wrong if the initial packet
            // duration is not set, but ONLY if the time base is "high" and if we
            // have b-frames!
            if (!packet->duration)
                if (!vc->have_first_packet)
                    if (vc->stream->codec->has_b_frames
                            || vc->stream->codec->max_b_frames)
                        if (vc->stream->time_base.num * 1000LL <=
                                vc->stream->time_base.den)
                            packet->duration = FFMAX(1, av_rescale_q(1,
                                 vc->stream->codec->time_base, vc->stream->time_base));
        }

        if (encode_lavc_write_frame(vo->encode_lavc_ctx, packet) < 0) {
            mp_msg(MSGT_ENCODE, MSGL_ERR, "vo-lavc: error writing\n");
            return;
        }

        vc->have_first_packet = 1;
    }
}

static int encode_video(struct vo *vo, AVFrame *frame, AVPacket *packet)
{
    struct priv *vc = vo->priv;
    if (encode_lavc_oformat_flags(vo->encode_lavc_ctx) & AVFMT_RAWPICTURE) {
        if (!frame)
            return 0;
        memcpy(vc->buffer, frame, sizeof(AVPicture));
        mp_msg(MSGT_ENCODE, MSGL_DBG2, "vo-lavc: got pts %f\n",
               frame->pts * (double) vc->stream->codec->time_base.num /
                            (double) vc->stream->codec->time_base.den);
        packet->size = sizeof(AVPicture);
        return packet->size;
    } else {
        int got_packet = 0;
        int status = avcodec_encode_video2(vc->stream->codec, packet,
                                           frame, &got_packet);
        int size = (status < 0) ? status : got_packet ? packet->size : 0;

        if (frame)
            mp_msg(MSGT_ENCODE, MSGL_DBG2, "vo-lavc: got pts %f; out size: %d\n",
                   frame->pts * (double) vc->stream->codec->time_base.num /
                   (double) vc->stream->codec->time_base.den, size);

        encode_lavc_write_stats(vo->encode_lavc_ctx, vc->stream);
        return size;
    }
}

static void add_osd_to_lastimg_draw_func(void *ctx, int x0,int y0, int w,int h,unsigned char* src, unsigned char *srca, int stride){
    struct priv *vc = ctx;
    unsigned char* dst;
    if(w<=0 || h<=0) return; // nothing to do...
    //    printf("OSD redraw: %d;%d %dx%d  \n",x0,y0,w,h);
    dst=vc->lastimg->planes[0]+
        vc->lastimg->stride[0]*y0+
        (vc->lastimg->bpp>>3)*x0;
    switch(vc->lastimg->imgfmt){
        case IMGFMT_BGR12:
        case IMGFMT_RGB12:
            vo_draw_alpha_rgb12(w, h, src, srca, stride, dst, vc->lastimg->stride[0]);
            break;
        case IMGFMT_BGR15:
        case IMGFMT_RGB15:
            vo_draw_alpha_rgb15(w,h,src,srca,stride,dst,vc->lastimg->stride[0]);
            break;
        case IMGFMT_BGR16:
        case IMGFMT_RGB16:
            vo_draw_alpha_rgb16(w,h,src,srca,stride,dst,vc->lastimg->stride[0]);
            break;
        case IMGFMT_BGR24:
        case IMGFMT_RGB24:
            vo_draw_alpha_rgb24(w,h,src,srca,stride,dst,vc->lastimg->stride[0]);
            break;
        case IMGFMT_BGR32:
        case IMGFMT_RGB32:
            vo_draw_alpha_rgb32(w,h,src,srca,stride,dst,vc->lastimg->stride[0]);
            break;
        case IMGFMT_YV12:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
        case IMGFMT_YVU9:
        case IMGFMT_IF09:
        case IMGFMT_Y800:
        case IMGFMT_Y8:
            vo_draw_alpha_yv12(w,h,src,srca,stride,dst,vc->lastimg->stride[0]);
            break;
        case IMGFMT_YUY2:
            vo_draw_alpha_yuy2(w,h,src,srca,stride,dst,vc->lastimg->stride[0]);
            break;
        case IMGFMT_UYVY:
            vo_draw_alpha_yuy2(w,h,src,srca,stride,dst+1,vc->lastimg->stride[0]);
            break;
        default:
            mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: tried to draw OSD on an usnupported pixel format\n");
    }
}

static void add_osd_to_lastimg(struct vo *vo)
{
    struct priv *vc = vo->priv;
    if(vc->osd) {
        osd_draw_text(vc->osd, vc->lastimg->w, vc->lastimg->h, add_osd_to_lastimg_draw_func, vc);
    }
}

static void draw_image(struct vo *vo, mp_image_t *mpi, double pts)
{
    struct priv *vc = vo->priv;
    struct encode_lavc_context *ectx = vo->encode_lavc_ctx;
    int i, size;
    AVFrame *frame;
    AVCodecContext *avc;
    int64_t frameipts;
    double nextpts;

    if (!vc)
        return;
    if (!encode_lavc_start(ectx)) {
        mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: NOTE: skipped initial video frame (probably because audio is not there yet)\n");
        return;
    }
    if (pts == MP_NOPTS_VALUE) {
        if (mpi)
            mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: frame without pts, please report; synthesizing pts instead\n");
        pts = vc->expected_next_pts;
    }

    avc = vc->stream->codec;

    if (vc->worst_time_base.den == 0) {
        //if (avc->time_base.num / avc->time_base.den >= vc->stream->time_base.num / vc->stream->time_base.den)
        if (avc->time_base.num * (double) vc->stream->time_base.den >=
                vc->stream->time_base.num * (double) avc->time_base.den) {
            mp_msg(MSGT_ENCODE, MSGL_V, "vo-lavc: NOTE: using codec time base "
                   "(%d/%d) for frame dropping; the stream base (%d/%d) is "
                   "not worse.\n", (int)avc->time_base.num,
                   (int)avc->time_base.den, (int)vc->stream->time_base.num,
                   (int)vc->stream->time_base.den);
            vc->worst_time_base = avc->time_base;
            vc->worst_time_base_is_stream = 0;
        } else {
            mp_msg(MSGT_ENCODE, MSGL_WARN, "vo-lavc: NOTE: not using codec time "
                   "base (%d/%d) for frame dropping; the stream base (%d/%d) "
                   "is worse.\n", (int)avc->time_base.num,
                   (int)avc->time_base.den, (int)vc->stream->time_base.num,
                   (int)vc->stream->time_base.den);
            vc->worst_time_base = vc->stream->time_base;
            vc->worst_time_base_is_stream = 1;
        }

        // NOTE: we use the following "axiom" of av_rescale_q:
        // if time base A is worse than time base B, then
        //   av_rescale_q(av_rescale_q(x, A, B), B, A) == x
        // this can be proven as long as av_rescale_q rounds to nearest, which
        // it currently does

        // av_rescale_q(x, A, B) * B = "round x*A to nearest multiple of B"
        // and:
        //    av_rescale_q(av_rescale_q(x, A, B), B, A) * A
        // == "round av_rescale_q(x, A, B)*B to nearest multiple of A"
        // == "round 'round x*A to nearest multiple of B' to nearest multiple of A"
        //
        // assume this fails. Then there is a value of x*A, for which the
        // nearest multiple of B is outside the range [(x-0.5)*A, (x+0.5)*A[.
        // Absurd, as this range MUST contain at least one multiple of B.
    }

    double timeunit = (double)vc->worst_time_base.num / vc->worst_time_base.den;

    double outpts;
    if (ectx->options->rawts)
        outpts = pts;
    else if (ectx->options->copyts) {
        // fix the discontinuity pts offset
        nextpts = pts;
        if (ectx->discontinuity_pts_offset == MP_NOPTS_VALUE) {
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }
        else if (fabs(nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts) > 30) {
            mp_msg(MSGT_ENCODE, MSGL_WARN,
                    "vo-lavc: detected an unexpected discontinuity (pts jumped by "
                    "%f seconds)\n",
                    nextpts + ectx->discontinuity_pts_offset - ectx->next_in_pts);
            ectx->discontinuity_pts_offset = ectx->next_in_pts - nextpts;
        }

        outpts = pts + ectx->discontinuity_pts_offset;
    }
    else {
        // adjust pts by knowledge of audio pts vs audio playback time
        double duration = 0;
        if (ectx->last_video_in_pts != MP_NOPTS_VALUE)
            duration = pts - ectx->last_video_in_pts;
        if (duration < 0)
            duration = timeunit;   // XXX warn about discontinuity?
        outpts = vc->lastpts + duration;
        if (ectx->audio_pts_offset != MP_NOPTS_VALUE) {
            double adj = outpts - pts - ectx->audio_pts_offset;
            adj = FFMIN(adj, duration * 0.1);
            adj = FFMAX(adj, -duration * 0.1);
            outpts -= adj;
        }
    }
    vc->lastpts = outpts;
    ectx->last_video_in_pts = pts;
    frameipts = floor((outpts + encode_lavc_getoffset(ectx, vc->stream))
                      / timeunit + 0.5);

    // calculate expected pts of next video frame
    vc->expected_next_pts = pts + timeunit;

    if (!ectx->options->rawts && ectx->options->copyts) {
        // set next allowed output pts value
        nextpts = vc->expected_next_pts + ectx->discontinuity_pts_offset;
        if (nextpts > ectx->next_in_pts)
            ectx->next_in_pts = nextpts;
    }

    // never-drop mode
    if (ectx->options->neverdrop && frameipts <= vc->lastipts) {
        mp_msg(MSGT_ENCODE, MSGL_INFO, "vo-lavc: -oneverdrop increased pts by %d\n",
               (int) (vc->lastipts - frameipts + 1));
        frameipts = vc->lastipts + 1;
        vc->lastpts = frameipts * timeunit - encode_lavc_getoffset(ectx, vc->stream);
    }

    if (vc->lastipts != MP_NOPTS_VALUE) {
        frame = avcodec_alloc_frame();

        // we have a valid image in lastimg
        while (vc->lastipts < frameipts) {
            int64_t thisduration = vc->harddup ? 1 : (frameipts - vc->lastipts);
            AVPacket packet;

            avcodec_get_frame_defaults(frame);

            // this is a nop, unless the worst time base is the STREAM time base
            frame->pts = av_rescale_q(vc->lastipts, vc->worst_time_base,
                                      avc->time_base);

            for (i = 0; i < 4; i++) {
                frame->data[i] = vc->lastimg->planes[i];
                frame->linesize[i] = vc->lastimg->stride[i];
            }
            frame->quality = avc->global_quality;

            av_init_packet(&packet);
            packet.data = vc->buffer;
            packet.size = vc->buffer_size;
            size = encode_video(vo, frame, &packet);
            write_packet(vo, size, &packet);

            vc->lastipts += thisduration;
            ++vc->lastdisplaycount;
        }

        av_free(frame);
    }

    if (!mpi) {
        // finish encoding
        do {
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = vc->buffer;
            packet.size = vc->buffer_size;
            size = encode_video(vo, NULL, &packet);
            write_packet(vo, size, &packet);
        } while (size > 0);
    } else {
        if (frameipts >= vc->lastframeipts) {
            if (vc->lastframeipts != MP_NOPTS_VALUE && vc->lastdisplaycount != 1)
                mp_msg(MSGT_ENCODE, MSGL_INFO,
                       "vo-lavc: Frame at pts %d got displayed %d times\n",
                       (int) vc->lastframeipts, vc->lastdisplaycount);
            copy_mpi(vc->lastimg, mpi);
            add_osd_to_lastimg(vo);

            // palette hack
            if (vc->lastimg->imgfmt == IMGFMT_RGB8 ||
                    vc->lastimg->imgfmt == IMGFMT_BGR8)
                memcpy(vc->lastimg->planes[1], mpi->planes[1], 1024);

            vc->lastframeipts = vc->lastipts = frameipts;
            if (ectx->options->rawts && vc->lastipts < 0) {
                mp_msg(MSGT_ENCODE, MSGL_ERR, "vo-lavc: why does this happen? DEBUG THIS! vc->lastipts = %lld\n", (long long) vc->lastipts);
                vc->lastipts = -1;
            }
            vc->lastdisplaycount = 0;
        } else
            mp_msg(MSGT_ENCODE, MSGL_INFO, "vo-lavc: Frame at pts %d got dropped "
                   "entirely because pts went backwards\n", (int) frameipts);
    }
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *vc = vo->priv;
    switch (request) {
    case VOCTRL_QUERY_FORMAT:
        return query_format(vo, *((uint32_t *)data));
    case VOCTRL_DRAW_IMAGE:
        draw_image(vo, (mp_image_t *)data, vo->next_pts);
        return 0;
    case VOCTRL_SET_YUV_COLORSPACE:
        vc->colorspace = *(struct mp_csp_details *)data;
        if (vc->stream) {
            encode_lavc_set_csp(vo->encode_lavc_ctx, vc->stream, vc->colorspace.format);
            encode_lavc_set_csp_levels(vo->encode_lavc_ctx, vc->stream, vc->colorspace.levels_out);
            vc->colorspace.format = encode_lavc_get_csp(vo->encode_lavc_ctx, vc->stream);
            vc->colorspace.levels_out = encode_lavc_get_csp_levels(vo->encode_lavc_ctx, vc->stream);
        }
        return 1;
    case VOCTRL_GET_YUV_COLORSPACE:
        *(struct mp_csp_details *)data = vc->colorspace;
        return 1;
    }
    return VO_NOTIMPL;
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *vc = vo->priv;
    vc->osd = osd;
    if(vc->lastimg)
        osd_update(vc->osd, vc->lastimg->w, vc->lastimg->h);
}

static void flip_page_timed(struct vo *vo, unsigned int pts_us, int duration)
{
}

static void check_events(struct vo *vo)
{
}

const struct vo_driver video_out_lavc = {
    .is_new = true,
    .buffer_frames = false,
    .info = &(const struct vo_info_s){
        "video encoding using libavcodec",
        "lavc",
        "Nicolas George <george@nsup.org>, Rudolf Polzer <divVerent@xonotic.org>",
        ""
    },
    .preinit = preinit,
    .config = config,
    .control = control,
    .uninit = uninit,
    .check_events = check_events,
    .draw_osd = draw_osd,
    .flip_page_timed = flip_page_timed,
};

// vim: sw=4 ts=4 et
