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
#include <libavutil/pixdesc.h>

#include "compat/libav.h"

#include "talloc.h"
#include "config.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/options.h"
#include "mpvcore/bstr.h"
#include "mpvcore/av_opts.h"
#include "mpvcore/av_common.h"
#include "mpvcore/codecs.h"

#include "compat/mpbswap.h"
#include "video/fmt-conversion.h"

#include "vd.h"
#include "video/img_format.h"
#include "video/mp_image_pool.h"
#include "video/filter/vf.h"
#include "demux/stheader.h"
#include "demux/demux_packet.h"
#include "osdep/numcores.h"
#include "video/csputils.h"

#include "lavc.h"

#if AVPALETTE_SIZE != MP_PALETTE_SIZE
#error palette too large, adapt video/mp_image.h:MP_PALETTE_SIZE
#endif

#include "mpvcore/m_option.h"

static void init_avctx(sh_video_t *sh, const char *decoder,
                       struct vd_lavc_hwdec *hwdec);
static void uninit_avctx(sh_video_t *sh);
static void setup_refcounting_hw(struct AVCodecContext *s);

static enum PixelFormat get_format_hwdec(struct AVCodecContext *avctx,
                                         const enum PixelFormat *pix_fmt);

static void uninit(struct sh_video *sh);

#define OPT_BASE_STRUCT struct MPOpts

const m_option_t lavc_decode_opts_conf[] = {
    OPT_FLAG_CONSTANTS("fast", lavc_param.fast, 0, 0, CODEC_FLAG2_FAST),
    OPT_STRING("skiploopfilter", lavc_param.skip_loop_filter_str, 0),
    OPT_STRING("skipidct", lavc_param.skip_idct_str, 0),
    OPT_STRING("skipframe", lavc_param.skip_frame_str, 0),
    OPT_INTRANGE("threads", lavc_param.threads, 0, 0, 16),
    OPT_FLAG_CONSTANTS("bitexact", lavc_param.bitexact, 0, 0, CODEC_FLAG_BITEXACT),
    OPT_STRING("o", lavc_param.avopt, 0),
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

const struct vd_lavc_hwdec mp_vd_lavc_vdpau;
const struct vd_lavc_hwdec mp_vd_lavc_vdpau_old;
const struct vd_lavc_hwdec mp_vd_lavc_vda;
const struct vd_lavc_hwdec mp_vd_lavc_vaapi;
const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy;

static const struct vd_lavc_hwdec mp_vd_lavc_crystalhd = {
    .type = HWDEC_CRYSTALHD,
    .codec_pairs = (const char *[]) {
        "mpeg2",        "mpeg2_crystalhd",
        "msmpeg4",      "msmpeg4_crystalhd",
        "wmv3",         "wmv3_crystalhd",
        "vc1",          "vc1_crystalhd",
        "h264",         "h264_crystalhd",
        "mpeg4",        "mpeg4_crystalhd",
        NULL
    },
};

static const struct vd_lavc_hwdec *hwdec_list[] = {
#if CONFIG_VDPAU
#if HAVE_AV_CODEC_NEW_VDPAU_API
    &mp_vd_lavc_vdpau,
#else
    &mp_vd_lavc_vdpau_old,
#endif
#endif // CONFIG_VDPAU
#if CONFIG_VDA
    &mp_vd_lavc_vda,
#endif
    &mp_vd_lavc_crystalhd,
#if CONFIG_VAAPI
    &mp_vd_lavc_vaapi,
    &mp_vd_lavc_vaapi_copy,
#endif
    NULL
};

static struct vd_lavc_hwdec *find_hwcodec(enum hwdec_type api)
{
    for (int n = 0; hwdec_list[n]; n++) {
        if (hwdec_list[n]->type == api)
            return (struct vd_lavc_hwdec *)hwdec_list[n];
    }
    return NULL;
}

static bool hwdec_codec_allowed(sh_video_t *sh, const char *codec)
{
    bstr s = bstr0(sh->opts->hwdec_codecs);
    while (s.len) {
        bstr item;
        bstr_split_tok(s, ",", &item, &s);
        if (bstr_equals0(item, "all") || bstr_equals0(item, codec))
            return true;
    }
    return false;
}

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

static int hwdec_probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                       const char *decoder, const char **hw_decoder)
{
    if (hwdec->codec_pairs) {
        for (int n = 0; hwdec->codec_pairs[n + 0]; n += 2) {
            const char *sw = hwdec->codec_pairs[n + 0];
            const char *hw = hwdec->codec_pairs[n + 1];
            if (decoder && strcmp(decoder, sw) == 0) {
                AVCodec *codec = avcodec_find_decoder_by_name(hw);
                *hw_decoder = hw;
                if (codec)
                    goto found;
            }
        }
        return HWDEC_ERR_NO_CODEC;
    found: ;
    }
    int r = 0;
    if (hwdec->probe)
        r = hwdec->probe(hwdec, info, decoder);
    return r;
}

