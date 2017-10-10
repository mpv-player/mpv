/*
 * video output driver for libcaca
 *
 * by Pigeon <pigeon@pigeond.net>
 *
 * Some functions/codes/ideas are from x11 and aalib vo
 *
 * TODO: support draw_alpha?
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <caca.h>

#include "config.h"
#include "vo.h"
#include "video/mp_image.h"

#include "input/keycodes.h"
#include "input/input.h"
#include "common/msg.h"
#include "input/input.h"

#include "config.h"
#if !HAVE_GPL
#error GPL only
#endif

struct priv {
    caca_canvas_t  *canvas;
    caca_display_t *display;
    caca_dither_t  *dither;
    uint8_t        *dither_buffer;
    const char     *dither_antialias;
    const char     *dither_charset;
    const char     *dither_color;
    const char     *dither_algo;

    /* image infos */
    int image_format;
    int image_width;
    int image_height;

    int screen_w, screen_h;
};

/* We want 24bpp always for now */
static const unsigned int bpp   = 24;
static const unsigned int depth = 3;
static const unsigned int rmask = 0xff0000;
static const unsigned int gmask = 0x00ff00;
static const unsigned int bmask = 0x0000ff;
static const unsigned int amask = 0;

static int resize(struct vo *vo)
{
    struct priv *priv = vo->priv;
    priv->screen_w = caca_get_canvas_width(priv->canvas);
    priv->screen_h = caca_get_canvas_height(priv->canvas);

    caca_free_dither(priv->dither);
    talloc_free(priv->dither_buffer);

    priv->dither = caca_create_dither(bpp, priv->image_width, priv->image_height,
                                depth * priv->image_width,
                                rmask, gmask, bmask, amask);
    if (priv->dither == NULL) {
        MP_FATAL(vo, "caca_create_dither failed!\n");
        return -1;
    }
    priv->dither_buffer =
        talloc_array(NULL, uint8_t, depth * priv->image_width * priv->image_height);

    /* Default libcaca features */
    caca_set_dither_antialias(priv->dither, priv->dither_antialias);
    caca_set_dither_charset(priv->dither, priv->dither_charset);
    caca_set_dither_color(priv->dither, priv->dither_color);
    caca_set_dither_algorithm(priv->dither, priv->dither_algo);

    return 0;
}

static int reconfig(struct vo *vo, struct mp_image_params *params)
{
    struct priv *priv = vo->priv;
    priv->image_height = params->h;
    priv->image_width  = params->w;
    priv->image_format = params->imgfmt;

    return resize(vo);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct priv *priv = vo->priv;
    memcpy_pic(priv->dither_buffer, mpi->planes[0], priv->image_width * depth, priv->image_height,
               priv->image_width * depth, mpi->stride[0]);
    caca_dither_bitmap(priv->canvas, 0, 0, priv->screen_w, priv->screen_h, priv->dither, priv->dither_buffer);
    talloc_free(mpi);
}

static void flip_page(struct vo *vo)
{
    struct priv *priv = vo->priv;
    caca_refresh_display(priv->display);
}

static void set_next_str(const char * const *list, const char **str,
                         const char **msg)
{
    int ind;
    for (ind = 0; list[ind]; ind += 2) {
        if (strcmp(list[ind], *str) == 0) {
            if (list[ind + 2] == NULL)
                ind = -2;
            *str = list[ind + 2];
            *msg = list[ind + 3];
            return;
        }
    }

    *str = list[0];
    *msg = list[1];
}

static const struct mp_keymap keysym_map[] = {
    {CACA_KEY_RETURN, MP_KEY_ENTER}, {CACA_KEY_ESCAPE, MP_KEY_ESC},
    {CACA_KEY_UP, MP_KEY_UP}, {CACA_KEY_DOWN, MP_KEY_DOWN},
    {CACA_KEY_LEFT, MP_KEY_LEFT}, {CACA_KEY_RIGHT, MP_KEY_RIGHT},
    {CACA_KEY_PAGEUP, MP_KEY_PAGE_UP}, {CACA_KEY_PAGEDOWN, MP_KEY_PAGE_DOWN},
    {CACA_KEY_HOME, MP_KEY_HOME}, {CACA_KEY_END, MP_KEY_END},
    {CACA_KEY_INSERT, MP_KEY_INSERT}, {CACA_KEY_DELETE, MP_KEY_DELETE},
    {CACA_KEY_BACKSPACE, MP_KEY_BACKSPACE}, {CACA_KEY_TAB, MP_KEY_TAB},
    {CACA_KEY_PAUSE, MP_KEY_PAUSE},
    {CACA_KEY_F1, MP_KEY_F+1}, {CACA_KEY_F2, MP_KEY_F+2},
    {CACA_KEY_F3, MP_KEY_F+3}, {CACA_KEY_F4, MP_KEY_F+4},
    {CACA_KEY_F5, MP_KEY_F+5}, {CACA_KEY_F6, MP_KEY_F+6},
    {CACA_KEY_F7, MP_KEY_F+7}, {CACA_KEY_F8, MP_KEY_F+8},
    {CACA_KEY_F9, MP_KEY_F+9}, {CACA_KEY_F10, MP_KEY_F+10},
    {CACA_KEY_F11, MP_KEY_F+11}, {CACA_KEY_F12, MP_KEY_F+12},
    {CACA_KEY_F13, MP_KEY_F+13}, {CACA_KEY_F14, MP_KEY_F+14},
    {CACA_KEY_F15, MP_KEY_F+15},
    {0, 0}
};

