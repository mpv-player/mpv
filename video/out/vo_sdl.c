/*
 * video output driver for SDL2
 *
 * by divVerent <divVerent@xonotic.org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#include "core/input/input.h"
#include "core/input/keycodes.h"
#include "core/mp_fifo.h"
#include "core/mp_msg.h"
#include "core/options.h"

#include "osdep/timer.h"

#include "sub/sub.h"

#include "video/mp_image.h"
#include "video/vfcap.h"

#include "aspect.h"
#include "config.h"
#include "vo.h"

struct formatmap_entry {
    Uint32 sdl;
    unsigned int mpv;
    int is_rgba;
};
const struct formatmap_entry formats[] = {
    {SDL_PIXELFORMAT_YV12, IMGFMT_YV12, 0},
    {SDL_PIXELFORMAT_IYUV, IMGFMT_IYUV, 0},
    {SDL_PIXELFORMAT_YUY2, IMGFMT_YUY2, 0},
    {SDL_PIXELFORMAT_UYVY, IMGFMT_UYVY, 0},
    {SDL_PIXELFORMAT_YVYU, IMGFMT_YVYU, 0},
#if BYTE_ORDER == BIG_ENDIAN
    {SDL_PIXELFORMAT_RGBX8888, IMGFMT_RGBA, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_BGRX8888, IMGFMT_BGRA, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_ARGB8888, IMGFMT_ARGB, 1},
    {SDL_PIXELFORMAT_RGBA8888, IMGFMT_RGBA, 1},
    {SDL_PIXELFORMAT_ABGR8888, IMGFMT_ABGR, 1},
    {SDL_PIXELFORMAT_BGRA8888, IMGFMT_BGRA, 1},
    {SDL_PIXELFORMAT_RGB24, IMGFMT_RGB24, 0},
    {SDL_PIXELFORMAT_BGR24, IMGFMT_BGR24, 0},
    {SDL_PIXELFORMAT_RGB888, IMGFMT_RGB24, 0},
    {SDL_PIXELFORMAT_BGR888, IMGFMT_BGR24, 0},
    {SDL_PIXELFORMAT_RGB565, IMGFMT_RGB16, 0},
    {SDL_PIXELFORMAT_BGR565, IMGFMT_BGR16, 0},
    {SDL_PIXELFORMAT_RGB555, IMGFMT_RGB15, 0},
    {SDL_PIXELFORMAT_BGR555, IMGFMT_BGR15, 0},
    {SDL_PIXELFORMAT_RGB444, IMGFMT_RGB12, 0}
#else
    {SDL_PIXELFORMAT_RGBX8888, IMGFMT_ABGR, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_BGRX8888, IMGFMT_ARGB, 0}, // has no alpha -> bad for OSD
    {SDL_PIXELFORMAT_ARGB8888, IMGFMT_BGRA, 1},
    {SDL_PIXELFORMAT_RGBA8888, IMGFMT_ABGR, 1},
    {SDL_PIXELFORMAT_ABGR8888, IMGFMT_RGBA, 1},
    {SDL_PIXELFORMAT_BGRA8888, IMGFMT_ARGB, 1},
    {SDL_PIXELFORMAT_RGB24, IMGFMT_RGB24, 0},
    {SDL_PIXELFORMAT_BGR24, IMGFMT_BGR24, 0},
    {SDL_PIXELFORMAT_RGB888, IMGFMT_BGR24, 0},
    {SDL_PIXELFORMAT_BGR888, IMGFMT_RGB24, 0},
    {SDL_PIXELFORMAT_RGB565, IMGFMT_BGR16, 0},
    {SDL_PIXELFORMAT_BGR565, IMGFMT_RGB16, 0},
    {SDL_PIXELFORMAT_RGB555, IMGFMT_BGR15, 0},
    {SDL_PIXELFORMAT_BGR555, IMGFMT_RGB15, 0},
    {SDL_PIXELFORMAT_RGB444, IMGFMT_BGR12, 0}
#endif
};

struct keymap_entry {
    SDL_Keycode sdl;
    int mpv;
};
const struct keymap_entry keys[] = {
    {SDLK_RETURN, KEY_ENTER},
    {SDLK_ESCAPE, KEY_ESC},
    {SDLK_BACKSPACE, KEY_BACKSPACE},
    {SDLK_TAB, KEY_TAB},
    {SDLK_PRINTSCREEN, KEY_PRINT},
    {SDLK_PAUSE, KEY_PAUSE},
    {SDLK_INSERT, KEY_INSERT},
    {SDLK_HOME, KEY_HOME},
    {SDLK_PAGEUP, KEY_PAGE_UP},
    {SDLK_DELETE, KEY_DELETE},
    {SDLK_END, KEY_END},
    {SDLK_PAGEDOWN, KEY_PAGE_DOWN},
    {SDLK_RIGHT, KEY_RIGHT},
    {SDLK_LEFT, KEY_LEFT},
    {SDLK_DOWN, KEY_DOWN},
    {SDLK_UP, KEY_UP},
    {SDLK_KP_ENTER, KEY_KPENTER},
    {SDLK_KP_1, KEY_KP1},
    {SDLK_KP_2, KEY_KP2},
    {SDLK_KP_3, KEY_KP3},
    {SDLK_KP_4, KEY_KP4},
    {SDLK_KP_5, KEY_KP5},
    {SDLK_KP_6, KEY_KP6},
    {SDLK_KP_7, KEY_KP7},
    {SDLK_KP_8, KEY_KP8},
    {SDLK_KP_9, KEY_KP9},
    {SDLK_KP_0, KEY_KP0},
    {SDLK_KP_PERIOD, KEY_KPDEC},
    {SDLK_POWER, KEY_POWER},
    {SDLK_MENU, KEY_MENU},
    {SDLK_STOP, KEY_STOP},
    {SDLK_MUTE, KEY_MUTE},
    {SDLK_VOLUMEUP, KEY_VOLUME_UP},
    {SDLK_VOLUMEDOWN, KEY_VOLUME_DOWN},
    {SDLK_KP_COMMA, KEY_KPDEC},
    {SDLK_AUDIONEXT, KEY_NEXT},
    {SDLK_AUDIOPREV, KEY_PREV},
    {SDLK_AUDIOSTOP, KEY_STOP},
    {SDLK_AUDIOPLAY, KEY_PLAY},
    {SDLK_AUDIOMUTE, KEY_MUTE},
    {SDLK_F1, KEY_F + 1},
    {SDLK_F2, KEY_F + 2},
    {SDLK_F3, KEY_F + 3},
    {SDLK_F4, KEY_F + 4},
    {SDLK_F5, KEY_F + 5},
    {SDLK_F6, KEY_F + 6},
    {SDLK_F7, KEY_F + 7},
    {SDLK_F8, KEY_F + 8},
    {SDLK_F9, KEY_F + 9},
    {SDLK_F10, KEY_F + 10},
    {SDLK_F11, KEY_F + 11},
    {SDLK_F12, KEY_F + 12},
    {SDLK_F13, KEY_F + 13},
    {SDLK_F14, KEY_F + 14},
    {SDLK_F15, KEY_F + 15},
    {SDLK_F16, KEY_F + 16},
    {SDLK_F17, KEY_F + 17},
    {SDLK_F18, KEY_F + 18},
    {SDLK_F19, KEY_F + 19},
    {SDLK_F20, KEY_F + 20},
    {SDLK_F21, KEY_F + 21},
    {SDLK_F22, KEY_F + 22},
    {SDLK_F23, KEY_F + 23},
    {SDLK_F24, KEY_F + 24}
};

struct priv {
    bool reinit_renderer;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_RendererInfo renderer_info;
    SDL_Texture *tex;
    mp_image_t texmpi;
    mp_image_t *ssmpi;
    struct mp_rect src_rect;
    struct mp_rect dst_rect;
    struct mp_osd_res osd_res;
    int int_pause;
    struct formatmap_entry osd_format;
    struct osd_bitmap_surface {
        int bitmap_id;
        int bitmap_pos_id;
        struct osd_target {
            SDL_Rect source;
            SDL_Rect dest;
            SDL_Texture *tex;
            SDL_Texture *tex2;
        } *targets;
        int num_targets;
        int targets_size;
    } osd_surfaces[MAX_OSD_PARTS];
    unsigned int mouse_timer;
    int mouse_hidden;
    int brightness, contrast;
};

static bool is_good_renderer(int n, const char *driver_name_wanted)
{
    SDL_RendererInfo ri;
    if (SDL_GetRenderDriverInfo(n, &ri))
        return false;

    if (driver_name_wanted && driver_name_wanted[0])
        if (strcmp(driver_name_wanted, ri.name))
            return false;

    int i, j;
    for (i = 0; i < ri.num_texture_formats; ++i)
        for (j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            if (ri.texture_formats[i] == formats[j].sdl)
                if (formats[j].is_rgba)
                    return true;

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

    if (vc->window) {
        SDL_DestroyWindow(vc->window);
        vc->window = NULL;
    }
}

static int init_renderer(struct vo *vo, int w, int h)
{
    struct priv *vc = vo->priv;

    vc->window = SDL_CreateWindow("MPV",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  w, h,
                                  SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (!vc->window) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_CreateWindow failedd\n");
        destroy_renderer(vo);
        return -1;
    }

    int n = SDL_GetNumRenderDrivers();
    int i;
    for (i = 0; i < n; ++i)
        if (is_good_renderer(i, SDL_GetHint(SDL_HINT_RENDER_DRIVER)))
            break;
    if (i >= n)
        for (i = 0; i < n; ++i)
            if (is_good_renderer(i, NULL))
                break;
    if (i >= n) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] No supported renderer\n");
        destroy_renderer(vo);
        return -1;
    }

    vc->renderer = SDL_CreateRenderer(vc->window, i, 0);
    if (!vc->renderer) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_CreateRenderer failed\n");
        destroy_renderer(vo);
        return -1;
    }

    if (SDL_GetRendererInfo(vc->renderer, &vc->renderer_info)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_GetRendererInfo failed\n");
        destroy_renderer(vo);
        return -1;
    }

    mp_msg(MSGT_VO, MSGL_INFO, "[sdl] Using %s\n", vc->renderer_info.name);

    int j;
    for (i = 0; i < vc->renderer_info.num_texture_formats; ++i)
        for (j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            if (vc->renderer_info.texture_formats[i] == formats[j].sdl)
                if (formats[j].is_rgba)
                    vc->osd_format = formats[j];

    return 0;
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
}

static void force_resize(struct vo *vo)
{
    struct priv *vc = vo->priv;
    int w, h;
    SDL_GetWindowSize(vc->window, &w, &h);
    resize(vo, w, h);
}

static void check_resize(struct vo *vo)
{
    struct priv *vc = vo->priv;
    int w, h;
    SDL_GetWindowSize(vc->window, &w, &h);
    if (vo->dwidth != w || vo->dheight != h)
        resize(vo, w, h);
}

static void set_fullscreen(struct vo *vo, int fs)
{
    struct priv *vc = vo->priv;
    struct MPOpts *opts = vo->opts;

    if (opts->vidmode)
        SDL_SetWindowDisplayMode(vc->window, NULL);
    else {
        SDL_DisplayMode mode;
        if (!SDL_GetCurrentDisplayMode(SDL_GetWindowDisplay(vc->window), &mode))
            SDL_SetWindowDisplayMode(vc->window, &mode);
    }

    if (SDL_SetWindowFullscreen(vc->window, fs)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_SetWindowFullscreen failed\n");
        return;
    }

    // toggling fullscreen might recreate the window, so better guard for this
    SDL_DisableScreenSaver();

    vo_fs = fs;
    force_resize(vo);
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct priv *vc = vo->priv;

    if (vc->reinit_renderer) {
        destroy_renderer(vo);
        vc->reinit_renderer = false;
    }

    if (vc->window)
        SDL_SetWindowSize(vc->window, d_width, d_height);
    else {
        if (init_renderer(vo, d_width, d_height) != 0)
            return -1;
    }

    if (vc->tex)
        SDL_DestroyTexture(vc->tex);
    Uint32 texfmt = SDL_PIXELFORMAT_UNKNOWN;
    int i, j;
    for (i = 0; i < vc->renderer_info.num_texture_formats; ++i)
        for (j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            if (vc->renderer_info.texture_formats[i] == formats[j].sdl)
                if (format == formats[j].mpv)
                    texfmt = formats[j].sdl;
    if (texfmt == SDL_PIXELFORMAT_UNKNOWN) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] Invalid pixel format\n");
        return -1;
    }

    vc->tex = SDL_CreateTexture(vc->renderer, texfmt,
                                SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!vc->tex) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] Could not create a texture\n");
        return -1;
    }

    mp_image_t *texmpi = &vc->texmpi;
    texmpi->width = texmpi->w = width;
    texmpi->height = texmpi->h = height;
    mp_image_setfmt(texmpi, format);
    switch (texmpi->num_planes) {
    case 1:
    case 3:
        break;
    default:
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] Invalid plane count\n");
        SDL_DestroyTexture(vc->tex);
        vc->tex = NULL;
        return -1;
    }

    vc->ssmpi = alloc_mpi(width, height, format);

    resize(vo, d_width, d_height);

    SDL_DisableScreenSaver();

    if (flags & VOFLAG_FULLSCREEN)
        set_fullscreen(vo, 1);

    SDL_SetWindowTitle(vc->window, vo_get_window_title(vo));

    SDL_ShowWindow(vc->window);

    check_resize(vo);

    return 0;
}

static void flip_page(struct vo *vo)
{
    struct priv *vc = vo->priv;
    SDL_RenderPresent(vc->renderer);
}

static void check_events(struct vo *vo)
{
    struct priv *vc = vo->priv;
    struct MPOpts *opts = vo->opts;
    SDL_Event ev;

    if (opts->cursor_autohide_delay >= 0) {
        if (!vc->mouse_hidden &&
            (GetTimerMS() - vc->mouse_timer >= opts->cursor_autohide_delay)) {
            SDL_ShowCursor(0);
            vc->mouse_hidden = 1;
        }
    } else if (opts->cursor_autohide_delay == -1) {
        if (vc->mouse_hidden) {
            SDL_ShowCursor(1);
            vc->mouse_hidden = 0;
        }
    } else if (opts->cursor_autohide_delay == -2) {
        if (!vc->mouse_hidden) {
            SDL_ShowCursor(0);
            vc->mouse_hidden = 1;
        }
    }

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_WINDOWEVENT:
            switch (ev.window.event) {
            case SDL_WINDOWEVENT_EXPOSED:
                vo->want_redraw = true;
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                check_resize(vo);
                break;
            }
            break;
        case SDL_QUIT:
            mplayer_put_key(vo->key_fifo, KEY_CLOSE_WIN);
            break;
        case SDL_TEXTINPUT: {
            int sdl_mod = SDL_GetModState();
            int mpv_mod = 0;
            // we ignore KMOD_LSHIFT, KMOD_RSHIFT and KMOD_RALT because
            // these are already factored into ev.text.text
            if (sdl_mod & (KMOD_LCTRL | KMOD_RCTRL))
                mpv_mod |= KEY_MODIFIER_CTRL;
            if (sdl_mod & KMOD_LALT)
                mpv_mod |= KEY_MODIFIER_ALT;
            if (sdl_mod & (KMOD_LGUI | KMOD_RGUI))
                mpv_mod |= KEY_MODIFIER_META;
            struct bstr t = {
                ev.text.text, strlen(ev.text.text)
            };
            mplayer_put_key_utf8(vo->key_fifo, mpv_mod, t);
            break;
        }
        case SDL_KEYDOWN: {
            // Issue: we don't know in advance whether this keydown event
            // will ALSO cause a SDL_TEXTINPUT event
            // So we're conservative, and only map non printable keycodes
            // (e.g. function keys, arrow keys, etc.)
            // However, this does lose some keypresses at least on X11
            // (e.g. Ctrl-A generates SDL_KEYDOWN only, but the key is
            // 'a'... and 'a' is normally also handled by SDL_TEXTINPUT).
            // The default config does not use Ctrl, so this is fine...
            int keycode = 0;
            int i;
            for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
                if (keys[i].sdl == ev.key.keysym.sym) {
                    keycode = keys[i].mpv;
                    break;
                }
            if (keycode) {
                if (ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
                    keycode |= KEY_MODIFIER_SHIFT;
                if (ev.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
                    keycode |= KEY_MODIFIER_CTRL;
                if (ev.key.keysym.mod & (KMOD_LALT | KMOD_RALT))
                    keycode |= KEY_MODIFIER_ALT;
                if (ev.key.keysym.mod & (KMOD_LGUI | KMOD_RGUI))
                    keycode |= KEY_MODIFIER_META;
                mplayer_put_key(vo->key_fifo, keycode);
            }
            break;
        }
        case SDL_MOUSEMOTION:
            if (opts->cursor_autohide_delay >= 0) {
                SDL_ShowCursor(1);
                vc->mouse_hidden = 0;
                vc->mouse_timer = GetTimerMS();
            }
            vo_mouse_movement(vo, ev.motion.x, ev.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (opts->cursor_autohide_delay >= 0) {
                SDL_ShowCursor(1);
                vc->mouse_hidden = 0;
                vc->mouse_timer = GetTimerMS();
            }
            mplayer_put_key(vo->key_fifo,
                            (MOUSE_BTN0 + ev.button.button - 1) | MP_KEY_DOWN);
            break;
        case SDL_MOUSEBUTTONUP:
            if (opts->cursor_autohide_delay >= 0) {
                SDL_ShowCursor(1);
                vc->mouse_hidden = 0;
                vc->mouse_timer = GetTimerMS();
            }
            mplayer_put_key(vo->key_fifo,
                            (MOUSE_BTN0 + ev.button.button - 1));
            break;
        case SDL_MOUSEWHEEL:
            break;
        }
    }
}

static void uninit(struct vo *vo)
{
    struct priv *vc = vo->priv;
    destroy_renderer(vo);
    free_mp_image(vc->ssmpi);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    talloc_free(vc);
}

static void subbitmap_to_texture(struct vo *vo, SDL_Texture *tex,
                                 struct sub_bitmap *bmp,
                                 uint32_t andmask, uint32_t ormask)
{
    struct priv *vc = vo->priv;

    uint32_t *temppixels;
    temppixels = talloc_array(vo, uint32_t, bmp->w * bmp->h);

    // apply pixel masks
    int x, y;
    for (y = 0; y < bmp->h; ++y) {
        const uint32_t *src =
            (const uint32_t *) ((const char *) bmp->bitmap + y * bmp->stride);
        uint32_t *dst = temppixels + y * bmp->w;
        for (x = 0; x < bmp->w; ++x)
            dst[x] = (src[x] & andmask) | ormask;
    }

    // convert to SDL's format and upload
    void *pixels;
    int pitch;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] Could not lock texture\n");
    } else {
        SDL_ConvertPixels(bmp->w, bmp->h, SDL_PIXELFORMAT_ARGB8888,
                          temppixels, sizeof(uint32_t) * bmp->w,
                          vc->osd_format.sdl,
                          pixels, pitch);
        SDL_UnlockTexture(tex);
    }

    talloc_free(temppixels);
}

static void generate_osd_part(struct vo *vo, struct sub_bitmaps *imgs)
{
    struct priv *vc = vo->priv;
    struct osd_bitmap_surface *sfc = &vc->osd_surfaces[imgs->render_index];

    if (imgs->format == SUBBITMAP_EMPTY || imgs->num_parts == 0)
        return;

    if (imgs->bitmap_pos_id == sfc->bitmap_pos_id)
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

        if (imgs->bitmap_id != sfc->bitmap_id || !target->tex) {
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
                mp_msg(MSGT_VO, MSGL_ERR, "[sdl] Could not create texture\n");
            }
            if (target->tex) {
                SDL_SetTextureBlendMode(target->tex,
                                        SDL_BLENDMODE_BLEND);
                subbitmap_to_texture(vo, target->tex, bmp,
                                     0xFF000000, 0x00000000); // RGBA -> 000A
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
                mp_msg(MSGT_VO, MSGL_ERR, "[sdl] Could not create texture\n");
            }
            if (target->tex2) {
                SDL_SetTextureBlendMode(target->tex2,
                                        SDL_BLENDMODE_ADD);
                subbitmap_to_texture(vo, target->tex2, bmp,
                                     0x00FFFFFF, 0xFF000000); // RGBA -> RGB1
            }
        }
    }

    sfc->bitmap_id = imgs->bitmap_id;
    sfc->bitmap_pos_id = imgs->bitmap_pos_id;
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

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct priv *vc = vo->priv;

    static const bool formats[SUBBITMAP_COUNT] = {
        [SUBBITMAP_RGBA] = true,
    };

    osd_draw(osd, vc->osd_res, osd->vo_pts, 0, formats, draw_osd_cb, vo);
}

static int preinit(struct vo *vo, const char *arg)
{
    struct priv *vc = vo->priv;

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] already initialized\n");
        return -1;
    }

    // work around broken XVidMode support for now (TODO regularily test
    // whether this is still necessary)
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_X11_XVIDMODE, "0",
                            SDL_HINT_DEFAULT);

    // predefine SDL defaults (SDL env vars shall override)
    SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1",
                            SDL_HINT_DEFAULT);

    // predefine MPV options (SDL env vars shall be overridden)
    if (vo_vsync)
        SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC, "1",
                                SDL_HINT_OVERRIDE);
    else
        SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC, "0",
                                SDL_HINT_OVERRIDE);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_Init failed\n");
        return -1;
    }

    // try creating a renderer (this also gets the renderer_info data
    // for query_format to use!)
    if (init_renderer(vo, 640, 480) != 0)
        return -1;

    // please reinitialize the renderer to proper size on config()
    vc->reinit_renderer = true;

    // we don't have proper event handling
    vo->wakeup_period = 0.02;

    // initialize the autohide timer properly
    vc->mouse_timer = GetTimerMS();

    return 0;
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct priv *vc = vo->priv;
    int i, j;
    int cap = VFCAP_CSP_SUPPORTED | VFCAP_FLIP | VFCAP_ACCEPT_STRIDE |
              VFCAP_OSD;
    for (i = 0; i < vc->renderer_info.num_texture_formats; ++i)
        for (j = 0; j < sizeof(formats) / sizeof(formats[0]); ++j)
            if (vc->renderer_info.texture_formats[i] == formats[j].sdl)
                if (format == formats[j].mpv)
                    return cap;
    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi, double pts)
{
    struct priv *vc = vo->priv;
    void *pixels;
    int pitch;

    // decode brightness/contrast
    int color_add = 0;
    int color_mod = 255;
    int brightness = vc->brightness;
    int contrast = vc->contrast;

    // only in this range it is possible to do brightness/contrast control
    // properly, using just additive render operations and color modding
    // (SDL2 provides no subtractive rendering, sorry)
    if (2 * brightness < contrast) {
        //brightness = (brightness + 2 * contrast) / 5; // closest point
        brightness = (brightness + contrast) / 3; // equal adjustment
        contrast = 2 * brightness;
    }

    // convert to values SDL2 likes
    color_mod = ((contrast + 100) * 255 + 50) / 100;
    color_add = ((2 * brightness - contrast) * 255 + 100) / 200;

    // clamp
    if (color_mod < 0)
        color_mod = 0;
    if (color_mod > 255)
        color_mod = 255;
    // color_add can't be < 0
    if (color_add > 255)
        color_add = 255;

    // typically this runs in parallel with the following copy_mpi call
    SDL_SetRenderDrawColor(vc->renderer, color_add, color_add, color_add, 255);
    SDL_RenderClear(vc->renderer);

    // use additive blending for the video texture only if the clear color is
    // not black (faster especially for the software renderer)
    if (color_add)
        SDL_SetTextureBlendMode(vc->tex, SDL_BLENDMODE_ADD);
    else
        SDL_SetTextureBlendMode(vc->tex, SDL_BLENDMODE_NONE);

    if (mpi) {
        if (SDL_LockTexture(vc->tex, NULL, &pixels, &pitch)) {
            mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_LockTexture failed\n");
            return;
        }

        mp_image_t *texmpi = &vc->texmpi;
        texmpi->planes[0] = pixels;
        texmpi->stride[0] = pitch;
        if (texmpi->num_planes == 3) {
            if (texmpi->imgfmt == IMGFMT_YV12) {
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
        copy_mpi(texmpi, mpi);

        SDL_UnlockTexture(vc->tex);
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

    // typically this runs in parallel with the following copy_mpi call
    if (color_mod > 255) {
        SDL_SetTextureColorMod(vc->tex, color_mod / 2, color_mod / 2, color_mod / 2);
        SDL_RenderCopy(vc->renderer, vc->tex, &src, &dst);
        SDL_RenderCopy(vc->renderer, vc->tex, &src, &dst);
    } else {
        SDL_SetTextureColorMod(vc->tex, color_mod, color_mod, color_mod);
        SDL_RenderCopy(vc->renderer, vc->tex, &src, &dst);
    }
    if (mpi)
        copy_mpi(vc->ssmpi, mpi);
}

static void update_screeninfo(struct vo *vo)
{
    struct priv *vc = vo->priv;
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplay(vc->window), &mode)) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_GetCurrentDisplayMode failed\n");
        return;
    }
    struct MPOpts *opts = vo->opts;
    opts->vo_screenwidth = mode.w;
    opts->vo_screenheight = mode.h;
    aspect_save_screenres(vo, opts->vo_screenwidth, opts->vo_screenheight);
}

static struct mp_image *get_screenshot(struct vo *vo)
{
    struct priv *vc = vo->priv;
    mp_image_t *image = alloc_mpi(vc->ssmpi->width, vc->ssmpi->height,
                                  vc->ssmpi->imgfmt);
    copy_mpi(image, vc->ssmpi);
    return image;
}

static struct mp_image *get_window_screenshot(struct vo *vo)
{
    struct priv *vc = vo->priv;
    mp_image_t *image = alloc_mpi(vo->dwidth, vo->dheight, vc->osd_format.mpv);
    if (SDL_RenderReadPixels(vc->renderer, NULL, vc->osd_format.sdl,
                             image->planes[0], image->stride[0])) {
        mp_msg(MSGT_VO, MSGL_ERR, "[sdl] SDL_RenderReadPixels failed\n");
        free_mp_image(image);
        return NULL;
    }
    return image;
}

static int set_eq(struct vo *vo, const char *name, int value)
{
    struct priv *vc = vo->priv;

    if (!strcasecmp(name, "brightness"))
        vc->brightness = value;
    else if (!strcasecmp(name, "contrast"))
        vc->contrast = value;
    else
        return VO_NOTIMPL;

    vo->want_redraw = true;

    return VO_TRUE;
}

static int get_eq(struct vo *vo, const char *name, int *value)
{
    struct priv *vc = vo->priv;

    if (!strcasecmp(name, "brightness"))
        *value = vc->brightness;
    else if (!strcasecmp(name, "contrast"))
        *value = vc->contrast;
    else
        return VO_NOTIMPL;

    return VO_TRUE;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct priv *vc = vo->priv;
    switch (request) {
    case VOCTRL_QUERY_FORMAT:
        return query_format(vo, *((uint32_t *)data));
    case VOCTRL_DRAW_IMAGE:
        draw_image(vo, (mp_image_t *)data, vo->next_pts);
        return 0;
    case VOCTRL_FULLSCREEN:
        set_fullscreen(vo, !vo_fs);
        return 1;
    case VOCTRL_PAUSE:
        return vc->int_pause = 1;
    case VOCTRL_RESUME:
        return vc->int_pause = 0;
    case VOCTRL_REDRAW_FRAME:
        draw_image(vo, NULL, MP_NOPTS_VALUE);
        return 1;
    case VOCTRL_UPDATE_SCREENINFO:
        update_screeninfo(vo);
        return 1;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        force_resize(vo);
        return VO_TRUE;
    case VOCTRL_SET_EQUALIZER: {
        struct voctrl_set_equalizer_args *args = data;
        return set_eq(vo, args->name, args->value);
    }
    case VOCTRL_GET_EQUALIZER: {
        struct voctrl_get_equalizer_args *args = data;
        return get_eq(vo, args->name, args->valueptr);
    }
    case VOCTRL_SCREENSHOT: {
        struct voctrl_screenshot_args *args = data;
        if (args->full_window)
            args->out_image = get_window_screenshot(vo);
        else
            args->out_image = get_screenshot(vo);
        return true;
    }
    }
    return VO_NOTIMPL;
}

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct priv

const struct vo_driver video_out_sdl = {
    .is_new = true,
    .info = &(const vo_info_t) {
        "SDL 2.0 Renderer",
        "sdl",
        "Rudolf Polzer <divVerent@xonotic.org>",
        ""
    },
    .priv_size = sizeof(struct priv),
    .preinit = preinit,
    .config = config,
    .control = control,
    .uninit = uninit,
    .check_events = check_events,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
};
