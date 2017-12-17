/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include <libavutil/hwcontext_drm.h>

#include "common.h"
#include "video/hwdec.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "libmpv/opengl_cb.h"
#include "video/out/drm_common.h"
#include "video/out/drm_prime.h"
#include "video/out/gpu/hwdec.h"
#include "video/mp_image.h"

#include "ra_gl.h"

extern const struct m_sub_options drm_conf;

struct drm_frame {
    struct drm_prime_framebuffer fb;
    struct mp_image *image; // associated mpv image
};

struct priv {
    struct mp_log *log;

    struct mp_image_params params;

    struct drm_atomic_context *ctx;
    struct drm_frame current_frame, old_frame;

    struct mp_rect src, dst;

    int display_w, display_h;
};

static void set_current_frame(struct ra_hwdec *hw, struct drm_frame *frame)
{
    struct priv *p = hw->priv;

    // frame will be on screen after next vsync
    // current_frame is currently the displayed frame and will be replaced
    // by frame after next vsync.
    // We used old frame as triple buffering to make sure that the drm framebuffer
    // is not being displayed when we release it.

    if (p->ctx) {
        drm_prime_destroy_framebuffer(p->log, p->ctx->fd, &p->old_frame.fb);
    }

    mp_image_setrefp(&p->old_frame.image, p->current_frame.image);
    p->old_frame.fb = p->current_frame.fb;

    if (frame) {
        p->current_frame.fb = frame->fb;
        mp_image_setrefp(&p->current_frame.image, frame->image);
    } else {
        memset(&p->current_frame.fb, 0, sizeof(p->current_frame.fb));
        mp_image_setrefp(&p->current_frame.image, NULL);
    }
}

static void scale_dst_rect(struct ra_hwdec *hw, int source_w, int source_h ,struct mp_rect *src, struct mp_rect *dst)
{
    struct priv *p = hw->priv;
    double hratio, vratio, ratio;

    // drm can allow to have a layer that has a different size from framebuffer
    // we scale here the destination size to video mode
    hratio = vratio = ratio = 1.0;

    hratio = (double)p->display_w / (double)source_w;
    vratio = (double)p->display_h / (double)source_h;
    ratio = hratio <= vratio ? hratio : vratio;

    dst->x0 = src->x0 * ratio;
    dst->x1 = src->x1 * ratio;
    dst->y0 = src->y0 * ratio;
    dst->y1 = src->y1 * ratio;

    int offset_x = (p->display_w - ratio * source_w) / 2;
    int offset_y = (p->display_h - ratio * source_h) / 2;

    dst->x0 += offset_x;
    dst->x1 += offset_x;
    dst->y0 += offset_y;
    dst->y1 += offset_y;
}

static int overlay_frame(struct ra_hwdec *hw, struct mp_image *hw_image,
                         struct mp_rect *src, struct mp_rect *dst, bool newframe)
{
    struct priv *p = hw->priv;
    GL *gl = ra_gl_get(hw->ra);
    AVDRMFrameDescriptor *desc = NULL;
    drmModeAtomicReq *request = NULL;
    struct drm_frame next_frame = {0};
    int ret;

