/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
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

#include "talloc.h"
#include "config.h"
#include "common/msg.h"
#include "options/options.h"
#include "misc/bstr.h"
#include "common/av_common.h"
#include "common/codecs.h"

#include "video/fmt-conversion.h"

#include "vd.h"
#include "video/img_format.h"
#include "video/filter/vf.h"
#include "video/decode/dec_video.h"
#include "demux/stheader.h"
#include "demux/packet.h"
#include "video/csputils.h"
#include "video/sws_utils.h"

#include "lavc.h"

#if AVPALETTE_SIZE != MP_PALETTE_SIZE
#error palette too large, adapt video/mp_image.h:MP_PALETTE_SIZE
#endif

#include "options/m_option.h"

static void init_avctx(struct dec_video *vd, const char *decoder,
                       struct vd_lavc_hwdec *hwdec);
static void uninit_avctx(struct dec_video *vd);

static int get_buffer2_hwdec(AVCodecContext *avctx, AVFrame *pic, int flags);
static enum AVPixelFormat get_format_hwdec(struct AVCodecContext *avctx,
                                           const enum AVPixelFormat *pix_fmt);

#define OPT_BASE_STRUCT struct vd_lavc_params

struct vd_lavc_params {
    int fast;
    int show_all;
    int skip_loop_filter;
    int skip_idct;
    int skip_frame;
    int framedrop;
    int threads;
    int bitexact;
    int check_hw_profile;
    char **avopts;
};

static const struct m_opt_choice_alternatives discard_names[] = {
    {"none",        AVDISCARD_NONE},
    {"default",     AVDISCARD_DEFAULT},
    {"nonref",      AVDISCARD_NONREF},
    {"bidir",       AVDISCARD_BIDIR},
    {"nonkey",      AVDISCARD_NONKEY},
    {"all",         AVDISCARD_ALL},
    {0}
};
#define OPT_DISCARD(name, field, flags) \
    OPT_GENERAL(int, name, field, flags, .type = CONF_TYPE_CHOICE, \
                .priv = (void *)discard_names)

const struct m_sub_options vd_lavc_conf = {
    .opts = (const m_option_t[]){
        OPT_FLAG("fast", fast, 0),
        OPT_FLAG("show-all", show_all, 0),
        OPT_DISCARD("skiploopfilter", skip_loop_filter, 0),
        OPT_DISCARD("skipidct", skip_idct, 0),
        OPT_DISCARD("skipframe", skip_frame, 0),
        OPT_DISCARD("framedrop", framedrop, 0),
        OPT_INTRANGE("threads", threads, 0, 0, 16),
        OPT_FLAG("bitexact", bitexact, 0),
        OPT_FLAG("check-hw-profile", check_hw_profile, 0),
        OPT_KEYVALUELIST("o", avopts, 0),
        {0}
    },
    .size = sizeof(struct vd_lavc_params),
    .defaults = &(const struct vd_lavc_params){
        .show_all = 0,
        .check_hw_profile = 1,
        .skip_loop_filter = AVDISCARD_DEFAULT,
        .skip_idct = AVDISCARD_DEFAULT,
        .skip_frame = AVDISCARD_DEFAULT,
        .framedrop = AVDISCARD_NONREF,
    },
};

const struct vd_lavc_hwdec mp_vd_lavc_vdpau;
const struct vd_lavc_hwdec mp_vd_lavc_vda;
const struct vd_lavc_hwdec mp_vd_lavc_vaapi;
const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy;
const struct vd_lavc_hwdec mp_vd_lavc_dxva2_copy;
const struct vd_lavc_hwdec mp_vd_lavc_rpi;

