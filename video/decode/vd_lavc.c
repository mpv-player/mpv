/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
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
#include "video/out/vo.h"

#include "lavc.h"

#if AVPALETTE_SIZE != MP_PALETTE_SIZE
#error palette too large, adapt video/mp_image.h:MP_PALETTE_SIZE
#endif

#include "options/m_option.h"

static void init_avctx(struct dec_video *vd, const char *decoder,
                       struct vd_lavc_hwdec *hwdec);
static void uninit_avctx(struct dec_video *vd);

static int get_buffer2_direct(AVCodecContext *avctx, AVFrame *pic, int flags);
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
    int dr;
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
        OPT_FLAG("dr", dr, 0),
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

extern const struct vd_lavc_hwdec mp_vd_lavc_mediacodec;
extern const struct vd_lavc_hwdec mp_vd_lavc_mediacodec_copy;
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
static const struct vd_lavc_hwdec mp_vd_lavc_rpi_copy = {
    .type = HWDEC_RPI_COPY,
    .lavc_suffix = "_mmal",
    .copying = true,
};
#endif

static const struct vd_lavc_hwdec mp_vd_lavc_rkmpp = {
    .type = HWDEC_RKMPP,
    .lavc_suffix = "_rkmpp",
    .image_format = IMGFMT_DRMPRIME,
};

#if HAVE_CUDA_HWACCEL
static const struct vd_lavc_hwdec mp_vd_lavc_nvdec = {
    .type = HWDEC_NVDEC,
    .interop_type = HWDEC_CUDA,
    .image_format = IMGFMT_CUDA,
    .generic_hwaccel = true,
    .set_hwframes = true,
};
static const struct vd_lavc_hwdec mp_vd_lavc_nvdec_copy = {
    .type = HWDEC_NVDEC_COPY,
    .create_standalone_dev = true,
    .create_standalone_dev_type = AV_HWDEVICE_TYPE_CUDA,
    .generic_hwaccel = true,
    .set_hwframes = true,
    .copying = true,
};
static const struct vd_lavc_hwdec mp_vd_lavc_cuda = {
    .type = HWDEC_CUDA,
    .image_format = IMGFMT_CUDA,
    .lavc_suffix = "_cuvid",
    .generic_hwaccel = true,
};
static const struct vd_lavc_hwdec mp_vd_lavc_cuda_copy = {
    .type = HWDEC_CUDA_COPY,
    .lavc_suffix = "_cuvid",
    .copying = true,
};
#endif

static const struct vd_lavc_hwdec mp_vd_lavc_crystalhd = {
    .type = HWDEC_CRYSTALHD,
    .lavc_suffix = "_crystalhd",
    .copying = true,
};

#if HAVE_VAAPI
static const struct vd_lavc_hwdec mp_vd_lavc_vaapi = {
    .type = HWDEC_VAAPI,
    .image_format = IMGFMT_VAAPI,
    .generic_hwaccel = true,
    .set_hwframes = true,
};

#include "video/vaapi.h"

static const struct vd_lavc_hwdec mp_vd_lavc_vaapi_copy = {
    .type = HWDEC_VAAPI_COPY,
    .copying = true,
    .image_format = IMGFMT_VAAPI,
    .generic_hwaccel = true,
    .set_hwframes = true,
    .create_dev = va_create_standalone,
};
#endif

#if HAVE_VDPAU
static const struct vd_lavc_hwdec mp_vd_lavc_vdpau = {
    .type = HWDEC_VDPAU,
    .image_format = IMGFMT_VDPAU,
    .generic_hwaccel = true,
    .set_hwframes = true,
};

#include "video/vdpau.h"

static const struct vd_lavc_hwdec mp_vd_lavc_vdpau_copy = {
    .type = HWDEC_VDPAU_COPY,
    .copying = true,
    .image_format = IMGFMT_VDPAU,
    .generic_hwaccel = true,
    .set_hwframes = true,
    .create_dev = vdpau_create_standalone,
};
#endif

