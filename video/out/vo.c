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

#include "talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "core/options.h"
#include "core/bstr.h"
#include "vo.h"
#include "aspect.h"
#include "core/input/input.h"
#include "core/mp_fifo.h"
#include "core/m_config.h"
#include "core/mp_msg.h"
#include "video/mp_image.h"
#include "video/vfcap.h"
#include "sub/sub.h"

//
// Externally visible list of all vo drivers
//
extern struct vo_driver video_out_x11;
extern struct vo_driver video_out_vdpau;
extern struct vo_driver video_out_xv;
extern struct vo_driver video_out_opengl;
extern struct vo_driver video_out_opengl_hq;
extern struct vo_driver video_out_opengl_old;
extern struct vo_driver video_out_null;
extern struct vo_driver video_out_image;
extern struct vo_driver video_out_lavc;
extern struct vo_driver video_out_caca;
extern struct vo_driver video_out_direct3d;
extern struct vo_driver video_out_direct3d_shaders;
extern struct vo_driver video_out_sdl;
extern struct vo_driver video_out_corevideo;

const struct vo_driver *video_out_drivers[] =
{
#if CONFIG_VDPAU
        &video_out_vdpau,
#endif
#ifdef CONFIG_GL
        &video_out_opengl,
#endif
#ifdef CONFIG_DIRECT3D
        &video_out_direct3d_shaders,
        &video_out_direct3d,
#endif
#ifdef CONFIG_COREVIDEO
        &video_out_corevideo,
#endif
#ifdef CONFIG_XV
        &video_out_xv,
#endif
#ifdef CONFIG_SDL2
        &video_out_sdl,
#endif
#ifdef CONFIG_GL
        &video_out_opengl_old,
#endif
#ifdef CONFIG_X11
        &video_out_x11,
#endif
        &video_out_null,
        // should not be auto-selected
        &video_out_image,
#ifdef CONFIG_CACA
        &video_out_caca,
#endif
#ifdef CONFIG_ENCODING
        &video_out_lavc,
#endif
#ifdef CONFIG_GL
        &video_out_opengl_hq,
#endif
        NULL
};


