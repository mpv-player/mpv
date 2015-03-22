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
#include <stdbool.h>
#include <assert.h>

#include <libavutil/common.h>

#include "config.h"

#include "vo.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "sub/osd.h"
#include "sub/img_convert.h"
#include "common/msg.h"
#include "input/input.h"
#include "osdep/endian.h"
#include "osdep/timer.h"

#include "wayland_common.h"

#include "video/out/wayland/buffer.h"

static void draw_image(struct vo *vo, mp_image_t *mpi);
static void draw_osd(struct vo *vo);

static const struct wl_buffer_listener buffer_listener;

// TODO: pay attention to the reported subpixel order
static const format_t format_table[] = {
    {WL_SHM_FORMAT_ARGB8888, IMGFMT_BGRA}, // 8b 8g 8r 8a
    {WL_SHM_FORMAT_XRGB8888, IMGFMT_BGR0},
    {WL_SHM_FORMAT_RGB332,   IMGFMT_RGB8}, // 3b 3g 2r
    {WL_SHM_FORMAT_BGR233,   IMGFMT_BGR8}, // 3r 3g 3b,
#if BYTE_ORDER == LITTLE_ENDIAN
    {WL_SHM_FORMAT_XRGB4444, IMGFMT_RGB444}, // 4b 4g 4r 4a
    {WL_SHM_FORMAT_XBGR4444, IMGFMT_BGR444}, // 4r 4g 4b 4a
    {WL_SHM_FORMAT_ARGB4444, IMGFMT_RGB444},
    {WL_SHM_FORMAT_ABGR4444, IMGFMT_BGR444},
    {WL_SHM_FORMAT_XRGB1555, IMGFMT_RGB555}, // 5b 5g 5r 1a
    {WL_SHM_FORMAT_XBGR1555, IMGFMT_BGR555}, // 5r 5g 5b 1a
    {WL_SHM_FORMAT_ARGB1555, IMGFMT_RGB555},
    {WL_SHM_FORMAT_ABGR1555, IMGFMT_BGR555},
    {WL_SHM_FORMAT_RGB565,   IMGFMT_RGB565}, // 5b 6g 5r
    {WL_SHM_FORMAT_BGR565,   IMGFMT_BGR565}, // 5r 6g 5b
#else
    {WL_SHM_FORMAT_RGBX4444, IMGFMT_BGR444}, // 4a 4b 4g 4r
    {WL_SHM_FORMAT_BGRX4444, IMGFMT_RGB444}, // 4a 4r 4g 4b
    {WL_SHM_FORMAT_RGBA4444, IMGFMT_BGR444},
    {WL_SHM_FORMAT_BGRA4444, IMGFMT_RGB444},
    {WL_SHM_FORMAT_RGBX5551, IMGFMT_BGR555}, // 1a 5g 5b 5r
    {WL_SHM_FORMAT_BGRX5551, IMGFMT_RGB555}, // 1a 5r 5g 5b
    {WL_SHM_FORMAT_RGBA5551, IMGFMT_BGR555},
    {WL_SHM_FORMAT_BGRA5551, IMGFMT_RGB555},
#endif
    {WL_SHM_FORMAT_RGB888,   IMGFMT_BGR24}, // 8b 8g 8r
    {WL_SHM_FORMAT_BGR888,   IMGFMT_RGB24}, // 8r 8g 8b
    {WL_SHM_FORMAT_XBGR8888, IMGFMT_RGB0},
    {WL_SHM_FORMAT_RGBX8888, IMGFMT_0BGR},
    {WL_SHM_FORMAT_BGRX8888, IMGFMT_0RGB},
    {WL_SHM_FORMAT_ABGR8888, IMGFMT_RGBA},
    {WL_SHM_FORMAT_RGBA8888, IMGFMT_ABGR},
    {WL_SHM_FORMAT_BGRA8888, IMGFMT_ARGB},
};

#define MAX_FORMAT_ENTRIES (sizeof(format_table) / sizeof(format_table[0]))
#define DEFAULT_FORMAT_ENTRY 1
#define DEFAULT_ALPHA_FORMAT_ENTRY 0

struct priv;

