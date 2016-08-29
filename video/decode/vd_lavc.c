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

#include "mpv_talloc.h"
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
#include "demux/demux.h"
#include "demux/stheader.h"
#include "demux/packet.h"
#include "video/csputils.h"
#include "video/sws_utils.h"

#if HAVE_AVUTIL_MASTERING_METADATA
#include <libavutil/mastering_display_metadata.h>
#endif

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
    int software_fallback;
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
        OPT_INT("threads", threads, M_OPT_MIN, .min = 0),
        OPT_FLAG("bitexact", bitexact, 0),
        OPT_FLAG("check-hw-profile", check_hw_profile, 0),
        OPT_CHOICE_OR_INT("software-fallback", software_fallback, 0, 1, INT_MAX,
                          ({"no", INT_MAX}, {"yes", 1})),
        OPT_KEYVALUELIST("o", avopts, 0),
        {0}
    },
    .size = sizeof(struct vd_lavc_params),
    .defaults = &(const struct vd_lavc_params){
        .show_all = 0,
        .check_hw_profile = 1,
        .software_fallback = 3,
        .skip_loop_filter = AVDISCARD_DEFAULT,
        .skip_idct = AVDISCARD_DEFAULT,
        .skip_frame = AVDISCARD_DEFAULT,
        .framedrop = AVDISCARD_NONREF,
    },
};

extern const struct vd_lavc_hwdec mp_vd_lavc_vdpau;
extern const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox;
extern const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox_copy;
extern const struct vd_lavc_hwdec mp_vd_lavc_vaapi;
extern const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy;
extern const struct vd_lavc_hwdec mp_vd_lavc_dxva2;
extern const struct vd_lavc_hwdec mp_vd_lavc_dxva2_copy;
extern const struct vd_lavc_hwdec mp_vd_lavc_d3d11va;
extern const struct vd_lavc_hwdec mp_vd_lavc_d3d11va_copy;

#if HAVE_RPI
static const struct vd_lavc_hwdec mp_vd_lavc_rpi = {
    .type = HWDEC_RPI,
    .lavc_suffix = "_mmal",
    .image_format = IMGFMT_MMAL,
};
#endif

#if HAVE_ANDROID
static const struct vd_lavc_hwdec mp_vd_lavc_mediacodec = {
    .type = HWDEC_MEDIACODEC,
    .lavc_suffix = "_mediacodec",
    .copying = true,
};
#endif

static const struct vd_lavc_hwdec *const hwdec_list[] = {
#if HAVE_RPI
    &mp_vd_lavc_rpi,
#endif
#if HAVE_VDPAU_HWACCEL
    &mp_vd_lavc_vdpau,
#endif
#if HAVE_VIDEOTOOLBOX_HWACCEL
    &mp_vd_lavc_videotoolbox,
    &mp_vd_lavc_videotoolbox_copy,
#endif
#if HAVE_VAAPI_HWACCEL
    &mp_vd_lavc_vaapi,
    &mp_vd_lavc_vaapi_copy,
#endif
#if HAVE_D3D_HWACCEL
    &mp_vd_lavc_d3d11va,
    &mp_vd_lavc_dxva2,
    &mp_vd_lavc_dxva2_copy,
    &mp_vd_lavc_d3d11va_copy,
#endif
#if HAVE_ANDROID
    &mp_vd_lavc_mediacodec,
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
            if (table[n].ff_profile == profile ||
                !lavc_param->check_hw_profile)
                return &table[n];
        }
    }
    return NULL;
}

// Check codec support, without checking the profile.
bool hwdec_check_codec_support(const char *codec,
                               const struct hwdec_profile_entry *table)
{
    enum AVCodecID codecid = mp_codec_to_av_codec_id(codec);
    for (int n = 0; table[n].av_codec; n++) {
        if (table[n].av_codec == codecid)
            return true;
    }
    return false;
}

int hwdec_get_max_refs(struct lavc_ctx *ctx)
{
    if (ctx->avctx->codec_id == AV_CODEC_ID_H264 ||
        ctx->avctx->codec_id == AV_CODEC_ID_HEVC)
        return 16;
    return 2;
}