static bool probe_hwdec(sh_video_t *sh, bool autoprobe, enum hwdec_type api,
                        const char *decoder, struct vd_lavc_hwdec **use_hwdec,
                        const char **use_decoder)
{
    struct vd_lavc_hwdec *hwdec = find_hwcodec(api);
    if (!hwdec) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Requested hardware decoder not "
                "compiled.\n");
        return false;
    }
    const char *hw_decoder = NULL;
    int r = hwdec_probe(hwdec, sh->hwdec_info, decoder, &hw_decoder);
    if (r >= 0) {
        *use_hwdec = hwdec;
        *use_decoder = hw_decoder;
        return true;
    } else if (r == HWDEC_ERR_NO_CODEC) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Hardware decoder '%s' not found in "
                "libavcodec.\n", hw_decoder ? hw_decoder : decoder);
    } else if (r == HWDEC_ERR_NO_CTX && !autoprobe) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_WARN, "VO does not support requested "
                "hardware decoder.\n");
    }
    return false;
}


static int init(sh_video_t *sh, const char *decoder)
{
    vd_ffmpeg_ctx *ctx;
    ctx = sh->context = talloc_zero(NULL, vd_ffmpeg_ctx);
    ctx->non_dr1_pool = talloc_steal(ctx, mp_image_pool_new(16));

    if (bstr_endswith0(bstr0(decoder), "_vdpau")) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_WARN, "VDPAU decoder '%s' was requested. "
                "This way of enabling hardware\ndecoding is not supported "
                "anymore. Use --hwdec=vdpau instead.\nThe --hwdec-codec=... "
                "option can be used to restrict which codecs are\nenabled, "
                "otherwise all hardware decoding is tried for all codecs.\n",
                decoder);
        uninit(sh);
        return 0;
    }

    struct vd_lavc_hwdec *hwdec = NULL;
    const char *hw_decoder = NULL;

    if (hwdec_codec_allowed(sh, decoder)) {
        if (sh->opts->hwdec_api == HWDEC_AUTO) {
            for (int n = 0; hwdec_list[n]; n++) {
                if (probe_hwdec(sh, true, hwdec_list[n]->type, decoder,
                    &hwdec, &hw_decoder))
                    break;
            }
        } else if (sh->opts->hwdec_api != HWDEC_NONE) {
            probe_hwdec(sh, false, sh->opts->hwdec_api, decoder,
                        &hwdec, &hw_decoder);
        }
    } else {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Not trying to use hardware decoding: "
                "codec %s is blacklisted by user.\n", decoder);
    }

    if (hwdec) {
        ctx->software_fallback_decoder = talloc_strdup(ctx, decoder);
        if (hw_decoder)
            decoder = hw_decoder;
        mp_tmsg(MSGT_DECVIDEO, MSGL_INFO, "Trying to use hardware decoding.\n");
    } else if (sh->opts->hwdec_api != HWDEC_NONE) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_INFO, "Using software decoding.\n");
    }

    init_avctx(sh, decoder, hwdec);
    if (!ctx->avctx) {
        if (ctx->software_fallback_decoder) {
            mp_tmsg(MSGT_DECVIDEO, MSGL_ERR, "Error initializing hardware "
                    "decoding, falling back to software decoding.\n");
            decoder = ctx->software_fallback_decoder;
            ctx->software_fallback_decoder = NULL;
            init_avctx(sh, decoder, NULL);
        }
        if (!ctx->avctx) {
            uninit(sh);
            return 0;
        }
    }
    return 1;
}

