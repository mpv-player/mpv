/*
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
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>

#include <libavutil/common.h>
#include <libavutil/opt.h>
#include <libavutil/intreadwrite.h>

#include "talloc.h"
#include "config.h"
#include "mp_msg.h"
#include "options.h"
#include "av_opts.h"

#include "mpbswap.h"
#include "fmt-conversion.h"

#include "vd.h"
#include "img_format.h"
#include "libmpdemux/stheader.h"
#include "libmpdemux/demux_packet.h"
#include "codec-cfg.h"
#include "osdep/numcores.h"
#include "libvo/csputils.h"

static const vd_info_t info = {
    "libavcodec video codecs",
    "ffmpeg",
    "",
    "",
    "native codecs",
    .print_name = "libavcodec",
};

#include "libavcodec/avcodec.h"

#if AVPALETTE_SIZE > 1024
#error palette too large, adapt libmpcodecs/vf.c:vf_get_image
#endif

typedef struct {
    AVCodecContext *avctx;
    AVFrame *pic;
    enum PixelFormat pix_fmt;
    int do_slices;
    int do_dr1;
    int vo_initialized;
    int best_csp;
    int qp_stat[32];
    double qp_sum;
    double inv_qp_sum;
    int ip_count;
    int b_count;
    AVRational last_sample_aspect_ratio;
    enum AVDiscard skip_frame;
} vd_ffmpeg_ctx;

#include "m_option.h"

static int get_buffer(AVCodecContext *avctx, AVFrame *pic);
static void release_buffer(AVCodecContext *avctx, AVFrame *pic);
static void draw_slice(struct AVCodecContext *s, const AVFrame *src,
                       int offset[4], int y, int type, int height);

static enum PixelFormat get_format(struct AVCodecContext *avctx,
                                   const enum PixelFormat *pix_fmt);
static void uninit(struct sh_video *sh);

const m_option_t lavc_decode_opts_conf[] = {
    OPT_INTRANGE("bug", lavc_param.workaround_bugs, 0, -1, 999999),
    OPT_FLAG_ON("gray", lavc_param.gray, 0),
    OPT_INTRANGE("idct", lavc_param.idct_algo, 0, 0, 99),
    OPT_INTRANGE("ec", lavc_param.error_concealment, 0, 0, 99),
    OPT_FLAG_ON("vstats", lavc_param.vstats, 0),
    OPT_INTRANGE("debug", lavc_param.debug, 0, 0, 9999999),
    OPT_INTRANGE("vismv", lavc_param.vismv, 0, 0, 9999999),
    OPT_INTRANGE("st", lavc_param.skip_top, 0, 0, 999),
    OPT_INTRANGE("sb", lavc_param.skip_bottom, 0, 0, 999),
    OPT_FLAG_CONSTANTS("fast", lavc_param.fast, 0, 0, CODEC_FLAG2_FAST),
    OPT_STRING("lowres", lavc_param.lowres_str, 0),
    OPT_STRING("skiploopfilter", lavc_param.skip_loop_filter_str, 0),
    OPT_STRING("skipidct", lavc_param.skip_idct_str, 0),
    OPT_STRING("skipframe", lavc_param.skip_frame_str, 0),
    OPT_INTRANGE("threads", lavc_param.threads, 0, 0, 16),
    OPT_FLAG_CONSTANTS("bitexact", lavc_param.bitexact, 0, 0, CODEC_FLAG_BITEXACT),
    OPT_STRING("o", lavc_param.avopt, 0),
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

static enum AVDiscard str2AVDiscard(char *str)
{
    if (!str)                               return AVDISCARD_DEFAULT;
    if (strcasecmp(str, "none"   ) == 0)    return AVDISCARD_NONE;
    if (strcasecmp(str, "default") == 0)    return AVDISCARD_DEFAULT;
    if (strcasecmp(str, "nonref" ) == 0)    return AVDISCARD_NONREF;
    if (strcasecmp(str, "bidir"  ) == 0)    return AVDISCARD_BIDIR;
    if (strcasecmp(str, "nonkey" ) == 0)    return AVDISCARD_NONKEY;
    if (strcasecmp(str, "all"    ) == 0)    return AVDISCARD_ALL;
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Unknown discard value %s\n", str);
    return AVDISCARD_DEFAULT;
}

static int init(sh_video_t *sh)
{
    struct lavc_param *lavc_param = &sh->opts->lavc_param;
    AVCodecContext *avctx;
    vd_ffmpeg_ctx *ctx;
    AVCodec *lavc_codec = NULL;
    enum PixelFormat rawfmt = PIX_FMT_NONE;
    int do_vis_debug = lavc_param->vismv ||
            (lavc_param->debug & (FF_DEBUG_VIS_MB_TYPE | FF_DEBUG_VIS_QP));

    ctx = sh->context = talloc_zero(NULL, vd_ffmpeg_ctx);

    if (sh->codec->dll) {
        lavc_codec = avcodec_find_decoder_by_name(sh->codec->dll);
        if (!lavc_codec) {
            mp_tmsg(MSGT_DECVIDEO, MSGL_ERR,
                    "Cannot find codec '%s' in libavcodec...\n",
                    sh->codec->dll);
            uninit(sh);
            return 0;
        }
    } else if (sh->libav_codec_id) {
        lavc_codec = avcodec_find_decoder(sh->libav_codec_id);
        if (!lavc_codec) {
            mp_tmsg(MSGT_DECVIDEO, MSGL_INFO, "Libavcodec has no decoder "
                   "for this codec\n");
            uninit(sh);
            return 0;
        }
    } else if (!IMGFMT_IS_HWACCEL(sh->format)) {
        rawfmt = imgfmt2pixfmt(sh->format);
        if (rawfmt != PIX_FMT_NONE)
            lavc_codec = avcodec_find_decoder_by_name("rawvideo");
    }
    if (!lavc_codec) {
        uninit(sh);
        return 0;
    }

    sh->codecname = lavc_codec->long_name;
    if (!sh->codecname)
        sh->codecname = lavc_codec->name;

    if (sh->opts->vd_use_slices
            && (lavc_codec->capabilities & CODEC_CAP_DRAW_HORIZ_BAND)
            && !do_vis_debug)
        ctx->do_slices = 1;

    if (lavc_codec->capabilities & CODEC_CAP_DR1 && !do_vis_debug
            && lavc_codec->id != CODEC_ID_H264
            && lavc_codec->id != CODEC_ID_INTERPLAY_VIDEO
            && lavc_codec->id != CODEC_ID_ROQ && lavc_codec->id != CODEC_ID_VP8
            && lavc_codec->id != CODEC_ID_LAGARITH)
        ctx->do_dr1 = 1;
    ctx->ip_count = ctx->b_count = 0;

    ctx->pic = avcodec_alloc_frame();
    ctx->avctx = avcodec_alloc_context3(lavc_codec);
    avctx = ctx->avctx;
    avctx->opaque = sh;
    avctx->codec_type = AVMEDIA_TYPE_VIDEO;
    avctx->codec_id = lavc_codec->id;

    if (lavc_codec->capabilities & CODEC_CAP_HWACCEL   // XvMC
        || lavc_codec->capabilities & CODEC_CAP_HWACCEL_VDPAU) {
        ctx->do_dr1    = true;
        ctx->do_slices = true;
        lavc_param->threads    = 1;
        avctx->get_format      = get_format;
        avctx->get_buffer      = get_buffer;
        avctx->release_buffer  = release_buffer;
        avctx->reget_buffer    = get_buffer;
        avctx->draw_horiz_band = draw_slice;
        if (lavc_codec->capabilities & CODEC_CAP_HWACCEL_VDPAU)
            mp_msg(MSGT_DECVIDEO, MSGL_V, "[VD_FFMPEG] VDPAU hardware "
                   "decoding.\n");
        avctx->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
    }

    if (lavc_param->threads == 0) {
        int threads = default_thread_count();
        if (threads < 1) {
            mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[VD_FFMPEG] Could not determine "
                   "thread count to use, defaulting to 1.\n");
            threads = 1;
        }
        threads = FFMIN(threads, 16);
        lavc_param->threads = threads;
    }
    /* Our get_buffer and draw_horiz_band callbacks are not safe to call
     * from other threads. */
    if (lavc_param->threads > 1) {
        ctx->do_dr1 = false;
        ctx->do_slices = false;
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Asking decoder to use "
                "%d threads if supported.\n", lavc_param->threads);
    }

    if (ctx->do_dr1) {
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
        avctx->get_buffer = get_buffer;
        avctx->release_buffer = release_buffer;
        avctx->reget_buffer = get_buffer;
    }

    avctx->flags |= lavc_param->bitexact;

    avctx->coded_width = sh->disp_w;
    avctx->coded_height = sh->disp_h;
    avctx->workaround_bugs = lavc_param->workaround_bugs;
    if (lavc_param->gray)
        avctx->flags |= CODEC_FLAG_GRAY;
    avctx->flags2 |= lavc_param->fast;
    if (rawfmt == PIX_FMT_NONE) {
        avctx->codec_tag = sh->format;
    } else {
        avctx->pix_fmt = rawfmt;
    }
    avctx->stream_codec_tag = sh->video.fccHandler;
    avctx->idct_algo = lavc_param->idct_algo;
    avctx->error_concealment = lavc_param->error_concealment;
    avctx->debug = lavc_param->debug;
    if (lavc_param->debug)
        av_log_set_level(AV_LOG_DEBUG);
    avctx->debug_mv = lavc_param->vismv;
    avctx->skip_top   = lavc_param->skip_top;
    avctx->skip_bottom = lavc_param->skip_bottom;
    if (lavc_param->lowres_str != NULL) {
        int lowres, lowres_w;
        sscanf(lavc_param->lowres_str, "%d,%d", &lowres, &lowres_w);
        if (lowres < 1 || lowres > 16 ||
                lowres_w > 0 && avctx->width < lowres_w)
            lowres = 0;
        avctx->lowres = lowres;
    }
    avctx->skip_loop_filter = str2AVDiscard(lavc_param->skip_loop_filter_str);
    avctx->skip_idct = str2AVDiscard(lavc_param->skip_idct_str);
    avctx->skip_frame = str2AVDiscard(lavc_param->skip_frame_str);

    if (lavc_param->avopt) {
        if (parse_avopts(avctx, lavc_param->avopt) < 0) {
            mp_msg(MSGT_DECVIDEO, MSGL_ERR,
                   "Your options /%s/ look like gibberish to me pal\n",
                   lavc_param->avopt);
            uninit(sh);
            return 0;
        }
    }

    // Do this after the above avopt handling in case it changes values
    ctx->skip_frame = avctx->skip_frame;

    mp_dbg(MSGT_DECVIDEO, MSGL_DBG2,
           "libavcodec.size: %d x %d\n", avctx->width, avctx->height);
    switch (sh->format) {
    case mmioFOURCC('S','V','Q','3'):
    case mmioFOURCC('A','V','R','n'):
    case mmioFOURCC('M','J','P','G'):
        /* AVRn stores huffman table in AVI header */
        /* Pegasus MJPEG stores it also in AVI header, but it uses the common
         * MJPG fourcc :( */
        if (!sh->bih || sh->bih->biSize <= sizeof(*sh->bih))
            break;
        av_opt_set_int(avctx, "extern_huff", 1, AV_OPT_SEARCH_CHILDREN);
        avctx->extradata_size = sh->bih->biSize - sizeof(*sh->bih);
        avctx->extradata = av_mallocz(avctx->extradata_size +
                                      FF_INPUT_BUFFER_PADDING_SIZE);
        memcpy(avctx->extradata, sh->bih + 1, avctx->extradata_size);
        break;

    case mmioFOURCC('R','V','1','0'):
    case mmioFOURCC('R','V','1','3'):
    case mmioFOURCC('R','V','2','0'):
    case mmioFOURCC('R','V','3','0'):
    case mmioFOURCC('R','V','4','0'):
        if (sh->bih->biSize < sizeof(*sh->bih) + 8) {
            // only 1 packet per frame & sub_id from fourcc
            avctx->extradata_size = 8;
            avctx->extradata = av_mallocz(avctx->extradata_size +
                                          FF_INPUT_BUFFER_PADDING_SIZE);
            ((uint32_t *)avctx->extradata)[0] = 0;
            ((uint32_t *)avctx->extradata)[1] =
                    sh->format == mmioFOURCC('R','V','1','3') ?
                    0x10003001 : 0x10000000;
        } else {
            // has extra slice header (demux_rm or rm->avi streamcopy)
            avctx->extradata_size = sh->bih->biSize - sizeof(*sh->bih);
            avctx->extradata = av_mallocz(avctx->extradata_size +
                                          FF_INPUT_BUFFER_PADDING_SIZE);
            memcpy(avctx->extradata, sh->bih + 1, avctx->extradata_size);
        }
        break;

    default:
        if (!sh->bih || sh->bih->biSize <= sizeof(*sh->bih))
            break;
        avctx->extradata_size = sh->bih->biSize - sizeof(*sh->bih);
        avctx->extradata = av_mallocz(avctx->extradata_size +
                                      FF_INPUT_BUFFER_PADDING_SIZE);
        memcpy(avctx->extradata, sh->bih + 1, avctx->extradata_size);
        break;
    }

    if (sh->bih)
        avctx->bits_per_coded_sample = sh->bih->biBitCount;

    avctx->thread_count = lavc_param->threads;

    /* open it */
    if (avcodec_open2(avctx, lavc_codec, NULL) < 0) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_ERR, "Could not open codec.\n");
        uninit(sh);
        return 0;
    }
    return 1;
}