static const struct vd_lavc_hwdec *const hwdec_list[] = {
#if HAVE_RPI
    &mp_vd_lavc_rpi,
#endif
#if HAVE_VDPAU_HWACCEL
    &mp_vd_lavc_vdpau,
#endif
#if HAVE_VDA_HWACCEL
    &mp_vd_lavc_vda,
#endif
#if HAVE_VAAPI_HWACCEL
    &mp_vd_lavc_vaapi_copy,
    &mp_vd_lavc_vaapi,
#endif
#if HAVE_DXVA2_HWACCEL
    &mp_vd_lavc_dxva2_copy,
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

static bool hwdec_codec_allowed(struct dec_video *vd, const char *codec)
{
    bstr s = bstr0(vd->opts->hwdec_codecs);
    while (s.len) {
        bstr item;
        bstr_split_tok(s, ",", &item, &s);
        if (bstr_equals0(item, "all") || bstr_equals0(item, codec))
            return true;
    }
    return false;
}

static void hwdec_lock(struct lavc_ctx *ctx)
{
    if (ctx->hwdec && ctx->hwdec->lock)
        ctx->hwdec->lock(ctx);
}
static void hwdec_unlock(struct lavc_ctx *ctx)
{
    if (ctx->hwdec && ctx->hwdec->unlock)
        ctx->hwdec->unlock(ctx);
}

// Find the correct profile entry for the current codec and profile.
// Assumes the table has higher profiles first (for each codec).
const struct hwdec_profile_entry *hwdec_find_profile(
    struct lavc_ctx *ctx, const struct hwdec_profile_entry *table)
{
    assert(AV_CODEC_ID_NONE == 0);
    struct vd_lavc_params *lavc_param = ctx->opts->vd_lavc_params;
    enum AVCodecID codec = ctx->avctx->codec_id;
    int profile = ctx->avctx->profile;
    // Assume nobody cares about these aspects of the profile
    if (codec == AV_CODEC_ID_H264) {
        if (profile == FF_PROFILE_H264_CONSTRAINED_BASELINE)
            profile = FF_PROFILE_H264_MAIN;
    }
    for (int n = 0; table[n].av_codec; n++) {
        if (table[n].av_codec == codec) {
            if (table[n].ff_profile == FF_PROFILE_UNKNOWN ||
                profile == FF_PROFILE_UNKNOWN ||
                table[n].ff_profile == profile ||
                !lavc_param->check_hw_profile)
                return &table[n];
        }
    }
    return NULL;
}

// Check codec support, without checking the profile.
bool hwdec_check_codec_support(const char *decoder,
                               const struct hwdec_profile_entry *table)
{
    enum AVCodecID codec = mp_codec_to_av_codec_id(decoder);
    for (int n = 0; table[n].av_codec; n++) {
        if (table[n].av_codec == codec)
            return true;
    }
    return false;
}

int hwdec_get_max_refs(struct lavc_ctx *ctx)
{
    return ctx->avctx->codec_id == AV_CODEC_ID_H264 ? 16 : 2;
}

void hwdec_request_api(struct mp_hwdec_info *info, const char *api_name)
{
    if (info && info->load_api)
        info->load_api(info, api_name);
}

static int hwdec_probe(struct vd_lavc_hwdec *hwdec, struct mp_hwdec_info *info,
                       const char *decoder)
{
    int r = 0;
    if (hwdec->probe)
        r = hwdec->probe(hwdec, info, decoder);
    return r;
}

static struct vd_lavc_hwdec *probe_hwdec(struct dec_video *vd, bool autoprobe,
                                         enum hwdec_type api,
                                         const char *decoder)
{
    struct vd_lavc_hwdec *hwdec = find_hwcodec(api);
    if (!hwdec) {
        MP_VERBOSE(vd, "Requested hardware decoder not compiled.\n");
        return NULL;
    }
    int r = hwdec_probe(hwdec, vd->hwdec_info, decoder);
    if (r == HWDEC_ERR_EMULATED) {
        if (autoprobe)
            return NULL;
        // User requested this explicitly.
        MP_WARN(vd, "Using emulated hardware decoding API.\n");
        r = 0;
    }
    if (r >= 0) {
        return hwdec;
    } else if (r == HWDEC_ERR_NO_CODEC) {
        MP_VERBOSE(vd, "Hardware decoder '%s' not found in "
                   "libavcodec.\n", decoder);
    } else if (r == HWDEC_ERR_NO_CTX && !autoprobe) {
        MP_WARN(vd, "VO does not support requested hardware decoder.\n");
    }
    return NULL;
}

static void uninit(struct dec_video *vd)
{
    uninit_avctx(vd);
    talloc_free(vd->priv);
}

static int init(struct dec_video *vd, const char *decoder)
{
    vd_ffmpeg_ctx *ctx;
    ctx = vd->priv = talloc_zero(NULL, vd_ffmpeg_ctx);
    ctx->log = vd->log;
    ctx->opts = vd->opts;

    ctx->selected_hwdec = vd->opts->hwdec_api;

    if (bstr_endswith0(bstr0(decoder), "_vdpau")) {
        MP_WARN(vd, "VDPAU decoder '%s' was requested. "
                "This way of enabling hardware\ndecoding is not supported "
                "anymore. Use --hwdec=vdpau instead.\nThe --hwdec-codec=... "
                "option can be used to restrict which codecs are\nenabled, "
                "otherwise all hardware decoding is tried for all codecs.\n",
                decoder);
        uninit(vd);
        return 0;
    }

    struct vd_lavc_hwdec *hwdec = NULL;

    if (hwdec_codec_allowed(vd, decoder)) {
        if (vd->opts->hwdec_api == HWDEC_AUTO) {
            for (int n = 0; hwdec_list[n]; n++) {
                hwdec = probe_hwdec(vd, true, hwdec_list[n]->type, decoder);
                if (hwdec)
                    break;
            }
        } else if (vd->opts->hwdec_api != HWDEC_NONE) {
            hwdec = probe_hwdec(vd, false, vd->opts->hwdec_api, decoder);
        }
    } else {
        MP_VERBOSE(vd, "Not trying to use hardware decoding: codec %s is not "
                   "on whitelist, or does not support hardware acceleration.\n",
                   decoder);
    }

    if (hwdec) {
        ctx->software_fallback_decoder = talloc_strdup(ctx, decoder);
        if (hwdec->get_codec)
            decoder = hwdec->get_codec(ctx);
        MP_INFO(vd, "Using hardware decoding.\n");
    } else if (vd->opts->hwdec_api != HWDEC_NONE) {
        MP_INFO(vd, "Using software decoding.\n");
    }

    init_avctx(vd, decoder, hwdec);
    if (!ctx->avctx) {
        if (ctx->software_fallback_decoder) {
            MP_ERR(vd, "Error initializing hardware decoding, "
                   "falling back to software decoding.\n");
            decoder = ctx->software_fallback_decoder;
            ctx->software_fallback_decoder = NULL;
            init_avctx(vd, decoder, NULL);
        }
        if (!ctx->avctx) {
            uninit(vd);
            return 0;
        }
    }

    return 1;
}

static void init_avctx(struct dec_video *vd, const char *decoder,
                       struct vd_lavc_hwdec *hwdec)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    struct vd_lavc_params *lavc_param = vd->opts->vd_lavc_params;
    bool mp_rawvideo = false;
    struct sh_stream *sh = vd->header;

    assert(!ctx->avctx);

    if (strcmp(decoder, "mp-rawvideo") == 0) {
        mp_rawvideo = true;
        decoder = "rawvideo";
    }

    AVCodec *lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec)
        return;

    ctx->hwdec_info = vd->hwdec_info;

    ctx->pix_fmt = AV_PIX_FMT_NONE;
    ctx->hwdec = hwdec;
    ctx->hwdec_fmt = 0;
    ctx->avctx = avcodec_alloc_context3(lavc_codec);
    AVCodecContext *avctx = ctx->avctx;
    if (!ctx->avctx)
        goto error;
    avctx->opaque = vd;
    avctx->codec_type = AVMEDIA_TYPE_VIDEO;
    avctx->codec_id = lavc_codec->id;

    avctx->refcounted_frames = 1;
    ctx->pic = av_frame_alloc();
    if (!ctx->pic)
        goto error;

    if (ctx->hwdec) {
        avctx->thread_count    = 1;
        avctx->get_format      = get_format_hwdec;
        if (ctx->hwdec->allocate_image)
            avctx->get_buffer2 = get_buffer2_hwdec;
        if (ctx->hwdec->init(ctx) < 0)
            goto error;
    } else {
        mp_set_avcodec_threads(vd->log, avctx, lavc_param->threads);
    }

    avctx->flags |= lavc_param->bitexact ? CODEC_FLAG_BITEXACT : 0;
    avctx->flags2 |= lavc_param->fast ? CODEC_FLAG2_FAST : 0;

    if (lavc_param->show_all) {
#ifdef CODEC_FLAG2_SHOW_ALL
        avctx->flags2 |= CODEC_FLAG2_SHOW_ALL; // ffmpeg only?
#endif
#ifdef CODEC_FLAG_OUTPUT_CORRUPT
        avctx->flags |= CODEC_FLAG_OUTPUT_CORRUPT; // added with Libav 10
#endif
    }

    avctx->skip_loop_filter = lavc_param->skip_loop_filter;
    avctx->skip_idct = lavc_param->skip_idct;
    avctx->skip_frame = lavc_param->skip_frame;

    mp_set_avopts(vd->log, avctx, lavc_param->avopts);

    // Do this after the above avopt handling in case it changes values
    ctx->skip_frame = avctx->skip_frame;

    avctx->codec_tag = sh->format;
    avctx->coded_width  = sh->video->disp_w;
    avctx->coded_height = sh->video->disp_h;
    avctx->bits_per_coded_sample = sh->video->bits_per_coded_sample;

    mp_lavc_set_extradata(avctx, sh->video->extradata, sh->video->extradata_len);

    if (mp_rawvideo) {
        avctx->pix_fmt = imgfmt2pixfmt(sh->format);
        avctx->codec_tag = 0;
        if (avctx->pix_fmt == AV_PIX_FMT_NONE && sh->format)
            MP_ERR(vd, "Image format %s not supported by lavc.\n",
                   mp_imgfmt_to_name(sh->format));
    }

    if (sh->lav_headers)
        mp_copy_lav_codec_headers(avctx, sh->lav_headers);

    /* open it */
    if (avcodec_open2(avctx, lavc_codec, NULL) < 0)
        goto error;

    return;