static void set_from_bih(AVCodecContext *avctx, uint32_t format,
                         BITMAPINFOHEADER *bih)
{

    switch (format) {
    case mmioFOURCC('S','V','Q','3'):
    case mmioFOURCC('A','V','R','n'):
    case mmioFOURCC('M','J','P','G'):
        /* AVRn stores huffman table in AVI header */
        /* Pegasus MJPEG stores it also in AVI header, but it uses the common
         * MJPG fourcc :( */
        if (bih->biSize <= sizeof(*bih))
           break;
        av_opt_set_int(avctx, "extern_huff", 1, AV_OPT_SEARCH_CHILDREN);
        avctx->extradata_size = bih->biSize - sizeof(*bih);
        avctx->extradata = av_mallocz(avctx->extradata_size +
                                      FF_INPUT_BUFFER_PADDING_SIZE);
        memcpy(avctx->extradata, bih + 1, avctx->extradata_size);
        break;

    case mmioFOURCC('R','V','1','0'):
    case mmioFOURCC('R','V','1','3'):
    case mmioFOURCC('R','V','2','0'):
    case mmioFOURCC('R','V','3','0'):
    case mmioFOURCC('R','V','4','0'):
        if (bih->biSize < sizeof(*bih) + 8) {
            // only 1 packet per frame & sub_id from fourcc
           avctx->extradata_size = 8;
            avctx->extradata = av_mallocz(avctx->extradata_size +
                                          FF_INPUT_BUFFER_PADDING_SIZE);
            ((uint32_t *)avctx->extradata)[0] = 0;
            ((uint32_t *)avctx->extradata)[1] =
                    format == mmioFOURCC('R','V','1','3') ?
                    0x10003001 : 0x10000000;
        } else {
            // has extra slice header (demux_rm or rm->avi streamcopy)
           avctx->extradata_size = bih->biSize - sizeof(*bih);
            avctx->extradata = av_mallocz(avctx->extradata_size +
                                          FF_INPUT_BUFFER_PADDING_SIZE);
            memcpy(avctx->extradata, bih + 1, avctx->extradata_size);
        }
        break;

    default:
        if (bih->biSize <= sizeof(*bih))
            break;
        avctx->extradata_size = bih->biSize - sizeof(*bih);
        avctx->extradata = av_mallocz(avctx->extradata_size +
                                      FF_INPUT_BUFFER_PADDING_SIZE);
        memcpy(avctx->extradata, bih + 1, avctx->extradata_size);
        break;
    }

    avctx->bits_per_coded_sample = bih->biBitCount;
    avctx->coded_width  = bih->biWidth;
    avctx->coded_height = bih->biHeight;
}