#if HAVE_VIDEOTOOLBOX_HWACCEL
static const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox = {
    .type = HWDEC_VIDEOTOOLBOX,
    .image_format = IMGFMT_VIDEOTOOLBOX,
    .generic_hwaccel = true,
    .set_hwframes = true,
};
static const struct vd_lavc_hwdec mp_vd_lavc_videotoolbox_copy = {
    .type = HWDEC_VIDEOTOOLBOX_COPY,
    .copying = true,
    .image_format = IMGFMT_VIDEOTOOLBOX,
    .generic_hwaccel = true,
    .create_standalone_dev = true,
    .create_standalone_dev_type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
    .set_hwframes = true,
    .delay_queue = HWDEC_DELAY_QUEUE_COUNT,
};
#endif

static const struct vd_lavc_hwdec *const hwdec_list[] = {
#if HAVE_RPI
    &mp_vd_lavc_rpi,
    &mp_vd_lavc_rpi_copy,
#endif
#if HAVE_VDPAU
    &mp_vd_lavc_vdpau,
    &mp_vd_lavc_vdpau_copy,
#endif
#if HAVE_VIDEOTOOLBOX_HWACCEL
    &mp_vd_lavc_videotoolbox,
    &mp_vd_lavc_videotoolbox_copy,
#endif
#if HAVE_VAAPI
    &mp_vd_lavc_vaapi,
    &mp_vd_lavc_vaapi_copy,
#endif
#if HAVE_D3D_HWACCEL
    &mp_vd_lavc_d3d11va,

 #if HAVE_D3D9_HWACCEL
    &mp_vd_lavc_dxva2,
    &mp_vd_lavc_dxva2_copy,
 #endif
    &mp_vd_lavc_d3d11va_copy,
#endif
#if HAVE_ANDROID
    &mp_vd_lavc_mediacodec,
    &mp_vd_lavc_mediacodec_copy,
#endif
#if HAVE_CUDA_HWACCEL
    &mp_vd_lavc_nvdec,
    &mp_vd_lavc_nvdec_copy,
    &mp_vd_lavc_cuda,
    &mp_vd_lavc_cuda_copy,
#endif
    &mp_vd_lavc_crystalhd,
    &mp_vd_lavc_rkmpp,
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

// This is intended to return the name of a decoder for a given wrapper API.
// Decoder wrappers are usually added to libavcodec with a specific suffix.
// For example the mmal h264 decoder is named h264_mmal.
// This API would e.g. return h264_mmal for
// hwdec_find_decoder("h264", "_mmal").
// Just concatenating the two names will not always work due to inconsistencies
// (e.g. "mpeg2video" vs. "mpeg2").
static const char *hwdec_find_decoder(const char *codec, const char *suffix)
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

static void standalone_dev_destroy(struct mp_hwdec_ctx *ctx)
{
    av_buffer_unref(&ctx->av_device_ref);
    talloc_free(ctx);
}

static struct mp_hwdec_ctx *hwdec_create_dev(struct dec_video *vd,
                                             struct vd_lavc_hwdec *hwdec,
                                             bool autoprobe)
{
    if (hwdec->create_standalone_dev) {
        struct mp_hwdec_ctx *ctx = talloc_ptrtype(NULL, ctx);
        *ctx = (struct mp_hwdec_ctx) {
            .type = hwdec->type,
            .ctx = NULL,
            .destroy = standalone_dev_destroy,
        };
        if (av_hwdevice_ctx_create(&ctx->av_device_ref,
                        hwdec->create_standalone_dev_type, NULL, NULL, 0) < 0)
        {
            standalone_dev_destroy(ctx);
            ctx = NULL;
        }
        return ctx;
    }
    if (hwdec->create_dev)
        return hwdec->create_dev(vd->global, vd->log, autoprobe);
    if (vd->hwdec_devs) {
        int type = hwdec->interop_type ? hwdec->interop_type : hwdec->type;
        hwdec_devices_request(vd->hwdec_devs, type);
        return hwdec_devices_get(vd->hwdec_devs, type);
    }
    return NULL;
}

static int hwdec_probe(struct dec_video *vd, struct vd_lavc_hwdec *hwdec,
                       const char *codec, bool autoprobe)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    int r = 0;
    if (hwdec->probe)
        r = hwdec->probe(ctx, hwdec, codec);
    if (hwdec->generic_hwaccel) {
        assert(!hwdec->probe && !hwdec->init && !hwdec->init_decoder &&
               !hwdec->uninit);
        struct mp_hwdec_ctx *dev = hwdec_create_dev(vd, hwdec, autoprobe);
        if (!dev)
            return hwdec->copying ? -1 : HWDEC_ERR_NO_CTX;
        if (dev->emulated)
            r = HWDEC_ERR_EMULATED;
        bool owns_hwdec_dev = !!hwdec->create_dev ||
                              hwdec->create_standalone_dev;
        if (owns_hwdec_dev && dev->destroy)
            dev->destroy(dev);
    }
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
        int level = autoprobe ? MSGL_V : MSGL_WARN;
        MP_MSG(vd, level, "Requested hardware decoder not compiled.\n");
        return NULL;
    }
    int r = hwdec_probe(vd, hwdec, codec, autoprobe);
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
    vd_ffmpeg_ctx *ctx = vd->priv;

    uninit_avctx(vd);

    pthread_mutex_destroy(&ctx->dr_lock);
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
        const char *orig_decoder = decoder;
        if (hwdec->lavc_suffix)
            decoder = hwdec_find_decoder(codec, hwdec->lavc_suffix);
        MP_VERBOSE(vd, "Trying hardware decoding.\n");
        if (strcmp(orig_decoder, decoder) != 0)
            MP_VERBOSE(vd, "Using underlying hw-decoder '%s'\n", decoder);
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
    ctx->hwdec_swpool = talloc_steal(ctx, mp_image_pool_new(17));
    ctx->dr_pool = talloc_steal(ctx, mp_image_pool_new(INT_MAX));

    pthread_mutex_init(&ctx->dr_lock, NULL);

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
    struct mp_codec_params *c = vd->codec;

    assert(!ctx->avctx);

    AVCodec *lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec)
        return;

    const AVCodecDescriptor *desc = avcodec_descriptor_get(lavc_codec->id);
    ctx->intra_only = desc && (desc->props & AV_CODEC_PROP_INTRA_ONLY);

    ctx->codec_timebase = mp_get_codec_timebase(vd->codec);

    // This decoder does not read pkt_timebase correctly yet.
    if (strstr(decoder, "_mmal"))
        ctx->codec_timebase = (AVRational){1, 1000000};

    ctx->hwdec = hwdec;
    ctx->hwdec_failed = false;
    ctx->hwdec_request_reinit = false;
    ctx->avctx = avcodec_alloc_context3(lavc_codec);
    AVCodecContext *avctx = ctx->avctx;
    if (!ctx->avctx)
        goto error;
    avctx->codec_type = AVMEDIA_TYPE_VIDEO;
    avctx->codec_id = lavc_codec->id;

