/*
 * vf_animejanai: GPU-resident AI upscaling filter (CUDA / TensorRT)
 *
 * The filter keeps frames in GPU memory end to end: IMGFMT_CUDA in, own
 * AVHWFramesContext output pool, IMGFMT_CUDA out. Inference runs in the
 * libaji shim, loaded at runtime across a strict C ABI (see aji.h) so the
 * TensorRT toolchain never links into mpv.
 *
 * Modes:
 *  - conf=animejanai.conf (+ model-dir/trtexec/slot): full chain selection
 *    per slot and video properties, engines built on first play.
 *  - engine=file.engine: single fixed engine (spike/debug).
 *  - neither: synchronized GPU-side plane copy (passthrough).
 *
 * Runtime: `vf-command animejanai slot <N>` switches the active slot.
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

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <ffnvcodec/dynlink_loader.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/pixfmt.h>

#include "common/common.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"
#include "options/path.h"
#include "refqueue.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

#include "aji.h"

#ifdef _WIN32
#define AJI_DEFAULT_LIB "aji.dll"
#else
#define AJI_DEFAULT_LIB "libaji.so"
#endif

static void *aji_lib_open(const char *path)
{
#ifdef _WIN32
    return (void *)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void *aji_lib_sym(void *handle, const char *name)
{
#ifdef _WIN32
    return (void *)GetProcAddress((HMODULE)handle, name);
#else
    return dlsym(handle, name);
#endif
}

static void aji_lib_close(void *handle)
{
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

static const char *aji_lib_error(void)
{
#ifdef _WIN32
    static __thread char buf[32];
    snprintf(buf, sizeof(buf), "error %lu", (unsigned long)GetLastError());
    return buf;
#else
    return dlerror();
#endif
}

struct opts {
    bool passthrough;
    char *engine;
    char *conf;
    char *model_dir;
    char *trtexec;
    char *trtexec_libdir;
    char *stats;
    char *lib;
    char *rife_model_dir;
    int slot;
    bool skip_seek_pre_target;
};

struct aji_api {
    void *handle;
    aji_ctx *(*create)(const aji_create_params *params);
    int (*set_slot)(aji_ctx *c, int slot);
    int (*configure)(aji_ctx *c, int w, int h, double fps, int *out_w,
                     int *out_h);
    int (*infer)(aji_ctx *c, const aji_frame *in, const aji_frame *out,
                 void *cu_stream);
    int (*rife_factor)(aji_ctx *c, int *num, int *den);
    int (*infer_rife)(aji_ctx *c, const aji_frame *a, const aji_frame *b,
                      double t, const aji_frame *out, void *cu_stream);
    const char *(*current_log)(aji_ctx *c);
    const char *(*last_error)(aji_ctx *c);
    void (*destroy)(aji_ctx **c);
};

struct priv {
    struct opts *opts;

    // Expanded copies of the path options (~~/ etc.). Kept separate from
    // opts: the option machinery owns and frees those strings itself.
    struct {
        char *conf, *model_dir, *trtexec, *trtexec_libdir, *stats, *engine,
             *lib, *rife_model_dir;
    } path;

    CudaFunctions *cu;
    AVBufferRef *av_device_ref;
    CUcontext cuda_ctx;
    CUstream stream;
    CUstream own_stream;

    AVBufferRef *hw_pool;

    struct aji_api api;
    aji_ctx *aji;
    int aji_fmt;
    bool aji_active;     // a chain/engine is configured; else passthrough copy
    bool configured;     // saw at least one reinit
    int cur_slot, pending_slot;

    struct mp_refqueue *queue;
    struct mp_image_params params, out_params;
    double fps;
    // Software layout of params.hw_subfmt, for plane geometry only.
    struct mp_image layout;

    // RIFE: per input frame, (factor-1) interpolated frames are emitted
    // before the upscaled frame itself. The first goes out through the
    // refqueue (consuming the input); the rest wait in outq and are
    // written on subsequent process() calls without consuming input.
    bool rife_on;
    int rife_factor;            // integer output multiplier (num/den)
    struct mp_image *rife_prev; // last emitted upscaled frame (left frame)
    struct mp_image *outq[8];
    int outq_n, outq_pos;
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
    int mp_level = level <= 1 ? MSGL_ERR : level == 2 ? MSGL_WARN : MSGL_V;
    mp_msg(vf->log, mp_level, "[aji] %s\n", msg);
}

static int map_matrix(struct mp_image_params *params)
{
    switch (params->repr.sys) {
    case PL_COLOR_SYSTEM_BT_601:     return AJI_MATRIX_BT601;
    case PL_COLOR_SYSTEM_BT_2020_NC: return AJI_MATRIX_BT2020;
    case PL_COLOR_SYSTEM_BT_709:     return AJI_MATRIX_BT709;
    default:
        // untagged: same SD heuristic as the legacy pipeline
        return params->h < 720 ? AJI_MATRIX_BT601 : AJI_MATRIX_BT709;
    }
}

static int map_range(struct mp_image_params *params)
{
    return params->repr.levels == PL_COLOR_LEVELS_FULL ? AJI_RANGE_FULL
                                                       : AJI_RANGE_LIMITED;
}

static int map_siting(struct mp_image_params *params)
{
    switch (params->chroma_location) {
    case PL_CHROMA_CENTER:   return AJI_SITING_CENTER;
    case PL_CHROMA_TOP_LEFT: return AJI_SITING_TOPLEFT;
    case PL_CHROMA_LEFT:
    default:                 return AJI_SITING_LEFT;
    }
}

static void flush_frames(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    mp_refqueue_flush(p->queue);
    mp_image_unrefp(&p->rife_prev);
    for (int i = 0; i < p->outq_n; i++)
        mp_image_unrefp(&p->outq[p->outq_pos + i]);
    p->outq_n = p->outq_pos = 0;
}

static void write_stats(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    if (!p->path.stats || !p->aji)
        return;
    FILE *f = fopen(p->path.stats, "w");
    if (!f) {
        MP_WARN(vf, "Cannot write stats file %s\n", p->path.stats);
        return;
    }
    fputs(p->api.current_log(p->aji), f);
    fclose(f);
}

// (Re)configure the shim for the current stream params and slot. Updates
// out_params (and thus the output pool geometry). May block on first-play
// engine builds, like the legacy pipeline.
static bool configure_aji(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    p->out_params = p->params;
    p->aji_active = false;
    if (!p->aji)
        return true;

    p->api.set_slot(p->aji, p->pending_slot);
    p->cur_slot = p->pending_slot;

    int ow = 0, oh = 0;
    int ret = p->api.configure(p->aji, p->params.w, p->params.h, p->fps,
                               &ow, &oh);
    if (ret < 0) {
        MP_ERR(vf, "configure failed: %s\n", p->api.last_error(p->aji));
        return false;
    }
    write_stats(vf);

    int rn = 0, rd = 0;
    p->rife_on = false;
    p->rife_factor = 1;
    mp_image_unrefp(&p->rife_prev);  // never interpolate across a reconfigure
    if (p->api.rife_factor(p->aji, &rn, &rd) && rd > 0 && rn > rd) {
        if (rn % rd == 0 && rn / rd <= 8) {
            p->rife_factor = rn / rd;
            p->rife_on = true;
            MP_VERBOSE(vf, "RIFE active: %dx interpolation\n", p->rife_factor);
        } else {
            MP_WARN(vf, "RIFE factor %d/%d is not a supported integer "
                        "multiplier; interpolation disabled\n", rn, rd);
        }
    }

    if (ret == 0) {
        MP_VERBOSE(vf, "No chain active for %dx%d@%.3f slot %d (passthrough)\n",
                   p->params.w, p->params.h, p->fps, p->cur_slot);
        return true;  // no scaling chain; rife (if any) still applies
    }

    p->aji_active = true;
    double sx = (double)ow / p->params.w, sy = (double)oh / p->params.h;
    p->out_params.w = ow;
    p->out_params.h = oh;
    p->out_params.crop.x0 = lrint(p->params.crop.x0 * sx);
    p->out_params.crop.y0 = lrint(p->params.crop.y0 * sy);
    p->out_params.crop.x1 = lrint(p->params.crop.x1 * sx);
    p->out_params.crop.y1 = lrint(p->params.crop.y1 * sy);
    // The shim's RGB->YUV output is always left-sited, matching the
    // reference pipeline (VS/zimg doesn't propagate chroma location from
    // unsubsampled sources); tag the output accordingly.
    p->out_params.chroma_location = PL_CHROMA_LEFT;
    MP_VERBOSE(vf, "Configured slot %d: %dx%d -> %dx%d\n", p->cur_slot,
               p->params.w, p->params.h, ow, oh);
    return true;
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

    if (p->aji_active) {
        const int mat = map_matrix(&p->params);
        const int rng = map_range(&p->params);
        const int sit = map_siting(&p->params);
        const aji_frame fin = {
            .width = p->params.w, .height = p->params.h, .format = p->aji_fmt,
            .matrix = mat, .range = rng, .siting = sit,
            .plane = {in->planes[0], in->planes[1]},
            .stride = {in->stride[0], in->stride[1]},
        };
        const aji_frame fout = {
            .width = p->out_params.w, .height = p->out_params.h,
            .format = p->aji_fmt,
            .matrix = mat, .range = rng, .siting = sit,
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

// Interpolate between two upscaled frames at time point t. Returns the
// interpolated image, a new ref of `a` on a scene change (the reference
// pipeline substitutes the left frame), or NULL on error. pts is left for
// the caller to set.
static struct mp_image *render_interp(struct mp_filter *vf,
                                      struct mp_image *a, struct mp_image *b,
                                      double t)
{
    struct priv *p = vf->priv;
    CUcontext dummy;

    struct mp_image *out = alloc_out(vf);
    if (!out)
        return NULL;
    mp_image_copy_attributes(out, b);

    const int mat = map_matrix(&p->out_params);
    const int rng = map_range(&p->out_params);
    const aji_frame fa = {
        .width = p->out_params.w, .height = p->out_params.h,
        .format = p->aji_fmt, .matrix = mat, .range = rng,
        .siting = AJI_SITING_LEFT,
        .plane = {a->planes[0], a->planes[1]},
        .stride = {a->stride[0], a->stride[1]},
    };
    aji_frame fb = fa, fout = fa;
    fb.plane[0] = b->planes[0];
    fb.plane[1] = b->planes[1];
    fb.stride[0] = b->stride[0];
    fb.stride[1] = b->stride[1];
    fout.plane[0] = out->planes[0];
    fout.plane[1] = out->planes[1];
    fout.stride[0] = out->stride[0];
    fout.stride[1] = out->stride[1];

    int ret = p->api.infer_rife(p->aji, &fa, &fb, t, &fout, p->stream);
    if (ret == AJI_SCENE) {
        talloc_free(out);
        return mp_image_new_ref(a);
    }
    if (ret != AJI_OK) {
        MP_ERR(vf, "interpolation failed: %s\n", p->api.last_error(p->aji));
        talloc_free(out);
        return NULL;
    }
    bool ok = false;
    if (CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) >= 0) {
        ok = CHECK_CU(p->cu->cuStreamSynchronize(p->stream)) >= 0;
        CHECK_CU(p->cu->cuCtxPopCurrent(&dummy));
    }
    if (!ok)
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
        p->fps = in_fmt->nominal_fps;
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
        if (!p->stream) {
            // ffmpeg's CUDA device context usually leaves this NULL (the
            // default stream); TensorRT then adds extra stream syncs per
            // enqueue, and CUDA graph capture is impossible. Use our own.
            // (Format re-adoptions reuse it; the device ctx is stable.)
            if (!p->own_stream &&
                CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) >= 0) {
                CUcontext dummy;
                CHECK_CU(p->cu->cuStreamCreate(&p->own_stream,
                                               CU_STREAM_NON_BLOCKING));
                CHECK_CU(p->cu->cuCtxPopCurrent(&dummy));
            }
            p->stream = p->own_stream;
        }

        if (p->api.handle && !p->aji) {
            // mpv suboption values cannot contain '=', so the option takes a
            // plain lib dir and the env assignment is composed here.
            char *env = p->path.trtexec_libdir
                ? talloc_asprintf(p, "LD_LIBRARY_PATH=%s", p->path.trtexec_libdir)
                : NULL;
            aji_create_params cp = {
                .api_version = AJI_API_VERSION,
                .cuda_context = p->cuda_ctx,
                .conf_path = p->path.conf,
                .model_dir = p->path.model_dir,
                .trtexec = p->path.trtexec,
                .trtexec_env = env,
                .slot = p->opts->slot,
                .rife_model_dir = p->path.rife_model_dir,
                .engine_path = p->path.engine,
                .max_width = p->params.w,
                .max_height = p->params.h,
                .log = aji_log_bridge,
                .log_opaque = vf,
            };
            p->aji = p->api.create(&cp);
            if (!p->aji) {
                MP_ERR(vf, "Failed to create inference context\n");
                mp_filter_internal_mark_failed(vf);
                return;
            }
        }

        if (p->aji) {
            enum AVPixelFormat sw = imgfmt2pixfmt(p->params.hw_subfmt);
            p->aji_fmt = sw == AV_PIX_FMT_NV12 ? AJI_FMT_NV12 :
                         sw == AV_PIX_FMT_P010 ? AJI_FMT_P010 : 0;
            if (!p->aji_fmt) {
                MP_ERR(vf, "Unsupported sw format %s for inference\n",
                       mp_imgfmt_to_name(p->params.hw_subfmt));
                mp_filter_internal_mark_failed(vf);
                return;
            }
        }

        if (!configure_aji(vf)) {
            mp_filter_internal_mark_failed(vf);
            return;
        }
        p->configured = true;

        mp_image_setfmt(&p->layout, p->params.hw_subfmt);
        mp_image_set_size(&p->layout, p->params.w, p->params.h);
        MP_VERBOSE(vf, "Stream: %dx%d@%.3f subfmt=%s -> %dx%d\n", p->params.w,
                   p->params.h, p->fps, mp_imgfmt_to_name(p->params.hw_subfmt),
                   p->out_params.w, p->out_params.h);
    }

    // Runtime slot switch (vf-command): reconfigure on the filter thread.
    if (p->configured && p->aji && p->pending_slot != p->cur_slot) {
        av_buffer_unref(&p->hw_pool);
        if (!configure_aji(vf)) {
            mp_filter_internal_mark_failed(vf);
            return;
        }
    }

    // Extra RIFE outputs first: they go straight to the out pin without
    // consuming input (the refqueue isn't touched while any are pending,
    // which also keeps EOF behind them).
    if (p->outq_n) {
        if (mp_pin_in_needs_data(vf->ppins[1])) {
            struct mp_image *img = p->outq[p->outq_pos];
            p->outq[p->outq_pos] = NULL;
            p->outq_pos++;
            if (--p->outq_n == 0)
                p->outq_pos = 0;
            mp_pin_in_write(vf->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, img));
            mp_filter_internal_mark_progress(vf);
        }
        return;
    }

    if (!mp_refqueue_can_output(p->queue))
        return;

    struct mp_image *out = render(vf);

    if (out && p->rife_on) {
        if (out->nominal_fps > 0)
            out->nominal_fps *= p->rife_factor;
        struct mp_image *first = out;
        if (p->rife_prev && p->rife_prev->pts != MP_NOPTS_VALUE &&
            out->pts != MP_NOPTS_VALUE && out->pts > p->rife_prev->pts) {
            struct mp_image *list[8];
            int n = 0;
            for (int k = 1; k < p->rife_factor && n < 7; k++) {
                double t = (double)k / p->rife_factor;
                struct mp_image *ip =
                    render_interp(vf, p->rife_prev, out, t);
                if (!ip)
                    break;  // degrade to fewer/no interpolated frames
                ip->pts = p->rife_prev->pts +
                          (out->pts - p->rife_prev->pts) * t;
                list[n++] = ip;
            }
            if (n) {
                list[n++] = out;
                first = list[0];
                for (int i = 1; i < n; i++)
                    p->outq[p->outq_n++] = list[i];
                p->outq_pos = 0;
            }
        }
        mp_image_unrefp(&p->rife_prev);
        p->rife_prev = mp_image_new_ref(out);
        mp_refqueue_write_out_pin(p->queue, first);
        return;
    }

    mp_refqueue_write_out_pin(p->queue, out);
}

static bool vf_animejanai_command(struct mp_filter *vf,
                                  struct mp_filter_command *cmd)
{
    struct priv *p = vf->priv;
    if (cmd->type != MP_FILTER_COMMAND_TEXT || !cmd->cmd)
        return false;
    if (strcmp(cmd->cmd, "slot") == 0 && cmd->arg) {
        int slot = atoi(cmd->arg);
        // slot 0 = bypass: the filter stays in the chain but passes frames
        // through, so on/off toggling never rebuilds the chain (a rebuild
        // would race with vf-commands sent right after a profile change).
        if (slot < 0 || (slot == 0 && strcmp(cmd->arg, "0") != 0)) {
            MP_ERR(vf, "Invalid slot '%s'\n", cmd->arg);
            return false;
        }
        p->pending_slot = slot;
        mp_filter_wakeup(vf);
        return true;
    }
    return false;
}

static void uninit(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    flush_frames(vf);
    talloc_free(p->queue);
    if (p->aji)
        p->api.destroy(&p->aji);
    if (p->api.handle)
        aji_lib_close(p->api.handle);
    if (p->own_stream && p->cu &&
        p->cu->cuCtxPushCurrent(p->cuda_ctx) == CUDA_SUCCESS) {
        CUcontext dummy;
        p->cu->cuStreamDestroy(p->own_stream);
        p->cu->cuCtxPopCurrent(&dummy);
    }
    av_buffer_unref(&p->hw_pool);
    av_buffer_unref(&p->av_device_ref);
    cuda_free_functions(&p->cu);
}

static const struct mp_filter_info vf_animejanai_filter = {
    .name = "animejanai",
    .process = vf_animejanai_process,
    .command = vf_animejanai_command,
    .reset = flush_frames,
    .destroy = uninit,
    .priv_size = sizeof(struct priv),
};

// During hr-seek, frames before the target would be upscaled and then
// discarded by the post-filter framedrop; skip inference on them entirely
// (same approach as the vf_vapoursynth fast-seek patch).
static bool drop_pre_seek_target(void *ctx, struct mp_image *mpi)
{
    struct mp_filter *vf = ctx;
    struct priv *p = vf->priv;

    if (!p->opts->skip_seek_pre_target || mpi->pts == MP_NOPTS_VALUE)
        return false;
    struct mp_stream_info *info = mp_filter_find_stream_info(vf);
    double target;
    if (!(info && info->get_hrseek && info->get_hrseek(info, &target)))
        return false;
    if (mpi->pts >= target - 0.005)
        return false;
    MP_DBG(vf, "drop pre-seek-target frame pts=%f target=%f\n", mpi->pts,
           target);
    return true;
}

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
    p->cur_slot = p->pending_slot = p->opts->slot;

    // Expand mpv path shortcuts (~~/ etc.) so portable configs can point at
    // files relative to the config directory (suboptions are not expanded
    // by the option parser itself). Empty options stay NULL.
    struct { char **dst; char *src; } path_opts[] = {
        {&p->path.conf, p->opts->conf},
        {&p->path.model_dir, p->opts->model_dir},
        {&p->path.trtexec, p->opts->trtexec},
        {&p->path.trtexec_libdir, p->opts->trtexec_libdir},
        {&p->path.stats, p->opts->stats},
        {&p->path.engine, p->opts->engine},
        {&p->path.lib, p->opts->lib},
        {&p->path.rife_model_dir, p->opts->rife_model_dir},
    };
    for (int i = 0; i < MP_ARRAY_SIZE(path_opts); i++) {
        if (path_opts[i].src && path_opts[i].src[0])
            *path_opts[i].dst = mp_get_user_path(p, f->global,
                                                 path_opts[i].src);
    }

    if (cuda_load_functions(&p->cu, NULL) < 0) {
        MP_ERR(f, "Failed to load CUDA driver API\n");
        goto fail;
    }

    bool want_aji = p->path.engine || p->path.conf;
    if (want_aji) {
        // Strict C ABI boundary: the inference backend (TensorRT) is loaded
        // at runtime and never linked.
        const char *lib = p->path.lib ? p->path.lib : AJI_DEFAULT_LIB;
        p->api.handle = aji_lib_open(lib);
        if (!p->api.handle) {
            MP_ERR(f, "Failed to load inference shim '%s': %s\n", lib,
                   aji_lib_error());
            goto fail;
        }
        p->api.create = aji_lib_sym(p->api.handle, "aji_create");
        p->api.set_slot = aji_lib_sym(p->api.handle, "aji_set_slot");
        p->api.configure = aji_lib_sym(p->api.handle, "aji_configure");
        p->api.infer = aji_lib_sym(p->api.handle, "aji_infer");
        p->api.rife_factor = aji_lib_sym(p->api.handle, "aji_rife_factor");
        p->api.infer_rife = aji_lib_sym(p->api.handle, "aji_infer_rife");
        p->api.current_log = aji_lib_sym(p->api.handle, "aji_current_log");
        p->api.last_error = aji_lib_sym(p->api.handle, "aji_last_error");
        p->api.destroy = aji_lib_sym(p->api.handle, "aji_destroy");
        if (!p->api.create || !p->api.set_slot || !p->api.configure ||
            !p->api.infer || !p->api.rife_factor || !p->api.infer_rife ||
            !p->api.current_log || !p->api.last_error || !p->api.destroy) {
            MP_ERR(f, "Inference shim '%s' is missing aji_* symbols "
                      "(ABI version mismatch?)\n", lib);
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
    mp_refqueue_set_drop_check(p->queue, drop_pre_seek_target, f);

    return f;

fail:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct opts
static const m_option_t vf_opts_fields[] = {
    {"conf", OPT_STRING(conf), .flags = M_OPT_FILE},
    {"model-dir", OPT_STRING(model_dir), .flags = M_OPT_FILE},
    {"trtexec", OPT_STRING(trtexec), .flags = M_OPT_FILE},
    {"trtexec-libdir", OPT_STRING(trtexec_libdir), .flags = M_OPT_FILE},
    {"slot", OPT_INT(slot), M_RANGE(0, 9999)},
    {"stats", OPT_STRING(stats), .flags = M_OPT_FILE},
    {"engine", OPT_STRING(engine), .flags = M_OPT_FILE},
    {"lib", OPT_STRING(lib), .flags = M_OPT_FILE},
    {"rife-model-dir", OPT_STRING(rife_model_dir), .flags = M_OPT_FILE},
    {"passthrough", OPT_BOOL(passthrough)},
    {"skip-seek-pre-target", OPT_BOOL(skip_seek_pre_target)},
    {0}
};

const struct mp_user_filter_entry vf_animejanai = {
    .desc = {
        .description = "AnimeJaNai AI upscaling filter (CUDA)",
        .name = "animejanai",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .passthrough = true,
            .slot = 1,
            .skip_seek_pre_target = true,
        },
        .options = vf_opts_fields,
    },
    .create = vf_animejanai_create,
};
