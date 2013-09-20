/*
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
#include "mpvcore/mp_common.h"
#include "mpvcore/bstr.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/m_config.h"
#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "sub/sub.h"

#include "gl_common.h"
#include "gl_osd.h"
#include "filter_kernels.h"
#include "video/memcpy_pic.h"
#include "gl_video.h"
#include "gl_lcms.h"

struct gl_priv {
    struct vo *vo;
    MPGLContext *glctx;
    GL *gl;

    struct gl_video *renderer;

    // Options
    struct gl_video_opts *renderer_opts;
    struct mp_icc_opts *icc_opts;
    int use_glFinish;
    int use_gl_debug;
    int allow_sw;
    int swap_interval;
    char *backend;

    int vo_flipped;

    int frames_rendered;
};

// Always called under mpgl_lock
static void resize(struct gl_priv *p)
{
    struct vo *vo = p->vo;

    MP_VERBOSE(vo, "Resize: %dx%d\n", vo->dwidth, vo->dheight);

    struct mp_rect wnd = {0, 0, vo->dwidth, vo->dheight};
    struct mp_rect src, dst;
    struct mp_osd_res osd;
    vo_get_src_dst_rects(vo, &src, &dst, &osd);

    gl_video_resize(p->renderer, &wnd, &src, &dst, &osd);

    vo->want_redraw = true;
}

static void flip_page(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    mpgl_lock(p->glctx);

    if (p->use_glFinish)
        gl->Finish();

    p->glctx->swapGlBuffers(p->glctx);

    p->frames_rendered++;
    if (p->frames_rendered > 5)
        gl_video_set_debug(p->renderer, false);

    mpgl_unlock(p->glctx);
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct gl_priv *p = vo->priv;

    mpgl_lock(p->glctx);

    gl_video_draw_osd(p->renderer, osd);

    mpgl_unlock(p->glctx);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct gl_priv *p = vo->priv;

    if (p->vo_flipped)
        mp_image_vflip(mpi);

    mpgl_lock(p->glctx);
    gl_video_upload_image(p->renderer, mpi);
    gl_video_render_frame(p->renderer);
    mpgl_unlock(p->glctx);
}

static int query_format(struct vo *vo, uint32_t format)
{
    int caps = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_FLIP;
    if (!gl_video_check_format(format))
        return 0;
    return caps;
}

static bool config_window(struct gl_priv *p, uint32_t d_width,
                          uint32_t d_height, uint32_t flags)
{
    if (p->renderer_opts->stereo_mode == GL_3D_QUADBUFFER)
        flags |= VOFLAG_STEREO;

    if (p->renderer_opts->alpha_mode == 1)
        flags |= VOFLAG_ALPHA;

    if (p->use_gl_debug)
        flags |= VOFLAG_GL_DEBUG;

    int mpgl_caps = MPGL_CAP_GL21 | MPGL_CAP_TEX_RG;
    if (!p->allow_sw)
        mpgl_caps |= MPGL_CAP_NO_SW;
    return mpgl_config_window(p->glctx, mpgl_caps, d_width, d_height, flags);
}

static void video_resize_redraw_callback(struct vo *vo, int w, int h)
{
    struct gl_priv *p = vo->priv;
    gl_video_resize_redraw(p->renderer, w, h);

}

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct gl_priv *p = vo->priv;

    mpgl_lock(p->glctx);

    if (!config_window(p, vo->dwidth, vo->dheight, flags)) {
        mpgl_unlock(p->glctx);
        return -1;
    }

    if (p->glctx->register_resize_callback) {
        p->glctx->register_resize_callback(vo, video_resize_redraw_callback);
    }

    gl_video_config(p->renderer, params);

    p->vo_flipped = !!(flags & VOFLAG_FLIPPING);

    resize(p);

    mpgl_unlock(p->glctx);

    return 0;
}

static bool reparse_cmdline(struct gl_priv *p, char *args)
{
    struct m_config *cfg = NULL;
    struct gl_video_opts *opts = NULL;
    int r = 0;

    if (strcmp(args, "-") == 0) {
        opts = p->renderer_opts;
    } else {
        cfg = m_config_new(NULL, sizeof(*opts), gl_video_conf.defaults,
                           gl_video_conf.opts,
                           p->vo->driver->init_option_string);
        opts = cfg->optstruct;
        r = m_config_parse_suboptions(cfg, "opengl", args);
    }

    if (r >= 0) {
        mpgl_lock(p->glctx);
        gl_video_set_options(p->renderer, opts);
        resize(p);
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
        bool r = gl_video_get_equalizer(p->renderer, args->name,
                                        args->valueptr);
        mpgl_unlock(p->glctx);
        return r ? VO_TRUE : VO_NOTIMPL;
    }
    case VOCTRL_SET_EQUALIZER: {
        struct voctrl_set_equalizer_args *args = data;
        mpgl_lock(p->glctx);
        bool r = gl_video_set_equalizer(p->renderer, args->name, args->value);
        mpgl_unlock(p->glctx);
        if (r)
            vo->want_redraw = true;
        return r ? VO_TRUE : VO_NOTIMPL;
    }
    case VOCTRL_GET_YUV_COLORSPACE:
        mpgl_lock(p->glctx);
        gl_video_get_csp_override(p->renderer, data);
        mpgl_unlock(p->glctx);
        return VO_TRUE;
    case VOCTRL_SCREENSHOT: {
        struct voctrl_screenshot_args *args = data;
        mpgl_lock(p->glctx);
        if (args->full_window)
            args->out_image = glGetWindowScreenshot(p->gl);
        else
            args->out_image = gl_video_download_image(p->renderer);
        mpgl_unlock(p->glctx);
        return true;
    }
    case VOCTRL_REDRAW_FRAME:
        mpgl_lock(p->glctx);
        gl_video_render_frame(p->renderer);
        mpgl_unlock(p->glctx);
        return true;
    case VOCTRL_SET_COMMAND_LINE: {
        char *arg = data;
        return reparse_cmdline(p, arg);
    }
    }

    mpgl_lock(p->glctx);
    int events = 0;
    int r = p->glctx->vo_control(vo, &events, request, data);
    if (events & VO_EVENT_RESIZE)
        resize(p);
    if (events & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
    mpgl_unlock(p->glctx);

    return r;
}

static void uninit(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    if (p->glctx) {
        if (p->renderer)
            gl_video_uninit(p->renderer);
        mpgl_uninit(p->glctx);
    }
}

static int preinit(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    p->vo = vo;

    p->glctx = mpgl_init(vo, p->backend);
    if (!p->glctx)
        goto err_out;
    p->gl = p->glctx->gl;

    if (!config_window(p, 320, 200, VOFLAG_HIDDEN))
        goto err_out;

    mpgl_set_context(p->glctx);

    if (p->gl->SwapInterval)
        p->gl->SwapInterval(p->swap_interval);

    p->renderer = gl_video_init(p->gl, vo->log);
    gl_video_set_output_depth(p->renderer, p->glctx->depth_r, p->glctx->depth_g,
                              p->glctx->depth_b);
    gl_video_set_options(p->renderer, p->renderer_opts);

    if (p->icc_opts->profile) {
        struct lut3d *lut3d = mp_load_icc(p->icc_opts, vo->log);
        if (!lut3d)
            goto err_out;
        gl_video_set_lut3d(p->renderer, lut3d);
        talloc_free(lut3d);
    }

    mpgl_unset_context(p->glctx);

    return 0;

err_out:
    uninit(vo);
    return -1;
}

#define OPT_BASE_STRUCT struct gl_priv
const struct m_option options[] = {
    OPT_FLAG("glfinish", use_glFinish, 0),
    OPT_INT("swapinterval", swap_interval, 0, OPTDEF_INT(1)),
    OPT_FLAG("debug", use_gl_debug, 0),
    OPT_STRING_VALIDATE("backend", backend, 0, mpgl_validate_backend_opt),
    OPT_FLAG("sw", allow_sw, 0),

    OPT_SUBSTRUCT("", renderer_opts, gl_video_conf, 0),
    OPT_SUBSTRUCT("", icc_opts, mp_icc_conf, 0),
    {0},
};

const struct vo_driver video_out_opengl = {
    .info = &(const vo_info_t) {
        "Extended OpenGL Renderer",
        "opengl",
        "Based on vo_gl.c by Reimar Doeffinger",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct gl_priv),
    .options = options,
};

const struct vo_driver video_out_opengl_hq = {
    .info = &(const vo_info_t) {
        "Extended OpenGL Renderer (high quality rendering preset)",
        "opengl-hq",
        "Based on vo_gl.c by Reimar Doeffinger",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct gl_priv),
    .options = options,
    .init_option_string = "lscale=lanczos2:dither-depth=auto:pbo:fbo-format=rgb16",
};