error:
    MP_ERR(vd, "Could not open codec.\n");
    uninit_avctx(vd);
}

static void uninit_avctx(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    AVCodecContext *avctx = ctx->avctx;

    if (avctx) {
        if (avctx->codec && avcodec_close(avctx) < 0)
            MP_ERR(vd, "Could not close codec.\n");

        av_freep(&avctx->extradata);
        av_freep(&avctx->slice_offset);
    }

    if (ctx->hwdec && ctx->hwdec->uninit)
        ctx->hwdec->uninit(ctx);

    av_freep(&ctx->avctx);

    av_frame_free(&ctx->pic);
}

static void update_image_params(struct dec_video *vd, AVFrame *frame,
                                struct mp_image_params *out_params)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    struct MPOpts *opts = ctx->opts;
    int width = frame->width;
    int height = frame->height;
    float aspect = av_q2d(frame->sample_aspect_ratio) * width / height;
    int pix_fmt = frame->format;

    if (pix_fmt != ctx->pix_fmt) {
        ctx->pix_fmt = pix_fmt;
        ctx->best_csp = pixfmt2imgfmt(pix_fmt);
        if (!ctx->best_csp)
            MP_ERR(vd, "lavc pixel format %s not supported.\n",
                   av_get_pix_fmt_name(pix_fmt));
    }

    int d_w, d_h;
    vf_set_dar(&d_w, &d_h, width, height, aspect);

    *out_params = (struct mp_image_params) {
        .imgfmt = ctx->best_csp,
        .w = width,
        .h = height,
        .d_w = d_w,
        .d_h = d_h,
        .colorspace = avcol_spc_to_mp_csp(ctx->avctx->colorspace),
        .colorlevels = avcol_range_to_mp_csp_levels(ctx->avctx->color_range),
        .primaries = avcol_pri_to_mp_csp_prim(ctx->avctx->color_primaries),
        .gamma = avcol_trc_to_mp_csp_trc(ctx->avctx->color_trc),
        .chroma_location =
            avchroma_location_to_mp(ctx->avctx->chroma_sample_location),
        .rotate = vd->header->video->rotate,
        .stereo_in = vd->header->video->stereo_mode,
    };

    if (opts->video_rotate < 0) {
        out_params->rotate = 0;
    } else {
        out_params->rotate = (out_params->rotate + opts->video_rotate) % 360;
    }
    out_params->stereo_out = opts->video_stereo_mode;
}

