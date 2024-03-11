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
#include <poll.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <libswscale/swscale.h>

#include "common/msg.h"
#include "drm_atomic.h"
#include "drm_common.h"
#include "osdep/timer.h"
#include "sub/osd.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/out/present_sync.h"
#include "video/sws_utils.h"
#include "vo.h"

#define IMGFMT_XRGB8888 IMGFMT_BGR0
#define IMGFMT_XBGR8888 IMGFMT_RGB0
#define IMGFMT_XRGB2101010 \
    pixfmt2imgfmt(MP_SELECT_LE_BE(AV_PIX_FMT_X2RGB10LE, AV_PIX_FMT_X2RGB10BE))
#define IMGFMT_XBGR2101010 \
    pixfmt2imgfmt(MP_SELECT_LE_BE(AV_PIX_FMT_X2BGR10LE, AV_PIX_FMT_X2BGR10BE))

#define BYTES_PER_PIXEL 4
#define BITS_PER_PIXEL 32

struct drm_frame {
    struct framebuffer *fb;
};

struct priv {
    struct drm_frame **fb_queue;
    unsigned int fb_queue_len;

    uint32_t drm_format;
    enum mp_imgfmt imgfmt;

    struct mp_image *last_input;
    struct mp_image *cur_frame;
    struct mp_image *cur_frame_cropped;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
    struct mp_sws_context *sws;

    struct framebuffer **bufs;
    int front_buf;
    int buf_count;
};

static void destroy_framebuffer(int fd, struct framebuffer *fb)
{
    if (!fb)
        return;

    if (fb->map) {
        munmap(fb->map, fb->size);
    }
    if (fb->id) {
        drmModeRmFB(fd, fb->id);
    }
    if (fb->handle) {
        struct drm_mode_destroy_dumb dreq = {
            .handle = fb->handle,
        };
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
    }
}

static struct framebuffer *setup_framebuffer(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct vo_drm_state *drm = vo->drm;

    struct framebuffer *fb = talloc_zero(drm, struct framebuffer);
    fb->width = drm->mode.mode.hdisplay;
    fb->height = drm->mode.mode.vdisplay;
    fb->fd = drm->fd;
    fb->handle = 0;

    // create dumb buffer
    struct drm_mode_create_dumb creq = {
        .width = fb->width,
        .height = fb->height,
        .bpp = BITS_PER_PIXEL,
    };

    if (drmIoctl(drm->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        MP_ERR(vo, "Cannot create dumb buffer: %s\n", mp_strerror(errno));
        goto err;
    }

    fb->stride = creq.pitch;
    fb->size = creq.size;
    fb->handle = creq.handle;

    // select format
    switch (drm->opts->drm_format) {
    case DRM_OPTS_FORMAT_XRGB2101010:
        p->drm_format = DRM_FORMAT_XRGB2101010;
        p->imgfmt = IMGFMT_XRGB2101010;
        break;
    case DRM_OPTS_FORMAT_XBGR2101010:
        p->drm_format = DRM_FORMAT_XRGB2101010;
        p->imgfmt = IMGFMT_XRGB2101010;
        break;
    case DRM_OPTS_FORMAT_XBGR8888:
        p->drm_format = DRM_FORMAT_XBGR8888;
        p->imgfmt = IMGFMT_XBGR8888;
        break;
    default:
        if (drm->opts->drm_format != DRM_OPTS_FORMAT_XRGB8888) {
            MP_VERBOSE(vo, "Requested format not supported by VO, "
                       "falling back to xrgb8888\n");
        }
        p->drm_format = DRM_FORMAT_XRGB8888;
        p->imgfmt = IMGFMT_XRGB8888;
        break;
    }

    // create framebuffer object for the dumb-buffer
    int ret = drmModeAddFB2(fb->fd, fb->width, fb->height,
                            p->drm_format,
                            (uint32_t[4]){fb->handle, 0, 0, 0},
                            (uint32_t[4]){fb->stride, 0, 0, 0},
                            (uint32_t[4]){0, 0, 0, 0},
                            &fb->id, 0);
    if (ret) {
        MP_ERR(vo, "Cannot create framebuffer: %s\n", mp_strerror(errno));
        goto err;
    }

    // prepare buffer for memory mapping
    struct drm_mode_map_dumb mreq = {
        .handle = fb->handle,
    };
    if (drmIoctl(drm->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
        MP_ERR(vo, "Cannot map dumb buffer: %s\n", mp_strerror(errno));
        goto err;
    }

    // perform actual memory mapping
    fb->map = mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    drm->fd, mreq.offset);
    if (fb->map == MAP_FAILED) {
        MP_ERR(vo, "Cannot map dumb buffer: %s\n", mp_strerror(errno));
        goto err;
    }

    memset(fb->map, 0, fb->size);
    return fb;

err:
    destroy_framebuffer(drm->fd, fb);
    return NULL;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;
    struct vo_drm_state *drm = vo->drm;

    vo->dwidth =drm->fb->width;
    vo->dheight = drm->fb->height;
    vo_get_src_dst_rects(vo, &p->src, &p->dst, &p->osd);

    int w = p->dst.x1 - p->dst.x0;
    int h = p->dst.y1 - p->dst.y0;

    p->sws->src = *params;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = p->imgfmt,
        .w = w,
        .h = h,
        .p_w = 1,
        .p_h = 1,
    };

    talloc_free(p->cur_frame);
    p->cur_frame = mp_image_alloc(p->imgfmt, drm->fb->width, drm->fb->height);
    mp_image_params_guess_csp(&p->sws->dst);
    mp_image_set_params(p->cur_frame, &p->sws->dst);
    mp_image_set_size(p->cur_frame, drm->fb->width, drm->fb->height);

    talloc_free(p->cur_frame_cropped);
    p->cur_frame_cropped = mp_image_new_dummy_ref(p->cur_frame);
    mp_image_crop_rc(p->cur_frame_cropped, p->dst);

    talloc_free(p->last_input);
    p->last_input = NULL;

    if (mp_sws_reinit(p->sws) < 0)
        return -1;

    mp_mutex_lock(&vo->params_mutex);
    vo->target_params = &p->sws->dst; // essentially constant, so this is okay
    mp_mutex_unlock(&vo->params_mutex);
    vo->want_redraw = true;
    return 0;
}

