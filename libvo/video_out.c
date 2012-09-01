/*
 * libvo common functions, variables used by many/all drivers.
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
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include <unistd.h>
//#include <sys/mman.h>

#include "config.h"
#include "options.h"
#include "talloc.h"
#include "video_out.h"
#include "aspect.h"
#include "geometry.h"
#include "input/input.h"
#include "mp_fifo.h"
#include "m_config.h"
#include "mp_msg.h"

#include "osdep/shmem.h"
#ifdef CONFIG_X11
#include "x11_common.h"
#endif

int xinerama_screen = -1;
int xinerama_x;
int xinerama_y;

int vo_nomouse_input = 0;
int vo_grabpointer = 1;
int vo_doublebuffering = 1;
int vo_vsync = 1;
int vo_fs = 0;
int vo_fsmode = 0;
float vo_panscan = 0.0f;
int vo_adapter_num=0;
int vo_refresh_rate=0;
int vo_keepaspect=1;
int vo_rootwin=0;
int vo_border=1;
int64_t WinID = -1;

int vo_pts=0; // for hw decoding
float vo_fps=0;

int vo_colorkey = 0x0000ff00; // default colorkey is green
                              // (0xff000000 means that colorkey has been disabled)

//
// Externally visible list of all vo drivers
//
extern struct vo_driver video_out_x11;
extern struct vo_driver video_out_vdpau;
extern struct vo_driver video_out_xv;
extern struct vo_driver video_out_gl_nosw;
extern struct vo_driver video_out_gl;
extern struct vo_driver video_out_gl3;
extern struct vo_driver video_out_null;
extern struct vo_driver video_out_image;
extern struct vo_driver video_out_caca;
extern struct vo_driver video_out_direct3d;
extern struct vo_driver video_out_direct3d_shaders;
extern struct vo_driver video_out_corevideo;

const struct vo_driver *video_out_drivers[] =
{
#ifdef CONFIG_DIRECT3D
        &video_out_direct3d_shaders,
        &video_out_direct3d,
#endif
#ifdef CONFIG_GL_COCOA
        &video_out_gl,
#endif
#ifdef CONFIG_COREVIDEO
        &video_out_corevideo,
#endif
#if CONFIG_VDPAU
        &video_out_vdpau,
#endif
#ifdef CONFIG_XV
        &video_out_xv,
#endif
#ifdef CONFIG_GL
        &video_out_gl3,
#if !defined CONFIG_GL_COCOA
        &video_out_gl,
#endif
#endif
#ifdef CONFIG_X11
        &video_out_x11,
#endif
#ifdef CONFIG_CACA
        &video_out_caca,
#endif
        &video_out_null,
        // should not be auto-selected
        &video_out_image,
#ifdef CONFIG_X11
#ifdef CONFIG_GL
        &video_out_gl_nosw,
#endif
#endif
        NULL
};


static int vo_preinit(struct vo *vo, char *arg)
{
    if (vo->driver->priv_size) {
        vo->priv = talloc_zero_size(vo, vo->driver->priv_size);
        if (vo->driver->priv_defaults)
            memcpy(vo->priv, vo->driver->priv_defaults, vo->driver->priv_size);
    }
    if (vo->driver->options) {
        struct m_config *cfg = m_config_simple(vo->priv);
        talloc_steal(vo->priv, cfg);
        m_config_register_options(cfg, vo->driver->options);
        char n[50];
        int l = snprintf(n, sizeof(n), "vo/%s", vo->driver->info->short_name);
        assert(l < sizeof(n));
        int r = m_config_parse_suboptions(cfg, n, arg);
        if (r < 0)
            return r;
    }
    return vo->driver->preinit(vo, arg);
}

int vo_control(struct vo *vo, uint32_t request, void *data)
{
    return vo->driver->control(vo, request, data);
}

// Return -1 if driver appears not to support a draw_image interface,
// 0 otherwise (whether the driver actually drew something or not).
int vo_draw_image(struct vo *vo, struct mp_image *mpi, double pts)
{
    if (!vo->config_ok)
        return 0;
    if (vo->driver->buffer_frames) {
        vo->driver->draw_image(vo, mpi, pts);
        return 0;
    }
    vo->frame_loaded = true;
    vo->next_pts = pts;
    // Guaranteed to support at least DRAW_IMAGE later
    if (vo->driver->is_new) {
        vo->waiting_mpi = mpi;
        return 0;
    }
    if (vo_control(vo, VOCTRL_DRAW_IMAGE, mpi) == VO_NOTIMPL)
        return -1;
    return 0;
}

int vo_redraw_frame(struct vo *vo)
{
    if (!vo->config_ok || !vo->hasframe)
        return -1;
    if (vo_control(vo, VOCTRL_REDRAW_FRAME, NULL) == true) {
        vo->redrawing = true;
        return 0;
    }
    return -1;
}

int vo_get_buffered_frame(struct vo *vo, bool eof)
{
    if (!vo->config_ok)
        return -1;
    if (vo->frame_loaded)
        return 0;
    if (!vo->driver->buffer_frames)
        return -1;
    vo->driver->get_buffered_frame(vo, eof);
    return vo->frame_loaded ? 0 : -1;
}

void vo_skip_frame(struct vo *vo)
{
    vo_control(vo, VOCTRL_SKIPFRAME, NULL);
    vo->frame_loaded = false;
}

int vo_draw_slice(struct vo *vo, uint8_t *src[], int stride[], int w, int h, int x, int y)
{
    return vo->driver->draw_slice(vo, src, stride, w, h, x, y);
}

void vo_new_frame_imminent(struct vo *vo)
{
    if (!vo->driver->is_new)
        return;
    if (vo->driver->buffer_frames)
        vo_control(vo, VOCTRL_NEWFRAME, NULL);
    else {
        vo_control(vo, VOCTRL_DRAW_IMAGE, vo->waiting_mpi);
        vo->waiting_mpi = NULL;
    }
}

void vo_draw_osd(struct vo *vo, struct osd_state *osd)
{
    if (!vo->config_ok)
        return;
    vo->driver->draw_osd(vo, osd);
}

void vo_flip_page(struct vo *vo, unsigned int pts_us, int duration)
{
    if (!vo->config_ok)
        return;
    if (!vo->redrawing) {
        vo->frame_loaded = false;
        vo->next_pts = MP_NOPTS_VALUE;
    }
    vo->want_redraw = false;
    vo->redrawing = false;
    if (vo->driver->flip_page_timed)
        vo->driver->flip_page_timed(vo, pts_us, duration);
    else
        vo->driver->flip_page(vo);
    vo->hasframe = true;
}

void vo_check_events(struct vo *vo)
{
    if (!vo->config_ok) {
        if (vo->registered_fd != -1)
            mp_input_rm_key_fd(vo->input_ctx, vo->registered_fd);
        vo->registered_fd = -1;
        return;
    }
    vo->driver->check_events(vo);
}

void vo_seek_reset(struct vo *vo)
{
    vo_control(vo, VOCTRL_RESET, NULL);
    vo->frame_loaded = false;
    vo->hasframe = false;
}

void vo_destroy(struct vo *vo)
{
    if (vo->registered_fd != -1)
        mp_input_rm_key_fd(vo->input_ctx, vo->registered_fd);
    vo->driver->uninit(vo);
    talloc_free(vo);
}

void list_video_out(void)
{
    int i = 0;
    mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Available video output drivers:\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_OUTPUTS\n");
    while (video_out_drivers[i]) {
        const vo_info_t *info = video_out_drivers[i++]->info;
        mp_msg(MSGT_GLOBAL, MSGL_INFO,"\t%s\t%s\n", info->short_name, info->name);
    }
    mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

struct vo *init_best_video_out(struct MPOpts *opts,
                               struct mp_fifo *key_fifo,
                               struct input_ctx *input_ctx)
{
    char **vo_list = opts->video_driver_list;
    int i;
    struct vo *vo = talloc_ptrtype(NULL, vo);
    struct vo initial_values = {
        .opts = opts,
        .key_fifo = key_fifo,
        .input_ctx = input_ctx,
        .event_fd = -1,
        .registered_fd = -1,
    };
    // first try the preferred drivers, with their optional subdevice param:
    if (vo_list && vo_list[0])
        while (vo_list[0][0]) {
            char *name = strdup(vo_list[0]);
            char *vo_subdevice = strchr(name,':');
            if (!strcmp(name, "pgm"))
                mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "The pgm video output driver has been replaced by -vo pnm:pgmyuv.\n");
            if (!strcmp(name, "md5"))
                mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "The md5 video output driver has been replaced by -vo md5sum.\n");
            if (vo_subdevice) {
                vo_subdevice[0] = 0;
                ++vo_subdevice;
            }
            for (i = 0; video_out_drivers[i]; i++) {
                const struct vo_driver *video_driver = video_out_drivers[i];
                const vo_info_t *info = video_driver->info;
                if (!strcmp(info->short_name, name)) {
                    // name matches, try it
                    *vo = initial_values;
                    vo->driver = video_driver;
                    if (!vo_preinit(vo, vo_subdevice)) {
                        free(name);
                        return vo; // success!
                    }
                    talloc_free_children(vo);
		}
	    }
            // continue...
            free(name);
            ++vo_list;
            if (!(vo_list[0])) {
                talloc_free(vo);
                return NULL; // do NOT fallback to others
            }
	}
    // now try the rest...
    for (i = 0; video_out_drivers[i]; i++) {
        const struct vo_driver *video_driver = video_out_drivers[i];
        *vo = initial_values;
        vo->driver = video_driver;
        if (!vo_preinit(vo, NULL))
            return vo; // success!
        talloc_free_children(vo);
    }
    talloc_free(vo);
    return NULL;
}

static int event_fd_callback(void *ctx, int fd)
{
    struct vo *vo = ctx;
    vo_check_events(vo);
    return MP_INPUT_NOTHING;
}

int vo_config(struct vo *vo, uint32_t width, uint32_t height,
                     uint32_t d_width, uint32_t d_height, uint32_t flags,
                     uint32_t format)
{
    struct MPOpts *opts = vo->opts;
    panscan_init(vo);
    aspect_save_orig(vo, width, height);
    aspect_save_prescale(vo, d_width, d_height);

    if (vo_control(vo, VOCTRL_UPDATE_SCREENINFO, NULL) == VO_TRUE) {
        aspect(vo, &d_width, &d_height, A_NOZOOM);
        vo->dx = (int)(opts->vo_screenwidth - d_width) / 2;
        vo->dy = (int)(opts->vo_screenheight - d_height) / 2;
        geometry(&vo->dx, &vo->dy, &d_width, &d_height,
                 opts->vo_screenwidth, opts->vo_screenheight);
        geometry_xy_changed |= xinerama_screen >= 0;
        vo->dx += xinerama_x;
        vo->dy += xinerama_y;
        vo->dwidth = d_width;
        vo->dheight = d_height;
    }

    int ret = vo->driver->config(vo, width, height, d_width, d_height, flags,
                                 format);
    vo->config_ok = (ret == 0);
    vo->config_count += vo->config_ok;
    if (vo->registered_fd == -1 && vo->event_fd != -1 && vo->config_ok) {
        mp_input_add_key_fd(vo->input_ctx, vo->event_fd, 1, event_fd_callback,
                            NULL, vo);
        vo->registered_fd = vo->event_fd;
    }
    vo->frame_loaded = false;
    vo->waiting_mpi = NULL;
    vo->redrawing = false;
    vo->hasframe = false;
    return ret;
}

/**
 * \brief lookup an integer in a table, table must have 0 as the last key
 * \param key key to search for
 * \result translation corresponding to key or "to" value of last mapping
 *         if not found.
 */
