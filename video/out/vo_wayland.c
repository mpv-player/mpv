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
#include "sub/osd.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "common/msg.h"
#include "video/sws_utils.h"

#include "wayland_common.h"
#include "wayland-version.h"

#define MAX_BUFFERS 2

// minium target version for the new formats (1.2.90)
#define MINIMUM_VERSION (1 << 16 | 2 << 8 | 90)

#define CURRENT_VERSION (WAYLAND_VERSION_MAJOR << 16 | \
                         WAYLAND_VERSION_MINOR << 8  | \
                         WAYLAND_VERSION_MICRO)

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
#if CURRENT_VERSION >= MINIMUM_VERSION
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
#endif
};

#define MAX_FORMAT_ENTRIES (sizeof(fmttable) / sizeof(fmttable[0]))
#define DEFAULT_FORMAT_ENTRY 1
#define DEFAULT_ALPHA_FORMAT_ENTRY 0

struct buffer {
    struct wl_buffer *wlbuf;
    bool is_busy;
    bool is_new;
    void *shm_data;
    size_t shm_size;
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
    const struct fmtentry *pref_format;
    int bytes_per_pixel;
    int stride;

    struct mp_rect src;
    struct mp_rect dst;
    int src_w, src_h;
    int dst_w, dst_h;
    struct mp_osd_res osd;

    struct mp_sws_context *sws;
    struct mp_image_params in_format;

    struct wl_callback *redraw_callback;

    struct buffer buffers[MAX_BUFFERS];
    struct buffer *front_buffer;
    struct buffer *back_buffer;
    struct buffer tmp_buffer;

    struct mp_image *original_image;
    int width;  // width of the original image
    int height;

    int x, y; // coords for resizing
    bool resize_attach;

    // options
    int enable_alpha;
    int use_default;
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

static void buffer_swap(struct priv *p)
{
    if (!p->back_buffer->is_new)
        return;

    struct buffer *tmp = p->back_buffer;
    p->back_buffer = p->front_buffer;
    p->front_buffer = tmp;
}


// returns NULL if the back_buffer contains a new image or if the back buffer
// is busy (unlikely)
static struct buffer * buffer_get_back(struct priv *p)
{
    if (p->back_buffer->is_busy)
        return NULL;

    return p->back_buffer;
}

static bool buffer_finalise_back(struct buffer *buf)
{
    buf->is_new = true;
    return true;
}

static struct buffer * buffer_get_front(struct priv *p)
{
    if (!p->front_buffer->is_new)
        return NULL;

    p->front_buffer->is_busy = true;
    return p->front_buffer;
}

static bool buffer_finalise_front(struct buffer *buf)
{
    buf->is_new = false; // is_busy is reset on handle_release
    return true;
}

static struct mp_image buffer_get_mp_image(struct priv *p, struct buffer *buf)
{
    struct mp_image img = {0};
    mp_image_set_params(&img, &p->sws->dst);

    img.planes[0] = buf->shm_data;
    img.stride[0] = p->stride;

    return img;
}

static bool create_shm_buffer(struct priv *p,
                              struct buffer *buffer,
                              int width,
                              int height,
                              uint32_t format)
{
    struct wl_shm_pool *pool;
    int fd, size;
    void *data;

    p->stride = FFALIGN(width * p->bytes_per_pixel, SWS_MIN_BYTE_ALIGN);
    size = p->stride * height;

    fd = os_create_anonymous_file(size);
    if (fd < 0) {
        MP_ERR(p->wl, "creating a buffer file for %d B failed: %m", size);
        return false;
    }

    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        MP_ERR(p->wl, "mmap failed: %m\n");
        close(fd);
        return false;
    }

    pool = wl_shm_create_pool(p->wl->display.shm, fd, size);
    buffer->wlbuf = wl_shm_pool_create_buffer(pool, 0, width, height,
                                              p->stride, format);
    wl_buffer_add_listener(buffer->wlbuf, &buffer_listener, buffer);
    wl_shm_pool_destroy(pool);
    close(fd);

    buffer->shm_size = size;
    buffer->shm_data = data;
    buffer->is_new = false;
    buffer->is_busy = false;
    return true;
}

static void destroy_shm_buffer(struct buffer *buffer)
{
    if (buffer->wlbuf) {
        wl_buffer_destroy(buffer->wlbuf);
        buffer->wlbuf = NULL;
    }
    if (buffer->shm_data) {
        munmap(buffer->shm_data, buffer->shm_size);
        buffer->shm_data = NULL;
        buffer->shm_size = 0;
    }
}