// We only use double buffering but the creation and usage is still open to
// triple buffering. Tripple buffering is now removed, because double buffering
// is now pixel-perfect.
struct buffer_pool {
    shm_buffer_t **buffers;
    shm_buffer_t *front_buffer; // just pointers to any of the buffers
    shm_buffer_t *back_buffer;
    uint32_t buffer_no;
};

struct supported_format {
    format_t format;
    bool is_alpha;
    struct wl_list link;
};

struct priv {
    struct vo *vo;
    struct vo_wayland_state *wl;

    struct wl_list format_list;
    const format_t *video_format; // pointer to element in supported_format list

    struct mp_rect src;
    struct mp_rect dst;
    int src_w, src_h;
    int dst_w, dst_h;
    struct mp_osd_res osd;

    struct mp_sws_context *sws;
    struct mp_image_params in_format;

    struct buffer_pool video_bufpool;

    struct mp_image *original_image;
    int width;  // width of the original image
    int height;

    int x, y; // coords for resizing

    struct wl_surface *osd_surfaces[MAX_OSD_PARTS];
    struct wl_subsurface *osd_subsurfaces[MAX_OSD_PARTS];
    shm_buffer_t *osd_buffers[MAX_OSD_PARTS];
    // this id tells us if the subtitle part has changed or not
    int change_id[MAX_OSD_PARTS];

    int64_t recent_flip_time; // last frame event

    // options
    int enable_alpha;
    int use_rgb565;
};

static bool is_alpha_format(const format_t *fmt)
{
    return !!(mp_imgfmt_get_desc(fmt->mp_format).flags & MP_IMGFLAG_ALPHA);
}

static const format_t* is_wayland_format_supported(struct priv *p,
                                                   enum wl_shm_format fmt)
{
    struct supported_format *sf;

    // find the matching format first
    wl_list_for_each(sf, &p->format_list, link) {
        if (sf->format.wl_format == fmt) {
            return &sf->format;
        }
    }

    return NULL;
}

// additinal buffer functions

static void buffer_finalise_front(shm_buffer_t *buf)
{
    SHM_BUFFER_SET_BUSY(buf);
    SHM_BUFFER_CLEAR_DIRTY(buf);
}

static void buffer_finalise_back(shm_buffer_t *buf)
{
    SHM_BUFFER_SET_DIRTY(buf);
}

static struct mp_image buffer_get_mp_image(struct priv *p,
                                           shm_buffer_t *buf)
{
    struct mp_image img = {0};
    mp_image_set_params(&img, &p->sws->dst);

    img.w = buf->stride / buf->bytes;
    img.h = buf->height;
    img.planes[0] = buf->data;
    img.stride[0] = buf->stride;

    return img;
}

// buffer pool functions

static void buffer_pool_reinit(struct priv *p,
                               struct buffer_pool *pool,
                               uint32_t buffer_no,
                               uint32_t width, uint32_t height,
                               format_t fmt,
                               struct wl_shm *shm)
{
    if (!pool->buffers)
        pool->buffers = calloc(buffer_no, sizeof(shm_buffer_t*));

    pool->buffer_no = buffer_no;

    for (uint32_t i = 0; i < buffer_no; ++i) {
        if (pool->buffers[i] == NULL)
            pool->buffers[i] = shm_buffer_create(width, height, fmt,
                                                 shm, &buffer_listener);
        else
            shm_buffer_resize(pool->buffers[i], width, height);
    }

    pool->back_buffer = pool->buffers[0];
    pool->front_buffer = pool->buffers[1];
}

static bool buffer_pool_resize(struct buffer_pool *pool,
                               int width,
                               int height)
{
    bool ret = true;

    for (uint32_t i = 0; ret && i < pool->buffer_no; ++i)
        shm_buffer_resize(pool->buffers[i], width, height);

    return ret;
}

static void buffer_pool_destroy(struct buffer_pool *pool)
{
    for (uint32_t i = 0; i < pool->buffer_no; ++i)
        shm_buffer_destroy(pool->buffers[i]);

    free(pool->buffers);
    pool->front_buffer = NULL;
    pool->back_buffer = NULL;
    pool->buffers = NULL;
}

static void buffer_pool_swap(struct buffer_pool *pool)
{
    if (SHM_BUFFER_IS_DIRTY(pool->back_buffer)) {
        shm_buffer_t *tmp = pool->back_buffer;
        pool->back_buffer = pool->front_buffer;
        pool->front_buffer = tmp;
    }
}