int lookup_keymap_table(const struct mp_keymap *map, int key) {
  while (map->from && map->from != key) map++;
  return map->to;
}

/**
 * \brief helper function for the kind of panscan-scaling that needs a source
 *        and destination rectangle like Direct3D and VDPAU
 */
static void src_dst_split_scaling(int src_size, int dst_size, int scaled_src_size,
                                  int *src_start, int *src_end, int *dst_start, int *dst_end) {
  if (scaled_src_size > dst_size) {
    int border = src_size * (scaled_src_size - dst_size) / scaled_src_size;
    // round to a multiple of 2, this is at least needed for vo_direct3d and ATI cards
    border = (border / 2 + 1) & ~1;
    *src_start = border;
    *src_end   = src_size - border;
    *dst_start = 0;
    *dst_end   = dst_size;
  } else {
    *src_start = 0;
    *src_end   = src_size;
    *dst_start = (dst_size - scaled_src_size) / 2;
    *dst_end   = *dst_start + scaled_src_size;
  }
}

/**
 * Calculate the appropriate source and destination rectangle to
 * get a correctly scaled picture, including pan-scan.
 * Can be extended to take future cropping support into account.
 *
 * \param crop specifies the cropping border size in the left, right, top and bottom members, may be NULL
 * \param borders the border values as e.g. EOSD (ASS) and properly placed DVD highlight support requires,
 *                may be NULL and only left and top are currently valid.
 */
