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

#include <libavutil/common.h>

#include "talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "options/options.h"
#include "bstr/bstr.h"
#include "vo.h"
#include "aspect.h"
#include "input/input.h"
#include "options/m_config.h"
#include "common/msg.h"
#include "common/global.h"
#include "video/mp_image.h"
#include "video/vfcap.h"
#include "sub/osd.h"

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
extern struct vo_driver video_out_vaapi;
extern struct vo_driver video_out_wayland;

const struct vo_driver *video_out_drivers[] =
{
#if HAVE_VDPAU
        &video_out_vdpau,
#endif
#if HAVE_GL
        &video_out_opengl,
#endif
#if HAVE_DIRECT3D
        &video_out_direct3d_shaders,
        &video_out_direct3d,
#endif
#if HAVE_COREVIDEO
        &video_out_corevideo,
#endif
#if HAVE_XV
        &video_out_xv,
#endif
#if HAVE_SDL2
        &video_out_sdl,
#endif
#if HAVE_GL
        &video_out_opengl_old,
#endif
#if HAVE_VAAPI
        &video_out_vaapi,
#endif
#if HAVE_X11
        &video_out_x11,
#endif
        &video_out_null,
        // should not be auto-selected
        &video_out_image,
#if HAVE_CACA
        &video_out_caca,
#endif
#if HAVE_ENCODING
        &video_out_lavc,
#endif
#if HAVE_GL
        &video_out_opengl_hq,
#endif
#if HAVE_WAYLAND
        &video_out_wayland,
#endif
        NULL
};

static void forget_frames(struct vo *vo);

static bool get_desc(struct m_obj_desc *dst, int index)
{
    if (index >= MP_ARRAY_SIZE(video_out_drivers) - 1)
        return false;
    const struct vo_driver *vo = video_out_drivers[index];
    *dst = (struct m_obj_desc) {
        .name = vo->name,
        .description = vo->description,
        .priv_size = vo->priv_size,
        .priv_defaults = vo->priv_defaults,
        .options = vo->options,
        .hidden = vo->encode,
        .p = vo,
    };
    return true;
}

// For the vo option
const struct m_obj_list vo_obj_list = {
    .get_desc = get_desc,
    .description = "video outputs",
    .aliases = {
        {"gl",        "opengl"},
        {"gl3",       "opengl-hq"},
        {0}
    },
    .allow_unknown_entries = true,
    .allow_trailer = true,
};

static int event_fd_callback(void *ctx, int fd)
{
    struct vo *vo = ctx;
    vo_check_events(vo);
    return MP_INPUT_NOTHING;
}

static struct vo *vo_create(struct mpv_global *global,
                            struct input_ctx *input_ctx,
                            struct encode_lavc_context *encode_lavc_ctx,
                            char *name, char **args)
{
    struct mp_log *log = mp_log_new(NULL, global->log, "vo");
    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, &vo_obj_list, bstr0(name))) {
        mp_msg(log, MSGL_ERR, "Video output %s not found!\n", name);
        talloc_free(log);
        return NULL;
    };
    struct vo *vo = talloc_ptrtype(NULL, vo);
    *vo = (struct vo) {
        .log = mp_log_new(vo, log, name),
        .driver = desc.p,
        .opts = &global->opts->vo,
        .global = global,
        .encode_lavc_ctx = encode_lavc_ctx,
        .input_ctx = input_ctx,
        .event_fd = -1,
        .monitor_par = 1,
        .max_video_queue = 1,
    };
    talloc_steal(vo, log);
    if (vo->driver->encode != !!vo->encode_lavc_ctx)
        goto error;
    struct m_config *config = m_config_from_obj_desc(vo, vo->log, &desc);
    if (m_config_apply_defaults(config, name, vo->opts->vo_defs) < 0)
        goto error;
    if (m_config_set_obj_params(config, args) < 0)
        goto error;
    vo->priv = config->optstruct;
    if (vo->driver->preinit(vo))
        goto error;
    if (vo->event_fd != -1) {
        mp_input_add_fd(vo->input_ctx, vo->event_fd, 1, NULL, event_fd_callback,
                        NULL, vo);
    }
    return vo;
error:
    talloc_free(vo);
    return NULL;
}

struct vo *init_best_video_out(struct mpv_global *global,
                               struct input_ctx *input_ctx,
                               struct encode_lavc_context *encode_lavc_ctx)
{
    struct m_obj_settings *vo_list = global->opts->vo.video_driver_list;
    // first try the preferred drivers, with their optional subdevice param:
    if (vo_list && vo_list[0].name) {
        for (int n = 0; vo_list[n].name; n++) {
            // Something like "-vo name," allows fallback to autoprobing.
            if (strlen(vo_list[n].name) == 0)
                goto autoprobe;
            struct vo *vo = vo_create(global, input_ctx, encode_lavc_ctx,
                                      vo_list[n].name, vo_list[n].attribs);
            if (vo)
                return vo;
        }
        return NULL;
    }
autoprobe:
    // now try the rest...
    for (int i = 0; video_out_drivers[i]; i++) {
        struct vo *vo = vo_create(global, input_ctx, encode_lavc_ctx,
                                  (char *)video_out_drivers[i]->name, NULL);
        if (vo)
            return vo;
    }
    return NULL;
}

void vo_destroy(struct vo *vo)
{
    if (vo->event_fd != -1)
        mp_input_rm_key_fd(vo->input_ctx, vo->event_fd);
    forget_frames(vo);
    vo->driver->uninit(vo);
    talloc_free(vo);
}

static void check_vo_caps(struct vo *vo)
{
    int rot = vo->params->rotate;
    if (rot) {
        bool ok = rot % 90 ? false : (vo->driver->caps & VO_CAP_ROTATE90);
        if (!ok) {
           MP_WARN(vo, "Video is flagged as rotated by %d degrees, but the "
                   "video output does not support this.\n", rot);
        }
    }
}

