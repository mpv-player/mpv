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

struct hwdec_info {
    char name[64];
    char method_name[16]; // non-unique name describing the hwdec method
    const AVCodec *codec; // implemented by this codec
    enum AVHWDeviceType lavc_device; // if not NONE, get a hwdevice
    bool copying; // if true, outputs sw frames, or copy to sw ourselves
    enum AVPixelFormat pix_fmt; // if not NONE, select in get_format
    bool use_hw_frames; // set AVCodecContext.hw_frames_ctx
    bool use_hw_device; // set AVCodecContext.hw_device_ctx

    // for internal sorting
    int auto_pos;
    int rank;
};

typedef struct lavc_ctx {
    struct mp_log *log;
    struct MPOpts *opts;
    AVCodecContext *avctx;
    AVFrame *pic;
    bool use_hwdec;
    struct hwdec_info hwdec; // valid only if use_hwdec==true
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

    // Wrapped AVHWDeviceContext* used for decoding.
    AVBufferRef *hwdec_dev;

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

#endif
