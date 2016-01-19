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
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <unistd.h>

#include <libswscale/swscale.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drm_common.h"

#include "common/msg.h"
#include "osdep/timer.h"
#include "sub/osd.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "vo.h"

#define IMGFMT IMGFMT_BGR0
#define BYTES_PER_PIXEL 4
#define BITS_PER_PIXEL 32
#define USE_MASTER 0
#define BUF_COUNT 2

struct framebuffer {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint32_t handle;
    uint8_t *map;
    uint32_t fb;
};

struct priv {
    char *device_path;
    int connector_id;
    int mode_id;

    struct kms *kms;
    drmModeCrtc *old_crtc;
    drmEventContext ev;

    bool vt_switcher_active;
    struct vt_switcher vt_switcher;

    struct framebuffer bufs[BUF_COUNT];
    int front_buf;
    bool active;
    bool pflip_happening;

    int32_t device_w;
    int32_t device_h;
    struct mp_image *last_input;
    struct mp_image *cur_frame;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
    struct mp_sws_context *sws;
};

static void fb_destroy(int fd, struct framebuffer *buf)
{
    if (buf->map) {
        munmap(buf->map, buf->size);
    }
    if (buf->fb) {
        drmModeRmFB(fd, buf->fb);
    }
    if (buf->handle) {
        struct drm_mode_destroy_dumb dreq = {
            .handle = buf->handle,
        };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    }
}

static bool fb_setup_single(struct vo *vo, int fd, struct framebuffer *buf)
{
    buf->handle = 0;

    // create dumb buffer
    struct drm_mode_create_dumb creq = {
        .width = buf->width,
        .height = buf->height,
        .bpp = BITS_PER_PIXEL,
    };
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        MP_ERR(vo, "Cannot create dumb buffer: %s\n", mp_strerror(errno));
        goto err;
    }
    buf->stride = creq.pitch;
    buf->size = creq.size;
    buf->handle = creq.handle;

    // create framebuffer object for the dumb-buffer
    if (drmModeAddFB(fd, buf->width, buf->height, 24, creq.bpp, buf->stride,
                     buf->handle, &buf->fb)) {
        MP_ERR(vo, "Cannot create framebuffer: %s\n", mp_strerror(errno));
        goto err;
    }

    // prepare buffer for memory mapping
    struct drm_mode_map_dumb mreq = {
        .handle = buf->handle,
    };
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
        MP_ERR(vo, "Cannot map dumb buffer: %s\n", mp_strerror(errno));
        goto err;
    }

    // perform actual memory mapping
    buf->map = mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, mreq.offset);
    if (buf->map == MAP_FAILED) {
        MP_ERR(vo, "Cannot map dumb buffer: %s\n", mp_strerror(errno));
        goto err;
    }

    memset(buf->map, 0, buf->size);
    return true;

err:
    fb_destroy(fd, buf);
    return false;
}

static bool fb_setup_double_buffering(struct vo *vo)
{
    struct priv *p = vo->priv;

    p->front_buf = 0;
    for (unsigned int i = 0; i < 2; i++) {
        p->bufs[i].width = p->kms->mode.hdisplay;
        p->bufs[i].height = p->kms->mode.vdisplay;
    }

    for (unsigned int i = 0; i < BUF_COUNT; i++) {
        if (!fb_setup_single(vo, p->kms->fd, &p->bufs[i])) {
            MP_ERR(vo, "Cannot create framebuffer for connector %d\n",
                   p->kms->connector->connector_id);
            for (unsigned int j = 0; j < i; j++) {
                fb_destroy(p->kms->fd, &p->bufs[j]);
            }
            return false;
        }
    }

    return true;
}

static void page_flipped(int fd, unsigned int frame, unsigned int sec,
                                 unsigned int usec, void *data)
{
    struct priv *p = data;
    p->pflip_happening = false;
}

static bool crtc_setup(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->active)
        return true;
    p->old_crtc = drmModeGetCrtc(p->kms->fd, p->kms->crtc_id);
    int ret = drmModeSetCrtc(p->kms->fd, p->kms->crtc_id,
                             p->bufs[p->front_buf + BUF_COUNT - 1].fb,
                             0,
                             0,
                             &p->kms->connector->connector_id,
                             1,
                             &p->kms->mode);
    p->active = true;
    return ret == 0;
}