static void uninit(sh_video_t *sh)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;

    sh->codecname = NULL;
    if (sh->opts->lavc_param.vstats && avctx->coded_frame) {
        for (int i = 1; i < 32; i++)
            mp_msg(MSGT_DECVIDEO, MSGL_INFO,
                   "QP: %d, count: %d\n", i, ctx->qp_stat[i]);
        mp_tmsg(MSGT_DECVIDEO, MSGL_INFO, "[VD_FFMPEG] Arithmetic mean of QP: "
            "%2.4f, Harmonic mean of QP: %2.4f\n",
            ctx->qp_sum / avctx->coded_frame->coded_picture_number,
            1.0 / (ctx->inv_qp_sum / avctx->coded_frame->coded_picture_number));
    }

    if (avctx) {
        if (avctx->codec && avcodec_close(avctx) < 0)
            mp_tmsg(MSGT_DECVIDEO, MSGL_ERR, "Could not close codec.\n");

        av_freep(&avctx->extradata);
        av_freep(&avctx->slice_offset);
    }

    av_freep(&avctx);
    av_freep(&ctx->pic);
    talloc_free(ctx);
}

static void draw_slice(struct AVCodecContext *s,
                       const AVFrame *src, int offset[4],
                       int y, int type, int height)
{
    sh_video_t *sh = s->opaque;
    uint8_t *source[MP_MAX_PLANES] = {
        src->data[0] + offset[0], src->data[1] + offset[1],
        src->data[2] + offset[2]
    };
    int strides[MP_MAX_PLANES] = {
        src->linesize[0], src->linesize[1], src->linesize[2]
    };
    if (height < 0) {
        int i;
        height = -height;
        y -= height;
        for (i = 0; i < MP_MAX_PLANES; i++) {
            strides[i] = -strides[i];
            source[i] -= strides[i];
        }
    }
    if (y < sh->disp_h) {
        height = FFMIN(height, sh->disp_h - y);
        mpcodecs_draw_slice(sh, source, strides, sh->disp_w, height, 0, y);
    }
}