// This is intended to return the name of a decoder for a given wrapper API.
// Decoder wrappers are usually added to libavcodec with a specific suffix.
// For example the mmal h264 decoder is named h264_mmal.
// This API would e.g. return h264_mmal for
// hwdec_find_decoder("h264", "_mmal").
// Just concatenating the two names will not always work due to inconsistencies
// (e.g. "mpeg2video" vs. "mpeg2").
const char *hwdec_find_decoder(const char *codec, const char *suffix)
{
    enum AVCodecID codec_id = mp_codec_to_av_codec_id(codec);
    if (codec_id == AV_CODEC_ID_NONE)
        return NULL;
    AVCodec *cur = NULL;
    for (;;) {
        cur = av_codec_next(cur);
        if (!cur)
            break;
        if (cur->id == codec_id && av_codec_is_decoder(cur) &&
            bstr_endswith0(bstr0(cur->name), suffix))
            return cur->name;
    }
    return NULL;
}

// Parallel to hwdec_find_decoder(): return whether a hwdec can use the given
// decoder. This can't be answered accurately; it works for wrapper decoders
// only (like mmal), and for real hwaccels this will always return false.
static bool hwdec_is_wrapper(struct vd_lavc_hwdec *hwdec, const char *decoder)
{
    if (!hwdec->lavc_suffix)
        return false;
    return bstr_endswith0(bstr0(decoder), hwdec->lavc_suffix);
}

static int hwdec_probe(struct dec_video *vd, struct vd_lavc_hwdec *hwdec,
                       const char *codec)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    int r = 0;
    if (hwdec->probe)
        r = hwdec->probe(ctx, hwdec, codec);
    if (r >= 0) {
        if (hwdec->lavc_suffix && !hwdec_find_decoder(codec, hwdec->lavc_suffix))
            return HWDEC_ERR_NO_CODEC;
    }
    return r;
}

static struct vd_lavc_hwdec *probe_hwdec(struct dec_video *vd, bool autoprobe,
                                         enum hwdec_type api,
                                         const char *codec)
{
    MP_VERBOSE(vd, "Probing '%s'...\n", m_opt_choice_str(mp_hwdec_names, api));
    struct vd_lavc_hwdec *hwdec = find_hwcodec(api);
    if (!hwdec) {
        MP_VERBOSE(vd, "Requested hardware decoder not compiled.\n");
        return NULL;
    }
    int r = hwdec_probe(vd, hwdec, codec);
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
        MP_VERBOSE(vd, "Hardware decoder for '%s' with the given API not found "
                       "in libavcodec.\n", codec);
    } else if (r == HWDEC_ERR_NO_CTX && !autoprobe) {
        MP_WARN(vd, "VO does not support requested hardware decoder, or "
                "loading it failed.\n");
    }
    return NULL;
}

static void uninit(struct dec_video *vd)
{
    uninit_avctx(vd);
    talloc_free(vd->priv);
}

static void force_fallback(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    uninit_avctx(vd);
    int lev = ctx->hwdec_notified ? MSGL_WARN : MSGL_V;
    mp_msg(vd->log, lev, "Falling back to software decoding.\n");
    init_avctx(vd, ctx->decoder, NULL);
}

static void reinit(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    const char *decoder = ctx->decoder;
    const char *codec = vd->codec->codec;

    uninit_avctx(vd);

    struct vd_lavc_hwdec *hwdec = NULL;

    if (hwdec_codec_allowed(vd, codec)) {
        int api = vd->opts->hwdec_api;
        if (HWDEC_IS_AUTO(api)) {
            // If a specific decoder is forced, we should try a hwdec method
            // that works with it, instead of simply failing later at runtime.
            // This is good for avoiding trying "normal" hwaccels on wrapper
            // decoders (like vaapi on a mmal decoder). Since libavcodec doesn't
            // tell us which decoder supports which hwaccel methods without
            // actually running it, do it by detecting such wrapper decoders.
            // On the other hand, e.g. "--hwdec=rpi" should always force the
            // wrapper decoder, so be careful not to break this case.
            bool might_be_wrapper = false;
            for (int n = 0; hwdec_list[n]; n++) {
                struct vd_lavc_hwdec *other = (void *)hwdec_list[n];
                if (hwdec_is_wrapper(other, decoder))
                    might_be_wrapper = true;
            }
            for (int n = 0; hwdec_list[n]; n++) {
                hwdec = probe_hwdec(vd, true, hwdec_list[n]->type, codec);
                if (hwdec) {
                    if (might_be_wrapper && !hwdec_is_wrapper(hwdec, decoder)) {
                        MP_VERBOSE(vd, "This hwaccel is not compatible.\n");
                        continue;
                    }
                    if (api == HWDEC_AUTO_COPY && !hwdec->copying) {
                        MP_VERBOSE(vd, "Not using this for auto-copy mode.\n");
                        continue;
                    }
                    break;
                }
            }
        } else if (api != HWDEC_NONE) {
            hwdec = probe_hwdec(vd, false, api, codec);
        }
    } else {
        MP_VERBOSE(vd, "Not trying to use hardware decoding: codec %s is not "
                   "on whitelist, or does not support hardware acceleration.\n",
                   codec);
    }

    if (hwdec) {
        if (hwdec->get_codec)
            decoder = hwdec->get_codec(ctx, decoder);
        if (hwdec->lavc_suffix)
            decoder = hwdec_find_decoder(codec, hwdec->lavc_suffix);
        MP_VERBOSE(vd, "Trying hardware decoding.\n");
    } else {
        MP_VERBOSE(vd, "Using software decoding.\n");
    }

    init_avctx(vd, decoder, hwdec);
    if (!ctx->avctx && hwdec)
        force_fallback(vd);
}

