#ifndef MPV_VDPAU_H
#define MPV_VDPAU_H

#include <stdbool.h>
#include <inttypes.h>

#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

#include "core/mp_msg.h"

#define CHECK_ST_ERROR(message) \
    do { \
        if (vdp_st != VDP_STATUS_OK) { \
            mp_msg(MSGT_VO, MSGL_ERR, "[vdpau] %s: %s\n", \
                   message, vdp->get_error_string(vdp_st)); \
            return -1; \
        } \
    } while (0)

#define CHECK_ST_WARNING(message) \
    do { \
        if (vdp_st != VDP_STATUS_OK) \
            mp_msg(MSGT_VO, MSGL_WARN, "[vdpau] %s: %s\n", \
                   message, vdp->get_error_string(vdp_st)); \
    } while (0)

struct vdp_functions {
#define VDP_FUNCTION(vdp_type, _, mp_name) vdp_type *mp_name;
#include "video/out/vdpau_template.c"
#undef VDP_FUNCTION
};

// Shared state. Objects created from different VdpDevices are often (always?)
// incompatible to each other, so all code must use a shared VdpDevice.
struct mp_vdpau_ctx {
    struct vdp_functions *vdp;
    VdpDevice vdp_device;
    bool is_preempted;                  // set to true during unavailability
    uint64_t preemption_counter;        // incremented after _restoring_
    bool (*status_ok)(struct mp_vdpau_ctx *ctx);
    struct mp_image *(*get_video_surface)(struct mp_vdpau_ctx *ctx, int fmt,
                                          VdpChromaType chroma, int w, int h);
    void *priv; // for VO
};

bool mp_vdpau_status_ok(struct mp_vdpau_ctx *ctx);

struct mp_image *mp_vdpau_get_video_surface(struct mp_vdpau_ctx *ctx, int fmt,
                                            VdpChromaType chroma, int w, int h);

bool mp_vdpau_get_format(int imgfmt, VdpChromaType *out_chroma_type,
                         VdpYCbCrFormat *out_pixel_format);

#endif