void calc_src_dst_rects(struct vo *vo, int src_width, int src_height,
                        struct vo_rect *src, struct vo_rect *dst,
                        struct vo_rect *borders, const struct vo_rect *crop)
{
  static const struct vo_rect no_crop = {0, 0, 0, 0, 0, 0};
  int scaled_width  = 0;
  int scaled_height = 0;
  if (!crop) crop = &no_crop;
  src_width  -= crop->left + crop->right;
  src_height -= crop->top  + crop->bottom;
  if (src_width  < 2) src_width  = 2;
  if (src_height < 2) src_height = 2;
  dst->left = 0; dst->right  = vo->dwidth;
  dst->top  = 0; dst->bottom = vo->dheight;
  src->left = 0; src->right  = src_width;
  src->top  = 0; src->bottom = src_height;
  if (borders) {
    borders->left = 0; borders->top = 0;
  }
  if (aspect_scaling()) {
    aspect(vo, &scaled_width, &scaled_height, A_WINZOOM);
    panscan_calc_windowed(vo);
    scaled_width  += vo->panscan_x;
    scaled_height += vo->panscan_y;
    if (borders) {
      borders->left = (vo->dwidth  - scaled_width ) / 2;
      borders->top  = (vo->dheight - scaled_height) / 2;
    }
    src_dst_split_scaling(src_width, vo->dwidth, scaled_width,
                          &src->left, &src->right, &dst->left, &dst->right);
    src_dst_split_scaling(src_height, vo->dheight, scaled_height,
                          &src->top, &src->bottom, &dst->top, &dst->bottom);
  }
  src->left += crop->left; src->right  += crop->left;
  src->top  += crop->top;  src->bottom += crop->top;
  src->width  = src->right  - src->left;
  src->height = src->bottom - src->top;
  dst->width  = dst->right  - dst->left;
  dst->height = dst->bottom - dst->top;
}