static int init_vo(sh_video_t *sh, enum PixelFormat pix_fmt)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;
    float aspect = av_q2d(avctx->sample_aspect_ratio) *
                   avctx->width / avctx->height;
    int width, height;

    width = avctx->width;
    height = avctx->height;

    /* Reconfiguring filter/VO chain may invalidate direct rendering buffers
     * we have allocated for libavcodec (including the VDPAU HW decoding
     * case). Is it guaranteed that the code below only triggers in a situation
     * with no busy direct rendering buffers for reference frames?
     */
    if (av_cmp_q(avctx->sample_aspect_ratio, ctx->last_sample_aspect_ratio) ||
            width != sh->disp_w || height != sh->disp_h ||
            pix_fmt != ctx->pix_fmt || !ctx->vo_initialized) {
        ctx->vo_initialized = 0;
        mp_msg(MSGT_DECVIDEO, MSGL_V, "[ffmpeg] aspect_ratio: %f\n", aspect);

        // Do not overwrite s->aspect on the first call, so that a container
        // aspect if available is preferred.
        // But set it even if the sample aspect did not change, since a
        // resolution change can cause an aspect change even if the
        // _sample_ aspect is unchanged.
        if (sh->aspect == 0 || ctx->last_sample_aspect_ratio.den)
            sh->aspect = aspect;
        ctx->last_sample_aspect_ratio = avctx->sample_aspect_ratio;
        sh->disp_w = width;
        sh->disp_h = height;
        ctx->pix_fmt = pix_fmt;
        ctx->best_csp = pixfmt2imgfmt(pix_fmt);
        const unsigned int *supported_fmts;
        if (ctx->best_csp == IMGFMT_YV12)
            supported_fmts = (const unsigned int[]){
                IMGFMT_YV12, IMGFMT_I420, IMGFMT_IYUV, 0xffffffff
            };
        else if (ctx->best_csp == IMGFMT_422P)
            supported_fmts = (const unsigned int[]){
                IMGFMT_422P, IMGFMT_YV12, IMGFMT_I420, IMGFMT_IYUV, 0xffffffff
            };
        else
            supported_fmts = (const unsigned int[]){ctx->best_csp, 0xffffffff};

        sh->colorspace = avcol_spc_to_mp_csp(avctx->colorspace);
        sh->color_range = avcol_range_to_mp_csp_levels(avctx->color_range);

        if (!mpcodecs_config_vo2(sh, sh->disp_w, sh->disp_h, supported_fmts,
                                 ctx->best_csp))
            return -1;
        ctx->vo_initialized = 1;
    }
    return 0;
}

