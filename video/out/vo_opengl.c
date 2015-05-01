/*
 * Based on vo_gl.c by Reimar Doeffinger.
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
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>

#include <libavutil/common.h>

#include "config.h"

#include "talloc.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "vo.h"
#include "video/mp_image.h"
#include "sub/osd.h"

#include "gl_common.h"
#include "gl_utils.h"
#include "gl_hwdec.h"
#include "gl_osd.h"
#include "filter_kernels.h"
#include "video/hwdec.h"
#include "gl_video.h"
#include "gl_lcms.h"

struct gl_priv {
    struct vo *vo;
    struct mp_log *log;
    MPGLContext *glctx;
    GL *gl;

    struct gl_video *renderer;
    struct gl_lcms *cms;

    struct gl_hwdec *hwdec;
    struct mp_hwdec_info hwdec_info;

    // Options
    struct gl_video_opts *renderer_opts;
    struct mp_icc_opts *icc_opts;
    int use_glFinish;
    int waitvsync;
    int use_gl_debug;
    int allow_sw;
    int swap_interval;
    int current_swap_interval;
    int dwm_flush;

    char *backend;

    bool frame_started;

    int frames_rendered;
    unsigned int prev_sgi_sync_count;

    // check-pattern sub-option; for testing/debugging
    int opt_pattern[2];
    int last_pattern;
    int matches, mismatches;
};

// Always called under mpgl_lock
static void resize(struct gl_priv *p)
{
    struct vo *vo = p->vo;

    MP_VERBOSE(vo, "Resize: %dx%d\n", vo->dwidth, vo->dheight);

    struct mp_rect src, dst;
    struct mp_osd_res osd;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);

    gl_video_resize(p->renderer, vo->dwidth, -vo->dheight, &src, &dst, &osd);

    vo->want_redraw = true;
}

static void check_pattern(struct vo *vo, int item)
{
    struct gl_priv *p = vo->priv;
    int expected = p->opt_pattern[p->last_pattern];
    if (item == expected) {
        p->last_pattern++;
        if (p->last_pattern >= 2)
            p->last_pattern = 0;
        p->matches++;
    } else {
        p->mismatches++;
        MP_WARN(vo, "wrong pattern, expected %d got %d (hit: %d, mis: %d)\n",
                expected, item, p->matches, p->mismatches);
    }
}

static void flip_page(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (!p->frame_started) {
        vo_increment_drop_count(vo, 1);
        return;
    }
    p->frame_started = false;

    mpgl_lock(p->glctx);

    p->glctx->swapGlBuffers(p->glctx);

    p->frames_rendered++;
    if (p->frames_rendered > 5 && !p->use_gl_debug)
        gl_video_set_debug(p->renderer, false);

    if (p->use_glFinish)
        gl->Finish();

    if (p->waitvsync || p->opt_pattern[0]) {
        if (gl->GetVideoSync) {
            unsigned int n1 = 0, n2 = 0;
            gl->GetVideoSync(&n1);
            if (p->waitvsync)
                gl->WaitVideoSync(2, (n1 + 1) % 2, &n2);
            int step = n1 - p->prev_sgi_sync_count;
            p->prev_sgi_sync_count = n1;
            MP_DBG(vo, "Flip counts: %u->%u, step=%d\n", n1, n2, step);
            if (p->opt_pattern[0])
                check_pattern(vo, step);
        } else {
            MP_WARN(vo, "GLX_SGI_video_sync not available, disabling.\n");
            p->waitvsync = 0;
            p->opt_pattern[0] = 0;
        }
    }

    if (p->glctx->DwmFlush) {
        p->current_swap_interval = p->glctx->DwmFlush(p->glctx, p->dwm_flush,
                                                      p->swap_interval,
                                                      p->current_swap_interval);
    }

    mpgl_unlock(p->glctx);
}

static void draw_image_timed(struct vo *vo, mp_image_t *mpi,
                             struct frame_timing *t)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    mpgl_lock(p->glctx);

    if (mpi)
        gl_video_set_image(p->renderer, mpi);

    if (p->glctx->start_frame && !p->glctx->start_frame(p->glctx)) {
        mpgl_unlock(p->glctx);
        return;
    }

    p->frame_started = true;
    gl_video_render_frame(p->renderer, 0, t);

    // The playloop calls this last before waiting some time until it decides
    // to call flip_page(). Tell OpenGL to start execution of the GPU commands
    // while we sleep (this happens asynchronously).
    gl->Flush();

    if (p->use_glFinish)
        gl->Finish();

    mpgl_unlock(p->glctx);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    draw_image_timed(vo, mpi, NULL);
}

static int query_format(struct vo *vo, int format)
{
    struct gl_priv *p = vo->priv;
    if (!gl_video_check_format(p->renderer, format))
        return 0;
    return 1;
}

static void video_resize_redraw_callback(struct vo *vo, int w, int h)
{
    struct gl_priv *p = vo->priv;
    gl_video_resize_redraw(p->renderer, w, -h);

}

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct gl_priv *p = vo->priv;

    mpgl_lock(p->glctx);

    if (!mpgl_reconfig_window(p->glctx, flags)) {
        mpgl_unlock(p->glctx);
        return -1;
    }

    if (p->glctx->register_resize_callback) {
        p->glctx->register_resize_callback(vo, video_resize_redraw_callback);
    }

    resize(p);

    gl_video_config(p->renderer, params);

    mpgl_unlock(p->glctx);

    return 0;
}

static void request_hwdec_api(struct gl_priv *p, const char *api_name)
{
    if (p->hwdec)
        return;
    mpgl_lock(p->glctx);
    p->hwdec = gl_hwdec_load_api(p->vo->log, p->gl, api_name);
    gl_video_set_hwdec(p->renderer, p->hwdec);
    if (p->hwdec)
        p->hwdec_info.hwctx = p->hwdec->hwctx;
    mpgl_unlock(p->glctx);
}

static void call_request_hwdec_api(struct mp_hwdec_info *info,
                                   const char *api_name)
{
    struct vo *vo = info->load_api_ctx;
    assert(&((struct gl_priv *)vo->priv)->hwdec_info == info);
    // Roundabout way to run hwdec loading on the VO thread.
    // Redirects to request_hwdec_api().
    vo_control(vo, VOCTRL_LOAD_HWDEC_API, (void *)api_name);
}

static bool get_and_update_icc_profile(struct gl_priv *p, int *events)
{
    if (p->icc_opts->profile_auto) {
        MP_VERBOSE(p, "Querying ICC profile...\n");
        bstr icc = bstr0(NULL);
        int r = p->glctx->vo_control(p->vo, events, VOCTRL_GET_ICC_PROFILE, &icc);

        if (r != VO_NOTAVAIL) {
            if (r == VO_FALSE) {
                MP_WARN(p, "Could not retrieve an ICC profile.\n");
            } else if (r == VO_NOTIMPL) {
                MP_ERR(p, "icc-profile-auto not implemented on this platform.\n");
            }

            gl_lcms_set_memory_profile(p->cms, &icc);
        }
    }

    struct lut3d *lut3d = NULL;
    if (!gl_lcms_has_changed(p->cms))
        return true;
    if (gl_lcms_get_lut3d(p->cms, &lut3d) && !lut3d)
        return false;
    gl_video_set_lut3d(p->renderer, lut3d);
    talloc_free(lut3d);
    return true;
}

static void get_and_update_ambient_lighting(struct gl_priv *p, int *events)
{
    int lux;
    int r = p->glctx->vo_control(p->vo, events, VOCTRL_GET_AMBIENT_LUX, &lux);
    if (r == VO_TRUE) {
        gl_video_set_ambient_lux(p->renderer, lux);
    }
    if (r != VO_TRUE && p->renderer_opts->gamma_auto) {
        MP_ERR(p, "gamma_auto option provided, but querying for ambient"
                  " lighting is not supported on this platform\n");
    }
}

static bool reparse_cmdline(struct gl_priv *p, char *args)
{
    struct m_config *cfg = NULL;
    struct gl_priv *opts = NULL;
    int r = 0;

    // list of options which can be changed at runtime
#define OPT_BASE_STRUCT struct gl_priv
    static const struct m_option change_otps[] = {
        OPT_SUBSTRUCT("", renderer_opts, gl_video_conf, 0),
        {0}
    };
#undef OPT_BASE_STRUCT

    if (strcmp(args, "-") == 0) {
        opts = p;
    } else {
        const struct gl_priv *vodef = p->vo->driver->priv_defaults;
        cfg = m_config_new(NULL, p->vo->log, sizeof(*opts), vodef, change_otps);
        opts = cfg->optstruct;
        r = m_config_parse_suboptions(cfg, "opengl", args);
    }

    if (r >= 0) {
        mpgl_lock(p->glctx);
        int queue = 0;
        gl_video_set_options(p->renderer, opts->renderer_opts, &queue);
        vo_set_flip_queue_params(p->vo, queue, opts->renderer_opts->interpolation);
        p->vo->want_redraw = true;
        mpgl_unlock(p->glctx);
    }

    talloc_free(cfg);
    return r >= 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct gl_priv *p = vo->priv;

    switch (request) {
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        mpgl_lock(p->glctx);
        resize(p);
        mpgl_unlock(p->glctx);
        return VO_TRUE;
    case VOCTRL_GET_EQUALIZER: {
        struct voctrl_get_equalizer_args *args = data;
        mpgl_lock(p->glctx);
        struct mp_csp_equalizer *eq = gl_video_eq_ptr(p->renderer);
        bool r = mp_csp_equalizer_get(eq, args->name, args->valueptr) >= 0;
        mpgl_unlock(p->glctx);
        return r ? VO_TRUE : VO_NOTIMPL;
    }
    case VOCTRL_SET_EQUALIZER: {
        struct voctrl_set_equalizer_args *args = data;
        mpgl_lock(p->glctx);
        struct mp_csp_equalizer *eq = gl_video_eq_ptr(p->renderer);
        bool r = mp_csp_equalizer_set(eq, args->name, args->value) >= 0;
        if (r)
            gl_video_eq_update(p->renderer);
        mpgl_unlock(p->glctx);
        if (r)
            vo->want_redraw = true;
        return r ? VO_TRUE : VO_NOTIMPL;
    }
    case VOCTRL_SCREENSHOT_WIN:
        mpgl_lock(p->glctx);
        *(struct mp_image **)data = glGetWindowScreenshot(p->gl);
        mpgl_unlock(p->glctx);
        return true;
    case VOCTRL_GET_HWDEC_INFO: {
        struct mp_hwdec_info **arg = data;
        *arg = &p->hwdec_info;
        return true;
    }
    case VOCTRL_LOAD_HWDEC_API:
        request_hwdec_api(p, data);
        return true;
    case VOCTRL_REDRAW_FRAME:
        mpgl_lock(p->glctx);
        if (!(p->glctx->start_frame && !p->glctx->start_frame(p->glctx))) {
            p->frame_started = true;
            gl_video_render_frame(p->renderer, 0, NULL);
        }
        mpgl_unlock(p->glctx);
        return true;
    case VOCTRL_SET_COMMAND_LINE: {
        char *arg = data;
        return reparse_cmdline(p, arg);
    }
    case VOCTRL_RESET:
        mpgl_lock(p->glctx);
        gl_video_reset(p->renderer);
        mpgl_unlock(p->glctx);
        return true;
    case VOCTRL_PAUSE:
        mpgl_lock(p->glctx);
        if (gl_video_showing_interpolated_frame(p->renderer))
            vo->want_redraw = true;
        mpgl_unlock(p->glctx);
        return true;
    }

    mpgl_lock(p->glctx);
    int events = 0;
    int r = p->glctx->vo_control(vo, &events, request, data);
    if (events & VO_EVENT_ICC_PROFILE_CHANGED) {
        get_and_update_icc_profile(p, &events);
        vo->want_redraw = true;
    }
    if (events & VO_EVENT_AMBIENT_LIGHTING_CHANGED) {
        get_and_update_ambient_lighting(p, &events);
        vo->want_redraw = true;
    }
    if (events & VO_EVENT_RESIZE)
        resize(p);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    vo_event(vo, events);
    mpgl_unlock(p->glctx);

    return r;
}

static void uninit(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    gl_video_uninit(p->renderer);
    gl_hwdec_uninit(p->hwdec);
    mpgl_uninit(p->glctx);
}

static int preinit(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    p->vo = vo;
    p->log = vo->log;

    int vo_flags = 0;

    if (p->renderer_opts->alpha_mode == 1)
        vo_flags |= VOFLAG_ALPHA;

    if (p->use_gl_debug)
        vo_flags |= VOFLAG_GL_DEBUG;

    if (p->allow_sw)
        vo->probing = false;

    p->glctx = mpgl_init(vo, p->backend, vo_flags);
    if (!p->glctx)
        goto err_out;
    p->gl = p->glctx->gl;

    mpgl_lock(p->glctx);

    if (p->gl->SwapInterval) {
        p->gl->SwapInterval(p->swap_interval);
    } else {
        MP_VERBOSE(vo, "swap_control extension missing.\n");
    }
    p->current_swap_interval = p->swap_interval;

    p->renderer = gl_video_init(p->gl, vo->log);
    if (!p->renderer)
        goto err_out;
    gl_video_set_osd_source(p->renderer, vo->osd);
    gl_video_set_output_depth(p->renderer, p->glctx->depth_r, p->glctx->depth_g,
                              p->glctx->depth_b);
    int queue = 0;
    gl_video_set_options(p->renderer, p->renderer_opts, &queue);
    vo_set_flip_queue_params(p->vo, queue, p->renderer_opts->interpolation);

    p->cms = gl_lcms_init(p, vo->log, vo->global);
    if (!p->cms)
        goto err_out;
    gl_lcms_set_options(p->cms, p->icc_opts);
    if (!get_and_update_icc_profile(p, &(int){0}))
        goto err_out;

    mpgl_unlock(p->glctx);

    p->hwdec_info.load_api = call_request_hwdec_api;
    p->hwdec_info.load_api_ctx = vo;

    return 0;

err_out:
    uninit(vo);
    return -1;
}

#define OPT_BASE_STRUCT struct gl_priv
static const struct m_option options[] = {
    OPT_FLAG("glfinish", use_glFinish, 0),
    OPT_FLAG("waitvsync", waitvsync, 0),
    OPT_INT("swapinterval", swap_interval, 0, OPTDEF_INT(1)),
    OPT_CHOICE("dwmflush", dwm_flush, 0,
               ({"no", 0}, {"windowed", 1}, {"yes", 2})),
    OPT_FLAG("debug", use_gl_debug, 0),
    OPT_STRING_VALIDATE("backend", backend, 0, mpgl_validate_backend_opt),
    OPT_FLAG("sw", allow_sw, 0),
    OPT_INTPAIR("check-pattern", opt_pattern, 0),

    OPT_SUBSTRUCT("", renderer_opts, gl_video_conf, 0),
    OPT_SUBSTRUCT("", icc_opts, mp_icc_conf, 0),
    {0},
};

#define CAPS VO_CAP_ROTATE90

const struct vo_driver video_out_opengl = {
    .description = "Extended OpenGL Renderer",
    .name = "opengl",
    .caps = CAPS,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .draw_image_timed = draw_image_timed,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct gl_priv),
    .options = options,
};

const struct vo_driver video_out_opengl_hq = {
    .description = "Extended OpenGL Renderer (high quality rendering preset)",
    .name = "opengl-hq",
    .caps = CAPS,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .draw_image_timed = draw_image_timed,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct gl_priv),
    .priv_defaults = &(const struct gl_priv){
        .renderer_opts = (struct gl_video_opts *)&gl_video_opts_hq_def,
    },
    .options = options,
};