static bool reinit_shm_buffers(struct priv *p, int width, int height)
{
    bool ret = true;
    enum wl_shm_format fmt = p->pref_format->wl_fmt;
    for (int i = 0; ret && i < MAX_BUFFERS; ++i) {
        destroy_shm_buffer(&p->buffers[i]);
        ret = create_shm_buffer(p, &p->buffers[i], width, height, fmt);
    }
    return ret;
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

    // if the newly resized buffer isn't attached, then don't resize again,
    // because the front buffer might be empty and the temporary buffer might
    // still be valid
    if (p->resize_attach)
        return false;

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
        .imgfmt = p->pref_format->mp_fmt,
        .w = p->dst_w,
        .h = p->dst_h,
        .d_w = p->dst_w,
        .d_h = p->dst_h,
    };

    mp_image_params_guess_csp(&p->sws->dst);

    if (mp_sws_reinit(p->sws) < 0)
        return false;

    // copy pointers
    p->tmp_buffer = *p->front_buffer;
    p->front_buffer->shm_data = NULL;
    p->front_buffer->wlbuf = NULL;

    if (!reinit_shm_buffers(p, p->dst_w, p->dst_h)) {
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
    p->resize_attach = true;
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
    struct buffer *buf = buffer_get_front(p);

    if (buf) {
        if (p->resize_attach) {
            wl_surface_attach(wl->window.surface, buf->wlbuf, p->x, p->y);
            wl_surface_damage(wl->window.surface, 0, 0, p->dst_w, p->dst_h);
            wl_surface_commit(wl->window.surface);

            if (callback)
                wl_callback_destroy(callback);

            p->redraw_callback = NULL;
            buffer_finalise_front(buf);
            p->resize_attach = false;

            destroy_shm_buffer(&p->tmp_buffer);

            // I have to destroy the callback and return early to avoid black flickers
            // I don't exactly know why this, but I guess the back buffer is still
            // empty. The callback loop will be restored on the next flip_page call
            return;
        }

        wl_surface_attach(wl->window.surface, buf->wlbuf, 0, 0);
        wl_surface_damage(wl->window.surface, 0, 0, p->dst_w, p->dst_h);

        if (callback)
            wl_callback_destroy(callback);

        p->redraw_callback = wl_surface_frame(wl->window.surface);
        wl_callback_add_listener(p->redraw_callback, &frame_listener, p);
        wl_surface_commit(wl->window.surface);
        buffer_finalise_front(buf);
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
    for (int i = 0; i < MAX_FORMAT_ENTRIES; ++i) {
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
    struct buffer *buf = buffer_get_back(p);

    if (!buf) {
        MP_WARN(p->wl, "can't draw, back buffer is busy\n");
        return;
    }

    struct mp_image src = *mpi;
    struct mp_rect src_rc = p->src;
    src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, src.fmt.align_x);
    src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, src.fmt.align_y);
    mp_image_crop_rc(&src, src_rc);

    struct mp_image img = buffer_get_mp_image(p, buf);
    mp_sws_scale(p->sws, &img, &src);

    mp_image_setrefp(&p->original_image, mpi);
    buffer_finalise_back(buf);
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *p = vo->priv;
    struct mp_image img = buffer_get_mp_image(p, p->back_buffer);
    osd_draw_on_image(osd, p->osd, osd->vo_pts, 0, &img);
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;

    buffer_swap(p);

    if (!p->redraw_callback) {
        MP_INFO(p->wl, "restart frame callback\n");
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

    p->width = vo->dwidth;
    p->height = vo->dheight;
    p->in_format = *fmt;

    struct supported_format *sf;

    // find the same format first
    wl_list_for_each(sf, &p->format_list, link) {
        if (sf->fmt->mp_fmt == fmt->imgfmt && (p->enable_alpha == sf->is_alpha)) {
            p->pref_format = sf->fmt;
            break;
        }
    }

    // if the format is not supported choose one of the fancy formats next
    // the default formats are always last
    if (!p->pref_format) {
        wl_list_for_each(sf, &p->format_list, link) {
            if (p->enable_alpha == sf->is_alpha) {
                p->pref_format = sf->fmt;
                break;
            }
        }
    }

    // if use default is enable overwrite the auto selected one
    if (p->use_default) {
        if (p->enable_alpha)
            p->pref_format = &fmttable[DEFAULT_ALPHA_FORMAT_ENTRY];
        else
            p->pref_format = &fmttable[DEFAULT_FORMAT_ENTRY];
    }


    p->bytes_per_pixel = mp_imgfmt_get_desc(p->pref_format->mp_fmt).bytes[0];
    MP_VERBOSE(p->wl, "bytes per pixel: %d\n", p->bytes_per_pixel);

    if (!reinit_shm_buffers(p, p->width, p->height)) {
        MP_ERR(p->wl, "failed to initialise buffers\n");
        return -1;
    }

    vo_wayland_config(vo, vo->dwidth, vo->dheight, flags);

    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    for (int i = 0; i < MAX_BUFFERS; ++i)
        destroy_shm_buffer(&p->buffers[i]);

    talloc_free(p->original_image);

    vo_wayland_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (!vo_wayland_init(vo)) {
        MP_ERR(p->wl, "could not initalise backend\n");
        return -1;
    }

    p->vo = vo;
    p->wl = vo->wayland;
    p->sws = mp_sws_alloc(vo);

    p->front_buffer = &p->buffers[1];
    p->back_buffer = &p->buffers[0];

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
        OPT_FLAG("default-format", use_default, 0),
        {0}
    },
};

