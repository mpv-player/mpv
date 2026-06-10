/*
 * vf_animejanai: GPU-resident AI upscaling filter (CUDA / TensorRT)
 *
 * Phase 0 spike A: synchronized GPU-side plane copy (passthrough), no
 * inference yet. Proves: IMGFMT_CUDA in -> own AVHWFramesContext pool ->
 * IMGFMT_CUDA out, with no hwdownload/autoconvert inserted by the chain.
 *
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

#include <ffnvcodec/dynlink_loader.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

#include "common/common.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "refqueue.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

struct opts {
    bool passthrough;
};

struct priv {
    struct opts *opts;

    CudaFunctions *cu;
    AVBufferRef *av_device_ref;
    CUcontext cuda_ctx;
    CUstream stream;

    AVBufferRef *hw_pool;

    struct mp_refqueue *queue;
    struct mp_image_params params, out_params;
    // Software layout of params.hw_subfmt, for plane geometry only.
    struct mp_image layout;
};

static int check_cu(struct mp_filter *vf, CUresult err, const char *func)
{
    struct priv *p = vf->priv;
    if (err == CUDA_SUCCESS)
        return 0;
    const char *err_name = NULL;
    const char *err_string = NULL;
    p->cu->cuGetErrorName(err, &err_name);
    p->cu->cuGetErrorString(err, &err_string);
    MP_ERR(vf, "%s failed: %s (%s)\n", func, err_name ? err_name : "unknown",
           err_string ? err_string : "unknown");
    return -1;
}

#define CHECK_CU(x) check_cu(vf, (x), #x)

static void flush_frames(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    mp_refqueue_flush(p->queue);
}

static struct mp_image *alloc_out(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    if (!mp_update_av_hw_frames_pool(&p->hw_pool, p->av_device_ref,
                                     IMGFMT_CUDA, p->out_params.hw_subfmt,
                                     p->out_params.w, p->out_params.h, false))
    {
        MP_ERR(vf, "Failed to create hw pool\n");
        return NULL;
    }

    AVFrame *av_frame = av_frame_alloc();
    MP_HANDLE_OOM(av_frame);
    if (av_hwframe_get_buffer(p->hw_pool, av_frame, 0) < 0) {
        MP_ERR(vf, "Failed to allocate frame from hw pool\n");
        av_frame_free(&av_frame);
        return NULL;
    }

    struct mp_image *img = mp_image_from_av_frame(av_frame);
    av_frame_free(&av_frame);
    if (!img) {
        MP_ERR(vf, "Internal error when converting AVFrame\n");
        return NULL;
    }

    return img;
}

static struct mp_image *render(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    CUcontext dummy;
    int ret = -1;

    struct mp_image *in = mp_refqueue_get(p->queue, 0);
    if (!in)
        return NULL;

    struct mp_image *out = alloc_out(vf);
    if (!out)
        return NULL;

    mp_image_copy_attributes(out, in);
    out->params = p->out_params;
    out->pts = in->pts;

    if (CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) < 0)
        goto done;

    // Synchronized copy out of the decoder surface ring: decouples our
    // frame lifetime from the fixed-size NVDEC pool.
    for (int n = 0; n < p->layout.fmt.num_planes; n++) {
        CUDA_MEMCPY2D cpy = {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .srcDevice     = (CUdeviceptr)(uintptr_t)in->planes[n],
            .srcPitch      = in->stride[n],
            .dstMemoryType = CU_MEMORYTYPE_DEVICE,
            .dstDevice     = (CUdeviceptr)(uintptr_t)out->planes[n],
            .dstPitch      = out->stride[n],
            .WidthInBytes  = mp_image_plane_w(&p->layout, n) *
                             p->layout.fmt.bpp[n] / 8,
            .Height        = mp_image_plane_h(&p->layout, n),
        };
        if (CHECK_CU(p->cu->cuMemcpy2DAsync(&cpy, p->stream)) < 0)
            goto pop;
    }

    if (CHECK_CU(p->cu->cuStreamSynchronize(p->stream)) < 0)
        goto pop;

    ret = 0;
pop:
    CHECK_CU(p->cu->cuCtxPopCurrent(&dummy));
done:
    if (ret < 0)
        TA_FREEP(&out);
    return out;
}

static void vf_animejanai_process(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    struct mp_image *in_fmt = mp_refqueue_execute_reinit(p->queue);
    if (in_fmt) {
        av_buffer_unref(&p->hw_pool);

        p->params = in_fmt->params;
        if (!p->params.hw_subfmt) {
            MP_ERR(vf, "Unknown hw_subfmt\n");
            mp_filter_internal_mark_failed(vf);
            return;
        }

        // Adopt the device (and thus CUcontext) the incoming frames live on.
        if (!in_fmt->hwctx) {
            MP_ERR(vf, "Input frame has no hw frames context\n");
            mp_filter_internal_mark_failed(vf);
            return;
        }
        AVHWFramesContext *fctx = (void *)in_fmt->hwctx->data;
        AVHWDeviceContext *avhwctx = fctx->device_ctx;
        if (avhwctx->type != AV_HWDEVICE_TYPE_CUDA) {
            MP_ERR(vf, "Input frames are not CUDA frames\n");
            mp_filter_internal_mark_failed(vf);
            return;
        }
        av_buffer_unref(&p->av_device_ref);
        p->av_device_ref = av_buffer_ref(fctx->device_ref);
        MP_HANDLE_OOM(p->av_device_ref);
        AVCUDADeviceContext *cudactx = avhwctx->hwctx;
        p->cuda_ctx = cudactx->cuda_ctx;
        p->stream = cudactx->stream;
        // Spike A: passthrough, output geometry == input geometry.
        p->out_params = p->params;

        mp_image_setfmt(&p->layout, p->params.hw_subfmt);
        mp_image_set_size(&p->layout, p->params.w, p->params.h);
        MP_VERBOSE(vf, "Configured: %dx%d subfmt=%s\n", p->params.w,
                   p->params.h, mp_imgfmt_to_name(p->params.hw_subfmt));
    }

    if (!mp_refqueue_can_output(p->queue))
        return;

    mp_refqueue_write_out_pin(p->queue, render(vf));
}

static void uninit(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    flush_frames(vf);
    talloc_free(p->queue);
    av_buffer_unref(&p->hw_pool);
    av_buffer_unref(&p->av_device_ref);
    cuda_free_functions(&p->cu);
}

static const struct mp_filter_info vf_animejanai_filter = {
    .name = "animejanai",
    .process = vf_animejanai_process,
    .reset = flush_frames,
    .destroy = uninit,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_animejanai_create(struct mp_filter *parent,
                                              void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_animejanai_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);
    p->queue = mp_refqueue_alloc(f);

    if (cuda_load_functions(&p->cu, NULL) < 0) {
        MP_ERR(f, "Failed to load CUDA driver API\n");
        goto fail;
    }

    // The CUDA device is adopted from the first input frame's
    // AVHWFramesContext (see process()), so the filter works regardless of
    // whether the VO provides a hwdec device (e.g. with --vo=null).

    mp_refqueue_add_in_format(p->queue, IMGFMT_CUDA, 0);
    mp_refqueue_set_refs(p->queue, 0, 0);
    mp_refqueue_set_mode(p->queue, 0);

    return f;

fail:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct opts
static const m_option_t vf_opts_fields[] = {
    {"passthrough", OPT_BOOL(passthrough)},
    {0}
};

const struct mp_user_filter_entry vf_animejanai = {
    .desc = {
        .description = "AnimeJaNai AI upscaling filter (CUDA)",
        .name = "animejanai",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .passthrough = true,
        },
        .options = vf_opts_fields,
    },
    .create = vf_animejanai_create,
};
