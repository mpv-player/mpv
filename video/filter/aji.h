/*
 * aji.h — AnimeJaNai inference shim, C ABI (version 4).
 *
 * Boundary between the mpv filter (mingw/gcc world) and the inference
 * backend (TensorRT/MSVC world on Windows). Only C types and opaque
 * handles cross this ABI; CUDA handles are passed as void*.
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

#define AJI_API_VERSION 4

typedef struct aji_ctx aji_ctx;

enum aji_format {
    AJI_FMT_NV12 = 1,   /* 8-bit 4:2:0, interleaved CbCr */
    AJI_FMT_P010 = 2,   /* 10-bit-in-16 (MSB) 4:2:0, interleaved CbCr */
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
    void *plane[2];           /* CUdeviceptr: Y, CbCr */
    ptrdiff_t stride[2];      /* bytes */
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

/* Upscale one frame through the configured plan. Enqueues on cu_stream;
 * no synchronization — caller owns the stream. Frame dims must match the
 * last aji_configure(). */
AJI_EXPORT int aji_infer(aji_ctx *c, const aji_frame *in,
                         const aji_frame *out, void *cu_stream);

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
 * pipeline). Enqueues on cu_stream but synchronizes it internally for
 * the scene-change decision. */
AJI_EXPORT int aji_infer_rife(aji_ctx *c, const aji_frame *a,
                              const aji_frame *b, double t,
                              const aji_frame *out, void *cu_stream);

AJI_EXPORT const char *aji_last_error(aji_ctx *c);

AJI_EXPORT void aji_destroy(aji_ctx **c);

#ifdef __cplusplus
}
#endif

#endif /* AJI_H */