static enum AVPixelFormat get_format_hwdec(struct AVCodecContext *avctx,
                                           const enum AVPixelFormat *fmt)
{
    struct dec_video *vd = avctx->opaque;
    vd_ffmpeg_ctx *ctx = vd->priv;

    MP_VERBOSE(vd, "Pixel formats supported by decoder:");
    for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; i++)
        MP_VERBOSE(vd, " %s", av_get_pix_fmt_name(fmt[i]));
    MP_VERBOSE(vd, "\n");

    assert(ctx->hwdec);

    if (ctx->hwdec->image_format) {
        for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; i++) {
            if (ctx->hwdec->image_format == pixfmt2imgfmt(fmt[i])) {
                // There could be more reasons for a change, and it's possible
                // that we miss some. (Might also depend on the hwaccel type.)
                bool change =
                    ctx->hwdec_w != avctx->width ||
                    ctx->hwdec_h != avctx->height ||
                    ctx->hwdec_fmt != ctx->hwdec->image_format ||
                    ctx->hwdec_profile != avctx->profile;
                ctx->hwdec_w = avctx->width;
                ctx->hwdec_h = avctx->height;
                ctx->hwdec_fmt = ctx->hwdec->image_format;
                ctx->hwdec_profile = avctx->profile;
                if (ctx->hwdec->init_decoder && change) {
                    if (ctx->hwdec->init_decoder(ctx, ctx->hwdec_fmt,
                                                 ctx->hwdec_w, ctx->hwdec_h) < 0)
                    {
                        ctx->hwdec_fmt = 0;
                        break;
                    }
                }
                return fmt[i];
            }
        }
    }

    return AV_PIX_FMT_NONE;
}

