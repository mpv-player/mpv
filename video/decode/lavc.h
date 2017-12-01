#ifndef MPV_LAVC_H
#define MPV_LAVC_H

#include <stdbool.h>
#include <pthread.h>

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>

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
    enum AVDiscard skip_frame;
    bool flushing;
    const char *decoder;
    bool hwdec_failed;
    bool hwdec_notified;

    bool intra_only;
    int framedrop_flags;

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
    // If true, create a AVHWDeviceContext with default parameters. In this
    // case, create_standalone_dev_type is set to a valid value.
    bool create_standalone_dev;
    enum AVHWDeviceType create_standalone_dev_type;
    // If not-0: the IMGFMT_ format that should be accepted in the libavcodec
    // get_format callback.
    int image_format;
    // Always returns a non-hwaccel image format.
    bool copying;
    // Setting this will queue the given number of frames before returning them
    // to the renderer. This can increase efficiency by not blocking on the
    // hardware pipeline by reading back immediately after decoding.
    int delay_queue;
    int (*probe)(struct lavc_ctx *ctx, struct vd_lavc_hwdec *hwdec,
                 const char *codec);
    int (*init)(struct lavc_ctx *ctx);
    int (*init_decoder)(struct lavc_ctx *ctx);
    void (*uninit)(struct lavc_ctx *ctx);
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
};

enum {
    HWDEC_ERR_NO_CTX = -2,
    HWDEC_ERR_NO_CODEC = -3,
    HWDEC_ERR_EMULATED = -4,    // probing successful, but emulated API detected
};

#endif
