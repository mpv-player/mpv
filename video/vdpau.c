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

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vdpau.h>

#include "vdpau.h"

#include "osdep/threads.h"
#include "osdep/timer.h"

#include "video/out/x11_common.h"
#include "img_format.h"
#include "mp_image.h"
#include "mp_image_pool.h"
#include "vdpau_mixer.h"

static void mark_vdpau_objects_uninitialized(struct mp_vdpau_ctx *ctx)
{
    for (int i = 0; i < MAX_VIDEO_SURFACES; i++) {
        ctx->video_surfaces[i].surface = VDP_INVALID_HANDLE;
        ctx->video_surfaces[i].osurface = VDP_INVALID_HANDLE;
        ctx->video_surfaces[i].allocated = false;
    }
    ctx->vdp_device = VDP_INVALID_HANDLE;
    ctx->preemption_obj = VDP_INVALID_HANDLE;
}

static void preemption_callback(VdpDevice device, void *context)
{
    struct mp_vdpau_ctx *ctx = context;

    pthread_mutex_lock(&ctx->preempt_lock);
    ctx->is_preempted = true;
    pthread_mutex_unlock(&ctx->preempt_lock);
}

static int win_x11_init_vdpau_procs(struct mp_vdpau_ctx *ctx, bool probing)
{
    Display *x11 = ctx->x11;
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
    vdp_st = vdp_device_create_x11(x11, DefaultScreen(x11), &ctx->vdp_device,
                                   &get_proc_address);
    if (vdp_st != VDP_STATUS_OK) {
        if (ctx->is_preempted) {
            MP_DBG(ctx, "Error calling vdp_device_create_x11 while preempted: %d\n",
                   vdp_st);
        } else {
            int lev = probing ? MSGL_V : MSGL_ERR;
            mp_msg(ctx->log, lev, "Error when calling vdp_device_create_x11: %d\n",
                   vdp_st);
        }
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

    if (ctx->av_device_ref) {
        AVHWDeviceContext *hwctx = (void *)ctx->av_device_ref->data;
        AVVDPAUDeviceContext *vdctx = hwctx->hwctx;

        vdctx->device = ctx->vdp_device;
        vdctx->get_proc_address = ctx->get_proc_address;
    }

    vdp_st = vdp.output_surface_create(ctx->vdp_device, VDP_RGBA_FORMAT_B8G8R8A8,
                                       1, 1, &ctx->preemption_obj);
    if (vdp_st != VDP_STATUS_OK) {
        MP_ERR(ctx, "Could not create dummy object: %s",
               vdp.get_error_string(vdp_st));
        return -1;
    }

    vdp.preemption_callback_register(ctx->vdp_device, preemption_callback, ctx);
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
    if (win_x11_init_vdpau_procs(ctx, false) < 0) {
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

// Check whether vdpau display preemption happened. The caller provides a
// preemption counter, which contains the logical timestamp of the last
// preemption handled by the caller. The counter can be 0 for init.
// If counter is NULL, only ever return -1 or 1.
// Return values:
//  -1: the display is currently preempted, and vdpau can't be used
//   0: a preemption event happened, and the caller must recover
//      (*counter is updated, and a second call will report status ok)
//   1: everything is fine, no preemption happened
int mp_vdpau_handle_preemption(struct mp_vdpau_ctx *ctx, uint64_t *counter)
{
    int r = 1;
    pthread_mutex_lock(&ctx->preempt_lock);

    const void *p[4] = {&(uint32_t){0}};
    uint32_t stride[4] = {4};
    VdpRect rc = {0};
    ctx->vdp.output_surface_put_bits_native(ctx->preemption_obj, p, stride, &rc);

    // First time init
    if (counter && !*counter)
        *counter = ctx->preemption_counter;

    if (handle_preemption(ctx) < 0)
        r = -1;

    if (counter && r > 0 && *counter < ctx->preemption_counter) {
        *counter = ctx->preemption_counter;
        r = 0; // signal recovery after preemption
    }

    pthread_mutex_unlock(&ctx->preempt_lock);
    return r;
}

struct surface_ref {
    struct mp_vdpau_ctx *ctx;
    int index;
};

static void release_decoder_surface(void *ptr)
{
    struct surface_ref *r = ptr;
    struct mp_vdpau_ctx *ctx = r->ctx;

    pthread_mutex_lock(&ctx->pool_lock);
    assert(ctx->video_surfaces[r->index].in_use);
    ctx->video_surfaces[r->index].in_use = false;
    pthread_mutex_unlock(&ctx->pool_lock);

    talloc_free(r);
}

static struct mp_image *create_ref(struct mp_vdpau_ctx *ctx, int index)
{
    struct surface_entry *e = &ctx->video_surfaces[index];
    assert(!e->in_use);
    e->in_use = true;
    e->age = ctx->age_counter++;
    struct surface_ref *ref = talloc_ptrtype(NULL, ref);
    *ref = (struct surface_ref){ctx, index};
    struct mp_image *res =
        mp_image_new_custom_ref(NULL, ref, release_decoder_surface);
    if (res) {
        mp_image_setfmt(res, e->rgb ? IMGFMT_VDPAU_OUTPUT : IMGFMT_VDPAU);
        mp_image_set_size(res, e->w, e->h);
        res->planes[0] = (void *)"dummy"; // must be non-NULL, otherwise arbitrary
        res->planes[3] = (void *)(intptr_t)(e->rgb ? e->osurface : e->surface);
    }
    return res;
}

static struct mp_image *mp_vdpau_get_surface(struct mp_vdpau_ctx *ctx,
                                             VdpChromaType chroma,
                                             VdpRGBAFormat rgb_format,
                                             bool rgb, int w, int h)
{
    struct vdp_functions *vdp = &ctx->vdp;
    int surface_index = -1;
    VdpStatus vdp_st;

    if (rgb) {
        chroma = (VdpChromaType)-1;
    } else {
        rgb_format = (VdpChromaType)-1;
    }

    pthread_mutex_lock(&ctx->pool_lock);

    // Destroy all unused surfaces that don't have matching parameters
    for (int n = 0; n < MAX_VIDEO_SURFACES; n++) {
        struct surface_entry *e = &ctx->video_surfaces[n];
        if (!e->in_use && e->allocated) {
            if (e->w != w || e->h != h || e->rgb != rgb ||
                e->chroma != chroma || e->rgb_format != rgb_format)
            {
                if (e->rgb) {
                    vdp_st = vdp->output_surface_destroy(e->osurface);
                } else {
                    vdp_st = vdp->video_surface_destroy(e->surface);
                }
                CHECK_VDP_WARNING(ctx, "Error when destroying surface");
                e->surface = e->osurface = VDP_INVALID_HANDLE;
                e->allocated = false;
            }
        }
    }

    // Try to find an existing unused surface
    for (int n = 0; n < MAX_VIDEO_SURFACES; n++) {
        struct surface_entry *e = &ctx->video_surfaces[n];
        if (!e->in_use && e->allocated) {
            assert(e->w == w && e->h == h);
            assert(e->chroma == chroma);
            assert(e->rgb_format == rgb_format);
            assert(e->rgb == rgb);
            if (surface_index >= 0) {
                struct surface_entry *other = &ctx->video_surfaces[surface_index];
                if (other->age < e->age)
                    continue;
            }
            surface_index = n;
        }
    }

    if (surface_index >= 0)
        goto done;

    // Allocate new surface
    for (int n = 0; n < MAX_VIDEO_SURFACES; n++) {
        struct surface_entry *e = &ctx->video_surfaces[n];
        if (!e->in_use) {
            assert(e->surface == VDP_INVALID_HANDLE);
            assert(e->osurface == VDP_INVALID_HANDLE);
            assert(!e->allocated);
            e->chroma = chroma;
            e->rgb_format = rgb_format;
            e->rgb = rgb;
            e->w = w;
            e->h = h;
            if (mp_vdpau_handle_preemption(ctx, NULL) >= 0) {
                if (rgb) {
                    vdp_st = vdp->output_surface_create(ctx->vdp_device, rgb_format,
                                                        w, h, &e->osurface);
                    e->allocated = e->osurface != VDP_INVALID_HANDLE;
                } else {
                    vdp_st = vdp->video_surface_create(ctx->vdp_device, chroma,
                                                    w, h, &e->surface);
                    e->allocated = e->surface != VDP_INVALID_HANDLE;
                }
                CHECK_VDP_WARNING(ctx, "Error when allocating surface");
            } else {
                e->allocated = false;
                e->osurface = VDP_INVALID_HANDLE;
                e->surface = VDP_INVALID_HANDLE;
            }
            surface_index = n;
            goto done;
        }
    }

done: ;
    struct mp_image *mpi = NULL;
    if (surface_index >= 0)
        mpi = create_ref(ctx, surface_index);

    pthread_mutex_unlock(&ctx->pool_lock);

    if (!mpi)
        MP_ERR(ctx, "no surfaces available in mp_vdpau_get_video_surface\n");
    return mpi;
}

struct mp_image *mp_vdpau_get_video_surface(struct mp_vdpau_ctx *ctx,
                                            VdpChromaType chroma, int w, int h)
{
    return mp_vdpau_get_surface(ctx, chroma, 0, false, w, h);
}

static void free_device_ref(struct AVHWDeviceContext *hwctx)
{
    struct mp_vdpau_ctx *ctx = hwctx->user_opaque;

    struct vdp_functions *vdp = &ctx->vdp;
    VdpStatus vdp_st;

    for (int i = 0; i < MAX_VIDEO_SURFACES; i++) {
        // can't hold references past context lifetime
        assert(!ctx->video_surfaces[i].in_use);
        if (ctx->video_surfaces[i].surface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->video_surface_destroy(ctx->video_surfaces[i].surface);
            CHECK_VDP_WARNING(ctx, "Error when calling vdp_video_surface_destroy");
        }
        if (ctx->video_surfaces[i].osurface != VDP_INVALID_HANDLE) {
            vdp_st = vdp->output_surface_destroy(ctx->video_surfaces[i].osurface);
            CHECK_VDP_WARNING(ctx, "Error when calling vdp_output_surface_destroy");
        }
    }

    if (ctx->preemption_obj != VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_destroy(ctx->preemption_obj);
        CHECK_VDP_WARNING(ctx, "Error when calling vdp_output_surface_destroy");
    }

    if (vdp->device_destroy && ctx->vdp_device != VDP_INVALID_HANDLE) {
        vdp_st = vdp->device_destroy(ctx->vdp_device);
        CHECK_VDP_WARNING(ctx, "Error when calling vdp_device_destroy");
    }

    if (ctx->close_display)
        XCloseDisplay(ctx->x11);

    pthread_mutex_destroy(&ctx->pool_lock);
    pthread_mutex_destroy(&ctx->preempt_lock);
    talloc_free(ctx);
}

struct mp_vdpau_ctx *mp_vdpau_create_device_x11(struct mp_log *log, Display *x11,
                                                bool probing)
{
    AVBufferRef *avref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VDPAU);
    if (!avref)
        return NULL;

    AVHWDeviceContext *hwctx = (void *)avref->data;
    AVVDPAUDeviceContext *vdctx = hwctx->hwctx;

    struct mp_vdpau_ctx *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mp_vdpau_ctx) {
        .log = log,
        .x11 = x11,
        .preemption_counter = 1,
        .av_device_ref = avref,
        .hwctx = {
            .av_device_ref = avref,
        },
    };
    mpthread_mutex_init_recursive(&ctx->preempt_lock);
    pthread_mutex_init(&ctx->pool_lock, NULL);

    hwctx->free = free_device_ref;
    hwctx->user_opaque = ctx;

    mark_vdpau_objects_uninitialized(ctx);

    if (win_x11_init_vdpau_procs(ctx, probing) < 0) {
        mp_vdpau_destroy(ctx);
        return NULL;
    }

    vdctx->device = ctx->vdp_device;
    vdctx->get_proc_address = ctx->get_proc_address;

    if (av_hwdevice_ctx_init(ctx->av_device_ref) < 0) {
        mp_vdpau_destroy(ctx);
        return NULL;
    }

    return ctx;
}

void mp_vdpau_destroy(struct mp_vdpau_ctx *ctx)
{
    if (!ctx)
        return;

    AVBufferRef *ref = ctx->av_device_ref;
    av_buffer_unref(&ref); // frees ctx as well
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

bool mp_vdpau_get_rgb_format(int imgfmt, VdpRGBAFormat *out_rgba_format)
{
    VdpRGBAFormat format = (VdpRGBAFormat)-1;

    switch (imgfmt) {
    case IMGFMT_BGRA:
        format = VDP_RGBA_FORMAT_B8G8R8A8; break;
    default:
        return false;
    }

    if (out_rgba_format)
        *out_rgba_format = format;
    return true;
}

// Use mp_vdpau_get_video_surface, and upload mpi to it. Return NULL on failure.
// If the image is already a vdpau video surface, just return a reference.
struct mp_image *mp_vdpau_upload_video_surface(struct mp_vdpau_ctx *ctx,
                                               struct mp_image *mpi)
{
    struct vdp_functions *vdp = &ctx->vdp;
    VdpStatus vdp_st;

    if (mpi->imgfmt == IMGFMT_VDPAU || mpi->imgfmt == IMGFMT_VDPAU_OUTPUT)
        return mp_image_new_ref(mpi);

    VdpChromaType chroma = (VdpChromaType)-1;
    VdpYCbCrFormat ycbcr = (VdpYCbCrFormat)-1;
    VdpRGBAFormat rgbafmt = (VdpRGBAFormat)-1;
    bool rgb = !mp_vdpau_get_format(mpi->imgfmt, &chroma, &ycbcr);
    if (rgb && !mp_vdpau_get_rgb_format(mpi->imgfmt, &rgbafmt))
        return NULL;

    struct mp_image *hwmpi =
        mp_vdpau_get_surface(ctx, chroma, rgbafmt, rgb, mpi->w, mpi->h);
    if (!hwmpi)
        return NULL;

    struct mp_image *src = mpi;
    if (mpi->stride[0] < 0)
        src = mp_image_new_copy(mpi); // unflips it when copying

    if (hwmpi->imgfmt == IMGFMT_VDPAU) {
        VdpVideoSurface surface = (intptr_t)hwmpi->planes[3];
        const void *destdata[3] = {src->planes[0], src->planes[2], src->planes[1]};
        if (src->imgfmt == IMGFMT_NV12)
            destdata[1] = destdata[2];
        vdp_st = vdp->video_surface_put_bits_y_cb_cr(surface,
            ycbcr, destdata, src->stride);
    } else {
        VdpOutputSurface rgb_surface = (intptr_t)hwmpi->planes[3];
        vdp_st = vdp->output_surface_put_bits_native(rgb_surface,
                                    &(const void *){src->planes[0]},
                                    &(uint32_t){src->stride[0]},
                                    NULL);
    }
    CHECK_VDP_WARNING(ctx, "Error when uploading surface");

    if (src != mpi)
        talloc_free(src);

    mp_image_copy_attributes(hwmpi, mpi);
    return hwmpi;
}

bool mp_vdpau_guess_if_emulated(struct mp_vdpau_ctx *ctx)
{
    struct vdp_functions *vdp = &ctx->vdp;
    VdpStatus vdp_st;
    char const* info = NULL;
    vdp_st = vdp->get_information_string(&info);
    CHECK_VDP_WARNING(ctx, "Error when calling vdp_get_information_string");
    return vdp_st == VDP_STATUS_OK && info && strstr(info, "VAAPI");
}

// (This clearly works only for contexts wrapped by our code.)
struct mp_vdpau_ctx *mp_vdpau_get_ctx_from_av(AVBufferRef *hw_device_ctx)
{
    AVHWDeviceContext *hwctx = (void *)hw_device_ctx->data;

    if (hwctx->free != free_device_ref)
        return NULL; // not ours

    return hwctx->user_opaque;
}

static bool is_emulated(struct AVBufferRef *hw_device_ctx)
{
    struct mp_vdpau_ctx *ctx = mp_vdpau_get_ctx_from_av(hw_device_ctx);
    if (!ctx)
        return false;

    return mp_vdpau_guess_if_emulated(ctx);
}

static struct AVBufferRef *vdpau_create_standalone(struct mpv_global *global,
        struct mp_log *log, struct hwcontext_create_dev_params *params)
{
    XInitThreads();

    Display *display = XOpenDisplay(NULL);
    if (!display)
        return NULL;

    struct mp_vdpau_ctx *vdp =
        mp_vdpau_create_device_x11(log, display, params->probing);
    if (!vdp) {
        XCloseDisplay(display);
        return NULL;
    }

    vdp->close_display = true;
    return vdp->hwctx.av_device_ref;
}

const struct hwcontext_fns hwcontext_fns_vdpau = {
    .av_hwdevice_type = AV_HWDEVICE_TYPE_VDPAU,
    .create_dev = vdpau_create_standalone,
    .is_emulated = is_emulated,
};