#if LIBAVCODEC_VERSION_MICRO >= 100
    avctx->pkt_timebase = ctx->codec_timebase;
#endif

    ctx->pic = av_frame_alloc();
    if (!ctx->pic)
        goto error;

    if (ctx->hwdec) {
        avctx->opaque = vd;
        avctx->thread_count = 1;
        avctx->hwaccel_flags |= AV_HWACCEL_FLAG_IGNORE_LEVEL;
        if (!lavc_param->check_hw_profile)
            avctx->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH;
        if (ctx->hwdec->image_format)
            avctx->get_format = get_format_hwdec;
        if (ctx->hwdec->generic_hwaccel) {
            ctx->hwdec_dev = hwdec_create_dev(vd, ctx->hwdec, false);
            if (!ctx->hwdec_dev)
                goto error;
            ctx->owns_hwdec_dev = !!ctx->hwdec->create_dev ||
                                  ctx->hwdec->create_standalone_dev;
            if (ctx->hwdec_dev->restore_device)
                ctx->hwdec_dev->restore_device(ctx->hwdec_dev);
            if (!ctx->hwdec->set_hwframes)
                avctx->hw_device_ctx = av_buffer_ref(ctx->hwdec_dev->av_device_ref);
        }
        ctx->max_delay_queue = ctx->hwdec->delay_queue;
        ctx->hw_probing = true;
    } else {
        mp_set_avcodec_threads(vd->log, avctx, lavc_param->threads);
    }

    if (!ctx->hwdec && vd->vo && lavc_param->dr) {
        avctx->opaque = vd;
        avctx->get_buffer2 = get_buffer2_direct;
        avctx->thread_safe_callbacks = 1;
    }

    avctx->flags |= lavc_param->bitexact ? AV_CODEC_FLAG_BITEXACT : 0;
    avctx->flags2 |= lavc_param->fast ? AV_CODEC_FLAG2_FAST : 0;

    if (lavc_param->show_all)
        avctx->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;

    avctx->skip_loop_filter = lavc_param->skip_loop_filter;
    avctx->skip_idct = lavc_param->skip_idct;
    avctx->skip_frame = lavc_param->skip_frame;

    mp_set_avopts(vd->log, avctx, lavc_param->avopts);

    // Do this after the above avopt handling in case it changes values
    ctx->skip_frame = avctx->skip_frame;

    if (mp_set_avctx_codec_headers(avctx, c) < 0) {
        MP_ERR(vd, "Could not set codec parameters.\n");
        goto error;
    }

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
    ctx->hwdec_request_reinit = false;
}

