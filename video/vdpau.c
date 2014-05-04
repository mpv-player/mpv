/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include "vdpau.h"

#include "osdep/timer.h"

#include "video/out/x11_common.h"
#include "video/img_format.h"
#include "video/mp_image.h"

static void mark_vdpau_objects_uninitialized(struct mp_vdpau_ctx *ctx)
{
    for (int i = 0; i < MAX_VIDEO_SURFACES; i++)
        ctx->video_surfaces[i].surface = VDP_INVALID_HANDLE;
    ctx->vdp_device = VDP_INVALID_HANDLE;
}

static void preemption_callback(VdpDevice device, void *context)
{
    struct mp_vdpau_ctx *ctx = context;
    ctx->is_preempted = true;
}

static int win_x11_init_vdpau_procs(struct mp_vdpau_ctx *ctx)
{
    struct vo_x11_state *x11 = ctx->x11;
    VdpStatus vdp_st;

    // Don't operate on ctx->vdp directly, so that even if init fails, ctx->vdp
    // will have the function pointers from the previous successful init, and
    // won't randomly make other code crash on calling NULL pointers.
    struct vdp_functions vdp = {0};

    if (!x11)
        return -1;

    struct vdp_function {
        const int id;
        int offset;
    };

    static const struct vdp_function vdp_func[] = {
#define VDP_FUNCTION(_, macro_name, mp_name) {macro_name, offsetof(struct vdp_functions, mp_name)},
#include "video/vdpau_functions.inc"
#undef VDP_FUNCTION
        {0, -1}
    };

    VdpGetProcAddress *get_proc_address;
    vdp_st = vdp_device_create_x11(x11->display, x11->screen, &ctx->vdp_device,
                                   &get_proc_address);
    if (vdp_st != VDP_STATUS_OK) {
        if (ctx->is_preempted)
            MP_DBG(ctx, "Error calling vdp_device_create_x11 while preempted: %d\n",
                   vdp_st);
        else
            MP_ERR(ctx, "Error when calling vdp_device_create_x11: %d\n", vdp_st);
        return -1;
    }

    for (const struct vdp_function *dsc = vdp_func; dsc->offset >= 0; dsc++) {
        vdp_st = get_proc_address(ctx->vdp_device, dsc->id,
                                  (void **)((char *)&vdp + dsc->offset));
        if (vdp_st != VDP_STATUS_OK) {
            MP_ERR(ctx, "Error when calling vdp_get_proc_address(function "
                   "id %d): %s\n",  dsc->id,
                   vdp.get_error_string ? vdp.get_error_string(vdp_st) : "?");
            return -1;
        }
    }

    ctx->vdp = vdp;
    ctx->get_proc_address = get_proc_address;

    vdp_st = vdp.preemption_callback_register(ctx->vdp_device,
                                              preemption_callback, ctx);
    return 0;
}

static int handle_preemption(struct mp_vdpau_ctx *ctx)
{
    if (!ctx->is_preempted)
        return 0;
    mark_vdpau_objects_uninitialized(ctx);
    if (!ctx->preemption_user_notified) {
        MP_ERR(ctx, "Got display preemption notice! Will attempt to recover.\n");
        ctx->preemption_user_notified = true;
    }
    /* Trying to initialize seems to be quite slow, so only try once a
     * second to avoid using 100% CPU. */
    if (ctx->last_preemption_retry_fail &&
        mp_time_sec() - ctx->last_preemption_retry_fail < 1.0)
        return -1;
    if (win_x11_init_vdpau_procs(ctx) < 0) {
        ctx->last_preemption_retry_fail = mp_time_sec();
        return -1;
    }
    ctx->preemption_user_notified = false;
    ctx->last_preemption_retry_fail = 0;
    ctx->is_preempted = false;
    ctx->preemption_counter++;
    MP_INFO(ctx, "Recovered from display preemption.\n");
    return 1;
}

// Check whether vdpau initialization and preemption status is ok and we can
// proceed normally.
bool mp_vdpau_status_ok(struct mp_vdpau_ctx *ctx)
{
    return handle_preemption(ctx) >= 0;
}

static void release_decoder_surface(void *ptr)
{
    bool *in_use_ptr = ptr;
    *in_use_ptr = false;
}

static struct mp_image *create_ref(struct surface_entry *e)
{
    assert(!e->in_use);
    e->in_use = true;
    struct mp_image *res =
        mp_image_new_custom_ref(&(struct mp_image){0}, &e->in_use,
                                release_decoder_surface);
    mp_image_setfmt(res, IMGFMT_VDPAU);
    mp_image_set_size(res, e->w, e->h);
    res->planes[0] = (void *)"dummy"; // must be non-NULL, otherwise arbitrary
    res->planes[3] = (void *)(intptr_t)e->surface;
    return res;
}

