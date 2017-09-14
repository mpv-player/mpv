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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/options.h"
#include "options/m_option.h"
#include "video/out/vo.h"

#include "context.h"

extern const struct ra_ctx_fns ra_ctx_glx;
extern const struct ra_ctx_fns ra_ctx_glx_probe;
extern const struct ra_ctx_fns ra_ctx_x11_egl;
extern const struct ra_ctx_fns ra_ctx_drm_egl;
extern const struct ra_ctx_fns ra_ctx_cocoa;
extern const struct ra_ctx_fns ra_ctx_wayland_egl;
extern const struct ra_ctx_fns ra_ctx_wgl;
extern const struct ra_ctx_fns ra_ctx_angle;
extern const struct ra_ctx_fns ra_ctx_dxinterop;
extern const struct ra_ctx_fns ra_ctx_rpi;
extern const struct ra_ctx_fns ra_ctx_mali;
extern const struct ra_ctx_fns ra_ctx_vdpauglx;

static const struct ra_ctx_fns *contexts[] = {
// OpenGL contexts:
#if HAVE_RPI
    &ra_ctx_rpi,
#endif
/*
#if HAVE_GL_COCOA
    &ra_ctx_cocoa,
#endif
#if HAVE_EGL_ANGLE_WIN32
    &ra_ctx_angle,
#endif
#if HAVE_GL_WIN32
    &ra_ctx_wgl,
#endif
#if HAVE_GL_DXINTEROP
    &ra_ctx_dxinterop,
#endif
*/
#if HAVE_GL_X11
    &ra_ctx_glx_probe,
#endif
#if HAVE_EGL_X11
    &ra_ctx_x11_egl,
#endif
#if HAVE_GL_X11
    &ra_ctx_glx,
#endif
#if HAVE_GL_WAYLAND
    &ra_ctx_wayland_egl,
#endif
#if HAVE_EGL_DRM
    &ra_ctx_drm_egl,
#endif
#if HAVE_MALI_FBDEV
    &ra_ctx_mali,
#endif
#if HAVE_VDPAU_GL_X11
    &ra_ctx_vdpauglx,
#endif
};

static bool get_help(struct mp_log *log, struct bstr param)
{
    if (bstr_equals0(param, "help")) {
        mp_info(log, "GPU contexts / APIs:\n");
        mp_info(log, "    auto (autodetect)\n");
        for (int n = 0; n < MP_ARRAY_SIZE(contexts); n++)
            mp_info(log, "    %s (%s)\n", contexts[n]->name, contexts[n]->type);
        return true;
    }

    return false;
}

int ra_ctx_validate_api(struct mp_log *log, const struct m_option *opt,
                        struct bstr name, struct bstr param)
{
    if (get_help(log, param))
        return M_OPT_EXIT;
    if (bstr_equals0(param, "auto"))
        return 1;
    for (int i = 0; i < MP_ARRAY_SIZE(contexts); i++) {
        if (bstr_equals0(param, contexts[i]->type))
            return 1;
    }
    return M_OPT_INVALID;
}

int ra_ctx_validate_context(struct mp_log *log, const struct m_option *opt,
                            struct bstr name, struct bstr param)
{
    if (get_help(log, param))
        return M_OPT_EXIT;
    if (bstr_equals0(param, "auto"))
        return 1;
    for (int i = 0; i < MP_ARRAY_SIZE(contexts); i++) {
        if (bstr_equals0(param, contexts[i]->name))
            return 1;
    }
    return M_OPT_INVALID;
}

// Create a VO window and create a RA context on it.
//  vo_flags: passed to the backend's create window function
struct ra_ctx *ra_ctx_create(struct vo *vo, const char *context_type,
                             const char *context_name, struct ra_ctx_opts opts)
{
    bool api_auto = !context_type || strcmp(context_type, "auto") == 0;
    bool ctx_auto = !context_name || strcmp(context_name, "auto") == 0;

    if (ctx_auto) {
        MP_VERBOSE(vo, "Probing for best GPU context.\n");
        opts.probing = true;
    }

    // Hack to silence backend (X11/Wayland/etc.) errors. Kill it once backends
    // are separate from `struct vo`
    bool old_probing = vo->probing;
    vo->probing = opts.probing;

    for (int i = 0; i < MP_ARRAY_SIZE(contexts); i++) {
        if (!opts.probing && strcmp(contexts[i]->name, context_name) != 0)
            continue;
        if (!api_auto && strcmp(contexts[i]->type, context_type) != 0)
            continue;

        struct ra_ctx *ctx = talloc_ptrtype(NULL, ctx);
        *ctx = (struct ra_ctx) {
            .vo = vo,
            .global = vo->global,
            .log = mp_log_new(ctx, vo->log, contexts[i]->type),
            .opts = opts,
            .fns = contexts[i],
        };

        MP_VERBOSE(ctx, "Initializing GPU context '%s'\n", ctx->fns->name);
        if (contexts[i]->init(ctx)) {
            vo->probing = old_probing;
            return ctx;
        }

        talloc_free(ctx);
    }

    // If we've reached this point, then none of the contexts matched the name
    // requested, or the backend creation failed for all of them.
    MP_ERR(vo, "Failed initializing any suitable GPU context!\n");
    vo->probing = old_probing;
    return NULL;
}

void ra_ctx_destroy(struct ra_ctx **ctx)
{
    if (*ctx)
        (*ctx)->fns->uninit(*ctx);
    talloc_free(*ctx);
    *ctx = NULL;
}