static void flush_all(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    for (int n = 0; n < ctx->num_delay_queue; n++)
        talloc_free(ctx->delay_queue[n]);
    ctx->num_delay_queue = 0;

    for (int n = 0; n < ctx->num_sent_packets; n++)
        talloc_free(ctx->sent_packets[n]);
    ctx->num_sent_packets = 0;

    for (int n = 0; n < ctx->num_requeue_packets; n++)
        talloc_free(ctx->requeue_packets[n]);
    ctx->num_requeue_packets = 0;

    reset_avctx(vd);
}

static void uninit_avctx(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    flush_all(vd);
    av_frame_free(&ctx->pic);
    av_buffer_unref(&ctx->cached_hw_frames_ctx);

    if (ctx->hwdec && ctx->hwdec->uninit)
        ctx->hwdec->uninit(ctx);
    ctx->hwdec = NULL;
    assert(ctx->hwdec_priv == NULL);

    avcodec_free_context(&ctx->avctx);

    if (ctx->hwdec_dev && ctx->owns_hwdec_dev && ctx->hwdec_dev->destroy)
        ctx->hwdec_dev->destroy(ctx->hwdec_dev);
    ctx->hwdec_dev = NULL;
    ctx->owns_hwdec_dev = false;

    ctx->hwdec_failed = false;
    ctx->hwdec_fail_count = 0;
    ctx->max_delay_queue = 0;
    ctx->hw_probing = false;
}