static void check_events(struct vo *vo)
{
    struct priv *priv = vo->priv;

    caca_event_t cev;
    while (caca_get_event(priv->display, CACA_EVENT_ANY, &cev, 0)) {

        switch (cev.type) {
        case CACA_EVENT_RESIZE:
            caca_refresh_display(priv->display);
            resize(vo);
            break;
        case CACA_EVENT_QUIT:
            mp_input_put_key(vo->input_ctx, MP_KEY_CLOSE_WIN);
            break;
        case CACA_EVENT_MOUSE_MOTION:
            mp_input_set_mouse_pos(vo->input_ctx, cev.data.mouse.x, cev.data.mouse.y);
            break;
        case CACA_EVENT_MOUSE_PRESS:
            mp_input_put_key(vo->input_ctx,
                    (MP_MBTN_BASE + cev.data.mouse.button - 1) | MP_KEY_STATE_DOWN);
            break;
        case CACA_EVENT_MOUSE_RELEASE:
            mp_input_put_key(vo->input_ctx,
                    (MP_MBTN_BASE + cev.data.mouse.button - 1) | MP_KEY_STATE_UP);
            break;
        case CACA_EVENT_KEY_PRESS:
        {
            int key = cev.data.key.ch;
            int mpkey = lookup_keymap_table(keysym_map, key);
            const char *msg_name;

            if (mpkey)
                mp_input_put_key(vo->input_ctx, mpkey);
            else
            switch (key) {
            case 'd':
            case 'D':
                /* Toggle dithering algorithm */
                set_next_str(caca_get_dither_algorithm_list(priv->dither),
                             &priv->dither_algo, &msg_name);
                caca_set_dither_algorithm(priv->dither, priv->dither_algo);
                break;

            case 'a':
            case 'A':
                /* Toggle antialiasing method */
                set_next_str(caca_get_dither_antialias_list(priv->dither),
                             &priv->dither_antialias, &msg_name);
                caca_set_dither_antialias(priv->dither, priv->dither_antialias);
                break;

            case 'h':
            case 'H':
                /* Toggle charset method */
                set_next_str(caca_get_dither_charset_list(priv->dither),
                             &priv->dither_charset, &msg_name);
                caca_set_dither_charset(priv->dither, priv->dither_charset);
                break;

            case 'c':
            case 'C':
                /* Toggle color method */
                set_next_str(caca_get_dither_color_list(priv->dither),
                             &priv->dither_color, &msg_name);
                caca_set_dither_color(priv->dither, priv->dither_color);
                break;

            default:
                if (key <= 255)
                    mp_input_put_key(vo->input_ctx, key);
                break;
            }
        }
        }
    }
}

static void uninit(struct vo *vo)
{
    struct priv *priv = vo->priv;
    caca_free_dither(priv->dither);
    priv->dither = NULL;
    talloc_free(priv->dither_buffer);
    priv->dither_buffer = NULL;
    caca_free_display(priv->display);
    caca_free_canvas(priv->canvas);
}

static int preinit(struct vo *vo)
{
    struct priv *priv = vo->priv;

    priv->dither_antialias = "default";
    priv->dither_charset   = "default";
    priv->dither_color     = "default";
    priv->dither_algo      = "none";

    priv->canvas = caca_create_canvas(0, 0);
    if (priv->canvas == NULL) {
        MP_ERR(vo, "failed to create canvas\n");
        return ENOSYS;
    }

    priv->display = caca_create_display(priv->canvas);

    if (priv->display == NULL) {
        MP_ERR(vo, "failed to create display\n");
        caca_free_canvas(priv->canvas);
        return ENOSYS;
    }

    caca_set_display_title(priv->display, "mpv");

    return 0;
}

static int query_format(struct vo *vo, int format)
{
    return format == IMGFMT_BGR24;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        check_events(vo);
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_caca = {
    .name = "caca",
    .description = "libcaca",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