static void init_avctx(sh_video_t *sh, const char *decoder,
                       struct vd_lavc_hwdec *hwdec)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    struct lavc_param *lavc_param = &sh->opts->lavc_param;
    bool mp_rawvideo = false;

    assert(!ctx->avctx);

    if (strcmp(decoder, "mp-rawvideo") == 0) {
        mp_rawvideo = true;
        decoder = "rawvideo";
    }

    AVCodec *lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec)
        return;

    ctx->hwdec_info = sh->hwdec_info;

    ctx->do_dr1 = ctx->do_hw_dr1 = 0;
    ctx->pix_fmt = PIX_FMT_NONE;
    ctx->image_params = (struct mp_image_params){0};
    ctx->vo_image_params = (struct mp_image_params){0};
    ctx->hwdec = hwdec;
    ctx->pic = avcodec_alloc_frame();
    ctx->avctx = avcodec_alloc_context3(lavc_codec);
    AVCodecContext *avctx = ctx->avctx;
    avctx->opaque = sh;
    avctx->codec_type = AVMEDIA_TYPE_VIDEO;
    avctx->codec_id = lavc_codec->id;

    avctx->thread_count = lavc_param->threads;

    if (ctx->hwdec && ctx->hwdec->allocate_image) {
        ctx->do_hw_dr1         = true;
        avctx->thread_count    = 1;
        if (ctx->hwdec->image_formats)
            avctx->get_format  = get_format_hwdec;
        setup_refcounting_hw(avctx);
        if (ctx->hwdec->init && ctx->hwdec->init(ctx) < 0) {
            uninit_avctx(sh);
            return;
        }
    } else {
#if HAVE_AVUTIL_REFCOUNTING
        avctx->refcounted_frames = 1;
#else
        if (lavc_codec->capabilities & CODEC_CAP_DR1) {
            ctx->do_dr1            = true;
            avctx->get_buffer      = mp_codec_get_buffer;
            avctx->release_buffer  = mp_codec_release_buffer;
        }
#endif
    }

    if (avctx->thread_count == 0) {
        int threads = default_thread_count();
        if (threads < 1) {
            mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[VD_FFMPEG] Could not determine "
                   "thread count to use, defaulting to 1.\n");
            threads = 1;
        }
        threads = FFMIN(threads, 16);
        avctx->thread_count = threads;
    }

    avctx->flags |= lavc_param->bitexact;

    avctx->flags2 |= lavc_param->fast;
    avctx->skip_loop_filter = str2AVDiscard(lavc_param->skip_loop_filter_str);
    avctx->skip_idct = str2AVDiscard(lavc_param->skip_idct_str);
    avctx->skip_frame = str2AVDiscard(lavc_param->skip_frame_str);

    if (lavc_param->avopt) {
        if (parse_avopts(avctx, lavc_param->avopt) < 0) {
            mp_msg(MSGT_DECVIDEO, MSGL_ERR,
                   "Your options /%s/ look like gibberish to me pal\n",
                   lavc_param->avopt);
            uninit_avctx(sh);
            return;
        }
    }

    // Do this after the above avopt handling in case it changes values
    ctx->skip_frame = avctx->skip_frame;

    avctx->codec_tag = sh->format;
    avctx->coded_width  = sh->disp_w;
    avctx->coded_height = sh->disp_h;

    // demux_mkv
    if (sh->bih)
        set_from_bih(avctx, sh->format, sh->bih);

    if (mp_rawvideo && sh->format >= IMGFMT_START && sh->format < IMGFMT_END) {
        avctx->pix_fmt = imgfmt2pixfmt(sh->format);
        avctx->codec_tag = 0;
    }

    if (sh->gsh->lav_headers)
        mp_copy_lav_codec_headers(avctx, sh->gsh->lav_headers);

    /* open it */
    if (avcodec_open2(avctx, lavc_codec, NULL) < 0) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_ERR, "Could not open codec.\n");
        uninit_avctx(sh);
        return;
    }
}

static void uninit_avctx(sh_video_t *sh)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;

    if (avctx) {
        if (avctx->codec && avcodec_close(avctx) < 0)
            mp_tmsg(MSGT_DECVIDEO, MSGL_ERR, "Could not close codec.\n");

        av_freep(&avctx->extradata);
        av_freep(&avctx->slice_offset);
    }

    av_freep(&ctx->avctx);
    avcodec_free_frame(&ctx->pic);

    if (ctx->hwdec && ctx->hwdec->uninit)
        ctx->hwdec->uninit(ctx);

#if !HAVE_AVUTIL_REFCOUNTING
    mp_buffer_pool_free(&ctx->dr1_buffer_pool);
#endif
    ctx->last_sample_aspect_ratio = (AVRational){0, 0};
}

static void uninit(sh_video_t *sh)
{
    vd_ffmpeg_ctx *ctx = sh->context;

    uninit_avctx(sh);
    talloc_free(ctx);
}