int vo_reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    vo->dwidth = params->d_w;
    vo->dheight = params->d_h;

    talloc_free(vo->params);
    vo->params = talloc_memdup(vo, params, sizeof(*params));

    int ret = vo->driver->reconfig(vo, vo->params, flags);
    vo->config_ok = ret >= 0;
    if (vo->config_ok) {
        check_vo_caps(vo);
    } else {
        talloc_free(vo->params);
        vo->params = NULL;
    }
    forget_frames(vo);
    vo->hasframe = false;
    return ret;
}

int vo_control(struct vo *vo, uint32_t request, void *data)
{
    return vo->driver->control(vo, request, data);
}

static void forget_frames(struct vo *vo)
{
    for (int n = 0; n < vo->num_video_queue; n++)
        talloc_free(vo->video_queue[n]);
    vo->num_video_queue = 0;
}

void vo_queue_image(struct vo *vo, struct mp_image *mpi)
{
    assert(mpi);
    if (!vo->config_ok)
        return;
    assert(mp_image_params_equals(vo->params, &mpi->params));
    mpi = mp_image_new_ref(mpi);
    if (vo->driver->filter_image)
        mpi = vo->driver->filter_image(vo, mpi);
    if (!mpi) {
        MP_ERR(vo, "Could not upload image.\n");
        return;
    }
    assert(vo->max_video_queue <= VO_MAX_QUEUE);
    assert(vo->num_video_queue < vo->max_video_queue);
    vo->video_queue[vo->num_video_queue++] = mpi;
}

// Return whether vo_queue_image() should be called.
bool vo_needs_new_image(struct vo *vo)
{
    if (!vo->config_ok)
        return false;
    return vo->num_video_queue < vo->max_video_queue;
}

// Return whether a frame can be displayed.
//  eof==true: return true if at least one frame is queued
//  eof==false: return true if "enough" frames are queued
bool vo_has_next_frame(struct vo *vo, bool eof)
{
    if (!vo->config_ok)
        return false;
    // Normally, buffer 1 image ahead, except if the queue is limited to less
    // than 2 entries, or if EOF is reached and there aren't enough images left.
    return eof ? vo->num_video_queue : vo->num_video_queue == vo->max_video_queue;
}

// Return the PTS of a future frame (where index==0 is the next frame)
double vo_get_next_pts(struct vo *vo, int index)
{
    if (index < 0 || index >= vo->num_video_queue)
        return MP_NOPTS_VALUE;
    return vo->video_queue[index]->pts;
}

int vo_redraw_frame(struct vo *vo)
{
    if (!vo->config_ok)
        return -1;
    if (vo_control(vo, VOCTRL_REDRAW_FRAME, NULL) == true) {
        vo->want_redraw = false;
        return 0;
    }
    return -1;
}

bool vo_get_want_redraw(struct vo *vo)
{
    if (!vo->config_ok)
        return false;
    return vo->want_redraw;
}

// Remove vo->video_queue[0]
static void shift_queue(struct vo *vo)
{
    if (!vo->num_video_queue)
        return;
    talloc_free(vo->video_queue[0]);
    vo->num_video_queue--;
    for (int n = 0; n < vo->num_video_queue; n++)
        vo->video_queue[n] = vo->video_queue[n + 1];
}

void vo_new_frame_imminent(struct vo *vo)
{
    assert(vo->num_video_queue > 0);
    vo->driver->draw_image(vo, vo->video_queue[0]);
    shift_queue(vo);
}

void vo_draw_osd(struct vo *vo, struct osd_state *osd)
{
    if (vo->config_ok && vo->driver->draw_osd)
        vo->driver->draw_osd(vo, osd);
}

void vo_flip_page(struct vo *vo, int64_t pts_us, int duration)
{
    if (!vo->config_ok)
        return;
    vo->want_redraw = false;
    if (vo->driver->flip_page_timed)
        vo->driver->flip_page_timed(vo, pts_us, duration);
    else
        vo->driver->flip_page(vo);
    vo->hasframe = true;
}

void vo_check_events(struct vo *vo)
{
    if (vo->config_ok) {
        vo_control(vo, VOCTRL_CHECK_EVENTS, NULL);
    }
}

void vo_seek_reset(struct vo *vo)
{
    vo_control(vo, VOCTRL_RESET, NULL);
    forget_frames(vo);
    vo->hasframe = false;
}

// Calculate the appropriate source and destination rectangle to
// get a correctly scaled picture, including pan-scan.
// out_src: visible part of the video
// out_dst: area of screen covered by the video source rectangle
// out_osd: OSD size, OSD margins, etc.
void vo_get_src_dst_rects(struct vo *vo, struct mp_rect *out_src,
                          struct mp_rect *out_dst, struct mp_osd_res *out_osd)
{
    if (!vo->params)
        return;
    mp_get_src_dst_rects(vo->log, vo->opts, vo->driver->caps, vo->params,
                         vo->dwidth, vo->dheight, vo->monitor_par,
                         out_src, out_dst, out_osd);
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
    if (!vo->opts->enable_mouse_movements)
        return;
    float p[2] = {posx, posy};
    vo_control(vo, VOCTRL_WINDOW_TO_OSD_COORDS, p);
    mp_input_set_mouse_pos(vo->input_ctx, p[0], p[1]);
}

/**
 * \brief lookup an integer in a table, table must have 0 as the last key
 * \param key key to search for
 * \result translation corresponding to key or "to" value of last mapping
 *         if not found.
 */
int lookup_keymap_table(const struct mp_keymap *map, int key)
{
    while (map->from && map->from != key)
        map++;
    return map->to;
}