static int init_generic_hwaccel(struct dec_video *vd, enum AVPixelFormat hw_fmt)
{
    struct lavc_ctx *ctx = vd->priv;
    struct vd_lavc_hwdec *hwdec = ctx->hwdec;
    AVBufferRef *new_frames_ctx = NULL;

    if (!ctx->hwdec_dev)
        goto error;

    if (!hwdec->set_hwframes)
        return 0;

    if (!ctx->hwdec_dev->av_device_ref) {
        MP_ERR(ctx, "Missing device context.\n");
        goto error;
    }

    if (avcodec_get_hw_frames_parameters(ctx->avctx,
            ctx->hwdec_dev->av_device_ref, hw_fmt, &new_frames_ctx) < 0)
    {
        MP_VERBOSE(ctx, "Hardware decoding of this stream is unsupported?\n");
        goto error;
    }

    AVHWFramesContext *new_fctx = (void *)new_frames_ctx->data;

    if (hwdec->image_format == IMGFMT_VIDEOTOOLBOX)
        new_fctx->sw_format = imgfmt2pixfmt(vd->opts->videotoolbox_format);
    if (vd->opts->hwdec_image_format)
        new_fctx->sw_format = imgfmt2pixfmt(vd->opts->hwdec_image_format);

    // The video output might not support all formats.
    // Note that supported_formats==NULL means any are accepted.
    int *render_formats = ctx->hwdec_dev->supported_formats;
    if (render_formats) {
        int mp_format = pixfmt2imgfmt(new_fctx->sw_format);
        bool found = false;
        for (int n = 0; render_formats[n]; n++) {
            if (render_formats[n] == mp_format) {
                found = true;
                break;
            }
        }
        if (!found) {
            MP_WARN(ctx, "Surface format %s not supported for direct rendering.\n",
                    mp_imgfmt_to_name(mp_format));
            goto error;
        }
    }

    // 1 surface is already included by libavcodec. The field is 0 if the
    // hwaccel supports dynamic surface allocation.
    if (new_fctx->initial_pool_size)
        new_fctx->initial_pool_size += HWDEC_EXTRA_SURFACES - 1;

    if (ctx->hwdec->hwframes_refine)
        ctx->hwdec->hwframes_refine(ctx, new_frames_ctx);

    // We might be able to reuse a previously allocated frame pool.
    if (ctx->cached_hw_frames_ctx) {
        AVHWFramesContext *old_fctx = (void *)ctx->cached_hw_frames_ctx->data;

        if (new_fctx->format            != old_fctx->format ||
            new_fctx->sw_format         != old_fctx->sw_format ||
            new_fctx->width             != old_fctx->width ||
            new_fctx->height            != old_fctx->height ||
            new_fctx->initial_pool_size != old_fctx->initial_pool_size)
            av_buffer_unref(&ctx->cached_hw_frames_ctx);
    }

    if (!ctx->cached_hw_frames_ctx) {
        if (av_hwframe_ctx_init(new_frames_ctx) < 0) {
            MP_ERR(ctx, "Failed to allocate hw frames.\n");
            goto error;
        }

        ctx->cached_hw_frames_ctx = new_frames_ctx;
        new_frames_ctx = NULL;
    }

    ctx->avctx->hw_frames_ctx = av_buffer_ref(ctx->cached_hw_frames_ctx);
    if (!ctx->avctx->hw_frames_ctx)
        goto error;

    av_buffer_unref(&new_frames_ctx);
    return 0;

error:
    av_buffer_unref(&new_frames_ctx);
    av_buffer_unref(&ctx->cached_hw_frames_ctx);
    return -1;
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

    const char *profile = avcodec_profile_name(avctx->codec_id, avctx->profile);
    MP_VERBOSE(vd, "Codec profile: %s (0x%x)\n", profile ? profile : "unknown",
               avctx->profile);

    assert(ctx->hwdec);

    ctx->hwdec_request_reinit |= ctx->hwdec_failed;
    ctx->hwdec_failed = false;

    enum AVPixelFormat select = AV_PIX_FMT_NONE;
    for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; i++) {
        if (ctx->hwdec->image_format == pixfmt2imgfmt(fmt[i])) {
            if (ctx->hwdec->generic_hwaccel) {
                if (init_generic_hwaccel(vd, fmt[i]) < 0)
                    break;
            } else {
                ctx->hwdec_request_reinit = false;
                if (ctx->hwdec->init_decoder && ctx->hwdec->init_decoder(ctx) < 0)
                    break;
            }
            select = fmt[i];
            break;
        }
    }

    if (select == AV_PIX_FMT_NONE) {
        ctx->hwdec_failed = true;
        for (int i = 0; fmt[i] != AV_PIX_FMT_NONE; i++) {
            const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt[i]);
            if (d && !(d->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
                select = fmt[i];
                break;
            }
        }
    }

    const char *name = av_get_pix_fmt_name(select);
    MP_VERBOSE(vd, "Requesting pixfmt '%s' from decoder.\n", name ? name : "-");
    return select;
}

