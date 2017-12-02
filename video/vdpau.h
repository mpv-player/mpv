#ifndef MPV_VDPAU_H
#define MPV_VDPAU_H

#include <stdbool.h>
#include <inttypes.h>

#include <pthread.h>

#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

#include "common/msg.h"
#include "hwdec.h"

#include "config.h"
#if !HAVE_GPL
#error GPL only
#endif

#define CHECK_VDP_ERROR_ST(ctx, message, statement) \
    do { \
        if (vdp_st != VDP_STATUS_OK) { \
            MP_ERR(ctx, "%s: %s\n", message, vdp->get_error_string(vdp_st)); \
            statement \
        } \
    } while (0)

#define CHECK_VDP_ERROR(ctx, message) \
    CHECK_VDP_ERROR_ST(ctx, message, return -1;)

#define CHECK_VDP_ERROR_NORETURN(ctx, message) \
    CHECK_VDP_ERROR_ST(ctx, message, ;)

#define CHECK_VDP_WARNING(ctx, message) \
    do { \
        if (vdp_st != VDP_STATUS_OK) \
            MP_WARN(ctx, "%s: %s\n", message, vdp->get_error_string(vdp_st)); \
    } while (0)

struct vdp_functions {
#define VDP_FUNCTION(vdp_type, _, mp_name) vdp_type *mp_name;
#include "video/vdpau_functions.inc"
#undef VDP_FUNCTION
};


#define MAX_VIDEO_SURFACES 50

// Shared state. Objects created from different VdpDevices are often (always?)
// incompatible to each other, so all code must use a shared VdpDevice.
struct mp_vdpau_ctx {
    struct mp_log *log;
    Display *x11;
    bool close_display;

    struct mp_hwdec_ctx hwctx;
    struct AVBufferRef *av_device_ref;

    // These are mostly immutable, except on preemption. We don't really care
    // to synchronize the preemption case fully correctly, because it's an
    // extremely obscure corner case, and basically a vdpau API design bug.
    // What we do will sort-of work anyway (no memory errors are possible).
    struct vdp_functions vdp;
    VdpGetProcAddress *get_proc_address;
    VdpDevice vdp_device;

    pthread_mutex_t preempt_lock;
    bool is_preempted;                  // set to true during unavailability
    uint64_t preemption_counter;        // incremented after _restoring_
    bool preemption_user_notified;
    double last_preemption_retry_fail;
    VdpOutputSurface preemption_obj;    // dummy for reliable preempt. check

    // Surface pool
    pthread_mutex_t pool_lock;
    int64_t age_counter;
    struct surface_entry {
        VdpVideoSurface surface;
        VdpOutputSurface osurface;
        bool allocated;
        int w, h;
        VdpRGBAFormat rgb_format;
        VdpChromaType chroma;
        bool rgb;
        bool in_use;
        int64_t age;
    } video_surfaces[MAX_VIDEO_SURFACES];
};

struct mp_vdpau_ctx *mp_vdpau_create_device_x11(struct mp_log *log, Display *x11,
                                                bool probing);
void mp_vdpau_destroy(struct mp_vdpau_ctx *ctx);

int mp_vdpau_handle_preemption(struct mp_vdpau_ctx *ctx, uint64_t *counter);

struct mp_image *mp_vdpau_get_video_surface(struct mp_vdpau_ctx *ctx,
                                            VdpChromaType chroma, int w, int h);

bool mp_vdpau_get_format(int imgfmt, VdpChromaType *out_chroma_type,
                         VdpYCbCrFormat *out_pixel_format);
bool mp_vdpau_get_rgb_format(int imgfmt, VdpRGBAFormat *out_rgba_format);

struct mp_image *mp_vdpau_upload_video_surface(struct mp_vdpau_ctx *ctx,
                                               struct mp_image *mpi);

struct mp_vdpau_ctx *mp_vdpau_get_ctx_from_av(struct AVBufferRef *hw_device_ctx);

bool mp_vdpau_guess_if_emulated(struct mp_vdpau_ctx *ctx);

#endif