static struct framebuffer *get_new_fb(struct vo *vo)
{
    struct priv *p = vo->priv;

    p->front_buf++;
    p->front_buf %= p->buf_count;

    return p->bufs[p->front_buf];
}

static void draw_image(struct vo *vo, mp_image_t *mpi, struct framebuffer *buf)
{
    struct priv *p = vo->priv;
    struct vo_drm_state *drm = vo->drm;

    if (drm->active && buf != NULL) {
        if (mpi) {
            struct mp_image src = *mpi;
            struct mp_rect src_rc = p->src;
            src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, mpi->fmt.align_x);
            src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, mpi->fmt.align_y);
            mp_image_crop_rc(&src, src_rc);

            mp_image_clear(p->cur_frame, 0, 0, p->cur_frame->w, p->dst.y0);
            mp_image_clear(p->cur_frame, 0, p->dst.y1, p->cur_frame->w, p->cur_frame->h);
            mp_image_clear(p->cur_frame, 0, p->dst.y0, p->dst.x0, p->dst.y1);
            mp_image_clear(p->cur_frame, p->dst.x1, p->dst.y0, p->cur_frame->w, p->dst.y1);

            mp_sws_scale(p->sws, p->cur_frame_cropped, &src);
            osd_draw_on_image(vo->osd, p->osd, src.pts, 0, p->cur_frame);
        } else {
            mp_image_clear(p->cur_frame, 0, 0, p->cur_frame->w, p->cur_frame->h);
            osd_draw_on_image(vo->osd, p->osd, 0, 0, p->cur_frame);
        }

        memcpy_pic(buf->map, p->cur_frame->planes[0],
                   p->cur_frame->w * BYTES_PER_PIXEL, p->cur_frame->h,
                   buf->stride,
                   p->cur_frame->stride[0]);
    }

    if (mpi != p->last_input) {
        talloc_free(p->last_input);
        p->last_input = mpi;
    }
}

static void enqueue_frame(struct vo *vo, struct framebuffer *fb)
{
    struct priv *p = vo->priv;

    struct drm_frame *new_frame = talloc(p, struct drm_frame);
    new_frame->fb = fb;
    MP_TARRAY_APPEND(p, p->fb_queue, p->fb_queue_len, new_frame);
}

