/*
 * aji.h — AnimeJaNai inference shim, C ABI (version 7).
 *
 * Boundary between the mpv filter (mingw/gcc world) and the inference
 * backends (MSVC world on Windows). Only C types and opaque handles
 * cross this ABI; CUDA/D3D11 handles are passed as void*.
 *
 * aji.dll itself is a thin dispatcher: it reads the conf's [global]
 * backend key and forwards to the sibling backend library (aji_trt /
 * aji_dml). Frame handles are backend-specific:
 *  - TensorRT: aji_frame.plane[] = CUdeviceptr per plane; cu_stream =
 *    CUstream. d3d11_device is ignored.
 *  - DirectML: aji_frame.plane[0] = ID3D11Texture2D*, plane[1] =
 *    (intptr_t)subresource index; stride[] is ignored; cu_stream is
 *    NULL. d3d11_device is required, and frame textures must be
 *    created with the SHARED + SHARED_NTHANDLE misc flags.
 *
 * Two modes:
 *  - conf mode: create with conf_path/model_dir; aji_configure() selects
 *    the active chain for (w, h, fps) per animejanai.conf semantics,
 *    building missing TensorRT engines on first use (trtexec subprocess).
 *  - direct mode: create with engine_path; a single fixed 2x engine
 *    (spike/harness compatibility).
 */

#ifndef AJI_H
#define AJI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  define AJI_EXPORT __declspec(dllexport)
#else
#  define AJI_EXPORT __attribute__((visibility("default")))
#endif

#define AJI_API_VERSION 7

typedef struct aji_ctx aji_ctx;

enum aji_format {
    AJI_FMT_NV12 = 1,   /* 8-bit 4:2:0, interleaved CbCr */
    AJI_FMT_P010 = 2,   /* 10-bit-in-16 (MSB) 4:2:0, interleaved CbCr */
    AJI_FMT_YUV444P16 = 3, /* 16-bit 4:4:4 planar: carries the model's
                              full-resolution chroma. Valid for input and
                              output; TensorRT backend only. */
    AJI_FMT_RGB10A2 = 4,   /* packed 10-bit RGB (DXGI R10G10B10A2 / mpv
                              x2bgr10: R in the low 10 bits). DirectML only —
                              what mpv hwuploads a 4:4:4 source to on D3D11.
                              Already RGB, so the backend skips the YUV
                              matrix and round-trips it as RGB. */
};

enum aji_matrix {
    AJI_MATRIX_BT601  = 1,
    AJI_MATRIX_BT709  = 2,
    AJI_MATRIX_BT2020 = 3,  /* non-constant luminance */
};

enum aji_range {
    AJI_RANGE_LIMITED = 1,
    AJI_RANGE_FULL    = 2,
};

enum aji_siting {           /* chroma sample position for 4:2:0 */
    AJI_SITING_LEFT    = 1, /* MPEG-2/4/H.264 default */
    AJI_SITING_CENTER  = 2, /* MPEG-1/JPEG */
    AJI_SITING_TOPLEFT = 3, /* common for UHD/HEVC 2020 */
};

enum aji_status {
    AJI_OK          = 0,
    AJI_SCENE       = 1,    /* aji_infer_rife: scene change, no interpolation */
    AJI_ERR         = -1,
    AJI_ERR_SHAPE   = -2,
    AJI_ERR_FORMAT  = -3,
    AJI_ERR_CUDA    = -4,
    AJI_ERR_ENGINE  = -5,
    AJI_ERR_CONF    = -6,
};

typedef void (*aji_log_fn)(void *opaque, int level, const char *msg);

typedef struct aji_create_params {
    uint32_t api_version;     /* AJI_API_VERSION */
    void *cuda_context;       /* CUcontext; NULL = retain primary ctx, dev 0 */

    /* conf mode */
    const char *conf_path;    /* animejanai.conf; NULL = direct mode */
    const char *model_dir;    /* dir with .onnx models + engine cache */
    const char *trtexec;      /* trtexec binary for on-first-play builds */
    const char *trtexec_env;  /* optional env prefix, e.g. "LD_LIBRARY_PATH=..." */
    int slot;                 /* initial slot (1-9, 1001-1003, 1010-1011) */
    const char *rife_model_dir; /* dir with rife_v*.onnx + engine cache;
                                   NULL disables RIFE even if a chain asks */
    int async_build;          /* nonzero: missing engines build on a
                                 background thread; aji_configure returns
                                 passthrough (or chain-without-rife)
                                 meanwhile, and aji_poll() reports when to
                                 reconfigure. Zero: builds block (CLI). */
    void *d3d11_device;       /* ID3D11Device* of the caller (the device
                                 the frame textures live on). Required by
                                 the DirectML backend; ignored by
                                 TensorRT. */

    /* direct mode */
    const char *engine_path;
    int max_width, max_height;

    aji_log_fn log;
    void *log_opaque;
} aji_create_params;