static int get_buffer(AVCodecContext *avctx, AVFrame *pic)
{
    sh_video_t *sh = avctx->opaque;
    vd_ffmpeg_ctx *ctx = sh->context;
    mp_image_t *mpi = NULL;
    int flags = MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE |
                MP_IMGFLAG_PREFER_ALIGNED_STRIDE;
    int type = MP_IMGTYPE_IPB;
    int width = avctx->width;
    int height = avctx->height;
    // special case to handle reget_buffer without buffer hints
    if (pic->opaque && pic->data[0] && !pic->buffer_hints)
        return 0;
    avcodec_align_dimensions(avctx, &width, &height);

    if (pic->buffer_hints) {
        mp_msg(MSGT_DECVIDEO, MSGL_DBG2, "Buffer hints: %u\n",
               pic->buffer_hints);
        type = MP_IMGTYPE_TEMP;
        if (pic->buffer_hints & FF_BUFFER_HINTS_READABLE)
            flags |= MP_IMGFLAG_READABLE;
        if (pic->buffer_hints & FF_BUFFER_HINTS_PRESERVE) {
            type = MP_IMGTYPE_STATIC;
            flags |= MP_IMGFLAG_PRESERVE;
        }
        if (pic->buffer_hints & FF_BUFFER_HINTS_REUSABLE) {
            type = MP_IMGTYPE_STATIC;
            flags |= MP_IMGFLAG_PRESERVE;
        }
        flags |= ctx->do_slices ? MP_IMGFLAG_DRAW_CALLBACK : 0;
        mp_msg(MSGT_DECVIDEO, MSGL_DBG2,
               type == MP_IMGTYPE_STATIC ? "using STATIC\n" : "using TEMP\n");
    } else {
        if (!pic->reference) {
            ctx->b_count++;
            flags |= ctx->do_slices ? MP_IMGFLAG_DRAW_CALLBACK : 0;
        } else {
            ctx->ip_count++;
            flags |= MP_IMGFLAG_PRESERVE | MP_IMGFLAG_READABLE
                     | (ctx->do_slices ? MP_IMGFLAG_DRAW_CALLBACK : 0);
        }
    }

    if (init_vo(sh, avctx->pix_fmt) < 0) {
        avctx->release_buffer = avcodec_default_release_buffer;
        avctx->get_buffer = avcodec_default_get_buffer;
        avctx->reget_buffer = avcodec_default_reget_buffer;
        if (pic->data[0])
            release_buffer(avctx, pic);
        return avctx->get_buffer(avctx, pic);
    }

    if (IMGFMT_IS_HWACCEL(ctx->best_csp))
        type =  MP_IMGTYPE_NUMBERED | (0xffff << 16);
    else if (!pic->buffer_hints) {
        if (ctx->b_count > 1 || ctx->ip_count > 2) {
            mp_tmsg(MSGT_DECVIDEO, MSGL_WARN, "[VD_FFMPEG] DRI failure.\n");

            ctx->do_dr1 = 0; //FIXME
            avctx->get_buffer = avcodec_default_get_buffer;
            avctx->reget_buffer = avcodec_default_reget_buffer;
            if (pic->data[0])
                release_buffer(avctx, pic);
            return avctx->get_buffer(avctx, pic);
        }

        if (avctx->has_b_frames || ctx->b_count)
            type = MP_IMGTYPE_IPB;
        else
            type = MP_IMGTYPE_IP;
        mp_msg(MSGT_DECVIDEO, MSGL_DBG2,
               type == MP_IMGTYPE_IPB ? "using IPB\n" : "using IP\n");
    }

    if (ctx->best_csp == IMGFMT_RGB8 || ctx->best_csp == IMGFMT_BGR8)
        flags |= MP_IMGFLAG_RGB_PALETTE;
    mpi = mpcodecs_get_image(sh, type, flags, width, height);
    if (!mpi)
        return -1;

    // ok, let's see what did we get:
    if (mpi->flags & MP_IMGFLAG_DRAW_CALLBACK &&
        !(mpi->flags & MP_IMGFLAG_DIRECT)) {
        // nice, filter/vo likes draw_callback :)
        avctx->draw_horiz_band = draw_slice;
    } else
        avctx->draw_horiz_band = NULL;
    if (IMGFMT_IS_HWACCEL(mpi->imgfmt))
        avctx->draw_horiz_band = draw_slice;

    pic->data[0] = mpi->planes[0];
    pic->data[1] = mpi->planes[1];
    pic->data[2] = mpi->planes[2];
    pic->data[3] = mpi->planes[3];

    /* Note: some (many) codecs in libavcodec require
     * linesize[1] == linesize[2] and no changes between frames.
     * Lavc will check that and die with an error message if it's not true.
     */
    pic->linesize[0] = mpi->stride[0];
    pic->linesize[1] = mpi->stride[1];
    pic->linesize[2] = mpi->stride[2];
    pic->linesize[3] = mpi->stride[3];

    pic->opaque = mpi;

    pic->type = FF_BUFFER_TYPE_USER;

    /* The libavcodec reordered_opaque functionality is implemented by
     * a similar copy in avcodec_default_get_buffer() and without a
     * workaround like this it'd stop working when a custom buffer
     * callback is used.
     */
    pic->reordered_opaque = avctx->reordered_opaque;
    return 0;
}

