#ifndef MPV_VDPAU_H
#define MPV_VDPAU_H

#include <stdbool.h>
#include <inttypes.h>

#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

#include "common/msg.h"

#define CHECK_VDP_ERROR(ctx, message) \
    do { \
        if (vdp_st != VDP_STATUS_OK) { \
            MP_ERR(ctx, "%s: %s\n", message, vdp->get_error_string(vdp_st)); \
            return -1; \
        } \
    } while (0)

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
    struct vdp_functions vdp;
    VdpGetProcAddress *get_proc_address;
    VdpDevice vdp_device;
    bool is_preempted;                  // set to true during unavailability
    uint64_t preemption_counter;        // incremented after _restoring_

    bool preemption_user_notified;
    double last_preemption_retry_fail;

    struct vo_x11_state *x11;

    // Surface pool
    struct surface_entry {
        VdpVideoSurface surface;
        int w, h;
        VdpChromaType chroma;
        bool in_use;
    } video_surfaces[MAX_VIDEO_SURFACES];

    struct mp_log *log;
};

struct mp_vdpau_ctx *mp_vdpau_create_device_x11(struct mp_log *log,
                                                struct vo_x11_state *x11);
void mp_vdpau_destroy(struct mp_vdpau_ctx *ctx);

bool mp_vdpau_status_ok(struct mp_vdpau_ctx *ctx);

struct mp_image *mp_vdpau_get_video_surface(struct mp_vdpau_ctx *ctx,
                                            VdpChromaType chroma, int w, int h);

bool mp_vdpau_get_format(int imgfmt, VdpChromaType *out_chroma_type,
                         VdpYCbCrFormat *out_pixel_format);

struct mp_image *mp_vdpau_upload_video_surface(struct mp_vdpau_ctx *ctx,
                                               struct mp_image *mpi);

#endif