typedef struct aji_frame {
    int width, height;        /* luma dimensions */
    int format;               /* enum aji_format */
    int matrix;               /* enum aji_matrix */
    int range;                /* enum aji_range */
    int siting;               /* enum aji_siting */
    void *plane[3];           /* CUdeviceptr: Y, CbCr (4:2:0) or Y, Cb, Cr
                                 (4:4:4). DirectML: plane[0] is the
                                 ID3D11Texture2D*, plane[1] the subresource
                                 index. */
    ptrdiff_t stride[3];      /* bytes */
} aji_frame;

AJI_EXPORT aji_ctx *aji_create(const aji_create_params *params);

/* Select the slot used by the next aji_configure() (conf mode). */
AJI_EXPORT int aji_set_slot(aji_ctx *c, int slot);

/* Configure for a stream. Conf mode: select the first chain matching
 * (w*h, fps), build/load its engines, allocate buffers; returns 1 and the
 * output dims if a chain is active, 0 if no chain matched (caller handles
 * passthrough). Direct mode: returns 1 with out = scale * in.
 * Engine builds run synchronously (minutes on first play, as today). */
AJI_EXPORT int aji_configure(aji_ctx *c, int w, int h, double fps,
                             int *out_w, int *out_h);

/* Upscale one frame through the configured plan. Enqueues asynchronously
 * (TensorRT: on cu_stream; DirectML: on the shim's D3D12 queue) and may
 * return before the GPU finishes — the output is complete only once a
 * ticket taken after this call completes (see aji_flush). Successive
 * calls are ordered on the same stream/queue, so queuing the next frame
 * before the previous one completes is safe. Frame dims must match the
 * last aji_configure(). */
AJI_EXPORT int aji_infer(aji_ctx *c, const aji_frame *in,
                         const aji_frame *out, void *cu_stream);

/* Completion tickets (pipelining). aji_flush() places a marker after all
 * work submitted so far and returns its ticket: monotonic, nonzero; 0 is
 * "nothing pending" and always counts as complete. Tickets complete in
 * submission order. cu_stream must be the stream the work was enqueued
 * on (TensorRT; ignored by DirectML). */
AJI_EXPORT uint64_t aji_flush(aji_ctx *c, void *cu_stream);

/* Poll a ticket: 1 complete, 0 pending, <0 error. */
AJI_EXPORT int aji_done(aji_ctx *c, uint64_t ticket);

/* Block until a ticket completes: AJI_OK, or an error on device
 * loss/timeout. */
AJI_EXPORT int aji_wait(aji_ctx *c, uint64_t ticket);

/* Human-readable description of the active configuration, formatted like
 * currentanimejanai.log (info lines, blank line, numbered steps). Valid
 * after aji_configure() until the next configure/destroy. */
AJI_EXPORT const char *aji_current_log(aji_ctx *c);

/* Direct mode: spatial scale of the loaded engine. Conf mode: 0. */
AJI_EXPORT int aji_scale_factor(aji_ctx *c);

/* RIFE interpolation factor of the active chain. Returns 1 and fills
 * num/den if RIFE is active after the last aji_configure(), else 0. */
AJI_EXPORT int aji_rife_factor(aji_ctx *c, int *num, int *den);

/* Async builds: returns 1 (once) when a background engine build finished;
 * the caller should re-run aji_configure, which now finds the engine in
 * the cache (or logs the failure and stays passthrough). Cheap to call
 * per frame. */
AJI_EXPORT int aji_poll(aji_ctx *c);

/* Interpolate between two already-upscaled frames (dims = configure's
 * output dims) at time point t in (0,1). Returns AJI_OK with *out
 * written, or AJI_SCENE if the pair straddles a scene change (out is
 * untouched; emit a duplicate of `a` instead, like the reference
 * pipeline). The documented synchronous exception to the ticket model:
 * the scene-change decision is a CPU readback, so this synchronizes
 * internally and the output is complete on return. */
AJI_EXPORT int aji_infer_rife(aji_ctx *c, const aji_frame *a,
                              const aji_frame *b, double t,
                              const aji_frame *out, void *cu_stream);

AJI_EXPORT const char *aji_last_error(aji_ctx *c);

AJI_EXPORT void aji_destroy(aji_ctx **c);

#ifdef __cplusplus
}
#endif

#endif /* AJI_H */
