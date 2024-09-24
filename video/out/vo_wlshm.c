/*
 * This file is part of mpv video player.
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

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "osdep/endian.h"
#include "present_sync.h"
#include "sub/osd.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "vo.h"
#include "wayland_common.h"

#define IMGFMT_WL_RGB MP_SELECT_LE_BE(IMGFMT_BGR0, IMGFMT_0RGB)

struct buffer {
    struct vo *vo;
    size_t size;
    struct wl_shm_pool *pool;
    struct wl_buffer *buffer;
    struct mp_image mpi;
    struct buffer *next;
};

struct priv {
    struct mp_sws_context *sws;
    struct buffer *free_buffers;
    struct mp_rect src;
    struct mp_rect dst;
    struct mp_osd_res osd;
};

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
    struct buffer *buf = data;
    struct vo *vo = buf->vo;
    struct priv *p = vo->priv;

    if (buf->mpi.w == vo->dwidth && buf->mpi.h == vo->dheight) {
        buf->next = p->free_buffers;
        p->free_buffers = buf;
    } else {
        talloc_free(buf);
    }
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_handle_release,
};

static void buffer_destroy(void *p)
{
    struct buffer *buf = p;
    wl_buffer_destroy(buf->buffer);
    wl_shm_pool_destroy(buf->pool);
    munmap(buf->mpi.planes[0], buf->size);
}

static struct buffer *buffer_create(struct vo *vo, int width, int height)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    int fd;
    int stride;
    size_t size;
    uint8_t *data;
    struct buffer *buf;

    stride = MP_ALIGN_UP(width * 4, MP_IMAGE_BYTE_ALIGN);
    size = height * stride;
    fd = vo_wayland_allocate_memfd(vo, size);
    if (fd < 0)
        goto error0;
    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
        goto error1;
    buf = talloc_zero(NULL, struct buffer);
    if (!buf)
        goto error2;
    buf->vo = vo;
    buf->size = size;
    mp_image_set_params(&buf->mpi, &p->sws->dst);
    mp_image_set_size(&buf->mpi, width, height);
    buf->mpi.planes[0] = data;
    buf->mpi.stride[0] = stride;
    buf->pool = wl_shm_create_pool(wl->shm, fd, size);
    if (!buf->pool)
        goto error3;
    buf->buffer = wl_shm_pool_create_buffer(buf->pool, 0, width, height,
                                            stride, WL_SHM_FORMAT_XRGB8888);
    if (!buf->buffer)
        goto error4;
    wl_buffer_add_listener(buf->buffer, &buffer_listener, buf);

    close(fd);
    talloc_set_destructor(buf, buffer_destroy);

    return buf;

error4:
    wl_shm_pool_destroy(buf->pool);
error3:
    talloc_free(buf);
error2:
    munmap(data, size);
error1:
    close(fd);
error0:
    return NULL;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct buffer *buf;

    while (p->free_buffers) {
        buf = p->free_buffers;
        p->free_buffers = buf->next;
        talloc_free(buf);
    }
    vo_wayland_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (!vo_wayland_init(vo))
        goto err;
    if (!vo->wl->shm) {
        MP_FATAL(vo->wl, "Compositor doesn't support the %s protocol!\n",
                 wl_shm_interface.name);
        goto err;
    }
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
    struct priv *p = vo->priv;
    return mp_sws_supports_formats(p->sws, IMGFMT_WL_RGB, format) ? 1 : 0;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *p = vo->priv;

    if (!vo_wayland_reconfig(vo))
        return -1;
    p->sws->src = *params;

    return 0;
}

static int resize(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    const int32_t width = mp_rect_w(wl->geometry);
    const int32_t height = mp_rect_h(wl->geometry);

    if (width == 0 || height == 0)
        return 1;

    struct buffer *buf;

    vo_wayland_set_opaque_region(wl, false);
    vo->want_redraw = true;
    vo->dwidth = width;
    vo->dheight = height;
    vo_get_src_dst_rects(vo, &p->src, &p->dst, &p->osd);

    p->sws->dst = (struct mp_image_params) {
        .imgfmt = IMGFMT_WL_RGB,
        .w = width,
        .h = height,
        .p_w = 1,
        .p_h = 1,
    };
    mp_image_params_guess_csp(&p->sws->dst);
    mp_mutex_lock(&vo->params_mutex);
    vo->target_params = &p->sws->dst;
    mp_mutex_unlock(&vo->params_mutex);

    while (p->free_buffers) {
        buf = p->free_buffers;
        p->free_buffers = buf->next;
        talloc_free(buf);
    }

    vo_wayland_handle_scale(wl);

    return mp_sws_reinit(p->sws);
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    switch (request) {
    case VOCTRL_SET_PANSCAN:
        resize(vo);
        return VO_TRUE;
    }

    int events = 0;
    int ret = vo_wayland_control(vo, &events, request, data);

    if (events & VO_EVENT_RESIZE)
        ret = resize(vo);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);
    return ret;
}

static void draw_frame(struct vo *vo, struct vo_frame *frame)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = vo->wl;
    struct mp_image *src = frame->current;
    struct buffer *buf;

    bool render = vo_wayland_check_visible(vo);
    if (!render)
        return;

    vo_wayland_handle_hdr_metadata(wl);

    buf = p->free_buffers;
    if (buf) {
        p->free_buffers = buf->next;
    } else {
        buf = buffer_create(vo, vo->dwidth, vo->dheight);
        if (!buf) {
            wl_surface_attach(wl->surface, NULL, 0, 0);
            return;
        }
    }
    if (src) {
        struct mp_image dst = buf->mpi;
        struct mp_rect src_rc;
        struct mp_rect dst_rc;
        src_rc.x0 = MP_ALIGN_DOWN(p->src.x0, src->fmt.align_x);
        src_rc.y0 = MP_ALIGN_DOWN(p->src.y0, src->fmt.align_y);
        src_rc.x1 = p->src.x1 - (p->src.x0 - src_rc.x0);
        src_rc.y1 = p->src.y1 - (p->src.y0 - src_rc.y0);
        dst_rc.x0 = MP_ALIGN_DOWN(p->dst.x0, dst.fmt.align_x);
        dst_rc.y0 = MP_ALIGN_DOWN(p->dst.y0, dst.fmt.align_y);
        dst_rc.x1 = p->dst.x1 - (p->dst.x0 - dst_rc.x0);
        dst_rc.y1 = p->dst.y1 - (p->dst.y0 - dst_rc.y0);
        mp_image_crop_rc(src, src_rc);
        mp_image_crop_rc(&dst, dst_rc);
        mp_sws_scale(p->sws, &dst, src);
        if (dst_rc.y0 > 0)
            mp_image_clear(&buf->mpi, 0, 0, buf->mpi.w, dst_rc.y0);
        if (buf->mpi.h > dst_rc.y1)
            mp_image_clear(&buf->mpi, 0, dst_rc.y1, buf->mpi.w, buf->mpi.h);
        if (dst_rc.x0 > 0)
            mp_image_clear(&buf->mpi, 0, dst_rc.y0, dst_rc.x0, dst_rc.y1);
        if (buf->mpi.w > dst_rc.x1)
            mp_image_clear(&buf->mpi, dst_rc.x1, dst_rc.y0, buf->mpi.w, dst_rc.y1);
        osd_draw_on_image(vo->osd, p->osd, src->pts, 0, &buf->mpi);
    } else {
        mp_image_clear(&buf->mpi, 0, 0, buf->mpi.w, buf->mpi.h);
        osd_draw_on_image(vo->osd, p->osd, 0, 0, &buf->mpi);
    }
    wl_surface_attach(wl->surface, buf->buffer, 0, 0);
}

static void flip_page(struct vo *vo)
{
    struct vo_wayland_state *wl = vo->wl;

    wl_surface_damage_buffer(wl->surface, 0, 0, vo->dwidth,
                             vo->dheight);
    wl_surface_commit(wl->surface);

    if (!wl->opts->wl_disable_vsync)
        vo_wayland_wait_frame(wl);

    if (wl->use_present)
        present_sync_swap(wl->present);
}

static void get_vsync(struct vo *vo, struct vo_vsync_info *info)
{
    struct vo_wayland_state *wl = vo->wl;
    if (wl->use_present)
        present_sync_get_info(wl->present, info);
}

const struct vo_driver video_out_wlshm = {
    .description = "Wayland SHM video output (software scaling)",
    .name = "wlshm",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_frame = draw_frame,
    .flip_page = flip_page,
    .get_vsync = get_vsync,
    .wakeup = vo_wayland_wakeup,
    .wait_events = vo_wayland_wait_events,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
