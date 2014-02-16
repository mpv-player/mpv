/*
 * This file is part of mpv video player.
 * Copyright © 2013 Alexander Preisinger <alexander.preisinger@gmail.com>
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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <libavutil/common.h>

#include "config.h"

#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "video/memcpy_pic.h"

#include "sub/osd.h"
#include "sub/img_convert.h"

#include "common/msg.h"

#include "wayland_common.h"
#include "wayland-version.h"

static void draw_image(struct vo *vo, mp_image_t *mpi);

static const struct wl_callback_listener frame_listener;
static const struct wl_buffer_listener buffer_listener;
static const struct wl_shm_listener shm_listener;

struct fmtentry {
    enum wl_shm_format wl_fmt;
    enum mp_imgfmt     mp_fmt;
};

// the first 2 Formats should be available on most platforms
// all other formats are optional
// the waylad byte order is reversed
static const struct fmtentry fmttable[] = {
    {WL_SHM_FORMAT_ARGB8888, IMGFMT_BGRA}, // 8b 8g 8r 8a
    {WL_SHM_FORMAT_XRGB8888, IMGFMT_BGR0},
    {WL_SHM_FORMAT_RGB332,   IMGFMT_BGR8}, // 3b 3g 2r
    {WL_SHM_FORMAT_BGR233,   IMGFMT_RGB8}, // 3r 3g 3b,
    {WL_SHM_FORMAT_XRGB4444, IMGFMT_BGR12_LE}, // 4b 4g 4r 4a
    {WL_SHM_FORMAT_XBGR4444, IMGFMT_RGB12_LE}, // 4r 4g 4b 4a
    {WL_SHM_FORMAT_RGBX4444, IMGFMT_RGB12_BE}, // 4a 4b 4g 4r
    {WL_SHM_FORMAT_BGRX4444, IMGFMT_BGR12_BE}, // 4a 4r 4g 4b
    {WL_SHM_FORMAT_ARGB4444, IMGFMT_BGR12_LE},
    {WL_SHM_FORMAT_ABGR4444, IMGFMT_RGB12_LE},
    {WL_SHM_FORMAT_RGBA4444, IMGFMT_RGB12_BE},
    {WL_SHM_FORMAT_BGRA4444, IMGFMT_BGR12_BE},
    {WL_SHM_FORMAT_XRGB1555, IMGFMT_BGR15_LE}, // 5b 5g 5r 1a
    {WL_SHM_FORMAT_XBGR1555, IMGFMT_RGB15_LE}, // 5r 5g 5b 1a
    {WL_SHM_FORMAT_RGBX5551, IMGFMT_RGB15_BE}, // 1a 5g 5b 5r
    {WL_SHM_FORMAT_BGRX5551, IMGFMT_BGR15_BE}, // 1a 5r 5g 5b
    {WL_SHM_FORMAT_ARGB1555, IMGFMT_BGR15_LE},
    {WL_SHM_FORMAT_ABGR1555, IMGFMT_RGB15_LE},
    {WL_SHM_FORMAT_RGBA5551, IMGFMT_RGB15_BE},
    {WL_SHM_FORMAT_BGRA5551, IMGFMT_BGR15_BE},
    {WL_SHM_FORMAT_RGB565,   IMGFMT_BGR16_LE}, // 5b 6g 5r
    {WL_SHM_FORMAT_BGR565,   IMGFMT_RGB16_LE}, // 5r 6g 5b
    {WL_SHM_FORMAT_RGB888,   IMGFMT_BGR24}, // 8b 8g 8r
    {WL_SHM_FORMAT_BGR888,   IMGFMT_RGB24}, // 8r 8g 8b
    {WL_SHM_FORMAT_XBGR8888, IMGFMT_RGB0},
    {WL_SHM_FORMAT_RGBX8888, IMGFMT_0BGR},
    {WL_SHM_FORMAT_BGRX8888, IMGFMT_0RGB},
    {WL_SHM_FORMAT_ABGR8888, IMGFMT_RGBA},
    {WL_SHM_FORMAT_RGBA8888, IMGFMT_ABGR},
    {WL_SHM_FORMAT_BGRA8888, IMGFMT_ARGB},
};

#define MAX_FORMAT_ENTRIES (sizeof(fmttable) / sizeof(fmttable[0]))
#define DEFAULT_FORMAT_ENTRY 1
#define DEFAULT_ALPHA_FORMAT_ENTRY 0

struct priv;

struct buffer {
    struct wl_buffer *wlbuf;
    bool is_busy;
    bool is_new;
    bool is_attached;
    bool to_resize;
    void *shm_data;
    size_t shm_size;
};

struct buffer_pool {
    struct buffer *buffers;
    struct buffer *front_buffer; // just pointers to any of the buffers
    struct buffer *middle_buffer; // just pointers to any of the buffers
    struct buffer *back_buffer;
    uint32_t buffer_no;
    uint32_t size;
    uint32_t stride;
    uint32_t bytes_per_pixel;
    enum wl_shm_format format; // TODO use fmtentry here
    struct wl_shm *shm;
};

struct supported_format {
    const struct fmtentry *fmt;
    bool is_alpha;
    struct wl_list link;
};

struct priv {
    struct vo *vo;
    struct vo_wayland_state *wl;

    struct wl_list format_list;
    const struct fmtentry *video_format;

    struct mp_rect src;
    struct mp_rect dst;
    int src_w, src_h;
    int dst_w, dst_h;
    struct mp_osd_res osd;

    struct mp_sws_context *sws;
    struct mp_image_params in_format;

    struct wl_callback *redraw_callback;

    struct buffer_pool video_bufpool;
    struct buffer *attached_buffer;

    struct mp_image *original_image;
    int width;  // width of the original image
    int height;

    int x, y; // coords for resizing

    // options
    int enable_alpha;
    int use_rgb565;
    int use_triplebuffering;
};

/* copied from weston clients */
static int set_cloexec_or_close(int fd)
{
    long flags;

    if (fd == -1)
        return -1;

    if ((flags = fcntl(fd, F_GETFD)) == -1)
        goto err;

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
        goto err;

    return fd;

err:
    close(fd);
    return -1;
}

