/*
 * aji.h — AnimeJaNai inference shim, C ABI.
 *
 * Boundary between the mpv filter (mingw/gcc world) and the inference
 * backend (TensorRT/MSVC world on Windows). Only C types and opaque
 * handles cross this ABI; CUDA handles are passed as void* / plain
 * integers so consumers need no CUDA headers.
 *
 * Phase 0 spike scope: single 2x engine, NV12/P010 in, same format out,
 * BT.709 limited range hardcoded. Colorspace metadata, chain selection
 * and engine building move in here in later phases.
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

#define AJI_API_VERSION 1

typedef struct aji_ctx aji_ctx;

enum aji_format {
    AJI_FMT_NV12 = 1,   /* 8-bit 4:2:0, interleaved CbCr */
    AJI_FMT_P010 = 2,   /* 10-bit-in-16 (MSB) 4:2:0, interleaved CbCr */
};

enum aji_status {
    AJI_OK          = 0,
    AJI_ERR         = -1,
    AJI_ERR_SHAPE   = -2,  /* frame dims outside engine profile / not even */
    AJI_ERR_FORMAT  = -3,
    AJI_ERR_CUDA    = -4,
    AJI_ERR_ENGINE  = -5,
};

typedef void (*aji_log_fn)(void *opaque, int level, const char *msg);

typedef struct aji_create_params {
    uint32_t api_version;     /* AJI_API_VERSION */
    void *cuda_context;       /* CUcontext; NULL = retain primary ctx, dev 0 */
    const char *engine_path;  /* serialized TensorRT engine */
    int max_width, max_height;/* max input dims, for buffer preallocation */
    aji_log_fn log;           /* optional */
    void *log_opaque;
} aji_create_params;

typedef struct aji_frame {
    int width, height;        /* luma dimensions */
    int format;               /* enum aji_format */
    void *plane[2];           /* CUdeviceptr: Y, CbCr */
    ptrdiff_t stride[2];      /* bytes */
} aji_frame;

/* Create a context. Deserializes the engine and preallocates I/O tensors. */
AJI_EXPORT aji_ctx *aji_create(const aji_create_params *params);

/* Upscale one frame. All work is enqueued on cu_stream (CUstream; NULL =
 * default stream); no synchronization is performed — the caller owns the
 * stream and syncs when it needs the output. in/out planes must be device
 * memory on the context passed at create time. out dims must be exactly
 * scale_factor * in dims. */
AJI_EXPORT int aji_infer(aji_ctx *c, const aji_frame *in,
                         const aji_frame *out, void *cu_stream);

/* Spatial scale factor of the loaded engine (spike: fixed 2). */
AJI_EXPORT int aji_scale_factor(aji_ctx *c);

/* Last error message (valid until next call on this ctx). */
AJI_EXPORT const char *aji_last_error(aji_ctx *c);

AJI_EXPORT void aji_destroy(aji_ctx **c);

#ifdef __cplusplus
}
#endif

#endif /* AJI_H */
