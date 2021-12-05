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

#include "options/m_config.h"
#include "context.h"
#include "ra_gl.h"
#include "utils.h"

// 0-terminated list of desktop GL versions a backend should try to
// initialize. Each entry is the minimum required version.
const int mpgl_min_required_gl_versions[] = {
    /*
     * Nvidia drivers will not provide the highest supported version
     * when 320 core is requested. Instead, it just returns 3.2. This
     * would be bad, as we actually want compute shaders that require
     * 4.2, so we have to request a sufficiently high version. We use
     * 440 to maximise driver compatibility as we don't need anything
     * from newer versions.
     */
    440,
    320,
    210,
    0
};

enum {
    FLUSH_NO = 0,
    FLUSH_YES,
    FLUSH_AUTO,
};

struct opengl_opts {
    int use_glfinish;
    int waitvsync;
    int vsync_pattern[2];
    int swapinterval;
    int early_flush;
    int gles_mode;
};

#define OPT_BASE_STRUCT struct opengl_opts
const struct m_sub_options opengl_conf = {
    .opts = (const struct m_option[]) {
        {"opengl-glfinish", OPT_FLAG(use_glfinish)},
        {"opengl-waitvsync", OPT_FLAG(waitvsync)},
        {"opengl-swapinterval", OPT_INT(swapinterval)},
        {"opengl-check-pattern-a", OPT_INT(vsync_pattern[0])},
        {"opengl-check-pattern-b", OPT_INT(vsync_pattern[1])},
        {"opengl-restrict", OPT_REMOVED(NULL)},
        {"opengl-es", OPT_CHOICE(gles_mode,
            {"auto", GLES_AUTO}, {"yes", GLES_YES}, {"no", GLES_NO})},
        {"opengl-early-flush", OPT_CHOICE(early_flush,
            {"no", FLUSH_NO}, {"yes", FLUSH_YES}, {"auto", FLUSH_AUTO})},
        {"opengl-debug", OPT_REPLACED("gpu-debug")},
        {"opengl-sw", OPT_REPLACED("gpu-sw")},
        {"opengl-vsync-fences", OPT_REPLACED("swapchain-depth")},
        {"opengl-backend", OPT_REPLACED("gpu-context")},
        {0},
    },
    .defaults = &(const struct opengl_opts) {
        .swapinterval = 1,
    },
    .size = sizeof(struct opengl_opts),
};

struct priv {
    GL *gl;
    struct mp_log *log;
    struct ra_gl_ctx_params params;
    struct opengl_opts *opts;
    struct ra_swapchain_fns fns;
    GLuint main_fb;
    struct ra_tex *wrapped_fb; // corresponds to main_fb
    // for debugging:
    int frames_rendered;
    unsigned int prev_sgi_sync_count;
    // for gl_vsync_pattern
    int last_pattern;
    int matches, mismatches;
    // for swapchain_depth simulation
    GLsync *vsync_fences;
    int num_vsync_fences;
};

enum gles_mode ra_gl_ctx_get_glesmode(struct ra_ctx *ctx)
{
    void *tmp = talloc_new(NULL);
    struct opengl_opts *opts;
    enum gles_mode mode;

    opts = mp_get_config_group(tmp, ctx->global, &opengl_conf);
    mode = opts->gles_mode;

    talloc_free(tmp);
    return mode;
}

void ra_gl_ctx_uninit(struct ra_ctx *ctx)
{
    if (ctx->swapchain) {
        struct priv *p = ctx->swapchain->priv;
        if (ctx->ra && p->wrapped_fb)
            ra_tex_free(ctx->ra, &p->wrapped_fb);
        talloc_free(ctx->swapchain);
        ctx->swapchain = NULL;
    }

    // Clean up any potentially left-over debug callback
    if (ctx->ra)
        ra_gl_set_debug(ctx->ra, false);

    ra_free(&ctx->ra);
}

static const struct ra_swapchain_fns ra_gl_swapchain_fns;

bool ra_gl_ctx_init(struct ra_ctx *ctx, GL *gl, struct ra_gl_ctx_params params)
{
    struct ra_swapchain *sw = ctx->swapchain = talloc_ptrtype(NULL, sw);
    *sw = (struct ra_swapchain) {
        .ctx = ctx,
    };

    struct priv *p = sw->priv = talloc_ptrtype(sw, p);
    *p = (struct priv) {
        .gl     = gl,
        .log    = ctx->log,
        .params = params,
        .opts   = mp_get_config_group(p, ctx->global, &opengl_conf),
        .fns    = ra_gl_swapchain_fns,
    };

    sw->fns = &p->fns;

    const struct ra_swapchain_fns *ext = p->params.external_swapchain;
    if (ext) {
        if (ext->color_depth)
            p->fns.color_depth = ext->color_depth;
        if (ext->start_frame)
            p->fns.start_frame = ext->start_frame;
        if (ext->submit_frame)
            p->fns.submit_frame = ext->submit_frame;
        if (ext->swap_buffers)
            p->fns.swap_buffers = ext->swap_buffers;
    }

    if (!gl->version && !gl->es)
        return false;

    if (gl->mpgl_caps & MPGL_CAP_SW) {
        MP_WARN(p, "Suspected software renderer or indirect context.\n");
        if (ctx->opts.probing && !ctx->opts.allow_sw)
            return false;
    }

    gl->debug_context = ctx->opts.debug;

    if (gl->SwapInterval) {
        gl->SwapInterval(p->opts->swapinterval);
    } else {
        MP_VERBOSE(p, "GL_*_swap_control extension missing.\n");
    }

    ctx->ra = ra_create_gl(p->gl, ctx->log);
    return !!ctx->ra;
}