static int create_tmpfile_cloexec(char *tmpname)
{
    int fd;

#ifdef HAVE_MKOSTEMP
    fd = mkostemp(tmpname, O_CLOEXEC);
    if (fd >= 0)
        unlink(tmpname);
#else
    fd = mkstemp(tmpname);
    if (fd >= 0) {
        fd = set_cloexec_or_close(fd);
        unlink(tmpname);
    }
#endif

    return fd;
}

static int os_create_anonymous_file(off_t size)
{
    static const char template[] = "/mpv-temp-XXXXXX";
    const char *path;
    char *name;
    int fd;

    path = getenv("XDG_RUNTIME_DIR");
    if (!path) {
        errno = ENOENT;
        return -1;
    }

    name = malloc(strlen(path) + sizeof(template));
    if (!name)
        return -1;

    strcpy(name, path);
    strcat(name, template);

    fd = create_tmpfile_cloexec(name);

    free(name);

    if (fd < 0)
        return -1;

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static bool is_alpha_format(const struct fmtentry *fmt)
{
    return !!(mp_imgfmt_get_desc(fmt->mp_fmt).flags & MP_IMGFLAG_ALPHA);
}

static const struct fmtentry * is_wayland_format_supported(struct priv *p,
                                                           enum wl_shm_format fmt)
{
    struct supported_format *sf;

    // find the matching format first
    wl_list_for_each(sf, &p->format_list, link) {
        if (sf->fmt->wl_fmt == fmt) {
            return sf->fmt;
        }
    }

    return NULL;
}

// buffer functions

static bool buffer_finalise_back(struct buffer *buf)
{
    buf->is_new = true;
    return true;
}

static bool buffer_finalise_front(struct buffer *buf)
{
    buf->is_new = false; // is_busy is reset on handle_release
    buf->is_busy = true;
    buf->is_attached = true;
    return true;
}

static void buffer_destroy_content(struct buffer *buf)
{
    if (buf->wlbuf) {
        wl_buffer_destroy(buf->wlbuf);
        buf->wlbuf = NULL;
    }
    if (buf->shm_data) {
        munmap(buf->shm_data, buf->shm_size);
        buf->shm_data = NULL;
        buf->shm_size = 0;
    }
}

static bool buffer_create_content(struct buffer_pool *pool,
                                  struct buffer *buf,
                                  int width,
                                  int height)
{
    int fd;
    void *data;
    struct wl_shm_pool *shm_pool;

    fd = os_create_anonymous_file(pool->size);
    if (fd < 0) {
        return false;
    }

    data = mmap(NULL, pool->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    // wl-buffers of the same shm_pool share it's content which might be useful
    // if we resize the buffers (from the docs).
    shm_pool = wl_shm_create_pool(pool->shm, fd, pool->size);
    buf->wlbuf = wl_shm_pool_create_buffer(shm_pool, 0, width, height,
                                           pool->stride, pool->format);
    wl_buffer_add_listener(buf->wlbuf, &buffer_listener, buf);

    wl_shm_pool_destroy(shm_pool);
    close(fd);

    buf->shm_size = pool->size;
    buf->shm_data = data;
    buf->is_new = false;
    buf->is_busy = false;
    return true;
}

static bool buffer_resize(struct buffer_pool *pool, struct buffer *buf,
                          uint32_t width, uint32_t height)
{
    if (buf->is_attached) {
        buf->to_resize = true;
        return true;
    }

    if (buf->shm_size == pool->size)
        return true;

    buf->to_resize = false;
    buffer_destroy_content(buf);
    return buffer_create_content(pool, buf, width, height);
}

static struct mp_image buffer_get_mp_image(struct priv *p,
                                           struct buffer_pool *pool,
                                           struct buffer *buf)
{
    struct mp_image img = {0};
    mp_image_set_params(&img, &p->sws->dst);

    img.planes[0] = buf->shm_data;
    img.stride[0] = pool->stride;

    return img;
}


// buffer pool functions

static void buffer_pool_init(struct priv *p,
                             struct buffer_pool *pool,
                             uint32_t buffer_no,
                             uint32_t width, uint32_t height,
                             const struct fmtentry *fmt,
                             struct wl_shm *shm)
{
    pool->shm = shm;
    pool->buffers = calloc(buffer_no, sizeof(struct buffer));
    pool->buffer_no = buffer_no;
    pool->format = fmt->wl_fmt;
    pool->bytes_per_pixel = mp_imgfmt_get_desc(fmt->mp_fmt).bytes[0];
    pool->stride = FFALIGN(width * pool->bytes_per_pixel, SWS_MIN_BYTE_ALIGN);
    pool->size = pool->stride * height;

    for (uint32_t i = 0; i < buffer_no; ++i)
        buffer_create_content(pool, &pool->buffers[i], width, height);

    if (buffer_no == 3) {
        pool->back_buffer = &pool->buffers[0];
        pool->middle_buffer = &pool->buffers[1];
        pool->front_buffer = &pool->buffers[2];
    }
    else if (buffer_no == 2) {
        pool->back_buffer = &pool->buffers[0];
        pool->front_buffer = &pool->buffers[1];
        pool->middle_buffer = NULL;
    }
    else {
        pool->back_buffer = NULL;
        pool->middle_buffer = NULL;
        pool->front_buffer = NULL;
    }
}

static bool buffer_pool_resize(struct buffer_pool *pool,
                               int width,
                               int height)
{
    bool ret = true;

    pool->stride = FFALIGN(width * pool->bytes_per_pixel, SWS_MIN_BYTE_ALIGN);
    pool->size = pool->stride * height;

    for (uint32_t i = 0; ret && i < pool->buffer_no; ++i)
        ret = buffer_resize(pool, &pool->buffers[i], width, height);

    return ret;
}

static void buffer_pool_destroy(struct buffer_pool *pool)
{
    for (uint32_t i = 0; i < pool->buffer_no; ++i)
        buffer_destroy_content(&pool->buffers[i]);

    free(pool->buffers);
    pool->front_buffer = NULL;
    pool->back_buffer = NULL;
    pool->buffers = NULL;
}

static void buffer_pool_swap(struct buffer_pool *pool)
{
    if (pool->buffer_no == 3) {
        if (pool->back_buffer->is_new) {
            struct buffer *tmp = pool->back_buffer;
            pool->back_buffer = pool->middle_buffer;
            pool->middle_buffer = tmp;
        }
        if (!pool->front_buffer->is_busy && !pool->front_buffer->is_new) {
            struct buffer *tmp = pool->front_buffer;
            pool->front_buffer = pool->middle_buffer;
            pool->middle_buffer = tmp;
        }
    }
    else if (pool->buffer_no == 2) {
        if (pool->back_buffer->is_new) {
            struct buffer *tmp = pool->back_buffer;
            pool->back_buffer = pool->front_buffer;
            pool->front_buffer = tmp;
        }
    }
}

// returns NULL if the back buffer is busy
static struct buffer * buffer_pool_get_back(struct buffer_pool *pool)
{
    if (!pool->back_buffer || pool->back_buffer->is_busy)
        return NULL;

    return pool->back_buffer;
}

// returns NULL if the front buffer is not new
static struct buffer * buffer_pool_get_front(struct buffer_pool *pool)
{
    if (!pool->front_buffer || !pool->front_buffer->is_new)
        return NULL;

    pool->front_buffer->is_busy = true;
    return pool->front_buffer;
}


static bool redraw_frame(struct priv *p)
{
    if (!p->original_image)
        return false;

    draw_image(p->vo, p->original_image);
    return true;
}

static mp_image_t *get_screenshot(struct priv *p)
{
    if (!p->original_image)
        return NULL;

    return mp_image_new_ref(p->original_image);
}

static bool resize(struct priv *p)
{
    struct vo_wayland_state *wl = p->wl;

    int32_t x = wl->window.sh_x;
    int32_t y = wl->window.sh_y;
    wl->vo->dwidth = wl->window.sh_width;
    wl->vo->dheight = wl->window.sh_height;

    vo_get_src_dst_rects(p->vo, &p->src, &p->dst, &p->osd);
    p->src_w = p->src.x1 - p->src.x0;
    p->src_h = p->src.y1 - p->src.y0;
    p->dst_w = p->dst.x1 - p->dst.x0;
    p->dst_h = p->dst.y1 - p->dst.y0;

    MP_DBG(wl, "resizing %dx%d -> %dx%d\n", wl->window.width,
                                            wl->window.height,
                                            p->dst_w,
                                            p->dst_h);

    if (x != 0)
        x = wl->window.width - p->dst_w;

    if (y != 0)
        y = wl->window.height - p->dst_h;

    mp_sws_set_from_cmdline(p->sws);
    p->sws->src = p->in_format;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = p->video_format->mp_fmt,
        .w = p->dst_w,
        .h = p->dst_h,
        .d_w = p->dst_w,
        .d_h = p->dst_h,
    };

    mp_image_params_guess_csp(&p->sws->dst);

    if (mp_sws_reinit(p->sws) < 0)
        return false;

    if (!buffer_pool_resize(&p->video_bufpool, p->dst_w, p->dst_h)) {
        MP_ERR(wl, "failed to resize buffers\n");
        return false;
    }

    wl->window.width = p->dst_w;
    wl->window.height = p->dst_h;

    // if no alpha enabled format is used then create an opaque region to allow
    // the compositor to optimize the drawing of the window
    if (!p->enable_alpha) {
        struct wl_region *opaque =
            wl_compositor_create_region(wl->display.compositor);
        wl_region_add(opaque, 0, 0, p->dst_w, p->dst_h);
        wl_surface_set_opaque_region(wl->window.surface, opaque);
        wl_region_destroy(opaque);
    }

    p->x = x;
    p->y = y;
    p->wl->window.events = 0;
    p->vo->want_redraw = true;
    return true;
}


/* wayland listeners */


static void buffer_handle_release(void *data, struct wl_buffer *buffer)
{
    struct buffer *buf = data;
    buf->is_busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_handle_release
};

static void frame_handle_redraw(void *data,
                                struct wl_callback *callback,
                                uint32_t time)
{
    struct priv *p = data;
    struct vo_wayland_state *wl = p->wl;
    buffer_pool_swap(&p->video_bufpool);
    struct buffer *buf = buffer_pool_get_front(&p->video_bufpool);

    if (buf) {
        wl_surface_attach(wl->window.surface, buf->wlbuf, p->x, p->y);
        wl_surface_damage(wl->window.surface, 0, 0, p->dst_w, p->dst_h);

        if (callback)
            wl_callback_destroy(callback);

        p->redraw_callback = wl_surface_frame(wl->window.surface);
        wl_callback_add_listener(p->redraw_callback, &frame_listener, p);
        wl_surface_commit(wl->window.surface);

        // resize attached buffer
       if (p->attached_buffer) {
            p->attached_buffer->is_attached = false;
            buffer_resize(&p->video_bufpool, p->attached_buffer, p->dst_w, p->dst_h);
        }
        p->attached_buffer = buf;
        buffer_finalise_front(buf);

        p->x = 0;
        p->y = 0;
    }
    else {
        if (callback)
            wl_callback_destroy(callback);

        p->redraw_callback = NULL;
    }
}

static const struct wl_callback_listener frame_listener = {
    frame_handle_redraw
};

static void shm_handle_format(void *data,
                              struct wl_shm *wl_shm,
                              uint32_t format)
{
    struct priv *p = data;
    for (uint32_t i = 0; i < MAX_FORMAT_ENTRIES; ++i) {
        if (fmttable[i].wl_fmt == format) {
            MP_INFO(p->wl, "format %s supported by hw\n",
                    mp_imgfmt_to_name(fmttable[i].mp_fmt));
            struct supported_format *sf = talloc(p, struct supported_format);
            sf->fmt = &fmttable[i];
            sf->is_alpha = is_alpha_format(sf->fmt);
            wl_list_insert(&p->format_list, &sf->link);
        }
    }
}

static const struct wl_shm_listener shm_listener = {
    shm_handle_format
};


/* mpv interface */

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *p = vo->priv;
    struct buffer *buf = buffer_pool_get_back(&p->video_bufpool);

    if (!buf) {
        MP_VERBOSE(p->wl, "can't draw, back buffer is busy\n");
        return;
    }

    if (buf->to_resize) {
        if (buf->is_attached) {
            MP_WARN(p->wl, "resizing attached buffer, use triple-buffering\n");
            buf->is_attached = false;
        }
        buffer_resize(&p->video_bufpool, buf, p->dst_w, p->dst_h);
    }

    struct mp_image src = *mpi;
    struct mp_rect src_rc = p->src;
    src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, src.fmt.align_x);
    src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, src.fmt.align_y);
    mp_image_crop_rc(&src, src_rc);

    struct mp_image img = buffer_get_mp_image(p, &p->video_bufpool, buf);
    mp_sws_scale(p->sws, &img, &src);

    mp_image_setrefp(&p->original_image, mpi);
    buffer_finalise_back(buf);
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *p = vo->priv;
    struct buffer *buf = buffer_pool_get_back(&p->video_bufpool);
    if (buf) {
        struct mp_image img = buffer_get_mp_image(p, &p->video_bufpool, buf);
        osd_draw_on_image(osd, p->osd, osd_get_vo_pts(osd), 0, &img);
    }
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;

    buffer_pool_swap(&p->video_bufpool);

    if (!p->redraw_callback) {
        MP_DBG(p->wl, "restart frame callback\n");
        frame_handle_redraw(p, NULL, 0);
    }
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct priv *p = vo->priv;
    struct supported_format *sf;
    wl_list_for_each_reverse(sf, &p->format_list, link) {
        if (sf->fmt->mp_fmt == format)
            return VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_CSP_SUPPORTED;
    }

    if (mp_sws_supported_format(format))
        return VFCAP_CSP_SUPPORTED;

    return 0;
}