static struct mp_image *get_surface_hwdec(struct dec_video *vd, AVFrame *pic)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

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
    if (!IMGFMT_IS_HWACCEL(imgfmt) || !ctx->hwdec)
        return NULL;

    // Using frame->width/height is bad. For non-mod 16 video (which would
    // require alignment of frame sizes) we want the decoded size, not the
    // aligned size. At least vdpau needs this: the video mixer is created
    // with decoded size, and the video surfaces must have matching size.
    int w = ctx->avctx->width;
    int h = ctx->avctx->height;

    if (ctx->hwdec->init_decoder) {
        if (imgfmt != ctx->hwdec_fmt && w != ctx->hwdec_w && h != ctx->hwdec_h)
            return NULL;
    }

    struct mp_image *mpi = ctx->hwdec->allocate_image(ctx, imgfmt, w, h);

    if (mpi) {
        for (int i = 0; i < 4; i++)
            pic->data[i] = mpi->planes[i];
    }

    return mpi;
}

static void free_mpi(void *opaque, uint8_t *data)
{
    struct mp_image *mpi = opaque;
    talloc_free(mpi);
}

static int get_buffer2_hwdec(AVCodecContext *avctx, AVFrame *pic, int flags)
{
    struct dec_video *vd = avctx->opaque;

    struct mp_image *mpi = get_surface_hwdec(vd, pic);
    if (!mpi)
        return -1;

    pic->buf[0] = av_buffer_create(NULL, 0, free_mpi, mpi, 0);

    return 0;
}