static void release_buffer(struct AVCodecContext *avctx, AVFrame *pic)
{
    mp_image_t *mpi = pic->opaque;
    sh_video_t *sh = avctx->opaque;
    vd_ffmpeg_ctx *ctx = sh->context;

    if (ctx->ip_count <= 2 && ctx->b_count <= 1) {
        if (mpi->flags & MP_IMGFLAG_PRESERVE)
            ctx->ip_count--;
        else
            ctx->b_count--;
    }

    if (mpi) {
        // Palette support: free palette buffer allocated in get_buffer
        if (mpi->bpp == 8)
            av_freep(&mpi->planes[1]);
        // release mpi (in case MPI_IMGTYPE_NUMBERED is used, e.g. for VDPAU)
        mpi->usage_count--;
    }

    if (pic->type != FF_BUFFER_TYPE_USER) {
        avcodec_default_release_buffer(avctx, pic);
        return;
    }

    for (int i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

static av_unused void swap_palette(void *pal)
{
    int i;
    uint32_t *p = pal;
    for (i = 0; i < AVPALETTE_COUNT; i++)
        p[i] = le2me_32(p[i]);
}

static struct mp_image *decode(struct sh_video *sh, struct demux_packet *packet,
                               void *data, int len, int flags,
                               double *reordered_pts)
{
    int got_picture = 0;
    int ret;
    vd_ffmpeg_ctx *ctx = sh->context;
    AVFrame *pic = ctx->pic;
    AVCodecContext *avctx = ctx->avctx;
    struct lavc_param *lavc_param = &sh->opts->lavc_param;
    mp_image_t *mpi = NULL;
    int dr1 = ctx->do_dr1;
    AVPacket pkt;

    if (!dr1)
        avctx->draw_horiz_band = NULL;

    if (flags & 2)
        avctx->skip_frame = AVDISCARD_ALL;
    else if (flags & 1)
        avctx->skip_frame = AVDISCARD_NONREF;
    else
        avctx->skip_frame = ctx->skip_frame;

    av_init_packet(&pkt);
    pkt.data = data;
    pkt.size = len;
    /* Some codecs (ZeroCodec, some cases of PNG) may want keyframe info
     * from demuxer. */
    if (packet && packet->keyframe)
        pkt.flags |= AV_PKT_FLAG_KEY;
    if (packet && packet->avpacket) {
        pkt.side_data = packet->avpacket->side_data;
        pkt.side_data_elems = packet->avpacket->side_data_elems;
    }
    // The avcodec opaque field stupidly supports only int64_t type
    union pts { int64_t i; double d; };
    avctx->reordered_opaque = (union pts){.d = *reordered_pts}.i;
    ret = avcodec_decode_video2(avctx, pic, &got_picture, &pkt);
    *reordered_pts = (union pts){.i = pic->reordered_opaque}.d;

    dr1 = ctx->do_dr1;
    if (ret < 0)
        mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Error while decoding frame!\n");
    //-- vstats generation
    while (lavc_param->vstats) { // always one time loop
        static FILE *fvstats = NULL;
        char filename[20];
        static long long int all_len = 0;
        static int frame_number = 0;
        static double all_frametime = 0.0;
        AVFrame *pic = avctx->coded_frame;
        double quality = 0.0;

        if (!pic)
            break;

        if (!fvstats) {
            time_t today2;
            struct tm *today;
            today2 = time(NULL);
            today = localtime(&today2);
            sprintf(filename, "vstats_%02d%02d%02d.log", today->tm_hour,
                    today->tm_min, today->tm_sec);
            fvstats = fopen(filename, "w");
            if (!fvstats) {
                perror("fopen");
                lavc_param->vstats = 0; // disable block
                break;
                /*exit(1);*/
            }
        }

        // average MB quantizer
        {
            int x, y;
            int w = ((avctx->width << avctx->lowres) + 15) >> 4;
            int h = ((avctx->height << avctx->lowres) + 15) >> 4;
            int8_t *q = pic->qscale_table;
            for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++)
                    quality += (double)*(q + x);
                q += pic->qstride;
            }
            quality /= w * h;
        }

        all_len += len;
        all_frametime += sh->frametime;
        fprintf(fvstats, "frame= %5d q= %2.2f f_size= %6d s_size= %8.0fkB ",
                ++frame_number, quality, len, (double)all_len / 1024);
        fprintf(fvstats, "time= %0.3f br= %7.1fkbits/s avg_br= %7.1fkbits/s ",
                all_frametime, (double)(len * 8) / sh->frametime / 1000.0,
                (double)(all_len * 8) / all_frametime / 1000.0);
        switch (pic->pict_type) {
        case AV_PICTURE_TYPE_I:
            fprintf(fvstats, "type= I\n");
            break;
        case AV_PICTURE_TYPE_P:
            fprintf(fvstats, "type= P\n");
            break;
        case AV_PICTURE_TYPE_S:
            fprintf(fvstats, "type= S\n");
            break;
        case AV_PICTURE_TYPE_B:
            fprintf(fvstats, "type= B\n");
            break;
        default:
            fprintf(fvstats, "type= ? (%d)\n", pic->pict_type);
            break;
        }

        ctx->qp_stat[(int)(quality + 0.5)]++;
        ctx->qp_sum += quality;
        ctx->inv_qp_sum += 1.0 / (double)quality;

        break;
    }
    //--

    if (!got_picture)
        return NULL;                     // skipped image

    if (init_vo(sh, avctx->pix_fmt) < 0)
        return NULL;

    if (dr1 && pic->opaque)
        mpi = (mp_image_t *)pic->opaque;

    if (!mpi)
        mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, MP_IMGFLAG_PRESERVE,
                                 avctx->width, avctx->height);
    if (!mpi) {   // temporary error?
        mp_tmsg(MSGT_DECVIDEO, MSGL_WARN,
                "[VD_FFMPEG] Couldn't allocate image for codec.\n");
        return NULL;
    }

    if (!dr1) {
        mpi->planes[0] = pic->data[0];
        mpi->planes[1] = pic->data[1];
        mpi->planes[2] = pic->data[2];
        mpi->planes[3] = pic->data[3];
        mpi->stride[0] = pic->linesize[0];
        mpi->stride[1] = pic->linesize[1];
        mpi->stride[2] = pic->linesize[2];
        mpi->stride[3] = pic->linesize[3];
    }

    if (!mpi->planes[0])
        return NULL;

    if (ctx->best_csp == IMGFMT_422P && mpi->chroma_y_shift == 1) {
        // we have 422p but user wants 420p
        mpi->stride[1] *= 2;
        mpi->stride[2] *= 2;
    }