static void crtc_release(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (!p->active)
        return;
    p->active = false;

    // wait for current page flip
    while (p->pflip_happening) {
        int ret = drmHandleEvent(p->kms->fd, &p->ev);
        if (ret) {
            MP_ERR(vo, "drmHandleEvent failed: %i\n", ret);
            break;
        }
    }

    if (p->old_crtc) {
        drmModeSetCrtc(p->kms->fd,
                       p->old_crtc->crtc_id,
                       p->old_crtc->buffer_id,
                       p->old_crtc->x,
                       p->old_crtc->y,
                       &p->kms->connector->connector_id,
                       1,
                       &p->old_crtc->mode);
        drmModeFreeCrtc(p->old_crtc);
        p->old_crtc = NULL;
    }
}

static void release_vt(void *data)
{
    struct vo *vo = data;
    crtc_release(vo);
    if (USE_MASTER) {
        //this function enables support for switching to x, weston etc.
        //however, for whatever reason, it can be called only by root users.
        //until things change, this is commented.
        struct priv *p = vo->priv;
        if (drmDropMaster(p->kms->fd)) {
            MP_WARN(vo, "Failed to drop DRM master: %s\n", mp_strerror(errno));
        }
    }
}

static void acquire_vt(void *data)
{
    struct vo *vo = data;
    if (USE_MASTER) {
        struct priv *p = vo->priv;
        if (drmSetMaster(p->kms->fd)) {
            MP_WARN(vo, "Failed to acquire DRM master: %s\n", mp_strerror(errno));
        }
    }

    crtc_setup(vo);
}



static int wait_events(struct vo *vo, int64_t until_time_us)
{
    struct priv *p = vo->priv;
    if (p->vt_switcher_active) {
        int64_t wait_us = until_time_us - mp_time_us();
        int timeout_ms = MPCLAMP((wait_us + 500) / 1000, 0, 10000);
        vt_switcher_poll(&p->vt_switcher, timeout_ms);
    }
    return 0;
}