static int init(struct dec_video *vd, const char *decoder)
{
    vd_ffmpeg_ctx *ctx;
    ctx = vd->priv = talloc_zero(NULL, vd_ffmpeg_ctx);
    ctx->log = vd->log;
    ctx->opts = vd->opts;
    ctx->decoder = talloc_strdup(ctx, decoder);
    ctx->hwdec_devs = vd->hwdec_devs;

    reinit(vd);

    if (!ctx->avctx) {
        uninit(vd);
        return 0;
    }
    return 1;
}

static void init_avctx(struct dec_video *vd, const char *decoder,
                       struct vd_lavc_hwdec *hwdec)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    struct vd_lavc_params *lavc_param = vd->opts->vd_lavc_params;
    bool mp_rawvideo = false;
    struct mp_codec_params *c = vd->codec;

    assert(!ctx->avctx);

    if (strcmp(decoder, "mp-rawvideo") == 0) {
        mp_rawvideo = true;
        decoder = "rawvideo";
    }

    AVCodec *lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec)
        return;

    ctx->codec_timebase = mp_get_codec_timebase(vd->codec);

    // This decoder does not read pkt_timebase correctly yet.
    if (strstr(decoder, "_mmal"))
        ctx->codec_timebase = (AVRational){1, 1000000};

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

#if LIBAVCODEC_VERSION_MICRO >= 100
    avctx->pkt_timebase = ctx->codec_timebase;
#endif

    avctx->refcounted_frames = 1;
    ctx->pic = av_frame_alloc();
    if (!ctx->pic)
        goto error;

    if (ctx->hwdec) {
        avctx->thread_count = 1;
        if (ctx->hwdec->image_format)
            avctx->get_format = get_format_hwdec;
        if (ctx->hwdec->allocate_image)
            avctx->get_buffer2 = get_buffer2_hwdec;
        if (ctx->hwdec->init && ctx->hwdec->init(ctx) < 0)
            goto error;
        ctx->max_delay_queue = ctx->hwdec->delay_queue;
    } else {
        mp_set_avcodec_threads(vd->log, avctx, lavc_param->threads);
    }

    avctx->flags |= lavc_param->bitexact ? CODEC_FLAG_BITEXACT : 0;
    avctx->flags2 |= lavc_param->fast ? CODEC_FLAG2_FAST : 0;

    if (lavc_param->show_all)
        avctx->flags |= CODEC_FLAG_OUTPUT_CORRUPT;

    avctx->skip_loop_filter = lavc_param->skip_loop_filter;
    avctx->skip_idct = lavc_param->skip_idct;
    avctx->skip_frame = lavc_param->skip_frame;

    mp_set_avopts(vd->log, avctx, lavc_param->avopts);

    // Do this after the above avopt handling in case it changes values
    ctx->skip_frame = avctx->skip_frame;

    avctx->codec_tag = c->codec_tag;
    avctx->coded_width  = c->disp_w;
    avctx->coded_height = c->disp_h;
    avctx->bits_per_coded_sample = c->bits_per_coded_sample;

    mp_lavc_set_extradata(avctx, c->extradata, c->extradata_size);

    if (mp_rawvideo) {
        avctx->pix_fmt = imgfmt2pixfmt(c->codec_tag);
        avctx->codec_tag = 0;
        if (avctx->pix_fmt == AV_PIX_FMT_NONE && c->codec_tag)
            MP_ERR(vd, "Image format %s not supported by lavc.\n",
                   mp_imgfmt_to_name(c->codec_tag));
    }

    mp_set_lav_codec_headers(avctx, c);

    /* open it */
    if (avcodec_open2(avctx, lavc_codec, NULL) < 0)
        goto error;

    return;

