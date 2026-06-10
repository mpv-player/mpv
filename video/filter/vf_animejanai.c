/*
 * vf_animejanai: GPU-resident AI upscaling filter (CUDA / TensorRT)
 *
 * The filter keeps frames in GPU memory end to end: IMGFMT_CUDA in, own
 * AVHWFramesContext output pool, IMGFMT_CUDA out. Inference runs in the
 * libaji shim, loaded at runtime across a strict C ABI (see aji.h) so the
 * TensorRT toolchain never links into mpv. With no engine configured the
 * filter degrades to a synchronized GPU-side plane copy (passthrough),
 * which is also the de-risk path used by phase 0 spike A.
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

#include <dlfcn.h>

#include <ffnvcodec/dynlink_loader.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/pixfmt.h>

#include "common/common.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"
#include "refqueue.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

#include "aji.h"

struct opts {
    bool passthrough;
    char *engine;
    char *lib;
};

struct aji_api {
    void *handle;
    aji_ctx *(*create)(const aji_create_params *params);
    int (*infer)(aji_ctx *c, const aji_frame *in, const aji_frame *out,
                 void *cu_stream);
    int (*scale_factor)(aji_ctx *c);
    const char *(*last_error)(aji_ctx *c);
    void (*destroy)(aji_ctx **c);
};

struct priv {
    struct opts *opts;

    CudaFunctions *cu;
    AVBufferRef *av_device_ref;
    CUcontext cuda_ctx;
    CUstream stream;

    AVBufferRef *hw_pool;

    struct aji_api api;
    aji_ctx *aji;
    int aji_fmt;
    int aji_w, aji_h;   // input dims the aji ctx was created for
    int scale;

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

static void aji_log_bridge(void *opaque, int level, const char *msg)
{
    struct mp_filter *vf = opaque;
    // levels follow TRT severity: 0/1 error, 2 warning, 3+ info/verbose
    int mp_level = level <= 1 ? MSGL_ERR : level == 2 ? MSGL_WARN : MSGL_V;
    mp_msg(vf->log, mp_level, "[aji] %s\n", msg);
}

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

    if (p->aji) {
        const aji_frame fin = {
            .width = p->params.w, .height = p->params.h, .format = p->aji_fmt,
            .plane = {in->planes[0], in->planes[1]},
            .stride = {in->stride[0], in->stride[1]},
        };
        const aji_frame fout = {
            .width = p->out_params.w, .height = p->out_params.h,
            .format = p->aji_fmt,
            .plane = {out->planes[0], out->planes[1]},
            .stride = {out->stride[0], out->stride[1]},
        };
        if (p->api.infer(p->aji, &fin, &fout, p->stream) != AJI_OK) {
            MP_ERR(vf, "inference failed: %s\n", p->api.last_error(p->aji));
            goto done;
        }
        // Keep the synchronized model: the input (decoder) surface must not
        // be recycled before our reads complete, and the output is complete
        // when handed downstream.
        if (CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) < 0)
            goto done;
        if (CHECK_CU(p->cu->cuStreamSynchronize(p->stream)) == 0)
            ret = 0;
        CHECK_CU(p->cu->cuCtxPopCurrent(&dummy));
        goto done;
    }

    if (CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) < 0)
        goto done;

    // Passthrough: synchronized copy out of the decoder surface ring.
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

        p->out_params = p->params;

        if (p->api.handle) {
            enum AVPixelFormat sw = imgfmt2pixfmt(p->params.hw_subfmt);
            p->aji_fmt = sw == AV_PIX_FMT_NV12 ? AJI_FMT_NV12 :
                         sw == AV_PIX_FMT_P010 ? AJI_FMT_P010 : 0;
            if (!p->aji_fmt) {
                MP_ERR(vf, "Unsupported sw format %s for inference\n",
                       mp_imgfmt_to_name(p->params.hw_subfmt));
                mp_filter_internal_mark_failed(vf);
                return;
            }

            if (p->aji && (p->aji_w != p->params.w || p->aji_h != p->params.h))
                p->api.destroy(&p->aji);

            if (!p->aji) {
                aji_create_params cp = {
                    .api_version = AJI_API_VERSION,
                    .cuda_context = p->cuda_ctx,
                    .engine_path = p->opts->engine,
                    .max_width = p->params.w,
                    .max_height = p->params.h,
                    .log = aji_log_bridge,
                    .log_opaque = vf,
                };
                p->aji = p->api.create(&cp);
                if (!p->aji) {
                    MP_ERR(vf, "Failed to create inference context for %s\n",
                           p->opts->engine);
                    mp_filter_internal_mark_failed(vf);
                    return;
                }
                p->aji_w = p->params.w;
                p->aji_h = p->params.h;
                p->scale = p->api.scale_factor(p->aji);
                MP_VERBOSE(vf, "Inference ready: %s, scale %dx\n",
                           p->opts->engine, p->scale);
            }

            p->out_params.w = p->params.w * p->scale;
            p->out_params.h = p->params.h * p->scale;
            p->out_params.crop.x0 *= p->scale;
            p->out_params.crop.y0 *= p->scale;
            p->out_params.crop.x1 *= p->scale;
            p->out_params.crop.y1 *= p->scale;
        }

        mp_image_setfmt(&p->layout, p->params.hw_subfmt);
        mp_image_set_size(&p->layout, p->params.w, p->params.h);
        MP_VERBOSE(vf, "Configured: %dx%d subfmt=%s -> %dx%d\n", p->params.w,
                   p->params.h, mp_imgfmt_to_name(p->params.hw_subfmt),
                   p->out_params.w, p->out_params.h);
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
    if (p->aji)
        p->api.destroy(&p->aji);
    if (p->api.handle)
        dlclose(p->api.handle);
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
    p->scale = 1;

    if (cuda_load_functions(&p->cu, NULL) < 0) {
        MP_ERR(f, "Failed to load CUDA driver API\n");
        goto fail;
    }

    if (p->opts->engine && p->opts->engine[0]) {
        // Strict C ABI boundary: the inference backend (TensorRT) is loaded
        // at runtime and never linked. (dlopen here; LoadLibrary on win32
        // once the Windows backend exists.)
        const char *lib = p->opts->lib && p->opts->lib[0] ? p->opts->lib
                                                          : "libaji.so";
        p->api.handle = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
        if (!p->api.handle) {
            MP_ERR(f, "Failed to load inference shim '%s': %s\n", lib,
                   dlerror());
            goto fail;
        }
        p->api.create = dlsym(p->api.handle, "aji_create");
        p->api.infer = dlsym(p->api.handle, "aji_infer");
        p->api.scale_factor = dlsym(p->api.handle, "aji_scale_factor");
        p->api.last_error = dlsym(p->api.handle, "aji_last_error");
        p->api.destroy = dlsym(p->api.handle, "aji_destroy");
        if (!p->api.create || !p->api.infer || !p->api.scale_factor ||
            !p->api.last_error || !p->api.destroy) {
            MP_ERR(f, "Inference shim '%s' is missing aji_* symbols\n", lib);
            goto fail;
        }
        MP_VERBOSE(f, "Loaded inference shim: %s\n", lib);
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
    {"engine", OPT_STRING(engine), .flags = M_OPT_FILE},
    {"lib", OPT_STRING(lib), .flags = M_OPT_FILE},
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