static void wakeup(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (p->vt_switcher_active)
        vt_switcher_interrupt_poll(&p->vt_switcher);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;

    vo->dwidth = p->device_w;
    vo->dheight = p->device_h;
    vo_get_src_dst_rects(vo, &p->src, &p->dst, &p->osd);

    int w = p->dst.x1 - p->dst.x0;
    int h = p->dst.y1 - p->dst.y0;

    // p->osd contains the parameters assuming OSD rendering in window
    // coordinates, but OSD can only be rendered in the intersection
    // between window and video rectangle (i.e. not into panscan borders).
    p->osd.w = w;
    p->osd.h = h;
    p->osd.mt = MPMIN(0, p->osd.mt);
    p->osd.mb = MPMIN(0, p->osd.mb);
    p->osd.mr = MPMIN(0, p->osd.mr);
    p->osd.ml = MPMIN(0, p->osd.ml);

    mp_sws_set_from_cmdline(p->sws, vo->opts->sws_opts);
    p->sws->src = *params;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT,
        .w = w,
        .h = h,
        .p_w = 1,
        .p_h = 1,
    };

    talloc_free(p->cur_frame);
    p->cur_frame = mp_image_alloc(IMGFMT, p->device_w, p->device_h);
    mp_image_params_guess_csp(&p->sws->dst);
    mp_image_set_params(p->cur_frame, &p->sws->dst);

    struct framebuffer *buf = p->bufs;
    for (unsigned int i = 0; i < BUF_COUNT; i++)
        memset(buf[i].map, 0, buf[i].size);

    if (mp_sws_reinit(p->sws) < 0)
        return -1;

    vo->want_redraw = true;
    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;

    if (p->active) {
        if (mpi) {
            struct mp_image src = *mpi;
            struct mp_rect src_rc = p->src;
            src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, mpi->fmt.align_x);
            src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, mpi->fmt.align_y);
            mp_image_crop_rc(&src, src_rc);
            mp_sws_scale(p->sws, p->cur_frame, &src);
            osd_draw_on_image(vo->osd, p->osd, src.pts, 0, p->cur_frame);
        } else {
            mp_image_clear(p->cur_frame, 0, 0, p->cur_frame->w, p->cur_frame->h);
            osd_draw_on_image(vo->osd, p->osd, 0, 0, p->cur_frame);
        }

        struct framebuffer *front_buf = &p->bufs[p->front_buf];
        int w = p->dst.x1 - p->dst.x0;
        int h = p->dst.y1 - p->dst.y0;
        int x = (p->device_w - w) >> 1;
        int y = (p->device_h - h) >> 1;
        int shift = y * front_buf->stride + x * BYTES_PER_PIXEL;
        memcpy_pic(front_buf->map + shift,
                   p->cur_frame->planes[0],
                   w * BYTES_PER_PIXEL,
                   h,
                   front_buf->stride,
                   p->cur_frame->stride[0]);
    }

    if (mpi != p->last_input) {
        talloc_free(p->last_input);
        p->last_input = mpi;
    }
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    if (!p->active || p->pflip_happening)
        return;

    int ret = drmModePageFlip(p->kms->fd, p->kms->crtc_id,
                              p->bufs[p->front_buf].fb,
                              DRM_MODE_PAGE_FLIP_EVENT, p);
    if (ret) {
        MP_WARN(vo, "Cannot flip page for connector\n");
    } else {
        p->front_buf++;
        p->front_buf %= BUF_COUNT;
        p->pflip_happening = true;
    }

    // poll page flip finish event
    const int timeout_ms = 3000;
    struct pollfd fds[1] = {
        { .events = POLLIN, .fd = p->kms->fd },
    };
    poll(fds, 1, timeout_ms);
    if (fds[0].revents & POLLIN) {
        ret = drmHandleEvent(p->kms->fd, &p->ev);
        if (ret != 0) {
            MP_ERR(vo, "drmHandleEvent failed: %i\n", ret);
            return;
        }
    }
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    crtc_release(vo);
    for (unsigned int i = 0; i < BUF_COUNT; i++)
        fb_destroy(p->kms->fd, &p->bufs[i]);

    if (p->kms) {
        kms_destroy(p->kms);
        p->kms = NULL;
    }

    if (p->vt_switcher_active)
        vt_switcher_destroy(&p->vt_switcher);

    talloc_free(p->last_input);
    talloc_free(p->cur_frame);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    p->sws = mp_sws_alloc(vo);
    p->ev.version = DRM_EVENT_CONTEXT_VERSION;
    p->ev.page_flip_handler = page_flipped;

    p->vt_switcher_active = vt_switcher_init(&p->vt_switcher, vo->log);
    if (p->vt_switcher_active) {
        vt_switcher_acquire(&p->vt_switcher, acquire_vt, vo);
        vt_switcher_release(&p->vt_switcher, release_vt, vo);
    } else {
        MP_WARN(vo, "Failed to set up VT switcher. Terminal switching will be unavailable.\n");
    }

    p->kms = kms_create(vo->log);
    if (!p->kms) {
        MP_ERR(vo, "Failed to create KMS.\n");
        goto err;
    }

    if (!kms_setup(p->kms, p->device_path, p->connector_id, p->mode_id)) {
        MP_ERR(vo, "Failed to configure KMS.\n");
        goto err;
    }

    if (!fb_setup_double_buffering(vo)) {
        MP_ERR(vo, "Failed to set up double buffering.\n");
        goto err;
    }

    uint64_t has_dumb;
    if (drmGetCap(p->kms->fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0) {
        MP_ERR(vo, "Device \"%s\" does not support dumb buffers.\n", p->device_path);
        goto err;
    }

    p->device_w = p->bufs[0].width;
    p->device_h = p->bufs[0].height;

    if (!crtc_setup(vo)) {
        MP_ERR(vo,
               "Cannot set CRTC for connector %u: %s\n",
               p->kms->connector->connector_id,
               mp_strerror(errno));
        goto err;
    }

    return 0;

err:
    uninit(vo);
    return -1;
}

static int query_format(struct vo *vo, int format)
{
    return sws_isSupportedInput(imgfmt2pixfmt(format));
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
    case VOCTRL_SCREENSHOT_WIN:
        *(struct mp_image**)data = mp_image_new_copy(p->cur_frame);
        return VO_TRUE;
    case VOCTRL_REDRAW_FRAME:
        draw_image(vo, p->last_input);
        return VO_TRUE;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        if (vo->config_ok)
            reconfig(vo, vo->params);
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_drm = {
    .name = "drm",
    .description = "Direct Rendering Manager",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .wait_events = wait_events,
    .wakeup = wakeup,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING("devpath", device_path, 0),
        OPT_INT("connector", connector_id, 0),
        OPT_INT("mode", mode_id, 0),
        {0},
    },
    .priv_defaults = &(const struct priv) {
        .device_path = "/dev/dri/card0",
        .connector_id = -1,
        .mode_id = 0,
    },
};