static int get_buffer2_direct(AVCodecContext *avctx, AVFrame *pic, int flags)
{
    struct dec_video *vd = avctx->opaque;
    vd_ffmpeg_ctx *p = vd->priv;

    pthread_mutex_lock(&p->dr_lock);

    int w = pic->width;
    int h = pic->height;
    int linesize_align[AV_NUM_DATA_POINTERS] = {0};
    avcodec_align_dimensions2(avctx, &w, &h, linesize_align);

    // We assume that different alignments are just different power-of-2s.
    // Thus, a higher alignment always satisfies a lower alignment.
    int stride_align = 0;
    for (int n = 0; n < AV_NUM_DATA_POINTERS; n++)
        stride_align = MPMAX(stride_align, linesize_align[n]);

    int imgfmt = pixfmt2imgfmt(pic->format);
    if (!imgfmt)
        goto fallback;

    if (p->dr_failed)
        goto fallback;

    // (For simplicity, we realloc on any parameter change, instead of trying
    // to be clever.)
    if (stride_align != p->dr_stride_align || w != p->dr_w || h != p->dr_h ||
        imgfmt != p->dr_imgfmt)
    {
        mp_image_pool_clear(p->dr_pool);
        p->dr_imgfmt = imgfmt;
        p->dr_w = w;
        p->dr_h = h;
        p->dr_stride_align = stride_align;
        MP_VERBOSE(p, "DR parameter change to %dx%d %s align=%d\n", w, h,
                   mp_imgfmt_to_name(imgfmt), stride_align);
    }

    struct mp_image *img = mp_image_pool_get_no_alloc(p->dr_pool, imgfmt, w, h);
    if (!img) {
        MP_VERBOSE(p, "Allocating new DR image...\n");
        img = vo_get_image(vd->vo, imgfmt, w, h, stride_align);
        if (!img) {
            MP_VERBOSE(p, "...failed..\n");
            goto fallback;
        }

        // Now make the mp_image part of the pool. This requires doing magic to
        // the image, so just add it to the pool and get it back to avoid
        // dealing with magic ourselves. (Normally this never fails.)
        mp_image_pool_add(p->dr_pool, img);
        img = mp_image_pool_get_no_alloc(p->dr_pool, imgfmt, w, h);
        if (!img)
            goto fallback;
    }

    // get_buffer2 callers seem very unappreciative of overwriting pic with a
    // new reference. The AVCodecContext.get_buffer2 comments tell us exactly
    // what we should do, so follow that.
    for (int n = 0; n < 4; n++) {
        pic->data[n] = img->planes[n];
        pic->linesize[n] = img->stride[n];
        pic->buf[n] = img->bufs[n];
        img->bufs[n] = NULL;
    }
    talloc_free(img);

    pthread_mutex_unlock(&p->dr_lock);

    return 0;

fallback:
    if (!p->dr_failed)
        MP_VERBOSE(p, "DR failed - disabling.\n");
    p->dr_failed = true;
    pthread_mutex_unlock(&p->dr_lock);

    return avcodec_default_get_buffer2(avctx, pic, flags);
}

static bool prepare_decoding(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    AVCodecContext *avctx = ctx->avctx;
    struct vd_lavc_params *opts = ctx->opts->vd_lavc_params;

    if (!avctx || ctx->hwdec_failed)
        return false;

    int drop = ctx->framedrop_flags;
    if (drop == 1) {
        avctx->skip_frame = opts->framedrop;    // normal framedrop
    } else if (drop == 2) {
        avctx->skip_frame = AVDISCARD_NONREF;   // hr-seek framedrop
        // Can be much more aggressive for true intra codecs.
        if (ctx->intra_only)
            avctx->skip_frame = AVDISCARD_ALL;
    } else {
        avctx->skip_frame = ctx->skip_frame;    // normal playback
    }

    if (ctx->hwdec_request_reinit)
        reset_avctx(vd);

    return true;
}

static void handle_err(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    struct vd_lavc_params *opts = ctx->opts->vd_lavc_params;

    MP_WARN(vd, "Error while decoding frame!\n");

    if (ctx->hwdec) {
        ctx->hwdec_fail_count += 1;
        if (ctx->hwdec_fail_count >= opts->software_fallback)
            ctx->hwdec_failed = true;
    }
}

static bool do_send_packet(struct dec_video *vd, struct demux_packet *pkt)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    AVCodecContext *avctx = ctx->avctx;

    if (!prepare_decoding(vd))
        return false;

    if (avctx->skip_frame == AVDISCARD_ALL)
        return true;

    AVPacket avpkt;
    mp_set_av_packet(&avpkt, pkt, &ctx->codec_timebase);

    int ret = avcodec_send_packet(avctx, pkt ? &avpkt : NULL);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return false;

    if (ctx->hw_probing && ctx->num_sent_packets < 32) {
        pkt = pkt ? demux_copy_packet(pkt) : NULL;
        MP_TARRAY_APPEND(ctx, ctx->sent_packets, ctx->num_sent_packets, pkt);
    }

    if (ret < 0)
        handle_err(vd);
    return true;
}

static bool send_packet(struct dec_video *vd, struct demux_packet *pkt)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    if (ctx->num_requeue_packets) {
        if (do_send_packet(vd, ctx->requeue_packets[0])) {
            talloc_free(ctx->requeue_packets[0]);
            MP_TARRAY_REMOVE_AT(ctx->requeue_packets, ctx->num_requeue_packets, 0);
        }
        return false;
    }

    return do_send_packet(vd, pkt);
}

