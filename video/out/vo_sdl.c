/*
 * video output driver for SDL 2.0+
 *
 * Copyright (C) 2012 Rudolf Polzer <divVerent@xonotic.org>
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include <SDL.h>

#include "common/msg.h"
#include "options/options.h"

#include "osdep/timer.h"

#include "sub/osd.h"

#include "video/mp_image.h"

#include "sdl_common.h"
#include "win_state.h"
#include "config.h"
#include "vo.h"

struct formatmap_entry {
    Uint32 sdl;
    unsigned int mpv;
    int is_rgba;
};
const struct formatmap_entry formats[] = {
    {SDL_PIXELFORMAT_YV12, IMGFMT_420P, 0},
    {SDL_PIXELFORMAT_IYUV, IMGFMT_420P, 0},
    {SDL_PIXELFORMAT_UYVY, IMGFMT_UYVY, 0},
    //{SDL_PIXELFORMAT_YVYU, IMGFMT_YVYU, 0},
#if BYTE_ORDER == BIG_ENDIAN
    {SDL_PIXELFORMAT_RGB888, IMGFMT_0RGB, 0}, // RGB888 means XRGB8888
    {SDL_PIXELFORMAT_RGBX8888, IMGFMT_RGB0, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_BGR888, IMGFMT_0BGR, 0}, // BGR888 means XBGR8888
    {SDL_PIXELFORMAT_BGRX8888, IMGFMT_BGR0, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_ARGB8888, IMGFMT_ARGB, 1}, // matches SUBBITMAP_RGBA
    {SDL_PIXELFORMAT_RGBA8888, IMGFMT_RGBA, 1},
    {SDL_PIXELFORMAT_ABGR8888, IMGFMT_ABGR, 1},
    {SDL_PIXELFORMAT_BGRA8888, IMGFMT_BGRA, 1},
#else
    {SDL_PIXELFORMAT_RGB888, IMGFMT_BGR0, 0}, // RGB888 means XRGB8888
    {SDL_PIXELFORMAT_RGBX8888, IMGFMT_0BGR, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_BGR888, IMGFMT_RGB0, 0}, // BGR888 means XBGR8888
    {SDL_PIXELFORMAT_BGRX8888, IMGFMT_0RGB, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_ARGB8888, IMGFMT_BGRA, 1}, // matches SUBBITMAP_RGBA
    {SDL_PIXELFORMAT_RGBA8888, IMGFMT_ABGR, 1},
    {SDL_PIXELFORMAT_ABGR8888, IMGFMT_RGBA, 1},
    {SDL_PIXELFORMAT_BGRA8888, IMGFMT_ARGB, 1},
#endif
    {SDL_PIXELFORMAT_RGB24, IMGFMT_RGB24, 0},
    {SDL_PIXELFORMAT_BGR24, IMGFMT_BGR24, 0},
    {SDL_PIXELFORMAT_RGB565, IMGFMT_RGB565, 0},
};

struct priv {
    SDL_Renderer *renderer;
    int renderer_index;
    SDL_RendererInfo renderer_info;
    SDL_Texture *tex;
    int tex_swapped;
    struct mp_image_params params;
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    struct mp_osd_res osd_res;
    struct formatmap_entry osd_format;
    struct osd_bitmap_surface {
        int change_id;
        struct osd_target {
            SDL_Rect source;
            SDL_Rect dest;
            SDL_Texture *tex;
            SDL_Texture *tex2;
        } *targets;
        int num_targets;
        int targets_size;
    } osd_surfaces[MAX_OSD_PARTS];
    double osd_pts;

    // options
    int allow_sw;
    int switch_mode;
    int vsync;
};

static bool lock_texture(struct vo *vo, struct mp_image *texmpi)
{
    struct priv *vc = vo->priv;
    *texmpi = (struct mp_image){0};
    mp_image_set_size(texmpi, vc->params.w, vc->params.h);
    mp_image_setfmt(texmpi, vc->params.imgfmt);
    switch (texmpi->num_planes) {
    case 1:
    case 3:
        break;
    default:
        MP_ERR(vo, "Invalid plane count\n");
        return false;
    }
    void *pixels;
    int pitch;
    if (SDL_LockTexture(vc->tex, NULL, &pixels, &pitch)) {
        MP_ERR(vo, "SDL_LockTexture failed\n");
        return false;
    }
    texmpi->planes[0] = pixels;
    texmpi->stride[0] = pitch;
    if (texmpi->num_planes == 3) {
        if (vc->tex_swapped) {
            texmpi->planes[2] =
                ((Uint8 *) texmpi->planes[0] + texmpi->h * pitch);
            texmpi->stride[2] = pitch / 2;
            texmpi->planes[1] =
                ((Uint8 *) texmpi->planes[2] + (texmpi->h * pitch) / 4);
            texmpi->stride[1] = pitch / 2;
        } else {
            texmpi->planes[1] =
                ((Uint8 *) texmpi->planes[0] + texmpi->h * pitch);
            texmpi->stride[1] = pitch / 2;
            texmpi->planes[2] =
                ((Uint8 *) texmpi->planes[1] + (texmpi->h * pitch) / 4);
            texmpi->stride[2] = pitch / 2;
        }
    }
    return true;
}

static bool is_good_renderer(SDL_RendererInfo *ri,
                             const char *driver_name_wanted, int allow_sw,
                             struct formatmap_entry *osd_format)
{
    if (driver_name_wanted && driver_name_wanted[0])
        if (strcmp(driver_name_wanted, ri->name))
            return false;

    if (!allow_sw &&
        !(ri->flags & SDL_RENDERER_ACCELERATED))
        return false;

    int i, j;
    for (i = 0; i < ri->num_texture_formats; ++i)
        for (j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            if (ri->texture_formats[i] == formats[j].sdl)
                if (formats[j].is_rgba) {
                    if (osd_format)
                        *osd_format = formats[j];
                    return true;
                }

    return false;
}

static void destroy_renderer(struct vo *vo)
{
    struct priv *vc = vo->priv;

    // free ALL the textures
    if (vc->tex) {
        SDL_DestroyTexture(vc->tex);
        vc->tex = NULL;
    }

    int i, j;
    for (i = 0; i < MAX_OSD_PARTS; ++i) {
        for (j = 0; j < vc->osd_surfaces[i].targets_size; ++j) {
            if (vc->osd_surfaces[i].targets[j].tex) {
                SDL_DestroyTexture(vc->osd_surfaces[i].targets[j].tex);
                vc->osd_surfaces[i].targets[j].tex = NULL;
            }
            if (vc->osd_surfaces[i].targets[j].tex2) {
                SDL_DestroyTexture(vc->osd_surfaces[i].targets[j].tex2);
                vc->osd_surfaces[i].targets[j].tex2 = NULL;
            }
        }
    }

    if (vc->renderer) {
        SDL_DestroyRenderer(vc->renderer);
        vc->renderer = NULL;
    }
}

static bool try_create_renderer(struct vo *vo, int i, const char *driver)
{
    struct priv *vc = vo->priv;

    // first probe
    SDL_RendererInfo ri;
    if (SDL_GetRenderDriverInfo(i, &ri))
        return false;
    if (!is_good_renderer(&ri, driver, vc->allow_sw, NULL))
        return false;

    vc->renderer = SDL_CreateRenderer(vo->sdl->window, i, 0);
    if (!vc->renderer) {
        MP_ERR(vo, "SDL_CreateRenderer failed\n");
        return false;
    }

    if (SDL_GetRendererInfo(vc->renderer, &vc->renderer_info)) {
        MP_ERR(vo, "SDL_GetRendererInfo failed\n");
        destroy_renderer(vo);
        return false;
    }

    if (!is_good_renderer(&vc->renderer_info, NULL, vc->allow_sw,
                          &vc->osd_format)) {
        MP_ERR(vo, "Renderer '%s' does not fulfill "
                                  "requirements on this system\n",
                                  vc->renderer_info.name);
        destroy_renderer(vo);
        return false;
    }

    if (vc->renderer_index != i) {
        MP_INFO(vo, "Using %s\n", vc->renderer_info.name);
        vc->renderer_index = i;
    }

    return true;
}

static int init_renderer(struct vo *vo)
{
    struct priv *vc = vo->priv;

    int n = SDL_GetNumRenderDrivers();
    int i;

    if (vc->renderer_index >= 0)
        if (try_create_renderer(vo, vc->renderer_index, NULL))
            return 0;

    for (i = 0; i < n; ++i)
        if (try_create_renderer(vo, i, SDL_GetHint(SDL_HINT_RENDER_DRIVER)))
            return 0;

    for (i = 0; i < n; ++i)
        if (try_create_renderer(vo, i, NULL))
            return 0;

    MP_ERR(vo, "No supported renderer\n");
    return -1;
}

static void resize(struct vo *vo, int w, int h)
{
    struct priv *vc = vo->priv;
    vo->dwidth = w;
    vo->dheight = h;
    vo_get_src_dst_rects(vo, &vc->src_rect, &vc->dst_rect,
                         &vc->osd_res);
    SDL_RenderSetLogicalSize(vc->renderer, w, h);
    vo->want_redraw = true;
    vo_wakeup(vo);
}

static void force_resize(struct vo *vo)
{
    int w, h;
    SDL_GetWindowSize(vo->sdl->window, &w, &h);
    resize(vo, w, h);
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *vc = vo->priv;

    vo_sdl_config(vo);

    if (vc->tex)
        SDL_DestroyTexture(vc->tex);
    Uint32 texfmt = SDL_PIXELFORMAT_UNKNOWN;
    int i, j;
    for (i = 0; i < vc->renderer_info.num_texture_formats; ++i)
        for (j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            if (vc->renderer_info.texture_formats[i] == formats[j].sdl)
                if (params->imgfmt == formats[j].mpv)
                    texfmt = formats[j].sdl;
    if (texfmt == SDL_PIXELFORMAT_UNKNOWN) {
        MP_ERR(vo, "Invalid pixel format\n");
        return -1;
    }

    vc->tex_swapped = texfmt == SDL_PIXELFORMAT_YV12;
    vc->tex = SDL_CreateTexture(vc->renderer, texfmt,
                                SDL_TEXTUREACCESS_STREAMING,
                                params->w, params->h);
    if (!vc->tex) {
        MP_ERR(vo, "Could not create a texture\n");
        return -1;
    }

    vc->params = *params;

    struct mp_image tmp;
    if (!lock_texture(vo, &tmp)) {
        SDL_DestroyTexture(vc->tex);
        vc->tex = NULL;
        return -1;
    }
    mp_image_clear(&tmp, 0, 0, tmp.w, tmp.h);
    SDL_UnlockTexture(vc->tex);

    force_resize(vo);

    return 0;
}

static void flip_page(struct vo *vo)
{
    struct priv *vc = vo->priv;
    SDL_RenderPresent(vc->renderer);
}

static void wakeup(struct vo *vo)
{
    vo_sdl_wakeup(vo);
}

static void wait_events(struct vo *vo, int64_t until_time_us)
{
    vo_sdl_wait_events(vo, until_time_us);
}

static void uninit(struct vo *vo)
{
    destroy_renderer(vo);
    vo_sdl_uninit(vo);
}

static inline void upload_to_texture(struct vo *vo, SDL_Texture *tex,
                                     int w, int h, void *bitmap, int stride)
{
    struct priv *vc = vo->priv;

    if (vc->osd_format.sdl == SDL_PIXELFORMAT_ARGB8888) {
        // NOTE: this optimization is questionable, because SDL docs say
        // that this way is slow.
        // It did measure up faster, though...
        SDL_UpdateTexture(tex, NULL, bitmap, stride);
        return;
    }

    void *pixels;
    int pitch;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch)) {
        MP_ERR(vo, "Could not lock texture\n");
    } else {
        SDL_ConvertPixels(w, h, SDL_PIXELFORMAT_ARGB8888,
                          bitmap, stride,
                          vc->osd_format.sdl,
                          pixels, pitch);
        SDL_UnlockTexture(tex);
    }
}

static inline void subbitmap_to_texture(struct vo *vo, SDL_Texture *tex,
                                        struct sub_bitmap *bmp,
                                        uint32_t ormask)
{
    if (ormask == 0) {
        upload_to_texture(vo, tex, bmp->w, bmp->h,
                          bmp->bitmap, bmp->stride);
    } else {
        uint32_t *temppixels;
        temppixels = talloc_array(vo, uint32_t, bmp->w * bmp->h);

        int x, y;
        for (y = 0; y < bmp->h; ++y) {
            const uint32_t *src =
                (const uint32_t *) ((const char *) bmp->bitmap + y * bmp->stride);
            uint32_t *dst = temppixels + y * bmp->w;
            for (x = 0; x < bmp->w; ++x)
                dst[x] = src[x] | ormask;
        }

        upload_to_texture(vo, tex, bmp->w, bmp->h,
                          temppixels, sizeof(uint32_t) * bmp->w);

        talloc_free(temppixels);
    }
}

static void generate_osd_part(struct vo *vo, struct sub_bitmaps *imgs)
{
    struct priv *vc = vo->priv;
    struct osd_bitmap_surface *sfc = &vc->osd_surfaces[imgs->render_index];

    if (imgs->format == SUBBITMAP_EMPTY || imgs->num_parts == 0)
        return;

    if (imgs->change_id == sfc->change_id)
        return;

    if (imgs->num_parts > sfc->targets_size) {
        sfc->targets = talloc_realloc(vc, sfc->targets,
                                      struct osd_target, imgs->num_parts);
        memset(&sfc->targets[sfc->targets_size], 0, sizeof(struct osd_target) *
               (imgs->num_parts - sfc->targets_size));
        sfc->targets_size = imgs->num_parts;
    }
    sfc->num_targets = imgs->num_parts;

    for (int i = 0; i < imgs->num_parts; i++) {
        struct osd_target *target = sfc->targets + i;
        struct sub_bitmap *bmp = imgs->parts + i;

        target->source = (SDL_Rect){
            0, 0, bmp->w, bmp->h
        };
        target->dest = (SDL_Rect){
            bmp->x, bmp->y, bmp->dw, bmp->dh
        };

        // tex: alpha blended texture
        if (target->tex) {
            SDL_DestroyTexture(target->tex);
            target->tex = NULL;
        }
        if (!target->tex)
            target->tex = SDL_CreateTexture(vc->renderer,
                    vc->osd_format.sdl, SDL_TEXTUREACCESS_STREAMING,
                    bmp->w, bmp->h);
        if (!target->tex) {
            MP_ERR(vo, "Could not create texture\n");
        }
        if (target->tex) {
            SDL_SetTextureBlendMode(target->tex,
                                    SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(target->tex, 0, 0, 0);
            subbitmap_to_texture(vo, target->tex, bmp, 0); // RGBA -> 000A
        }

        // tex2: added texture
        if (target->tex2) {
            SDL_DestroyTexture(target->tex2);
            target->tex2 = NULL;
        }
        if (!target->tex2)
            target->tex2 = SDL_CreateTexture(vc->renderer,
                    vc->osd_format.sdl, SDL_TEXTUREACCESS_STREAMING,
                    bmp->w, bmp->h);
        if (!target->tex2) {
            MP_ERR(vo, "Could not create texture\n");
        }
        if (target->tex2) {
            SDL_SetTextureBlendMode(target->tex2,
                                    SDL_BLENDMODE_ADD);
            subbitmap_to_texture(vo, target->tex2, bmp,
                                    0xFF000000); // RGBA -> RGB1
        }
    }

    sfc->change_id = imgs->change_id;
}

static void draw_osd_part(struct vo *vo, int index)
{
    struct priv *vc = vo->priv;
    struct osd_bitmap_surface *sfc = &vc->osd_surfaces[index];
    int i;

    for (i = 0; i < sfc->num_targets; i++) {
        struct osd_target *target = sfc->targets + i;
        if (target->tex)
            SDL_RenderCopy(vc->renderer, target->tex,
                           &target->source, &target->dest);
        if (target->tex2)
            SDL_RenderCopy(vc->renderer, target->tex2,
                           &target->source, &target->dest);
    }
}

static void draw_osd_cb(void *ctx, struct sub_bitmaps *imgs)
{
    struct vo *vo = ctx;
    generate_osd_part(vo, imgs);
    draw_osd_part(vo, imgs->render_index);
}

static void draw_osd(struct vo *vo)
{
    struct priv *vc = vo->priv;

    static const bool osdformats[SUBBITMAP_COUNT] = {
        [SUBBITMAP_RGBA] = true,
    };

    osd_draw(vo->osd, vc->osd_res, vc->osd_pts, 0, osdformats, draw_osd_cb, vo);
}

static int preinit(struct vo *vo)
{
    struct priv *vc = vo->priv;

    // predefine MPV options (SDL env vars shall be overridden)
    SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC, vc->vsync ? "1" : "0",
                            SDL_HINT_OVERRIDE);

    if (!vo_sdl_init(vo, 0)) {
        vo_sdl_uninit(vo);
        return -1;
    }

    // try creating a renderer (this also gets the renderer_info data
    // for query_format to use!)
    if (init_renderer(vo) != 0) {
        vo_sdl_uninit(vo);
        return -1;
    }

    MP_WARN(vo, "Warning: this legacy VO has bad performance. Consider fixing "
                "your graphics drivers, or not forcing the sdl VO.\n");

    return 0;
}

static int query_format(struct vo *vo, int format)
{
    struct priv *vc = vo->priv;
    int i, j;
    for (i = 0; i < vc->renderer_info.num_texture_formats; ++i)
        for (j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            if (vc->renderer_info.texture_formats[i] == formats[j].sdl)
                if (format == formats[j].mpv)
                    return 1;
    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *vc = vo->priv;

    // typically this runs in parallel with the following mp_image_copy call
    SDL_SetRenderDrawColor(vc->renderer, 0, 0, 0, 255);
    SDL_RenderClear(vc->renderer);

    SDL_SetTextureBlendMode(vc->tex, SDL_BLENDMODE_NONE);

    if (mpi) {
        vc->osd_pts = mpi->pts;

        mp_image_t texmpi;
        if (!lock_texture(vo, &texmpi)) {
            talloc_free(mpi);
            return;
        }

        mp_image_copy(&texmpi, mpi);

        SDL_UnlockTexture(vc->tex);

        talloc_free(mpi);
    }

    SDL_Rect src, dst;
    src.x = vc->src_rect.x0;
    src.y = vc->src_rect.y0;
    src.w = vc->src_rect.x1 - vc->src_rect.x0;
    src.h = vc->src_rect.y1 - vc->src_rect.y0;
    dst.x = vc->dst_rect.x0;
    dst.y = vc->dst_rect.y0;
    dst.w = vc->dst_rect.x1 - vc->dst_rect.x0;
    dst.h = vc->dst_rect.y1 - vc->dst_rect.y0;

    SDL_RenderCopy(vc->renderer, vc->tex, &src, &dst);

    draw_osd(vo);
}

static struct mp_image *get_window_screenshot(struct vo *vo)
{
    struct priv *vc = vo->priv;
    struct mp_image *image = mp_image_alloc(vc->osd_format.mpv, vo->dwidth,
                                                                vo->dheight);
    if (!image)
        return NULL;
    if (SDL_RenderReadPixels(vc->renderer, NULL, vc->osd_format.sdl,
                             image->planes[0], image->stride[0])) {
        MP_ERR(vo, "SDL_RenderReadPixels failed\n");
        talloc_free(image);
        return NULL;
    }
    return image;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    switch (request) {
    case VOCTRL_REDRAW_FRAME:
        draw_image(vo, NULL);
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        force_resize(vo);
        return VO_TRUE;
    case VOCTRL_SCREENSHOT_WIN:
        *(struct mp_image **)data = get_window_screenshot(vo);
        return VO_TRUE;
    }

    int events = 0;
    int r = vo_sdl_control(vo, &events, request, data);
    if (vo->config_ok && (events & (VO_EVENT_EXPOSE | VO_EVENT_RESIZE)))
        force_resize(vo);
    vo_event(vo, events);
    return r;
}

#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_sdl = {
    .description = "SDL 2.0 Renderer",
    .name = "sdl",
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .renderer_index = -1,
        .vsync = 1,
    },
    .options = (const struct m_option []){
        OPT_FLAG("sw", allow_sw, 0),
        OPT_FLAG("switch-mode", switch_mode, 0),
        OPT_FLAG("vsync", vsync, 0),
        {NULL}
    },
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .uninit = uninit,
    .flip_page = flip_page,
    .wait_events = wait_events,
    .wakeup = wakeup,
    .options_prefix = "sdl",
};