// returns NULL if the back buffer is busy
static shm_buffer_t * buffer_pool_get_back(struct buffer_pool *pool)
{
    if (!pool->back_buffer || SHM_BUFFER_IS_BUSY(pool->back_buffer))
        return NULL;

    return pool->back_buffer;
}

static shm_buffer_t * buffer_pool_get_front(struct buffer_pool *pool)
{
    return pool->front_buffer;
}

static bool redraw_frame(struct priv *p)
{
    draw_image(p->vo, NULL);
    return true;
}

static bool resize(struct priv *p)
{
    struct vo_wayland_state *wl = p->wl;

    if (!p->video_bufpool.back_buffer || SHM_BUFFER_IS_BUSY(p->video_bufpool.back_buffer))
        return false; // skip resizing if we can't garantuee pixel perfectness!

    int32_t x = wl->window.sh_x;
    int32_t y = wl->window.sh_y;
    wl->vo->dwidth = wl->window.sh_width;
    wl->vo->dheight = wl->window.sh_height;

    vo_get_src_dst_rects(p->vo, &p->src, &p->dst, &p->osd);
    p->src_w = p->src.x1 - p->src.x0;
    p->src_h = p->src.y1 - p->src.y0;
    p->dst_w = p->dst.x1 - p->dst.x0;
    p->dst_h = p->dst.y1 - p->dst.y0;

    mp_input_set_mouse_transform(p->vo->input_ctx, &p->dst, NULL);

    MP_DBG(wl, "resizing %dx%d -> %dx%d\n", wl->window.width,
                                            wl->window.height,
                                            p->dst_w,
                                            p->dst_h);

    if (x != 0)
        x = wl->window.width - p->dst_w;

    if (y != 0)
        y = wl->window.height - p->dst_h;

    mp_sws_set_from_cmdline(p->sws, p->vo->opts->sws_opts);
    p->sws->src = p->in_format;
    p->sws->dst = (struct mp_image_params) {
        .imgfmt = p->video_format->mp_format,
        .w = p->dst_w,
        .h = p->dst_h,
        .d_w = p->dst_w,
        .d_h = p->dst_h,
    };

    mp_image_params_guess_csp(&p->sws->dst);

    if (mp_sws_reinit(p->sws) < 0)
        return false;

    if (!buffer_pool_resize(&p->video_bufpool, p->dst_w, p->dst_h)) {
        MP_ERR(wl, "failed to resize video buffers\n");
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
        wl_surface_set_opaque_region(wl->window.video_surface, opaque);
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
    shm_buffer_t *buf = data;

    if (SHM_BUFFER_IS_ONESHOT(buf)) {
        shm_buffer_destroy(buf);
        return;
    }

    SHM_BUFFER_CLEAR_BUSY(buf);
    // does nothing and returns 0 if no pending resize flag was set
    shm_buffer_pending_resize(buf);
}

static const struct wl_buffer_listener buffer_listener = {
    buffer_handle_release
};

static void shm_handle_format(void *data,
                              struct wl_shm *wl_shm,
                              uint32_t format)
{
    struct priv *p = data;
    for (uint32_t i = 0; i < MAX_FORMAT_ENTRIES; ++i) {
        if (format_table[i].wl_format == format) {
            MP_INFO(p->wl, "format %s supported by hw\n",
                    mp_imgfmt_to_name(format_table[i].mp_format));
            struct supported_format *sf = talloc(p, struct supported_format);
            sf->format = format_table[i];
            sf->is_alpha = is_alpha_format(&sf->format);
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

    if (mpi) {
        talloc_free(p->original_image);
        p->original_image = mpi;
    }

    if (!p->wl->frame.pending)
        return;

    shm_buffer_t *buf = buffer_pool_get_back(&p->video_bufpool);

    if (!buf) {
        // TODO: use similar handling of busy buffers as the osd buffers
        // if the need arises
        MP_VERBOSE(p->wl, "can't draw, back buffer is busy\n");
        return;
    }

    struct mp_image img = buffer_get_mp_image(p, buf);

    if (p->original_image) {
        struct mp_image src = *p->original_image;
        struct mp_rect src_rc = p->src;
        src_rc.x0 = MP_ALIGN_DOWN(src_rc.x0, src.fmt.align_x);
        src_rc.y0 = MP_ALIGN_DOWN(src_rc.y0, src.fmt.align_y);
        mp_image_crop_rc(&src, src_rc);

        mp_sws_scale(p->sws, &img, &src);
    } else {
        mp_image_clear(&img, 0, 0, img.w, img.h);
    }

    buffer_finalise_back(buf);

    draw_osd(vo);
}

static void draw_osd_cb(void *ctx, struct sub_bitmaps *imgs)
{
    struct priv *p = ctx;
    int id = imgs->render_index;

    struct wl_surface *s = p->osd_surfaces[id];

    if (imgs->change_id != p->change_id[id]) {
        p->change_id[id] = imgs->change_id;

        struct mp_rect bb;
        if (!mp_sub_bitmaps_bb(imgs, &bb))
            return;

        int width = mp_rect_w(bb);
        int height = mp_rect_h(bb);

        if (!p->osd_buffers[id]) {
            p->osd_buffers[id] = shm_buffer_create(width,
                                                   height,
                                                   format_table[DEFAULT_ALPHA_FORMAT_ENTRY],
                                                   p->wl->display.shm,
                                                   &buffer_listener);
        }
        else if (SHM_BUFFER_IS_BUSY(p->osd_buffers[id])) {
            // freed on release in buffer_listener
            // garantuees pixel perfect resizing of subtitles and osd
            SHM_BUFFER_SET_ONESHOT(p->osd_buffers[id]);
            p->osd_buffers[id] = shm_buffer_create(width,
                                                   height,
                                                   format_table[DEFAULT_ALPHA_FORMAT_ENTRY],
                                                   p->wl->display.shm,
                                                   &buffer_listener);
        }
        else {
            shm_buffer_resize(p->osd_buffers[id], width, height);
        }

        shm_buffer_t *buf = p->osd_buffers[id];
        SHM_BUFFER_SET_BUSY(buf);

        struct mp_image wlimg = buffer_get_mp_image(p, buf);

        for (int n = 0; n < imgs->num_parts; n++) {
            struct sub_bitmap *sub = &imgs->parts[n];
            memcpy_pic(wlimg.planes[0], sub->bitmap, sub->w * 4, sub->h,
                       wlimg.stride[0], sub->stride);
        }

        wl_subsurface_set_position(p->osd_subsurfaces[id], 0, 0);
        wl_surface_attach(s, buf->buffer, bb.x0, bb.y0);
        wl_surface_damage(s, 0, 0, width, height);
        wl_surface_commit(s);
    }
    else {
        // p->osd_buffer, garantueed to exist here
        assert(p->osd_buffers[id]);
        wl_surface_attach(s, p->osd_buffers[id]->buffer, 0, 0);
        wl_surface_commit(s);
    }
}

static const bool osd_formats[SUBBITMAP_COUNT] = {
    [SUBBITMAP_RGBA] = true,
};

static void draw_osd(struct vo *vo)
{
    struct priv *p = vo->priv;

    // deattach all buffers and attach all needed buffers in osd_draw
    // only the most recent attach & commit is applied once the parent surface
    // is committed
    for (int i = 0; i < MAX_OSD_PARTS; ++i) {
        struct wl_surface *s = p->osd_surfaces[i];
        wl_surface_attach(s, NULL, 0, 0);
        wl_surface_damage(s, 0, 0, p->dst_w, p->dst_h);
        wl_surface_commit(s);
    }

    double pts = p->original_image ? p->original_image->pts : 0;
    osd_draw(vo->osd, p->osd, pts, 0, osd_formats, draw_osd_cb, p);
}

static void flip_page(struct vo *vo)
{
    struct priv *p = vo->priv;

    if (!p->wl->frame.pending)
        return;

    buffer_pool_swap(&p->video_bufpool);

    shm_buffer_t *buf = buffer_pool_get_front(&p->video_bufpool);
    wl_surface_attach(p->wl->window.video_surface, buf->buffer, p->x, p->y);
    wl_surface_damage(p->wl->window.video_surface, 0, 0, p->dst_w, p->dst_h);
    wl_surface_commit(p->wl->window.video_surface);
    buffer_finalise_front(buf);

    p->x = 0;
    p->y = 0;
    p->recent_flip_time = mp_time_us();
    p->wl->frame.pending = false;
}

static int query_format(struct vo *vo, int format)
{
    struct priv *p = vo->priv;
    struct supported_format *sf;
    wl_list_for_each_reverse(sf, &p->format_list, link) {
        if (sf->format.mp_format == format)
            return 1;
    }

    if (mp_sws_supported_format(format))
        return 1;

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
        if (sf->format.mp_format == fmt->imgfmt &&
            (p->enable_alpha == sf->is_alpha))
        {
            p->video_format = &sf->format;
            break;
        }
    }

    if (!p->video_format) {
        // if use default is enable overwrite the auto selected one
        if (p->enable_alpha)
            p->video_format = &format_table[DEFAULT_ALPHA_FORMAT_ENTRY];
        else
            p->video_format = &format_table[DEFAULT_FORMAT_ENTRY];
    }

    // overides alpha
    // use rgb565 if performance is your main concern
    if (p->use_rgb565) {
        MP_INFO(p->wl, "using rgb565\n");
        const format_t *entry =
            is_wayland_format_supported(p, WL_SHM_FORMAT_RGB565);
        if (entry)
            p->video_format = entry;
    }

    buffer_pool_reinit(p, &p->video_bufpool, 2, p->width, p->height,
                       *p->video_format, p->wl->display.shm);

    vo_wayland_config(vo, flags);

    resize(p);

    return 0;
}

static void uninit(struct vo *vo)
{
    struct priv *p = vo->priv;
    buffer_pool_destroy(&p->video_bufpool);

    talloc_free(p->original_image);

    for (int i = 0; i < MAX_OSD_PARTS; ++i) {
        shm_buffer_destroy(p->osd_buffers[i]);
        wl_subsurface_destroy(p->osd_subsurfaces[i]);
        wl_surface_destroy(p->osd_surfaces[i]);
    }

    vo_wayland_uninit(vo);
}

static int preinit(struct vo *vo)
{
    struct priv *p = vo->priv;
    struct vo_wayland_state *wl = NULL;

    if (!vo_wayland_init(vo))
        return -1;

    wl = vo->wayland;

    p->vo = vo;
    p->wl = wl;
    p->sws = mp_sws_alloc(vo);

    wl_list_init(&p->format_list);

    wl_shm_add_listener(wl->display.shm, &shm_listener, p);
    wl_display_dispatch(wl->display.display);

    // Commits on surfaces bound to a subsurface are cached until the parent
    // surface is commited, in this case the video surface.
    // Which means we can call commit anywhere.
    struct wl_region *input =
        wl_compositor_create_region(wl->display.compositor);
    for (int i = 0; i < MAX_OSD_PARTS; ++i) {
        p->osd_surfaces[i] =
            wl_compositor_create_surface(wl->display.compositor);
        wl_surface_attach(p->osd_surfaces[i], NULL, 0, 0);
        wl_surface_set_input_region(p->osd_surfaces[i], input);
        p->osd_subsurfaces[i] =
            wl_subcompositor_get_subsurface(wl->display.subcomp,
                                            p->osd_surfaces[i],
                                            wl->window.video_surface); // parent
        wl_surface_commit(p->osd_surfaces[i]);
        wl_subsurface_set_sync(p->osd_subsurfaces[i]);
    }
    wl_region_destroy(input);

    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *p = vo->priv;
    switch (request) {
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
    {
        resize(p);
        return VO_TRUE;
    }
    case VOCTRL_REDRAW_FRAME:
        return redraw_frame(p);
    case VOCTRL_GET_RECENT_FLIP_TIME:
    {
        *(int64_t*) data = p->recent_flip_time;
        return VO_TRUE;
    }
    }
    int events = 0;
    int r = vo_wayland_control(vo, &events, request, data);

    // NOTE: VO_EVENT_EXPOSE is never returned by the wayland backend
    if (events & VO_EVENT_RESIZE)
        resize(p);

    vo_event(vo, events);

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
    .flip_page = flip_page,
    .uninit = uninit,
    .options = (const struct m_option[]) {
        OPT_FLAG("alpha", enable_alpha, 0),
        OPT_FLAG("rgb565", use_rgb565, 0),
        {0}
    },
};

