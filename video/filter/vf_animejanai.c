/*
 * vf_animejanai: GPU-resident AI upscaling filter (CUDA / D3D11)
 *
 * The filter keeps frames in GPU memory end to end: IMGFMT_CUDA or
 * IMGFMT_D3D11 in, own AVHWFramesContext output pool, same format out.
 * Inference runs in the libaji shim, loaded at runtime across a strict
 * C ABI (see aji.h) so no inference toolchain links into mpv; the shim
 * dispatches on the conf's backend key (TensorRT on CUDA frames,
 * DirectML on D3D11 frames).
 *
 * On the D3D11 path, decoder textures are neither shareable nor
 * implicitly synchronized (see vf_amf.c), so each input frame is staged
 * with a GPU copy into a filter-owned SHARED|NTHANDLE texture; the
 * output pool's textures get the same flags so the shim's D3D12 side
 * can open everything it touches.
 *
 * Modes:
 *  - conf=animejanai.conf (+ model-dir/trtexec/slot): full chain selection
 *    per slot and video properties, engines built on first play.
 *  - engine=file.engine: single fixed engine (spike/debug).
 *  - neither: synchronized GPU-side plane copy (passthrough).
 *
 * Upscale chains run pipelined (queue-depth frames in flight, default 3):
 * aji_infer only submits, completion is gated per frame through the
 * shim's ticket API (aji_flush/aji_wait), and the refqueue's future-ref
 * window provides the input read-ahead. RIFE chains stay synchronous
 * (their scene-change decision is a CPU readback per frame pair).
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

#include "config.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <ffnvcodec/dynlink_loader.h>

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/pixfmt.h>

#if HAVE_D3D11
#include <libavutil/hwcontext_d3d11va.h>
#endif

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
    // With an explicit path, resolve the shim's own dependencies (the
    // TensorRT runtime) from its directory, so the whole inference runtime
    // can live in one self-contained dir instead of next to mpv.exe or on
    // PATH. The search flags need a fully qualified, normalized path
    // (config expansion produces absolute-but-unnormalized ones).
    if (strpbrk(path, "/\\")) {
        char full[MAX_PATH];
        DWORD n = GetFullPathNameA(path, sizeof(full), full, NULL);
        const char *p = (n > 0 && n < sizeof(full)) ? full : path;
        return (void *)LoadLibraryExA(p, NULL,
                                      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                      LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    }
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
    bool output_444;
    int queue_depth;
};

#define MAX_DEPTH 4

struct aji_api {
    void *handle;
    aji_ctx *(*create)(const aji_create_params *params);
    int (*set_slot)(aji_ctx *c, int slot);
    int (*configure)(aji_ctx *c, int w, int h, double fps, int *out_w,
                     int *out_h);
    int (*infer)(aji_ctx *c, const aji_frame *in, const aji_frame *out,
                 void *cu_stream);
    uint64_t (*flush)(aji_ctx *c, void *cu_stream);
    int (*done)(aji_ctx *c, uint64_t ticket);
    int (*wait)(aji_ctx *c, uint64_t ticket);
    int (*rife_factor)(aji_ctx *c, int *num, int *den);
    int (*rife_before_upscale)(aji_ctx *c);  // optional (may be NULL)
    int (*infer_rife)(aji_ctx *c, const aji_frame *a, const aji_frame *b,
                      double t, const aji_frame *out, void *cu_stream);
    int (*poll)(aji_ctx *c);
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
    CUstream decode_stream;          // stream NVDEC decode/copy runs on (may be NULL/default)
    CUevent  decode_evt[MAX_DEPTH];  // per-frame decode->inference ordering events
    int      decode_evt_next;
    bool     decode_evt_ok;

    bool is_d3d11;
#if HAVE_D3D11
    ID3D11Device *d3d_dev;          // borrowed; av_device_ref keeps it alive
    ID3D11DeviceContext *d3d_ctx;
    // Shareable input staging textures, one per pipelined frame: the
    // D3D11 copy into a stage and the shim's D3D12 reads of it are not
    // ordered across APIs, so a stage must not be rewritten while a
    // queued frame still reads it. Slot reuse is safe because a slot
    // comes around again only after its previous frame's ticket was
    // waited at emission.
    ID3D11Texture2D *d3d_stage[MAX_DEPTH];
    int d3d_stage_count, d3d_stage_next;
    int d3d_stage_w, d3d_stage_h;
    DXGI_FORMAT d3d_stage_fmt;
#endif

    AVBufferRef *hw_pool;

    struct aji_api api;
    aji_ctx *aji;
    int aji_fmt;
    int out_fmt;         // output frames: aji_fmt or AJI_FMT_YUV444P16
    bool aji_active;     // a chain/engine is configured; else passthrough copy
    bool configured;     // saw at least one reinit
    int cur_slot, pending_slot;

    struct mp_refqueue *queue;
    struct mp_image_params params, out_params;
    double fps;
    // Software layout of params.hw_subfmt, for plane geometry only.
    struct mp_image layout;

    // Pipelined inference: frames submitted to the GPU but not yet
    // emitted, oldest first. ring[i].src identifies the refqueue frame at
    // relative position i (borrowed pointer, identity only - the
    // refqueue's future-ref window keeps it alive and thus the decoder
    // surface pinned until emission waits the ticket). depth is the
    // active queue depth: 1 (synchronous) for RIFE chains and bypass,
    // else the queue-depth option.
    struct {
        struct mp_image *src;
        struct mp_image *out;
        uint64_t ticket;
    } ring[MAX_DEPTH];
    int ring_n;
    int depth;

    // RIFE: outputs live on a uniform grid of num/den times the input
    // rate (vsmlrt video_player semantics: output j sits at input
    // position j*den/num; integer positions pass the source frame
    // through, fractional ones interpolate at t = frac). Per input pair
    // the due grid points are emitted: the first goes out through the
    // refqueue (consuming the input), the rest wait in outq and are
    // written on subsequent process() calls without consuming input.
    // With fractional factors some source frames are never emitted and
    // only serve as interpolation endpoints.
    bool rife_on;
    int rife_num, rife_den;     // reduced output multiplier num/den
    int rife_acc;               // next grid point's offset into the
                                // current pair, in 1/num units (0, num]
    struct mp_image *rife_prev; // last upscaled frame (left endpoint)
    struct mp_image *outq[8];
    int outq_n, outq_pos;
    // RIFE-first (CUDA only): interpolate the source pair, then upscale every
    // emitted frame. rife_prev then holds the previous *source* frame and
    // src_pool supplies source-resolution buffers for the interpolation temps.
    bool rife_first;
    AVBufferRef *src_pool;
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

// Inference reads the decoder surface on p->stream; decode/copy ran on
// p->decode_stream. When they differ (we made a private non-blocking
// stream), nothing orders them, so a HAGS-scheduled read can race ahead
// of decode and pick up a recycled pool slot's previous frame. Insert an
// explicit dependency: record decode's progress, make inference wait.
static bool order_after_decode(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    if (p->is_d3d11 || p->stream == p->decode_stream || !p->decode_evt_ok)
        return true;                       // already ordered / nothing to do
    CUcontext dummy;
    if (CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) < 0)
        return false;
    CUevent ev = p->decode_evt[p->decode_evt_next];
    p->decode_evt_next = (p->decode_evt_next + 1) % MAX_DEPTH;
    bool ok = CHECK_CU(p->cu->cuEventRecord(ev, p->decode_stream)) >= 0 &&
              CHECK_CU(p->cu->cuStreamWaitEvent(p->stream, ev, 0)) >= 0;
    p->cu->cuCtxPopCurrent(&dummy);
    return ok;
}

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

// Wait until every submitted frame's GPU work completed. The ring entries
// stay queued for emission; tickets complete in order, so waiting the
// newest covers all.
static bool drain_ring(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    if (!p->ring_n)
        return true;
    if (p->api.wait(p->aji, p->ring[p->ring_n - 1].ticket) != AJI_OK) {
        MP_ERR(vf, "pipeline drain failed: %s\n", p->api.last_error(p->aji));
        return false;
    }
    return true;
}

static void clear_ring(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    drain_ring(vf);
    for (int i = 0; i < p->ring_n; i++)
        mp_image_unrefp(&p->ring[i].out);
    p->ring_n = 0;
}

static void flush_frames(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    // GPU reads of the queued input frames must finish before the
    // refqueue drops them
    clear_ring(vf);
    mp_refqueue_flush(p->queue);
    mp_image_unrefp(&p->rife_prev);
    for (int i = 0; i < p->outq_n; i++)
        mp_image_unrefp(&p->outq[p->outq_pos + i]);
    p->outq_n = p->outq_pos = 0;
    // the output grid restarts at the next frame (= a source frame)
    p->rife_acc = p->rife_den;
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

// Pipeline depth for the current configuration. RIFE chains stay
// synchronous (their scene-change decision is a CPU readback per pair),
// as do bypass and passthrough; active upscale chains run queue-depth
// frames deep, with the refqueue's future-ref window supplying the
// read-ahead (depth - 1 buffered future frames).
static void update_depth(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    int depth = MPCLAMP(p->opts->queue_depth, 1, MAX_DEPTH);
    p->depth = (p->aji_active && !p->rife_on) ? depth : 1;
    mp_refqueue_set_refs(p->queue, 0, p->depth - 1);
}

// (Re)configure the shim for the current stream params and slot. Updates
// out_params (and thus the output pool geometry). May block on first-play
// engine builds, like the legacy pipeline.
static bool configure_aji(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    p->out_params = p->params;
    p->aji_active = false;
    if (!p->aji) {
        update_depth(vf);
        return true;
    }

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
    p->rife_first = false;
    p->rife_num = p->rife_den = 1;
    mp_image_unrefp(&p->rife_prev);  // never interpolate across a reconfigure
    if (p->api.rife_factor(p->aji, &rn, &rd) && rd > 0 && rn > rd) {
        int a = rn, b = rd;
        while (b) {  // reduce the fraction
            int t = a % b;
            a = b;
            b = t;
        }
        rn /= a;
        rd /= a;
        if (rn <= 8 * rd) {  // at most 8 outputs per input pair (outq size)
            p->rife_num = rn;
            p->rife_den = rd;
            p->rife_on = true;
            MP_VERBOSE(vf, "RIFE active: %d/%d interpolation\n", rn, rd);
        } else {
            MP_WARN(vf, "RIFE factor %d/%d exceeds the supported 8x output "
                        "multiplication; interpolation disabled\n", rn, rd);
        }
    }
    p->rife_acc = p->rife_den;

    if (ret == 0) {
        MP_VERBOSE(vf, "No chain active for %dx%d@%.3f slot %d (passthrough)\n",
                   p->params.w, p->params.h, p->fps, p->cur_slot);
        // RIFE-only chains interpolate the passthrough copies, which stay
        // in the input format; never inherit a stale out_fmt from a
        // previously configured upscale chain (out_params was just reset
        // to the input geometry above).
        p->out_fmt = p->aji_fmt;
        update_depth(vf);
        return true;  // no scaling chain; rife (if any) still applies
    }

    p->aji_active = true;
    p->out_fmt = p->aji_fmt;
    if (p->opts->output_444 && !p->is_d3d11) {
        // Option forces full-resolution 4:4:4 even from a 4:2:0 source
        // (exceeds the reference pipeline, which always subsampled back to
        // 4:2:0). D3D11/DirectML stays 4:2:0 - DXGI has no planar 16-bit
        // 4:4:4 video format for the pool.
        p->out_fmt = AJI_FMT_YUV444P16;
    }
    if (p->out_fmt == AJI_FMT_YUV444P16) {
        // Either the option forced it, or a 4:4:4 source mirrors straight
        // through to 4:4:4 output. The model emits full-16-bit 4:4:4 planar;
        // tag the pool to match (444 input is CUDA-only, gated at reinit).
        p->out_params.hw_subfmt = pixfmt2imgfmt(AV_PIX_FMT_YUV444P16);
        // out_params was copied from the input; its bit encoding must follow
        // the format change to 16-bit, or the CUDA->Vulkan interop
        // (gpu-api=vulkan) renders the planes with a ~0.5 LSB normalization
        // shift (the d3d11/sw path re-derives this and is unaffected).
        p->out_params.repr.bits = (struct pl_bit_encoding){
            .sample_depth = 16, .color_depth = 16, .bit_shift = 0,
        };
    }
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
    // RIFE-first: interpolate at the source resolution, then upscale every
    // frame. The shim configured RIFE for the matching resolution, so the
    // order here must agree with what aji_rife_before_upscale() reports
    // (TensorRT and DirectML both support it; older shims report 0).
    p->rife_first = p->rife_on && p->aji_active &&
                    p->api.rife_before_upscale &&
                    p->api.rife_before_upscale(p->aji);
    if (p->rife_first)
        MP_VERBOSE(vf, "RIFE-first: interpolating before upscaling\n");
    update_depth(vf);
    MP_VERBOSE(vf, "Configured slot %d: %dx%d -> %dx%d (depth %d)\n",
               p->cur_slot, p->params.w, p->params.h, ow, oh, p->depth);
    return true;
}

#if HAVE_D3D11
// Like mp_update_av_hw_frames_pool for IMGFMT_D3D11, but the textures
// also get the SHARED|NTHANDLE misc flags so the inference shim's D3D12
// device can open them.
static bool update_d3d11_pool(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    if (p->hw_pool) {
        AVHWFramesContext *hw_frames = (void *)p->hw_pool->data;
        if (hw_frames->width != p->out_params.w ||
            hw_frames->height != p->out_params.h ||
            hw_frames->sw_format != imgfmt2pixfmt(p->out_params.hw_subfmt))
            av_buffer_unref(&p->hw_pool);
    }
    if (p->hw_pool)
        return true;

    p->hw_pool = av_hwframe_ctx_alloc(p->av_device_ref);
    if (!p->hw_pool)
        return false;
    AVHWFramesContext *hw_frames = (void *)p->hw_pool->data;
    hw_frames->format = AV_PIX_FMT_D3D11;
    hw_frames->sw_format = imgfmt2pixfmt(p->out_params.hw_subfmt);
    hw_frames->width = p->out_params.w;
    hw_frames->height = p->out_params.h;
    AVD3D11VAFramesContext *d3d_frames = hw_frames->hwctx;
    d3d_frames->BindFlags = D3D11_BIND_RENDER_TARGET |
                            D3D11_BIND_SHADER_RESOURCE;
    d3d_frames->MiscFlags = D3D11_RESOURCE_MISC_SHARED |
                            D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    if (av_hwframe_ctx_init(p->hw_pool) < 0) {
        av_buffer_unref(&p->hw_pool);
        return false;
    }
    return true;
}

// RIFE-first (D3D11): a source-resolution shareable pool for interpolation
// temps (and shareable staged copies of the source frames). Mirrors
// update_d3d11_pool but at the input geometry.
static bool update_d3d11_src_pool(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    if (p->src_pool) {
        AVHWFramesContext *hw_frames = (void *)p->src_pool->data;
        if (hw_frames->width != p->params.w ||
            hw_frames->height != p->params.h ||
            hw_frames->sw_format != imgfmt2pixfmt(p->params.hw_subfmt))
            av_buffer_unref(&p->src_pool);
    }
    if (p->src_pool)
        return true;

    p->src_pool = av_hwframe_ctx_alloc(p->av_device_ref);
    if (!p->src_pool)
        return false;
    AVHWFramesContext *hw_frames = (void *)p->src_pool->data;
    hw_frames->format = AV_PIX_FMT_D3D11;
    hw_frames->sw_format = imgfmt2pixfmt(p->params.hw_subfmt);
    hw_frames->width = p->params.w;
    hw_frames->height = p->params.h;
    AVD3D11VAFramesContext *d3d_frames = hw_frames->hwctx;
    d3d_frames->BindFlags = D3D11_BIND_RENDER_TARGET |
                            D3D11_BIND_SHADER_RESOURCE;
    d3d_frames->MiscFlags = D3D11_RESOURCE_MISC_SHARED |
                            D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
    if (av_hwframe_ctx_init(p->src_pool) < 0) {
        av_buffer_unref(&p->src_pool);
        return false;
    }
    return true;
}
#endif

static struct mp_image *alloc_out(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    bool pool_ok;
    if (p->is_d3d11) {
#if HAVE_D3D11
        pool_ok = update_d3d11_pool(vf);
#else
        pool_ok = false;
#endif
    } else {
        pool_ok = mp_update_av_hw_frames_pool(&p->hw_pool, p->av_device_ref,
                                              IMGFMT_CUDA,
                                              p->out_params.hw_subfmt,
                                              p->out_params.w,
                                              p->out_params.h, false);
    }
    if (!pool_ok) {
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

// Enqueue inference for `in` and append it to the in-flight ring without
// waiting for the GPU. `in` stays alive (and the decoder surface pinned)
// through the refqueue until emission waits the entry's ticket.
static bool submit_frame(struct mp_filter *vf, struct mp_image *in)
{
    struct priv *p = vf->priv;

    mp_assert(p->ring_n < MAX_DEPTH);

    struct mp_image *out = alloc_out(vf);
    if (!out)
        return false;
    mp_image_copy_attributes(out, in);
    out->params = p->out_params;
    out->pts = in->pts;

    const int mat = map_matrix(&p->params);
    const int rng = map_range(&p->params);
    const int sit = map_siting(&p->params);
    bool ok;
    void *stream = NULL;

#if HAVE_D3D11
    if (p->is_d3d11) {
        // Stage the decode slice into a shareable texture first: decoder
        // surfaces are neither shareable nor synchronized for other
        // devices, and the same-device copy orders against decode on the
        // immediate context (vf_amf.c precedent).
        ID3D11Texture2D *stage = p->d3d_stage[p->d3d_stage_next];
        p->d3d_stage_next = (p->d3d_stage_next + 1) % p->d3d_stage_count;
        ID3D11DeviceContext_CopySubresourceRegion(p->d3d_ctx,
            (ID3D11Resource *)stage, 0, 0, 0, 0,
            (ID3D11Resource *)in->planes[0], (UINT)(intptr_t)in->planes[1],
            NULL);
        const aji_frame fin = {
            .width = p->params.w, .height = p->params.h,
            .format = p->aji_fmt,
            .matrix = mat, .range = rng, .siting = sit,
            .plane = {stage, 0},
        };
        const aji_frame fout = {
            .width = p->out_params.w, .height = p->out_params.h,
            .format = p->aji_fmt,
            .matrix = mat, .range = rng, .siting = sit,
            .plane = {out->planes[0], out->planes[1]},
        };
        ok = p->api.infer(p->aji, &fin, &fout, NULL) == AJI_OK;
    } else
#endif
    {
        const aji_frame fin = {
            .width = p->params.w, .height = p->params.h, .format = p->aji_fmt,
            .matrix = mat, .range = rng, .siting = sit,
            // plane[2]/stride[2] carry Cr for 4:4:4 input; ignored by the
            // engine for the 2-plane NV12/P010 formats.
            .plane = {in->planes[0], in->planes[1], in->planes[2]},
            .stride = {in->stride[0], in->stride[1], in->stride[2]},
        };
        const aji_frame fout = {
            .width = p->out_params.w, .height = p->out_params.h,
            .format = p->out_fmt,
            .matrix = mat, .range = rng, .siting = sit,
            .plane = {out->planes[0], out->planes[1], out->planes[2]},
            .stride = {out->stride[0], out->stride[1], out->stride[2]},
        };
        if (!order_after_decode(vf)) {
            talloc_free(out);
            return false;
        }
        stream = p->stream;
        ok = p->api.infer(p->aji, &fin, &fout, p->stream) == AJI_OK;
    }

    uint64_t ticket = ok ? p->api.flush(p->aji, stream) : 0;
    if (!ticket) {
        MP_ERR(vf, "inference failed: %s\n", p->api.last_error(p->aji));
        // a failed call may still have queued partial device work that
        // references `out`; let it finish before the frame is freed
        p->api.wait(p->aji, p->api.flush(p->aji, stream));
        talloc_free(out);
        return false;
    }

    p->ring[p->ring_n].src = in;
    p->ring[p->ring_n].out = out;
    p->ring[p->ring_n].ticket = ticket;
    p->ring_n++;
    return true;
}

// Pop the oldest in-flight frame, waiting until its GPU work (writes to
// the output, reads of the decoder surface) completed.
static struct mp_image *pop_ring(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    mp_assert(p->ring_n > 0);
    struct mp_image *out = p->ring[0].out;
    uint64_t ticket = p->ring[0].ticket;
    p->ring_n--;
    memmove(&p->ring[0], &p->ring[1], p->ring_n * sizeof(p->ring[0]));

    if (p->api.wait(p->aji, ticket) != AJI_OK) {
        MP_ERR(vf, "inference wait failed: %s\n", p->api.last_error(p->aji));
        talloc_free(out);
        return NULL;
    }
    return out;
}

static struct mp_image *render(struct mp_filter *vf)
{
    struct priv *p = vf->priv;
    CUcontext dummy;
    int ret = -1;

    struct mp_image *in = mp_refqueue_get(p->queue, 0);
    if (!in)
        return NULL;

    // Bypass (slot 0 / no matching chain): hand the decoder frame through
    // untouched — exactly as safe as filterless playback. With RIFE
    // active the copy stays: the buffered prev/cur frames (+ outq) would
    // otherwise pin entries of the decoder's fixed surface ring, which is
    // the documented copy-on-arrival rationale.
    if (!p->aji_active && !p->rife_on)
        return mp_image_new_ref(in);

    if (p->aji_active) {
        // Synchronous inference (RIFE chains, queue-depth=1, pipeline
        // fallback): submit and immediately wait.
        if (!submit_frame(vf, in))
            return NULL;
        return pop_ring(vf);
    }

    struct mp_image *out = alloc_out(vf);
    if (!out)
        return NULL;

    mp_image_copy_attributes(out, in);
    out->params = p->out_params;
    out->pts = in->pts;

#if HAVE_D3D11
    if (p->is_d3d11) {
        // Passthrough: same-device copy into the output slice (implicitly
        // synchronized on the immediate context).
        ID3D11DeviceContext_CopySubresourceRegion(p->d3d_ctx,
            (ID3D11Resource *)out->planes[0],
            (UINT)(intptr_t)out->planes[1], 0, 0, 0,
            (ID3D11Resource *)in->planes[0], (UINT)(intptr_t)in->planes[1],
            NULL);
        return out;
    }
#endif

    if (!order_after_decode(vf))
        goto done;

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
        .format = p->out_fmt, .matrix = mat, .range = rng,
        .siting = AJI_SITING_LEFT,
        .plane = {a->planes[0], a->planes[1], a->planes[2]},
        .stride = {a->stride[0], a->stride[1], a->stride[2]},
    };
    aji_frame fb = fa, fout = fa;
    for (int i = 0; i < 3; i++) {
        fb.plane[i] = b->planes[i];
        fb.stride[i] = b->stride[i];
        fout.plane[i] = out->planes[i];
        fout.stride[i] = out->stride[i];
    }

    // b is the just-decoded source frame; order the read after decode (same
    // HAGS race the upscale path guards against).
    if (!order_after_decode(vf)) {
        talloc_free(out);
        return NULL;
    }
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
    bool ok = p->is_d3d11;  // the DirectML shim completes synchronously
    if (!ok && CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) >= 0) {
        ok = CHECK_CU(p->cu->cuStreamSynchronize(p->stream)) >= 0;
        CHECK_CU(p->cu->cuCtxPopCurrent(&dummy));
    }
    if (!ok)
        TA_FREEP(&out);
    return out;
}

// RIFE-first (CUDA): allocate a source-resolution hwframe for an
// interpolation temp (the upscale models then consume it). Mirrors alloc_out
// but at the input geometry and from a dedicated pool.
static struct mp_image *alloc_src(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    bool pool_ok;
    if (p->is_d3d11) {
#if HAVE_D3D11
        pool_ok = update_d3d11_src_pool(vf);
#else
        pool_ok = false;
#endif
    } else {
        pool_ok = mp_update_av_hw_frames_pool(&p->src_pool, p->av_device_ref,
                                              IMGFMT_CUDA, p->params.hw_subfmt,
                                              p->params.w, p->params.h, false);
    }
    if (!pool_ok) {
        MP_ERR(vf, "Failed to create source hw pool\n");
        return NULL;
    }
    AVFrame *av_frame = av_frame_alloc();
    MP_HANDLE_OOM(av_frame);
    if (av_hwframe_get_buffer(p->src_pool, av_frame, 0) < 0) {
        MP_ERR(vf, "Failed to allocate source frame from hw pool\n");
        av_frame_free(&av_frame);
        return NULL;
    }
    struct mp_image *img = mp_image_from_av_frame(av_frame);
    av_frame_free(&av_frame);
    if (!img)
        MP_ERR(vf, "Internal error when converting source AVFrame\n");
    return img;
}

#if HAVE_D3D11
// RIFE-first (D3D11): copy a decoder source texture into a fresh shareable
// source-res texture, so the shim's D3D12 device can open it for interpolation
// (decoder textures are neither shareable nor synchronized). The same staged
// copy serves as a rife endpoint for two pairs and as an upscale input.
static struct mp_image *stage_src_d3d11(struct mp_filter *vf,
                                        struct mp_image *src)
{
    struct priv *p = vf->priv;
    struct mp_image *out = alloc_src(vf);
    if (!out)
        return NULL;
    mp_image_copy_attributes(out, src);
    out->params = p->params;
    ID3D11DeviceContext_CopySubresourceRegion(p->d3d_ctx,
        (ID3D11Resource *)out->planes[0], (UINT)(intptr_t)out->planes[1],
        0, 0, 0,
        (ID3D11Resource *)src->planes[0], (UINT)(intptr_t)src->planes[1],
        NULL);
    return out;
}
#endif

// RIFE-first: interpolate two source-resolution frames at time t into a fresh
// source-resolution frame (the caller upscales it). Returns a new ref of
// `a` on a scene change, NULL on error. pts is left for the caller.
static struct mp_image *interp_source(struct mp_filter *vf,
                                      struct mp_image *a, struct mp_image *b,
                                      double t)
{
    struct priv *p = vf->priv;
    CUcontext dummy;

    struct mp_image *out = alloc_src(vf);
    if (!out)
        return NULL;
    mp_image_copy_attributes(out, b);
    out->params = p->params;

    const int mat = map_matrix(&p->params);
    const int rng = map_range(&p->params);
    const int sit = map_siting(&p->params);
    const aji_frame fa = {
        .width = p->params.w, .height = p->params.h, .format = p->aji_fmt,
        .matrix = mat, .range = rng, .siting = sit,
        .plane = {a->planes[0], a->planes[1], a->planes[2]},
        .stride = {a->stride[0], a->stride[1], a->stride[2]},
    };
    aji_frame fb = fa, fout = fa;
    for (int i = 0; i < 3; i++) {
        fb.plane[i] = b->planes[i];
        fb.stride[i] = b->stride[i];
        fout.plane[i] = out->planes[i];
        fout.stride[i] = out->stride[i];
    }

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
    bool ok = p->is_d3d11;   // the DirectML shim completes synchronously
    if (!ok && CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) >= 0) {
        ok = CHECK_CU(p->cu->cuStreamSynchronize(p->stream)) >= 0;
        CHECK_CU(p->cu->cuCtxPopCurrent(&dummy));
    }
    if (!ok)
        TA_FREEP(&out);
    return out;
}

// RIFE-first: upscale one source-resolution frame synchronously (submit +
// wait), used on the source frame and on each interpolation temp.
static struct mp_image *upscale_image(struct mp_filter *vf,
                                      struct mp_image *in)
{
    if (!submit_frame(vf, in))
        return NULL;
    return pop_ring(vf);
}

static void vf_animejanai_process(struct mp_filter *vf)
{
    struct priv *p = vf->priv;

    struct mp_image *in_fmt = mp_refqueue_execute_reinit(p->queue);
    if (in_fmt) {
        av_buffer_unref(&p->hw_pool);
        av_buffer_unref(&p->src_pool);

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
        if (avhwctx->type == AV_HWDEVICE_TYPE_CUDA && p->cu) {
            p->is_d3d11 = false;
            av_buffer_unref(&p->av_device_ref);
            p->av_device_ref = av_buffer_ref(fctx->device_ref);
            MP_HANDLE_OOM(p->av_device_ref);
            AVCUDADeviceContext *cudactx = avhwctx->hwctx;
            p->cuda_ctx = cudactx->cuda_ctx;
            p->stream = cudactx->stream;
            p->decode_stream = cudactx->stream;   // the real decode stream (may be NULL)
            if (!p->stream) {
                // ffmpeg's CUDA device context usually leaves this NULL (the
                // default stream); TensorRT then adds extra stream syncs per
                // enqueue, and CUDA graph capture is impossible. Use our own.
                // (Format re-adoptions reuse it; the device ctx is stable.)
                if (!p->own_stream &&
                    CHECK_CU(p->cu->cuCtxPushCurrent(p->cuda_ctx)) >= 0) {
                    CUcontext dummy;
                    if (CHECK_CU(p->cu->cuStreamCreate(&p->own_stream,
                                            CU_STREAM_NON_BLOCKING)) >= 0) {
                        // A non-blocking stream does not order against the
                        // default stream where decode/copy runs; the events
                        // give that ordering explicitly (order_after_decode).
                        bool evt_ok = true;
                        for (int i = 0; i < MAX_DEPTH; i++)
                            evt_ok &= CHECK_CU(p->cu->cuEventCreate(
                                &p->decode_evt[i],
                                CU_EVENT_DISABLE_TIMING)) >= 0;
                        if (evt_ok) {
                            p->decode_evt_ok = true;
                        } else {
                            // Without the events the private stream cannot be
                            // ordered after decode. Don't keep it and silently
                            // race: drop it and run inference on the decode/
                            // default stream (implicitly ordered, at the cost
                            // of per-enqueue TRT syncs and no CUDA graphs).
                            MP_WARN(vf, "CUDA event setup failed; falling back "
                                    "to the decode stream (no CUDA graphs)\n");
                            for (int i = 0; i < MAX_DEPTH; i++) {
                                if (p->decode_evt[i])
                                    p->cu->cuEventDestroy(p->decode_evt[i]);
                                p->decode_evt[i] = NULL;
                            }
                            p->cu->cuStreamDestroy(p->own_stream);
                            p->own_stream = NULL;
                        }
                    }
                    CHECK_CU(p->cu->cuCtxPopCurrent(&dummy));
                }
                // own_stream is NULL if creation/event setup failed; then
                // p->stream stays the decode (default) stream and inference
                // is ordered implicitly, never racing.
                p->stream = p->own_stream;
            }
#if HAVE_D3D11
        } else if (avhwctx->type == AV_HWDEVICE_TYPE_D3D11VA) {
            p->is_d3d11 = true;
            av_buffer_unref(&p->av_device_ref);
            p->av_device_ref = av_buffer_ref(fctx->device_ref);
            MP_HANDLE_OOM(p->av_device_ref);
            AVD3D11VADeviceContext *d3dctx = avhwctx->hwctx;
            p->d3d_dev = d3dctx->device;
            p->d3d_ctx = d3dctx->device_context;
            p->stream = NULL;
#endif
        } else {
            MP_ERR(vf, "Input frames are neither CUDA nor D3D11 frames%s\n",
                   avhwctx->type == AV_HWDEVICE_TYPE_CUDA
                       ? " (CUDA driver unavailable)" : "");
            mp_filter_internal_mark_failed(vf);
            return;
        }

        if (p->api.handle && !p->aji) {
            // mpv suboption values cannot contain '=', so the option takes a
            // plain lib dir and the env assignment is composed here.
            char *env = p->path.trtexec_libdir
                ? talloc_asprintf(p, "LD_LIBRARY_PATH=%s", p->path.trtexec_libdir)
                : NULL;
            aji_create_params cp = {
                .api_version = AJI_API_VERSION,
                .cuda_context = p->is_d3d11 ? NULL : p->cuda_ctx,
#if HAVE_D3D11
                .d3d11_device = p->is_d3d11 ? p->d3d_dev : NULL,
#endif
                .conf_path = p->path.conf,
                .model_dir = p->path.model_dir,
                .trtexec = p->path.trtexec,
                .trtexec_env = env,
                .slot = p->opts->slot,
                .rife_model_dir = p->path.rife_model_dir,
                .async_build = 1,
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
            // x2bgr10 (packed 10-bit RGB) is what mpv hwuploads a 4:4:4 source
            // to on D3D11 (no planar 4:4:4 DXGI format); the DirectML backend
            // ingests it as RGB and round-trips it as RGB.
            p->aji_fmt = sw == AV_PIX_FMT_NV12 ? AJI_FMT_NV12 :
                         sw == AV_PIX_FMT_P010 ? AJI_FMT_P010 :
                         sw == AV_PIX_FMT_YUV444P16 ? AJI_FMT_YUV444P16 :
                         sw == AV_PIX_FMT_X2BGR10 ? AJI_FMT_RGB10A2 : 0;
            if (!p->aji_fmt) {
                MP_ERR(vf, "Unsupported sw format %s for inference\n",
                       mp_imgfmt_to_name(p->params.hw_subfmt));
                mp_filter_internal_mark_failed(vf);
                return;
            }
            // 4:4:4 planar ingest is TensorRT/CUDA only (no planar 16-bit
            // 4:4:4 DXGI format); on D3D11 it arrives as x2bgr10 RGB instead.
            if (p->is_d3d11 && p->aji_fmt == AJI_FMT_YUV444P16) {
                MP_ERR(vf, "yuv444p16 input requires the TensorRT/CUDA backend\n");
                mp_filter_internal_mark_failed(vf);
                return;
            }
            // RGB ingest is DirectML/D3D11 only (the CUDA path gets native
            // 4:4:4); reject it cleanly on CUDA rather than mis-decoding.
            if (!p->is_d3d11 && p->aji_fmt == AJI_FMT_RGB10A2) {
                MP_ERR(vf, "x2bgr10 input requires the DirectML backend\n");
                mp_filter_internal_mark_failed(vf);
                return;
            }
        }

#if HAVE_D3D11
        if (p->is_d3d11) {
            const DXGI_FORMAT fmt =
                p->aji_fmt == AJI_FMT_RGB10A2 ? DXGI_FORMAT_R10G10B10A2_UNORM :
                p->aji_fmt == AJI_FMT_P010    ? DXGI_FORMAT_P010 :
                                                DXGI_FORMAT_NV12;
            const int want = MPCLAMP(p->opts->queue_depth, 1, MAX_DEPTH);
            if (p->d3d_stage_count != want || p->d3d_stage_w != p->params.w ||
                p->d3d_stage_h != p->params.h || p->d3d_stage_fmt != fmt) {
                for (int i = 0; i < p->d3d_stage_count; i++)
                    ID3D11Texture2D_Release(p->d3d_stage[i]);
                p->d3d_stage_count = 0;
                p->d3d_stage_next = 0;
                D3D11_TEXTURE2D_DESC desc = {
                    .Width = p->params.w,
                    .Height = p->params.h,
                    .MipLevels = 1,
                    .ArraySize = 1,
                    .Format = fmt,
                    .SampleDesc = { .Count = 1 },
                    .Usage = D3D11_USAGE_DEFAULT,
                    .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                    .MiscFlags = D3D11_RESOURCE_MISC_SHARED |
                                 D3D11_RESOURCE_MISC_SHARED_NTHANDLE,
                };
                for (int i = 0; i < want; i++) {
                    if (FAILED(ID3D11Device_CreateTexture2D(p->d3d_dev,
                                                            &desc, NULL,
                                                            &p->d3d_stage[i]))) {
                        MP_ERR(vf, "Failed to create the shared staging "
                                   "texture\n");
                        mp_filter_internal_mark_failed(vf);
                        return;
                    }
                    p->d3d_stage_count = i + 1;
                }
                p->d3d_stage_w = p->params.w;
                p->d3d_stage_h = p->params.h;
                p->d3d_stage_fmt = fmt;
            }
        }
#endif

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

    // A background engine build finished: reconfigure, and the chain (or
    // RIFE) activates mid-playback. Until then frames pass through.
    if (p->configured && p->aji && p->api.poll(p->aji)) {
        MP_VERBOSE(vf, "background engine build finished; reconfiguring\n");
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

    if (p->rife_first) {
        // RIFE-first: interpolate the source pair, then upscale each emitted
        // frame. Same output-grid cadence as the default path below, but
        // rife_prev holds the previous *source* frame and the heavy upscaler
        // runs per emitted frame instead of RIFE at the output res.
        struct mp_image *src = mp_refqueue_get(p->queue, 0);
        if (!src)
            return;
        const double in_fps = src->nominal_fps;
        // The rife/upscale read this owned source frame: a ref of the decoder
        // frame on CUDA, a shareable staged copy on D3D11 (decoder textures
        // aren't shareable to the shim's D3D12 device).
        struct mp_image *cur_src;
#if HAVE_D3D11
        if (p->is_d3d11)
            cur_src = stage_src_d3d11(vf, src);
        else
#endif
            cur_src = mp_image_new_ref(src);
        if (!cur_src) {
            mp_filter_internal_mark_failed(vf);
            return;
        }
        struct mp_image *list[8];
        int n = 0;
        bool failed = false;
        if (p->rife_prev && p->rife_prev->pts != MP_NOPTS_VALUE &&
            src->pts != MP_NOPTS_VALUE && src->pts > p->rife_prev->pts) {
            while (p->rife_acc <= p->rife_num && n < 8) {
                struct mp_image *frame;
                double pts;
                if (p->rife_acc == p->rife_num) {
                    frame = upscale_image(vf, cur_src);  // source grid point
                    pts = src->pts;
                } else {
                    double t = (double)p->rife_acc / p->rife_num;
                    struct mp_image *tmp =
                        interp_source(vf, p->rife_prev, cur_src, t);
                    frame = tmp ? upscale_image(vf, tmp) : NULL;
                    mp_image_unrefp(&tmp);
                    pts = p->rife_prev->pts +
                          (src->pts - p->rife_prev->pts) * t;
                }
                if (!frame) {
                    failed = true;
                    break;
                }
                frame->pts = pts;
                if (in_fps > 0)
                    frame->nominal_fps = in_fps * p->rife_num / p->rife_den;
                MP_DBG(vf, "rife-first: %s pts=%f\n",
                       p->rife_acc == p->rife_num ? "source" : "interp", pts);
                list[n++] = frame;
                p->rife_acc += p->rife_den;
            }
            if (failed || !n) {
                for (int i = 0; i < n; i++)
                    mp_image_unrefp(&list[i]);
                n = 0;
            } else {
                p->rife_acc -= p->rife_num;  // rebase into the next pair
            }
        }
        if (!n) {
            // fresh chain (start/seek), invalid pts, or a failed interp: emit
            // the upscaled source as the grid origin and resync.
            p->rife_acc = p->rife_den;
            struct mp_image *u = upscale_image(vf, cur_src);
            if (!u) {
                mp_image_unrefp(&cur_src);
                mp_filter_internal_mark_failed(vf);
                return;
            }
            u->pts = src->pts;
            if (in_fps > 0)
                u->nominal_fps = in_fps * p->rife_num / p->rife_den;
            list[n++] = u;
        }
        mp_image_unrefp(&p->rife_prev);
        p->rife_prev = cur_src;  // previous *source* endpoint (owns the ref)
        for (int i = 1; i < n; i++)
            p->outq[p->outq_n++] = list[i];
        p->outq_pos = 0;
        mp_refqueue_write_out_pin(p->queue, list[0]);
        return;
    }

    // Pipelined inference: submit the current frame plus the refqueue's
    // buffered future frames before waiting on the oldest, so the GPU
    // already works on frame N+1 (and N+2, ...) while N's wait blocks.
    // Stream/queue ordering in the shim makes overlapping submissions
    // safe; the refqueue window keeps each input alive until its
    // emission below.
    if (p->aji_active && !p->rife_on && p->depth > 1) {
        for (int rel = p->ring_n; p->ring_n < p->depth; rel++) {
            struct mp_image *src = mp_refqueue_get(p->queue, rel);
            if (!src)
                break;  // EOF tail or fewer futures buffered yet
            if (!submit_frame(vf, src))
                break;  // degrade; the current frame falls back below
        }
    }

    struct mp_image *out;
    if (p->ring_n && p->ring[0].src == mp_refqueue_get(p->queue, 0)) {
        // also drains leftovers after a mid-flight reconfigure (their
        // outputs are valid frames of the pre-reconfigure chain)
        out = pop_ring(vf);
    } else {
        if (p->ring_n) {
            MP_WARN(vf, "pipeline lost sync; rendering synchronously\n");
            clear_ring(vf);
        }
        out = render(vf);
    }

    if (out && p->rife_on) {
        if (out->nominal_fps > 0)
            out->nominal_fps = out->nominal_fps * p->rife_num / p->rife_den;
        struct mp_image *first = out;
        bool emitted_cur = true;
        if (p->rife_prev && p->rife_prev->pts != MP_NOPTS_VALUE &&
            out->pts != MP_NOPTS_VALUE && out->pts > p->rife_prev->pts) {
            // Emit the output-grid points due within this input pair:
            // offsets acc, acc+den, ... (in 1/num units) while <= num.
            // An offset of exactly num is the right source frame itself;
            // with fractional factors some pairs end on an interpolation
            // and the source frame is never emitted.
            struct mp_image *list[8];
            int n = 0;
            bool failed = false;
            emitted_cur = false;
            while (p->rife_acc <= p->rife_num && n < 8) {
                if (p->rife_acc == p->rife_num) {
                    list[n++] = out;
                    emitted_cur = true;
                    MP_DBG(vf, "rife: source frame pts=%f\n", out->pts);
                } else {
                    double t = (double)p->rife_acc / p->rife_num;
                    struct mp_image *ip =
                        render_interp(vf, p->rife_prev, out, t);
                    if (!ip) {
                        failed = true;  // degrade to fewer frames
                        break;
                    }
                    ip->pts = p->rife_prev->pts +
                              (out->pts - p->rife_prev->pts) * t;
                    MP_DBG(vf, "rife: interp t=%f pts=%f\n", t, ip->pts);
                    list[n++] = ip;
                }
                p->rife_acc += p->rife_den;
            }
            if (failed || !n) {
                // resync the grid on the next pair, starting from `out`
                p->rife_acc = p->rife_den;
                if (!emitted_cur) {
                    // drop any interpolations made before the failure and
                    // emit the source frame so playback keeps moving
                    for (int i = 0; i < n; i++)
                        mp_image_unrefp(&list[i]);
                    n = 0;
                    emitted_cur = true;
                }
            } else {
                p->rife_acc -= p->rife_num;  // rebase into the next pair
            }
            if (n) {
                first = list[0];
                for (int i = 1; i < n; i++)
                    p->outq[p->outq_n++] = list[i];
                p->outq_pos = 0;
            }
        } else {
            // fresh pair chain (start of stream or after a seek): `out`
            // is the grid origin and the next point lies den/num in
            p->rife_acc = p->rife_den;
        }
        mp_image_unrefp(&p->rife_prev);
        p->rife_prev = mp_image_new_ref(out);
        if (!emitted_cur)
            mp_image_unrefp(&out);  // endpoint only; rife_prev keeps it
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
    if (strcmp(cmd->cmd, "poll") == 0) {
        // No-op wakeup: while playback is paused no frames flow, so
        // process() (and with it the background-build completion poll)
        // never runs. The engine-build monitor script kicks this so a
        // finished build is noticed and playback can auto-resume.
        mp_filter_wakeup(vf);
        return true;
    }
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
        for (int i = 0; i < MAX_DEPTH; i++)
            if (p->decode_evt[i])
                p->cu->cuEventDestroy(p->decode_evt[i]);
        p->cu->cuStreamDestroy(p->own_stream);
        p->cu->cuCtxPopCurrent(&dummy);
    }
#if HAVE_D3D11
    for (int i = 0; i < p->d3d_stage_count; i++)
        ID3D11Texture2D_Release(p->d3d_stage[i]);
#endif
    av_buffer_unref(&p->hw_pool);
    av_buffer_unref(&p->src_pool);
    av_buffer_unref(&p->av_device_ref);
    if (p->cu)
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

    // Not fatal: on non-NVIDIA machines the D3D11/DirectML path carries
    // the filter; CUDA frames are rejected at reinit instead.
    if (cuda_load_functions(&p->cu, NULL) < 0) {
        p->cu = NULL;
        MP_VERBOSE(f, "CUDA driver API unavailable (D3D11 input only)\n");
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
        p->api.flush = aji_lib_sym(p->api.handle, "aji_flush");
        p->api.done = aji_lib_sym(p->api.handle, "aji_done");
        p->api.wait = aji_lib_sym(p->api.handle, "aji_wait");
        p->api.rife_factor = aji_lib_sym(p->api.handle, "aji_rife_factor");
        // Optional (added without an API_VERSION bump); NULL on older shims,
        // which then run the default upscale-then-RIFE order.
        p->api.rife_before_upscale =
            aji_lib_sym(p->api.handle, "aji_rife_before_upscale");
        p->api.infer_rife = aji_lib_sym(p->api.handle, "aji_infer_rife");
        p->api.poll = aji_lib_sym(p->api.handle, "aji_poll");
        p->api.current_log = aji_lib_sym(p->api.handle, "aji_current_log");
        p->api.last_error = aji_lib_sym(p->api.handle, "aji_last_error");
        p->api.destroy = aji_lib_sym(p->api.handle, "aji_destroy");
        if (!p->api.create || !p->api.set_slot || !p->api.configure ||
            !p->api.infer || !p->api.flush || !p->api.done || !p->api.wait ||
            !p->api.rife_factor || !p->api.infer_rife ||
            !p->api.poll ||
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
#if HAVE_D3D11
    mp_refqueue_add_in_format(p->queue, IMGFMT_D3D11, 0);
#endif
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
    {"output-444", OPT_BOOL(output_444)},
    {"queue-depth", OPT_INT(queue_depth), M_RANGE(1, MAX_DEPTH)},
    {0}
};

const struct mp_user_filter_entry vf_animejanai = {
    .desc = {
        .description = "AnimeJaNai AI upscaling filter (CUDA/D3D11)",
        .name = "animejanai",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .passthrough = true,
            .slot = 1,
            .skip_seek_pre_target = true,
            .output_444 = true,
            .queue_depth = 3,
        },
        .options = vf_opts_fields,
    },
    .create = vf_animejanai_create,
};