static void dequeue_frame(struct vo *vo)
{
    struct priv *p = vo->priv;

    talloc_free(p->fb_queue[0]);
    MP_TARRAY_REMOVE_AT(p->fb_queue, p->fb_queue_len, 0);
}

static void swapchain_step(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (p->fb_queue_len > 0) {
        dequeue_frame(vo);
    }
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct vo_drm_state *drm = vo->drm;
    struct priv *p = vo->priv;

    if (!drm->active)
        return;

    drm->still = frame->still;

    // we redraw the entire image when OSD needs to be redrawn
    struct framebuffer *fb =  p->bufs[p->front_buf];
    const bool repeat = frame->repeat && !frame->redraw;
    if (!repeat) {
        fb = get_new_fb(vo);
        draw_image(vo, mp_image_new_ref(frame->current), fb);
    }

    enqueue_frame(vo, fb);
}

static void queue_flip(struct vo *vo, struct drm_frame *frame)
{
    struct vo_drm_state *drm = vo->drm;

    drm->fb = frame->fb;

    int ret = drmModePageFlip(drm->fd, drm->crtc_id,
                              drm->fb->id, DRM_MODE_PAGE_FLIP_EVENT, drm);
    if (ret)
        MP_WARN(vo, "Failed to queue page flip: %s\n", mp_strerror(errno));
    drm->waiting_for_flip = !ret;
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct vo_drm_state *drm = vo->drm;
    const bool drain = drm->paused || drm->still;

    if (!drm->active)
        return;

    while (drain || p->fb_queue_len > vo->opts->swapchain_depth) {
        if (drm->waiting_for_flip) {
            vo_drm_wait_on_flip(vo->drm);
            swapchain_step(vo);
        }
        if (p->fb_queue_len <= 1)
            break;
        if (!p->fb_queue[1] || !p->fb_queue[1]->fb) {
            MP_ERR(vo, "Hole in swapchain?\n");
            swapchain_step(vo);
            continue;
        }
        queue_flip(vo, p->fb_queue[1]);
    }
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct vo_drm_state *drm = vo->drm;
    present_sync_get_info(drm->present, info);
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;

    vo_drm_uninit(vo);

    while (p->fb_queue_len > 0) {
        swapchain_step(vo);
    }

    talloc_free(p->last_input);
    talloc_free(p->cur_frame);
    talloc_free(p->cur_frame_cropped);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (!vo_drm_init(vo))
        goto err;

    struct vo_drm_state *drm = vo->drm;
    p->buf_count = vo->opts->swapchain_depth + 1;
    p->bufs = talloc_zero_array(p, struct framebuffer *, p->buf_count);

    p->front_buf = 0;
    for (int i = 0; i < p->buf_count; i++) {
        p->bufs[i] = setup_framebuffer(vo);
        if (!p->bufs[i])
            goto err;
    }
    drm->fb = p->bufs[0];

    vo->drm->width = vo->drm->fb->width;
    vo->drm->height = vo->drm->fb->height;

    if (!vo_drm_acquire_crtc(vo->drm)) {
        MP_ERR(vo, "Failed to set CRTC for connector %u: %s\n",
               vo->drm->connector->connector_id, mp_strerror(errno));
        goto err;
    }

    vo_drm_set_monitor_par(vo);
    p->sws = mp_sws_alloc(vo);
    p->sws->log = vo->log;
    mp_sws_enable_cmdline_opts(p->sws, vo->global);
    return 0;

err:
    uninit(vo);
    return -1;
}

static int query_format(struct vo *vo, int format)
{
    return sws_isSupportedInput(imgfmt2pixfmt(format));
}

static int control(struct vo *vo, uint32_t request, void *arg)
{
    switch (request) {
    case VOCTRL_SET_PANSCAN:
        if (vo->config_ok)
            reconfig(vo, vo->params);
        return VO_TRUE;
    }

    int events = 0;
    int ret = vo_drm_control(vo, &events, request, arg);
    vo_event(vo, events);
    return ret;
}

const struct vo_driver video_out_drm = {
    .name = "drm",
    .description = "Direct Rendering Manager (software scaling)",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .get_vsync = get_vsync,
    .uninit = uninit,
    .wait_events = vo_drm_wait_events,
    .wakeup = vo_drm_wakeup,
    .priv_size = sizeof(struct priv),
};