#if BYTE_ORDER == BIG_ENDIAN
    // FIXME: this might cause problems for buffers with FF_BUFFER_HINTS_PRESERVE
    if (mpi->bpp == 8)
        swap_palette(mpi->planes[1]);
#endif

    mpi->qscale = pic->qscale_table;
    mpi->qstride = pic->qstride;
    mpi->pict_type = pic->pict_type;
    mpi->qscale_type = pic->qscale_type;
    mpi->fields = MP_IMGFIELD_ORDERED;
    if (pic->interlaced_frame)
        mpi->fields |= MP_IMGFIELD_INTERLACED;
    if (pic->top_field_first)
        mpi->fields |= MP_IMGFIELD_TOP_FIRST;
    if (pic->repeat_pict == 1)
        mpi->fields |= MP_IMGFIELD_REPEAT_FIRST;

    return mpi;
}

static enum PixelFormat get_format(struct AVCodecContext *avctx,
                                   const enum PixelFormat *fmt)
{
    sh_video_t *sh = avctx->opaque;
    int i;

    for (i = 0; fmt[i] != PIX_FMT_NONE; i++) {
        int imgfmt = pixfmt2imgfmt(fmt[i]);
        if (!IMGFMT_IS_HWACCEL(imgfmt))
            continue;
        mp_msg(MSGT_DECVIDEO, MSGL_V, "[VD_FFMPEG] Trying pixfmt=%d.\n", i);
        if (init_vo(sh, fmt[i]) >= 0)
            break;
    }
    return fmt[i];
}