static void update_image_params(sh_video_t *sh, AVFrame *frame)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    int width = frame->width;
    int height = frame->height;
    float aspect = av_q2d(frame->sample_aspect_ratio) * width / height;
    int pix_fmt = frame->format;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 40, 0)
    pix_fmt = ctx->avctx->pix_fmt;
#endif

    if (av_cmp_q(frame->sample_aspect_ratio, ctx->last_sample_aspect_ratio) ||
            width != sh->disp_w || height != sh->disp_h ||
            pix_fmt != ctx->pix_fmt || !ctx->image_params.imgfmt)
    {
        mp_image_pool_clear(ctx->non_dr1_pool);
        mp_msg(MSGT_DECVIDEO, MSGL_V, "[ffmpeg] aspect_ratio: %f\n", aspect);

        // Do not overwrite s->aspect on the first call, so that a container
        // aspect if available is preferred.
        // But set it even if the sample aspect did not change, since a
        // resolution change can cause an aspect change even if the
        // _sample_ aspect is unchanged.
        if (sh->aspect == 0 || ctx->last_sample_aspect_ratio.den)
            sh->aspect = aspect;
        ctx->last_sample_aspect_ratio = frame->sample_aspect_ratio;
        sh->disp_w = width;
        sh->disp_h = height;

        ctx->pix_fmt = pix_fmt;
        ctx->best_csp = pixfmt2imgfmt(pix_fmt);

        ctx->image_params = (struct mp_image_params) {
            .imgfmt = ctx->best_csp,
            .w = width,
            .h = height,
            // Ideally, we should also set aspect ratio, but we aren't there yet
            // - so vd.c calculates display size from sh->aspect.
            .d_w = width,
            .d_h = height,
            .colorspace = avcol_spc_to_mp_csp(ctx->avctx->colorspace),
            .colorlevels = avcol_range_to_mp_csp_levels(ctx->avctx->color_range),
            .chroma_location =
                avchroma_location_to_mp(ctx->avctx->chroma_sample_location),
        };
    }
}

static enum PixelFormat get_format_hwdec(struct AVCodecContext *avctx,
                                         const enum PixelFormat *fmt)
{
    sh_video_t *sh = avctx->opaque;
    vd_ffmpeg_ctx *ctx = sh->context;

    mp_msg(MSGT_DECVIDEO, MSGL_V, "Pixel formats supported by decoder:");
    for (int i = 0; fmt[i] != PIX_FMT_NONE; i++)
        mp_msg(MSGT_DECVIDEO, MSGL_V, " %s", av_get_pix_fmt_name(fmt[i]));
    mp_msg(MSGT_DECVIDEO, MSGL_V, "\n");

    assert(ctx->hwdec);

    for (int i = 0; fmt[i] != PIX_FMT_NONE; i++) {
        const int *okfmt = ctx->hwdec->image_formats;
        for (int n = 0; okfmt && okfmt[n]; n++) {
            if (imgfmt2pixfmt(okfmt[n]) == fmt[i])
                return fmt[i];
        }
    }

    return PIX_FMT_NONE;
}

static struct mp_image *get_surface_hwdec(struct sh_video *sh, AVFrame *pic)
{
    vd_ffmpeg_ctx *ctx = sh->context;

    /* Decoders using ffmpeg's hwaccel architecture (everything except vdpau)
     * can fall back to software decoding automatically. However, we don't
     * want that: multithreading was already disabled. ffmpeg's fallback
     * isn't really useful, and causes more trouble than it helps.
     *
     * Instead of trying to "adjust" the thread_count fields in avctx, let
     * decoding fail hard. Then decode_with_fallback() will do our own software
     * fallback. Fully reinitializing the decoder is saner, and will probably
     * save us from other weird corner cases, like having to "reroute" the
     * get_buffer callback.
     */
    int imgfmt = pixfmt2imgfmt(pic->format);
    if (!IMGFMT_IS_HWACCEL(imgfmt))
        return NULL;