static int reconfig(struct vo *vo, struct mp_image_params *fmt, int flags)
{
    struct priv *p = vo->priv;
    mp_image_unrefp(&p->original_image);

    p->width = fmt->w;
    p->height = fmt->h;
    p->in_format = *fmt;

    struct supported_format *sf;

    // find the matching format first
    wl_list_for_each(sf, &p->format_list, link) {
        if (sf->fmt->mp_fmt == fmt->imgfmt && (p->enable_alpha == sf->is_alpha)) {
            p->video_format = sf->fmt;
            break;
        }
    }

    if (!p->video_format) {
        // if use default is enable overwrite the auto selected one
        if (p->enable_alpha)
            p->video_format = &fmttable[DEFAULT_ALPHA_FORMAT_ENTRY];
        else
            p->video_format = &fmttable[DEFAULT_FORMAT_ENTRY];
    }

    // overides alpha
    // use rgb565 if performance is your main concern
    if (p->use_rgb565) {
        const struct fmtentry *entry =
            is_wayland_format_supported(p, WL_SHM_FORMAT_RGB565);
        if (entry)
            p->video_format = entry;
    }

    buffer_pool_init(p, &p->video_bufpool, (p->use_triplebuffering ? 3 : 2),
            p->width, p->height, p->video_format, p->wl->display.shm);

    vo_wayland_config(vo, vo->dwidth, vo->dheight, flags);

    if (p->wl->window.events & VO_EVENT_RESIZE)
        resize(p);

    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    buffer_pool_destroy(&p->video_bufpool);

    if (p->redraw_callback)
        wl_callback_destroy(p->redraw_callback);

    talloc_free(p->original_image);

    vo_wayland_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (!vo_wayland_init(vo))
        return -1;

    p->vo = vo;
    p->wl = vo->wayland;
    p->sws = mp_sws_alloc(vo);

    wl_list_init(&p->format_list);

    wl_shm_add_listener(p->wl->display.shm, &shm_listener, p);
    wl_display_dispatch(p->wl->display.display);
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        resize(p);
        return VO_TRUE;
    case VOCTRL_REDRAW_FRAME:
        return redraw_frame(p);
    case VOCTRL_WINDOW_TO_OSD_COORDS:
    {
        // OSD is rendered into the scaled image
        float *c = data;
        struct mp_rect *dst = &p->dst;
        c[0] = av_clipf(c[0], dst->x0, dst->x1) - dst->x0;
        c[1] = av_clipf(c[1], dst->y0, dst->y1) - dst->y0;
        return VO_TRUE;
    }
    case VOCTRL_SCREENSHOT:
    {
        struct voctrl_screenshot_args *args = data;
        args->out_image = get_screenshot(p);
        return true;
    }
    }
    int events = 0;
    int r = vo_wayland_control(vo, &events, request, data);

    // NOTE: VO_EVENT_EXPOSE is never returned by the wayland backend
    if (events & VO_EVENT_RESIZE)
        resize(p);

    return r;
}

#define OPT_BASE_STRUCT struct priv
const struct vo_driver video_out_wayland = {
    .description = "Wayland SHM video output",
    .name = "wayland",
    .priv_size = sizeof(struct priv),
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .uninit = uninit,
    .options = (const struct m_option[]) {
        OPT_FLAG("alpha", enable_alpha, 0),
        OPT_FLAG("rgb565", use_rgb565, 0),
        OPT_FLAG("triple-buffering", use_triplebuffering, 0),
        {0}
    },
};