static int control(sh_video_t *sh, int cmd, void *arg, ...)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;
    switch (cmd) {
    case VDCTRL_QUERY_FORMAT: {
        int format = (*((int *)arg));
        if (format == ctx->best_csp)
            return CONTROL_TRUE;
        // possible conversions:
        switch (format) {
        case IMGFMT_YV12:
        case IMGFMT_IYUV:
        case IMGFMT_I420:
            // "converted" using pointer/stride modification
            if (ctx->best_csp == IMGFMT_YV12)
                return CONTROL_TRUE;   // u/v swap
            if (ctx->best_csp == IMGFMT_422P && !ctx->do_dr1)
                return CONTROL_TRUE;   // half stride
            break;
        }
        return CONTROL_FALSE;
    }
    case VDCTRL_RESYNC_STREAM:
        avcodec_flush_buffers(avctx);
        return CONTROL_TRUE;
    case VDCTRL_QUERY_UNSEEN_FRAMES:;
        int delay = avctx->has_b_frames;
        if (avctx->active_thread_type & FF_THREAD_FRAME)
            delay += avctx->thread_count - 1;
        return delay + 10;
    case VDCTRL_RESET_ASPECT:
        if (ctx->vo_initialized)
            ctx->vo_initialized = false;
        init_vo(sh, avctx->pix_fmt);
        return true;
    }
    return CONTROL_UNKNOWN;
}

const struct vd_functions mpcodecs_vd_ffmpeg = {
    .info = &info,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode2 = decode
};