    // Using frame->width/height is bad. For non-mod 16 video (which would
    // require alignment of frame sizes) we want the decoded size, not the
    // aligned size. At least vdpau needs this: the video mixer is created
    // with decoded size, and the video surfaces must have matching size.
    int w = ctx->avctx->width;
    int h = ctx->avctx->height;

    struct mp_image *mpi = ctx->hwdec->allocate_image(ctx, imgfmt, w, h);

    if (mpi) {
        for (int i = 0; i < 4; i++)
            pic->data[i] = mpi->planes[i];
    }

    return mpi;
}

#if HAVE_AVUTIL_REFCOUNTING

static void free_mpi(void *opaque, uint8_t *data)
{
    struct mp_image *mpi = opaque;
    talloc_free(mpi);
}

static int get_buffer2_hwdec(AVCodecContext *avctx, AVFrame *pic, int flags)
{
    sh_video_t *sh = avctx->opaque;

    struct mp_image *mpi = get_surface_hwdec(sh, pic);
    if (!mpi)
        return -1;

    pic->buf[0] = av_buffer_create(NULL, 0, free_mpi, mpi, 0);

    return 0;
}

static void setup_refcounting_hw(AVCodecContext *avctx)
{
    avctx->get_buffer2 = get_buffer2_hwdec;
    avctx->refcounted_frames = 1;
}

#else /* HAVE_AVUTIL_REFCOUNTING */

static int get_buffer_hwdec(AVCodecContext *avctx, AVFrame *pic)
{
    sh_video_t *sh = avctx->opaque;

    struct mp_image *mpi = get_surface_hwdec(sh, pic);
    if (!mpi)
        return -1;

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

static void release_buffer_hwdec(AVCodecContext *avctx, AVFrame *pic)
{
    mp_image_t *mpi = pic->opaque;

    assert(pic->type == FF_BUFFER_TYPE_USER);
    assert(mpi);

    talloc_free(mpi);

    for (int i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

static void setup_refcounting_hw(AVCodecContext *avctx)
{
    avctx->get_buffer = get_buffer_hwdec;
    avctx->release_buffer = release_buffer_hwdec;
}

#endif /* HAVE_AVUTIL_REFCOUNTING */

#if HAVE_AVUTIL_REFCOUNTING

static struct mp_image *image_from_decoder(struct sh_video *sh)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    AVFrame *pic = ctx->pic;

    struct mp_image *img = mp_image_from_av_frame(pic);
    av_frame_unref(pic);

    return img;
}

#else /* HAVE_AVUTIL_REFCOUNTING */

static void fb_ref(void *b)
{
    mp_buffer_ref(b);
}

static void fb_unref(void *b)
{
    mp_buffer_unref(b);
}

static bool fb_is_unique(void *b)
{
    return mp_buffer_is_unique(b);
}

static struct mp_image *image_from_decoder(struct sh_video *sh)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    AVFrame *pic = ctx->pic;

    struct mp_image new = {0};
    mp_image_copy_fields_from_av_frame(&new, pic);

    struct mp_image *mpi;
    if (ctx->do_hw_dr1 && pic->opaque) {
        mpi = pic->opaque; // reordered frame
        assert(mpi);
        mpi = mp_image_new_ref(mpi);
        mp_image_copy_attributes(mpi, &new);
    } else if (ctx->do_dr1 && pic->opaque) {
        struct FrameBuffer *fb = pic->opaque;
        mp_buffer_ref(fb); // initial reference for mpi
        mpi = mp_image_new_external_ref(&new, fb, fb_ref, fb_unref,
                                        fb_is_unique, NULL);
    } else {
        mpi = mp_image_pool_new_copy(ctx->non_dr1_pool, &new);
    }
    return mpi;
}

#endif /* HAVE_AVUTIL_REFCOUNTING */

static int decode(struct sh_video *sh, struct demux_packet *packet,
                  int flags, double *reordered_pts, struct mp_image **out_image)
{
    int got_picture = 0;
    int ret;
    vd_ffmpeg_ctx *ctx = sh->context;
    AVFrame *pic = ctx->pic;
    AVCodecContext *avctx = ctx->avctx;
    AVPacket pkt;

    if (flags & 2)
        avctx->skip_frame = AVDISCARD_ALL;
    else if (flags & 1)
        avctx->skip_frame = AVDISCARD_NONREF;
    else
        avctx->skip_frame = ctx->skip_frame;

    mp_set_av_packet(&pkt, packet);

    // The avcodec opaque field stupidly supports only int64_t type
    union pts { int64_t i; double d; };
    avctx->reordered_opaque = (union pts){.d = *reordered_pts}.i;
    ret = avcodec_decode_video2(avctx, pic, &got_picture, &pkt);
    if (ret < 0) {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Error while decoding frame!\n");
        return -1;
    }
    *reordered_pts = (union pts){.i = pic->reordered_opaque}.d;

    if (!got_picture)
        return 0;                     // skipped image

    update_image_params(sh, pic);

    struct mp_image *mpi = image_from_decoder(sh);
    assert(mpi->planes[0]);
    mp_image_set_params(mpi, &ctx->image_params);

    if (ctx->hwdec && ctx->hwdec->process_image)
        mpi = ctx->hwdec->process_image(ctx, mpi);

    struct mp_image_params vo_params;
    mp_image_params_from_image(&vo_params, mpi);

    if (!mp_image_params_equals(&vo_params, &ctx->vo_image_params)) {
        if (mpcodecs_reconfig_vo(sh, &vo_params) < 0) {
            talloc_free(mpi);
            return -1;
        }
        ctx->vo_image_params = vo_params;
    }

    *out_image = mpi;
    return 1;
}

static struct mp_image *decode_with_fallback(struct sh_video *sh,
                                struct demux_packet *packet,
                                int flags, double *reordered_pts)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    if (!ctx->avctx)
        return NULL;

    struct mp_image *mpi = NULL;
    int res = decode(sh, packet, flags, reordered_pts, &mpi);
    if (res >= 0)
        return mpi;

    // Failed hardware decoding? Try again in software.
    if (ctx->software_fallback_decoder) {
        uninit_avctx(sh);
        sh->vf_initialized = 0;
        mp_tmsg(MSGT_DECVIDEO, MSGL_ERR, "Error using hardware "
                "decoding, falling back to software decoding.\n");
        const char *decoder = ctx->software_fallback_decoder;
        ctx->software_fallback_decoder = NULL;
        init_avctx(sh, decoder, NULL);
        if (ctx->avctx) {
            mpi = NULL;
            decode(sh, packet, flags, reordered_pts, &mpi);
            return mpi;
        }
    }

    return NULL;
}