error:
    MP_ERR(vd, "Could not open codec.\n");
    uninit_avctx(vd);
}

static void reset_avctx(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    if (ctx->avctx && avcodec_is_open(ctx->avctx))
        avcodec_flush_buffers(ctx->avctx);
    ctx->flushing = false;
}

static void flush_all(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    for (int n = 0; n < ctx->num_delay_queue; n++)
        talloc_free(ctx->delay_queue[n]);
    ctx->num_delay_queue = 0;

    reset_avctx(vd);
}

static void uninit_avctx(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    flush_all(vd);
    av_frame_free(&ctx->pic);

    if (ctx->avctx) {
        if (avcodec_close(ctx->avctx) < 0)
            MP_ERR(vd, "Could not close codec.\n");
        av_freep(&ctx->avctx->extradata);
    }

    if (ctx->hwdec && ctx->hwdec->uninit)
        ctx->hwdec->uninit(ctx);
    ctx->hwdec = NULL;

    av_freep(&ctx->avctx);

    ctx->hwdec_failed = false;
    ctx->hwdec_fail_count = 0;
    ctx->max_delay_queue = 0;
}

static void update_image_params(struct dec_video *vd, AVFrame *frame,
                                struct mp_image_params *out_params)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    struct MPOpts *opts = ctx->opts;

#if HAVE_AVUTIL_MASTERING_METADATA
    // Get the reference peak (for HDR) if available. This is cached into ctx
    // when it's found, since it's not available on every frame (and seems to
    // be only available for keyframes)
    AVFrameSideData *sd = av_frame_get_side_data(frame,
                          AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
    if (sd) {
        AVMasteringDisplayMetadata *mdm = (AVMasteringDisplayMetadata *)sd->data;
        if (mdm->has_luminance) {
            double peak = av_q2d(mdm->max_luminance);
            if (!isnormal(peak) || peak < 10 || peak > 100000) {
                // Invalid data, ignore it. Sadly necessary
                MP_WARN(vd, "Invalid HDR reference peak in stream: %f\n", peak);
            } else {
                ctx->cached_hdr_peak = peak;
            }
        }
    }
#endif

    *out_params = (struct mp_image_params) {
        .imgfmt = pixfmt2imgfmt(frame->format),
        .w = frame->width,
        .h = frame->height,
        .p_w = frame->sample_aspect_ratio.num,
        .p_h = frame->sample_aspect_ratio.den,
        .color = {
            .space = avcol_spc_to_mp_csp(ctx->avctx->colorspace),
            .levels = avcol_range_to_mp_csp_levels(ctx->avctx->color_range),
            .primaries = avcol_pri_to_mp_csp_prim(ctx->avctx->color_primaries),
            .gamma = avcol_trc_to_mp_csp_trc(ctx->avctx->color_trc),
            .sig_peak = ctx->cached_hdr_peak,
        },
        .chroma_location =
            avchroma_location_to_mp(ctx->avctx->chroma_sample_location),
        .rotate = vd->codec->rotate,
        .stereo_in = vd->codec->stereo_mode,
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

#if HAVE_AVCODEC_PROFILE_NAME
    const char *profile = avcodec_profile_name(avctx->codec_id, avctx->profile);
    MP_VERBOSE(vd, "Codec profile: %s (0x%x)\n", profile ? profile : "unknown",
               avctx->profile);
#endif

    assert(ctx->hwdec);

    ctx->hwdec_request_reinit |= ctx->hwdec_failed;
    ctx->hwdec_failed = false;

    for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; i++) {
        if (ctx->hwdec->image_format == pixfmt2imgfmt(fmt[i])) {
            // There could be more reasons for a change, and it's possible
            // that we miss some. (Might also depend on the hwaccel type.)
            bool change =
                ctx->hwdec_w != avctx->coded_width ||
                ctx->hwdec_h != avctx->coded_height ||
                ctx->hwdec_fmt != ctx->hwdec->image_format ||
                ctx->hwdec_profile != avctx->profile ||
                ctx->hwdec_request_reinit;
            ctx->hwdec_w = avctx->coded_width;
            ctx->hwdec_h = avctx->coded_height;
            ctx->hwdec_fmt = ctx->hwdec->image_format;
            ctx->hwdec_profile = avctx->profile;
            ctx->hwdec_request_reinit = false;
            if (change && ctx->hwdec->init_decoder) {
                if (ctx->hwdec->init_decoder(ctx, ctx->hwdec_w, ctx->hwdec_h) < 0)
                {
                    ctx->hwdec_fmt = 0;
                    break;
                }
            }
            return fmt[i];
        }
    }

    ctx->hwdec_failed = true;
    for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; i++) {
        const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt[i]);
        if (d && !(d->flags & AV_PIX_FMT_FLAG_HWACCEL))
            return fmt[i];
    }
    return AV_PIX_FMT_NONE;
}