void ra_gl_ctx_resize(struct ra_swapchain *sw, int w, int h, int fbo)
{
    struct priv *p = sw->priv;
    if (p->main_fb == fbo && p->wrapped_fb && p->wrapped_fb->params.w == w
        && p->wrapped_fb->params.h == h)
        return;

    if (p->wrapped_fb)
        ra_tex_free(sw->ctx->ra, &p->wrapped_fb);

    p->main_fb = fbo;
    p->wrapped_fb = ra_create_wrapped_fb(sw->ctx->ra, fbo, w, h);
}

int ra_gl_ctx_color_depth(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;
    GL *gl = p->gl;

    if (!p->wrapped_fb)
        return 0;

    if ((gl->es < 300 && !gl->version) || !(gl->mpgl_caps & MPGL_CAP_FB))
        return 0;

    gl->BindFramebuffer(GL_FRAMEBUFFER, p->main_fb);

    GLenum obj = gl->version ? GL_BACK_LEFT : GL_BACK;
    if (p->main_fb)
        obj = GL_COLOR_ATTACHMENT0;

    GLint depth_g = 0;

    gl->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, obj,
                            GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &depth_g);

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);

    return depth_g;
}

bool ra_gl_ctx_start_frame(struct ra_swapchain *sw, struct ra_fbo *out_fbo)
{
    struct priv *p = sw->priv;
    *out_fbo = (struct ra_fbo) {
         .tex = p->wrapped_fb,
         .flip = !p->params.flipped, // OpenGL FBs are normally flipped
    };
    return true;
}

bool ra_gl_ctx_submit_frame(struct ra_swapchain *sw, const struct vo_frame *frame)
{
    struct priv *p = sw->priv;
    GL *gl = p->gl;

    if (p->opts->use_glfinish)
        gl->Finish();

    if (gl->FenceSync && !p->params.external_swapchain) {
        GLsync fence = gl->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (fence)
            MP_TARRAY_APPEND(p, p->vsync_fences, p->num_vsync_fences, fence);
    }

    switch (p->opts->early_flush) {
    case FLUSH_AUTO:
        if (frame->display_synced)
            break;
        // fall through
    case FLUSH_YES:
        gl->Flush();
    }

    return true;
}

static void check_pattern(struct priv *p, int item)
{
    int expected = p->opts->vsync_pattern[p->last_pattern];
    if (item == expected) {
        p->last_pattern++;
        if (p->last_pattern >= 2)
            p->last_pattern = 0;
        p->matches++;
    } else {
        p->mismatches++;
        MP_WARN(p, "wrong pattern, expected %d got %d (hit: %d, mis: %d)\n",
                expected, item, p->matches, p->mismatches);
    }
}

void ra_gl_ctx_swap_buffers(struct ra_swapchain *sw)
{
    struct priv *p = sw->priv;
    GL *gl = p->gl;

    p->params.swap_buffers(sw->ctx);
    p->frames_rendered++;

    if (p->frames_rendered > 5 && !sw->ctx->opts.debug)
        ra_gl_set_debug(sw->ctx->ra, false);

    if ((p->opts->waitvsync || p->opts->vsync_pattern[0])
        && gl->GetVideoSync)
    {
        unsigned int n1 = 0, n2 = 0;
        gl->GetVideoSync(&n1);
        if (p->opts->waitvsync)
            gl->WaitVideoSync(2, (n1 + 1) % 2, &n2);
        int step = n1 - p->prev_sgi_sync_count;
        p->prev_sgi_sync_count = n1;
        MP_DBG(p, "Flip counts: %u->%u, step=%d\n", n1, n2, step);
        if (p->opts->vsync_pattern[0])
            check_pattern(p, step);
    }

    while (p->num_vsync_fences >= sw->ctx->vo->opts->swapchain_depth) {
        gl->ClientWaitSync(p->vsync_fences[0], GL_SYNC_FLUSH_COMMANDS_BIT, 1e9);
        gl->DeleteSync(p->vsync_fences[0]);
        MP_TARRAY_REMOVE_AT(p->vsync_fences, p->num_vsync_fences, 0);
    }
}

static void ra_gl_ctx_get_vsync(struct ra_swapchain *sw,
                                struct vo_vsync_info *info)
{
    struct priv *p = sw->priv;
    if (p->params.get_vsync)
        p->params.get_vsync(sw->ctx, info);
}

static const struct ra_swapchain_fns ra_gl_swapchain_fns = {
    .color_depth   = ra_gl_ctx_color_depth,
    .start_frame   = ra_gl_ctx_start_frame,
    .submit_frame  = ra_gl_ctx_submit_frame,
    .swap_buffers  = ra_gl_ctx_swap_buffers,
    .get_vsync     = ra_gl_ctx_get_vsync,
};