    if (hw_image) {

        // grab opengl-cb windowing info to eventually upscale the overlay
        // as egl windows could be upscaled to primary plane.
        struct mpv_opengl_cb_window_pos *glparams =
                gl ? (struct mpv_opengl_cb_window_pos *)
                mpgl_get_native_display(gl, "opengl-cb-window-pos") : NULL;
        if (glparams) {
            scale_dst_rect(hw, glparams->width, glparams->height, dst, &p->dst);
        } else {
            p->dst = *dst;
        }
        p->src = *src;

        // grab drm interop info
        struct mpv_opengl_cb_drm_params *drmparams =
                gl ? (struct mpv_opengl_cb_drm_params *)
                mpgl_get_native_display(gl, "opengl-cb-drm-params") : NULL;
        if (drmparams)
            request = (drmModeAtomicReq *)drmparams->atomic_request;

        next_frame.image = hw_image;
        desc = (AVDRMFrameDescriptor *)hw_image->planes[0];

        if (desc) {
            int srcw = p->src.x1 - p->src.x0;
            int srch = p->src.y1 - p->src.y0;
            int dstw = MP_ALIGN_UP(p->dst.x1 - p->dst.x0, 2);
            int dsth = MP_ALIGN_UP(p->dst.y1 - p->dst.y0, 2);

            if (drm_prime_create_framebuffer(p->log, p->ctx->fd, desc, srcw, srch, &next_frame.fb)) {
                ret = -1;
                goto fail;
            }

            if (request) {
                drm_object_set_property(request, p->ctx->overlay_plane, "FB_ID", next_frame.fb.fb_id);
                drm_object_set_property(request,  p->ctx->overlay_plane, "CRTC_ID", p->ctx->crtc->id);
                drm_object_set_property(request,  p->ctx->overlay_plane, "SRC_X",   p->src.x0 << 16);
                drm_object_set_property(request,  p->ctx->overlay_plane, "SRC_Y",   p->src.y0 << 16);
                drm_object_set_property(request,  p->ctx->overlay_plane, "SRC_W",   srcw << 16);
                drm_object_set_property(request,  p->ctx->overlay_plane, "SRC_H",   srch << 16);
                drm_object_set_property(request,  p->ctx->overlay_plane, "CRTC_X",  MP_ALIGN_DOWN(p->dst.x0, 2));
                drm_object_set_property(request,  p->ctx->overlay_plane, "CRTC_Y",  MP_ALIGN_DOWN(p->dst.y0, 2));
                drm_object_set_property(request,  p->ctx->overlay_plane, "CRTC_W",  dstw);
                drm_object_set_property(request,  p->ctx->overlay_plane, "CRTC_H",  dsth);
                drm_object_set_property(request,  p->ctx->overlay_plane, "ZPOS",    0);
            } else {
                ret = drmModeSetPlane(p->ctx->fd, p->ctx->overlay_plane->id, p->ctx->crtc->id, next_frame.fb.fb_id, 0,
                                      MP_ALIGN_DOWN(p->dst.x0, 2), MP_ALIGN_DOWN(p->dst.y0, 2), dstw, dsth,
                                      p->src.x0 << 16, p->src.y0 << 16 , srcw << 16, srch << 16);
                if (ret < 0) {
                    MP_ERR(hw, "Failed to set the plane %d (buffer %d).\n", p->ctx->overlay_plane->id,
                                next_frame.fb.fb_id);
                    goto fail;
                }
            }
        }
    }

    set_current_frame(hw, &next_frame);
    return 0;

 fail:
    drm_prime_destroy_framebuffer(p->log, p->ctx->fd, &next_frame.fb);
    return ret;
}

static void uninit(struct ra_hwdec *hw)
{
    struct priv *p = hw->priv;

    set_current_frame(hw, NULL);

    if (p->ctx) {
        drm_atomic_destroy_context(p->ctx);
        p->ctx = NULL;
    }
}

static int init(struct ra_hwdec *hw)
{
    struct priv *p = hw->priv;
    int drm_overlay;

    if (!ra_is_gl(hw->ra))
        return -1;

    p->log = hw->log;

    void *tmp = talloc_new(NULL);
    struct drm_opts *opts = mp_get_config_group(tmp, hw->global, &drm_conf);
    drm_overlay = opts->drm_overlay_id;
    talloc_free(tmp);

    GL *gl = ra_gl_get(hw->ra);
    struct mpv_opengl_cb_drm_params *params =
            gl ? (struct mpv_opengl_cb_drm_params *)
            mpgl_get_native_display(gl, "opengl-cb-drm-params") : NULL;
    if (!params) {
        MP_VERBOSE(hw, "Could not get drm interop info.\n");
        goto err;
    }

    if (params->fd) {
        p->ctx = drm_atomic_create_context(p->log, params->fd, params->crtc_id,
                                           drm_overlay);
        if (!p->ctx) {
            mp_err(p->log, "Failed to retrieve DRM atomic context.\n");
            goto err;
        }
    } else {
        mp_err(p->log, "Failed to retrieve DRM fd from native display.\n");
        goto err;
    }

    drmModeCrtcPtr crtc;
    crtc = drmModeGetCrtc(p->ctx->fd, p->ctx->crtc->id);
    if (crtc) {
        p->display_w = crtc->mode.hdisplay;
        p->display_h = crtc->mode.vdisplay;
        drmModeFreeCrtc(crtc);
    }


    uint64_t has_prime;
    if (drmGetCap(p->ctx->fd, DRM_CAP_PRIME, &has_prime) < 0) {
        MP_ERR(hw, "Card does not support prime handles.\n");
        goto err;
    }

    return 0;

err:
    uninit(hw);
    return -1;
}

const struct ra_hwdec_driver ra_hwdec_drmprime_drm = {
    .name = "drmprime-drm",
    .priv_size = sizeof(struct priv),
    .imgfmts = {IMGFMT_DRMPRIME, 0},
    .init = init,
    .overlay_frame = overlay_frame,
    .uninit = uninit,
};
