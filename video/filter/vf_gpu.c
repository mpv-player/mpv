/*
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

#include "common/common.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/options.h"
#include "video/out/aspect.h"
#include "video/out/gpu/video.h"
#include "video/out/opengl/egl_helpers.h"
#include "video/out/opengl/ra_gl.h"

struct offscreen_ctx {
    struct mp_log *log;
    struct ra *ra;
    void *priv;

    void (*set_context)(struct offscreen_ctx *ctx, bool enable);
};

struct gl_offscreen_ctx {
    GL gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
};

static void gl_ctx_destroy(void *p)
{
    struct offscreen_ctx *ctx = p;
    struct gl_offscreen_ctx *gl = ctx->priv;

    ra_free(&ctx->ra);

    if (gl->egl_context)
        eglDestroyContext(gl->egl_display, gl->egl_context);
}

static void gl_ctx_set_context(struct offscreen_ctx *ctx, bool enable)
{
    struct gl_offscreen_ctx *gl = ctx->priv;
    EGLContext c = enable ? gl->egl_context : EGL_NO_CONTEXT;

    if (!eglMakeCurrent(gl->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, c))
        MP_ERR(ctx, "Could not make EGL context current.\n");
}

static struct offscreen_ctx *gl_offscreen_ctx_create(struct mpv_global *global,
                                                     struct mp_log *log)
{
    struct offscreen_ctx *ctx = talloc_zero(NULL, struct offscreen_ctx);
    struct gl_offscreen_ctx *gl = talloc_zero(ctx, struct gl_offscreen_ctx);
    talloc_set_destructor(ctx, gl_ctx_destroy);
    *ctx = (struct offscreen_ctx){
        .log = log,
        .priv = gl,
        .set_context = gl_ctx_set_context,
    };

    // This appears to work with Mesa. EGL 1.5 doesn't specify what a "default
    // display" is at all.
    gl->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(gl->egl_display, NULL, NULL)) {
        MP_ERR(ctx, "Could not initialize EGL.\n");
        goto error;
    }

    // Unfortunately, mpegl_create_context() is entangled with ra_ctx.
    // Fortunately, it does not need much, and we can provide a stub.
    struct ra_ctx ractx = {
        .log = ctx->log,
        .global = global,
    };
    EGLConfig config;
    if (!mpegl_create_context(&ractx, gl->egl_display, &gl->egl_context, &config))
    {
        MP_ERR(ctx, "Could not create EGL context.\n");
        goto error;
    }

    if (!eglMakeCurrent(gl->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                        gl->egl_context))
    {
        MP_ERR(ctx, "Could not make EGL context current.\n");
        goto error;
    }

    mpegl_load_functions(&gl->gl, ctx->log);
    ctx->ra = ra_create_gl(&gl->gl, ctx->log);

    if (!ctx->ra)
        goto error;

    gl_ctx_set_context(ctx, false);

    return ctx;

error:
    talloc_free(ctx);
    return NULL;
}

static void offscreen_ctx_set_current(struct offscreen_ctx *ctx, bool enable)
{
    if (ctx->set_context)
        ctx->set_context(ctx, enable);
}

struct gpu_opts {
    int w, h;
};

struct priv {
    struct gpu_opts *opts;
    struct m_config_cache *vo_opts_cache;
    struct mp_vo_opts *vo_opts;

    struct offscreen_ctx *ctx;
    struct gl_video *renderer;
    struct ra_tex *target;

    struct mp_image_params img_params;
    uint64_t next_frame_id;
};

static struct mp_image *gpu_render_frame(struct mp_filter *f, struct mp_image *in)
{
    struct priv *priv = f->priv;
    bool ok = false;
    struct mp_image *res = NULL;
    struct ra *ra = priv->ctx->ra;

    if (priv->opts->w <= 0)
        priv->opts->w = in->w;
    if (priv->opts->h <= 0)
        priv->opts->h = in->h;

    int w = priv->opts->w;
    int h = priv->opts->h;

    struct vo_frame frame = {
        .pts = in->pts,
        .duration = -1,
        .num_vsyncs = 1,
        .current = in,
        .num_frames = 1,
        .frames = {in},
        .frame_id = ++(priv->next_frame_id),
    };

    bool need_reconfig = m_config_cache_update(priv->vo_opts_cache);

    if (!mp_image_params_static_equal(&priv->img_params, &in->params)) {
        gl_video_config(priv->renderer, &in->params);
        need_reconfig = true;
    }

    if (!mp_image_params_equal(&priv->img_params, &in->params))
        priv->img_params = in->params;

    if (need_reconfig) {
        struct mp_rect src, dst;
        struct mp_osd_res osd;

        struct mp_stream_info *info = mp_filter_find_stream_info(f);
        struct osd_state *osd_state = info ? info->osd : NULL;
        if (osd_state) {
            osd_set_render_subs_in_filter(osd_state, true);
            // Assume the osd_state doesn't somehow disappear.
            gl_video_set_osd_source(priv->renderer, osd_state);
        }

        mp_get_src_dst_rects(f->log, priv->vo_opts, VO_CAP_ROTATE90, &in->params,
                             w, h, 1, &src, &dst, &osd);

        gl_video_resize(priv->renderer, &src, &dst, &osd);
    }

    if (!priv->target) {
        struct ra_tex_params params = {
            .dimensions = 2,
            .downloadable = true,
            .w = w,
            .h = h,
            .d = 1,
            .render_dst = true,
        };

        params.format = ra_find_unorm_format(ra, 1, 4);

        if (!params.format || !params.format->renderable)
            goto done;

        priv->target = ra_tex_create(ra, &params);
        if (!priv->target)
            goto done;
    }

    // (it doesn't have access to the OSD though)
    int flags = RENDER_FRAME_SUBS | RENDER_FRAME_VF_SUBS;
    gl_video_render_frame(priv->renderer, &frame, &(struct ra_fbo){priv->target},
                          flags);

    res = mp_image_alloc(IMGFMT_RGB0, w, h);
    if (!res)
        goto done;

    struct ra_tex_download_params download_params = {
        .tex = priv->target,
        .dst = res->planes[0],
        .stride = res->stride[0],
    };
    if (!ra->fns->tex_download(ra, &download_params))
        goto done;

    ok = true;
done:
    if (!ok)
        TA_FREEP(&res);
    return res;
}

static void gpu_process(struct mp_filter *f)
{
    struct priv *priv = f->priv;

    if (!mp_pin_can_transfer_data(f->ppins[1], f->ppins[0]))
        return;

    struct mp_frame frame = mp_pin_out_read(f->ppins[0]);

    if (mp_frame_is_signaling(frame)) {
        mp_pin_in_write(f->ppins[1], frame);
        return;
    }

    if (frame.type != MP_FRAME_VIDEO)
        goto error;

    offscreen_ctx_set_current(priv->ctx, true);

    struct mp_image *mpi = frame.data;
    struct mp_image *res = gpu_render_frame(f, mpi);
    if (!res) {
        MP_ERR(f, "Could not render or retrieve frame.\n");
        goto error;
    }

    // It's not clear which parameters to copy.
    res->pts = mpi->pts;
    res->dts = mpi->dts;
    res->nominal_fps = mpi->nominal_fps;

    talloc_free(mpi);

    mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, res));
    return;

error:
    mp_frame_unref(&frame);
    mp_filter_internal_mark_failed(f);
    offscreen_ctx_set_current(priv->ctx, false);
}

static void gpu_reset(struct mp_filter *f)
{
    struct priv *priv = f->priv;

    offscreen_ctx_set_current(priv->ctx, true);
    gl_video_reset(priv->renderer);
    offscreen_ctx_set_current(priv->ctx, false);
}

static void gpu_destroy(struct mp_filter *f)
{
    struct priv *priv = f->priv;

    if (priv->ctx) {
        offscreen_ctx_set_current(priv->ctx, true);

        gl_video_uninit(priv->renderer);
        ra_tex_free(priv->ctx->ra, &priv->target);

        offscreen_ctx_set_current(priv->ctx, false);
    }

    talloc_free(priv->ctx);
}

static const struct mp_filter_info gpu_filter = {
    .name = "gpu",
    .process = gpu_process,
    .reset = gpu_reset,
    .destroy = gpu_destroy,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *gpu_create(struct mp_filter *parent, void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &gpu_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *priv = f->priv;
    priv->opts = talloc_steal(priv, options);
    priv->vo_opts_cache = m_config_cache_alloc(f, f->global, &vo_sub_opts);
    priv->vo_opts = priv->vo_opts_cache->opts;

    priv->ctx = gl_offscreen_ctx_create(f->global, f->log);
    if (!priv->ctx) {
        MP_FATAL(f, "Could not create offscreen ra context.\n");
        goto error;
    }

    if (!priv->ctx->ra->fns->tex_download) {
        MP_FATAL(f, "Offscreen ra context does not support image retrieval.\n");
        goto error;
    }

    offscreen_ctx_set_current(priv->ctx, true);

    priv->renderer = gl_video_init(priv->ctx->ra, f->log, f->global);
    assert(priv->renderer); // can't fail (strangely)

    offscreen_ctx_set_current(priv->ctx, false);

    MP_WARN(f, "This is experimental. Keep in mind:\n");
    MP_WARN(f, " - OSD rendering is done in software.\n");
    MP_WARN(f, " - Encoding will convert the RGB output to yuv420p in software.\n");
    MP_WARN(f, " - Using this with --vo=gpu will filter the video twice!\n");
    MP_WARN(f, "   (And you can't prevent this; they use the same options.)\n");
    MP_WARN(f, " - Some features are simply not supported.\n");

    return f;

error:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct gpu_opts
const struct mp_user_filter_entry vf_gpu = {
    .desc = {
        .description = "vo_gpu as filter",
        .name = "gpu",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .options = (const struct m_option[]){
            {"w", OPT_INT(w)},
            {"h", OPT_INT(h)},
            {0}
        },
    },
    .create = gpu_create,
};