static int control(sh_video_t *sh, int cmd, void *arg)
{
    vd_ffmpeg_ctx *ctx = sh->context;
    AVCodecContext *avctx = ctx->avctx;
    switch (cmd) {
    case VDCTRL_RESYNC_STREAM:
        avcodec_flush_buffers(avctx);
        return CONTROL_TRUE;
    case VDCTRL_QUERY_UNSEEN_FRAMES:;
        int delay = avctx->has_b_frames;
        assert(delay >= 0);
        if (avctx->active_thread_type & FF_THREAD_FRAME)
            delay += avctx->thread_count - 1;
        *(int *)arg = delay;
        return CONTROL_TRUE;
    case VDCTRL_REINIT_VO:
        if (ctx->vo_image_params.imgfmt)
            mpcodecs_reconfig_vo(sh, &ctx->vo_image_params);
        return true;
    case VDCTRL_GET_PARAMS:
        *(struct mp_image_params *)arg = ctx->vo_image_params;
        return ctx->vo_image_params.imgfmt ? true : CONTROL_NA;
    }
    return CONTROL_UNKNOWN;
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_lavc_decoders(list, AVMEDIA_TYPE_VIDEO);
    mp_add_decoder(list, "lavc", "mp-rawvideo", "mp-rawvideo",
                   "raw video");
}

const struct vd_functions mpcodecs_vd_ffmpeg = {
    .name = "lavc",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode = decode_with_fallback,
};
