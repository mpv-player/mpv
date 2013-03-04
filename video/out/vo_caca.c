/*
 * video output driver for libcaca
 *
 * by Pigeon <pigeon@pigeond.net>
 *
 * Some functions/codes/ideas are from x11 and aalib vo
 *
 * TODO: support draw_alpha?
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
#include <caca.h>

#include "config.h"
#include "vo.h"
#include "video/mp_image.h"
#include "video/vfcap.h"
#include "video/memcpy_pic.h"

#include "core/input/keycodes.h"
#include "core/input/input.h"
#include "core/mp_msg.h"
#include "core/mp_fifo.h"

/* caca stuff */
static caca_canvas_t  *canvas;
static caca_display_t *display;
static caca_dither_t  *dither           = NULL;
static uint8_t        *dither_buffer    = NULL;
static const char     *dither_antialias = "default";
static const char     *dither_charset   = "default";
static const char     *dither_color     = "default";
static const char     *dither_algo      = "none";

/* image infos */
static int image_format;
static int image_width;
static int image_height;

static int screen_w, screen_h;

/* We want 24bpp always for now */
static unsigned int bpp   = 24;
static unsigned int depth = 3;
static unsigned int rmask = 0xff0000;
static unsigned int gmask = 0x00ff00;
static unsigned int bmask = 0x0000ff;
static unsigned int amask = 0;

static int resize(void)
{
    screen_w = caca_get_canvas_width(canvas);
    screen_h = caca_get_canvas_height(canvas);

    caca_free_dither(dither);
    talloc_free(dither_buffer);

    dither = caca_create_dither(bpp, image_width, image_height,
                                depth * image_width,
                                rmask, gmask, bmask, amask);
    if (dither == NULL) {
        mp_msg(MSGT_VO, MSGL_FATAL, "vo_caca: caca_create_dither failed!\n");
        return ENOSYS;
    }
    dither_buffer =
        talloc_array(NULL, uint8_t, depth * image_width * image_height);

    /* Default libcaca features */
    caca_set_dither_antialias(dither, dither_antialias);
    caca_set_dither_charset(dither, dither_charset);
    caca_set_dither_color(dither, dither_color);
    caca_set_dither_algorithm(dither, dither_algo);

    return 0;
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    image_height = height;
    image_width  = width;
    image_format = format;

    return resize();
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    memcpy_pic(dither_buffer, mpi->planes[0], image_width * depth, image_height,
               image_width * depth, mpi->stride[0]);
    caca_dither_bitmap(canvas, 0, 0, screen_w, screen_h, dither, dither_buffer);
}

static void flip_page(struct vo *vo)
{
    caca_refresh_display(display);
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
    caca_event_t cev;
    while (caca_get_event(display, CACA_EVENT_ANY, &cev, 0)) {

        switch (cev.type) {
        case CACA_EVENT_RESIZE:
            caca_refresh_display(display);
            resize();
            break;
        case CACA_EVENT_QUIT:
            mplayer_put_key(vo->key_fifo, MP_KEY_CLOSE_WIN);
            break;
        case CACA_EVENT_MOUSE_MOTION:
            vo_mouse_movement(vo, cev.data.mouse.x, cev.data.mouse.y);
            break;
        case CACA_EVENT_MOUSE_PRESS:
            if (!vo->opts->nomouse_input)
                mplayer_put_key(vo->key_fifo,
                    (MP_MOUSE_BTN0 + cev.data.mouse.button - 1) | MP_KEY_STATE_DOWN);
            break;
        case CACA_EVENT_MOUSE_RELEASE:
            if (!vo->opts->nomouse_input)
                mplayer_put_key(vo->key_fifo,
                                MP_MOUSE_BTN0 + cev.data.mouse.button - 1);
            break;
        case CACA_EVENT_KEY_PRESS:
        {
            int key = cev.data.key.ch;
            int mpkey = lookup_keymap_table(keysym_map, key);
            const char *msg_name;

            if (mpkey)
                mplayer_put_key(vo->key_fifo, mpkey);
            else
            switch (key) {
            case 'd':
            case 'D':
                /* Toggle dithering algorithm */
                set_next_str(caca_get_dither_algorithm_list(dither),
                             &dither_algo, &msg_name);
                caca_set_dither_algorithm(dither, dither_algo);
                break;

            case 'a':
            case 'A':
                /* Toggle antialiasing method */
                set_next_str(caca_get_dither_antialias_list(dither),
                             &dither_antialias, &msg_name);
                caca_set_dither_antialias(dither, dither_antialias);
                break;

            case 'h':
            case 'H':
                /* Toggle charset method */
                set_next_str(caca_get_dither_charset_list(dither),
                             &dither_charset, &msg_name);
                caca_set_dither_charset(dither, dither_charset);
                break;

            case 'c':
            case 'C':
                /* Toggle color method */
                set_next_str(caca_get_dither_color_list(dither),
                             &dither_color, &msg_name);
                caca_set_dither_color(dither, dither_color);
                break;

            default:
                if (key <= 255)
                    mplayer_put_key(vo->key_fifo, key);
                break;
            }
        }
        }
    }
}

static void uninit(struct vo *vo)
{
    caca_free_dither(dither);
    dither = NULL;
    talloc_free(dither_buffer);
    dither_buffer = NULL;
    caca_free_display(display);
    caca_free_canvas(canvas);
}

static int preinit(struct vo *vo, const char *arg)
{
    if (arg) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_caca: Unknown subdevice: %s\n", arg);
        return ENOSYS;
    }

    canvas = caca_create_canvas(0, 0);
    if (canvas == NULL) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_caca: failed to create canvas\n");
        return ENOSYS;
    }

    display = caca_create_display(canvas);

    if (display == NULL) {
        mp_msg(MSGT_VO, MSGL_ERR, "vo_caca: failed to create display\n");
        caca_free_canvas(canvas);
        return ENOSYS;
    }

    caca_set_display_title(display, "mpv");

    return 0;
}

static int query_format(struct vo *vo, uint32_t format)
{
    if (format == IMGFMT_BGR24)
        return VFCAP_CSP_SUPPORTED;

    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

const struct vo_driver video_out_caca = {
    .info = &(const vo_info_t) {
        "libcaca",
        "caca",
        "Pigeon <pigeon@pigeond.net>",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};