static int get_buffer2_hwdec(AVCodecContext *avctx, AVFrame *pic, int flags)
{
    struct dec_video *vd = avctx->opaque;
    vd_ffmpeg_ctx *ctx = vd->priv;

    int imgfmt = pixfmt2imgfmt(pic->format);
    if (!ctx->hwdec || ctx->hwdec_fmt != imgfmt)
        ctx->hwdec_failed = true;

    /* Hardware decoding failed, and we will trigger a proper fallback later
     * when returning from the decode call. (We are forcing complete
     * reinitialization later to reset the thread count properly.)
     */
    if (ctx->hwdec_failed)
        return avcodec_default_get_buffer2(avctx, pic, flags);

    // We expect it to use the exact size used to create the hw decoder in
    // get_format_hwdec(). For cropped video, this is expected to be the
    // uncropped size (usually coded_width/coded_height).
    int w = pic->width;
    int h = pic->height;

    if (imgfmt != ctx->hwdec_fmt && w != ctx->hwdec_w && h != ctx->hwdec_h)
        return -1;

    struct mp_image *mpi = ctx->hwdec->allocate_image(ctx, w, h);
    if (!mpi)
        return -1;

    for (int i = 0; i < 4; i++) {
        pic->data[i] = mpi->planes[i];
        pic->buf[i] = mpi->bufs[i];
        mpi->bufs[i] = NULL;
    }
    talloc_free(mpi);

    return 0;
}

static struct mp_image *read_output(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    if (!ctx->num_delay_queue)
        return NULL;

    struct mp_image *res = ctx->delay_queue[0];
    MP_TARRAY_REMOVE_AT(ctx->delay_queue, ctx->num_delay_queue, 0);

    if (ctx->hwdec && ctx->hwdec->process_image)
        res = ctx->hwdec->process_image(ctx, res);

    return res ? mp_img_swap_to_native(res) : NULL;
}