struct mp_image *mp_vdpau_get_video_surface(struct mp_vdpau_ctx *ctx,
                                            VdpChromaType chroma, int w, int h)
{
    struct vdp_functions *vdp = &ctx->vdp;
    VdpStatus vdp_st;

    // Destroy all unused surfaces that don't have matching parameters
    for (int n = 0; n < MAX_VIDEO_SURFACES; n++) {
        struct surface_entry *e = &ctx->video_surfaces[n];
        if (!e->in_use && e->surface != VDP_INVALID_HANDLE) {
            if (e->chroma != chroma || e->w != w || e->h != h) {
                vdp_st = vdp->video_surface_destroy(e->surface);
                CHECK_VDP_WARNING(ctx, "Error when calling vdp_video_surface_destroy");
                e->surface = VDP_INVALID_HANDLE;
            }
        }
    }

    // Try to find an existing unused surface
    for (int n = 0; n < MAX_VIDEO_SURFACES; n++) {
        struct surface_entry *e = &ctx->video_surfaces[n];
        if (!e->in_use && e->surface != VDP_INVALID_HANDLE) {
            assert(e->w == w && e->h == h);
            assert(e->chroma == chroma);
            return create_ref(e);
        }
    }

    // Allocate new surface
    for (int n = 0; n < MAX_VIDEO_SURFACES; n++) {
        struct surface_entry *e = &ctx->video_surfaces[n];
        if (!e->in_use) {
            assert(e->surface == VDP_INVALID_HANDLE);
            e->chroma = chroma;
            e->w = w;
            e->h = h;
            if (ctx->is_preempted) {
                MP_WARN(ctx, "Preempted, no surface.\n");
            } else {
                vdp_st = vdp->video_surface_create(ctx->vdp_device, chroma,
                                                   w, h, &e->surface);
                CHECK_VDP_WARNING(ctx, "Error when calling vdp_video_surface_create");
            }
            return create_ref(e);
        }
    }

    MP_ERR(ctx, "no surfaces available in mp_vdpau_get_video_surface\n");
    return NULL;
}

struct mp_vdpau_ctx *mp_vdpau_create_device_x11(struct mp_log *log,
                                                struct vo_x11_state *x11)
{
    struct mp_vdpau_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mp_vdpau_ctx) {
        .log = log,
        .x11 = x11,
    };

    mark_vdpau_objects_uninitialized(ctx);

    if (win_x11_init_vdpau_procs(ctx) < 0) {
        if (ctx->vdp.device_destroy)
            ctx->vdp.device_destroy(ctx->vdp_device);
        talloc_free(ctx);
        return NULL;
    }
    return ctx;
}

void mp_vdpau_destroy(struct mp_vdpau_ctx *ctx)
{
    struct vdp_functions *vdp = &ctx->vdp;
    VdpStatus vdp_st;

    for (int i = 0; i < MAX_VIDEO_SURFACES; i++) {
        // can't hold references past context lifetime
        assert(!ctx->video_surfaces[i].in_use);
        if (ctx->video_surfaces[i].surface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->video_surface_destroy(ctx->video_surfaces[i].surface);
            CHECK_VDP_WARNING(ctx, "Error when calling vdp_video_surface_destroy");
        }
    }

    if (ctx->vdp_device != VDP_INVALID_HANDLE) {
        vdp_st = vdp->device_destroy(ctx->vdp_device);
        CHECK_VDP_WARNING(ctx, "Error when calling vdp_device_destroy");
    }

    talloc_free(ctx);
}

bool mp_vdpau_get_format(int imgfmt, VdpChromaType *out_chroma_type,
                         VdpYCbCrFormat *out_pixel_format)
{
    VdpChromaType chroma = VDP_CHROMA_TYPE_420;
    VdpYCbCrFormat ycbcr = (VdpYCbCrFormat)-1;

    switch (imgfmt) {
    case IMGFMT_420P:
        ycbcr = VDP_YCBCR_FORMAT_YV12;
        break;
    case IMGFMT_NV12:
        ycbcr = VDP_YCBCR_FORMAT_NV12;
        break;
    case IMGFMT_YUYV:
        ycbcr = VDP_YCBCR_FORMAT_YUYV;
        chroma = VDP_CHROMA_TYPE_422;
        break;
    case IMGFMT_UYVY:
        ycbcr = VDP_YCBCR_FORMAT_UYVY;
        chroma = VDP_CHROMA_TYPE_422;
        break;
    case IMGFMT_VDPAU:
        break;
    default:
        return false;
    }

    if (out_chroma_type)
        *out_chroma_type = chroma;
    if (out_pixel_format)
        *out_pixel_format = ycbcr;
    return true;
}

// Use mp_vdpau_get_video_surface, and upload mpi to it. Return NULL on failure.
// If the image is already a vdpau video surface, just return a reference.
struct mp_image *mp_vdpau_upload_video_surface(struct mp_vdpau_ctx *ctx,
                                               struct mp_image *mpi)
{
    struct vdp_functions *vdp = &ctx->vdp;
    VdpStatus vdp_st;

    if (mpi->imgfmt == IMGFMT_VDPAU)
        return mp_image_new_ref(mpi);

    VdpChromaType chroma_type;
    VdpYCbCrFormat pixel_format;
    if (!mp_vdpau_get_format(mpi->imgfmt, &chroma_type, &pixel_format))
        return NULL;

    struct mp_image *hwmpi =
        mp_vdpau_get_video_surface(ctx, chroma_type, mpi->w, mpi->h);
    if (!hwmpi)
        return NULL;

    VdpVideoSurface surface = (intptr_t)hwmpi->planes[3];
    const void *destdata[3] = {mpi->planes[0], mpi->planes[2], mpi->planes[1]};
    if (mpi->imgfmt == IMGFMT_NV12)
        destdata[1] = destdata[2];
    vdp_st = vdp->video_surface_put_bits_y_cb_cr(surface,
                pixel_format, destdata, mpi->stride);
    CHECK_VDP_WARNING(ctx, "Error when calling vdp_video_surface_put_bits_y_cb_cr");

    mp_image_copy_attributes(hwmpi, mpi);
    return hwmpi;
}