// Returns whether decoder is still active (!EOF state).
static bool decode_frame(struct dec_video *vd)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    AVCodecContext *avctx = ctx->avctx;

    if (!prepare_decoding(vd))
        return true;

    int ret = avcodec_receive_frame(avctx, ctx->pic);
    if (ret == AVERROR_EOF) {
        // If flushing was initialized earlier and has ended now, make it start
        // over in case we get new packets at some point in the future.
        reset_avctx(vd);
        return false;
    } else if (ret < 0 && ret != AVERROR(EAGAIN)) {
        handle_err(vd);
    }

    if (!ctx->pic->buf[0])
        return true;

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
        return true;
    }
    assert(mpi->planes[0] || mpi->planes[3]);
    mpi->pts = mp_pts_from_av(ctx->pic->pts, &ctx->codec_timebase);
    mpi->dts = mp_pts_from_av(ctx->pic->pkt_dts, &ctx->codec_timebase);

#if LIBAVCODEC_VERSION_MICRO >= 100
    mpi->pkt_duration =
        mp_pts_from_av(ctx->pic->pkt_duration, &ctx->codec_timebase);
#endif

    av_frame_unref(ctx->pic);

    MP_TARRAY_APPEND(ctx, ctx->delay_queue, ctx->num_delay_queue, mpi);
    return true;
}

static bool receive_frame(struct dec_video *vd, struct mp_image **out_image)
{
    vd_ffmpeg_ctx *ctx = vd->priv;

    assert(!*out_image);

    bool progress = decode_frame(vd);

    if (ctx->hwdec_failed) {
        // Failed hardware decoding? Try again in software.
        struct demux_packet **pkts = ctx->sent_packets;
        int num_pkts = ctx->num_sent_packets;
        ctx->sent_packets = NULL;
        ctx->num_sent_packets = 0;

        force_fallback(vd);

        ctx->requeue_packets = pkts;
        ctx->num_requeue_packets = num_pkts;
    }

    if (!ctx->num_delay_queue)
        return progress;

    if (ctx->num_delay_queue <= ctx->max_delay_queue && progress)
        return true;

    struct mp_image *res = ctx->delay_queue[0];
    MP_TARRAY_REMOVE_AT(ctx->delay_queue, ctx->num_delay_queue, 0);

    if (ctx->hwdec && ctx->hwdec->process_image)
        res = ctx->hwdec->process_image(ctx, res);

    res = res ? mp_img_swap_to_native(res) : NULL;
    if (!res)
        return progress;

    if (ctx->hwdec && ctx->hwdec->copying && (res->fmt.flags & MP_IMGFLAG_HWACCEL))
    {
        struct mp_image *sw = mp_image_hw_download(res, ctx->hwdec_swpool);
        mp_image_unrefp(&res);
        res = sw;
        if (!res) {
            MP_ERR(vd, "Could not copy back hardware decoded frame.\n");
            ctx->hwdec_fail_count = INT_MAX - 1; // force fallback
            handle_err(vd);
            return false;
        }
    }

    if (!ctx->hwdec_notified && vd->opts->hwdec_api != HWDEC_NONE) {
        if (ctx->hwdec) {
            MP_INFO(vd, "Using hardware decoding (%s).\n",
                    m_opt_choice_str(mp_hwdec_names, ctx->hwdec->type));
        } else {
            MP_VERBOSE(vd, "Using software decoding.\n");
        }
        ctx->hwdec_notified = true;
    }

    if (ctx->hw_probing) {
        for (int n = 0; n < ctx->num_sent_packets; n++)
            talloc_free(ctx->sent_packets[n]);
        ctx->num_sent_packets = 0;
        ctx->hw_probing = false;
    }

    *out_image = res;
    return true;
}

static int control(struct dec_video *vd, int cmd, void *arg)
{
    vd_ffmpeg_ctx *ctx = vd->priv;
    switch (cmd) {
    case VDCTRL_RESET:
        flush_all(vd);
        return CONTROL_TRUE;
    case VDCTRL_SET_FRAMEDROP:
        ctx->framedrop_flags = *(int *)arg;
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
}

const struct vd_functions mpcodecs_vd_ffmpeg = {
    .name = "lavc",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .send_packet = send_packet,
    .receive_frame = receive_frame,
};