static int decode(struct dec_video *vd, struct demux_packet *packet,
                  int flags, struct mp_image **out_image)
{
    int got_picture = 0;
    int ret;
    vd_ffmpeg_ctx *ctx = vd->priv;
    AVCodecContext *avctx = ctx->avctx;
    struct vd_lavc_params *lavc_param = ctx->opts->vd_lavc_params;
    AVPacket pkt;

    if (flags) {
        // hr-seek framedrop vs. normal framedrop
        avctx->skip_frame = flags == 2 ? AVDISCARD_NONREF : lavc_param->framedrop;
    } else {
        // normal playback
        avctx->skip_frame = ctx->skip_frame;
    }

    mp_set_av_packet(&pkt, packet, NULL);

    hwdec_lock(ctx);
    ret = avcodec_decode_video2(avctx, ctx->pic, &got_picture, &pkt);
    hwdec_unlock(ctx);
    if (ret < 0) {
        MP_WARN(vd, "Error while decoding frame!\n");
        return -1;
    }

    // Skipped frame, or delayed output due to multithreaded decoding.
    if (!got_picture)
        return 0;

    struct mp_image_params params;
    update_image_params(vd, ctx->pic, &params);
    vd->codec_pts = mp_pts_from_av(ctx->pic->pkt_pts, NULL);
    vd->codec_dts = mp_pts_from_av(ctx->pic->pkt_dts, NULL);

    struct mp_image *mpi = mp_image_from_av_frame(ctx->pic);
    av_frame_unref(ctx->pic);
    if (!mpi)
        return 0; // mpi==NULL, or OOM
    assert(mpi->planes[0] || mpi->planes[3]);
    mp_image_set_params(mpi, &params);

    if (ctx->hwdec && ctx->hwdec->process_image)
        mpi = ctx->hwdec->process_image(ctx, mpi);

    *out_image = mp_img_swap_to_native(mpi);
    return 1;
}

static int force_fallback(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    if (ctx->software_fallback_decoder) {
        uninit_avctx(vd);
        MP_ERR(vd, "Error using hardware "
                "decoding, falling back to software decoding.\n");
        const char *decoder = ctx->software_fallback_decoder;
        ctx->software_fallback_decoder = NULL;
        init_avctx(vd, decoder, NULL);
        return ctx->avctx ? CONTROL_OK : CONTROL_ERROR;
    }
    return CONTROL_FALSE;
}

static struct mp_image *decode_with_fallback(struct dec_video *vd,
                                struct demux_packet *packet, int flags)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    if (!ctx->avctx)
        return NULL;

    struct mp_image *mpi = NULL;
    int res = decode(vd, packet, flags, &mpi);
    if (res < 0) {
        // Failed hardware decoding? Try again in software.
        if (force_fallback(vd) == CONTROL_OK)
            decode(vd, packet, flags, &mpi);
    }

    return mpi;
}

static int control(struct dec_video *vd, int cmd, void *arg)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    AVCodecContext *avctx = ctx->avctx;
    switch (cmd) {
    case VDCTRL_RESET:
        avcodec_flush_buffers(avctx);
        return CONTROL_TRUE;
    case VDCTRL_QUERY_UNSEEN_FRAMES:;
        int delay = avctx->has_b_frames;
        assert(delay >= 0);
        if (avctx->active_thread_type & FF_THREAD_FRAME)
            delay += avctx->thread_count - 1;
        *(int *)arg = delay;
        return CONTROL_TRUE;
    case VDCTRL_GET_HWDEC: {
        int hwdec = ctx->selected_hwdec;
        if (!ctx->software_fallback_decoder)
            hwdec = 0;
        *(int *)arg = hwdec;
        return CONTROL_TRUE;
    }
    case VDCTRL_FORCE_HWDEC_FALLBACK:
        return force_fallback(vd);
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