static void decode(struct dec_video *vd, struct demux_packet *packet,
                   int flags, struct mp_image **out_image)
{
    int got_picture = 0;
    int ret;
    vd_ffmpeg_ctx *ctx = vd->priv;
    AVCodecContext *avctx = ctx->avctx;
    struct vd_lavc_params *opts = ctx->opts->vd_lavc_params;
    bool consumed = false;
    AVPacket pkt;

    if (!avctx)
        return;

    if (flags) {
        // hr-seek framedrop vs. normal framedrop
        avctx->skip_frame = flags == 2 ? AVDISCARD_NONREF : opts->framedrop;
    } else {
        // normal playback
        avctx->skip_frame = ctx->skip_frame;
    }

    mp_set_av_packet(&pkt, packet, &ctx->codec_timebase);
    ctx->flushing |= !pkt.data;

    // Reset decoder if hw state got reset, or new data comes during flushing.
    if (ctx->hwdec_request_reinit || (pkt.data && ctx->flushing))
        reset_avctx(vd);

    hwdec_lock(ctx);
#if HAVE_AVCODEC_NEW_CODEC_API
    ret = avcodec_send_packet(avctx, packet ? &pkt : NULL);
    if (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        if (ret >= 0)
            consumed = true;
        ret = avcodec_receive_frame(avctx, ctx->pic);
        if (ret >= 0)
            got_picture = 1;
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            ret = 0;
    } else {
        consumed = true;
    }
#else
    ret = avcodec_decode_video2(avctx, ctx->pic, &got_picture, &pkt);
    consumed = true;
#endif
    hwdec_unlock(ctx);

    // Reset decoder if it was fully flushed. Caller might send more flush
    // packets, or even new actual packets.
    if (ctx->flushing && (ret < 0 || !got_picture))
        reset_avctx(vd);

    if (ret < 0) {
        MP_WARN(vd, "Error while decoding frame!\n");
        if (ctx->hwdec) {
            ctx->hwdec_fail_count += 1;
            // The FFmpeg VT hwaccel is buggy and can crash after 1 broken frame.
            bool vt = ctx->hwdec && ctx->hwdec->type == HWDEC_VIDEOTOOLBOX;
            if (ctx->hwdec_fail_count >= opts->software_fallback || vt)
                ctx->hwdec_failed = true;
        }
        if (!ctx->hwdec_failed && packet)
            packet->len = 0; // skip failed packet
        return;
    }

    if (ctx->hwdec && ctx->hwdec_failed) {
        av_frame_unref(ctx->pic);
        return;
    }

    if (packet && consumed)
        packet->len = 0;

    // Skipped frame, or delayed output due to multithreaded decoding.
    if (!got_picture) {
        if (!packet)
            *out_image = read_output(vd);
        return;
    }

    ctx->hwdec_fail_count = 0;

    AVFrameSideData *sd = NULL;
    sd = av_frame_get_side_data(ctx->pic, AV_FRAME_DATA_A53_CC);
    if (sd) {
        struct demux_packet *cc = new_demux_packet_from(sd->data, sd->size);
        cc->pts = vd->codec_pts;
        cc->dts = vd->codec_dts;
        demuxer_feed_caption(vd->header, cc);
    }

    struct mp_image *mpi = mp_image_from_av_frame(ctx->pic);
    if (!mpi) {
        av_frame_unref(ctx->pic);
        return;
    }
    assert(mpi->planes[0] || mpi->planes[3]);
    mpi->pts = mp_pts_from_av(ctx->pic->pkt_pts, &ctx->codec_timebase);
    mpi->dts = mp_pts_from_av(ctx->pic->pkt_dts, &ctx->codec_timebase);

    struct mp_image_params params;
    update_image_params(vd, ctx->pic, &params);
    mp_image_set_params(mpi, &params);

    av_frame_unref(ctx->pic);

    MP_TARRAY_APPEND(ctx, ctx->delay_queue, ctx->num_delay_queue, mpi);
    if (ctx->num_delay_queue > ctx->max_delay_queue)
        *out_image = read_output(vd);
}

static struct mp_image *decode_with_fallback(struct dec_video *vd,
                                struct demux_packet *packet, int flags)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    if (!ctx->avctx)
        return NULL;

    struct mp_image *mpi = NULL;
    decode(vd, packet, flags, &mpi);
    if (ctx->hwdec_failed) {
        // Failed hardware decoding? Try again in software.
        force_fallback(vd);
        if (ctx->avctx)
            decode(vd, packet, flags, &mpi);
    }

    if (mpi && !ctx->hwdec_notified && vd->opts->hwdec_api != HWDEC_NONE) {
        if (ctx->hwdec) {
            MP_INFO(vd, "Using hardware decoding (%s).\n",
                    m_opt_choice_str(mp_hwdec_names, ctx->hwdec->type));
        } else {
            MP_INFO(vd, "Using software decoding.\n");
        }
        ctx->hwdec_notified = true;
    }

    return mpi;
}

static int control(struct dec_video *vd, int cmd, void *arg)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    switch (cmd) {
    case VDCTRL_RESET:
        flush_all(vd);
        return CONTROL_TRUE;
    case VDCTRL_GET_BFRAMES: {
        AVCodecContext *avctx = ctx->avctx;
        if (!avctx)
            break;
        if (ctx->hwdec && ctx->hwdec->type == HWDEC_RPI)
            break; // MMAL has arbitrary buffering, thus unknown
        *(int *)arg = avctx->has_b_frames;
        return CONTROL_TRUE;
    }
    case VDCTRL_GET_HWDEC: {
        *(int *)arg = ctx->hwdec ? ctx->hwdec->type : 0;
        return CONTROL_TRUE;
    }
    case VDCTRL_FORCE_HWDEC_FALLBACK:
        if (ctx->hwdec) {
            force_fallback(vd);
            return ctx->avctx ? CONTROL_OK : CONTROL_ERROR;
        }
        return CONTROL_FALSE;
    case VDCTRL_REINIT:
        reinit(vd);
        return CONTROL_TRUE;
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