static int vo_preinit(struct vo *vo, char *arg)
{
    if (vo->driver->encode != !!vo->encode_lavc_ctx)
        return -1;
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
int vo_draw_image(struct vo *vo, struct mp_image *mpi)
{
    if (!vo->config_ok)
        return 0;
    if (vo->driver->buffer_frames) {
        vo->driver->draw_image(vo, mpi);
        return 0;
    }
    vo->frame_loaded = true;
    vo->next_pts = mpi->pts;
    assert(!vo->waiting_mpi);
    vo->waiting_mpi = mp_image_new_ref(mpi);
    return 0;
}

int vo_redraw_frame(struct vo *vo)
{
    if (!vo->config_ok || !vo->hasframe)
        return -1;
    if (vo_control(vo, VOCTRL_REDRAW_FRAME, NULL) == true) {
        vo->want_redraw = false;
        vo->redrawing = true;
        return 0;
    }
    return -1;
}

bool vo_get_want_redraw(struct vo *vo)
{
    if (!vo->config_ok || !vo->hasframe)
        return false;
    return vo->want_redraw;
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
    mp_image_unrefp(&vo->waiting_mpi);
}

void vo_new_frame_imminent(struct vo *vo)
{
    if (vo->driver->buffer_frames)
        vo_control(vo, VOCTRL_NEWFRAME, NULL);
    else {
        assert(vo->frame_loaded);
        assert(vo->waiting_mpi);
        assert(vo->waiting_mpi->pts == vo->next_pts);
        vo->driver->draw_image(vo, vo->waiting_mpi);
        mp_image_unrefp(&vo->waiting_mpi);
    }
}

void vo_draw_osd(struct vo *vo, struct osd_state *osd)
{
    if (vo->config_ok && vo->driver->draw_osd)
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
    vo->next_wakeup_time = GetTimerMS() + 60 * 1000;
    if (!vo->config_ok) {
        if (vo->registered_fd != -1)
            mp_input_rm_key_fd(vo->input_ctx, vo->registered_fd);
        vo->registered_fd = -1;
        return;
    }
    vo->driver->check_events(vo);
}

// Return the amount of time vo_check_events() should be called in milliseconds.
// Note: video timing is completely separate from this.
unsigned int vo_get_sleep_time(struct vo *vo)
{
    unsigned int sleep = 60 * 1000;
    if (vo->config_ok && vo->next_wakeup_time) {
        unsigned int now = GetTimerMS();
        sleep = 0;
        if (vo->next_wakeup_time >= now)
            sleep = vo->next_wakeup_time - now;
    }
    return sleep;
}

void vo_seek_reset(struct vo *vo)
{
    vo_control(vo, VOCTRL_RESET, NULL);
    vo->frame_loaded = false;
    vo->hasframe = false;
    mp_image_unrefp(&vo->waiting_mpi);
}

void vo_destroy(struct vo *vo)
{
    if (vo->registered_fd != -1)
        mp_input_rm_key_fd(vo->input_ctx, vo->registered_fd);
    vo->driver->uninit(vo);
    talloc_free(vo->waiting_mpi);
    talloc_free(vo);
}

void list_video_out(void)
{
    mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Available video output drivers:\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_OUTPUTS\n");
    for (int i = 0; video_out_drivers[i]; i++) {
        const vo_info_t *info = video_out_drivers[i]->info;
        if (!video_out_drivers[i]->encode) {
            mp_msg(MSGT_GLOBAL, MSGL_INFO,"\t%s\t%s\n",
                   info->short_name, info->name);
        }
    }
    mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

static void replace_legacy_vo_name(bstr *name)
{
    bstr new = *name;
    if (bstr_equals0(*name, "gl"))
        new = bstr0("opengl");
    if (bstr_equals0(*name, "gl3"))
        new = bstr0("opengl-hq");
    if (!bstr_equals(*name, new)) {
        mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "VO driver '%.*s' has been replaced "
                "with '%.*s'!\n", BSTR_P(*name), BSTR_P(new));
    }
    *name = new;
}

struct vo *init_best_video_out(struct mp_vo_opts *opts,
                               struct mp_fifo *key_fifo,
                               struct input_ctx *input_ctx,
                               struct encode_lavc_context *encode_lavc_ctx)
{
    char **vo_list = opts->video_driver_list;
    int i;
    struct vo *vo = talloc_ptrtype(NULL, vo);
    struct vo initial_values = {
        .opts = opts,
        .key_fifo = key_fifo,
        .encode_lavc_ctx = encode_lavc_ctx,
        .input_ctx = input_ctx,
        .event_fd = -1,
        .registered_fd = -1,
        .aspdat = { .monitor_par = 1 },
    };
    // first try the preferred drivers, with their optional subdevice param:
    if (vo_list && vo_list[0])
        while (vo_list[0][0]) {
            char *arg = vo_list[0];
            bstr name = bstr0(arg);
            char *params = strchr(arg, ':');
            if (params) {
                name = bstr_splice(name, 0, params - arg);
                params++;
            }
            replace_legacy_vo_name(&name);
            for (i = 0; video_out_drivers[i]; i++) {
                const struct vo_driver *video_driver = video_out_drivers[i];
                const vo_info_t *info = video_driver->info;
                if (bstr_equals0(name, info->short_name)) {
                    // name matches, try it
                    *vo = initial_values;
                    vo->driver = video_driver;
                    if (!vo_preinit(vo, params))
                        return vo; // success!
                    talloc_free_children(vo);
		}
	    }
            // continue...
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

// Fit *w/*h into the size specified by geo.
static void apply_autofit(int *w, int *h, int scr_w, int scr_h,
                          struct m_geometry *geo, bool allow_upscale)
{
    if (!geo->wh_valid)
        return;

    int dummy;
    int n_w = *w, n_h = *h;
    m_geometry_apply(&dummy, &dummy, &n_w, &n_h, scr_w, scr_h, geo);

    if (!allow_upscale && *w <= n_w && *h <= n_h)
        return;

    // If aspect mismatches, always make the window smaller than the fit box
    double asp = (double)*w / *h;
    double n_asp = (double)n_w / n_h;
    if (n_asp <= asp) {
        *w = n_w;
        *h = n_w / asp;
    } else {
        *w = n_h * asp;
        *h = n_h;
    }
}

// Set window size (vo->dwidth/dheight) and position (vo->dx/dy) according to
// the video display size d_w/d_h.
// NOTE: currently, all GUI backends do their own handling of window geometry
//       additional to this code. This is to deal with initial window placement,
//       fullscreen handling, avoiding resize on config() with no size change,
//       multi-monitor stuff, and possibly more.
static void determine_window_geometry(struct vo *vo, int d_w, int d_h)
{
    struct mp_vo_opts *opts = vo->opts;

    int scr_w = opts->screenwidth;
    int scr_h = opts->screenheight;

    aspect_calc_monitor(vo, &d_w, &d_h);

    apply_autofit(&d_w, &d_h, scr_w, scr_h, &opts->autofit, true);
    apply_autofit(&d_w, &d_h, scr_w, scr_h, &opts->autofit_larger, false);

    vo->dx = (int)(opts->screenwidth - d_w) / 2;
    vo->dy = (int)(opts->screenheight - d_h) / 2;
    m_geometry_apply(&vo->dx, &vo->dy, &d_w, &d_h, scr_w, scr_h,
                     &opts->geometry);

    vo->dx += vo->xinerama_x;
    vo->dy += vo->xinerama_y;
    vo->dwidth = d_w;
    vo->dheight = d_h;
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
    aspect_save_videores(vo, width, height, d_width, d_height);

    if (vo_control(vo, VOCTRL_UPDATE_SCREENINFO, NULL) == VO_TRUE) {
        determine_window_geometry(vo, d_width, d_height);
        d_width = vo->dwidth;
        d_height = vo->dheight;
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

static void print_video_rect(struct vo *vo, struct mp_rect src,
                             struct mp_rect dst, struct mp_osd_res osd)
{
    int lv = MSGL_V;

    int sw = src.x1 - src.x0, sh = src.y1 - src.y0;
    int dw = dst.x1 - dst.x0, dh = dst.y1 - dst.y0;

    mp_msg(MSGT_VO, lv, "[vo] Window size: %dx%d\n",
           vo->dwidth, vo->dheight);
    mp_msg(MSGT_VO, lv, "[vo] Video source: %dx%d (%dx%d)\n",
           vo->aspdat.orgw, vo->aspdat.orgh,
           vo->aspdat.prew, vo->aspdat.preh);
    mp_msg(MSGT_VO, lv, "[vo] Video display: (%d, %d) %dx%d -> (%d, %d) %dx%d\n",
           src.x0, src.y0, sw, sh, dst.x0, dst.y0, dw, dh);
    mp_msg(MSGT_VO, lv, "[vo] Video scale: %f/%f\n",
           (double)dw / sw, (double)dh / sh);
    mp_msg(MSGT_VO, lv, "[vo] OSD borders: l=%d t=%d r=%d b=%d\n",
           osd.ml, osd.mt, osd.mr, osd.mb);
    mp_msg(MSGT_VO, lv, "[vo] Video borders: l=%d t=%d r=%d b=%d\n",
           dst.x0, dst.y0, vo->dwidth - dst.x1, vo->dheight - dst.y1);
}

static void src_dst_split_scaling(int src_size, int dst_size,
                                  int scaled_src_size, int *src_start,
                                  int *src_end, int *dst_start, int *dst_end)
{
    if (scaled_src_size > dst_size) {
        int border = src_size * (scaled_src_size - dst_size) / scaled_src_size;
        // round to a multiple of 2, this is at least needed for vo_direct3d
        // and ATI cards
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

// Calculate the appropriate source and destination rectangle to
// get a correctly scaled picture, including pan-scan.
// out_src: visible part of the video
// out_dst: area of screen covered by the video source rectangle
// out_osd: OSD size, OSD margins, etc.
void vo_get_src_dst_rects(struct vo *vo, struct mp_rect *out_src,
                          struct mp_rect *out_dst, struct mp_osd_res *out_osd)
{
    int src_w = vo->aspdat.orgw;
    int src_h = vo->aspdat.orgh;
    struct mp_rect dst = {0, 0, vo->dwidth, vo->dheight};
    struct mp_rect src = {0, 0, src_w,      src_h};
    struct mp_osd_res osd = {
        .w = vo->dwidth,
        .h = vo->dheight,
        .display_par = vo->aspdat.monitor_par,
        .video_par = vo->aspdat.par,
    };
    if (vo->opts->keepaspect || vo->opts->fs) {
        int scaled_width, scaled_height;
        aspect_calc_panscan(vo, &scaled_width, &scaled_height);
        int border_w = vo->dwidth  - scaled_width;
        int border_h = vo->dheight - scaled_height;
        osd.ml = border_w / 2;
        osd.mt = border_h / 2;
        osd.mr = border_w - osd.ml;
        osd.mb = border_h - osd.mt;
        src_dst_split_scaling(src_w, vo->dwidth, scaled_width,
                              &src.x0, &src.x1, &dst.x0, &dst.x1);
        src_dst_split_scaling(src_h, vo->dheight, scaled_height,
                              &src.y0, &src.y1, &dst.y0, &dst.y1);
    }

    *out_src = src;
    *out_dst = dst;
    *out_osd = osd;

    print_video_rect(vo, src, dst, osd);
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
  if (!vo->opts->enable_mouse_movements)
    return;
  snprintf(cmd_str, sizeof(cmd_str), "set_mouse_pos %i %i", posx, posy);
  mp_input_queue_cmd(vo->input_ctx, mp_input_parse_cmd(bstr0(cmd_str), ""));
}

