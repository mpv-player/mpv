#ifndef MPV_LAVC_H
#define MPV_LAVC_H

#include <stdbool.h>
#include <pthread.h>

#include <libavcodec/avcodec.h>

#include "config.h"

#include "demux/stheader.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"
#include "video/hwdec.h"

#define HWDEC_DELAY_QUEUE_COUNT 2

// Maximum number of surfaces the player wants to buffer.
// This number might require adjustment depending on whatever the player does;
// for example, if vo_opengl increases the number of reference surfaces for
// interpolation, this value has to be increased too.
#define HWDEC_EXTRA_SURFACES 6

struct mpv_global;

typedef struct lavc_ctx {
    struct mp_log *log;
    struct MPOpts *opts;
    AVCodecContext *avctx;
    AVFrame *pic;
    struct vd_lavc_hwdec *hwdec;
    AVRational codec_timebase;
    enum AVPixelFormat pix_fmt;
    enum AVDiscard skip_frame;
    bool flushing;
    const char *decoder;
    bool hwdec_failed;
    bool hwdec_notified;

    bool intra_only;
    int framedrop_flags;

    // For HDR side-data caching
    float cached_sig_peak;

    bool hw_probing;
    struct demux_packet **sent_packets;
    int num_sent_packets;

    struct demux_packet **requeue_packets;
    int num_requeue_packets;

    struct mp_image **delay_queue;
    int num_delay_queue;
    int max_delay_queue;

    // From VO
    struct mp_hwdec_devices *hwdec_devs;

    // For free use by hwdec implementation
    void *hwdec_priv;

    // Set by generic hwaccels.
    struct mp_hwdec_ctx *hwdec_dev;
    bool owns_hwdec_dev;

    int hwdec_fmt;
    int hwdec_w;
    int hwdec_h;
    int hwdec_profile;

    bool hwdec_request_reinit;
    int hwdec_fail_count;

    struct mp_image_pool *hwdec_swpool;

    AVBufferRef *cached_hw_frames_ctx;

    // --- The following fields are protected by dr_lock.
    pthread_mutex_t dr_lock;
    bool dr_failed;
    struct mp_image_pool *dr_pool;
    int dr_imgfmt, dr_w, dr_h, dr_stride_align;
} vd_ffmpeg_ctx;

struct vd_lavc_hwdec {
    enum hwdec_type type;
    // If non-0, get this hwdec type from the VO (for the AVHWDeviceContext).
    enum hwdec_type interop_type;
    // If not-0: the IMGFMT_ format that should be accepted in the libavcodec
    // get_format callback.
    int image_format;
    // Always returns a non-hwaccel image format.
    bool copying;
    // Setting this will queue the given number of frames before calling
    // process_image() or returning them to the renderer. This can increase
    // efficiency by not blocking on the hardware pipeline by reading back
    // immediately after decoding.
    int delay_queue;
    // If true, AVCodecContext will destroy the underlying decoder.
    bool volatile_context;
    int (*probe)(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec);
    int (*init)(struct lavc_ctx *ctx);
    int (*init_decoder)(struct lavc_ctx *ctx, int w, int h);
    void (*uninit)(struct lavc_ctx *ctx);
    // Note: if init_decoder is set, this will always use the values from the
    //       last successful init_decoder call. Otherwise, it's up to you.
    struct mp_image *(*allocate_image)(struct lavc_ctx *ctx, int w, int h);
    // Process the image returned by the libavcodec decoder.
    struct mp_image *(*process_image)(struct lavc_ctx *ctx, struct mp_image *img);
    // For copy hwdecs. If probing is true, don't log errors if unavailable.
    // The returned device will be freed with mp_hwdec_ctx->destroy.
    struct mp_hwdec_ctx *(*create_dev)(struct mpv_global *global,
                                       struct mp_log *log, bool probing);
    // Optional. Fill in special hwaccel- and codec-specific requirements.
    void (*hwframes_refine)(struct lavc_ctx *ctx, AVBufferRef *hw_frames_ctx);
    // Suffix for libavcodec decoder. If non-NULL, the codec is overridden
    // with hwdec_find_decoder.
    // Intuitively, this will force the corresponding wrapper decoder.
    const char *lavc_suffix;
    // Generic hwaccels set AVCodecContext.hw_frames_ctx in get_format().
    bool generic_hwaccel;
    // If set, AVCodecContext.hw_frames_ctx will be initialized in get_format,
    // and pixfmt_map must be non-NULL.
    bool set_hwframes;
    // Array of pixfmt pairs. The first pixfmt is the AVCodecContext.sw_pix_fmt,
    // the second the required AVHWFramesContext.sw_format.
    const enum AVPixelFormat (*pixfmt_map)[2];
    // The generic hwaccel has a fixed pool size. Enough surfaces need to be
    // preallocated before decoding begins. If false, pool size is left to 0.
    bool static_pool;
};

enum {
    HWDEC_ERR_NO_CTX = -2,
    HWDEC_ERR_NO_CODEC = -3,
    HWDEC_ERR_EMULATED = -4,    // probing successful, but emulated API detected
};

struct hwdec_profile_entry {
    enum AVCodecID av_codec;
    int ff_profile;
    uint64_t hw_profile;
};

int hwdec_get_max_refs(struct lavc_ctx *ctx);
int hwdec_setup_hw_frames_ctx(struct lavc_ctx *ctx, AVBufferRef *device_ctx,
                              int av_sw_format, int initial_pool_size);

#endif