// Return the window title the VO should set. Always returns a null terminated
// string. The string is valid until frontend code is invoked again. Copy it if
// you need to keep the string for an extended period of time.
const char *vo_get_window_title(struct vo *vo)
{
    if (!vo->window_title)
        vo->window_title = talloc_strdup(vo, "");
    return vo->window_title;
}

/**
 * Generates a mouse movement message if those are enable and sends it
 * to the "main" MPlayer.
 *
 * \param posx new x position of mouse
 * \param posy new y position of mouse
 */
void vo_mouse_movement(struct vo *vo, int posx, int posy)
{
  char cmd_str[40];
  if (!enable_mouse_movements)
    return;
  snprintf(cmd_str, sizeof(cmd_str), "set_mouse_pos %i %i", posx, posy);
  mp_input_queue_cmd(vo->input_ctx, mp_input_parse_cmd(cmd_str));
}

#if defined(CONFIG_FBDEV) || defined(CONFIG_VESA)
/* Borrowed from vo_fbdev.c
Monitor ranges related functions*/

char *monitor_hfreq_str = NULL;
char *monitor_vfreq_str = NULL;
char *monitor_dotclock_str = NULL;

float range_max(range_t *r)
{
float max = 0;

	for (/* NOTHING */; (r->min != -1 && r->max != -1); r++)
		if (max < r->max) max = r->max;
	return max;
}


int in_range(range_t *r, float f)
{
	for (/* NOTHING */; (r->min != -1 && r->max != -1); r++)
		if (f >= r->min && f <= r->max)
			return 1;
	return 0;
}

range_t *str2range(char *s)
{
	float tmp_min, tmp_max;
	char *endptr = s;	// to start the loop
	range_t *r = NULL;
	int i;

	if (!s)
		return NULL;
	for (i = 0; *endptr; i++) {
		if (*s == ',')
			goto out_err;
		if (!(r = realloc(r, sizeof(*r) * (i + 2)))) {
			mp_msg(MSGT_GLOBAL, MSGL_WARN,"can't realloc 'r'\n");
			return NULL;
		}
		tmp_min = strtod(s, &endptr);
		if (*endptr == 'k' || *endptr == 'K') {
			tmp_min *= 1000.0;
			endptr++;
		} else if (*endptr == 'm' || *endptr == 'M') {
			tmp_min *= 1000000.0;
			endptr++;
		}
		if (*endptr == '-') {
			tmp_max = strtod(endptr + 1, &endptr);
			if (*endptr == 'k' || *endptr == 'K') {
				tmp_max *= 1000.0;
				endptr++;
			} else if (*endptr == 'm' || *endptr == 'M') {
				tmp_max *= 1000000.0;
				endptr++;
			}
			if (*endptr != ',' && *endptr)
				goto out_err;
		} else if (*endptr == ',' || !*endptr) {
			tmp_max = tmp_min;
		} else
			goto out_err;
		r[i].min = tmp_min;
		r[i].max = tmp_max;
		if (r[i].min < 0 || r[i].max < 0)
			goto out_err;
		s = endptr + 1;
	}
	r[i].min = r[i].max = -1;
	return r;
out_err:
	free(r);
	return NULL;
}

/* Borrowed from vo_fbdev.c END */
#endif
